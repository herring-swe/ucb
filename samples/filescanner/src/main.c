#include <ucb/argparse.h>
#include <ucb/cstring.h>
#include <ucb/fs.h>
#include <ucb/memdbg.h>
#include <ucb/string_ex.h>
#include <ucb/ucb.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    ucb_init_console();
    // The final report is installed automatically at exit and only shows
    // something when memory is leaked.
    UCB_MEMTRACK_ENABLE(false);

    ucb_error* err = NULL;

    ucb_vector_str* paths = ucb_vector_str_new();
    bool recursive = false;

    ucb_arg_parser* parser = ucb_arg_parser_new("ucb_filescanner");
    ucb_arg_parser_set_description(parser, "Simple file scanner");
    // Simple file version reporting... fine.
    ucb_arg_parser_set_version(parser, "1.0\n© 2026 Åke Svedin");

    // Positionals are all added during parse.
    // -h/--help and --version are ordinary options with dedicated actions;
    // parsing stops and reports the status so one exit path can print and clean up.
    ucb_arg_opt opts[] = {
        {
            .name = "recursive",
            .short_name = 'r',
            .type = UCB_ARG_TYPE_BOOL,
            .action = UCB_ARG_ACTION_STORE_TRUE,
            .dest = &recursive,
            .help = "Scan recursively",
        },
        {
            .name = "version",
            .long_name = "version",
            .action = UCB_ARG_ACTION_VERSION,
            .help = "Show version information",
        },
        {
            .name = "help",
            .short_name = 'h',
            .long_name = "help",
            .action = UCB_ARG_ACTION_HELP,
            .help = "Show this help message and exit",
        },
    };

    if (!ucb_arg_parser_add_options(parser, opts, sizeof(opts) / sizeof(opts[0])))
    {
        // Adding options is not user input; malformed descriptors abort inside
        // the add call, so a false return is only an internal allocation failure.
        ucb_arg_parser_free(parser);
        ucb_vector_str_free_full(paths);
        return 1;
    }

    ucb_arg_status status = ucb_arg_parser_parse(parser, argc, argv, paths, &err);
    if (status != UCB_ARG_OK)
    {
        if (UCB_IS_THROWN(err))
        {
            ucb_arg_parser_print_error(parser, err, ucb_file_stderr());
            ucb_arg_parser_print_help(parser, ucb_file_stdout());
            ucb_error_clear(&err);
        }
        ucb_arg_parser_free(parser);
        ucb_vector_str_free_full(paths);
        return status == UCB_ARG_ERROR ? 1 : 0;
    }

    printf("Recursive: %d\n", recursive);
    if (ucb_vector_str_is_empty(paths))
    {
        ucb_str* pwd = ucb_fs_cwd();
        printf("Path: %s\n", UCB_CSTR(pwd));
        ucb_str_free(pwd);
    }
    else
    {
        for (size_t i = 0; i < ucb_vector_str_size(paths); i++)
        {
            printf("Path: %s\n", UCB_CSTR(ucb_vector_str_get(paths, i)));
        }
    }

    ucb_arg_parser_free(parser);
    ucb_vector_str_free_full(paths);

    return 0;
}
