#ifndef _BINLOG_ITER_H_
#define _BINLOG_ITER_H_
#include "swift/shannon_db.h"

namespace shannon {

enum LogOpType {
  UNKNOWN_TYPE  = 0,
  KEY_ADD       = 1,
  KEY_DELETE    = 2,
  DB_CREATE     = 3,
  DB_DELETE     = 4
};

class LogIterator {
 public:
  LogIterator() {};
  virtual ~LogIterator() {};

  // After Next(), an iterator is either positioned at a key/value cmd, or not valid.
  virtual bool Valid() const = 0;
  virtual void Next() = 0;

  // will update status after each action
  virtual Status status() const = 0;
  // please check Valid(), and use key() first
  // then check status() == OK(), and use value()
  // check status() again, can use others
  virtual Slice key() = 0;
  virtual Slice value() = 0;
  virtual LogOpType optype() = 0;
  virtual uint64_t timestamp() = 0;
  virtual int32_t db() = 0;
  virtual int32_t cf() = 0;
};

Status NewLogIterator(std::string& device, uint64_t timestamp, LogIterator **);

} // namespace shannon

#endif
