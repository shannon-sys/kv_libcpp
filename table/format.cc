#include "format.h"

namespace shannon {

void BlockHandle::EncodeTo(std::string *dst) const {
  // Sanity check that all fields have been set
  assert(offset_ != ~static_cast<uint64_t>(0));
  assert(size_ != ~static_cast<uint64_t>(0));
  PutVarint64(dst, offset_);
  PutVarint64(dst, size_);
}

Status BlockHandle::DecodeFrom(Slice *input) {
  if (GetVarint64(input, &offset_) && GetVarint64(input, &size_)) {
    return Status::OK();
  } else {
    return Status::Corruption("bad block handle");
  }
}

void Footer::EncodeTo(std::string *dst) const {
  const size_t original_size = dst->size();
  dst->push_back(static_cast<char>(1));
  metaindex_handle_.EncodeTo(dst);
  index_handle_.EncodeTo(dst);
  dst->resize(original_size + kNewVersionsEncodedLength - 12); // Padding
  PutFixed32(dst, 0);
  PutFixed32(dst,
             static_cast<uint32_t>(kBlockBasedTableMagicNumber & 0xffffffffu));
  PutFixed32(dst, static_cast<uint32_t>(kBlockBasedTableMagicNumber >> 32));
  assert(dst->size() == original_size + kNewVersionsEncodedLength);
  (void)original_size; // Disable unused variable warning.
}

} // namespace shannon
