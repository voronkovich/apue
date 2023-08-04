#define _GNU_SOURCE

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static struct opts
{
    short int all;
    short int list;
    short int directory;
    char *separator;
} opts;

void
lsfile(char *path)
{
    char *basename = strrchr(path, '/');
    basename = (basename == NULL) ? path : basename + 1;

    fputs(basename, stdout);
}

void
lsdir(char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        err(EXIT_FAILURE, "Cannot open directory '%s'", path);
    }

    struct dirent *dent = NULL;
    while ((dent = readdir(dir)) != NULL) {
        if (!opts.all && dent->d_name[0] == '.') {
            continue;
        }

        char filepath[PATH_MAX + 1];
        snprintf(
            filepath, sizeof(filepath),
            "%s/%s", path, dent->d_name
        );

        lsfile(filepath);

        fputs(opts.separator, stdout);
    }
}

int
main(int argc, char *argv[])
{
    opts.all = 0;
    opts.list = 0;
    opts.directory = 0;
    opts.separator = "  ";

    int opt;
    extern int optind;
    while ((opt = getopt(argc, argv, "+adlm")) != -1) {
        switch (opt) {
            case 'a':
                opts.all = 1;
                break;
            case 'l':
                opts.list = 1;
                opts.separator = "\n";
                break;
            case 'd':
                opts.directory = 1;
                break;
            case 'm':
                opts.separator = ", ";
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
    argc -= optind;
    argv += optind;

    char *path = argc > 0 ? argv[0] : ".";

    struct stat sb;
    if (stat(path, &sb) == -1) {
        err(EXIT_FAILURE, "stat()");
    }

    if (S_ISDIR(sb.st_mode) && !opts.directory) {
        lsdir(path);
    } else {
        lsfile(path);
    }

    if (!opts.list) {
        fputs("\n", stdout);
    }

    return EXIT_SUCCESS;
}
