#ifndef __ANALYZE_SST_H
#define __ANALYZE_SST_H

#include "../shannon_db.h"
#include "../venice_macro.h"
#include "util.h"
#define ERR_MSG_MAX_LEN 128
struct status {
	int err_type;
	char msg[ERR_MSG_MAX_LEN];
};
typedef struct status status_t;

struct kv_node {
	char *key;
	char *value;
	uint32_t key_len;
	uint32_t value_len;
	uint64_t sequence;
	uint32_t type;
};
typedef struct kv_node kv_node_t;


/*** footer ***/
// (64 + (7 - 1)) / 7 = 10

enum {
	KMAX_ENCODED_LEN_BLOCKHANDLE = 20,
	KENCODED_LEN_FOOT = 48, // 2 * 20 + 8 leveldb
	KNEW_VERSION_ENCODED_LEN_FOOT = 53, // 1 + 20 *2 + 4 +8 rocksdb
	KBLOCK_TRAILER_SIZE = 5  // 1type + 4B CRC
};
#define KLEGACY_TABLE_MAGIC_NUMBER 0xdb4775248b80fb57ull // leveldb
#define KBASED_TABLE_MAGIC_NUMBER 0x88e241b785f4cff7ull //rocksdb
struct block_handle {
	uint64_t offset;
	uint64_t size;
};
typedef struct block_handle block_handle_t;

struct foot {
	uint8_t checksum_type; // checksum_type
	block_handle_t metaindex_handle;
	block_handle_t index_handle;
	uint32_t version;
	int legacy_footer_format;
};
typedef struct foot foot_t;

/*** data block and index block ***/
struct database_options {
	struct shannon_db *db;
	struct shannon_cf_handle *cf_handle;
	struct db_options *options;
	struct db_writeoptions *woptions;
	db_writebatch_t *wb;
	uint8_t end; // write kvs to ssd at last
	char *db_name;
	char *cf_name;
	char *dev_name;
};
typedef struct database_options database_options_t;

#define KPROPERTIES_BLOCK "rocksdb.properties"
// Old property block name for backward compatibility
#define KPROPERTIES_BLOCK_OLD_NAME "rockdb.stats"
#define KMETA_NUMS 1

#define KCOLUMN_FAMILY_NAME "rocksdb.column.family.name"
#define KINDEX_TYPE "rocksdb.block.based.table.index.type"
#define KPROPERTIES 2
struct meta_block_properties {
	char cf_name[CF_NAME_LEN + 1]; // add other property in future
	uint8_t index_type;
};
typedef struct meta_block_properties meta_block_properties_t;

struct block_rep {
	uint32_t restart_nums;
	uint32_t *restart_offset;
	uint32_t data_size; // except restarts_offset and restart_num
	slice_t block; // uncompressed content
	char *filename;
	uint8_t checksum_type;
	uint8_t last_data_block;
	meta_block_properties_t *props; // for property meta block

	database_options_t *opt; // write kv to ssd
	uint32_t kv_put_count;
	uint32_t kv_del_count;
	uint32_t data_block_count; // used by index_block
};
typedef struct block_rep block_rep_t;

extern int block_handle_decode_from(block_handle_t *dst, slice_t *input);
extern int foot_decode_from(foot_t *dst, slice_t *input, int verify);
extern int read_foot(slice_t *result, char *filename);
extern int check_and_uncompress_block(slice_t *result, slice_t *input, uint8_t checksum_type);
extern int read_block(slice_t *result, char *filename, block_handle_t *handle, uint8_t checksum_type);

/*** data block ***/
extern int get_data_block_restart_offset(block_rep_t *rep);
extern int process_one_kv(block_rep_t *rep, kv_node_t *kv);
extern int decode_data_block_restart_interval(block_rep_t *rep, int index);
extern int decode_data_block(block_rep_t *rep);

/*** index block ***/
extern int get_index_block_restart_offset(block_rep_t *rep);
extern int process_one_block_handle(block_rep_t *rep, kv_node_t *kv);
extern int decode_index_block_restart_interval(block_rep_t *rep, int index);
extern int decode_index_block(block_rep_t *rep);

/*** index block ***/
extern int decode_property_block(block_rep_t *rep);
extern int decode_meta_index_block(block_rep_t *rep);
extern int process_meta_index(char *filename, meta_block_properties_t *props, foot_t *foot);

#endif /* end of __ANALYZE_SST_H */
