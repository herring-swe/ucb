/**
 * @file file.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unified file, pipe and standard stream I/O handle
 *
 * @ref ucb_file is a heap allocated, opaque I/O handle that represents a regular
 * file, a pipe end, a console/tty or another character or block device. It
 * unifies the common operations an APR @c apr_file_t or a POSIX file descriptor
 * exposes, while remaining portable to Windows.
 *
 * Design contract:
 * - The data plane is **unbuffered** (file-descriptor semantics). Callers that
 *   need buffering should use @ref ucb_buffer.
 * - Every handle exposes a @ref ucb_file_kind and a @ref ucb_file_caps
 *   capability bitmask. Operations that a handle cannot perform fail with a
 *   thrown @ref UCB_ERROR_NOT_IMPLEMENTED rather than being impossible to
 *   compile against.
 * - A handle is **not thread-safe**. Use it from one thread at a time. Sharing
 *   a handle between threads requires external synchronization.
 * - Sockets are intentionally not represented here. Their connection oriented
 *   API differs and Windows @c SOCKET is not a @c HANDLE.
 */

#ifndef UCB_FILE_H
#define UCB_FILE_H

#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ucb_buffer;
typedef struct ucb_buffer ucb_buffer;

/**
 * @struct ucb_file
 * @brief Opaque unified I/O handle
 *
 * Created with @ref ucb_file_open, @ref ucb_file_open_mode, @ref ucb_file_from_fd,
 * @c ucb_file_from_handle (Windows), @ref ucb_file_dup or @ref ucb_file_stdin and
 * friends. Release an owning handle with @ref ucb_file_free.
 */
typedef struct ucb_file ucb_file;

/**
 * @brief The kind of resource a @ref ucb_file refers to
 */
typedef enum ucb_file_kind
{
    UCB_FILE_KIND_UNKNOWN = 0, ///< Invalid, closed or unrecognized handle
    UCB_FILE_KIND_REGULAR,     ///< Regular file on a filesystem
    UCB_FILE_KIND_PIPE,        ///< Anonymous or named pipe end
    UCB_FILE_KIND_CONSOLE,     ///< TTY, terminal or console stream
    UCB_FILE_KIND_DEVICE,      ///< Character or block device (e.g. /dev/null)
} ucb_file_kind;

/**
 * @brief Capability bitmask of a @ref ucb_file
 *
 * Queried with @ref ucb_file_get_caps. Operations not advertised here fail with
 * a thrown @ref UCB_ERROR_NOT_IMPLEMENTED.
 */
typedef enum ucb_file_caps
{
    UCB_FILE_CAP_READ = 1u << 0,  ///< @ref ucb_file_read is supported
    UCB_FILE_CAP_WRITE = 1u << 1, ///< @ref ucb_file_write is supported
    UCB_FILE_CAP_SEEK = 1u << 2,  ///< @ref ucb_file_seek / @ref ucb_file_tell
    UCB_FILE_CAP_POLL = 1u << 3,  ///< @ref ucb_file_wait_readable / writable
} ucb_file_caps;

/**
 * @brief Flags for opening or wrapping a @ref ucb_file
 */
typedef enum ucb_file_open_flags
{
    UCB_FILE_READ = 1u << 0,     ///< Open for reading
    UCB_FILE_WRITE = 1u << 1,    ///< Open for writing
    UCB_FILE_APPEND = 1u << 2,   ///< Writes append to the end
    UCB_FILE_TRUNCATE = 1u << 3, ///< Truncate an existing file to zero length
    UCB_FILE_CREATE = 1u << 4,   ///< Create the file if it does not exist
    UCB_FILE_EXCL = 1u << 5,     ///< With @ref UCB_FILE_CREATE, fail if it exists
    UCB_FILE_NONBLOCK = 1u << 6, ///< Non-blocking I/O where supported
    UCB_FILE_CLOEXEC = 1u << 7,  ///< Not inherited by child processes
    UCB_FILE_INHERIT = 1u << 8,  ///< Inherited by child processes (Process API)
    UCB_FILE_BINARY = 1u << 9,   ///< Binary mode (no text translation)
    UCB_FILE_DIRECT = 1u << 10,  ///< Unbuffered direct I/O where supported
} ucb_file_open_flags;

