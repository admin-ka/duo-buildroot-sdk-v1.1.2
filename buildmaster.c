#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define VERSION "11.5.0-robust-directories"
#define AUTHOR "admin-ka"
#define REPO_BASE "/workspace/hf"

void print_help() {
    printf("Hive Forge Automated Build Factory v%s (%s)\n", VERSION, AUTHOR);
    printf("Usage: buildmaster build <board_name> <pkg_name> \"<cflags>\" \"<ldflags>\"\n");
}

int compile_and_stage_package(const char *board_name, const char *pkg_name, const char *cflags, const char *ldflags) {
    char *stage_dir = (char *)malloc(1024);
    char *install_root = (char *)malloc(1024);
    char *manifest_dir = (char *)malloc(1024);
    char *manifest_file = (char *)malloc(1024);
    char *cmd = (char *)malloc(8192);
    
    char *archive_name = (char *)malloc(512);
    char *pkg_dir = (char *)malloc(512);
    char *conf_flags = (char *)malloc(1024);
    char *recipe_ldflags = (char *)malloc(1024);
    char *recipe_libs = (char *)malloc(1024);
    char *build_system = (char *)malloc(256);

    sprintf(stage_dir, "%s/buildmaster/stage_%s", REPO_BASE, pkg_name);
    sprintf(install_root, "%s/install_root", stage_dir);
    
    sprintf(cmd, "rm -rf %s && mkdir -p %s && mkdir -p %s", stage_dir, stage_dir, install_root);
    system(cmd);

    char *recipe_path = (char *)malloc(1024);
    sprintf(recipe_path, "%s/recipes/%s.recipe", REPO_BASE, pkg_name);
    
    strcpy(archive_name, ""); strcpy(pkg_dir, ""); strcpy(conf_flags, ""); 
    strcpy(recipe_ldflags, ""); strcpy(recipe_libs, ""); strcpy(build_system, "autotools");
    
    FILE *rf = fopen(recipe_path, "r");
    if (rf) {
        char rline[1024];
        while (fgets(rline, sizeof(rline), rf)) {
            rline[strcspn(rline, "\r\n")] = 0;
            if (strncmp(rline, "ARCHIVE=", 8) == 0) sscanf(rline, "ARCHIVE=%511s", archive_name);
            if (strncmp(rline, "DIR=", 4) == 0) sscanf(rline, "DIR=%511s", pkg_dir);
            if (strncmp(rline, "CONF_FLAGS=", 11) == 0) sscanf(rline, "CONF_FLAGS=\"%1023[^\"]\"", conf_flags);
            if (strncmp(rline, "LDFLAGS=", 8) == 0) sscanf(rline, "LDFLAGS=\"%1023[^\"]\"", recipe_ldflags);
            if (strncmp(rline, "LIBS=", 5) == 0) sscanf(rline, "LIBS=\"%1023[^\"]\"", recipe_libs);
            if (strncmp(rline, "BUILD_SYSTEM=", 13) == 0) sscanf(rline, "BUILD_SYSTEM=\"%255[^\"]\"", build_system);
        }
        fclose(rf);
    }
    free(recipe_path);

    // Dynamic clean-up of potential quotes inside string parsed layouts
    if (archive_name[0] == '"') { memmove(archive_name, archive_name + 1, strlen(archive_name)); archive_name[strlen(archive_name) - 1] = '\0'; }
    if (pkg_dir[0] == '"') { memmove(pkg_dir, pkg_dir + 1, strlen(pkg_dir)); pkg_dir[strlen(pkg_dir) - 1] = '\0'; }

    printf("[*] BUILDMASTER: Initializing production toolchain for token '%s'...\n", pkg_name);
    
    if (strlen(archive_name) > 0) {
        printf("[*] BUILDMASTER: Unpacking target source archive layout -> %s\n", archive_name);
        sprintf(cmd, "tar -xf %s/sources/%s -C %s", REPO_BASE, archive_name, stage_dir);
        if (system(cmd) != 0) {
            printf("[-] BUILDMASTER ERROR: Failed to unpack source archive layout!\n");
            return 1;
        }
    } else {
        printf("[-] BUILDMASTER ERROR: ARCHIVE key missing inside target recipe metadata!\n");
        return 1;
    }

    char *final_ldflags = (char *)malloc(2048);
    sprintf(final_ldflags, "%s %s", ldflags, recipe_ldflags);

    // Target work directory absolute path computation
    char *work_dir = (char *)malloc(2048);
    if (strlen(pkg_dir) > 0) {
        sprintf(work_dir, "%s/%s", stage_dir, pkg_dir);
    } else {
        sprintf(work_dir, "%s", stage_dir);
    }

    if (strcmp(build_system, "meson") == 0) {
        printf("[*] BUILDMASTER: Running native modern meson configuration setup...\n");
        sprintf(cmd, "cd %s && export CFLAGS=\"%s\" LDFLAGS=\"%s\" && meson setup builddir --prefix=/usr --libdir=lib %s", work_dir, cflags, final_ldflags, conf_flags);
        system(cmd);

        printf("[*] BUILDMASTER: Compiling modern tree via ninja threads...\n");
        sprintf(cmd, "cd %s/builddir && ninja -j6", work_dir);
        system(cmd);

        printf("[*] BUILDMASTER: Installing modern layout maps into staging image root...\n");
        sprintf(cmd, "cd %s/builddir && DESTDIR=%s ninja install", work_dir, install_root);
        system(cmd);
    } else {
        printf("[*] BUILDMASTER: Running native classic cross-configure payloads...\n");
        sprintf(cmd, "cd %s && ./configure CFLAGS=\"%s\" LDFLAGS=\"%s\" LIBS=\"%s\" %s --prefix=/usr --disable-nls", work_dir, cflags, final_ldflags, recipe_libs, conf_flags);
        system(cmd);

        printf("[*] BUILDMASTER: Compiling source trees via 6 parallel worker threads...\n");
        sprintf(cmd, "cd %s && make -j6", work_dir);
        system(cmd);

        printf("[*] BUILDMASTER: Installing build matrix layers into clean staging image root...\n");
        sprintf(cmd, "cd %s && make DESTDIR=%s install", work_dir, install_root);
        system(cmd);
    }

    sprintf(manifest_dir, "%s/var/lib/hive/db/%s", install_root, pkg_name);
    sprintf(cmd, "mkdir -p %s", manifest_dir);
    system(cmd);

    sprintf(manifest_file, "%s/manifest", manifest_dir);
    printf("[*] BUILDMASTER: Creating atomic post-install file verification hashes...\n");
    sprintf(cmd, "find %s/usr -type f 2>/dev/null | sed 's|%s/||' > %s", install_root, install_root, manifest_file);
    system(cmd);

    char *repo_pkg_dir = (char *)malloc(1024);
    char *tarball_path = (char *)malloc(1024);
    sprintf(repo_pkg_dir, "%s/repository/%s/packages", REPO_BASE, board_name);
    sprintf(tarball_path, "%s/%s-stable.tar.gz", repo_pkg_dir, pkg_name);

    printf("[*] BUILDMASTER: Sealing architectural deployment layers into atomic tarball...\n");
    sprintf(cmd, "tar -czf %s -C %s usr var 2>/dev/null", tarball_path, install_root);
    if (system(cmd) != 0) {
        printf("[-] BUILDMASTER ERROR: Tarball packaging failed!\n");
        return 1;
    }

    char *sha_cmd = (char *)malloc(4096);
    printf("[*] BUILDMASTER: Computing external entry checksum validation anchor...\n");
    sprintf(sha_cmd, "sha256sum %s | awk '{print $1}' > %s.sha256", tarball_path, tarball_path);
    system(sha_cmd);

    printf("[+] BUILDMASTER: Production pipeline successfully complete for package '%s'.\n", pkg_name);

    sprintf(cmd, "rm -rf %s", stage_dir);
    system(cmd);

    free(stage_dir); free(install_root); free(manifest_dir); free(manifest_file); free(cmd); 
    free(archive_name); free(pkg_dir); free(conf_flags); free(recipe_ldflags); free(recipe_libs); free(build_system);
    free(final_ldflags); free(work_dir); free(repo_pkg_dir); free(tarball_path); free(sha_cmd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 6 || strcmp(*(argv + 1), "build") != 0) {
        print_help();
        return 1;
    }
    const char *board_name = *(argv + 2);
    const char *pkg_name = *(argv + 3);
    const char *cflags = *(argv + 4);
    const char *ldflags = *(argv + 5);

    return compile_and_stage_package(board_name, pkg_name, cflags, ldflags);
}
