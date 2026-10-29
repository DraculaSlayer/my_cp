#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <getopt.h>
#include <pwd.h>

#define BUFFER_SIZE 4096

const char *get_name_directory(const char *path) {
    char fuck[512];

    strcpy(fuck, path);
    
    char *name = basename(fuck);

    return name;
}
const char *get_name_file(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }
    return path;
}

int verify_path(const char *path) {
    struct stat info;

    if (stat(path, &info) != 0) {
        return 0;
    }

    if (S_ISDIR(info.st_mode)) {
        return 1;
    } 
    else if (S_ISREG(info.st_mode)) {
        return 2;
    } 
    else {
        return 3;
    }
}

void print_usage(const char *progname) {
    printf("Uso: %s [OPCIONES] ORIGEN DESTINO\n", progname);
    printf("Copia archivos o directorios de forma recursiva.\n\n");
    printf("Opciones:\n");
    printf("  -h, --help      Muestra esta ayuda y sale\n");
    printf("  -r, --recursive Modo recursivo");
}

void watch_bar(int current, int total) {
    int width_bar = 25;
    float porcente = (float)current / total;
    int complet = (int)(porcente * width_bar);

    printf("\r[");
    for (int i = 0; i < width_bar; i++) {
        if (i < complet) {
            printf("=");
        } else if (i == complet) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %3d%% (%d/%d)", (int)(porcente * 100), current, total);

    fflush(stdout); 
}

int copy_origin(int src_fd, int dst_fd, long total_size, long *bytes_copy) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;

    lseek(src_fd, 0, SEEK_SET);
    lseek(dst_fd, 0, SEEK_SET);

    *bytes_copy = 0;

    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error de escritura");
            close(src_fd);
            close(dst_fd);
            return EXIT_FAILURE;
        }
        *bytes_copy += bytes_written;
        watch_bar(*bytes_copy, total_size);
    }

    return EXIT_SUCCESS;
}

char *expand_tilde(const char *path) {
    if (!path) return NULL;

    if (path[0] != '~') {
        return strdup(path);
    }

    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        } else {
            return strdup(path);
        }
    }

    if (path[1] == '\0') {
        return strdup(home);
    }

    if (path[1] == '/') {
        size_t len = strlen(home) + strlen(path + 1) + 1;
        char *expanded = malloc(len);
        if (!expanded) return NULL;
        snprintf(expanded, len, "%s%s", home, path + 1);
        return expanded;
    }

    return strdup(path);
}

int copy_files(const char *src_file, const char *dst_file) {
    struct stat st;
    if (stat(src_file, &st) < 0) {
        perror("Error al hacer stat del origen");
        return EXIT_FAILURE;
    }

    int src_fd = open(src_file, O_RDONLY);
    if (src_fd < 0) {
        perror("Error al abrir el origen");
        return EXIT_FAILURE;
    }
    
    char path_complete[512];
    if (verify_path(dst_file) == 1) {

        snprintf(path_complete, sizeof(path_complete), "%s/%s", dst_file, get_name_file(src_file));
    } 
    if (verify_path(dst_file) == 2 || verify_path(dst_file) == 0) {
        snprintf(path_complete, sizeof(path_complete), "%s", dst_file);
    }
    
    int dst_fd = open(path_complete, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("Error al abrir/crear el destino");
        close(src_fd);
        return EXIT_FAILURE;
    }
    
    ssize_t ret;
    long bytes_copy = 0;

    while (1) {
        ret = copy_file_range(src_fd, NULL, dst_fd, NULL, BUFFER_SIZE, 0);
        
        if (ret < 0) {
            copy_origin(src_fd, dst_fd, st.st_size, &bytes_copy);
            break;
        }
        
        if (ret == 0) {
            break;
        }
        
        bytes_copy += ret;
        watch_bar(bytes_copy, st.st_size);
    }

    if (fchmod(dst_fd, st.st_mode) < 0) {
        perror("Advertencia: No se pudieron replicar los permisos");
    }

    struct timespec times[2];
    times[0] = st.st_atim;
    times[1] = st.st_mtim;
    if (futimens(dst_fd, times) < 0) {
        perror("Advertencia: No se pudieron replicar las fechas");
    }

    close(src_fd);
    close(dst_fd);

    return EXIT_SUCCESS;
}

void copy_directory(const char *src_dir, const char *dst_dir) {
    DIR *dirp;
    struct dirent *dp;
    char src_path[1024];
    char dst_path[1024];
    struct stat st_dst;

    if (stat(dst_dir, &st_dst) == 0 && S_ISDIR(st_dst.st_mode)) {
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, get_name_directory(src_dir));
    } else {
        snprintf(dst_path, sizeof(dst_path), "%s", dst_dir);
    }

    if (mkdir(dst_path, 0755) < 0) {
    }

    dirp = opendir(src_dir);
    if (dirp == NULL) {
        perror("Error al abrir el directorio fuente");
        return;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, dp->d_name);
        char item_dst_path[1280];
        snprintf(item_dst_path, sizeof(item_dst_path), "%s/%s", dst_path, dp->d_name);

        struct stat st_src;
        if (lstat(src_path, &st_src) < 0) {
            perror("Error al hacer stat del elemento");
            continue;
        }

        if (S_ISDIR(st_src.st_mode)) {
            copy_directory(src_path, item_dst_path);
        } else if (S_ISREG(st_src.st_mode)) {
            copy_files(src_path, item_dst_path);
        }
    }

    closedir(dirp);
}

int main(int argc, char *argv[]) {

    static struct option const long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"recursive", no_argument, NULL, 'r'},
        {NULL,   0,           NULL,  0 }
    };

    int opt;
    bool recursive = false;

    while((opt = getopt_long(argc, argv, ":rh", long_options, NULL)) != -1)  
    {  
        switch(opt)  
        {  
            case 'r':
                recursive = true;
                break;
            case 'h':
                print_usage("cp");
                exit(EXIT_SUCCESS);
                break;
            case '?':  
                printf("unknown option: %c\n", optopt);
                print_usage("cp");
                return EXIT_FAILURE;
        }  
    }  

    if ((argc - optind) >= 2) {
        char *src = expand_tilde(argv[argc - 2]);
        char *dst = expand_tilde(argv[argc - 1]);
        
        if (recursive == true) {
            copy_directory(src, dst);
        }else {
            copy_files(src, dst);
        }
        free(src);
        free(dst);
    } else {
        fprintf(stderr, "Error: Faltan argumentos (se requieren origen y destino).\n");
        return EXIT_FAILURE;
    }
     
    return 0;

}