/**
 * @brief Origin for @ref ucb_file_seek
 */
typedef enum ucb_file_whence
{
    UCB_FILE_SEEK_SET = 0, ///< Relative to the start of the file
    UCB_FILE_SEEK_CUR,     ///< Relative to the current position
    UCB_FILE_SEEK_END,     ///< Relative to the end of the file
} ucb_file_whence;

/* -------------------------------------------------------------------------- */
/*                              Open and lifetime                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Open or create a regular file
 *
 * Opens a UTF-8 encoded @p path with the given @p flags. At least one of
 * @ref UCB_FILE_READ or @ref UCB_FILE_WRITE must be set. When @ref
 * UCB_FILE_CREATE is set, the file is created with a default mode (read/write
 * for the owner, read for others, subject to the platform umask), the same as
 * @ref ucb_file_open_mode with @c mode @c 0666.
 *
 * @param path UTF-8 encoded path to open, must be non-NULL
 * @param flags bitwise OR of @ref ucb_file_open_flags
 * @param perr optional location to store the error on failure
 * @return a new owning handle, or UCB_NULL on failure
 */
UCB_API ucb_file* ucb_file_open(const char* path, unsigned flags, ucb_error** perr);

/**
 * @brief Open or create a regular file with an explicit creation mode
 *
 * Like @ref ucb_file_open but uses @p mode for newly created files. On Windows
 * @p mode is ignored.
 *
 * @param path UTF-8 encoded path to open, must be non-NULL
 * @param flags bitwise OR of @ref ucb_file_open_flags
 * @param mode POSIX creation mode (permission bits) for new files
 * @param perr optional location to store the error on failure
 * @return a new owning handle, or UCB_NULL on failure
 */
UCB_API ucb_file* ucb_file_open_mode(const char* path,
                                     unsigned flags,
                                     unsigned mode,
                                     ucb_error** perr);

/**
 * @brief Close the underlying OS resource of a handle
 *
 * Closes the underlying file descriptor or handle if this object owns it. The
 * @ref ucb_file object itself remains allocated and can be freed with
 * @ref ucb_file_free. Closing is idempotent: calling it on an already closed
 * handle returns true without side effects.
 *
 * Closing a non-owning handle (including the standard stream singletons and
 * handles created with @c own = false) never closes the underlying OS resource.
 *
 * @param file the handle, may be UCB_NULL
 * @param perr optional location to store the error on failure
 * @return true on success, false if the OS close failed
 */
UCB_API bool ucb_file_close(ucb_file* file, ucb_error** perr);

/**
 * @brief Close (if owned) and free a handle
 *
 * For an owning handle this closes the underlying OS resource and frees the
 * @ref ucb_file object. The standard stream singletons and other handles with
 * @c own = false are left untouched, so this function must not be used to
 * release a non-owning handle's structure. @p file may be UCB_NULL.
 *
 * Errors from the implicit close are ignored. Use @ref ucb_file_close first if
 * the close status is needed.
 *
 * @param file the handle to free, may be UCB_NULL
 */
UCB_API void ucb_file_free(ucb_file* file);

