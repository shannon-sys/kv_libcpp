// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "swift/log_iter.h"
#include "src/venice_kv.h"
#include "src/venice_ioctl.h"

namespace shannon {

class LogIteratorImpl : public LogIterator {
 public:
  LogIteratorImpl(std::string& device, int fd, int idx, uint64_t seq)
    : device_(device),
      fd_(fd),
      iter_index_(idx),
      iter_sequence_(seq),
      valid_(false) {
  }

  virtual ~LogIteratorImpl();

  virtual bool Valid() const { return valid_; }
  virtual Status status() const { return status_; }
  virtual Slice key() override;
  virtual Slice value() override;
  virtual LogOpType optype() override;
  virtual uint64_t timestamp() override;
  virtual int32_t db() override;
  virtual int32_t cf() override;

  virtual void Next() override;
 private:
  int fd_;
  std::string device_;
  int iter_index_;
  uint64_t iter_sequence_;

  /* only used by next(); and prev() in the future */
  bool valid_;
  /* will update at each action */
  Status status_;

#define LOG_READED_NONE           (0)
#define LOG_READED_KEY            (1 << 0)
#define LOG_READED_VALUE          (1 << 1)
#define LOG_READED_MASK           (0x00000003)
  uint32_t read_flag_;
  int32_t db_;
  int32_t cf_;
  LogOpType optype_;
  uint64_t timestamp_;
  std::string saved_key_;
  std::string saved_value_;
  void set_optype(unsigned char type);

  // No copying allowed
  LogIteratorImpl(const LogIteratorImpl&);
  void operator=(const LogIteratorImpl&);

};

