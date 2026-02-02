/**
 * @file errcodes.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Error codes
 */

#ifndef UCB_ERROR_CODES_H
#define UCB_ERROR_CODES_H

enum ucb_error_code
{
    UCB_OK = 0,                  //
    UCB_ERROR_WARNING,           // Used for warnings only
    UCB_ERROR_OUT_OF_MEMORY,     // Out of memory
    UCB_ERROR_INVALID_ARG,       // Invalid argument
    UCB_ERROR_UNHANDLED_ERROR,   // Previous error set but not handled by caller
    UCB_ERROR_OUT_OF_BOUNDS,     // Index or operation out of bounds
    UCB_ERROR_INVALID_UTF8,      // Invalid UTF-8 sequence
    UCB_ERROR_INVALID_CODEPOINT, //
    UCB_ERROR_INTERNAL,          // Internal error, should not happen
    UCB_ERROR_INVALID_STATE,     // Invalid state. The error message should describe the state
    UCB_ERROR_NOT_IMPLEMENTED,
    UCB_ERROR_BUFFER,
    UCB_ERROR_LOCKED,
    UCB_ERROR_MAX_SIZE,
    /**
     * @brief FATAL ERROR: Attempt to free memory that was not allocated by ucb.
     * May also catch double free's. This is only enabled in debug builds
     */
    UCB_ERROR_INVALID_ALLOC,
    UCB_ERROR_THREAD_BUSY, // Thread already running
    /**
     * @brief Mutex is locked by another thread
     * A fatal error if releasing a mutex, as this is undefined behavior.
     */
    UCB_ERROR_MUTEX_LOCKED,

    UCB_ERRSYS_UNKNOWN = 999, // Unknown system error

