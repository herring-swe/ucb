/**
 * @file file.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief file and pipe tests
 */

#include <ucb/buffer.h>
#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/pipe.h>

#include <doctest.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>

#include <cerrno>
#else
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#endif

static std::string temp_path(const char* suffix)
{
    static unsigned counter = 0;
#ifdef _WIN32
    unsigned pid = static_cast<unsigned>(_getpid());
#else
    unsigned pid = static_cast<unsigned>(getpid());
#endif
    char buf[96];
    std::snprintf(buf, sizeof(buf), "ucb_test_io_%u_%u_%s", pid, counter++, suffix);
    return std::string(buf);
}

static int remove_path(const std::string& path)
{
#ifdef _WIN32
    // std::remove uses the ANSI code page; convert the UTF-8 path to UTF-16.
    wchar_t* wpath = ucb_cstr_to_wchar(path.c_str(), 0, UCB_NULL, UCB_NULL);
    if (!wpath)
        return -1;
    int rc = _wremove(wpath);
    ucb_free(wpath);
    return rc;
#else
    return std::remove(path.c_str());
#endif
}

static bool read_all_string(ucb_file* file, std::string& out)
{
    ucb_buffer* buf = ucb_buffer_new_heap(64);
    bool ok = ucb_file_read_buffer(file, buf, nullptr);
    if (ok)
        out.assign(buf->data, buf->size);
    ucb_buffer_free(buf);
    return ok;
}

TEST_SUITE_BEGIN("file");

