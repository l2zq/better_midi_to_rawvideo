#include "mmap-load.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
struct file_global file;
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