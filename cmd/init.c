#define _GNU_SOURCE   

#include "../mpkg.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


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
        fprintf(stderr, "XDG_CONFIG_HOME not set, defaulting to ~/.config .\n");
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
    // PATH/profiles/bin
    if (join_path(p, sizeof(p), path, "profiles/bin") != 0) { perror("join_path profiles/bin"); return 1; }
    if (mkdir_p(p, 0755) != 0) { perror("mkdir_p profiles/bin"); return 1; }
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

    // Create the package database
    char db_path[4096];
    if (join_path(db_path, sizeof(db_path), path, "var/db/packages.db") != 0)
    {
        perror("join_path db");
        return 1;
    }
    if (db_open(db_path) != 0)
    {
        fprintf(stderr, "Failed to initialize package database\n");
        return 1;
    }
    db_close();

    // Detect shell and add profiles/bin to PATH
const char *shell = getenv("SHELL");
if (!shell)
{
    fprintf(stderr, "SHELL not set, skipping PATH setup\n");
}
    else
    {
        char rc_path[4096];
        char export_line[8192];

        // Build the export line
        snprintf(export_line, sizeof(export_line), "\n# mpkg\nexport PATH=\"%s/profiles/bin:$PATH\"\n", path);

        // Pick the right rc file
        if (strstr(shell, "zsh"))
            snprintf(rc_path, sizeof(rc_path), "%s/.zshrc", home);
        else if (strstr(shell, "bash"))
            snprintf(rc_path, sizeof(rc_path), "%s/.bashrc", home);
        else if (strstr(shell, "fish"))
            snprintf(rc_path, sizeof(rc_path), "%s/.config/fish/config.fish", home);
        else
        {
            fprintf(stderr, "Unknown shell %s, add the following to your shell rc manually:\n%s\n", shell, export_line);
            goto done;
        }

        FILE *rc = fopen(rc_path, "a");
        if (!rc)
        {
            fprintf(stderr, "Could not open %s\n", rc_path);
        }
        else
        {
            fputs(export_line, rc);
            fclose(rc);
            printf("Added profiles/bin to PATH in %s\n", rc_path);
            printf("Run 'source %s' or restart your shell\n", rc_path);
        }
    }
    done:

    printf("Initialized mpkg under %s\n", path);
    printf("Config file stored under %s/mpkg\n", configp);
    return 0;
}
