#ifndef SHANNONDB_INCLUDE_TABLE_BUILDER_H_
#define SHANNONDB_INCLUDE_TABLE_BUILDER_H_

#include "block_builder.h"
#include "../shannon_db.h"

struct filter_block_builder{
	int op;
};
struct sst_file_writer{
	int  cur_db_index;
	int  cur_cf_index;
	char cur_db_name[NAME_LEN];
	char cur_cf_name[NAME_LEN];
	int  num_cf;
	int  num_db;
	struct shannon_db_iterator *cur_cf_iter;
	char **column_family_names;
	struct shannon_db_iterator **iters_handles;
	char dev_name[NAME_LEN];
};

struct table_rep {
	char  file_size;
	uint64_t offset;
	uint64_t cur_offset;
	struct block_builder *data_block;
	struct index_block *index_block;
	struct index_block *meta_index_block;
	struct properities_block *prop_block;
	char *last_key;
	struct filter_block *filter_block;
	struct block_handle *pending_handle;
	struct filter_policy *filter_policy;
	int64_t num_entries;
	int64_t data_block_count;
	bool closed;
	int is_rocksdb;
	bool pending_index_entry;
	uint64_t raw_value_size;
	uint64_t raw_key_size;
	uint64_t column_family_id;
	char column_family_name[NAME_LEN];
	char filter_policy_name[NAME_LEN];
	int block_restart_data_interval;
	int block_restart_index_interval;
	int block_size;
	int compression;
	int verify;
	uint64_t creation_time;
	uint64_t oldest_key_time;
	char  filename[100];
};

extern int write_block(struct table_rep *rep, struct block_builder *data_builder, struct block_handle *handle);
extern slice_t *block_builder_finish(struct block_builder *block);
extern void shannon_db_sstfilewriter_finish(struct table_rep *rep);
extern int shannon_db_sstfilewriter_add(struct table_rep *rep, char *key, size_t keylen, char *val, size_t vallen, uint64_t time, int type);
extern struct table_rep  *shannon_db_sstfilewriter_create(char *db_name, char *cf_name, int cf_index, struct db_sstoptions *option);

#endif
