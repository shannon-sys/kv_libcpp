#include "table_builder.h"
#include "block_builder.h"
#include "filter_block.h"
#include "format.h"
#include "meta_block.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/filename.h"
#include <assert.h>
namespace shannon {

extern const std::string sPropertiesBlock = "rocksdb.properties";
inline std::string CompressionTypeToString(CompressionType compression_type) {
  switch (compression_type) {
  case kNoCompression:
    return "NoCompression";
  case kSnappyCompression:
    return "Snappy";
  case kZlibCompression:
    return "Zlib";
  case kBZip2Compression:
    return "BZip2";
  case kLZ4Compression:
    return "LZ4";
  case kXpressCompression:
    return "Xpress";
  case kZSTD:
    return "ZSTD";
  case kZSTDNotFinalCompression:
    return "ZSTDNotFinal";
  default:
    assert(false);
    return "";
  }
}
struct TableBuilder::Rep {
  Options options;
  Options index_block_options;
  WritableFile *file;
  uint64_t offset;
  Status status;
  BlockBuilder data_block;
  BlockBuilder index_block;
  std::string last_key;
  uint64_t num_entries;
  uint64_t creation_time;
  uint64_t raw_key_size;
  uint64_t raw_value_size;
  uint64_t data_size;
  uint64_t num_data_blocks;
  bool closed; // Either Finish() or Abandon() has been called.
  FilterBlockBuilder *filter_block;
  PropertyBlockBuilder *prop_block;

  bool pending_index_entry;
  BlockHandle pending_handle; // Handle to add to index block

  std::string compressed_output;
  std::string columnfamily_name;
  uint64_t columnfamily_id;

