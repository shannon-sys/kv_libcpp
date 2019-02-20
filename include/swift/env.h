
#ifndef STORAGE_SHANNON_INCLUDE_ENV_H_
#define STORAGE_SHANNON_INCLUDE_ENV_H_

#include "swift/status.h"
#include <string>

namespace shannon {

class WritableFile {
public:
  WritableFile() {}
  virtual ~WritableFile();

  virtual Status Append(const Slice &data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;

private:
  // No copying allowed
  WritableFile(const WritableFile &);
  void operator=(const WritableFile &);
};

class Env {
public:
  virtual Status GetCurrentTime(int64_t *unix_time) = 0;
  virtual std::string TimeToString(uint64_t time) = 0;
  virtual Status NewWritableFile(const std::string &fname,
                                 WritableFile **result) = 0;
  virtual Status CreateDir(const std::string &name) = 0;
  virtual bool FileExists(const std::string &fname) = 0;
  virtual Status DeleteFile(const std::string &fname) = 0;
  static Env *Default();
  Env(){};
  virtual ~Env();
};

class EnvWrapper : public Env {
public:
  Status GetCurrentTime(int64_t *unix_time) override {
    return target_->GetCurrentTime(unix_time);
  }

  std::string TimeToString(uint64_t time) override {
    return target_->TimeToString(time);
  }

  Status NewWritableFile(const std::string &f, WritableFile **r) {
    return target_->NewWritableFile(f, r);
  }

  Status CreateDir(const std::string &name) { return target_->CreateDir(name); }

  Status DeleteFile(const std::string &name) {
    return target_->DeleteFile(name);
  }
  bool FileExists(const std::string &name) { return target_->FileExists(name); }
  virtual ~EnvWrapper();

private:
  Env *target_;
};

} // namespace shannon

#endif // STORAGE_SHANNON_INCLUDE_ENV_H_
