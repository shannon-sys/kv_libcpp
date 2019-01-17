// Copyright (c) 2011 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SHANNON_INCLUDE_ENV_H_
#define STORAGE_SHANNON_INCLUDE_ENV_H_

#include <string>
#include "swift/status.h"

namespace shannon {

class Env {
 public:
  virtual Status GetCurrentTime(int64_t* unix_time) = 0;
  virtual std::string TimeToString(uint64_t time) = 0;
  static Env* Default();
};

class EnvWrapper : public Env {
 public:
  Status GetCurrentTime(int64_t* unix_time) override {
    return target_->GetCurrentTime(unix_time);
  }

  std::string TimeToString(uint64_t time) override {
    return target_->TimeToString(time);
  }

 private:
  Env* target_;
};

}  // namespace shannon

#endif  // STORAGE_SHANNON_INCLUDE_ENV_H_
