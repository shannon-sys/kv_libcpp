// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef ADVANCED_OPTIONS_H_
#define ADVANCED_OPTIONS_H_

#include <memory>

namespace shannon {

class Slice;
enum CompressionType : unsigned char;
struct Options;

enum CompactionStyle : char {
  // level based compaction style
  kCompactionStyleLevel = 0x0,
  // Universal compaction style
  // Not supported in shannon_LITE.
  kCompactionStyleUniversal = 0x1,
  // FIFO compaction style
  // Not supported in shannon_LITE
  kCompactionStyleFIFO = 0x2,
  // Disable background compaction. Compaction jobs are submitted
  // via CompactFiles().
  // Not supported in shannon_LITE
  kCompactionStyleNone = 0x3,
};

// In Level-based compaction, it Determines which file from a level to be
// picked to merge to the next level. We suggest people try
// kMinOverlappingRatio first when you tune your database.
enum CompactionPri : char {
  // Slightly prioritize larger files by size compensated by #deletes
  kByCompensatedSize = 0x0,
  // First compact files whose data's latest update time is oldest.
  // Try this if you only update some hot keys in small ranges.
  kOldestLargestSeqFirst = 0x1,
  // First compact files whose range hasn't been compacted to the next level
  // for the longest. If your updates are random across the key space,
  // write amplification is slightly better with this option.
  kOldestSmallestSeqFirst = 0x2,
  // First compact files whose ratio between overlapping size in next level
  // and its size is the smallest. It in many cases can optimize write
  // amplification.
  kMinOverlappingRatio = 0x3,
};

struct CompactionOptionsFIFO {
  // once the total sum of table files reaches this, we will delete the oldest
  // table file
  // Default: 1GB
  uint64_t max_table_files_size;

  // Drop files older than TTL. TTL based deletion will take precedence over
  // size based deletion if ttl > 0.
  // delete if sst_file_creation_time < (current_time - ttl)
  // unit: seconds. Ex: 1 day = 1 * 24 * 60 * 60
  // Default: 0 (disabled)
  uint64_t ttl = 0;

  // If true, try to do compaction to compact smaller files into larger ones.
  // Minimum files to compact follows options.level0_file_num_compaction_trigger
  // and compaction won't trigger if average compact bytes per del file is
  // larger than options.write_buffer_size. This is to protect large files
  // from being compacted again.
  // Default: false;
  bool allow_compaction = false;

  CompactionOptionsFIFO() : max_table_files_size(1 * 1024 * 1024 * 1024) {}
  CompactionOptionsFIFO(uint64_t _max_table_files_size, bool _allow_compaction,
                        uint64_t _ttl = 0)
      : max_table_files_size(_max_table_files_size),
        ttl(_ttl),
        allow_compaction(_allow_compaction) {}
};

// Compression options for different compression algorithms like Zlib
struct CompressionOptions {
  // shannon's generic default compression level. Internally it'll be translated
  // to the default compression level specific to the library being used (see
  // comment above `ColumnFamilyOptions::compression`).
  //
  // The default value is the max 16-bit int as it'll be written out in OPTIONS
  // file, which should be portable.
  const static int kDefaultCompressionLevel = 32767;

  int window_bits;
  int level;
  int strategy;

  // Maximum size of dictionaries used to prime the compression library.
  // Enabling dictionary can improve compression ratios when there are
  // repetitions across data blocks.
  //
  // The dictionary is created by sampling the SST file data. If
  // `zstd_max_train_bytes` is nonzero, the samples are passed through zstd's
  // dictionary generator. Otherwise, the random samples are used directly as
  // the dictionary.
  //
  // When compression dictionary is disabled, we compress and write each block
  // before buffering data for the next one. When compression dictionary is
  // enabled, we buffer all SST file data in-memory so we can sample it, as data
  // can only be compressed and written after the dictionary has been finalized.
  // So users of this feature may see increased memory usage.
  //
  // Default: 0.
  uint32_t max_dict_bytes;

  // Maximum size of training data passed to zstd's dictionary trainer. Using
  // zstd's dictionary trainer can achieve even better compression ratio
  // improvements than using `max_dict_bytes` alone.
  //
  // The training data will be used to generate a dictionary of max_dict_bytes.
  //
  // Default: 0.
  uint32_t zstd_max_train_bytes;

  // When the compression options are set by the user, it will be set to "true".
  // For bottommost_compression_opts, to enable it, user must set enabled=true.
  // Otherwise, bottommost compression will use compression_opts as default
  // compression options.
  //
  // For compression_opts, if compression_opts.enabled=false, it is still
  // used as compression options for compression process.
  //
  // Default: false.
  bool enabled;

  CompressionOptions()
      : window_bits(-14),
        level(kDefaultCompressionLevel),
        strategy(0),
        max_dict_bytes(0),
        zstd_max_train_bytes(0),
        enabled(false) {}
  CompressionOptions(int wbits, int _lev, int _strategy, int _max_dict_bytes,
                     int _zstd_max_train_bytes, bool _enabled)
      : window_bits(wbits),
        level(_lev),
        strategy(_strategy),
        max_dict_bytes(_max_dict_bytes),
        zstd_max_train_bytes(_zstd_max_train_bytes),
        enabled(_enabled) {}
};

}  // namespace shannon

#endif
