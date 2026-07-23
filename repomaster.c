#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define VERSION "2.0.0-automated-dispatcher"
#define AUTHOR "admin-ka"
#define REPO_BASE "/workspace/hf"

void print_help() {
    printf("Hive Forge Repository Lifecycle Manager v%s (%s)\n", VERSION, AUTHOR);
    printf("Usage: repomaster dispatch <board_name> <pkg_name>[:version]\n");
}

int create_dir_if_missing(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == 0) {
            return 1;
        }
    }
    return 0;
}

void bootstrap_board_repository(const char *board_name) {
    char *path = (char *)malloc(1024);
    sprintf(path, "mkdir -p %s/repository/%s/packages", REPO_BASE, board_name);
    system(path);
    free(path);
}

int validate_package_in_release(const char *board_name, const char *pkg_name, const char *req_version) {
    char *path = (char *)malloc(1024);
    char *line = (char *)malloc(2048);
    char *expected_match = (char *)malloc(1024);
    int found = 0;

    if (strlen(req_version) > 0) {
        sprintf(expected_match, "%s:%s", pkg_name, req_version);
    } else {
        sprintf(expected_match, "%s:", pkg_name);
    }

    sprintf(path, "%s/boards/%s/system.conf", REPO_BASE, board_name);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, 2048, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strcmp(line, expected_match) == 0 || (strlen(req_version) == 0 && strncmp(line, expected_match, strlen(expected_match)) == 0)) {
                printf("[+] REPOMASTER: Found '%s' in Core System release matrix.\n", pkg_name);
                found = 1; break;
            }
        }
        fclose(f);
    }

    if (!found) {
        sprintf(path, "%s/boards/%s/apps.conf", REPO_BASE, board_name);
        f = fopen(path, "r");
        if (f) {
            while (fgets(line, 2048, f)) {
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, expected_match) == 0 || (strlen(req_version) == 0 && strncmp(line, expected_match, strlen(expected_match)) == 0)) {
                    printf("[+] REPOMASTER: Found '%s' in User Applications release matrix.\n", pkg_name);
                    found = 1; break;
                }
            }
            fclose(f);
        }
    }
    
    free(path); free(line); free(expected_match);
    return found;
}

void extract_build_flags(const char *board_name, char *out_cflags, char *out_ldflags) {
    char *path = (char *)malloc(1024);
    char *line = (char *)malloc(2048);

    strcpy(out_cflags, "-O2");
    strcpy(out_ldflags, "");

    sprintf(path, "%s/boards/%s/build.conf", REPO_BASE, board_name);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, 2048, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strncmp(line, "SYS_CFLAGS=", 11) == 0) {
                sscanf(line, "SYS_CFLAGS=\"%511[^\"]\"", out_cflags);
            }
            if (strncmp(line, "SYS_LDFLAGS=", 12) == 0) {
                sscanf(line, "SYS_LDFLAGS=\"%511[^\"]\"", out_ldflags);
            }
        }
        fclose(f);
    }
    free(path); free(line);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || strcmp(*(argv + 1), "dispatch") != 0) {
        print_help();
        return 1;
    }

    const char *board_name = *(argv + 2);
    const char *raw_atom = *(argv + 3);
    
    char *pkg_name = (char *)malloc(512);
    char *req_version = (char *)malloc(512);
    memset(pkg_name, 0, 512); memset(req_version, 0, 512);
    
    char *colon = strchr(raw_atom, ':');
    if (colon) {
        strncpy(pkg_name, raw_atom, colon - raw_atom);
        strcpy(req_version, colon + 1);
    } else {
        strcpy(pkg_name, raw_atom);
    }

    bootstrap_board_repository(board_name);

    if (!validate_package_in_release(board_name, pkg_name, req_version)) {
        printf("[-] REPOMASTER SECURITY BLOCK: Target '%s' is rejected!\n", raw_atom);
        return 1;
    }

    char *target_cflags = (char *)malloc(1024);
    char *target_ldflags = (char *)malloc(1024);
    char *build_cmd_buffer = (char *)malloc(8192);
    
    extract_build_flags(board_name, target_cflags, target_ldflags);
    printf("[*] REPOMASTER: CFLAGS -> %s\n", target_cflags);
    printf("[*] REPOMASTER: LDFLAGS -> %s\n", target_ldflags);

    printf("[*] REPOMASTER: Launching automated build factory pipeline...\n");
    sprintf(build_cmd_buffer, "/workspace/hf/buildmaster/buildmaster build %s %s \"%s\" \"%s\"", board_name, pkg_name, target_cflags, target_ldflags);
    system(build_cmd_buffer);
    
    free(pkg_name); free(req_version); free(target_cflags); free(target_ldflags); free(build_cmd_buffer);
    return 0;
}
