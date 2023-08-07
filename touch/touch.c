#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PERMS_0644 (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

static struct options
{
    bool create_file;
    bool access;
    bool modify;
    int time;
} opts;

void
touch(char *pathname)
{
    int flags = O_RDONLY | (opts.create_file ? O_CREAT : 0);

    int fd = open(pathname, flags, PERMS_0644);
    if (fd == -1) {
        err(EXIT_FAILURE, "cannot touch '%s'", pathname);
    }

    time_t timestamp = opts.time >= 0 ? (time_t) opts.time : time(NULL);
    struct timespec timestamps[2];
    memset(timestamps, 0, sizeof(timestamps));

    if (opts.access) {
        timestamps[0].tv_sec = timestamp;
        timestamps[0].tv_nsec = 0;
    } else {
        timestamps[0].tv_nsec = UTIME_OMIT;
    }

    if (opts.modify) {
        timestamps[1].tv_sec = timestamp;
        timestamps[1].tv_nsec = 0;
    } else {
        timestamps[1].tv_nsec = UTIME_OMIT;
    }

    if (futimens(fd, timestamps) == -1) {
        err(EXIT_FAILURE, "cannot touch '%s'", pathname);
    }

    close(fd);
}

int
main(int argc, char *argv[])
{
    opts.create_file = true;
    opts.access = true;
    opts.modify = true;
    opts.time = -1;

    int opt = -1;
    extern int optind;
    extern char *optarg;
    while ((opt = getopt(argc, argv, "+:acmt:")) != -1) {
        switch (opt) {
            case 'a':
                opts.modify = false;
                break;
            case 'c':
                opts.create_file = false;
                break;
            case 'm':
                opts.access = false;
                break;
            case 't':
                opts.time = atoi(optarg);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    argc -= optind;
    argv += optind;
    if (argc < 1) {
        errx(EXIT_FAILURE, "missing file operand");
    }

    touch(argv[0]);

    return EXIT_SUCCESS;
}
