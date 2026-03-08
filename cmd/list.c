#include "../mpkg.h"
#include <stdio.h>

int list()
{
    char root[4096];
    if (read_root(root, sizeof(root)) != 0) return 1;

    char db_path[4096];
    if (join_path(db_path, sizeof(db_path), root, "var/db/packages.db") != 0)
    {
        fprintf(stderr, "db path too long\n");
        return 1;
    }

    if (db_open(db_path) != 0) return 1;
    int rc = db_list_packages();
    db_close();
    return rc;
}