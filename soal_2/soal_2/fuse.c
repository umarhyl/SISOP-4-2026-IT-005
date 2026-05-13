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
static const char KEY = 0x76;

static void get_enc_path(char *fpath, const char *path) {
    char temp[1000];
    if (strcmp(path, "/") == 0) {
        sprintf(temp, "%s", dirpath);
        strcpy(fpath, temp);
        return;
    }

    sprintf(temp, "%s%s", dirpath, path);
    
    struct stat st;
    if (lstat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcpy(fpath, temp);
    } else {
        sprintf(fpath, "%s%s.enc", dirpath, path);
    }
}

static void xor_cipher(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] ^= KEY;
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    char fpath[1000];
    get_enc_path(fpath, path);
    
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    get_enc_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char filename[256];
        strcpy(filename, de->d_name);

        char *ext = strstr(filename, ".enc");
        if (ext != NULL && strlen(ext) == 4) {
            *ext = '\0'; 
        }

        if (filler(buf, filename, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[1000];
    sprintf(fpath, "%s%s", dirpath, path);
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[1000];
    sprintf(fpath, "%s%s", dirpath, path);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = creat(fpath, mode);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    get_enc_path(fpath, path);
    
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        res = -errno;
    } else {
        xor_cipher(buf, res);
    }

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    get_enc_path(fpath, path);

    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    xor_cipher(enc_buf, size);

    int res = pwrite(fd, enc_buf, size, offset);
    if (res == -1) res = -errno;

    free(enc_buf);
    close(fd);
    return res;
}

static int xmp_truncate(const char *path, off_t size) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_access(const char *path, int mask) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = access(fpath, mask);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2]) {
    char fpath[1000];
    get_enc_path(fpath, path);
    int res = utimensat(0, fpath, ts, AT_SYMLINK_NOFOLLOW);
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
    if (argc >= 3) {
        realpath(argv[1], dirpath);
        argv[1] = argv[2];
        argv[2] = NULL;
        argc--;
    }
    return fuse_main(argc, argv, &xmp_oper, NULL);
}