#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "fileoperate.h"

namespace shannon {

void GetFilesize(char *filename, size_t *file_size)
{
  struct stat statbuf;
  int ret = 0;

  ret = stat(filename, &statbuf);
  if (ret == 0) {
    *file_size = statbuf.st_size;
  } else {
    printf("get file size failed: %s\n", strerror(errno));
    *file_size = 0;
  }
}

size_t ReadFile(char *filename, uint64_t offset, uint64_t size, Slice *result)
{
  int fd;
  char *buf;
  ssize_t tmp_read;
  size_t file_size, read_size = 0;

  if (size <= 0) {
    printf("read size must bigger than 0\n");
    return 0;
  }
  GetFilesize(filename, &file_size);
  if (file_size == 0 || (uint64_t)(file_size) < offset + size) {
    printf("read file too long!\n");
    return 0;
  }
  buf = (char *)malloc(size);
  if (buf == NULL) {
    printf("read file fail, malloc error\n");
    return 0;
  }

  if ((fd = open(filename, O_RDONLY)) == -1) {
    printf("can not open file:%s, %s\n", filename, strerror(errno));
    goto free_buf;
  }
  if (lseek(fd, (off_t)(offset), SEEK_SET) == -1) {
    printf("file=%s seek to offset=%lu failed, %s\n",
           filename, offset, strerror(errno));
    goto close_fd;
  }
  while (read_size < size) {
    tmp_read = read(fd, buf + read_size, size);
    if (tmp_read == -1 || tmp_read == 0) {
      printf("file=%s read failed, %s\n", filename, strerror(errno));
      goto close_fd;
    }
    read_size += tmp_read;
    size -= tmp_read;
  }
  close(fd);
  *result = Slice((const char *)buf, read_size);
  return read_size;

close_fd:
  if (fd > 0)
    close(fd);
free_buf:
  if (buf)
    free(buf);
  return 0;
}

} // namespace shannon