/* -------------------------------------------------------------------------- */
/*                               Native interop                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wrap an existing POSIX/CRT file descriptor
 *
 * The returned handle is valid as long as @p fd is valid. When @p own is true a
 * successful @ref ucb_file_close or @ref ucb_file_free closes @p fd.
 *
 * On Windows @p fd is a C runtime descriptor and is converted to its underlying
 * @c HANDLE, which then backs all I/O on the returned handle. Because the access
 * mode of an existing native handle cannot be queried, a wrapped Windows handle
 * advertises both @ref UCB_FILE_CAP_READ and @ref UCB_FILE_CAP_WRITE; misuse of
 * a direction the handle does not support surfaces as a thrown system error.
 *
 * @param fd file descriptor, must be non-negative and open
 * @param own true if the handle should close @p fd, false for a borrowed view
 * @param perr optional location to store the error on failure
 * @return a new handle, or UCB_NULL on failure
 */
UCB_API ucb_file* ucb_file_from_fd(int fd, bool own, ucb_error** perr);

/**
 * @brief Get the POSIX/CRT file descriptor backing a handle
 *
 * @param file the handle, may be UCB_NULL
 * @return the descriptor, or -1 if the handle is invalid or has no descriptor
 *         (for example a file opened natively on Windows)
 */
UCB_API int ucb_file_get_fd(const ucb_file* file);

#ifdef _WIN32
/**
 * @brief Wrap an existing Windows @c HANDLE
 *
 * When @p own is true a successful @ref ucb_file_close or @ref ucb_file_free
 * closes @p handle with @c CloseHandle.
 *
 * As with @ref ucb_file_from_fd, the access mode of an existing @c HANDLE cannot
 * be queried, so the wrapped handle advertises both read and write capabilities.
 *
 * @param handle the Windows handle to wrap, must not be UCB_NULL
 * @param own true if the handle should be closed, false for a borrowed view
 * @param perr optional location to store the error on failure
 * @return a new handle, or UCB_NULL on failure
 */
UCB_API ucb_file* ucb_file_from_handle(void* handle, bool own, ucb_error** perr);

/**
 * @brief Get the Windows @c HANDLE backing a handle
 *
 * @param file the handle, may be UCB_NULL
 * @return the @c HANDLE, or UCB_NULL if the handle is invalid
 */
UCB_API void* ucb_file_get_handle(const ucb_file* file);
#endif // _WIN32

/* -------------------------------------------------------------------------- */
/*                            Duplication and sharing                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Duplicate a handle
 *
 * Creates an independent owning handle referring to the same underlying
 * resource. The inheritance state of @p file is preserved. Duplication is the
 * supported way to obtain an owning handle for a standard stream or another
 * borrowed view.
 *
 * @param file the handle to duplicate, must be non-NULL and valid
 * @param perr optional location to store the error on failure
 * @return a new owning handle, or UCB_NULL on failure
 */
UCB_API ucb_file* ucb_file_dup(ucb_file* file, ucb_error** perr);

/**
 * @brief Control whether the underlying resource is inherited by child processes
 *
 * @param file the handle, must be non-NULL and valid
 * @param inherit true to mark the resource inheritable, false to hide it
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_set_inherit(ucb_file* file, bool inherit, ucb_error** perr);

/* -------------------------------------------------------------------------- */
/*                                    I/O                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read up to @p size bytes into @p buf
 *
 * May perform a short transfer. Returns the number of bytes read, @c 0 at end
 * of input, or @c -1 on error with the error thrown into @p perr. @c EINTR is
 * retried internally. A non-blocking handle with no data available throws
 * @ref UCB_ERRSYS_EAGAIN; wait with @ref ucb_file_wait_readable first.
 *
 * @param file the handle, must be non-NULL, valid and readable
 * @param buf destination buffer of at least @p size bytes
 * @param size maximum number of bytes to read
 * @param perr optional location to store the error on failure
 * @return bytes read, 0 on end of input, or -1 on error
 */
UCB_API ucb_ssize ucb_file_read(ucb_file* file, void* buf, size_t size, ucb_error** perr);

