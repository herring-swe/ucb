/**
 * @file argparse.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Python-optparse style argument parsing implementation
 */

#include "ucb/argparse.h"

#include "ucb/container/impl/vector_ptr.h"
#include "ucb/container/impl/vector_str.h"
#include "ucb/memory.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Internal types                                */
/* -------------------------------------------------------------------------- */

typedef struct ucb_arg_opt_impl
{
    ucb_str name;
    char short_name;
    ucb_str long_name;
    ucb_str canon;
    ucb_arg_type type;
    ucb_arg_action action;
    void* dest;
    ucb_str def;
    bool has_def;
    ucb_str metavar;
    ucb_str help;
    ucb_vector_str* choices;
    bool required;
    bool seen;
} ucb_arg_opt_impl;

struct ucb_arg_parser
{
    ucb_str prog_base;
    ucb_str usage;
    ucb_str description;
    ucb_str version;
    bool has_version;
    int width;
    ucb_vector_ptr* opts;
};

/* -------------------------------------------------------------------------- */
/*                              Small helpers                                 */
/* -------------------------------------------------------------------------- */

static void set_basename(ucb_str* out, const char* path)
{
    const char* base = path;
    for (const char* c = path; *c; ++c)
    {
        if (*c == '/' || *c == '\\')
            base = c + 1;
    }
    if (*base == '\0')
        base = path;
    ucb_str_assign_c(out, base);
}

static bool ieq(const char* a, const char* b)
{
    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static const char* type_name(ucb_arg_type type)
{
    switch (type)
    {
    case UCB_ARG_TYPE_NONE:
        return "none";
    case UCB_ARG_TYPE_BOOL:
        return "bool";
    case UCB_ARG_TYPE_STR:
        return "str";
    case UCB_ARG_TYPE_INT:
        return "int";
    case UCB_ARG_TYPE_UINT:
        return "uint";
    case UCB_ARG_TYPE_LONG:
        return "long";
    case UCB_ARG_TYPE_ULONG:
        return "ulong";
    case UCB_ARG_TYPE_LLONG:
        return "llong";
    case UCB_ARG_TYPE_ULLONG:
        return "ullong";
    case UCB_ARG_TYPE_SIZE:
        return "size";
    case UCB_ARG_TYPE_FLOAT:
        return "float";
    case UCB_ARG_TYPE_DOUBLE:
        return "double";
    default:
        return "unknown";
    }
}

static const char* action_name(ucb_arg_action action)
{
    switch (action)
    {
    case UCB_ARG_ACTION_STORE:
        return "store";
    case UCB_ARG_ACTION_STORE_TRUE:
        return "store_true";
    case UCB_ARG_ACTION_STORE_FALSE:
        return "store_false";
    case UCB_ARG_ACTION_COUNT:
        return "count";
    case UCB_ARG_ACTION_APPEND:
        return "append";
    default:
        return "unknown";
    }
}

static bool opt_takes_value(const ucb_arg_opt_impl* o)
{
    return o->action == UCB_ARG_ACTION_STORE || o->action == UCB_ARG_ACTION_APPEND;
}

/* -------------------------------------------------------------------------- */
/*                                 Converters                                 */
/* -------------------------------------------------------------------------- */

static bool is_space_char(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static bool parse_ll(const char* s, long long* out)
{
    if (!s || !*s || is_space_char(s[0]))
        return false;
    errno = 0;
    char* end = UCB_NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE)
        return false;
    *out = v;
    return true;
}

static bool parse_ull(const char* s, unsigned long long* out)
{
    // strtoull skips leading whitespace and then accepts a sign, so a value
    // like " -1" would wrap to a huge unsigned result. Reject any leading
    // whitespace and an explicit negative sign before parsing.
    if (!s || !*s || is_space_char(s[0]) || s[0] == '-')
        return false;
    errno = 0;
    char* end = UCB_NULL;
    unsigned long long v = strtoull(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE)
        return false;
    *out = v;
    return true;
}

static bool parse_flt(const char* s, float* out)
{
    if (!s || !*s || is_space_char(s[0]))
        return false;
    errno = 0;
    char* end = UCB_NULL;
    float v = strtof(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE)
        return false;
    *out = v;
    return true;
}

static bool parse_dbl(const char* s, double* out)
{
    if (!s || !*s || is_space_char(s[0]))
        return false;
    errno = 0;
    char* end = UCB_NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE)
        return false;
    *out = v;
    return true;
}

static bool parse_bool_lit(const char* s, bool* out)
{
    if (!s)
        return false;
    if (ieq(s, "true") || ieq(s, "1") || ieq(s, "yes") || ieq(s, "on"))
    {
        *out = true;
        return true;
    }
    if (ieq(s, "false") || ieq(s, "0") || ieq(s, "no") || ieq(s, "off"))
    {
        *out = false;
        return true;
    }
    return false;
}

