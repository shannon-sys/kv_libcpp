#include "block_builder.h"
#include "swift/slice.h"
#include "util/hash.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace shannon {
struct TablePropertiesNames {
  static const std::string kDataSize;
  static const std::string kIndexSize;
  static const std::string kIndexPartitions;
  static const std::string kTopLevelIndexSize;
  static const std::string kIndexKeyIsUserKey;
  static const std::string kIndexValueIsDeltaEncoded;
  static const std::string kFilterSize;
  static const std::string kRawKeySize;
  static const std::string kRawValueSize;
  static const std::string kNumDataBlocks;
  static const std::string kNumEntries;
  static const std::string kDeletedKeys;
  static const std::string kMergeOperands;
  static const std::string kNumRangeDeletions;
  static const std::string kFormatVersion;
  static const std::string kFixedKeyLen;
  static const std::string kFilterPolicy;
  static const std::string kColumnFamilyName;
  static const std::string kColumnFamilyId;
  static const std::string kComparator;
  static const std::string kMergeOperator;
  static const std::string kPrefixExtractorName;
  static const std::string kPropertyCollectors;
  static const std::string kCompression;
  static const std::string kCreationTime;
  static const std::string kOldestKeyTime;
};

struct TableProperties {
public:
  // the total size of all data blocks.
  uint64_t data_size = 0;
  // the size of index block.
  uint64_t index_size = 0;
  // Total number of index partitions if kTwoLevelIndexSearch is used
  uint64_t index_partitions = 0;
  // Size of the top-level index if kTwoLevelIndexSearch is used
  uint64_t top_level_index_size = 0;
  // Whether the index key is user key. Otherwise it includes 8 byte of sequence
  // number added by internal key format.
  uint64_t index_key_is_user_key = 0;
  // Whether delta encoding is used to encode the index values.
  uint64_t index_value_is_delta_encoded = 0;
  // the size of filter block.
  uint64_t filter_size = 0;
  // total raw key size
  uint64_t raw_key_size = 0;
  // total raw value size
  uint64_t raw_value_size = 0;
  // the number of blocks in this table
  uint64_t num_data_blocks = 0;
  // the number of entries in this table
  uint64_t num_entries = 0;
  // the number of deletions in the table
  uint64_t num_deletions = 0;
  // the number of merge operands in the table
  uint64_t num_merge_operands = 0;
  // the number of range deletions in this table
  uint64_t num_range_deletions = 0;
  // format version, reserved for backward compatibility
  uint64_t format_version = 0;
  // If 0, key is variable length. Otherwise number of bytes for each key.
  uint64_t fixed_key_len = 0;
  // ID of column family for this SST file, corresponding to the CF identified
  // by column_family_name.
  uint64_t column_family_id = 0;
  // The time when the SST file was created.
  // Since SST files are immutable, this is equivalent to last modified time.
  uint64_t creation_time = 0;
  // Timestamp of the earliest key. 0 means unknown.
  uint64_t oldest_key_time = 0;

  // Name of the column family with which this SST file is associated.
  // If column family is unknown, `column_family_name` will be an empty string.
  std::string column_family_name;

  // The name of the filter policy used in this table.
  // If no filter policy is used, `filter_policy_name` will be an empty string.
  std::string filter_policy_name;

  // The name of the comparator used in this table.
  std::string comparator_name;

  // The name of the merge operator used in this table.
  // If no merge operator is used, `merge_operator_name` will be "nullptr".
  std::string merge_operator_name;

  // The name of the prefix extractor used in this table
  // If no prefix extractor is used, `prefix_extractor_name` will be "nullptr".
  std::string prefix_extractor_name;

  // The names of the property collectors factories used in this table
  // separated by commas
  // {collector_name[1]},{collector_name[2]},{collector_name[3]} ..
  std::string property_collectors_names;

  // The compression algo used to compress the SST files.
  std::string compression_name;

  std::string ToString(const std::string &prop_delim = "; ",
                       const std::string &kv_delim = "=") const;

  void Add(const TableProperties &tp);
};

class PropertyBlockBuilder {
public:
  PropertyBlockBuilder(const PropertyBlockBuilder &) = delete;
  PropertyBlockBuilder &operator=(const PropertyBlockBuilder &) = delete;

  PropertyBlockBuilder();

  void AddTableProperty(const TableProperties &props);
  void Add(const std::string &key, uint64_t value);
  void Fixed64Add(const std::string &key, uint64_t value);
  void Fixed32Add(const std::string &key, uint32_t value);
  void Add(const std::string &key, const std::string &value);
  // Write all the added entries to the block and return the block contents
  Slice ProperityBlockFinish();

private:
  std::string buffer_;
  std::vector<uint32_t> prop_restarts_;
};
}
