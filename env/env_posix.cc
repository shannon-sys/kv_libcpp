#include "swift/shannon_db.h"
#include "swift/env.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>

namespace shannon {

class PosixEnv : public Env {
 public:
  PosixEnv() {  }
  virtual ~PosixEnv() {  }
  virtual Status GetCurrentTime(int64_t* unix_time) override {
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
};

Env* Env::Default() {
  static PosixEnv default_env;
  return &default_env;
}

}  // namespace shannon