static bool convert_store(const ucb_arg_opt_impl* o, const char* v, ucb_error** perr)
{
    switch (o->type)
    {
    case UCB_ARG_TYPE_BOOL:
    {
        bool b = false;
        if (!parse_bool_lit(v, &b))
            goto invalid;
        *(bool*)o->dest = b;
        return true;
    }
    case UCB_ARG_TYPE_STR:
    {
        if (!ucb_str_assign_c((ucb_str*)o->dest, v))
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "out of memory");
            return false;
        }
        return true;
    }
    case UCB_ARG_TYPE_INT:
    {
        long long x = 0;
        if (!parse_ll(v, &x) || x < INT_MIN || x > INT_MAX)
            goto invalid;
        *(int*)o->dest = (int)x;
        return true;
    }
    case UCB_ARG_TYPE_UINT:
    {
        unsigned long long x = 0;
        if (!parse_ull(v, &x) || x > UINT_MAX)
            goto invalid;
        *(unsigned int*)o->dest = (unsigned int)x;
        return true;
    }
    case UCB_ARG_TYPE_LONG:
    {
        long long x = 0;
        if (!parse_ll(v, &x))
            goto invalid;
#if LONG_MAX < LLONG_MAX
        if (x < LONG_MIN || x > LONG_MAX)
            goto invalid;
#endif
        *(long*)o->dest = (long)x;
        return true;
    }
    case UCB_ARG_TYPE_ULONG:
    {
        unsigned long long x = 0;
        if (!parse_ull(v, &x))
            goto invalid;
#if ULONG_MAX < ULLONG_MAX
        if (x > ULONG_MAX)
            goto invalid;
#endif
        *(unsigned long*)o->dest = (unsigned long)x;
        return true;
    }
    case UCB_ARG_TYPE_LLONG:
    {
        long long x = 0;
        if (!parse_ll(v, &x))
            goto invalid;
        *(long long*)o->dest = x;
        return true;
    }
    case UCB_ARG_TYPE_ULLONG:
    {
        unsigned long long x = 0;
        if (!parse_ull(v, &x))
            goto invalid;
        *(unsigned long long*)o->dest = x;
        return true;
    }
    case UCB_ARG_TYPE_SIZE:
    {
        unsigned long long x = 0;
        if (!parse_ull(v, &x))
            goto invalid;
#if SIZE_MAX < ULLONG_MAX
        if (x > SIZE_MAX)
            goto invalid;
#endif
        *(size_t*)o->dest = (size_t)x;
        return true;
    }
    case UCB_ARG_TYPE_FLOAT:
    {
        float x = 0.0f;
        if (!parse_flt(v, &x))
            goto invalid;
        *(float*)o->dest = x;
        return true;
    }
    case UCB_ARG_TYPE_DOUBLE:
    {
        double x = 0.0;
        if (!parse_dbl(v, &x))
            goto invalid;
        *(double*)o->dest = x;
        return true;
    }
    default:
        return false;
    }

invalid:
    ucb_throw_format(perr,
                     UCB_ERROR_INVALID_ARG,
                     "invalid %s value for %s: '%s'",
                     type_name(o->type),
                     o->canon.data,
                     v);
    return false;
}

