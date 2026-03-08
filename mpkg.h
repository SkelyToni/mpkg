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

// Path utilities
int join_path(char *out, size_t outsz, const char *a, const char *b);
int mkdir_p(const char *path, mode_t mode);

// Config utilities
int read_config(const char *key, char *out, size_t outsz);
int read_root(char *out, size_t outsz);

// Database
int db_open(const char *path);
int db_close(void);
int db_exists(const char *name);
int db_insert(const char *name, const char *version, const char *sha256, const char *store_path);

// Commands
int install(char *mpkg_path);
int init(const char *root);

#endif