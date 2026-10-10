#include <ucb/argparse.h>
#include <ucb/buffer.h>
#include <ucb/cstring.h>
#include <ucb/dir.h>
#include <ucb/fs.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/path.h>
#include <ucb/string_ex.h>
#include <ucb/ucb.h>

#include <stdio.h>
#include <stdlib.h>

/**
 * Take scan dirs. Removes those that are nested in each other (if recursive).
 * Starts the scan from the topmost dirs.
 * @param dirs
 */
static int resolve_scan_dirs(ucb_vector_str* dirs, bool recursive, ucb_path*** out_paths)
{
    ucb_path** paths = ucb_calloc_type(dirs->size, ucb_path*);
    int npaths = 0;

    for (int is = 0; is < ucb_vector_str_size(dirs); is++)
    {
        ucb_str* str = ucb_vector_str_get(dirs, is);
        ucb_path* path = ucb_path_new(UCB_CSTR(str), ucb_str_len(str));
        ucb_path_normalize(path);
        ucb_path_set_style(path, UCB_PATH_STYLE_POSIX);

        // Check if duplicate or inside of other
        if (npaths)
        {
            for (int ip = 0; ip < npaths; ip++)
            {
                if (ucb_path_equals(path, paths[ip]))
                {
                    ucb_path_free(path);
                    path = NULL;
                    break;
                }
                if (recursive && ucb_path_is_inside(path, paths[ip]))
                {
                    // Keep the shortest
                    if (ucb_path_len(path) < ucb_path_len(paths[ip]))
                    {
                        fprintf(stderr, "Skipping nested path: %s\n", ucb_path_cstr(paths[ip]));
                        ucb_path_free(paths[ip]);
                        paths[ip] = path;
                    }
                    else
                    {
                        fprintf(stderr, "Skipping nested path: %s\n", ucb_path_cstr(path));
                        ucb_path_free(path);
                    }
                    path = NULL;
                    break;
                }
            }
        }
        if (path)
        {
            paths[npaths++] = path;
        }
    }

    if (out_paths)
        *out_paths = paths;
    return npaths;
}

static void report_entry(const char* fn, const ucb_fs_stat* st, const char* lead)
{
    if (!st)
    {
        printf("%se: %s\n", lead, fn);
        return;
    }

    switch (st->kind)
    {
    case UCB_FS_KIND_DIR:
        printf("%sd: %s/\n", lead, fn);
        break;
    case UCB_FS_KIND_FILE:
        printf("%sf: %s\n", lead, fn);
        break;
    case UCB_FS_KIND_SYMLINK:
        printf("%sl: %s\n", lead, fn);
        break;
    case UCB_FS_KIND_OTHER:
        printf("%so: %s\n", lead, fn);
        break;
    case UCB_FS_KIND_UNKNOWN:
        printf("%su: %s\n", lead, fn);
        break;

    default:
        break;
    }
}

