/**
 * @file argparse.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Python-optparse style argument parsing
 *
 * @ref ucb_arg_parser is a heap allocated, opaque argument parser inspired by
 * Python's @c optparse. It uses a hybrid builder/table API: options are declared
 * one by one with @ref ucb_arg_parser_add or in a table with
 * @ref ucb_arg_parser_add_options, and a single call to
 * @ref ucb_arg_parser_parse fills caller owned destinations.
 *
 * Design contract:
 * - Parsing is **non-fatal for user input**. @ref ucb_arg_parser_parse never
 *   exits the process; it returns a @ref ucb_arg_status and, on failure, throws
 *   a @ref UCB_ERROR_INVALID_ARG error into the provided @ref ucb_error pointer.
 * - Value binding is direct: every option carries a @c void* destination that
 *   must outlive parsing. The parser writes parsed and default values into it.
 * - On add, the parser deep-copies the option descriptor, every string and the
 *   choices array. The caller's @ref ucb_arg_opt may be a temporary local; only
 *   the @c dest pointers must stay valid until parsing.
 * - All textual inputs are UTF-8 encoded @c const char* values.
 * - A parser is **not thread-safe**. Use it from one thread at a time.
 * - GNU command line conventions are used: @c --long, @c --long=value,
 *   @c -ab (two short flags), @c -ovalue and @c -o value. Options and positional
 *   arguments may interleave and @c -- stops option scanning.
 *
 * @section argparse_interpolation Interpolation
 *
 * Usage, description and help texts support a small interpolation language:
 * - @c %prog is replaced with the basename of the program name.
 * - @c %% is a literal percent sign.
 * - @c %(field)s is replaced with an option field, where @c field is one of
 *   @c prog, @c name, @c type, @c metavar, @c choices, @c default, @c action,
 *   @c short or @c long.
 * - @c %default is a shorthand for @c %(default)s.
 *
 * Unknown or malformed fields are kept verbatim.
 *
 * @section argparse_memory Memory and ownership
 *
 * - The parser is created with @ref ucb_arg_parser_new and released with
 *   @ref ucb_arg_parser_free.
 * - A @ref UCB_ARG_TYPE_STR destination is a caller initialized @ref ucb_str.
 *   The parser overwrites it with an owned copy; the caller releases it with
 *   @ref ucb_str_release.
 * - A @ref UCB_ARG_ACTION_APPEND destination is a caller initialized
 *   @ref ucb_vector_str; the parser pushes owned @ref ucb_str copies into it and
 *   clears it at the start of parsing. The caller owns the elements.
 * - Positional arguments are pushed as owned @ref ucb_str copies into the
 *   optional @ref ucb_vector_str passed to @ref ucb_arg_parser_parse.
 *
 * @section argparse_argv The @c argv convention
 *
 * @ref ucb_arg_parser_parse takes @c argc and @c argv as received by @c main:
 * @c argv[0] is the program name and is ignored by the scanner. The program name
 * used for @c %prog and error prefixes is the one given to
 * @ref ucb_arg_parser_new.
 */

#ifndef UCB_ARGPARSE_H
#define UCB_ARGPARSE_H

#include <ucb/container/impl/vector_str.h>
#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/file.h>
#include <ucb/string.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * @struct ucb_arg_parser
 * @brief Opaque argument parser handle
 *
 * Created with @ref ucb_arg_parser_new and released with
 * @ref ucb_arg_parser_free.
 */
typedef struct ucb_arg_parser ucb_arg_parser;

/**
 * @brief The value type an option stores
 *
 * @ref UCB_ARG_TYPE_NONE is used by options that do not consume and store a
 * typed value: boolean flags, counters and raw string append actions.
 */
typedef enum ucb_arg_type
{
    UCB_ARG_TYPE_NONE = 0, ///< No typed value (flags, count, append)
    UCB_ARG_TYPE_BOOL,     ///< @c bool destination
    UCB_ARG_TYPE_STR,      ///< @ref ucb_str destination
    UCB_ARG_TYPE_INT,      ///< @c int destination
    UCB_ARG_TYPE_UINT,     ///< @c unsigned @c int destination
    UCB_ARG_TYPE_LONG,     ///< @c long destination
    UCB_ARG_TYPE_ULONG,    ///< @c unsigned @c long destination
    UCB_ARG_TYPE_LLONG,    ///< @c long @c long destination
    UCB_ARG_TYPE_ULLONG,   ///< @c unsigned @c long @c long destination
    UCB_ARG_TYPE_SIZE,     ///< @c size_t destination
    UCB_ARG_TYPE_FLOAT,    ///< @c float destination
    UCB_ARG_TYPE_DOUBLE,   ///< @c double destination
} ucb_arg_type;

