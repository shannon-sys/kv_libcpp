#ifndef STORAGE_SHANNON_INCLUDE_TABLE_BUILDER_H_
#define STORAGE_SHANNON_INCLUDE_TABLE_BUILDER_H_

#include "swift/filter_policy.h"
#include "swift/options.h"
#include "swift/status.h"
#include <stdint.h>

namespace shannon {

class BlockBuilder;
class BlockHandle;
class WritableFile;

class TableBuilder {
public:
  TableBuilder(const Options &options, WritableFile *file);
  TableBuilder(const Options &options, WritableFile *file,
               const string &columnfamily_name, uint32_t columnfamily_id);
  ~TableBuilder();

  Status ChangeOptions(const Options &options);
  void Add(const Slice &key, const Slice &value);

  void Flush();

  Status status() const;

  Status Finish(uint64_t creation_time, uint64_t oldest_key_time);

  void Abandon();

  uint64_t NumEntries() const;

  uint64_t FileSize() const;

private:
  bool ok() const { return status().ok(); }
  void WriteBlock(BlockBuilder *block, BlockHandle *handle);
  void WritePropertiesBlock(BlockBuilder *block, BlockHandle *handle);
  void WriteRawBlock(const Slice &data, CompressionType, BlockHandle *handle);

  struct Rep;
  Rep *rep_;

  // No copying allowed
  TableBuilder(const TableBuilder &);
  void operator=(const TableBuilder &);
};

} // namespace shannon

#endif // STORAGE_SHANNON_INCLUDE_TABLE_BUILDER_H_