static void scan_dir(const char* path, bool recursive, int level)
{
    ucb_error* err = NULL;

    ucb_dir* dir = ucb_dir_open(path, 0, &err);
    if (UCB_IS_THROWN(err))
    {
        printf("Error: %s\n", err->msg);
        ucb_error_clear(&err);
        return;
    }

    char str[256];
    if (level < 0)
    {
        level = 0;
    }
    else
    {
        int i;
        for (i = 0; i < level; i++)
        {
            str[i] = ' ';
        }
        str[i] = '\0';
    }

    ucb_dir_entry entry;
    ucb_fs_stat st;

    size_t plen = strlen(path) + 1;
    ucb_buffer pbuf;
    ucb_buffer_init_heap(&pbuf, plen + 64);
    ucb_buffer_push(&pbuf, path, plen - 1);
    ucb_buffer_push(&pbuf, ucb_path_sep_cstr(), 2); // Null terminated

    while (ucb_dir_next(dir, &entry, &err))
    {
        ucb_buffer_truncate(&pbuf, plen);
        ucb_buffer_push(&pbuf, entry.name, strlen(entry.name));
        ucb_buffer_push_null(&pbuf);

        const char* sub_path = (const char*)ucb_buffer_data(&pbuf);

        if (!ucb_fs_get_stat(sub_path, UCB_FS_NOFOLLOW, &st, &err))
        {
            fprintf(stderr, "Failed to read %s: %s\n", sub_path, err->msg);
            ucb_error_clear(&err);
            report_entry(entry.name, UCB_NULL, str);
        }
        else
        {
            report_entry(entry.name, &st, str);
        }

        if (recursive && st.kind == UCB_FS_KIND_DIR)
        {
            scan_dir(sub_path, recursive, level + 1);
        }
    }
    ucb_buffer_release(&pbuf);

    if (UCB_IS_THROWN(err))
    {
        printf("Error: %s\n", err->msg);
        ucb_error_clear(&err);
        return;
    }

    ucb_dir_close(dir, NULL);
    ucb_dir_free(dir);
}

int main(int argc, char** argv)
{
    ucb_init_console();
    // The final report is installed automatically at exit and only shows
    // something when memory is leaked.
    UCB_MEMTRACK_ENABLE(false);

    ucb_error* err = NULL;

    ucb_vector_str* spaths = ucb_vector_str_new();
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
        ucb_vector_str_free_full(spaths);
        return 1;
    }

    ucb_arg_status status =
        ucb_arg_parser_parse(parser, argc, (const char* const*)argv, spaths, &err);
    if (status != UCB_ARG_OK)
    {
        if (UCB_IS_THROWN(err))
        {
            ucb_arg_parser_print_error(parser, err, ucb_file_stderr());
            ucb_arg_parser_print_help(parser, ucb_file_stdout());
            ucb_error_clear(&err);
        }
        ucb_arg_parser_free(parser);
        ucb_vector_str_free_full(spaths);
        return status == UCB_ARG_ERROR ? 1 : 0;
    }

    printf("Recursive: %d\n", recursive);

    if (ucb_vector_str_is_empty(spaths))
    {
        ucb_vector_str_push_back(spaths, ucb_str_new(".", 0));
    }

    ucb_path** paths = NULL;
    int nump = resolve_scan_dirs(spaths, recursive, &paths);
    if (nump < 0)
    {
        fprintf(stderr, "Failed to resolve scan directories\n");
    }
    else
    {
        for (int i = 0; i < nump; i++)
        {
            const char* path = UCB_CSTR(paths[i]);
            ucb_fs_stat st;

            if (!ucb_fs_get_stat(path, UCB_FS_NOFOLLOW, &st, &err))
            {
                fprintf(stderr, "Failed to read %s: %s\n", path, err->msg);
                ucb_error_clear(&err);
                report_entry(path, UCB_NULL, "");
                continue;
            }

            report_entry(path, &st, "");
            if (recursive && st.kind == UCB_FS_KIND_DIR)
            {
                scan_dir(path, recursive, 1);
            }
            ucb_path_free(paths[i]);
        }
    }
    ucb_free((void*)paths);

    // if (ucb_vector_str_is_empty(paths))
    // {
    //     ucb_str* pwd = ucb_fs_cwd();
    //     printf("Path: %s\n", UCB_CSTR(pwd));
    //     scan_dir(UCB_CSTR(pwd), recursive, 0);
    //     ucb_str_free(pwd);
    // }
    // else
    // {
    //     for (size_t i = 0; i < ucb_vector_str_size(paths); i++)
    //     {
    //         const char* path = UCB_CSTR(ucb_vector_str_get(paths, i));
    //         printf("Path: %s\n", path);
    //         scan_dir(path, recursive, 0);
    //     }
    // }

    ucb_arg_parser_free(parser);
    ucb_vector_str_free_full(spaths);

    return 0;
}
