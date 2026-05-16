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

static const char *dirpath = "/home/hyl/kuliah/semester-2/sistem-operasi/praktikum/SISOP-4-2026-IT-005/soal_2/soal_2/encrypted_storage";
static const char KEY = 0x76;

void make_fpath(char *fpath, const char *path) {
    if (strcmp(path, "/") == 0) {
        sprintf(fpath, "%s", dirpath);
        return;
    }
    
    char temp[1024];
    sprintf(temp, "%s%s", dirpath, path);
    
    struct stat st;
    if (lstat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcpy(fpath, temp);
    } else {
        sprintf(fpath, "%s%s.enc", dirpath, path);
    }
}

void xor_cipher(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= KEY;
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;
    char fpath[1024];
    make_fpath(fpath, path);
    
    res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1024];
    make_fpath(fpath, path);
    
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

        char entry_name[1024];
        strcpy(entry_name, de->d_name);
        
        char *ext = strstr(entry_name, ".enc");
        if (ext != NULL && strlen(ext) == 4) {
            *ext = '\0';
        }

        if (filler(buf, entry_name, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[1024];
    sprintf(fpath, "%s%s", dirpath, path);
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[1024];
    sprintf(fpath, "%s%s", dirpath, path);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1024];
    sprintf(fpath, "%s%s.enc", dirpath, path);
    int res = creat(fpath, mode);
    if (res == -1) return -errno;
    fi->fh = res;
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char fpath[1024];
    make_fpath(fpath, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1024];
    make_fpath(fpath, path);
    
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    else xor_cipher(buf, res); // Dekripsi

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1024];
    make_fpath(fpath, path);
    
    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    xor_cipher(enc_buf, size); // Enkripsi

    int res = pwrite(fd, enc_buf, size, offset);
    if (res == -1) res = -errno;

    free(enc_buf);
    close(fd);
    return res;
}

static int xmp_truncate(const char *path, off_t size) {
    char fpath[1024];
    make_fpath(fpath, path);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[1024];
    make_fpath(fpath, path);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_access(const char *path, int mask) {
    char fpath[1024];
    make_fpath(fpath, path);
    int res = access(fpath, mask);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec tv[2]) {
    char fpath[1024];
    make_fpath(fpath, path);
    int res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,
    .create   = xmp_create,
    .open     = xmp_open,
    .read     = xmp_read,
    .write    = xmp_write,
    .truncate = xmp_truncate,
    .unlink   = xmp_unlink,
    .access   = xmp_access,
    .utimens  = xmp_utimens,
};

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}