/**
 * @brief What an option does with its value
 */
typedef enum ucb_arg_action
{
    /**
     * @brief Consume a value and store it converted into @c dest
     *
     * Requires a non-@ref UCB_ARG_TYPE_NONE @ref ucb_arg_opt::type and a
     * non-NULL @ref ucb_arg_opt::dest.
     */
    UCB_ARG_ACTION_STORE = 0,
    /**
     * @brief Boolean flag that sets @c dest to true
     *
     * Does not consume a value. @c dest is a @c bool*.
     */
    UCB_ARG_ACTION_STORE_TRUE,
    /**
     * @brief Boolean flag that sets @c dest to false
     *
     * Does not consume a value. @c dest is a @c bool*.
     */
    UCB_ARG_ACTION_STORE_FALSE,
    /**
     * @brief Count occurrences of a flag
     *
     * Does not consume a value. @c dest is an @c int* that is incremented for
     * every occurrence, so @c -vvv yields @c 3.
     */
    UCB_ARG_ACTION_COUNT,
    /**
     * @brief Append the raw value to a string vector
     *
     * Consumes a value and pushes an owned @ref ucb_str copy into the
     * @ref ucb_vector_str pointed to by @c dest.
     */
    UCB_ARG_ACTION_APPEND,
    /**
     * @brief Print help and end parsing with @ref UCB_ARG_HELP
     *
     * Does not consume a value and ignores @c dest and @c type. When the option
     * is encountered, the parser writes the full help text to stdout and returns
     * @ref UCB_ARG_HELP immediately, so the caller only needs to clean up and
     * exit. Declare this on whichever option should act as help, for example
     * @c -h/--help. Use @ref ucb_arg_parser_print_help directly to write to
     * another destination.
     */
    UCB_ARG_ACTION_HELP,
    /**
     * @brief Print the version and end parsing with @ref UCB_ARG_VERSION
     *
     * Like @ref UCB_ARG_ACTION_HELP but writes the version line to stdout and
     * returns @ref UCB_ARG_VERSION immediately. Declare it on your version
     * option. Use @ref ucb_arg_parser_print_version directly to write to another
     * destination.
     */
    UCB_ARG_ACTION_VERSION,
} ucb_arg_action;

/**
 * @brief Descriptor for a single option
 *
 * This is a plain value that may live on the stack. @ref ucb_arg_parser_add
 * deep-copies every field, so the descriptor may be a temporary.
 *
 * At least one of @ref short_name or @ref long_name must be provided. There are
 * no reserved names: @c -h, @c --help and @c --version are ordinary options that
 * the caller declares like any other and wires to @ref UCB_ARG_ACTION_HELP or
 * @ref UCB_ARG_ACTION_VERSION, which print and stop parsing automatically.
 */
typedef struct ucb_arg_opt
{
    /** @brief Required identifier used for help and @c %name */
    const char* name;
    /** @brief Short option character (for example @c 'v'), or @c 0 for none */
    char short_name;
    /** @brief Long option name without dashes (for example @c "verbose"), or NULL */
    const char* long_name;
    /** @brief Value type, see @ref ucb_arg_type */
    ucb_arg_type type;
    /** @brief What the option does, see @ref ucb_arg_action */
    ucb_arg_action action;
    /**
     * @brief Destination written by the parser
     *
     * Required for @ref UCB_ARG_ACTION_STORE, @ref UCB_ARG_ACTION_STORE_TRUE,
     * @ref UCB_ARG_ACTION_STORE_FALSE, @ref UCB_ARG_ACTION_COUNT and
     * @ref UCB_ARG_ACTION_APPEND, and ignored by @ref UCB_ARG_ACTION_HELP and
     * @ref UCB_ARG_ACTION_VERSION. The pointer type follows the action and type;
     * see @ref ucb_arg_action and @ref ucb_arg_type.
     */
    void* dest;
    /** @brief Optional string default, parsed with the option's converter */
    const char* def;
    /** @brief Optional metavar for help, defaults to the uppercased @ref name */
    const char* metavar;
    /** @brief Optional help text, supports interpolation */
    const char* help;
    /** @brief Optional array of allowed raw values */
    const char* const* choices;
    /** @brief Number of entries in @ref choices */
    size_t num_choices;
    /** @brief When true the option must appear at least once */
    bool required;
} ucb_arg_opt;

/**
 * @brief Create a zero initialized @ref ucb_arg_opt
 *
 * All pointers are NULL, @c short_name is @c 0, @c type is
 * @ref UCB_ARG_TYPE_NONE and @c action is @ref UCB_ARG_ACTION_STORE.
 *
 * @return a zero initialized option descriptor
 */
static inline ucb_arg_opt ucb_arg_opt_make(void)
{
    ucb_arg_opt opt = {0};
    return opt;
}

