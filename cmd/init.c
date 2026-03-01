#define _POSIX_C_SOURCE 200809L

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int join_path(char *out, size_t outsz, const char *a, const char *b)
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

static int mkdir_p(const char *path, mode_t mode)
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


int init(const char *root)
{
    // Path for root dir
    char path[4096];
    // Path for the config dir
    char configp[4096];
    // path to home dir
    const char *home = getenv("HOME");
    if (!home) 
    {
        fprintf(stderr, "HOME not set\n");
        return 1;
    } 

    // Path for config dir
    const char *config = getenv("XDG_CONFIG_HOME");
    // Fallback to /home
    if (!config || config == NULL) 
    {
        fprintf(stderr, "XDG_CONFIG_HOME not set, defaulting to ~/.conifg .\n");
        if (join_path(configp, sizeof(configp), home, ".config") != 0) { perror("join_path base config"); return 1; }
    }
    else
    {
        int n = snprintf(configp, sizeof(configp), "%s", config);
        if (n < 0 || n >= (int)sizeof(configp)) {
        fprintf(stderr, "Config path too long\n");
        return 1;
        }
    }

    // If no root is specified create ~/.mpkg
    if (root == NULL)
    {    
        if(join_path(path, sizeof(path), home, ".mpkg") != 0)
        {
            fprintf(stderr, "Root path too long\n");
            return 1;
        }
    }
    // If root is specified create the specified location
    else
    {
        int n = snprintf(path, sizeof(path), "%s", root);
        if (n < 0 || n >= (int)sizeof(path)) 
        {
            fprintf(stderr, "Root path too long\n");
            return 1;
        }
    }
    // Create root dir
    if (mkdir_p(path, 0755) != 0) 
    {
        perror("mkdir_p root");
        return 1;
    }

    // Create mpkg subdirectories
    char p[4096]; // Buffer for root path 
    char c[4096]; // Buffer for config path

    // PATH/store
    if (join_path(p, sizeof(p), path, "store") != 0) { perror("join_path store"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p store"); return 1; }
    // PATH/profiles
    if (join_path(p, sizeof(p), path, "profiles") != 0) { perror("join_path profiles"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p profiles"); return 1; }
    // PATH/var
    if (join_path(p, sizeof(p), path, "var") != 0) { perror("join_path var"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p var"); return 1; }
    // PATH/var/db
    if (join_path(p, sizeof(p), path, "var/db") != 0) { perror("join_path db"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p db"); return 1; }
    // PATH/var/log
    if (join_path(p, sizeof(p), path, "var/log") != 0) { perror("join_path log"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p log"); return 1; }
    // Config folder
    if (join_path(c, sizeof(c), configp, "mpkg") != 0) { perror("join_path config"); return 1; }
    if (mkdir_p(c, 0755) != 0) { perror("mkdir_p config"); return 1; }

    // Create config file with correct root pointer
    char cfg[4096];
    if (join_path(cfg, sizeof(cfg), c, "config") != 0) { perror("join_path config file"); return 1; }

    FILE *f = fopen(cfg, "w");
    if (!f) {
        perror("fopen cfgfile");
        return 1;
    }
    fprintf(f, "root=%s\n", path);
    fclose(f);


    printf("Initialized mpkg under %s", path);
    printf("Config file stored under %s/mpkg ", configp);
    return 0;
}