#include "filename.h"
#include "src/column_family.h"
#include "swift/options.h"
#include "swift/shannon_db.h"
#include "swift/slice.h"
#include "swift/status.h"
#include "swift/table.h"
#include "table/dbformat.h"
#include <ctype.h>
#include <stdio.h>

#ifdef HAVE_SNAPPY
#include <snappy.h>
#endif // defined(HAVE_SNAPPY)
namespace shannon {

static std::string MakeFileName(const std::string &name, uint64_t number,
                                const char *cfname, const char *suffix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/%s%06llu.%s", cfname,
           static_cast<unsigned long long>(number), suffix);
  return name + buf;
}

std::string TableFileName(const std::string &name, uint64_t number,
                          const char *cfname) {
  return MakeFileName(name, number, cfname, "sst");
}

std::string SSTTableFileName(const std::string &name, uint64_t number,
                             const char *cfname) {
  return MakeFileName(name, number, cfname, "sst");
}

bool Snappy_Compress(const char *input, size_t length, ::std::string *output) {
#ifdef HAVE_SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif // defined(HAVE_SNAPPY)

  return false;
}

} // namespace shannon