/**
 * @brief Outcome of @ref ucb_arg_parser_parse
 */
typedef enum ucb_arg_status
{
    UCB_ARG_OK = 0, ///< Parsing completed successfully
    /**
     * @brief An option declared with @ref UCB_ARG_ACTION_HELP was encountered
     *
     * Parsing stopped early after the parser wrote the help text to stdout.
     */
    UCB_ARG_HELP,
    /**
     * @brief An option declared with @ref UCB_ARG_ACTION_VERSION was encountered
     *
     * Parsing stopped early after the parser wrote the version line to stdout.
     */
    UCB_ARG_VERSION,
    UCB_ARG_ERROR, ///< A parse or validation error was thrown into @c perr
} ucb_arg_status;

/* -------------------------------------------------------------------------- */
/*                                  Lifetime                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a new argument parser
 *
 * @p prog is the program name used for @c %prog and error prefixes. Only its
 * basename is used in generated text; both @c / and @c \\ separators are
 * recognized.
 *
 * @param prog program name, must be non-NULL
 * @return a new owning handle, or UCB_NULL on failure
 */
UCB_API ucb_arg_parser* ucb_arg_parser_new(const char* prog);

/**
 * @brief Release a parser and all owned descriptors and strings
 *
 * @p p may be UCB_NULL.
 *
 * @param p the parser to free, may be UCB_NULL
 */
UCB_API void ucb_arg_parser_free(ucb_arg_parser* p);

/* -------------------------------------------------------------------------- */
/*                                   Metadata                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set a custom usage line
 *
 * @p usage supports interpolation. When unused the default @c "%prog [options]"
 * is rendered.
 *
 * @param p the parser, must be non-NULL
 * @param usage the usage text, or NULL to clear it
 * @return true on success, false on allocation failure
 */
UCB_API bool ucb_arg_parser_set_usage(ucb_arg_parser* p, const char* usage);

/**
 * @brief Set the description shown at the top of @c --help
 *
 * @p description supports interpolation and is wrapped to the configured width.
 *
 * @param p the parser, must be non-NULL
 * @param description the description, or NULL to clear it
 * @return true on success, false on allocation failure
 */
UCB_API bool ucb_arg_parser_set_description(ucb_arg_parser* p, const char* description);

/**
 * @brief Set the version string
 *
 * The string is used by @ref ucb_arg_parser_version_str and
 * @ref ucb_arg_parser_print_version, which renders @c "<prog> version <version>".
 * This does not register any option; declare an option such as @c --version with
 * @ref UCB_ARG_ACTION_VERSION yourself.
 *
 * @param p the parser, must be non-NULL
 * @param version the version string, or NULL to clear it
 * @return true on success, false on allocation failure
 */
UCB_API bool ucb_arg_parser_set_version(ucb_arg_parser* p, const char* version);

/**
 * @brief Set the help output width in columns
 *
 * The default is 80. Values below 20 are clamped to 20.
 *
 * @param p the parser, must be non-NULL
 * @param width the desired width in columns
 */
UCB_API void ucb_arg_parser_set_width(ucb_arg_parser* p, int width);

/* -------------------------------------------------------------------------- */
/*                              Option declaration                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add a single option
 *
 * The descriptor and all of its strings are deep-copied. This does not parse
 * command line input, so a malformed descriptor is a programming error: it is
 * reported with @ref UCB_VERIFY / @ref UCB_REPORT and aborts. That includes an
 * empty @ref ucb_arg_opt::name, a missing short and long name, duplicate names
 * and an action whose required @ref ucb_arg_opt::type or @ref ucb_arg_opt::dest
 * is missing.
 *
 * @param p the parser, must be non-NULL
 * @param opt the option descriptor, must be non-NULL
 * @return true on success, false only on an internal allocation failure
 */
UCB_API bool ucb_arg_parser_add(ucb_arg_parser* p, const ucb_arg_opt* opt);

/**
 * @brief Add a table of options
 *
 * A thin loop over @ref ucb_arg_parser_add. Since a malformed descriptor aborts,
 * the loop either completes or does not return.
 *
 * @param p the parser, must be non-NULL
 * @param opts array of @p count descriptors, must be non-NULL when @p count > 0
 * @param count number of descriptors
 * @return true on success, false only on an internal allocation failure
 */
UCB_API bool ucb_arg_parser_add_options(ucb_arg_parser* p, const ucb_arg_opt* opts, size_t count);