static void zero_dest(const ucb_arg_opt_impl* o)
{
    switch (o->type)
    {
    case UCB_ARG_TYPE_BOOL:
        *(bool*)o->dest = false;
        break;
    case UCB_ARG_TYPE_INT:
        *(int*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_UINT:
        *(unsigned int*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_LONG:
        *(long*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_ULONG:
        *(unsigned long*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_LLONG:
        *(long long*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_ULLONG:
        *(unsigned long long*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_SIZE:
        *(size_t*)o->dest = 0;
        break;
    case UCB_ARG_TYPE_FLOAT:
        *(float*)o->dest = 0.0f;
        break;
    case UCB_ARG_TYPE_DOUBLE:
        *(double*)o->dest = 0.0;
        break;
    default:
        break;
    }
}

static bool init_default(ucb_arg_opt_impl* o, ucb_error** perr)
{
    o->seen = false;
    switch (o->action)
    {
    case UCB_ARG_ACTION_STORE:
        if (o->type == UCB_ARG_TYPE_STR)
        {
            ucb_str* dst = (ucb_str*)o->dest;
            ucb_str_clear(dst);
            if (!ucb_str_assign_c(dst, o->has_def ? o->def.data : ""))
            {
                ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "out of memory");
                return false;
            }
        }
        else
        {
            zero_dest(o);
            if (o->has_def && !convert_store(o, o->def.data, perr))
                return false;
        }
        break;
    case UCB_ARG_ACTION_STORE_TRUE:
    {
        bool b = false;
        if (o->has_def && !parse_bool_lit(o->def.data, &b))
        {
            ucb_throw_format(perr,
                             UCB_ERROR_INVALID_ARG,
                             "invalid bool default for %s: '%s'",
                             o->canon.data,
                             o->def.data);
            return false;
        }
        *(bool*)o->dest = b;
        break;
    }
    case UCB_ARG_ACTION_STORE_FALSE:
    {
        bool b = true;
        if (o->has_def && !parse_bool_lit(o->def.data, &b))
        {
            ucb_throw_format(perr,
                             UCB_ERROR_INVALID_ARG,
                             "invalid bool default for %s: '%s'",
                             o->canon.data,
                             o->def.data);
            return false;
        }
        *(bool*)o->dest = b;
        break;
    }
    case UCB_ARG_ACTION_COUNT:
    {
        long long x = 0;
        if (o->has_def && (!parse_ll(o->def.data, &x) || x < INT_MIN || x > INT_MAX))
        {
            ucb_throw_format(perr,
                             UCB_ERROR_INVALID_ARG,
                             "invalid count default for %s: '%s'",
                             o->canon.data,
                             o->def.data);
            return false;
        }
        *(int*)o->dest = (int)x;
        break;
    }
    case UCB_ARG_ACTION_APPEND:
        ucb_vector_str_clear_deep((ucb_vector_str*)o->dest);
        break;
    default:
        break;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                  Lifetime                                  */
/* -------------------------------------------------------------------------- */

static void opt_impl_free(ucb_arg_opt_impl* o)
{
    if (!o)
        return;
    ucb_str_release(&o->name);
    ucb_str_release(&o->long_name);
    ucb_str_release(&o->canon);
    ucb_str_release(&o->def);
    ucb_str_release(&o->metavar);
    ucb_str_release(&o->help);
    if (o->choices)
        ucb_vector_str_free_full(o->choices);
    ucb_free(o);
}

UCB_API ucb_arg_parser* ucb_arg_parser_new(const char* prog, ucb_error** perr)
{
    UCB_UNUSED(perr);
    UCB_VERIFY_ARGS(prog);

    ucb_arg_parser* p = ucb_calloc_type(1, ucb_arg_parser);
    if (!p)
        return UCB_NULL;

    ucb_str_init_empty(&p->prog_base);
    ucb_str_init_empty(&p->usage);
    ucb_str_init_empty(&p->description);
    ucb_str_init_empty(&p->version);
    p->width = 80;

    p->opts = ucb_vector_ptr_new();
    if (!p->opts)
    {
        ucb_free(p);
        return UCB_NULL;
    }

    set_basename(&p->prog_base, prog);
    return p;
}

UCB_API void ucb_arg_parser_free(ucb_arg_parser* p)
{
    if (!p)
        return;

    ucb_str_release(&p->prog_base);
    ucb_str_release(&p->usage);
    ucb_str_release(&p->description);
    ucb_str_release(&p->version);

    if (p->opts)
    {
        for (size_t i = 0; i < p->opts->size; ++i)
            opt_impl_free((ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i));
        ucb_vector_ptr_free(p->opts);
    }

    ucb_free(p);
}

/* -------------------------------------------------------------------------- */
/*                                   Metadata                                 */
/* -------------------------------------------------------------------------- */

UCB_API bool ucb_arg_parser_set_usage(ucb_arg_parser* p, const char* usage)
{
    UCB_VERIFY_ARGS(p);
    if (!usage)
    {
        ucb_str_clear(&p->usage);
        return true;
    }
    return ucb_str_assign_c(&p->usage, usage);
}

UCB_API bool ucb_arg_parser_set_description(ucb_arg_parser* p, const char* description)
{
    UCB_VERIFY_ARGS(p);
    if (!description)
    {
        ucb_str_clear(&p->description);
        return true;
    }
    return ucb_str_assign_c(&p->description, description);
}

UCB_API bool ucb_arg_parser_set_version(ucb_arg_parser* p, const char* version)
{
    UCB_VERIFY_ARGS(p);
    if (!version)
    {
        ucb_str_clear(&p->version);
        p->has_version = false;
        return true;
    }
    if (!ucb_str_assign_c(&p->version, version))
        return false;
    p->has_version = p->version.size > 0;
    return true;
}

UCB_API void ucb_arg_parser_set_width(ucb_arg_parser* p, int width)
{
    UCB_VERIFY_ARGS(p);
    p->width = width < 20 ? 20 : width;
}

/* -------------------------------------------------------------------------- */
/*                              Option lookup                                 */
/* -------------------------------------------------------------------------- */

static ucb_arg_opt_impl* find_short(ucb_arg_parser* p, char c)
{
    for (size_t i = 0; i < p->opts->size; ++i)
    {
        ucb_arg_opt_impl* o = (ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i);
        if (o->short_name == c)
            return o;
    }
    return UCB_NULL;
}

static ucb_arg_opt_impl* find_long(ucb_arg_parser* p, const char* name, size_t len)
{
    if (len == 0)
        return UCB_NULL;
    for (size_t i = 0; i < p->opts->size; ++i)
    {
        ucb_arg_opt_impl* o = (ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i);
        if (o->long_name.size == len && strncmp(o->long_name.data, name, len) == 0)
            return o;
    }
    return UCB_NULL;
}

static bool is_reserved(char short_name, const char* long_name)
{
    if (short_name == 'h')
        return true;
    if (long_name)
    {
        if (strcmp(long_name, "help") == 0)
            return true;
        if (strcmp(long_name, "version") == 0)
            return true;
    }
    return false;
}

/* -------------------------------------------------------------------------- */
/*                              Option declaration                            */
/* -------------------------------------------------------------------------- */

UCB_API bool ucb_arg_parser_add(ucb_arg_parser* p, const ucb_arg_opt* opt, ucb_error** perr)
{
    UCB_VERIFY_ARGS(p && opt);

    if (!opt->name || !*opt->name)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "option name is required");
        return false;
    }
    if (opt->short_name == 0 && !opt->long_name)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "option requires a short or long name");
        return false;
    }
    if (is_reserved(opt->short_name, opt->long_name))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "option name is reserved (-h/--help/--version)");
        return false;
    }
    if (opt->short_name && find_short(p, opt->short_name))
    {
        ucb_throw_format(perr,
                         UCB_ERROR_INVALID_ARG,
                         "duplicate short option '-%c'",
                         opt->short_name);
        return false;
    }
    if (opt->long_name && find_long(p, opt->long_name, strlen(opt->long_name)))
    {
        ucb_throw_format(perr,
                         UCB_ERROR_INVALID_ARG,
                         "duplicate long option '--%s'",
                         opt->long_name);
        return false;
    }

    switch (opt->action)
    {
    case UCB_ARG_ACTION_STORE:
        if (opt->type == UCB_ARG_TYPE_NONE)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "STORE requires a value type");
            return false;
        }
        if (!opt->dest)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "STORE requires a destination");
            return false;
        }
        break;
    case UCB_ARG_ACTION_STORE_TRUE:
    case UCB_ARG_ACTION_STORE_FALSE:
    case UCB_ARG_ACTION_COUNT:
        if (!opt->dest)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "option requires a destination");
            return false;
        }
        break;
    case UCB_ARG_ACTION_APPEND:
        if (!opt->dest)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "APPEND requires a destination");
            return false;
        }
        if (opt->type != UCB_ARG_TYPE_NONE && opt->type != UCB_ARG_TYPE_STR)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "APPEND only supports NONE or STR type");
            return false;
        }
        break;
    default:
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "unknown option action");
        return false;
    }

    if (opt->num_choices)
    {
        if (!opt->choices)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "choices array is NULL");
            return false;
        }
        if (opt->action != UCB_ARG_ACTION_STORE && opt->action != UCB_ARG_ACTION_APPEND)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "choices require a STORE or APPEND action");
            return false;
        }
    }

    ucb_arg_opt_impl* o = ucb_calloc_type(1, ucb_arg_opt_impl);
    if (!o)
        return false;

    ucb_str_init_empty(&o->name);
    ucb_str_init_empty(&o->long_name);
    ucb_str_init_empty(&o->canon);
    ucb_str_init_empty(&o->def);
    ucb_str_init_empty(&o->metavar);
    ucb_str_init_empty(&o->help);

    o->short_name = opt->short_name;
    o->type = opt->type;
    o->action = opt->action;
    o->dest = opt->dest;
    o->required = opt->required;

    ucb_str_assign_c(&o->name, opt->name);
    if (opt->long_name)
        ucb_str_assign_c(&o->long_name, opt->long_name);
    if (opt->def)
    {
        ucb_str_assign_c(&o->def, opt->def);
        o->has_def = true;
    }
    if (opt->metavar)
        ucb_str_assign_c(&o->metavar, opt->metavar);
    if (opt->help)
        ucb_str_assign_c(&o->help, opt->help);

    if (opt->short_name)
    {
        ucb_str_append_c(&o->canon, "-");
        ucb_str_append_cstr(&o->canon, &o->short_name, 1);
    }
    if (opt->long_name)
    {
        if (opt->short_name)
            ucb_str_append_c(&o->canon, "/");
        ucb_str_append_c(&o->canon, "--");
        ucb_str_append(&o->canon, &o->long_name);
    }

    if (opt->num_choices)
    {
        o->choices = ucb_vector_str_new();
        if (!o->choices)
        {
            opt_impl_free(o);
            return false;
        }
        for (size_t i = 0; i < opt->num_choices; ++i)
        {
            ucb_str* choice = ucb_str_new_c(opt->choices[i]);
            if (!choice)
            {
                opt_impl_free(o);
                return false;
            }
            ucb_vector_str_push_back(o->choices, choice);
        }
    }

    ucb_vector_ptr_push_back(p->opts, o);
    return true;
}

