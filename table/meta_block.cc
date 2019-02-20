#include <map>
#include <memory>
#include <string>
#include <vector>
#include "block_builder.h"
#include "meta_block.h"
#include "format.h"
namespace shannon {
const std::string TablePropertiesNames::kDataSize  =
    "rocksdb.data.size";
const std::string TablePropertiesNames::kIndexSize =
    "rocksdb.index.size";
const std::string TablePropertiesNames::kIndexPartitions =
    "rocksdb.index.partitions";
const std::string TablePropertiesNames::kTopLevelIndexSize =
    "rocksdb.top-level.index.size";
const std::string TablePropertiesNames::kIndexKeyIsUserKey =
    "rocksdb.index.key.is.user.key";
const std::string TablePropertiesNames::kIndexValueIsDeltaEncoded =
    "rocksdb.index.value.is.delta.encoded";
const std::string TablePropertiesNames::kFilterSize =
    "rocksdb.filter.size";
const std::string TablePropertiesNames::kRawKeySize =
    "rocksdb.raw.key.size";
const std::string TablePropertiesNames::kRawValueSize =
    "rocksdb.raw.value.size";
const std::string TablePropertiesNames::kNumDataBlocks =
    "rocksdb.num.data.blocks";
const std::string TablePropertiesNames::kNumEntries =
    "rocksdb.num.entries";
const std::string TablePropertiesNames::kDeletedKeys =
    "rocksdb.deleted.keys";
const std::string TablePropertiesNames::kMergeOperands =
    "rocksdb.merge.operands";
const std::string TablePropertiesNames::kNumRangeDeletions =
    "rocksdb.num.range-deletions";
const std::string TablePropertiesNames::kFilterPolicy =
    "rocksdb.filter.policy";
const std::string TablePropertiesNames::kFormatVersion =
    "rocksdb.format.version";
const std::string TablePropertiesNames::kFixedKeyLen =
    "rocksdb.fixed.key.length";
const std::string TablePropertiesNames::kColumnFamilyId =
    "rocksdb.column.family.id";
const std::string TablePropertiesNames::kColumnFamilyName =
    "rocksdb.column.family.name";
const std::string TablePropertiesNames::kComparator =
    "rocksdb.comparator";
const std::string TablePropertiesNames::kMergeOperator =
    "rocksdb.merge.operator";
const std::string TablePropertiesNames::kPrefixExtractorName =
    "rocksdb.prefix.extractor.name";
const std::string TablePropertiesNames::kPropertyCollectors =
    "rocksdb.property.collectors";
const std::string TablePropertiesNames::kCompression =
    "rocksdb.compression";
const std::string TablePropertiesNames::kCreationTime =
    "rocksdb.creation.time";
const std::string TablePropertiesNames::kOldestKeyTime =
    "rocksdb.oldest.key.time";
const std::string kVersion =
    "rocksdb.external_sst_file.version";
const std::string kGlobalSeqno =
    "rocksdb.external_sst_file.global_seqno";

PropertyBlockBuilder::PropertyBlockBuilder() {
        prop_restarts_.push_back(0);       // First restart point is at offset 0
}

void PropertyBlockBuilder::Add(const std::string& key,
                               const std::string& value) {
  size_t shared = 0;
  const size_t non_shared = key.size() - shared;
  prop_restarts_.push_back(buffer_.size());
  // Add "<shared><non_shared><value_size>" to buffer_
  PutVarint32(&buffer_, shared);
  PutVarint32(&buffer_, non_shared);
  PutVarint32(&buffer_, value.size());
  // Add string delta to buffer_ followed by value
  buffer_.append(key.data() + shared, non_shared);
  buffer_.append(value.data(), value.size());
}

void PropertyBlockBuilder::Add(const std::string& name, uint64_t val) {
  std::string dst;
  PutVarint64(&dst, val);
  Add(name, dst);
}

void PropertyBlockBuilder::AddTableProperty(const TableProperties& props) {
  Add(TablePropertiesNames::kColumnFamilyId, props.column_family_id);
  if (!props.column_family_name.empty()) {
    Add(TablePropertiesNames::kColumnFamilyName, props.column_family_name);
  }
  if (!props.compression_name.empty()) {
    Add(TablePropertiesNames::kCompression, props.compression_name);
  }
  Add(TablePropertiesNames::kCreationTime, props.creation_time);
  Add(TablePropertiesNames::kDataSize, props.data_size);
  Add(kGlobalSeqno, 0);
  Add(kVersion, 2);
  Add(TablePropertiesNames::kFormatVersion, 0);
  Add(TablePropertiesNames::kIndexSize, props.index_size);
  Add(TablePropertiesNames::kNumDataBlocks, props.num_data_blocks);
  Add(TablePropertiesNames::kNumEntries, props.num_entries);
  Add(TablePropertiesNames::kOldestKeyTime, props.oldest_key_time);
  Add(TablePropertiesNames::kRawKeySize, props.raw_key_size);
  Add(TablePropertiesNames::kRawValueSize, props.raw_value_size);
  if (!props.filter_policy_name.empty()) {
    Add(TablePropertiesNames::kFilterPolicy, props.filter_policy_name);
  }
}

Slice PropertyBlockBuilder::ProperityBlockFinish() {
  // Append restart array
  for (size_t i = 0; i < prop_restarts_.size(); i++) {
    PutFixed32(&buffer_, prop_restarts_[i]);
  }
  PutFixed32(&buffer_, prop_restarts_.size());
  return Slice(buffer_);
}
}

