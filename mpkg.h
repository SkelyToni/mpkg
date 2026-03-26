#ifndef MPKG_H
#define MPKG_H

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>

// Struct
struct mpkg
{
    char name[4096];
    char version[32];
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

// Package utils
int parse(FILE *path, struct mpkg *sbuffer);

// Database
int db_open(const char *db_path);
int db_close();
int db_package_exists(const char *name);
int db_add_package(const char *name, const char *version, const char *hash, const char *install_path);
int db_remove_package(const char *name);
int db_list_packages();
int db_get_install_path(const char *name, char *out, size_t outsz);
int db_get_all_install_paths(char ***paths, int *count);

// Commands
int init(const char *root);
int install(char *mpkg_path);
int list(void);
int help();
int remove_package(const char *name);
int gc(void);
int update(char *mpkg_path);


#endif