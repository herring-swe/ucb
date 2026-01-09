/**
 * @file btrace.c
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Cross-platform backtrace implementation
 */

#include "ucb/btrace.h"

#include "ucb/buffer_private.h"
#include "ucb/config.h"
#include "ucb/cstring.h"
#include "ucb/debug.h"
#include "ucb/error.h"
#include "ucb/memory.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <DbgHelp.h>
#else
#include <execinfo.h>
#endif

#ifdef _WIN32
static size_t grow_25(ucb_buffer* buf, size_t size_needed)
{
    size_t cap = (buf->capacity * 125) / 100;
    while (size_needed > cap - buf->used)
        cap = (cap * 125) / 100;
    return cap;
}

static int format_line(ucb_buffer* buf, void* address, HANDLE process, PSYMBOL_INFO symbol)
{
    // Resolve symbol
    int slen = -1;
    if (SymFromAddr(process, (DWORD64)address, 0, symbol))
    {
        DWORD displacement_line = 0;
        IMAGEHLP_LINE64 line    = {0};
        line.SizeOfStruct       = sizeof(line);

        // Resolve source file/line
        if (SymGetLineFromAddr64(process, (DWORD64)address, &displacement_line, &line))
        {
            // Format: "module!symbol+offset (file:line)"
            IMAGEHLP_MODULE64 module;
            ZeroMemory(&module, sizeof(module));
            module.SizeOfStruct = sizeof(module);
            if (SymGetModuleInfo64(process, (DWORD64)address, &module))
            {
                slen = ucb_buffer_push_format(buf, "%s!%s+0x%llX (%s:%lu)", module.ModuleName,
                                              symbol->Name, (DWORD64)address - symbol->Address,
                                              line.FileName, line.LineNumber);
            }
            else
            {
                slen = ucb_buffer_push_format(buf, "%s+0x%llX (%s:%lu)", symbol->Name,
                                              (DWORD64)address - symbol->Address, line.FileName,
                                              line.LineNumber);
            }
        }
        else
        {
            // Fallback: symbol + offset
            slen = ucb_buffer_push_format(buf, "%s+0x%llX [0x%p]", symbol->Name,
                                          (DWORD64)address - symbol->Address, address);
        }
    }
    else
    {
        // Fallback: just address
        slen = ucb_buffer_push_format(buf, "[0x%p]", address);
    }
    return slen;
}
#endif

ucb_btrace* ucb_btrace_new()
{
    return (ucb_btrace*)calloc(1, sizeof(ucb_btrace));
}

void ucb_btrace_init(ucb_btrace* bt)
{
    UCB_VERIFY_ARGS(bt);

    bt->count = 0;
    bt->strs  = NULL;
}

void ucb_btrace_release(ucb_btrace* bt)
{
    if (bt)
    {
        if (bt->strs)
            free(bt->strs);
        bt->count = 0;
        bt->strs  = UCB_NULL;
    }
}

void ucb_btrace_free(ucb_btrace* bt)
{
    if (bt)
    {
        ucb_btrace_release(bt);
        free(bt);
    }
}

ucb_btrace* ucb_btrace_clone(const ucb_btrace* bt)
{
    ucb_btrace* clone = UCB_NULL;
    if (bt)
    {
        clone = ucb_btrace_new();
        ucb_btrace_copy(clone, bt);
    }
    return clone;
}

void ucb_btrace_copy(ucb_btrace* dst, const ucb_btrace* src)
{
    UCB_VERIFY_ARGS(dst && src);
    if (dst->strs)
        ucb_btrace_release(dst);
    dst->count = src->count;

    size_t array_size = dst->count * sizeof(char*);
    size_t total_size = array_size;

    for (size_t i = 0; i < src->count; ++i)
        total_size += strlen(src->strs[i]) + 1;

    // Allocate and copy whole memory block
    dst->strs = malloc(total_size);
    ucb_memcpy_s(dst->strs, total_size, src->strs, total_size);

    // Update dst->strs to point to its own memory
    char* str     = (char*)dst->strs + array_size;
    size_t remain = total_size - array_size;
    for (size_t i = 0; i < dst->count; ++i)
    {
        dst->strs[i] = str;
        str += strlen(dst->strs[i]) + 1;
        remain -= str - dst->strs[i];
    }
    UCB_ASSERT_INTERNAL(remain == 0, "Validation failed");
}

void ucb_btrace_capture(ucb_btrace* bt)
{
    UCB_VERIFY_ARGS(bt);

    if (bt->strs && bt->count)
        ucb_btrace_release(bt);

    int frames  = 0;
    char** strs = UCB_NULL;

#ifdef _WIN32
    void* callstack[UCB_BTRACE_MAX_FRAMES];
    frames               = RtlCaptureStackBackTrace(1, UCB_BTRACE_MAX_FRAMES, callstack, NULL);
    SYMBOL_INFO* symbol  = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen   = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    if (frames)
    {
        ucb_buffer buf;

        // Pre-allocate buffer with an estimate
        size_t array_size = frames * sizeof(char*);
        size_t total_size = array_size + frames * sizeof(char) * 80;

        if (!ucb_buffer_init_malloc(&buf, total_size))
        {
            frames = 0;
            goto cleanup;
        }
        buf.grow_func = grow_25;
        buf.used      = array_size;

        for (int i = 0; i < frames; ++i)
        {
            // Format line, after this buf.data may be changed so don't store pointer to it.
            int slen = format_line(&buf, callstack[i], process, symbol);
            if (slen < 0)
            {
                UCB_WARN(UCB_ERRSYS_UNKNOWN, "Failed to format line for backtrace");
                frames = i;
                if (!frames)
                    ucb_buffer_release(&buf);
                break;
            }
        }
        if (buf.data)
        {
            ucb_buffer_fit(&buf);
            UCB_DPRINT("Backtrace buffer initial: %zu, final: %zu\n", total_size, buf.capacity);
            ucb_buffer_transfer(&buf, &(void*)strs, &total_size, UCB_NULL, UCB_NULL);
            ucb_buffer_release(&buf);

            // Update all pointers
            size_t offset = array_size;
            for (int i = 0; i < frames; ++i)
            {
                strs[i] = (char*)strs + offset;
                offset += strlen(strs[i]) + 1;
            }
        }
        else
        {
            strs   = UCB_NULL;
            frames = 0;
        }
    }

cleanup:
    SymCleanup(process);
    free(symbol);
#else
    void* callstack[UCB_BTRACE_MAX_FRAMES + 1];
    frames = backtrace(callstack, UCB_BTRACE_MAX_FRAMES + 1);
    if (frames > 1)
    {
        /**
         * This assumes the layout of the memory returned is:
         * frames * sizeof(char*)
         * followed by each string + null terminator
         *
         * TODO: Verify above or create our own layout
         */
        strs = backtrace_symbols(&callstack[1], frames - 1);
        frames--;
    }

#endif
    if (frames <= 1 || !strs)
    {
        frames = 0;
        UCB_WARN(UCB_ERRSYS_UNKNOWN, "Failed to get backtrace");
    }

    bt->count = frames;
    bt->strs  = strs;
}

void ucb_btrace_print(const ucb_btrace* bt, FILE* stream, int indent)
{
    UCB_VERIFY_ARGS(bt && stream);

    if (!bt->strs)
    {
        fprintf(stream, "%*sNo backtrace available\n", indent, " ");
    }
    else
    {
        for (size_t i = 0; i < bt->count; i++)
        {
            fprintf(stream, "%*s%s\n", indent, " ", bt->strs[i]);
        }
    }
}