  Rep(const Options &opt, WritableFile *f)
      : options(opt), index_block_options(opt), file(f), offset(0),
        data_block(&options), index_block(&index_block_options), num_entries(0),
        num_data_blocks(0), creation_time(0), raw_value_size(0),
        raw_key_size(0), data_size(0), columnfamily_id(0),
        columnfamily_name("default"), closed(false),
        filter_block(opt.filter_policy == NULL ? NULL : new FilterBlockBuilder(
                                                            opt.filter_policy)),
        prop_block(new PropertyBlockBuilder()), pending_index_entry(false) {
    index_block_options.block_restart_interval = 1;
  }
};

TableBuilder::TableBuilder(const Options &options, WritableFile *file,
                           const string &columnfamily_name,
                           uint32_t columnfamily_id)
    : rep_(new Rep(options, file)) {
  if (rep_->filter_block != NULL) {
    rep_->filter_block->StartBlock(0);
  }
  rep_->columnfamily_name.assign(columnfamily_name.data(),
                                 columnfamily_name.size());
  rep_->columnfamily_id = columnfamily_id;
}

TableBuilder::TableBuilder(const Options &options, WritableFile *file)
    : rep_(new Rep(options, file)) {
  if (rep_->filter_block != NULL) {
    rep_->filter_block->StartBlock(0);
  }
}

TableBuilder::~TableBuilder() {
  assert(rep_->closed); // Catch errors where caller forgot to call Finish()
  delete rep_->filter_block;
  delete rep_->prop_block;
  delete rep_;
}

Status TableBuilder::ChangeOptions(const Options &options) {
  // Note: if more fields are added to Options, update
  // this function to catch changes that should not be allowed to
  // change in the middle of building a Table.
  if (options.comparator != rep_->options.comparator) {
    return Status::InvalidArgument("changing comparator while building table");
  }

  // Note that any live BlockBuilders point to rep_->options and therefore
  // will automatically pick up the updated options.
  rep_->options = options;
  rep_->index_block_options = options;
  rep_->index_block_options.block_restart_interval = 1;
  return Status::OK();
}

void TableBuilder::Add(const Slice &key, const Slice &value) {
  Rep *r = rep_;
  assert(!r->closed);
  if (!ok())
    return;
  if (r->num_entries > 0) {
    assert(r->options.comparator->Compare(key, Slice(r->last_key)) > 0);
  }

  if (r->pending_index_entry) {
    assert(r->data_block.empty());
    r->options.comparator->FindShortestSeparator(&r->last_key, key);
    std::string handle_encoding;
    r->pending_handle.EncodeTo(&handle_encoding);
    r->index_block.Add(r->last_key, Slice(handle_encoding));
    r->pending_index_entry = false;
  }

  if (r->filter_block != NULL) {
    r->filter_block->AddKey(key);
  }

  r->last_key.assign(key.data(), key.size());
  r->num_entries++;
  r->data_block.Add(key, value);
  r->raw_key_size += key.size();
  r->raw_value_size += value.size();
  const size_t estimated_block_size = r->data_block.CurrentSizeEstimate();
  if (estimated_block_size >= r->options.block_size) {
    Flush();
  }
}

void TableBuilder::Flush() {
  Rep *r = rep_;
  assert(!r->closed);
  if (!ok())
    return;
  if (r->data_block.empty())
    return;
  assert(!r->pending_index_entry);
  WriteBlock(&r->data_block, &r->pending_handle);
  if (ok()) {
    r->pending_index_entry = true;
    r->status = r->file->Flush();
  }
  if (r->filter_block != NULL) {
    r->filter_block->StartBlock(r->offset);
  }
  ++r->num_data_blocks;
  r->data_size += r->offset;
}

void TableBuilder::WriteBlock(BlockBuilder *block, BlockHandle *handle) {
  // File format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  Rep *r = rep_;
  Slice raw = block->Finish();

  Slice block_contents;
  CompressionType type = r->options.compression;
  // CompressionType type = kNoCompression;
  // TODO(postrelease): Support more compression options: zlib?
  switch (type) {
  case kNoCompression:
    block_contents = raw;
    break;
  case kSnappyCompression: {
    std::string *compressed = &r->compressed_output;
    if (Snappy_Compress(raw.data(), raw.size(), compressed) &&
        compressed->size() < raw.size() - (raw.size() / 8u)) {
      block_contents = *compressed;
    } else {
      // Snappy not supported, or compressed less than 12.5%, so just
      // store uncompressed form
      block_contents = raw;
      type = kNoCompression;
    }
    break;
  }
  }
  WriteRawBlock(block_contents, type, handle);
  r->compressed_output.clear();
  block->Reset();
}

void TableBuilder::WriteRawBlock(const Slice &block_contents,
                                 CompressionType type, BlockHandle *handle) {
  Rep *r = rep_;
  handle->set_offset(r->offset);
  handle->set_size(block_contents.size());
  r->status = r->file->Append(block_contents);
  if (r->status.ok()) {
    char trailer[kBlockTrailerSize];
    trailer[0] = type;
    uint32_t crc = crc32c::Value(block_contents.data(), block_contents.size());
    crc = crc32c::Extend(crc, trailer, 1); // Extend crc to cover block type
    EncodeFixed32(trailer + 1, crc32c::Mask(crc));
    r->status = r->file->Append(Slice(trailer, kBlockTrailerSize));
    if (r->status.ok()) {
      r->offset += block_contents.size() + kBlockTrailerSize;
    }
  }
}

Status TableBuilder::status() const { return rep_->status; }

Status TableBuilder::Finish(uint64_t creation_time, uint64_t oldest_key_time) {
  Rep *r = rep_;
  Flush();
  assert(!r->closed);
  r->closed = true;

  BlockHandle filter_block_handle, metaindex_block_handle, index_block_handle,
      prop_block_handle;

  // Write filter block
  if (ok() && r->filter_block != NULL) {
    WriteRawBlock(r->filter_block->Finish(), kNoCompression,
                  &filter_block_handle);
  }
  if (ok() && r->prop_block != NULL) {
    TableProperties props;
    props.column_family_name = r->columnfamily_name;
    props.column_family_id = r->columnfamily_id;
    props.compression_name = CompressionTypeToString(r->options.compression);
    props.merge_operator_name = "nullptr";
    props.comparator_name = "nullptr";
    props.raw_key_size = r->raw_key_size;
    props.raw_value_size = r->raw_value_size;
    props.num_entries = r->num_entries;
    props.num_data_blocks = r->num_data_blocks;
    props.data_size = r->data_size;
    props.creation_time = creation_time;
    props.oldest_key_time = oldest_key_time;
    r->prop_block->AddTableProperty(props);
    WriteRawBlock(r->prop_block->ProperityBlockFinish(), kNoCompression,
                  &prop_block_handle);
  }
  // Write metaindex block
  if (ok()) {
    BlockBuilder meta_index_block(&r->options);
    if (r->filter_block != NULL) {
      // Add mapping from "filter.Name" to location of filter data
      std::string key = "filter.";
      key.append(r->options.filter_policy->Name());
      std::string handle_encoding;
      filter_block_handle.EncodeTo(&handle_encoding);
      meta_index_block.Add(key, handle_encoding);
    }
    if (r->prop_block != NULL) {
      std::string prop_handle_encoding;
      prop_block_handle.EncodeTo(&prop_handle_encoding);
      meta_index_block.Add(sPropertiesBlock, prop_handle_encoding);
    }
    // TODO(postrelease): Add stats and other meta blocks
    WriteBlock(&meta_index_block, &metaindex_block_handle);
  }

  // Write index block
  if (ok()) {
    if (r->pending_index_entry) {
      r->options.comparator->FindShortSuccessor(&r->last_key);
      std::string handle_encoding;
      r->pending_handle.EncodeTo(&handle_encoding);
      r->index_block.Add(r->last_key, Slice(handle_encoding));
      r->pending_index_entry = false;
    }
    WriteBlock(&r->index_block, &index_block_handle);
  }

  // Write footer
  if (ok()) {
    Footer footer;
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    std::string footer_encoding;
    footer.EncodeTo(&footer_encoding);
    r->status = r->file->Append(footer_encoding);
    if (r->status.ok()) {
      r->offset += footer_encoding.size();
    }
  }
  return r->status;
}

void TableBuilder::Abandon() {
  Rep *r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t TableBuilder::NumEntries() const { return rep_->num_entries; }

uint64_t TableBuilder::FileSize() const { return rep_->offset; }

uint64_t TableBuilder::CurFileSize() const { return rep_->raw_value_size + rep_->raw_value_size;}
} // namespace shannon