/**
 * @brief Write up to @p size bytes from @p buf
 *
 * May perform a short transfer. Returns the number of bytes written or @c -1 on
 * error with the error thrown into @p perr. @c EINTR is retried internally.
 *
 * For a call that keeps retrying until every byte is written, use
 * @ref ucb_file_write_full.
 *
 * @param file the handle, must be non-NULL, valid and writable
 * @param buf source buffer of at least @p size bytes
 * @param size number of bytes to write
 * @param perr optional location to store the error on failure
 * @return bytes written, or -1 on error
 * @see ucb_file_write_full
 */
UCB_API ucb_ssize ucb_file_write(ucb_file* file, const void* buf, size_t size, ucb_error** perr);

/**
 * @brief Read a seekable handle fully into @p buf
 *
 * Reads from the current position to end of input into @p buf, which must hold
 * at least @p capacity bytes. Only handles advertising @ref UCB_FILE_CAP_SEEK
 * are supported; a non-seekable handle throws @ref UCB_ERROR_NOT_IMPLEMENTED.
 *
 * If the remaining input does not fit in @p capacity, nothing is read, the
 * function returns false, @ref UCB_ERROR_OUT_OF_BOUNDS is thrown, and
 * @p out_size is set to the number of bytes required so the caller can size a
 * buffer for a retry. On success @p out_size is set to the number of bytes read.
 *
 * @param file the handle, must be non-NULL, valid, readable and seekable
 * @param buf destination buffer of at least @p capacity bytes
 * @param capacity size of @p buf in bytes
 * @param out_size required location for the bytes read, or the required size on
 *                 a too-small buffer
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_read_full(ucb_file* file,
                                void* buf,
                                size_t capacity,
                                size_t* out_size,
                                ucb_error** perr);

/**
 * @brief Write all @p size bytes from @p buf
 *
 * Retries short transfers and @c EINTR until every byte is written.
 *
 * @param file the handle, must be non-NULL, valid and writable
 * @param buf source buffer of at least @p size bytes
 * @param size number of bytes to write
 * @param perr optional location to store the error on failure
 * @return true on success
 * @see ucb_file_write
 */
UCB_API bool ucb_file_write_full(ucb_file* file, const void* buf, size_t size, ucb_error** perr);

/**
 * @brief Read until end of input, appending to a buffer
 *
 * Retries short transfers and @c EINTR, appending every chunk to @p out. The
 * buffer may grow as needed. For a regular file this reads to end of file.
 * This is the safe, convenient variant of @ref ucb_file_read_full; it works on
 * non-seekable handles as well. Returns false if the file read or the buffer
 * allocation fails.
 *
 * @param file the handle, must be non-NULL, valid and readable
 * @param out destination buffer, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_read_buffer(ucb_file* file, ucb_buffer* out, ucb_error** perr);

/**
 * @brief Write all data held by a buffer
 *
 * Convenience wrapper around @ref ucb_file_write_full that writes @c buf->size
 * bytes from @c buf->data. An empty buffer is a successful no-op.
 *
 * @param file the handle, must be non-NULL, valid and writable
 * @param buf source buffer, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 * @see ucb_file_write_full
 */
UCB_API bool ucb_file_write_buffer(ucb_file* file, const ucb_buffer* buf, ucb_error** perr);

