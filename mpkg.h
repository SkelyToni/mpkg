#ifndef MPKG_H
#define MPKG_H

#include <stddef.h>
#include <sys/types.h>

// Struct
struct mpkg
{
    char name[4096];
    char version[4];
    char url[4096];
    char sha256[65];
};

// Path and file utils
int join_path(char *out, size_t outsz, const char *a, const char *b);
int mkdir_p(const char *path, mode_t mode);
int rm_rf(const char *path);

// Config utilities
int read_config(const char *key, char *out, size_t outsz);
int read_root(char *out, size_t outsz);

// Database
int db_open(const char *db_path);
int db_close();
int db_package_exists(const char *name);
int db_add_package(const char *name, const char *version, const char *hash, const char *install_path);
int db_remove_package(const char *name);
int db_list_packages();
int get_all_store_paths(char ***paths, int *count);
int db_get_install_path(const char *name, char *out, size_t outsz);

// Commands
int init(const char *root);
int install(char *mpkg_path);
int list(void);
int help();
int remove_package(const char *name);
int gc(void);


#endif