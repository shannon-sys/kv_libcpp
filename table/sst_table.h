#ifndef STORAGE_SHANNONDB_INCLUDE_SST_H_
#define STORAGE_SHANNONDB_INCLUDE_SST_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "venice_macro.h"
#include "swift/slice.h"
#include "swift/status.h"
#include "swift/options.h"
#include "swift/table.h"
#include "swift/shannon_db.h"
#include "column_family.h"

namespace shannon {

#define DEBUG_CONFIG
#ifdef DEBUG_CONFIG
#define DEBUG(format, arg...) \
  printf("%s(), line=%d: " format, __FUNCTION__, __LINE__, ##arg)
#else
#define DEBUG(format, arg...)
#endif /* end of #ifdef DEBUG_CONFIG */

enum {
  kTypeDeletion = 0, // value type
  kTypeValue = 1
};

struct KvNode {
  char *key;
  char *value;
  uint32_t key_len;
  uint32_t value_len;
  uint64_t sequence;
  uint32_t type;
};

/*** footer ***/
// (64 + (7 - 1)) / 7 = 10
enum {
  kMaxEncodedLenBlockHandle = 20,
  kEncodedLenFoot = 48, // 2 * 20 + 8 leveldb
  kNewVersionEncodedLenFoot = 53, // 1 + 20 *2 + 4 +8 rocksdb
  kBlockTrailerSize = 5  // 1type + 4B CRC
};
#define kLegacyTableMagicNumber 0xdb4775248b80fb57ull // leveldb
#define kBasedTableMagicNumber 0x88e241b785f4cff7ull //rocksdb

struct BlockHandle {
  uint64_t offset;
  uint64_t size;
};

struct Foot {
  uint8_t checksum_type; // checksum_type
  BlockHandle metaindex_handle;
  BlockHandle index_handle;
  uint32_t version;
  int legacy_footer_format;
};

class ColumnFamilyHandle;
class WriteBatchNonatomic;

struct DatabaseOptions {
  DB *db;
  WriteBatchNonatomic *wb;
  WriteOptions write_opt;
  ColumnFamilyHandle *cf;
  uint8_t end; // write kvs to ssd at last
};

#define kPropertiesBlock "rocksdb.properties"
// Old property block name for backward compatibility
#define kPropertiesBlockOldName "rocksdb.stats"
#define kMetaNums 1

#define kColumnFamilyName "rocksdb.column.family.name"
#define kIndexType "rocksdb.block.based.table.index.type"
#define kProperties 2
struct MetaBlockProperties {
  char cf_name[CF_NAME_LEN + 1]; // add other property in future
  uint8_t index_type;
};

struct BlockRep {
  uint32_t restart_nums;
  uint32_t *restart_offset;
  uint32_t data_size; // except restarts_offset and restart_num
  Slice block; // uncompressed content
  char *filename;
  uint8_t checksum_type;
  uint8_t last_data_block;
  MetaBlockProperties *props; // for property meta block

  DatabaseOptions *opt; // write kv to ssd
  uint32_t kv_put_count;
  uint32_t kv_del_count;
  uint32_t data_block_count; // used by index_block
};

extern Status BlockHandleDecodeFrom(BlockHandle *dst, Slice *input);
extern Status FootDecodeFrom(Foot *dst, Slice *input, int verify);
extern Status ReadFoot(Slice *result, char *filename);
extern Status CheckAndUncompressBlock(Slice *result, Slice *input, uint8_t checksum_type);
extern Status ReadBlock(Slice *result, char *filename, BlockHandle *handle, uint8_t checksum_type);

/*** data block ***/
extern Status GetDataBlockRestartOffset(BlockRep *rep);
extern Status ProcessOneKv(BlockRep *rep, KvNode *kv);
extern Status DecodeDataBlockRestartInterval(BlockRep *rep, int index);
extern Status DecodeDataBlock(BlockRep *rep);

/*** index block ***/
extern Status GetIndexBlockRestartOffset(BlockRep *rep);
extern Status ProcessOneBlockHandle(BlockRep *rep, KvNode *kv);
extern Status DecodeIndexBlockRestartInterval(BlockRep *rep, int index);
extern Status DecodeIndexBlock(BlockRep *rep);

/** meata block **/
extern Status DecodePropertyBlock(BlockRep *rep);
extern Status DecodeMetaIndexBlock(BlockRep *rep);
extern Status ProcessMetaIndex(char *filename, MetaBlockProperties *props, Foot *foot);

extern Status FindCFHandleIndex(std::vector<ColumnFamilyHandle *> *handles, char *name, int name_len, int *index);

extern Status AnalyzeSst(char *filename, int verify, DB *db,
                std::vector<ColumnFamilyHandle *> *handles);

}  // namespace shannon

#endif  // STORAGE_SHANNONDB_INCLUDE_SST_H_
