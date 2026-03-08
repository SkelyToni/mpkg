#include "../mpkg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <archive.h>
#include <archive_entry.h>


// Helper functions to install
int parse(FILE *path, struct mpkg *sbuffer);
int fetch(char *url, FILE **archive);
int checksum(FILE *archive, char *expected);
int store(char *name, char *version, char *sha256sum, char *store_path);
int uncompress(FILE *archive, char *store_path);
int profile(char *store_path);
int db(char *name, char *version, char *hash, char *root_path);

int install(char *mpkg_path)
{
    // Open and .mpkg file
    FILE *path = fopen(mpkg_path, "r");
    if(path == NULL)
    {
        fprintf(stderr, "Unable to locate file\n");
        return 1;
    }
    // Struct buffer for parsing
    struct mpkg sbuffer;

    // Parse the file
    parse(path, &sbuffer);
    // Return if package already installed
    if(db_exists(sbuffer.name) == 0)
    {
        fprintf(stderr, "Package already installed.");
        return 2;
    }
    // Fetch the compressed file
    FILE *archive = NULL;
    if (fetch(sbuffer.url, &archive) != 0)
    {
        fprintf(stderr, "Failed to fetch.");
        return 3;
    }
    // Compare checksums
    if (checksum(archive, sbuffer.sha256) != 0)
    {
        fprintf(stderr, "Checksums don't match.");
        return 4;
    }
    fclose(archive);
    // Create store path
    char store_path[4096];
    if(store(sbuffer.name, sbuffer.version, sbuffer.sha256, store_path) != 0)
    {
        fprintf(stderr, "Unable to create store path.");
        return 5;
    }
    // Uncompress the archive
    if (uncompress(archive, store_path) != 0)
    {
        fprintf(stderr, "Unable to uncompress the archive.");
        return 6; 
    }
    // Syslink the profile 
    if (profile(store_path) != 0)
    {
        fprintf(stderr, "Unable to syslink the profile.");
        return 7; 
    }
    // Update DB
    if (db(sbuffer.name, sbuffer.version, sbuffer.sha256, store_path) != 0)
    {
        fprintf(stderr, "Unable to update database.");
        return 8;
    }
    return 0;
}

int parse(FILE *path, struct mpkg *sbuffer)
{
    char buffer[8260];
    // Type buffer
    char tbuffer[15];
    // Line counter
    int c = 0;
    while(fgets(buffer, 8260, path) != NULL)
    {
        sscanf(buffer, "%15[^=]", tbuffer);
        if (strcmp(tbuffer, "name") == 0)
        {
            sscanf(buffer, "%*15[^=]=%s", sbuffer->name);
        }
        else if (strcmp(tbuffer, "version") == 0)
        {
            sscanf(buffer, "%*15[^=]=%s", sbuffer->version);
        }
        else if (strcmp(tbuffer, "url") == 0)
        {
            sscanf(buffer, "%*15[^=]=%s", sbuffer->url);
        }
        else if (strcmp(tbuffer, "sha256sum") == 0)
        {
            sscanf(buffer, "%*15[^=]=%s", sbuffer->sha256);
        }
        c++;
    }
    fclose(path);
    return 0;
}

int fetch(char *url, FILE **archive)
{
    // Create a temp file to write to
    *archive = tmpfile();
    if (*archive == NULL)
    {
        fprintf(stderr, "Failed to create temp file.\n");
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        fprintf(stderr, "Failed to init curl.\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, *archive);  // write directly to FILE*
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);   // follow redirects

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        fprintf(stderr, "Fetch failed: %s\n", curl_easy_strerror(res));
        return 1;
    }

    // Seek back to start so callers can read from the beginning
    rewind(*archive);
    return 0;
}
int checksum(FILE *archive, char *expected)
{
    // Get the file size 
    fseek(archive, 0, SEEK_END);  // seek to end
    size_t size = ftell(archive); // position = file size in bytes
    rewind(archive);              // seek back to start before reading
    // Compute checksum
    char checksum[65];
    unsigned char *buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    fread(buf, 1, size, archive);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(buf, size, hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(checksum + (i * 2), "%02x", hash[i]);
    }
    checksum[64] = '\0';
    free(buf);
    if(strcmp(expected, checksum) == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int store(char *name, char *version, char *sha256sum, char *store_path)
    {
        char root_path[4096];
        if (read_root(root_path, sizeof(root_path)) != 0) return 1;
        // Create full store path name
        char full_store_path[4096];
        int a = snprintf(full_store_path, 4096, "%s/%s/%s-%s-%s", root_path, "store", sha256sum, name, version);
        if (a < 0 || a >= 4096) 
        {
            fprintf(stderr, "Store path name too long.");
            return 1;
        }   
        if (mkdir(full_store_path, 0755) != 0) 
        {
            fprintf(stderr, "Failed to create store path");
            return 1;
        }
        strcpy(store_path, full_store_path);
        return 0;    
}

// CC wrote 
int uncompress(FILE *archive, char *store_path)
{
    struct archive *a = archive_read_new();
    struct archive *out = archive_write_disk_new();

    // Support all common formats and filters automatically
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    // Preserve permissions and timestamps when extracting
    archive_write_disk_set_options(out,
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_TIME);

    // Open archive from FILE*
    int fd = fileno(archive);
    if (archive_read_open_fd(a, fd, 4096) != ARCHIVE_OK)
    {
        fprintf(stderr, "Failed to open archive: %s\n", archive_error_string(a));
        return 1;
    }

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        // Redirect extraction to store path
        char full_path[4096];
        snprintf(full_path, 4096, "%s/%s", store_path, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, full_path);

        if (archive_write_header(out, entry) != ARCHIVE_OK)
        {
            fprintf(stderr, "Failed to write header: %s\n", archive_error_string(out));
            return 1;
        }

        // Copy data blocks
        const void *buf;
        size_t size;
        la_int64_t offset;
        while (archive_read_data_block(a, &buf, &size, &offset) == ARCHIVE_OK)
        {
            archive_write_data_block(out, buf, size, offset);
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(out);
    archive_write_free(out);
    return 0;
}

int profile(char *store_path)
{
    // Get root path from config
    char root[4096];
    if (read_root(root, sizeof(root)) != 0) return 1;

    // Build profiles/bin path
    char profiles_bin[4096];
    if (join_path(profiles_bin, sizeof(profiles_bin), root, "profiles/bin") != 0)
    {
        fprintf(stderr, "profiles/bin path too long\n");
        return 1;
    }

    // Build store bin path
    char store_bin[4096];
    if (join_path(store_bin, sizeof(store_bin), store_path, "bin") != 0)
    {
        fprintf(stderr, "store bin path too long\n");
        return 1;
    }

    // Open store bin dir
    DIR *bin_dir = opendir(store_bin);
    if (!bin_dir)
    {
        fprintf(stderr, "No bin directory found in package\n");
        return 1;
    }

    // Walk and symlink each binary
    struct dirent *entry;
    while ((entry = readdir(bin_dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char src[4096];
        char dest[4096];
        if (join_path(src,  sizeof(src),  store_bin,    entry->d_name) != 0) continue;
        if (join_path(dest, sizeof(dest), profiles_bin, entry->d_name) != 0) continue;

        if (symlink(src, dest) != 0)
        {
            fprintf(stderr, "Failed to symlink %s\n", entry->d_name);
            closedir(bin_dir);
            return 1;
        }
    }

    closedir(bin_dir);
    return 0;
}

int db(char *name, char *version, char *hash, char *root_path)
{
    if(db_add_package(name, version, hash, root_path) == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
    
}