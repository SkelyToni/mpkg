#include "../mpkg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

int gc()
{
    // Get root and store paths
    char root[4096];
    if (read_root(root, sizeof(root)) != 0) return 1;
    char store[4096];
    if (join_path(store, sizeof(store), root, "store") != 0) return 1;

    // Open the database
    char db_path[4096];
    if (join_path(db_path, sizeof(db_path), root, "var/db/packages.db") != 0) return 1;
    if (db_open(db_path) != 0) return 1;

    // Get the database paths
    char **paths = NULL;
    int count = 0;
    if(db_get_all_install_paths(&paths, &count) != 0)
    {
        fprintf(stderr, "Failed to get a list of all store paths of the profile.\n");
        db_close();
        return 1;
    }

    // Open the store dir
   DIR *dir = opendir(store);
    if (!dir) { db_close(); return 1; }

    // Cycle trough all the subdirectories
    struct dirent *entry;
    // Buffer for current dir
    char dbuf[4096];
    while ((entry = readdir(dir)) != NULL)
    {
        int found = 0;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // entry->d_name is the name of each item
        // The full path of the current store item
        if (join_path(dbuf, sizeof(dbuf), store, entry->d_name) != 0) { db_close(); return 1; }
        for(int i =0; i<count;i++)
        {
            if(strcmp(dbuf, paths[i]) == 0)
            {
                found++;
            }
        }
        if (!found)
        {
            // Delete the subdirectory
            printf("Removing %s\n", dbuf);
            if(rm_rf(dbuf) != 0)
            {
                fprintf(stderr, "Failed to remove subdirectory %s.\n", entry->d_name);
                closedir(dir);
                for (int i = 0; i < count; i++) free(paths[i]);
                free(paths);
                db_close();
                return 1;
            }
        }
    }  
    for (int i = 0; i < count; i++) free(paths[i]);
    free(paths);    
    closedir(dir);
    db_close();
    return 0;
}