/* -------------------------------------------------------------------------- */
/*                                   Parsing                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Parse command line arguments
 *
 * The scanner follows GNU conventions:
 * - @c -- stops option scanning; every remaining token is positional.
 * - Long options are @c --name or @c --name=value (exact match, no
 *   abbreviation). A value taking option accepts @c =value or the next token; a
 *   flag rejects @c =value.
 * - Short options may be bundled (@c -ab) and a value taking option consumes the
 *   rest of the token (@c -ovalue) or the next token (@c -o value).
 * - A lone @c - and tokens that look like negative numbers are positional.
 * - No option name is handled implicitly. An option declared with
 *   @ref UCB_ARG_ACTION_HELP writes the help text to stdout and returns
 *   @ref UCB_ARG_HELP immediately; one declared with
 *   @ref UCB_ARG_ACTION_VERSION writes the version line to stdout and returns
 *   @ref UCB_ARG_VERSION immediately. They apply wherever the caller attached
 *   them (@c -h/--help and @c --version are the usual choices).
 *
 * At the start of the call every destination is initialized: numeric and bool
 * destinations are zeroed and then receive their string default (if any),
 * string destinations are cleared and assigned their default, counters are reset
 * and append vectors are cleared deep. After scanning, every option marked
 * @ref ucb_arg_opt::required must have been seen.
 *
 * Positional arguments are appended as owned @ref ucb_str copies to
 * @p positionals when it is non-NULL. @c argv[0] is skipped, so the program
 * name is not treated as a positional.
 *
 * On failure the function throws @ref UCB_ERROR_INVALID_ARG into @p perr and
 * returns @ref UCB_ARG_ERROR.
 *
 * @param p the parser, must be non-NULL
 * @param argc argument count as received by @c main
 * @param argv argument vector as received by @c main; @c argv[0] is the program
 *             name and is ignored
 * @param positionals optional vector that receives owned positional copies, or
 *                    UCB_NULL to discard them
 * @param perr optional location to store the error on failure
 * @return the parse status
 */
UCB_API ucb_arg_status ucb_arg_parser_parse(ucb_arg_parser* p,
                                            int argc,
                                            const char* const* argv,
                                            ucb_vector_str* positionals,
                                            ucb_error** perr);

/* -------------------------------------------------------------------------- */
/*                                   Output                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Render the full help text into a new string
 *
 * The result contains the usage line, the description and an aligned option
 * list. It is owned by the caller and must be released with @ref ucb_str_free.
 *
 * @param p the parser, must be non-NULL
 * @return a new string, or UCB_NULL on failure
 */
UCB_API ucb_str* ucb_arg_parser_help_str(const ucb_arg_parser* p);

/**
 * @brief Render the usage line into a new string
 *
 * The result is owned by the caller and must be released with @ref ucb_str_free.
 *
 * @param p the parser, must be non-NULL
 * @return a new string, or UCB_NULL on failure
 */
UCB_API ucb_str* ucb_arg_parser_usage_str(const ucb_arg_parser* p);

/**
 * @brief Render the version line into a new string
 *
 * The result is owned by the caller and must be released with @ref ucb_str_free.
 *
 * @param p the parser, must be non-NULL
 * @return a new string, or UCB_NULL on failure
 */
UCB_API ucb_str* ucb_arg_parser_version_str(const ucb_arg_parser* p);

/**
 * @brief Write the full help text to a file handle
 *
 * @param p the parser, must be non-NULL
 * @param out writable destination, must be non-NULL
 * @return true on success, false only on an internal allocation or write failure
 */
UCB_API bool ucb_arg_parser_print_help(const ucb_arg_parser* p, ucb_file* out);

/**
 * @brief Write the usage line to a file handle
 *
 * @param p the parser, must be non-NULL
 * @param out writable destination, must be non-NULL
 * @return true on success, false only on an internal allocation or write failure
 */
UCB_API bool ucb_arg_parser_print_usage(const ucb_arg_parser* p, ucb_file* out);

/**
 * @brief Write the version line to a file handle
 *
 * A version string must have been set with @ref ucb_arg_parser_set_version;
 * omitting it is a programming error and aborts via @ref UCB_VERIFY_ARGS.
 *
 * @param p the parser, must be non-NULL and have a version string set
 * @param out writable destination, must be non-NULL
 * @return true on success, false only on an internal allocation or write failure
 */
UCB_API bool ucb_arg_parser_print_version(const ucb_arg_parser* p, ucb_file* out);

/**
 * @brief Write a usage line followed by a formatted error
 *
 * Produces the usage line, then @c "<prog>: error: <message>".
 *
 * @param p the parser, must be non-NULL
 * @param err the error to report, must be non-NULL
 * @param out writable destination, must be non-NULL
 * @return true on success, false only on an internal allocation or write failure
 */
UCB_API bool ucb_arg_parser_print_error(const ucb_arg_parser* p,
                                        const ucb_error* err,
                                        ucb_file* out);

#endif // UCB_ARGPARSE_H