    /*
     * POSIX error codes.
     * All errors are not available on all platforms, and numbers are not
     * portable. strerror must be used against original value.
     * Note that EWOULDBLOCK = EAGAIN.
     * STRUNCATE is Windows addition and handled separately in
     */
    UCB_ERRSYS_NOERROR         = 1000, // No error
    UCB_ERRSYS_EPERM           = 1001, // Operation not permitted
    UCB_ERRSYS_ENOENT          = 1002, // No such file or directory
    UCB_ERRSYS_ESRCH           = 1003, // No such process
    UCB_ERRSYS_EINTR           = 1004, // Interrupted system call
    UCB_ERRSYS_EIO             = 1005, // Input/output error
    UCB_ERRSYS_ENXIO           = 1006, // No such device or address
    UCB_ERRSYS_E2BIG           = 1007, // Argument list too long
    UCB_ERRSYS_ENOEXEC         = 1008, // Exec format error
    UCB_ERRSYS_EBADF           = 1009, // Bad file descriptor
    UCB_ERRSYS_ECHILD          = 1010, // No child processes
    UCB_ERRSYS_EAGAIN          = 1011, // Resource temporarily unavailable (same as EWOULDBLOCK)
    UCB_ERRSYS_ENOMEM          = 1012, // Cannot allocate memory
    UCB_ERRSYS_EACCES          = 1013, // Permission denied
    UCB_ERRSYS_EFAULT          = 1014, // Bad address
    UCB_ERRSYS_ENOTBLK         = 1015, // Block device required
    UCB_ERRSYS_EBUSY           = 1016, // Device or resource busy
    UCB_ERRSYS_EEXIST          = 1017, // File exists
    UCB_ERRSYS_EXDEV           = 1018, // Invalid cross-device link
    UCB_ERRSYS_ENODEV          = 1019, // No such device
    UCB_ERRSYS_ENOTDIR         = 1020, // Not a directory
    UCB_ERRSYS_EISDIR          = 1021, // Is a directory
    UCB_ERRSYS_EINVAL          = 1022, // Invalid argument
    UCB_ERRSYS_ENFILE          = 1023, // Too many open files in system
    UCB_ERRSYS_EMFILE          = 1024, // Too many open files
    UCB_ERRSYS_ENOTTY          = 1025, // Inappropriate ioctl for device
    UCB_ERRSYS_ETXTBSY         = 1026, // Text file busy
    UCB_ERRSYS_EFBIG           = 1027, // File too large
    UCB_ERRSYS_ENOSPC          = 1028, // No space left on device
    UCB_ERRSYS_ESPIPE          = 1029, // Illegal seek
    UCB_ERRSYS_EROFS           = 1030, // Read-only file system
    UCB_ERRSYS_EMLINK          = 1031, // Too many links
    UCB_ERRSYS_EPIPE           = 1032, // Broken pipe
    UCB_ERRSYS_EDOM            = 1033, // Numerical argument out of domain
    UCB_ERRSYS_ERANGE          = 1034, // Numerical result out of range
    UCB_ERRSYS_EDEADLK         = 1035, // Resource deadlock avoided
    UCB_ERRSYS_ENAMETOOLONG    = 1036, // File name too long
    UCB_ERRSYS_ENOLCK          = 1037, // No locks available
    UCB_ERRSYS_ENOSYS          = 1038, // Function not implemented
    UCB_ERRSYS_ENOTEMPTY       = 1039, // Directory not empty
    UCB_ERRSYS_ELOOP           = 1040, // Too many levels of symbolic links
    UCB_ERRSYS_ENOMSG          = 1042, // No message of desired type
    UCB_ERRSYS_EIDRM           = 1043, // Identifier removed
    UCB_ERRSYS_ECHRNG          = 1044, // Channel number out of range
    UCB_ERRSYS_EL2NSYNC        = 1045, // Level 2 not synchronized
    UCB_ERRSYS_EL3HLT          = 1046, // Level 3 halted
    UCB_ERRSYS_EL3RST          = 1047, // Level 3 reset
    UCB_ERRSYS_ELNRNG          = 1048, // Link number out of range
    UCB_ERRSYS_EUNATCH         = 1049, // Protocol driver not attached
    UCB_ERRSYS_ENOCSI          = 1050, // No CSI structure available
    UCB_ERRSYS_EL2HLT          = 1051, // Level 2 halted
    UCB_ERRSYS_EBADE           = 1052, // Invalid exchange
    UCB_ERRSYS_EBADR           = 1053, // Invalid request descriptor
    UCB_ERRSYS_EXFULL          = 1054, // Exchange full
    UCB_ERRSYS_ENOANO          = 1055, // No anode
    UCB_ERRSYS_EBADRQC         = 1056, // Invalid request code
    UCB_ERRSYS_EBADSLT         = 1057, // Invalid slot
    UCB_ERRSYS_EBFONT          = 1059, // Bad font file format
    UCB_ERRSYS_ENOSTR          = 1060, // Device not a stream
    UCB_ERRSYS_ENODATA         = 1061, // No data available
    UCB_ERRSYS_ETIME           = 1062, // Timer expired
    UCB_ERRSYS_ENOSR           = 1063, // Out of streams resources
    UCB_ERRSYS_ENONET          = 1064, // Machine is not on the network
    UCB_ERRSYS_ENOPKG          = 1065, // Package not installed
    UCB_ERRSYS_EREMOTE         = 1066, // Object is remote
    UCB_ERRSYS_ENOLINK         = 1067, // Link has been severed
    UCB_ERRSYS_EADV            = 1068, // Advertise error
    UCB_ERRSYS_ESRMNT          = 1069, // Srmount error
    UCB_ERRSYS_ECOMM           = 1070, // Communication error on send
    UCB_ERRSYS_EPROTO          = 1071, // Protocol error
    UCB_ERRSYS_EMULTIHOP       = 1072, // Multihop attempted
    UCB_ERRSYS_EDOTDOT         = 1073, // RFS specific error
    UCB_ERRSYS_EBADMSG         = 1074, // Bad message
    UCB_ERRSYS_EOVERFLOW       = 1075, // Value too large for defined data type
    UCB_ERRSYS_ENOTUNIQ        = 1076, // Name not unique on network
    UCB_ERRSYS_EBADFD          = 1077, // File descriptor in bad state
    UCB_ERRSYS_EREMCHG         = 1078, // Remote address changed
    UCB_ERRSYS_ELIBACC         = 1079, // Can not access a needed shared library
    UCB_ERRSYS_ELIBBAD         = 1080, // Accessing a corrupted shared library
    UCB_ERRSYS_ELIBSCN         = 1081, // .lib section in a.out corrupted
    UCB_ERRSYS_ELIBMAX         = 1082, // Attempting to link in too many shared libraries
    UCB_ERRSYS_ELIBEXEC        = 1083, // Cannot exec a shared library directly
    UCB_ERRSYS_EILSEQ          = 1084, // Invalid or incomplete multibyte or wide character
    UCB_ERRSYS_ERESTART        = 1085, // Interrupted system call should be restarted
    UCB_ERRSYS_ESTRPIPE        = 1086, // Streams pipe error
    UCB_ERRSYS_EUSERS          = 1087, // Too many users
    UCB_ERRSYS_ENOTSOCK        = 1088, // Socket operation on non-socket
    UCB_ERRSYS_EDESTADDRREQ    = 1089, // Destination address required
    UCB_ERRSYS_EMSGSIZE        = 1090, // Message too long
    UCB_ERRSYS_EPROTOTYPE      = 1091, // Protocol wrong type for socket
    UCB_ERRSYS_ENOPROTOOPT     = 1092, // Protocol not available
    UCB_ERRSYS_EPROTONOSUPPORT = 1093, // Protocol not supported
    UCB_ERRSYS_ESOCKTNOSUPPORT = 1094, // Socket type not supported
    UCB_ERRSYS_EOPNOTSUPP      = 1095, // Operation not supported
    UCB_ERRSYS_EPFNOSUPPORT    = 1096, // Protocol family not supported
    UCB_ERRSYS_EAFNOSUPPORT    = 1097, // Address family not supported by protocol
    UCB_ERRSYS_EADDRINUSE      = 1098, // Address already in use
    UCB_ERRSYS_EADDRNOTAVAIL   = 1099, // Cannot assign requested address
    UCB_ERRSYS_ENETDOWN        = 1100, // Network is down
    UCB_ERRSYS_ENETUNREACH     = 1101, // Network is unreachable
    UCB_ERRSYS_ENETRESET       = 1102, // Network dropped connection on reset
    UCB_ERRSYS_ECONNABORTED    = 1103, // Software caused connection abort
    UCB_ERRSYS_ECONNRESET      = 1104, // Connection reset by peer
    UCB_ERRSYS_ENOBUFS         = 1105, // No buffer space available
    UCB_ERRSYS_EISCONN         = 1106, // Transport endpoint is already connected
    UCB_ERRSYS_ENOTCONN        = 1107, // Transport endpoint is not connected
    UCB_ERRSYS_ESHUTDOWN       = 1108, // Cannot send after transport endpoint shutdown
    UCB_ERRSYS_ETOOMANYREFS    = 1109, // Too many references: cannot splice
    UCB_ERRSYS_ETIMEDOUT       = 1110, // Connection timed out
    UCB_ERRSYS_ECONNREFUSED    = 1111, // Connection refused
    UCB_ERRSYS_EHOSTDOWN       = 1112, // Host is down
    UCB_ERRSYS_EHOSTUNREACH    = 1113, // No route to host
    UCB_ERRSYS_EALREADY        = 1114, // Operation already in progress
    UCB_ERRSYS_EINPROGRESS     = 1115, // Operation now in progress
    UCB_ERRSYS_ESTALE          = 1116, // Stale file handle
    UCB_ERRSYS_EUCLEAN         = 1117, // Structure needs cleaning
    UCB_ERRSYS_ENOTNAM         = 1118, // Not a Xenix named type file
    UCB_ERRSYS_ENAVAIL         = 1119, // No Xenix semaphores available
    UCB_ERRSYS_EISNAM          = 1120, // Is a named type file
    UCB_ERRSYS_EREMOTEIO       = 1121, // Remote I/O error
    UCB_ERRSYS_EDQUOT          = 1122, // Disk quota exceeded
    UCB_ERRSYS_ENOMEDIUM       = 1123, // No medium found
    UCB_ERRSYS_EMEDIUMTYPE     = 1124, // Wrong medium type
    UCB_ERRSYS_ECANCELED       = 1125, // Operation canceled
    UCB_ERRSYS_ENOKEY          = 1126, // Required key not available
    UCB_ERRSYS_EKEYEXPIRED     = 1127, // Key has expired
    UCB_ERRSYS_EKEYREVOKED     = 1128, // Key has been revoked
    UCB_ERRSYS_EKEYREJECTED    = 1129, // Key was rejected by service
    UCB_ERRSYS_EOWNERDEAD      = 1130, // Owner died
    UCB_ERRSYS_ENOTRECOVERABLE = 1131, // State not recoverable
    UCB_ERRSYS_ERFKILL         = 1132, // Operation not possible due to RF-kill
    UCB_ERRSYS_EHWPOISON       = 1133, // Memory page has hardware error
    UCB_ERRSYS_ENOTSUP         = 1134, // Not supported parameter or option (May be same as EOPNOTSUPP)
    UCB_ERRSYS_STRUNCATE       = 1200, // String truncated (Windows "Secure CRT" extension)

    /*
     * Additional errors for WIN32 that doesn't map to any of the above
     * Note, the use of general categories
     */
    UCB_ERRSYS_WIN_GENERIC           = 2000,
    UCB_ERRSYS_WIN_INVALID_HANDLE    = 2001, // The handle is invalid
    UCB_ERRSYS_WIN_CURRENT_DIRECTORY = 2002, // The directory cannot be removed
    UCB_ERRSYS_WIN_ERROR_DIRECTORY   = 2003, // The directory name is invalid
};

#endif // UCB_ERROR_CODES_H