  Status NewLogIterator(std::string& device, uint64_t timestamp, LogIterator **log_iter) {
    int fd, ret = 0;
    struct uapi_log_iter_create option;
    Status s;

    /* open device */
    fd = open(device.data(), O_RDWR);
    if (fd < 0) {
      std::cout<<"create new log iter: open device="<<device.data()<<" failed"<<endl;
      return Status::IOError(strerror(errno));
    }

    option.timestamp = timestamp;
    option.iter.iter_index = -1;
    option.iter.iter_sequence = 0;
    option.iter.valid_iter = 0;
    ret = ioctl(fd, IOCTL_CREATE_LOG_ITER, &option);
    if (ret < 0 || option.iter.valid_iter == 0) {
      close(fd);
      *log_iter = NULL;
      return Status::IOError("ioctl create_log_iter failed\n");
    }

    *log_iter = new LogIteratorImpl(device, fd, option.iter.iter_index, option.iter.iter_sequence);
     return s;
  }

LogIteratorImpl::~LogIteratorImpl() {
  struct uapi_log_iterator option;
  int ret = 0;

  option.iter_index = iter_index_;
  option.iter_sequence = iter_sequence_;
  ret = ioctl(fd_, IOCTL_DESTROY_LOG_ITER, &option);
  if (ret < 0) {
    std::cout<<"ioctl destroy_log_iter idx="<<iter_index_<<" failed!\n"<<endl;
  }
  if (fd_) {
    close(fd_);
  }
}

void LogIteratorImpl::Next() {
  struct uapi_log_iter_move_option option;
  int ret = 0;

  option.iter.iter_index = iter_index_;
  option.iter.iter_sequence = iter_sequence_;
  option.iter.valid_iter = 1;
  option.move_direction = LOG_MOVE_NEXT;
  option.valid_key = 0;
  ret = ioctl(fd_, IOCTL_LOG_ITER_MOVE, &option);
  valid_ = option.valid_key == 0 ? false : true;
  if (ret < 0) {
    if (option.iter.valid_iter == 0) {
      status_ = Status::Corruption("Invalid log iterator!!!");
    } else {
      status_ = Status::IOError("Next Failed", strerror(errno));
    }
    read_flag_ = LOG_READED_NONE;
    valid_ = false;
    return;
  }

  if (valid_) {
    status_ = Status::OK();
    read_flag_ = LOG_READED_NONE;
  } else {
    status_ = Status::NotFound("Not found new key/value cmd");
  }
}

void LogIteratorImpl::set_optype(unsigned char type)
{
  switch (type) {
    case LOG_ADD_KEY:
      optype_ = KEY_ADD; break;
    case LOG_DELETE_KEY:
      optype_ = KEY_DELETE; break;
    case LOG_CREATE_DB:
      optype_ = DB_CREATE; break;
    case LOG_DELETE_DB:
      optype_ = DB_DELETE; break;
    default:
      optype_ = UNKNOWN_TYPE;
  }
}

Slice LogIteratorImpl::key() {
  struct uapi_log_iter_get_option option;
  int ret = 0;

  if (!valid_) {
    return Slice();
  }
  if ((read_flag_ & LOG_READED_KEY)) {
    return saved_key_;
  }

  option.iter.iter_index = iter_index_;
  option.iter.iter_sequence = iter_sequence_;
  option.iter.valid_iter = 1;
  option.get_type = LOG_ITER_GET_KEY;
  option.valid_key = 0;
  option.key = (char *)malloc(MAX_KEY_SIZE);
  option.key_buf_len = MAX_KEY_SIZE;
  ret = ioctl(fd_, IOCTL_LOG_ITER_GET, &option);
  if (ret < 0) {
    if (option.iter.valid_iter == 0) {
      status_ = Status::Corruption("Invalid log iterator!!!");
    } else {
      status_ = Status::IOError("Iter get key failed", strerror(errno));
    }
    read_flag_ = (read_flag_ & ~LOG_READED_KEY) & LOG_READED_MASK;
    free(option.key);
    return Slice();
  }

  if (option.valid_key == 0) {
    status_ = Status::NotFound("Not found key");
    read_flag_ = (read_flag_ & ~LOG_READED_KEY) & LOG_READED_MASK;
    free(option.key);
    return Slice();
  }

  // set key, db, cf, optype, timestamp, read_flag
  status_ = Status::OK();
  read_flag_ &= LOG_READED_KEY;
  saved_key_.assign(option.key, option.key_len);
  free(option.key);
  db_ = option.db_index;
  cf_ = option.cf_index;
  set_optype(option.optype);
  timestamp_ = option.timestamp;

  return saved_key_;
}

Slice LogIteratorImpl::value() {
  struct uapi_log_iter_get_option option;
  int ret = 0;

  if (!valid_) {
    return Slice();
  }
  if ((read_flag_ & LOG_READED_VALUE)) {
    return saved_value_;
  }

  option.iter.iter_index = iter_index_;
  option.iter.iter_sequence = iter_sequence_;
  option.iter.valid_iter = 1;
  option.get_type = LOG_ITER_GET_VALUE;
  option.valid_key = 0;
  option.value = (char *)malloc(MAX_VALUE_SIZE);
  option.value_buf_len = MAX_VALUE_SIZE;
  ret = ioctl(fd_, IOCTL_LOG_ITER_GET, &option);
  if (ret < 0) {
    if (option.iter.valid_iter == 0) {
      status_ = Status::Corruption("Invalid log iterator!!!");
    } else {
      status_ = Status::IOError("Iter get value failed", strerror(errno));
    }
    read_flag_ = (read_flag_ & ~LOG_READED_VALUE) & LOG_READED_MASK;
    free(option.value);
    return Slice();
  }

  if (option.valid_key == 0) {
    status_ = Status::NotFound("Not found value");
    read_flag_ = (read_flag_ & ~LOG_READED_VALUE) & LOG_READED_MASK;
    free(option.value);
    return Slice();
  }

  // set value, db, cf, optype, timestamp, read_flag
  status_ = Status::OK();
  read_flag_ &= LOG_READED_VALUE;
  saved_value_.assign(option.value, option.value_len);
  free(option.value);
  db_ = option.db_index;
  cf_ = option.cf_index;
  set_optype(option.optype);
  timestamp_ = option.timestamp;

  return saved_value_;
}

LogOpType LogIteratorImpl::optype() {
  if (!(read_flag_ & LOG_READED_MASK)) {
    this->key();
  }
  if (status_ != Status::OK()) {
    std::cout<<"Get new optype failed!"<<endl;
  }
  return optype_;
}

int32_t LogIteratorImpl::db() {
  if (!(read_flag_ & LOG_READED_MASK)) {
    this->key();
  }
  if (status_ != Status::OK()) {
    std::cout<<"Get new db_index failed!"<<endl;
  }
  return db_;
}

int32_t LogIteratorImpl::cf() {
  if (!(read_flag_ & LOG_READED_MASK)) {
    this->key();
  }
  if (status_ != Status::OK()) {
    std::cout<<"Get new cf_index failed!"<<endl;
  }
  return cf_;
}

uint64_t LogIteratorImpl::timestamp() {
  if (!(read_flag_ & LOG_READED_MASK)) {
    this->key();
  }
  if (status_ != Status::OK()) {
    std::cout<<"Get new timestamp failed!"<<endl;
  }
  return timestamp_;
}

} // namespace shannon