TEST_CASE("file - open close")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("open.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);
    CHECK(err == UCB_NULL);
    CHECK(ucb_file_is_valid(file));
    CHECK(ucb_file_get_kind(file) == UCB_FILE_KIND_REGULAR);
    CHECK((ucb_file_get_caps(file) & UCB_FILE_CAP_WRITE) != 0);
    CHECK((ucb_file_get_caps(file) & UCB_FILE_CAP_SEEK) != 0);

    REQUIRE(ucb_file_write(file, "hello", 5, &err) == 5);
    CHECK(ucb_file_close(file, &err));
    // Idempotent close
    CHECK(ucb_file_close(file, &err));
    CHECK_FALSE(ucb_file_is_valid(file));
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    std::string content;
    REQUIRE(read_all_string(file, content));
    CHECK(content == "hello");
    ucb_file_free(file);

    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - read write")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("rw.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(),
                      UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_READ | UCB_FILE_WRITE,
                      &err);
    REQUIRE(file != UCB_NULL);

    CHECK(ucb_file_write(file, "0123456789", 10, &err) == 10);
    REQUIRE(ucb_file_seek(file, 0, UCB_FILE_SEEK_SET, &err));

    char buf[16] = {0};
    CHECK(ucb_file_read(file, buf, 4, &err) == 4);
    CHECK(std::memcmp(buf, "0123", 4) == 0);
    CHECK(ucb_file_read(file, buf, sizeof(buf), &err) == 6);
    CHECK(std::memcmp(buf, "456789", 6) == 0);
    // End of input
    CHECK(ucb_file_read(file, buf, sizeof(buf), &err) == 0);

    REQUIRE(ucb_file_seek(file, 0, UCB_FILE_SEEK_SET, &err));
    std::string content;
    REQUIRE(read_all_string(file, content));
    CHECK(content == "0123456789");

    ucb_file_free(file);
    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - read all growth")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("growth.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(),
                      UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_READ | UCB_FILE_WRITE,
                      &err);
    REQUIRE(file != UCB_NULL);

    std::string payload(100000, 'x');
    REQUIRE(ucb_file_write_full(file, payload.data(), payload.size(), &err));
    REQUIRE(ucb_file_seek(file, 0, UCB_FILE_SEEK_SET, &err));

    ucb_buffer* buf = ucb_buffer_new_heap(64);
    REQUIRE(buf->grow_func == UCB_NULL);
    REQUIRE(ucb_file_read_buffer(file, buf, &err));
    CHECK(buf->size == payload.size());
    CHECK(buf->alloc >= buf->size);
    // The temporary exponential grow strategy must be restored.
    CHECK(buf->grow_func == UCB_NULL);
    ucb_buffer_free(buf);

    ucb_file_free(file);
    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - write_buffer")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("writebuf.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);

    ucb_buffer* buf = ucb_buffer_new_heap(8);
    REQUIRE(buf != UCB_NULL);
    REQUIRE(ucb_buffer_push(buf, "hello ", 6));
    REQUIRE(ucb_buffer_push(buf, "buffer", 6));
    REQUIRE(ucb_file_write_buffer(file, buf, &err));
    ucb_buffer_free(buf);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    std::string content;
    REQUIRE(read_all_string(file, content));
    CHECK(content == "hello buffer");
    ucb_file_free(file);

    // An empty buffer is a successful no-op.
    file = ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);
    ucb_buffer* empty = ucb_buffer_new_heap(4);
    REQUIRE(empty != UCB_NULL);
    REQUIRE(ucb_file_write_buffer(file, empty, &err));
    ucb_buffer_free(empty);
    ucb_file_free(file);

    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - write_full")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("writefull.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);

    CHECK(ucb_file_write_full(file, "full", 4, &err));
    CHECK(ucb_file_write_full(file, "full", 0, &err));

    // A larger payload must be fully written even if a single write is short.
    std::string payload(100000, 'w');
    CHECK(ucb_file_write_full(file, payload.data(), payload.size(), &err));

    ucb_file_free(file);
    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - read_full")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("readfull.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(),
                      UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_READ | UCB_FILE_WRITE,
                      &err);
    REQUIRE(file != UCB_NULL);
    REQUIRE(ucb_file_write_full(file, "0123456789", 10, &err));
    REQUIRE(ucb_file_seek(file, 0, UCB_FILE_SEEK_SET, &err));

    SUBCASE("exact fit")
    {
        char buf[10] = {0};
        size_t size = 0;
        REQUIRE(ucb_file_read_full(file, buf, sizeof(buf), &size, &err));
        CHECK(size == 10);
        CHECK(std::memcmp(buf, "0123456789", 10) == 0);
    }

    SUBCASE("buffer too small")
    {
        char buf[4] = {0};
        size_t size = 0;
        CHECK_FALSE(ucb_file_read_full(file, buf, sizeof(buf), &size, &err));
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERROR_OUT_OF_BOUNDS);
        CHECK(size == 10); // required size, so the caller can allocate
        ucb_error_clear(&err);
    }

    SUBCASE("from a non-zero position")
    {
        REQUIRE(ucb_file_seek(file, 6, UCB_FILE_SEEK_SET, &err));
        char buf[4] = {0};
        size_t size = 0;
        REQUIRE(ucb_file_read_full(file, buf, sizeof(buf), &size, &err));
        CHECK(size == 4);
        CHECK(std::memcmp(buf, "6789", 4) == 0);
    }

    ucb_file_free(file);
    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - read_full non-seekable")
{
    UCB_MEMTRACK_PUSH();

    ucb_file* read_end = UCB_NULL;
    ucb_file* write_end = UCB_NULL;
    ucb_error* err = UCB_NULL;
    REQUIRE(ucb_pipe_create(&read_end, &write_end, 0, &err));

    char buf[8] = {0};
    size_t size = 0;
    CHECK_FALSE(ucb_file_read_full(read_end, buf, sizeof(buf), &size, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_NOT_IMPLEMENTED);
    ucb_error_clear(&err);

    ucb_file_free(read_end);
    ucb_file_free(write_end);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - seek")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("seek.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(),
                      UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_READ | UCB_FILE_WRITE,
                      &err);
    REQUIRE(file != UCB_NULL);
    REQUIRE(ucb_file_write(file, "0123456789", 10, &err) == 10);

    int64_t pos = -1;
    REQUIRE(ucb_file_seek(file, 5, UCB_FILE_SEEK_SET, &err));
    REQUIRE(ucb_file_tell(file, &pos, &err));
    CHECK(pos == 5);

    char ch = 0;
    CHECK(ucb_file_read(file, &ch, 1, &err) == 1);
    CHECK(ch == '5');

    REQUIRE(ucb_file_seek(file, -2, UCB_FILE_SEEK_CUR, &err));
    REQUIRE(ucb_file_tell(file, &pos, &err));
    CHECK(pos == 4);

    REQUIRE(ucb_file_seek(file, -2, UCB_FILE_SEEK_END, &err));
    REQUIRE(ucb_file_tell(file, &pos, &err));
    CHECK(pos == 8);

    char tail[3] = {0};
    CHECK(ucb_file_read(file, tail, 2, &err) == 2);
    CHECK(std::memcmp(tail, "89", 2) == 0);

    ucb_file_free(file);
    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - append truncate")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("append.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);
    REQUIRE(ucb_file_write(file, "abc", 3, &err) == 3);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_WRITE | UCB_FILE_APPEND, &err);
    REQUIRE(file != UCB_NULL);
    REQUIRE(ucb_file_write(file, "def", 3, &err) == 3);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    std::string content;
    REQUIRE(read_all_string(file, content));
    CHECK(content == "abcdef");
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_WRITE | UCB_FILE_TRUNCATE, &err);
    REQUIRE(file != UCB_NULL);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    content.clear();
    REQUIRE(read_all_string(file, content));
    CHECK(content.empty());
    ucb_file_free(file);

    // Truncate combined with append must open on all platforms. On Windows this
    // previously requested FILE_APPEND_DATA without GENERIC_WRITE and failed.
    file = ucb_file_open(path.c_str(), UCB_FILE_WRITE | UCB_FILE_APPEND | UCB_FILE_TRUNCATE, &err);
    REQUIRE(file != UCB_NULL);
    REQUIRE(ucb_file_write(file, "xy", 2, &err) == 2);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    content.clear();
    REQUIRE(read_all_string(file, content));
    CHECK(content == "xy");
    ucb_file_free(file);

    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - errors")
{
    UCB_MEMTRACK_PUSH();

    std::string path = temp_path("errors.tmp");
    ucb_error* err = UCB_NULL;

    SUBCASE("missing file")
    {
        ucb_file* file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
        CHECK(file == UCB_NULL);
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERRSYS_ENOENT);
        ucb_error_clear(&err);
    }

    SUBCASE("read on write only")
    {
        ucb_file* file =
            ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
        REQUIRE(file != UCB_NULL);
        char buf[4];
        CHECK(ucb_file_read(file, buf, sizeof(buf), &err) == -1);
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERROR_NOT_IMPLEMENTED);
        ucb_error_clear(&err);
        ucb_file_free(file);
        CHECK(remove_path(path) == 0);
    }

    SUBCASE("directory for write")
    {
        ucb_file* file = ucb_file_open(".", UCB_FILE_WRITE, &err);
        CHECK(file == UCB_NULL);
        REQUIRE(err != UCB_NULL);
        ucb_error_clear(&err);
    }

    ucb_error_clear(&err);
    UCB_MEMTRACK_POP();
}

