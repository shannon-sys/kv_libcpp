#include "swift/env.h"
#include "swift/shannon_db.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace shannon {

static int open_read_only_file_limit = -1;
static int mmap_limit = -1;

static const size_t kBufSize = 65536;
static Status PosixError(const std::string &context, int err_number) {
  if (err_number == ENOENT) {
    return Status::NotFound(context, strerror(err_number));
  } else {
    return Status::IOError(context, strerror(err_number));
  }
}

class PosixWritableFile : public WritableFile {
private:
  // buf_[0, pos_-1] contains data to be written to fd_.
  std::string filename_;
  int fd_;
  char buf_[kBufSize];
  size_t pos_;

public:
  PosixWritableFile(const std::string &fname, int fd)
      : filename_(fname), fd_(fd), pos_(0) {}

  ~PosixWritableFile() {
    if (fd_ >= 0) {
      // Ignoring any potential errors
      Close();
    }
  }

  virtual Status Append(const Slice &data) {
    size_t n = data.size();
    const char *p = data.data();
    // Fit as much as possible into buffer.
    size_t copy = std::min(n, kBufSize - pos_);
    memcpy(buf_ + pos_, p, copy);
    p += copy;
    n -= copy;
    pos_ += copy;
    if (n == 0) {
      return Status::OK();
    }
    // Can't fit in buffer, so need to do at least one write.
    Status s = FlushBuffered();
    if (!s.ok()) {
      return s;
    }

    // Small writes go to buffer, large writes are written directly.
    if (n < kBufSize) {
      memcpy(buf_, p, n);
      pos_ = n;
      return Status::OK();
    }
    return WriteRaw(p, n);
  }

  virtual Status Close() {
    Status result = FlushBuffered();
    const int r = close(fd_);
    if (r < 0 && result.ok()) {
      result = PosixError(filename_, errno);
    }
    fd_ = -1;
    return result;
  }

  virtual Status Flush() { return FlushBuffered(); }

  Status SyncDirIfManifest() {
    const char *f = filename_.c_str();
    const char *sep = strrchr(f, '/');
    Slice basename;
    std::string dir;
    if (sep == NULL) {
      dir = ".";
      basename = f;
    } else {
      dir = std::string(f, sep - f);
      basename = sep + 1;
    }
    Status s;
    if (basename.starts_with("MANIFEST")) {
      int fd = open(dir.c_str(), O_RDONLY);
      if (fd < 0) {
        s = PosixError(dir, errno);
      } else {
        if (fsync(fd) < 0) {
          s = PosixError(dir, errno);
        }
        close(fd);
      }
    }
    return s;
  }

  virtual Status Sync() {
    // Ensure new files referred to by the manifest are in the filesystem.
    Status s = SyncDirIfManifest();
    if (!s.ok()) {
      return s;
    }
    s = FlushBuffered();
    if (s.ok()) {
      if (fdatasync(fd_) != 0) {
        s = PosixError(filename_, errno);
      }
    }
    return s;
  }

private:
  Status FlushBuffered() {
    Status s = WriteRaw(buf_, pos_);
    pos_ = 0;
    return s;
  }

  Status WriteRaw(const char *p, size_t n) {
    while (n > 0) {
      ssize_t r = write(fd_, p, n);
      if (r < 0) {
        if (errno == EINTR) {
          continue; // Retry
        }
        return PosixError(filename_, errno);
      }
      p += r;
      n -= r;
    }
    return Status::OK();
  }
};

class PosixEnv : public Env {
public:
  PosixEnv() {}
  virtual ~PosixEnv() {}
  virtual Status GetCurrentTime(int64_t *unix_time) override {
    if (unix_time == NULL)
      return Status::InvalidArgument("unix_time is not null!");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *unix_time = tv.tv_sec;
    return Status();
  }

  virtual std::string TimeToString(uint64_t time) override {
    char buffer[32];
    sprintf(buffer, "%lu", time);
    return string(buffer);
  }

  virtual bool FileExists(const std::string &fname) {
    return access(fname.c_str(), F_OK) == 0;
  }

  virtual Status DeleteFile(const std::string &fname) {
    Status result;
    if (unlink(fname.c_str()) != 0) {
      result = PosixError(fname, errno);
    }
    return result;
  }

  virtual Status CreateDir(const std::string &name) {
    Status result;
    if (mkdir(name.c_str(), 0755) != 0) {
      result = PosixError(name, errno);
    }
    return result;
  }

  virtual Status NewWritableFile(const std::string &fname,
                                 WritableFile **result) override {
    Status s;
    int fd = open(fname.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
      *result = NULL;
      s = PosixError(fname, errno);
    } else {
      *result = new PosixWritableFile(fname, fd);
    }
    return s;
  }
};

Env *Env::Default() {
  static PosixEnv default_env;
  return &default_env;
}

} // namespace shannon
