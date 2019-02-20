#ifndef STORAGE_SHANNON_TABLE_FORMAT_H_
#define STORAGE_SHANNON_TABLE_FORMAT_H_

#include "swift/slice.h"
#include "swift/status.h"
#include "table_builder.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include <stdint.h>
#include <string>

namespace shannon {

class Block;
struct ReadOptions;

// BlockHandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class BlockHandle {
public:
  BlockHandle();

  // The offset of the block in the file.
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

  // The size of the stored block
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void EncodeTo(std::string *dst) const;
  Status DecodeFrom(Slice *input);

  // Maximum encoding length of a BlockHandle
  enum { kMaxEncodedLength = 10 + 10 };

private:
  uint64_t offset_;
  uint64_t size_;
};

// Footer encapsulates the fixed information stored at the tail
// end of every table file.
class Footer {
public:
  Footer() {}

  // The block handle for the metaindex block of the table
  const BlockHandle &metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const BlockHandle &h) { metaindex_handle_ = h; }

  // The block handle for the index block of the table
  const BlockHandle &index_handle() const { return index_handle_; }
  void set_index_handle(const BlockHandle &h) { index_handle_ = h; }

  void EncodeTo(std::string *dst) const;
  Status DecodeFrom(Slice *input);

  // Encoded length of a Footer.  Note that the serialization of a
  // Footer will always occupy exactly this many bytes.  It consists
  // of two block handles and a magic number.
  enum {
    kEncodedLength = 2 * BlockHandle::kMaxEncodedLength + 8,
    kNewVersionsEncodedLength = 1 + 2 * BlockHandle::kMaxEncodedLength + 4 + 8
  };

private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};

static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;
static const uint64_t kBlockBasedTableMagicNumber = 0x88e241b785f4cff7ull;
// 1-byte type + 32-bit crc
static const size_t kBlockTrailerSize = 5;

struct BlockContents {
  Slice data;          // Actual contents of data
  bool cachable;       // True iff data can be cached
  bool heap_allocated; // True iff caller should delete[] data.data()
};

// Implementation details follow.  Clients should ignore,

inline BlockHandle::BlockHandle()
    : offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0)) {}

} // namespace shannon

#endif // STORAGE_SHANNON_TABLE_FORMAT_H_
