#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#define BUFFER_SIZE 4096

const char* get_file_name(const char *path) {
    const char *name = strrchr(path, '/');
    
    return name ? name + 1 : path;
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

void copy_origin(int src_fd, int dst_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;

    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error de escritura");
            close(src_fd);
            close(dst_fd);
        }
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

        snprintf(path_complete, sizeof(path_complete), "%s/%s", dst_file, get_file_name(src_file));
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

    while (1) {
        ret = copy_file_range(src_fd, NULL, dst_fd, NULL, BUFFER_SIZE, 0);
        
        if (ret < 0) {
            copy_origin(src_fd, dst_fd);
            break;
        }
        
        if (ret == 0) {
            break;
        }
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
}

int main(int argc, char *argv[]) {
    int opt;
    
    while((opt = getopt(argc, argv, ":r")) != -1)  
    {  
        switch(opt)  
        {  
            case 'r':  
                printf("option: %c\n", opt);  
                break;  
            case ':':  
                printf("option needs a value\n");  
                break;  
            case '?':  
                printf("unknown option: %c\n", optopt);
                break;  
        }  
    }  
      
    for(; optind < argc; optind++){     
        printf("extra arguments: %s\n", argv[optind]);  
    }   
    return 0;
}
