#include "pkgsmaster.h"
#include <sys/stat.h>

void pkgsmaster_check(void) {
    DIR *dir;
    struct dirent *entry;
    char manifest_path[MAX_PATH];
    char raw_path[MAX_PATH];
    char absolute_path[MAX_PATH];
    FILE *fp;
    int broken_files = 0;

    dir = opendir(HIVEDB_PATH);
    if (dir == NULL) {
        perror("[-] Error opening DB directory for integrity check");
        return;
    }

    printf("[*] Running system integrity check...\n");

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(manifest_path, sizeof(manifest_path), "%s%s/manifest", HIVEDB_PATH, entry->d_name);
        fp = fopen(manifest_path, "r");
        if (fp == NULL) {
            continue;
        }

        while (fgets(raw_path, sizeof(raw_path), fp)) {
            raw_path[strcspn(raw_path, "\n")] = 0;
            if (strlen(raw_path) == 0) continue;

            /* Standardize path: Ensure it is absolute by adding leading slash if missing */
            if (raw_path[0] != '/') {
                snprintf(absolute_path, sizeof(absolute_path), "/%s", raw_path);
            } else {
                strncpy(absolute_path, raw_path, MAX_PATH);
            }

            /* Check if the file physically exists on the rootfs */
            if (access(absolute_path, F_OK) == -1) {
                printf("[!] Broken package '%s': Missing file -> %s\n", entry->d_name, absolute_path);
                broken_files++;
            }
        }
        fclose(fp);
    }
    closedir(dir);

    if (broken_files == 0) {
        printf("[+] Integrity check completed. All files are intact.\n");
    } else {
        printf("[-] Integrity check failed. Total missing files: %d\n", broken_files);
    }
}