UCB_API bool ucb_arg_parser_add_options(ucb_arg_parser* p,
                                        const ucb_arg_opt* opts,
                                        size_t count,
                                        ucb_error** perr)
{
    UCB_VERIFY_ARGS(p);
    if (count && !opts)
    {
        UCB_VERIFY_ARGS(opts);
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (!ucb_arg_parser_add(p, &opts[i], perr))
            return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                               Apply actions                                */
/* -------------------------------------------------------------------------- */

static bool check_choices(const ucb_arg_opt_impl* o, const char* v, ucb_error** perr)
{
    if (!o->choices || o->choices->size == 0)
        return true;
    for (size_t i = 0; i < o->choices->size; ++i)
    {
        if (strcmp(ucb_str_cstr(ucb_vector_str_get(o->choices, i)), v) == 0)
            return true;
    }

    ucb_str list = ucb_str_make();
    for (size_t i = 0; i < o->choices->size; ++i)
    {
        if (i)
            ucb_str_append_c(&list, ", ");
        ucb_str_append_c(&list, "'");
        ucb_str_append(&list, ucb_vector_str_get(o->choices, i));
        ucb_str_append_c(&list, "'");
    }
    ucb_throw_format(perr,
                     UCB_ERROR_INVALID_ARG,
                     "invalid choice for %s: '%s' (choose from %s)",
                     o->canon.data,
                     v,
                     list.data);
    ucb_str_release(&list);
    return false;
}

static bool apply_option(ucb_arg_opt_impl* o, const char* value, ucb_error** perr)
{
    switch (o->action)
    {
    case UCB_ARG_ACTION_STORE:
        if (!check_choices(o, value, perr))
            return false;
        if (!convert_store(o, value, perr))
            return false;
        break;
    case UCB_ARG_ACTION_APPEND:
        if (!check_choices(o, value, perr))
            return false;
        {
            ucb_str* s = ucb_str_new_c(value);
            if (!s)
            {
                ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "out of memory");
                return false;
            }
            ucb_vector_str_push_back((ucb_vector_str*)o->dest, s);
        }
        break;
    case UCB_ARG_ACTION_STORE_TRUE:
        *(bool*)o->dest = true;
        break;
    case UCB_ARG_ACTION_STORE_FALSE:
        *(bool*)o->dest = false;
        break;
    case UCB_ARG_ACTION_COUNT:
        (*(int*)o->dest)++;
        break;
    default:
        return false;
    }
    o->seen = true;
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                   Parsing                                  */
/* -------------------------------------------------------------------------- */

static bool is_negative_number(const char* arg)
{
    if (arg[1] >= '0' && arg[1] <= '9')
        return true;
    if (arg[1] == '.')
        return true;
    return false;
}

static bool push_positional(ucb_vector_str* positionals, const char* arg, ucb_error** perr)
{
    if (!positionals)
        return true;
    ucb_str* s = ucb_str_new_c(arg);
    if (!s)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "out of memory");
        return false;
    }
    ucb_vector_str_push_back(positionals, s);
    return true;
}

