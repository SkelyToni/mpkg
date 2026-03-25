#include "../mpkg.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

int remove_package(const char *name)
{
    // Open db
    char root[4096];
    if (read_root(root, sizeof(root)) != 0) return 1;

    char db_path[4096];
    if (join_path(db_path, sizeof(db_path), root, "var/db/packages.db") != 0)
    {
        fprintf(stderr, "db path too long\n");
        return 1;
    }
    if (db_open(db_path) != 0) return 1;

    // Check package exists
    if (db_package_exists(name) == 0)
    {
        fprintf(stderr, "Package '%s' is not installed\n", name);
        db_close();
        return 1;
    }

    // Get install path from db
    char install_path[4096];
    if (db_get_install_path(name, install_path, sizeof(install_path)) != 0)
    {
        fprintf(stderr, "Failed to get install path for '%s'\n", name);
        db_close();
        return 1;
    }
    // Build profiles bin path
    char profiles_bin[4096];
    if (join_path(profiles_bin, sizeof(profiles_bin), root, "profiles/bin") != 0)
    {
        fprintf(stderr, "profiles/bin path too long\n");
        db_close();
        return 1;
    }

    DIR *bin_dir = opendir(profiles_bin);
    if (bin_dir)
    {
        struct dirent *entry;
        while ((entry = readdir(bin_dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char dest[4096];
            if (join_path(dest, sizeof(dest), profiles_bin, entry->d_name) != 0) continue;

            // Read where the symlink points
            char link_target[4096];
            ssize_t len = readlink(dest, link_target, sizeof(link_target) - 1);
            if (len < 0) continue;
            link_target[len] = '\0';

            // Remove if it points into this package's store path
            if (strncmp(link_target, install_path, strlen(install_path)) == 0)
            {
                if (unlink(dest) != 0)
                    fprintf(stderr, "Warning: failed to remove symlink %s\n", entry->d_name);
            }
        }
        closedir(bin_dir);
    }

    // Remove from db
    if (db_remove_package(name) != 0)
    {
        fprintf(stderr, "Failed to remove package from database\n");
        db_close();
        return 1;
    }

    db_close();
    printf("Removed '%s' from profile\n", name);
    printf("Run 'mpkg gc' to free disk space\n");
    return 0;
}
