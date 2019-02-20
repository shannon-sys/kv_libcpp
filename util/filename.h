#ifndef STORAGE_SHANNON_DB_FILENAME_H_
#define STORAGE_SHANNON_DB_FILENAME_H_

#include "swift/slice.h"
#include "swift/status.h"
#include <stdint.h>
#include <string>

namespace shannon {

class Env;

enum FileType {
  kLogFile,
  kDBLockFile,
  kTableFile,
  kDescriptorFile,
  kCurrentFile,
  kTempFile,
  kInfoLogFile // Either the current one, or an old one
};

extern std::string TableFileName(const std::string &name, uint64_t number,
                                 const char *cfname);
extern std::string SSTTableFileName(const std::string &dbname, uint64_t number);

bool Snappy_Compress(const char *input, size_t length, ::std::string *output);

} // namespace shannon

#endif // STORAGE_SHANNON_DB_FILENAME_H_
