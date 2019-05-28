
#ifndef SHANNON_DB_INCLUDE_OPTIONS_H_
#define SHANNON_DB_INCLUDE_OPTIONS_H_

#include <stddef.h>
#include <vector>
#include <string>
#include "swift/env.h"
#include "swift/compaction_filter.h"
#include "swift/comparator.h"
#include "swift/table.h"
#include "swift/advanced_options.h"

namespace shannon {

class Snapshot;
struct ColumnFamilyDescriptor;
class ColumnFamilyHandle;
struct Options;

enum CompressionType : unsigned char {
  kNoCompression = 0x0,
  kSnappyCompression = 0x1,
  kZlibCompression = 0x2,
  kBZip2Compression = 0x3,
  kLZ4Compression = 0x5,
  kXpressCompression = 0x6,
  kZSTD = 0x7,
  kZSTDNotFinalCompression = 0x40,
  kDisableCompressionOption = 0xff
};

struct DBOptions {
  bool create_if_missing = false;
  bool create_missing_column_families = false;
  size_t keep_log_file_num = 1000;
  size_t max_manifest_file_size = 64 * 1024 * 1024;
  size_t max_log_file_size = 0;
  size_t write_buffer_size = 64 << 20;
//build_sst
  size_t block_size = 4096;
  int block_restart_interval = 16;
  size_t max_file_size = 2 * 1024 * 1024;
//NULL
  FilterPolicy* filter_policy = NULL;

  size_t target_file_size_base = 64 * 1048576;
  int max_background_flushes = -1;
  int max_background_compactions = -1;
  int max_open_files = -1;
  int max_bytes_for_level_multiplier = -1;
  int optimize_filters_for_hits = -1;
  int level_compaction_dynamic_level_bytes = -1;
  CompressionType compression = kNoCompression;
  Env* env = Env::Default();
  DBOptions(){}
};
struct AdvancedColumnFamilyOptions {
  size_t cache_size = 1024;
  AdvancedColumnFamilyOptions() { }
};

struct ColumnFamilyOptions : public AdvancedColumnFamilyOptions {
  const Comparator* inner_comparator = BytewiseComparator();
  std::shared_ptr<CompactionFilterFactory> compaction_filter_factory = nullptr;
  std::shared_ptr<TableFactory> table_factory;
  ColumnFamilyOptions() { }
};

struct Options : public DBOptions, public ColumnFamilyOptions {
  Options() : DBOptions(), ColumnFamilyOptions() {  };
  Options(const DBOptions& db_options,
          const ColumnFamilyOptions& column_family_options)
      : DBOptions(db_options), ColumnFamilyOptions(column_family_options) {}
};

// Options that control read operations
struct ReadOptions {
  // If true, all data read from underlying storage will be
  // verified against corr
  // Default: false
  bool verify_checksums;

  // Should the data read for this iteration be cached in memory?
  // Callers may wish to set this field to false for bulk scans.
  // Default: true
  bool fill_cache;

  // If "snapshot" is non-NULL, read as of the supplied snapshot
  // (which must belong to the DB that is being read and which must
  // not have been released).  If "snapshot" is NULL, use an impliicit
  // snapshot of the state at the beginning of this read operation.
  // Default: NULL
  const Snapshot* snapshot;

  ReadOptions()
      : verify_checksums(false),
        fill_cache(true),
        snapshot(NULL) {
  }
};

// Options that control write operations
struct WriteOptions {
  // Default: true
  bool sync;
  // Should the data write for this iteration be cached in memory?
  // Default: true
  bool fill_cache;

  WriteOptions()
      : sync(true),
        fill_cache(true) {
  }
};
struct CompactRangeOptions {
  bool exclusive_manual_compaction = true;
  bool change_level = false;
};

};  // namespace shannon

#endif  // SHANNON_DB_INCLUDE_OPTIONS_H_