UCB_API ucb_arg_status ucb_arg_parser_parse(ucb_arg_parser* p,
                                            int argc,
                                            const char* const* argv,
                                            ucb_vector_str* positionals,
                                            ucb_error** perr)
{
    UCB_VERIFY_ARGS(p);
    UCB_VERIFY_ARGS(argc >= 0);
    if (argc > 0)
        UCB_VERIFY_ARGS(argv);

    for (size_t i = 0; i < p->opts->size; ++i)
    {
        if (!init_default((ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i), perr))
            return UCB_ARG_ERROR;
    }

    bool no_more = false;
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (!no_more && strcmp(arg, "--") == 0)
        {
            no_more = true;
            continue;
        }

        if (!no_more && arg[0] == '-' && arg[1] == '-' && arg[2] != '\0')
        {
            const char* body = arg + 2;
            const char* eq = strchr(body, '=');
            size_t name_len = eq ? (size_t)(eq - body) : strlen(body);

            if (name_len == 4 && strncmp(body, "help", 4) == 0)
            {
                if (eq)
                {
                    ucb_throw(perr,
                              UCB_ERROR_INVALID_ARG,
                              "option '--help' does not take an argument");
                    return UCB_ARG_ERROR;
                }
                return UCB_ARG_HELP;
            }
            if (p->has_version && name_len == 7 && strncmp(body, "version", 7) == 0)
            {
                if (eq)
                {
                    ucb_throw(perr,
                              UCB_ERROR_INVALID_ARG,
                              "option '--version' does not take an argument");
                    return UCB_ARG_ERROR;
                }
                return UCB_ARG_VERSION;
            }

            ucb_arg_opt_impl* o = find_long(p, body, name_len);
            if (!o)
            {
                ucb_throw_format(perr,
                                 UCB_ERROR_INVALID_ARG,
                                 "unrecognized option '--%.*s'",
                                 (int)name_len,
                                 body);
                return UCB_ARG_ERROR;
            }

            if (opt_takes_value(o))
            {
                const char* value = UCB_NULL;
                if (eq)
                    value = eq + 1;
                else if (i + 1 < argc)
                    value = argv[++i];
                else
                {
                    ucb_throw_format(perr,
                                     UCB_ERROR_INVALID_ARG,
                                     "option %s requires an argument",
                                     o->canon.data);
                    return UCB_ARG_ERROR;
                }
                if (!apply_option(o, value, perr))
                    return UCB_ARG_ERROR;
            }
            else
            {
                if (eq)
                {
                    ucb_throw_format(perr,
                                     UCB_ERROR_INVALID_ARG,
                                     "option %s does not take an argument",
                                     o->canon.data);
                    return UCB_ARG_ERROR;
                }
                if (!apply_option(o, UCB_NULL, perr))
                    return UCB_ARG_ERROR;
            }
            continue;
        }

        if (!no_more && arg[0] == '-' && arg[1] != '\0' && arg[1] != '-' &&
            !is_negative_number(arg))
        {
            const char* body = arg + 1;
            for (size_t k = 0; body[k] != '\0'; ++k)
            {
                char c = body[k];
                if (c == 'h')
                    return UCB_ARG_HELP;

                ucb_arg_opt_impl* o = find_short(p, c);
                if (!o)
                {
                    ucb_throw_format(perr, UCB_ERROR_INVALID_ARG, "invalid option -- '%c'", c);
                    return UCB_ARG_ERROR;
                }

                if (opt_takes_value(o))
                {
                    const char* value = UCB_NULL;
                    if (body[k + 1] != '\0')
                    {
                        value = &body[k + 1];
                    }
                    else if (i + 1 < argc)
                    {
                        value = argv[++i];
                    }
                    else
                    {
                        ucb_throw_format(perr,
                                         UCB_ERROR_INVALID_ARG,
                                         "option %s requires an argument",
                                         o->canon.data);
                        return UCB_ARG_ERROR;
                    }
                    if (!apply_option(o, value, perr))
                        return UCB_ARG_ERROR;
                    break;
                }
                if (!apply_option(o, UCB_NULL, perr))
                    return UCB_ARG_ERROR;
            }
            continue;
        }

        if (!push_positional(positionals, arg, perr))
            return UCB_ARG_ERROR;
    }

    for (size_t i = 0; i < p->opts->size; ++i)
    {
        ucb_arg_opt_impl* o = (ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i);
        if (o->required && !o->seen)
        {
            ucb_throw_format(perr, UCB_ERROR_INVALID_ARG, "option %s is required", o->canon.data);
            return UCB_ARG_ERROR;
        }
    }

    return UCB_ARG_OK;
}

