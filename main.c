#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096

const char* get_name(const char *path) {
    char fuck[500];

    strcpy(fuck, path);
    
    char *name = basename(fuck);

    return name;
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

void copy_origin(int src_fd, int dst_fd, long total_size, long *bytes_copy) {
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
        }
        *bytes_copy += bytes_written;
        watch_bar(*bytes_copy, total_size);
    }
}

int copy_files(const char *src_file, const char *dst_file) {
    struct stat st;
    if (stat(src_file, &st) < 0) {
        perror("Error al hacer stat del origen");
        return 1;
    }

    int src_fd = open(src_file, O_RDONLY);
    if (src_fd < 0) {
        perror("Error al abrir el origen");
        return 1;
    }
    
    char path_complete[512];
    if (verify_path(dst_file) == 1) {

        snprintf(path_complete, sizeof(path_complete), "%s/%s", dst_file, get_name(src_file));
    } 
    if (verify_path(dst_file) == 2 || verify_path(dst_file) == 0) {
        snprintf(path_complete, sizeof(path_complete), "%s", dst_file);
    }

    int dst_fd = open(path_complete, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("Error al abrir/crear el destino");
        close(src_fd);
        return 1;
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

    return 0;
}

void copy_directory(const char *src_dir, const char *dst_dir) {
    DIR *dirp;
    struct dirent *dp;
    char src_path[1024];
    char dst_path[1024];
    struct stat st_dst;

    if (stat(dst_dir, &st_dst) == 0 && S_ISDIR(st_dst.st_mode)) {
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, get_name(src_dir));
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
        char item_dst_path[1024];
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
    int opt;
    char *path_src[512];
    char *path_dst[512];
    bool recursive = false;

    while((opt = getopt(argc, argv, ":r")) != -1)  
    {  
        switch(opt)  
        {  
            case 'r':
                recursive = true;
                break;
            case '?':  
                printf("unknown option: %c\n", optopt);
                return 1;
        }  
    }  

    if ((argc - optind) >= 2) {
        char *src = argv[argc - 2];
        char *dst = argv[argc - 1];
        
        if (recursive == true) {
            copy_directory(src, dst);
        }else {
            copy_files(src, dst);
        }
    } else {
        fprintf(stderr, "Error: Faltan argumentos (se requieren origen y destino).\n");
        return 1;
    }
     
    return 0;

}