/* -------------------------------------------------------------------------- */
/*                             Position and sync                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Move the read/write position of a seekable handle
 *
 * Requires @ref UCB_FILE_CAP_SEEK; otherwise @ref UCB_ERROR_NOT_IMPLEMENTED is
 * thrown.
 *
 * @param file the handle, must be non-NULL and valid
 * @param offset byte offset, interpreted according to @p whence
 * @param whence origin of the offset
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_seek(ucb_file* file,
                           int64_t offset,
                           ucb_file_whence whence,
                           ucb_error** perr);

/**
 * @brief Get the current read/write position of a seekable handle
 *
 * Requires @ref UCB_FILE_CAP_SEEK; otherwise @ref UCB_ERROR_NOT_IMPLEMENTED is
 * thrown.
 *
 * @param file the handle, must be non-NULL and valid
 * @param out_pos destination for the position, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_tell(const ucb_file* file, int64_t* out_pos, ucb_error** perr);

/**
 * @brief Flush any buffered data associated with the handle
 *
 * The data plane is unbuffered, so for handled file descriptors this is a
 * no-op that always succeeds. It exists so callers can express intent and so a
 * buffered backend can be added later without an API change.
 *
 * @param file the handle, must be non-NULL and valid
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_flush(ucb_file* file, ucb_error** perr);

/**
 * @brief Flush data to the underlying storage device
 *
 * Calls @c fsync on POSIX and @c FlushFileBuffers on Windows.
 *
 * @param file the handle, must be non-NULL and valid
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_file_sync(ucb_file* file, ucb_error** perr);

/* -------------------------------------------------------------------------- */
/*                                   Query                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get the kind of resource a handle refers to
 * @param file the handle, may be UCB_NULL
 * @return the kind, or @ref UCB_FILE_KIND_UNKNOWN if @p file is invalid
 */
UCB_API ucb_file_kind ucb_file_get_kind(const ucb_file* file);

/**
 * @brief Get the capability bitmask of a handle
 * @param file the handle, may be UCB_NULL
 * @return bitwise OR of @ref ucb_file_caps, or 0 if @p file is invalid
 */
UCB_API unsigned ucb_file_get_caps(const ucb_file* file);

/**
 * @brief Check whether a handle is valid and usable
 * @param file the handle, may be UCB_NULL
 * @return true if the handle refers to an open resource
 */
UCB_API bool ucb_file_is_valid(const ucb_file* file);

/* -------------------------------------------------------------------------- */
/*                              Standard streams                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get a non-owning view of the standard input stream
 *
 * The returned object is a shared singleton. Freeing or closing it never closes
 * the underlying OS stream. Use @ref ucb_file_dup to obtain an owning handle.
 *
 * @return the standard input handle, never UCB_NULL
 */
UCB_API ucb_file* ucb_file_stdin(void);

/**
 * @brief Get a non-owning view of the standard output stream
 * @see ucb_file_stdin
 * @return the standard output handle, never UCB_NULL
 */
UCB_API ucb_file* ucb_file_stdout(void);

/**
 * @brief Get a non-owning view of the standard error stream
 * @see ucb_file_stdin
 * @return the standard error handle, never UCB_NULL
 */
UCB_API ucb_file* ucb_file_stderr(void);

/* -------------------------------------------------------------------------- */
/*                                Wait helpers                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wait until a handle becomes readable
 *
 * @p timeout_ms follows the usual convention: negative waits indefinitely,
 * @c 0 polls once, and a positive value is a timeout in milliseconds.
 *
 * On timeout the function returns false with no error thrown. On failure it
 * returns false with the error thrown into @p perr, so callers distinguish the
 * two with @ref UCB_IS_THROWN or a local error pointer.
 *
 * @param file the handle, must be non-NULL, valid and support
 *             @ref UCB_FILE_CAP_POLL
 * @param timeout_ms timeout in milliseconds, negative for infinite
 * @param perr optional location to store the error on failure
 * @return true if readable, false on timeout or error
 */
UCB_API bool ucb_file_wait_readable(ucb_file* file, int timeout_ms, ucb_error** perr);

/**
 * @brief Wait until a handle becomes writable
 *
 * Windows anonymous pipes cannot report writability without overlapped I/O, so
 * this throws @ref UCB_ERROR_NOT_IMPLEMENTED there. On POSIX it uses @c poll.
 *
 * @param file the handle, must be non-NULL, valid and support
 *             @ref UCB_FILE_CAP_POLL
 * @param timeout_ms timeout in milliseconds, negative for infinite
 * @param perr optional location to store the error on failure
 * @return true if writable, false on timeout or error
 */
UCB_API bool ucb_file_wait_writable(ucb_file* file, int timeout_ms, ucb_error** perr);

#endif // UCB_FILE_H
