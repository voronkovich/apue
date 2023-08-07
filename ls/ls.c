#define _GNU_SOURCE

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum sort_by
{
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_MTIME,
    UNSORTED
};

static struct opts
{
    bool all;
    bool list;
    bool directory;
    bool comma;
    enum sort_by sort;
    bool sort_reverse;
} opts;

struct ls_entry
{
    char *name;
    struct stat *stat;
    struct ls_entry *next;
};

struct ls_entry *
ls_entry_create(char *name, struct stat *stat)
{
    struct ls_entry *ent = malloc(sizeof(struct ls_entry));
    if (ent == NULL) {
        err(EXIT_FAILURE, "Cannot allocate memory");
    }

    ent->name = name;
    ent->stat = stat;
    ent->next = NULL;

    return ent;
}

int
ls_entry_cmp(struct ls_entry *ent1, struct ls_entry *ent2)
{
    int cmp = 0;

    switch (opts.sort) {
        case SORT_BY_NAME:
            cmp = strcasecmp(ent1->name, ent2->name);
            break;
        case SORT_BY_SIZE:
            cmp = ent2->stat->st_size - ent1->stat->st_size;
            break;
        case SORT_BY_MTIME:
            cmp = ent2->stat->st_mtime - ent1->stat->st_mtime;
            break;
        case UNSORTED:
            cmp = 0;
            break;
    }

    if (opts.sort_reverse) {
        cmp *= -1;
    }

    return cmp;
}

struct ls_entry *
ls_entry_list_insert(struct ls_entry *ents, struct ls_entry *ent)
{
    if (ents == NULL) {
        return ent;
    }

    if (ent == NULL) {
        return ents;
    }

    if (ls_entry_cmp(ent, ents) < 0) {
        ent->next = ents;

        return ent;
    }

    struct ls_entry *curr = ents;
    while (curr->next != NULL) {
        if (ls_entry_cmp(ent, curr->next) < 0) {
            break;
        }

        curr = curr->next;
    }

    ent->next = curr->next;
    curr->next = ent;

    return ents;
}

struct stat *
get_stat(char *filepath)
{
    struct stat *st = malloc(sizeof(struct stat));
    if (st == NULL) {
        err(EXIT_FAILURE, "Cannot allocate memory");
    }

    if (stat(filepath, st) == -1) {
        err(EXIT_FAILURE, "stat() '%s'", filepath);
    }

    return st;
}

struct ls_entry *
get_entries(char *path)
{
    struct ls_entry *ents = NULL;
    struct ls_entry *new = NULL;

    struct stat *pstat = get_stat(path);
    if (!S_ISDIR(pstat->st_mode) || opts.directory) {
        ents = ls_entry_create(path, pstat);

        return ents;
    }

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

        new = ls_entry_create(dent->d_name, get_stat(filepath));

        ents = ls_entry_list_insert(ents, new);
    }

    return ents;
}

void
list_entries(struct ls_entry *ents)
{
    char *sep = "\t";
    if (opts.comma) {
        sep = ", ";
    }
    if (opts.list)  {
        sep = "\n";
    }

    while (ents != NULL) {
        if (ents->next == NULL) {
            sep = "";
        }

        printf("%s%s", ents->name, sep);

        ents = ents->next;
    }

    fputs("\n", stdout);
}

int
main(int argc, char *argv[])
{
    opts.all = false;
    opts.list = false;
    opts.directory = false;
    opts.sort = SORT_BY_NAME;
    opts.sort_reverse = false;

    int opt;
    extern int optind;
    while ((opt = getopt(argc, argv, "+adlmrtSU")) != -1) {
        switch (opt) {
            case 'a':
                opts.all = true;
                break;
            case 'd':
                opts.directory = true;
                break;
            case 'l':
                opts.list = true;
                break;
            case 'm':
                opts.comma = true;
                break;
            case 'r':
                opts.sort_reverse = true;
                break;
            case 't':
                opts.sort = SORT_BY_MTIME;
                break;
            case 'S':
                opts.sort = SORT_BY_SIZE;
                break;
            case 'U':
                opts.sort = UNSORTED;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
    argc -= optind;
    argv += optind;

    char *path = argc > 0 ? argv[0] : ".";

    struct ls_entry *ents;

    ents = get_entries(path);
    list_entries(ents);

    return EXIT_SUCCESS;
}
