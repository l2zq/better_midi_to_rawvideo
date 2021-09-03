#include "file-load.h"
#include <errno.h>

struct file_global file;
#ifdef USE_MMAP
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
bool file_load(const char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1)
    return false;
  struct stat buf;
  if (fstat(fd, &buf) == -1 // clang-format off
  || (file.mem = mmap(NULL, file.len = buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
    int tmp_errno = errno;
    close(fd);
    errno   =   tmp_errno;
    return false; // clang-format on
  }
  close(fd);
  return true;
}
int file_free(void) {
  munmap(file.mem, file.len);
  return 0;
}
#else
#include <stdio.h>
#include <stdlib.h>
bool file_load(const char *filename) {
  int tmp_errno;
  bool ret = false;
  FILE *fp = fopen(filename, "rb");
  if (fp != NULL) {
    if (fseek(fp, 0, SEEK_END) != -1) {
      long length = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      if ((file.mem = malloc(length)) != NULL) {
        if (fread(file.mem, 1, length, fp) != length) {
          tmp_errno = errno;
          free(file.mem);
          errno = tmp_errno;
        } else {
          file.len = length;
          ret = true;
        }
      }
    }
    tmp_errno = errno;
    fclose(fp);
    errno = tmp_errno;
  }
  return ret;
}
int file_free(void) {
  free(file.mem);
  return 0;
}
#endif