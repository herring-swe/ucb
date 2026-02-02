/**
 * @file error_errno.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling errno implementation
 */

#include "ucb/error.h"

#include "ucb/debug.h"
#include "ucb/errcodes.h"

#include <errno.h>
#include <string.h>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifndef NOERROR
#define NOERROR 0
#endif

ucb_ecode ucb_err_wrap_errno(int err)
{
    switch (err)
    {
#ifdef NOERROR
    case NOERROR:
        return UCB_ERRSYS_NOERROR;
#endif
#ifdef EPERM
    case EPERM:
        return UCB_ERRSYS_EPERM;
#endif
#ifdef ENOENT
    case ENOENT:
        return UCB_ERRSYS_ENOENT;
#endif
#ifdef ESRCH
    case ESRCH:
        return UCB_ERRSYS_ESRCH;
#endif
#ifdef EINTR
    case EINTR:
        return UCB_ERRSYS_EINTR;
#endif
#ifdef EIO
    case EIO:
        return UCB_ERRSYS_EIO;
#endif
#ifdef ENXIO
    case ENXIO:
        return UCB_ERRSYS_ENXIO;
#endif
#ifdef E2BIG
    case E2BIG:
        return UCB_ERRSYS_E2BIG;
#endif
#ifdef ENOEXEC
    case ENOEXEC:
        return UCB_ERRSYS_ENOEXEC;
#endif
#ifdef EBADF
    case EBADF:
        return UCB_ERRSYS_EBADF;
#endif
#ifdef ECHILD
    case ECHILD:
        return UCB_ERRSYS_ECHILD;
#endif
#ifdef EAGAIN
    case EAGAIN:
        return UCB_ERRSYS_EAGAIN;
#endif
#ifdef ENOMEM
    case ENOMEM:
        return UCB_ERRSYS_ENOMEM;
#endif
#ifdef EACCES
    case EACCES:
        return UCB_ERRSYS_EACCES;
#endif
#ifdef EFAULT
    case EFAULT:
        return UCB_ERRSYS_EFAULT;
#endif
#ifdef ENOTBLK
    case ENOTBLK:
        return UCB_ERRSYS_ENOTBLK;
#endif
#ifdef EBUSY
    case EBUSY:
        return UCB_ERRSYS_EBUSY;
#endif
#ifdef EEXIST
    case EEXIST:
        return UCB_ERRSYS_EEXIST;
#endif
#ifdef EXDEV
    case EXDEV:
        return UCB_ERRSYS_EXDEV;
#endif
#ifdef ENODEV
    case ENODEV:
        return UCB_ERRSYS_ENODEV;
#endif
#ifdef ENOTDIR
    case ENOTDIR:
        return UCB_ERRSYS_ENOTDIR;
#endif
#ifdef EISDIR
    case EISDIR:
        return UCB_ERRSYS_EISDIR;
#endif
#ifdef EINVAL
    case EINVAL:
        return UCB_ERRSYS_EINVAL;
#endif
#ifdef ENFILE
    case ENFILE:
        return UCB_ERRSYS_ENFILE;
#endif
#ifdef EMFILE
    case EMFILE:
        return UCB_ERRSYS_EMFILE;
#endif
#ifdef ENOTTY
    case ENOTTY:
        return UCB_ERRSYS_ENOTTY;
#endif
#ifdef ETXTBSY
    case ETXTBSY:
        return UCB_ERRSYS_ETXTBSY;
#endif
#ifdef EFBIG
    case EFBIG:
        return UCB_ERRSYS_EFBIG;
#endif
#ifdef ENOSPC
    case ENOSPC:
        return UCB_ERRSYS_ENOSPC;
#endif
#ifdef ESPIPE
    case ESPIPE:
        return UCB_ERRSYS_ESPIPE;
#endif
#ifdef EROFS
    case EROFS:
        return UCB_ERRSYS_EROFS;
#endif
#ifdef EMLINK
    case EMLINK:
        return UCB_ERRSYS_EMLINK;
#endif
#ifdef EPIPE
    case EPIPE:
        return UCB_ERRSYS_EPIPE;
#endif
#ifdef EDOM
    case EDOM:
        return UCB_ERRSYS_EDOM;
#endif
#ifdef ERANGE
    case ERANGE:
        return UCB_ERRSYS_ERANGE;
#endif
#ifdef EDEADLK
    case EDEADLK:
        return UCB_ERRSYS_EDEADLK;
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
        return UCB_ERRSYS_ENAMETOOLONG;
#endif
#ifdef ENOLCK
    case ENOLCK:
        return UCB_ERRSYS_ENOLCK;
#endif
#ifdef ENOSYS
    case ENOSYS:
        return UCB_ERRSYS_ENOSYS;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:
        return UCB_ERRSYS_ENOTEMPTY;
#endif
#ifdef ELOOP
    case ELOOP:
        return UCB_ERRSYS_ELOOP;
#endif
#ifdef ENOMSG
    case ENOMSG:
        return UCB_ERRSYS_ENOMSG;
#endif
#ifdef EIDRM
    case EIDRM:
        return UCB_ERRSYS_EIDRM;
#endif
#ifdef ECHRNG
    case ECHRNG:
        return UCB_ERRSYS_ECHRNG;
#endif
#ifdef EL2NSYNC
    case EL2NSYNC:
        return UCB_ERRSYS_EL2NSYNC;
#endif
#ifdef EL3HLT
    case EL3HLT:
        return UCB_ERRSYS_EL3HLT;
#endif
#ifdef EL3RST
    case EL3RST:
        return UCB_ERRSYS_EL3RST;
#endif
#ifdef ELNRNG
    case ELNRNG:
        return UCB_ERRSYS_ELNRNG;
#endif
#ifdef EUNATCH
    case EUNATCH:
        return UCB_ERRSYS_EUNATCH;
#endif
#ifdef ENOCSI
    case ENOCSI:
        return UCB_ERRSYS_ENOCSI;
#endif
#ifdef EL2HLT
    case EL2HLT:
        return UCB_ERRSYS_EL2HLT;
#endif
#ifdef EBADE
    case EBADE:
        return UCB_ERRSYS_EBADE;
#endif
#ifdef EBADR
    case EBADR:
        return UCB_ERRSYS_EBADR;
#endif
#ifdef EXFULL
    case EXFULL:
        return UCB_ERRSYS_EXFULL;
#endif
#ifdef ENOANO
    case ENOANO:
        return UCB_ERRSYS_ENOANO;
#endif
#ifdef EBADRQC
    case EBADRQC:
        return UCB_ERRSYS_EBADRQC;
#endif
#ifdef EBADSLT
    case EBADSLT:
        return UCB_ERRSYS_EBADSLT;
#endif
#ifdef EBFONT
    case EBFONT:
        return UCB_ERRSYS_EBFONT;
#endif
#ifdef ENOSTR
    case ENOSTR:
        return UCB_ERRSYS_ENOSTR;
#endif
#ifdef ENODATA
    case ENODATA:
        return UCB_ERRSYS_ENODATA;
#endif
#ifdef ETIME
    case ETIME:
        return UCB_ERRSYS_ETIME;
#endif
#ifdef ENOSR
    case ENOSR:
        return UCB_ERRSYS_ENOSR;
#endif
#ifdef ENONET
    case ENONET:
        return UCB_ERRSYS_ENONET;
#endif
#ifdef ENOPKG
    case ENOPKG:
        return UCB_ERRSYS_ENOPKG;
#endif
#ifdef EREMOTE
    case EREMOTE:
        return UCB_ERRSYS_EREMOTE;
#endif
#ifdef ENOLINK
    case ENOLINK:
        return UCB_ERRSYS_ENOLINK;
#endif
#ifdef EADV
    case EADV:
        return UCB_ERRSYS_EADV;
#endif
#ifdef ESRMNT
    case ESRMNT:
        return UCB_ERRSYS_ESRMNT;
#endif
#ifdef ECOMM
    case ECOMM:
        return UCB_ERRSYS_ECOMM;
#endif
#ifdef EPROTO
    case EPROTO:
        return UCB_ERRSYS_EPROTO;
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
        return UCB_ERRSYS_EMULTIHOP;
#endif
#ifdef EDOTDOT
    case EDOTDOT:
        return UCB_ERRSYS_EDOTDOT;
#endif
#ifdef EBADMSG
    case EBADMSG:
        return UCB_ERRSYS_EBADMSG;
#endif
#ifdef EOVERFLOW
    case EOVERFLOW:
        return UCB_ERRSYS_EOVERFLOW;
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ:
        return UCB_ERRSYS_ENOTUNIQ;
#endif
#ifdef EBADFD
    case EBADFD:
        return UCB_ERRSYS_EBADFD;
#endif
#ifdef EREMCHG
    case EREMCHG:
        return UCB_ERRSYS_EREMCHG;
#endif
#ifdef ELIBACC
    case ELIBACC:
        return UCB_ERRSYS_ELIBACC;
#endif
#ifdef ELIBBAD
    case ELIBBAD:
        return UCB_ERRSYS_ELIBBAD;
#endif
#ifdef ELIBSCN
    case ELIBSCN:
        return UCB_ERRSYS_ELIBSCN;
#endif
#ifdef ELIBMAX
    case ELIBMAX:
        return UCB_ERRSYS_ELIBMAX;
#endif
#ifdef ELIBEXEC
    case ELIBEXEC:
        return UCB_ERRSYS_ELIBEXEC;
#endif
#ifdef EILSEQ
    case EILSEQ:
        return UCB_ERRSYS_EILSEQ;
#endif
#ifdef ERESTART
    case ERESTART:
        return UCB_ERRSYS_ERESTART;
#endif
#ifdef ESTRPIPE
    case ESTRPIPE:
        return UCB_ERRSYS_ESTRPIPE;
#endif
#ifdef EUSERS
    case EUSERS:
        return UCB_ERRSYS_EUSERS;
#endif
#ifdef ENOTSOCK
    case ENOTSOCK:
        return UCB_ERRSYS_ENOTSOCK;
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ:
        return UCB_ERRSYS_EDESTADDRREQ;
#endif
#ifdef EMSGSIZE
    case EMSGSIZE:
        return UCB_ERRSYS_EMSGSIZE;
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE:
        return UCB_ERRSYS_EPROTOTYPE;
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
        return UCB_ERRSYS_ENOPROTOOPT;
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
        return UCB_ERRSYS_EPROTONOSUPPORT;
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:
        return UCB_ERRSYS_ESOCKTNOSUPPORT;
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP:
        return UCB_ERRSYS_EOPNOTSUPP;
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:
        return UCB_ERRSYS_EPFNOSUPPORT;
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
        return UCB_ERRSYS_EAFNOSUPPORT;
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
        return UCB_ERRSYS_EADDRINUSE;
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
        return UCB_ERRSYS_EADDRNOTAVAIL;
#endif
#ifdef ENETDOWN
    case ENETDOWN:
        return UCB_ERRSYS_ENETDOWN;
#endif
#ifdef ENETUNREACH
    case ENETUNREACH:
        return UCB_ERRSYS_ENETUNREACH;
#endif
#ifdef ENETRESET
    case ENETRESET:
        return UCB_ERRSYS_ENETRESET;
#endif
#ifdef ECONNABORTED
    case ECONNABORTED:
        return UCB_ERRSYS_ECONNABORTED;
#endif
#ifdef ECONNRESET
    case ECONNRESET:
        return UCB_ERRSYS_ECONNRESET;
#endif
#ifdef ENOBUFS
    case ENOBUFS:
        return UCB_ERRSYS_ENOBUFS;
#endif
#ifdef EISCONN
    case EISCONN:
        return UCB_ERRSYS_EISCONN;
#endif
#ifdef ENOTCONN
    case ENOTCONN:
        return UCB_ERRSYS_ENOTCONN;
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN:
        return UCB_ERRSYS_ESHUTDOWN;
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS:
        return UCB_ERRSYS_ETOOMANYREFS;
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT:
        return UCB_ERRSYS_ETIMEDOUT;
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
        return UCB_ERRSYS_ECONNREFUSED;
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN:
        return UCB_ERRSYS_EHOSTDOWN;
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
        return UCB_ERRSYS_EHOSTUNREACH;
#endif
#ifdef EALREADY
    case EALREADY:
        return UCB_ERRSYS_EALREADY;
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
        return UCB_ERRSYS_EINPROGRESS;
#endif
#ifdef ESTALE
    case ESTALE:
        return UCB_ERRSYS_ESTALE;
#endif
#ifdef EUCLEAN
    case EUCLEAN:
        return UCB_ERRSYS_EUCLEAN;
#endif
#ifdef ENOTNAM
    case ENOTNAM:
        return UCB_ERRSYS_ENOTNAM;
#endif
#ifdef ENAVAIL
    case ENAVAIL:
        return UCB_ERRSYS_ENAVAIL;
#endif
#ifdef EISNAM
    case EISNAM:
        return UCB_ERRSYS_EISNAM;
#endif
#ifdef EREMOTEIO
    case EREMOTEIO:
        return UCB_ERRSYS_EREMOTEIO;
#endif
#ifdef EDQUOT
    case EDQUOT:
        return UCB_ERRSYS_EDQUOT;
#endif
#ifdef ENOMEDIUM
    case ENOMEDIUM:
        return UCB_ERRSYS_ENOMEDIUM;
#endif
#ifdef EMEDIUMTYPE
    case EMEDIUMTYPE:
        return UCB_ERRSYS_EMEDIUMTYPE;
#endif
#ifdef ECANCELED
    case ECANCELED:
        return UCB_ERRSYS_ECANCELED;
#endif
#ifdef ENOKEY
    case ENOKEY:
        return UCB_ERRSYS_ENOKEY;
#endif
#ifdef EKEYEXPIRED
    case EKEYEXPIRED:
        return UCB_ERRSYS_EKEYEXPIRED;
#endif
#ifdef EKEYREVOKED
    case EKEYREVOKED:
        return UCB_ERRSYS_EKEYREVOKED;
#endif
#ifdef EKEYREJECTED
    case EKEYREJECTED:
        return UCB_ERRSYS_EKEYREJECTED;
#endif
#ifdef EOWNERDEAD
    case EOWNERDEAD:
        return UCB_ERRSYS_EOWNERDEAD;
#endif
#ifdef ENOTRECOVERABLE
    case ENOTRECOVERABLE:
        return UCB_ERRSYS_ENOTRECOVERABLE;
#endif
#ifdef ERFKILL
    case ERFKILL:
        return UCB_ERRSYS_ERFKILL;
#endif
#ifdef EHWPOISON
    case EHWPOISON:
        return UCB_ERRSYS_EHWPOISON;
#endif
#if defined(ENOTSUP) && (!defined(EOPNOTSUPP) || EOPNOTSUPP != ENOTSUP)
    case ENOTSUP:
        return UCB_ERRSYS_ENOTSUP;
#endif
#ifdef STRUNCATE
    case STRUNCATE:
        return UCB_ERRSYS_STRUNCATE;
#endif
    default:
        UCB_DPRINT("UCB: Unhandled errno: %d\n", err);
        return UCB_ERRSYS_UNKNOWN;
    }
}

ucb_ecode ucb_err_get_errno(void)
{
    return ucb_err_wrap_errno(errno);
}

bool ucb_report_errno(int status, const char* UCB_RESTRICT msg, const char* UCB_RESTRICT function)
{
    if (status == 0)
        return false;
    if (msg)
    {
        ucb_error_report(UCB_ERRLVL_SYSTEM,
                         ucb_error_format(ucb_err_wrap_errno(status), "%s: %s", function, msg));
    }
    else
    {
        ucb_error_report(UCB_ERRLVL_SYSTEM,
                         ucb_error_format(ucb_err_wrap_errno(status), "%s: Unexpected error - %s",
                                          function, strerror(status)));
    }
    return true;
}

bool ucb_throw_errno(const ucb_error** perr, int status, const char* msg)
{
    if (status == 0)
        return false;
    if (msg)
        ucb_throw(perr, ucb_err_wrap_errno(status), msg);
    else
        ucb_throw_format(perr, ucb_err_wrap_errno(status), "%s", strerror(status));
    return true;
}