#ifndef _WIN32
TEST_CASE("file - unsupported type")
{
    UCB_MEMTRACK_PUSH();

    // A directory is not a supported ucb_file resource; opening one for reading
    // must be rejected rather than exposed with a device capability set.
    ucb_error* err = UCB_NULL;
    ucb_file* file = ucb_file_open(".", UCB_FILE_READ, &err);
    CHECK(file == UCB_NULL);
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_NOT_IMPLEMENTED);
    ucb_error_clear(&err);

    UCB_MEMTRACK_POP();
}
#endif // !_WIN32

TEST_CASE("file - std streams")
{
    UCB_MEMTRACK_PUSH();

    ucb_file* in = ucb_file_stdin();
    ucb_file* out = ucb_file_stdout();
    ucb_file* err = ucb_file_stderr();

    REQUIRE(in != UCB_NULL);
    REQUIRE(out != UCB_NULL);
    REQUIRE(err != UCB_NULL);
    CHECK(in != out);
    CHECK(out != err);

    // Non-owning views are never closed by free.
    ucb_file_free(out);
    CHECK(ucb_file_is_valid(out) == ucb_file_is_valid(ucb_file_stdout()));

#ifdef _WIN32
    CHECK(ucb_file_get_fd(ucb_file_stdout()) == 1);
#else
    CHECK(ucb_file_get_fd(ucb_file_stdout()) == 1);
#endif

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - interop")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;

    // Borrowed standard output descriptor.
    ucb_file* file = ucb_file_from_fd(1, false, &err);
    REQUIRE(file != UCB_NULL);
    CHECK(ucb_file_get_fd(file) == 1);
    CHECK(ucb_file_is_valid(file));
    ucb_file_free(file);
    CHECK(err == UCB_NULL);

#ifndef _WIN32
    // Owning descriptor round trip.
    std::string path = temp_path("interop.tmp");
    int fd = ::open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
    REQUIRE(fd >= 0);

    file = ucb_file_from_fd(fd, true, &err);
    REQUIRE(file != UCB_NULL);
    CHECK(ucb_file_get_fd(file) == fd);
    REQUIRE(ucb_file_write(file, "fd", 2, &err) == 2);
    ucb_file_free(file);

    fd = ::open(path.c_str(), O_RDONLY);
    REQUIRE(fd >= 0);
    char buf[4] = {0};
    CHECK(::read(fd, buf, sizeof(buf)) == 2);
    CHECK(std::memcmp(buf, "fd", 2) == 0);
    ::close(fd);
    CHECK(remove_path(path) == 0);
#endif

    UCB_MEMTRACK_POP();
}