/* -------------------------------------------------------------------------- */
/*                                Interpolation                               */
/* -------------------------------------------------------------------------- */

static void append_metavar(ucb_str* out, const ucb_arg_opt_impl* o)
{
    if (o->metavar.size)
    {
        ucb_str_append(out, &o->metavar);
        return;
    }
    for (size_t i = 0; i < o->name.size; ++i)
    {
        char c = o->name.data[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        ucb_str_append_cstr(out, &c, 1);
    }
}

static void append_default(ucb_str* out, const ucb_arg_opt_impl* o)
{
    switch (o->action)
    {
    case UCB_ARG_ACTION_STORE_TRUE:
        ucb_str_append_c(out, o->has_def ? o->def.data : "false");
        break;
    case UCB_ARG_ACTION_STORE_FALSE:
        ucb_str_append_c(out, o->has_def ? o->def.data : "true");
        break;
    case UCB_ARG_ACTION_COUNT:
        ucb_str_append_c(out, o->has_def ? o->def.data : "0");
        break;
    case UCB_ARG_ACTION_STORE:
    case UCB_ARG_ACTION_APPEND:
    default:
        if (o->has_def)
            ucb_str_append(out, &o->def);
        break;
    }
}

static void append_field(ucb_str* out,
                         const ucb_arg_parser* p,
                         const ucb_arg_opt_impl* o,
                         const char* field,
                         size_t len)
{
    if (len == 4 && strncmp(field, "prog", 4) == 0)
    {
        ucb_str_append(out, &p->prog_base);
        return;
    }
    if (!o)
    {
        ucb_str_append_c(out, "%(");
        if (len)
            ucb_str_append_cstr(out, field, len);
        ucb_str_append_c(out, ")s");
        return;
    }

    if (len == 4 && strncmp(field, "name", 4) == 0)
        ucb_str_append(out, &o->name);
    else if (len == 4 && strncmp(field, "type", 4) == 0)
        ucb_str_append_c(out, type_name(o->type));
    else if (len == 7 && strncmp(field, "metavar", 7) == 0)
        append_metavar(out, o);
    else if (len == 7 && strncmp(field, "choices", 7) == 0)
    {
        if (o->choices)
        {
            for (size_t i = 0; i < o->choices->size; ++i)
            {
                if (i)
                    ucb_str_append_c(out, ", ");
                ucb_str_append(out, ucb_vector_str_get(o->choices, i));
            }
        }
    }
    else if (len == 7 && strncmp(field, "default", 7) == 0)
        append_default(out, o);
    else if (len == 6 && strncmp(field, "action", 6) == 0)
        ucb_str_append_c(out, action_name(o->action));
    else if (len == 5 && strncmp(field, "short", 5) == 0)
    {
        if (o->short_name)
        {
            ucb_str_append_c(out, "-");
            ucb_str_append_cstr(out, &o->short_name, 1);
        }
    }
    else if (len == 4 && strncmp(field, "long", 4) == 0)
    {
        if (o->long_name.size)
        {
            ucb_str_append_c(out, "--");
            ucb_str_append(out, &o->long_name);
        }
    }
    else
    {
        ucb_str_append_c(out, "%(");
        if (len)
            ucb_str_append_cstr(out, field, len);
        ucb_str_append_c(out, ")s");
    }
}

static ucb_str* interp_text(const ucb_arg_parser* p, const ucb_arg_opt_impl* o, const ucb_str* src)
{
    ucb_str* out = ucb_str_new_empty();
    if (!out)
        return UCB_NULL;

    const char* s = src->data;
    size_t n = src->size;
    size_t i = 0;
    while (i < n)
    {
        if (s[i] != '%')
        {
            size_t j = i;
            while (j < n && s[j] != '%')
                ++j;
            ucb_str_append_cstr(out, s + i, j - i);
            i = j;
            continue;
        }
        if (i + 1 < n && s[i + 1] == '%')
        {
            ucb_str_append_c(out, "%");
            i += 2;
            continue;
        }
        if (i + 5 <= n && strncmp(s + i, "%prog", 5) == 0)
        {
            ucb_str_append(out, &p->prog_base);
            i += 5;
            continue;
        }
        if (i + 8 <= n && strncmp(s + i, "%default", 8) == 0)
        {
            if (o)
                append_default(out, o);
            else
                ucb_str_append_c(out, "%default");
            i += 8;
            continue;
        }
        if (i + 1 < n && s[i + 1] == '(')
        {
            size_t j = i + 2;
            while (j < n && s[j] != ')')
                ++j;
            if (j < n && j + 1 < n && s[j + 1] == 's')
            {
                append_field(out, p, o, s + i + 2, j - (i + 2));
                i = j + 2;
                continue;
            }
        }
        ucb_str_append_c(out, "%");
        ++i;
    }
    return out;
}

/* -------------------------------------------------------------------------- */
/*                            Help text rendering                             */
/* -------------------------------------------------------------------------- */

static void append_indent(ucb_str* out, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        ucb_str_append_c(out, " ");
}

static void append_wrapped(ucb_str* out, const char* text, size_t indent, size_t width)
{
    size_t col = indent;
    const char* p = text;
    while (*p)
    {
        if (*p == '\n')
        {
            ucb_str_append_c(out, "\n");
            append_indent(out, indent);
            col = indent;
            ++p;
            continue;
        }
        const char* w = p;
        while (*p && *p != ' ' && *p != '\n')
            ++p;
        size_t wlen = (size_t)(p - w);

        if (col > indent)
        {
            if (col + 1 + wlen > width)
            {
                ucb_str_append_c(out, "\n");
                append_indent(out, indent);
                col = indent;
            }
            else
            {
                ucb_str_append_c(out, " ");
                ++col;
            }
        }
        if (wlen)
            ucb_str_append_cstr(out, w, wlen);
        col += wlen;
        while (*p == ' ')
            ++p;
    }
    ucb_str_append_c(out, "\n");
}

static void format_left(ucb_str* out, const ucb_arg_opt_impl* o, int builtin)
{
    if (builtin == 1)
    {
        ucb_str_append_c(out, "-h, --help");
        return;
    }
    if (builtin == 2)
    {
        ucb_str_append_c(out, "    --version");
        return;
    }

    bool has_short = o->short_name != 0;
    bool has_long = o->long_name.size > 0;

    if (has_short)
    {
        ucb_str_append_c(out, "-");
        ucb_str_append_cstr(out, &o->short_name, 1);
    }
    else
    {
        ucb_str_append_c(out, "    ");
    }

    if (has_long)
    {
        if (has_short)
            ucb_str_append_c(out, ", ");
        ucb_str_append_c(out, "--");
        ucb_str_append(out, &o->long_name);
        if (opt_takes_value(o))
        {
            ucb_str_append_c(out, "=");
            append_metavar(out, o);
        }
    }
    else if (opt_takes_value(o))
    {
        ucb_str_append_c(out, " ");
        append_metavar(out, o);
    }
}

typedef struct help_entry
{
    ucb_str left;
    const ucb_arg_opt_impl* opt;
    int builtin; /* 0 user, 1 help, 2 version */
} help_entry;

static void append_usage_line(ucb_str* out, const ucb_arg_parser* p)
{
    ucb_str_append_c(out, "Usage: ");
    if (p->usage.size)
    {
        ucb_str* t = interp_text(p, UCB_NULL, &p->usage);
        if (t)
        {
            ucb_str_append(out, t);
            ucb_str_free(t);
        }
    }
    else
    {
        ucb_str_append(out, &p->prog_base);
        ucb_str_append_c(out, " [options]");
    }
    ucb_str_append_c(out, "\n");
}

static ucb_str* build_help(const ucb_arg_parser* p)
{
    ucb_str* out = ucb_str_new_empty();
    if (!out)
        return UCB_NULL;

    append_usage_line(out, p);
    ucb_str_append_c(out, "\n");

    if (p->description.size)
    {
        ucb_str* t = interp_text(p, UCB_NULL, &p->description);
        if (t)
        {
            append_wrapped(out, t->data, 0, (size_t)p->width);
            ucb_str_free(t);
        }
        ucb_str_append_c(out, "\n");
    }

    ucb_str_append_c(out, "Options:\n");

    size_t count = p->opts->size + 1 + (p->has_version ? 1 : 0);
    help_entry* entries = ucb_calloc(count, sizeof(help_entry));
    if (!entries)
    {
        ucb_str_free(out);
        return UCB_NULL;
    }

    size_t idx = 0;
    ucb_str_init_empty(&entries[idx].left);
    entries[idx].builtin = 1;
    format_left(&entries[idx].left, UCB_NULL, 1);
    ++idx;

    for (size_t i = 0; i < p->opts->size; ++i)
    {
        ucb_arg_opt_impl* o = (ucb_arg_opt_impl*)ucb_vector_ptr_get(p->opts, i);
        ucb_str_init_empty(&entries[idx].left);
        entries[idx].opt = o;
        entries[idx].builtin = 0;
        format_left(&entries[idx].left, o, 0);
        ++idx;
    }

    if (p->has_version)
    {
        ucb_str_init_empty(&entries[idx].left);
        entries[idx].builtin = 2;
        format_left(&entries[idx].left, UCB_NULL, 2);
        ++idx;
    }

    size_t max_left = 0;
    for (size_t i = 0; i < count; ++i)
    {
        if (entries[i].left.size > max_left)
            max_left = entries[i].left.size;
    }

    size_t width = (size_t)p->width;
    size_t desc_col = max_left + 2;
    size_t limit = width > 20 ? width - 20 : 1;
    if (desc_col > limit)
        desc_col = limit;
    if (desc_col < 1)
        desc_col = 1;

    for (size_t i = 0; i < count; ++i)
    {
        help_entry* e = &entries[i];
        ucb_str* help = UCB_NULL;
        if (e->builtin == 1)
            help = ucb_str_new_c("show this help message and exit");
        else if (e->builtin == 2)
            help = ucb_str_new_c("show program's version number and exit");
        else
        {
            help = interp_text(p, e->opt, &e->opt->help);
            if (help && e->opt->has_def &&
                (e->opt->action == UCB_ARG_ACTION_STORE || e->opt->action == UCB_ARG_ACTION_APPEND))
            {
                const char* raw = e->opt->help.size ? e->opt->help.data : "";
                if (!strstr(raw, "%default") && !strstr(raw, "%(default)"))
                {
                    ucb_str_append_c(help, " (default: ");
                    append_default(help, e->opt);
                    ucb_str_append_c(help, ")");
                }
            }
        }
        if (!help)
        {
            for (size_t k = 0; k < count; ++k)
                ucb_str_release(&entries[k].left);
            ucb_free(entries);
            ucb_str_free(out);
            return UCB_NULL;
        }

        ucb_str_append(out, &e->left);
        if (e->left.size + 2 > desc_col)
        {
            ucb_str_append_c(out, "\n");
            append_indent(out, desc_col);
        }
        else
        {
            append_indent(out, desc_col - e->left.size);
        }
        append_wrapped(out, help->data, desc_col, width);
        ucb_str_free(help);
    }

    for (size_t i = 0; i < count; ++i)
        ucb_str_release(&entries[i].left);
    ucb_free(entries);

    return out;
}

/* -------------------------------------------------------------------------- */
/*                                   Output                                   */
/* -------------------------------------------------------------------------- */

static bool write_str(ucb_file* out, const ucb_str* s, ucb_error** perr)
{
    if (s->size == 0)
        return true;
    return ucb_file_write_full(out, s->data, s->size, perr);
}

UCB_API ucb_str* ucb_arg_parser_help_str(const ucb_arg_parser* p, ucb_error** perr)
{
    UCB_UNUSED(perr);
    UCB_VERIFY_ARGS(p);
    return build_help(p);
}

UCB_API ucb_str* ucb_arg_parser_usage_str(const ucb_arg_parser* p, ucb_error** perr)
{
    UCB_UNUSED(perr);
    UCB_VERIFY_ARGS(p);
    ucb_str* out = ucb_str_new_empty();
    if (!out)
        return UCB_NULL;
    append_usage_line(out, p);
    return out;
}

UCB_API ucb_str* ucb_arg_parser_version_str(const ucb_arg_parser* p, ucb_error** perr)
{
    UCB_UNUSED(perr);
    UCB_VERIFY_ARGS(p);

    ucb_str* out = ucb_str_new_empty();
    if (!out)
        return UCB_NULL;

    ucb_str_append(out, &p->prog_base);
    ucb_str_append_c(out, " version");
    if (p->version.size)
    {
        ucb_str_append_c(out, " ");
        ucb_str* t = interp_text(p, UCB_NULL, &p->version);
        if (t)
        {
            ucb_str_append(out, t);
            ucb_str_free(t);
        }
    }
    ucb_str_append_c(out, "\n");
    return out;
}

UCB_API bool ucb_arg_parser_print_help(const ucb_arg_parser* p, ucb_file* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(p && out);
    ucb_str* s = build_help(p);
    if (!s)
        return false;
    bool ok = write_str(out, s, perr);
    ucb_str_free(s);
    return ok;
}

UCB_API bool ucb_arg_parser_print_usage(const ucb_arg_parser* p, ucb_file* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(p && out);
    ucb_str* s = ucb_arg_parser_usage_str(p, perr);
    if (!s)
        return false;
    bool ok = write_str(out, s, perr);
    ucb_str_free(s);
    return ok;
}

UCB_API bool ucb_arg_parser_print_version(const ucb_arg_parser* p, ucb_file* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(p && out);
    ucb_str* s = ucb_arg_parser_version_str(p, perr);
    if (!s)
        return false;
    bool ok = write_str(out, s, perr);
    ucb_str_free(s);
    return ok;
}

UCB_API bool ucb_arg_parser_print_error(const ucb_arg_parser* p,
                                        const ucb_error* err,
                                        ucb_file* out,
                                        ucb_error** perr)
{
    UCB_VERIFY_ARGS(p && err && out);

    ucb_str* s = ucb_str_new_empty();
    if (!s)
        return false;
    append_usage_line(s, p);
    ucb_str_append(s, &p->prog_base);
    ucb_str_append_c(s, ": error: ");
    ucb_str_append_c(s, err->msg ? err->msg : "(no message)");
    ucb_str_append_c(s, "\n");

    bool ok = write_str(out, s, perr);
    ucb_str_free(s);
    return ok;
}
