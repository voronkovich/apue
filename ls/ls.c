#define _GNU_SOURCE

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define USERNAME_MAXLEN 32
#define GROUPNAME_MAXLEN 32
#define FILEOWNER_MAXLEN (USERNAME_MAXLEN + GROUPNAME_MAXLEN + 1)
#define FILETIME_FORMAT "%x %T"

enum sort_by
{
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_MTIME,
    UNSORTED
};

enum filetime
{
    ATIME,
    MTIME,
};

static struct opts
{
    bool all;
    bool list;
    bool directory;
    bool comma;
    bool numeric_ids;
    enum filetime filetime;
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

char
filetype(mode_t mode)
{
    if (S_ISREG(mode))  return '-';
    if (S_ISDIR(mode))  return 'd';
    if (S_ISLNK(mode))  return 'l';
    if (S_ISFIFO(mode)) return 'p';
    if (S_ISSOCK(mode)) return 's';
    if (S_ISCHR(mode))  return 'c';
    if (S_ISBLK(mode))  return 'b';

    return '?';
}

void
fileperms(mode_t mode, char perms[9])
{
    perms[0] = mode & S_IRUSR ? 'r'  : '-';
    perms[1] = mode & S_IWUSR ? 'w'  : '-';
    perms[2] = mode & S_IXUSR ? 'x'  : '-';
    perms[3] = mode & S_IRGRP ? 'r'  : '-';
    perms[4] = mode & S_IWGRP ? 'w'  : '-';
    perms[5] = mode & S_IXGRP ? 'x'  : '-';
    perms[6] = mode & S_IROTH ? 'r'  : '-';
    perms[7] = mode & S_IWOTH ? 'w'  : '-';
    perms[8] = mode & S_IXOTH ? 'x'  : '-';
}

void
fileowner(uid_t uid, gid_t gid, char owner[FILEOWNER_MAXLEN + 1])
{
    struct passwd *usr = NULL;
    char username[USERNAME_MAXLEN + 1] = "";

    struct group *grp = NULL;
    char groupname[GROUPNAME_MAXLEN + 1] = "";

    if (!opts.numeric_ids) {
        if ((usr = getpwuid(uid)) == NULL) {
            warn("cannot get username for '%d'", uid);
        }
        if ((grp = getgrgid(gid)) == NULL) {
            warn("cannot get groupname for '%d'", gid);
        }
    }

    if (usr != NULL) {
        strncpy(username, usr->pw_name, USERNAME_MAXLEN + 1);
    } else {
        snprintf(username, USERNAME_MAXLEN + 1, "%u", uid);
    }

    if (grp != NULL) {
        strncpy(groupname, grp->gr_name, GROUPNAME_MAXLEN + 1);
    } else {
        snprintf(groupname, GROUPNAME_MAXLEN + 1, "%u", gid);
    }

    snprintf(owner, FILEOWNER_MAXLEN + 1, "%s %s", username, groupname);
}

int
time2str(time_t time, char *format, char *timebuf, size_t bufsize)
{
    struct tm *tm = localtime(&time);
    if (tm == NULL) {
        return -1;
    }

    return strftime(timebuf, bufsize, format, tm);
}

void
filetime(struct ls_entry *ent, char *timebuf, size_t bufsize)
{
    time_t time = ent->stat->st_mtime;
    if (opts.filetime == ATIME) {
        time = ent->stat->st_atime;
    }

    if (time2str(time, FILETIME_FORMAT, timebuf, bufsize) <= 0) {
        warn("cannot convert time for '%s'", ent->name);
    }
}

void
print_entry(struct ls_entry *ent)
{
    if (!opts.list) {
        fputs(ent->name, stdout);
        return;
    }

    char perms[10] = "---------";
    fileperms(ent->stat->st_mode, perms);

    char owner[FILEOWNER_MAXLEN + 1] = "";
    fileowner(ent->stat->st_uid, ent->stat->st_gid, owner);

    char time[24] = "";
    filetime(ent, time, sizeof(time));

    printf("%c%s %-2lu\t%s %-10lu\t%s  %s",
        filetype(ent->stat->st_mode),
        perms,
        ent->stat->st_nlink,
        owner,
        ent->stat->st_size,
        time,
        ent->name
    );
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

    struct ls_entry *ent = ents;

    while (ent != NULL) {
        if (ent->next == NULL) {
            sep = "";
        }

        print_entry(ent);

        ent = ent->next;
        fputs(sep, stdout);
    }

    fputs("\n", stdout);
}

int
main(int argc, char *argv[])
{
    opts.all = false;
    opts.list = false;
    opts.directory = false;
    opts.numeric_ids = false;
    opts.filetime = MTIME;
    opts.sort = SORT_BY_NAME;
    opts.sort_reverse = false;

    int opt;
    extern int optind;
    while ((opt = getopt(argc, argv, "+adlmnurtSU")) != -1) {
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
            case 'n':
                opts.numeric_ids = true;
                break;
            case 'u':
                opts.filetime = ATIME;
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
