#include "../mpkg.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


int update(char *mpkg_path)
{
    // parse manifest to get name and version
    struct mpkg sbuffer;
    FILE *decl = fopen(mpkg_path, "r");
    if(decl == NULL)
    {
        fprintf(stderr, "Failed to open package declaration.\n");
        return -1;
    }
    parse(decl, &sbuffer);
    // check installed, check version differs
    char root[4096];
    read_root(root, 4096);
    char db_path[4096];
    if (join_path(db_path, sizeof(db_path), root, "var/db/packages.db") != 0) return 1;
    db_open(db_path);
    if (db_package_exists(sbuffer.name)!= 1)
    {
        fprintf(stderr, "Package not installed, could not update.");
        db_close();
        return -2;
    }
    db_close();
    // remove_package(name)
    printf("Removing old package version from profile....\n");
    if(remove_package(sbuffer.name) != 0)
    {
        fprintf(stderr, "Could not remove the old version.");
        return -3;

    }
    // install(mpkg_path)
    printf("Installing the new package version...\n");
    if(install(mpkg_path) != 0)
    {
        fprintf(stderr, "Could not install new version.");
        return -4;
    }
    return 0;
}