TEST_CASE("file - pipe")
{
    UCB_MEMTRACK_PUSH();

    ucb_file* read_end = UCB_NULL;
    ucb_file* write_end = UCB_NULL;
    ucb_error* err = UCB_NULL;

    REQUIRE(ucb_pipe_create(&read_end, &write_end, 0, &err));
    REQUIRE(read_end != UCB_NULL);
    REQUIRE(write_end != UCB_NULL);
    CHECK(ucb_file_get_kind(read_end) == UCB_FILE_KIND_PIPE);
    CHECK(ucb_file_get_kind(write_end) == UCB_FILE_KIND_PIPE);
    CHECK((ucb_file_get_caps(read_end) & UCB_FILE_CAP_READ) != 0);
    CHECK((ucb_file_get_caps(read_end) & UCB_FILE_CAP_SEEK) == 0);

#ifndef _WIN32
    // A close-on-exec pipe must have FD_CLOEXEC set on both descriptors.
    {
        ucb_file* cr = UCB_NULL;
        ucb_file* cw = UCB_NULL;
        REQUIRE(ucb_pipe_create(&cr, &cw, UCB_FILE_CLOEXEC, &err));
        int rfd = ucb_file_get_fd(cr);
        int wfd = ucb_file_get_fd(cw);
        REQUIRE(rfd >= 0);
        REQUIRE(wfd >= 0);
        CHECK((fcntl(rfd, F_GETFD) & FD_CLOEXEC) != 0);
        CHECK((fcntl(wfd, F_GETFD) & FD_CLOEXEC) != 0);
        ucb_file_free(cr);
        ucb_file_free(cw);
    }
#endif

    // A pipe cannot be seeked.
    CHECK_FALSE(ucb_file_seek(read_end, 0, UCB_FILE_SEEK_SET, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_NOT_IMPLEMENTED);
    ucb_error_clear(&err);

    REQUIRE(ucb_file_write(write_end, "ping", 4, &err) == 4);

    CHECK(ucb_file_wait_readable(read_end, 0, &err));

    char buf[8] = {0};
    CHECK(ucb_file_read(read_end, buf, sizeof(buf), &err) == 4);
    CHECK(std::memcmp(buf, "ping", 4) == 0);

    // Timeout with no data. On Windows an anonymous pipe read handle does not
    // reliably report the drained state through WaitForSingleObject, so this
    // strict poll check is limited to POSIX.
#ifndef _WIN32
    CHECK_FALSE(ucb_file_wait_readable(read_end, 0, &err));
    CHECK(err == UCB_NULL);
#endif

#ifdef _WIN32
    CHECK_FALSE(ucb_file_wait_writable(write_end, 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_NOT_IMPLEMENTED);
    ucb_error_clear(&err);
#else
    CHECK(ucb_file_wait_writable(write_end, 0, &err));
#endif

    // Closing the write end signals end of input.
    ucb_file_free(write_end);
    CHECK(ucb_file_wait_readable(read_end, 0, &err));
    CHECK(ucb_file_read(read_end, buf, sizeof(buf), &err) == 0);
    ucb_file_free(read_end);

    CHECK(err == UCB_NULL);

    UCB_MEMTRACK_POP();
}

#ifdef _WIN32
TEST_CASE("file - close crt descriptor")
{
    UCB_MEMTRACK_PUSH();

    std::string path_a = temp_path("crtclose_a.tmp");
    std::string path_b = temp_path("crtclose_b.tmp");
    ucb_error* err = UCB_NULL;

    int fd = _open(path_a.c_str(), _O_CREAT | _O_RDWR | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    REQUIRE(fd >= 0);

    ucb_file* file = ucb_file_from_fd(fd, true, &err);
    REQUIRE(file != UCB_NULL);
    // Must close through the CRT, not CloseHandle, so the descriptor slot is
    // released. The next _open reuses the lowest free descriptor.
    ucb_file_free(file);

    int fd2 =
        _open(path_b.c_str(), _O_CREAT | _O_RDWR | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    REQUIRE(fd2 >= 0);
    CHECK(fd2 == fd);
    _close(fd2);

    CHECK(remove_path(path_a) == 0);
    CHECK(remove_path(path_b) == 0);

    UCB_MEMTRACK_POP();
}
#endif // _WIN32

#ifdef _WIN32
TEST_CASE("file - unicode path")
{
    UCB_MEMTRACK_PUSH();

    // UTF-8 encoded "ucb_test_åäö_<pid>.tmp"
    std::string path = temp_path("unicode_\xC3\xA5\xC3\xA4\xC3\xB6.tmp");
    ucb_error* err = UCB_NULL;

    ucb_file* file =
        ucb_file_open(path.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    REQUIRE(file != UCB_NULL);
    CHECK(err == UCB_NULL);
    REQUIRE(ucb_file_write(file, "unicode", 7, &err) == 7);
    ucb_file_free(file);

    file = ucb_file_open(path.c_str(), UCB_FILE_READ, &err);
    REQUIRE(file != UCB_NULL);
    std::string content;
    REQUIRE(read_all_string(file, content));
    CHECK(content == "unicode");
    ucb_file_free(file);

    CHECK(remove_path(path) == 0);

    UCB_MEMTRACK_POP();
}
#endif // _WIN32

TEST_SUITE_END();
