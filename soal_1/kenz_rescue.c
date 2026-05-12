#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

static char dirpath[1000];

static void get_koordinat_amba(char *buffer) {
    strcpy(buffer, "Tujuan Mas Amba: ");
    
    for (int i = 1; i <= 7; i++) {
        char fpath[1000];
        sprintf(fpath, "%s/%d.txt", dirpath, i);
        
        FILE *f = fopen(fpath, "r");
        if (!f) continue;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "KOORD: ", 7) == 0) {
                line[strcspn(line, "\r\n")] = 0;
                strcat(buffer, line + 7);
                break;
            }
        }
        fclose(f);
    }
    strcat(buffer, "\n");
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        
        char content[2048];
        get_koordinat_amba(content);
        stbuf->st_size = strlen(content);
        return 0;
    }

    int res;
    char fpath[1000];
    sprintf(fpath, "%s%s", dirpath, path);
    
    res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    
    if(strcmp(path,"/") == 0) {
        path = dirpath;
        sprintf(fpath, "%s", path);
    } else {
        sprintf(fpath, "%s%s", dirpath, path);
    }

    int res = 0;
    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;

    dp = opendir(fpath);
    if (dp == NULL) return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        res = (filler(buf, de->d_name, &st, 0));
        if(res != 0) break;
    }
    closedir(dp);

    if (strcmp(path, dirpath) == 0) { 
        filler(buf, "tujuan.txt", NULL, 0);
    }

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) return 0;

    char fpath[1000];
    if(strcmp(path,"/") == 0) {
        path = dirpath;
        sprintf(fpath, "%s", path);
    } else {
        sprintf(fpath, "%s%s", dirpath, path);
    }
    
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[2048];
        get_koordinat_amba(content);
        size_t len = strlen(content);
        
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    char fpath[1000];
    if(strcmp(path,"/") == 0) {
        path = dirpath;
        sprintf(fpath, "%s", path);
    } else {
        sprintf(fpath, "%s%s", dirpath, path);
    }

    int res = 0;
    int fd = 0;
    (void) fi;

    fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open = xmp_open,
    .read = xmp_read,
};

int main(int argc, char *argv[]) {
    umask(0);

    if (argc >= 3) {
        realpath(argv[1], dirpath); 
        argv[1] = argv[2];
        argv[2] = NULL;
        argc--;
    }

    return fuse_main(argc, argv, &xmp_oper, NULL);
}