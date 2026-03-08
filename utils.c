#define _POSIX_C_SOURCE 200809L

#include "mpkg.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sqlite3.h>
#include <dirent.h> 

// Path utils
int join_path(char *out, size_t outsz, const char *a, const char *b);
int mkdir_p(const char *path, mode_t mode);
// Config utils
int read_config(const char *key, char *out, size_t outsz);
int read_root(char *out, size_t outsz);
// DB utils
int db_open(const char *db_path);
int db_close();
int db_package_exists(const char *name);
int db_add_package(const char *name, const char *version, const char *hash, const char *install_path);
int db_remove_package(const char *name);
int db_list_packages();
int db_get_install_path(const char *name, char *out, size_t outsz);
int get_all_store_paths(char ***paths, int *count);
// Folder utils
static int rm_entry(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
int rm_rf(const char *path);

static sqlite3 *db = NULL;

int join_path(char *out, size_t outsz, const char *a, const char *b)
{
    // Safely make path string
    int n = snprintf(out, outsz, "%s/%s", a, b);
    if (n < 0 || n >= (int)outsz) 
    {
        errno = ENAMETOOLONG; 
        return 1; 
    }
    return 0;
}

int mkdir_p(const char *path, mode_t mode)
{
    // Recursive directory helper function
    if (!path || path[0] == '\0') {
        errno = EINVAL;
        return 1;
    }

    char tmp[4096];

    if (strlen(path) >= sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return 1;
    }

    strcpy(tmp, path);

    // Create every path
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, mode) != 0) {
                if (errno != EEXIST)
                    return 1;
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST)
            return 1;
    }

    return 0;
}

// Read a value for a given key from the mpkg config file
// Returns 0 on success, 1 on error
int read_config(const char *key, char *out, size_t outsz)
{
    char config_dir[4096];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg)
    {
        strncpy(config_dir, xdg, 4095);
        config_dir[4095] = '\0';
    }
    else
    {
        const char *home = getenv("HOME");
        if (!home) { fprintf(stderr, "HOME not set\n"); return 1; }
        snprintf(config_dir, 4096, "%s/.config", home);
    }

    char config_path[4096];
    snprintf(config_path, 4096, "%s/mpkg/config", config_dir);

    FILE *cfg = fopen(config_path, "r");
    if (!cfg)
    {
        fprintf(stderr, "Config not initialized, please run mpkg init\n");
        return 1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), cfg) != NULL)
    {
        line[strcspn(line, "\n")] = '\0';

        char k[256];
        char v[4096];
        if (sscanf(line, "%255[^=]=%4095s", k, v) == 2)
        {
            if (strcmp(k, key) == 0)
            {
                fclose(cfg);
                int n = snprintf(out, outsz, "%s", v);
                if (n < 0 || n >= (int)outsz)
                {
                    fprintf(stderr, "Value too long for key %s\n", key);
                    return 1;
                }
                return 0;
            }
        }
    }

    fclose(cfg);
    fprintf(stderr, "Key '%s' not found in config\n", key);
    return 1;
}

// Convenience wrapper to get the root path
int read_root(char *out, size_t outsz)
{
    return read_config("root", out, outsz);
}


int db_open(const char *db_path)
{
    if (sqlite3_open(db_path, &db) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS packages ("
        "   id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   name         TEXT NOT NULL UNIQUE,"
        "   version      TEXT NOT NULL,"
        "   hash         TEXT NOT NULL,"
        "   install_path TEXT NOT NULL,"
        "   installed_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to create table: %s\n", err);
        sqlite3_free(err);
        return 1;
    }

    return 0;
}

int db_close()
{
    if (db)
    {
        sqlite3_close(db);
        db = NULL;
    }
    return 0;
}

int db_package_exists(const char *name)
{
    if (!db)
    {
        fprintf(stderr, "Database not open\n");
        return -1;
    }

    const char *sql = "SELECT COUNT(*) FROM packages WHERE name = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    int exists = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        exists = sqlite3_column_int(stmt, 0) > 0;

    sqlite3_finalize(stmt);
    return exists;
}

int db_add_package(const char *name, const char *version, const char *hash, const char *install_path)
{
    if (!db)
    {
        fprintf(stderr, "Database not open\n");
        return 1;
    }

    const char *sql =
        "INSERT INTO packages (name, version, hash, install_path) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_text(stmt, 1, name,         -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, version,      -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hash,         -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, install_path, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "Failed to insert package: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

int db_remove_package(const char *name)
{
    if (!db)
    {
        fprintf(stderr, "Database not open\n");
        return 1;
    }

    const char *sql = "DELETE FROM packages WHERE name = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "Failed to remove package: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (sqlite3_changes(db) == 0)
    {
        fprintf(stderr, "Package '%s' not found\n", name);
        return 1;
    }

    return 0;
}

int db_list_packages()
{
    if (!db)
    {
        fprintf(stderr, "Database not open\n");
        return 1;
    }

    const char *sql =
        "SELECT name, version, hash, install_path, installed_at "
        "FROM packages ORDER BY name;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("%-20s %-10s %-12s %-40s %s\n",
           "Name", "Version", "Hash", "Install Path", "Installed At");
    printf("%-20s %-10s %-12s %-40s %s\n",
           "----", "-------", "----", "------------", "------------");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *name    = (const char *)sqlite3_column_text(stmt, 0);
        const char *version = (const char *)sqlite3_column_text(stmt, 1);
        const char *hash    = (const char *)sqlite3_column_text(stmt, 2);
        const char *path    = (const char *)sqlite3_column_text(stmt, 3);
        const char *date    = (const char *)sqlite3_column_text(stmt, 4);

        // Show only first 10 chars of hash for readability
        char short_hash[11];
        strncpy(short_hash, hash, 10);
        short_hash[10] = '\0';

        printf("%-20s %-10s %-12s %-40s %s\n", name, version, short_hash, path, date);
        found = 1;
    }

    if (!found)
        printf("No packages installed.\n");

    sqlite3_finalize(stmt);
    return 0;
}

int db_get_install_path(const char *name, char *out, size_t outsz)
{
    const char *sql = "SELECT install_path FROM packages WHERE name = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 1;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    int rc = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *path = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(out, outsz, "%s", path);
        rc = 0;
    }

    sqlite3_finalize(stmt);
    return rc;
}

int get_all_store_paths(char ***paths, int *count)
{
    char root[4096];
    if (read_root(root, sizeof(root)) != 0) return 1;

    char store[4096];
    if (join_path(store, sizeof(store), root, "store") != 0) return 1;

    DIR *dir = opendir(store);
    if (!dir)
    {
        fprintf(stderr, "Failed to open store directory\n");
        return 1;
    }

    int capacity = 16;
    *count = 0;
    *paths = malloc(capacity * sizeof(char *));
    if (!*paths) return 1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (*count >= capacity)
        {
            capacity *= 2;
            *paths = realloc(*paths, capacity * sizeof(char *));
            if (!*paths) { closedir(dir); return 1; }
        }

        char full_path[4096];
        join_path(full_path, sizeof(full_path), store, entry->d_name);
        (*paths)[*count] = strdup(full_path);
        (*count)++;
    }

    closedir(dir);
    return 0;
}

static int rm_entry(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    if (typeflag == FTW_DP)
        return rmdir(path);
    else
        return remove(path);
}

int rm_rf(const char *path)
{
    if (nftw(path, rm_entry, 16, FTW_DEPTH | FTW_PHYS) != 0)
    {
        fprintf(stderr, "Failed to remove %s\n", path);
        return 1;
    }
    return 0;
}