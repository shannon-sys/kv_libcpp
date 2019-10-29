#ifndef __SHANNON_DB_H__
#define __SHANNON_DB_H__
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/types.h>

#define CF_NAME_LEN                   32
#define DB_NAME_LEN                   32
#define MAX_DATABASE_COUNT            16
#define MAX_CF_COUNT                  16
#define MAX_CHECKPOINT_COUNT          16
#define MAX_SNAPSHOT_COUNT            256
#define MAX_KEY_SIZE                  128
#define BLOCK_SIZE                    4096
#define MAX_VALUE_SIZE                (4UL << 20)
#define MAX_BATCH_SIZE                (8UL << 20)
#define MAX_BATCH_NONATOMIC_SIZE      (32UL << 20)

enum error_type {
	KVERR_BASE       = 500,
	KVERR_NO_KEY,
	KVERR_OVERSIZE_KEY,
	KVERR_OVERSIZE_VALUE,
	KVERR_MALLOC_FAIL,
	KVERR_BATCH_FULL,
	KVERR_BATCH_INVALID,
	KVERR_INVALID_PARAMETER,
};

typedef struct shannon_db {
	int dev_fd;
	int db_index;
	char dbname[32];
} shannon_db_t;

typedef struct shannon_cf_handle {
	int db_index;
	int cf_index;
	char cf_name[32];
} shannon_cf_handle_t;

typedef struct db_options {
	/* default: 1 */
	int create_if_missing;
	/* not supported yet */
	int compression;
} db_options_t;

typedef struct db_readoptions {
	/* default: 1 */
	int fill_cache;
	/* default: 0 */
	int only_read_key;
	__u64 snapshot;
} db_readoptions_t;

typedef struct db_writeoptions {
	/* only support sync=1 */
	int sync;
	/* default: 1 */
	int fill_cache;
} db_writeoptions_t;

typedef struct db_sstoptions {
	int block_restart_data_interval;
	int block_restart_index_interval;
	unsigned long block_size;
	int compression;
	int is_rocksdb;
} db_sstoptions_t;

extern shannon_db_t *shannon_db_open(const struct db_options *opt,
			const char *dev_name, const char *db_name, char **errptr);
extern void shannon_db_close(struct shannon_db *db);
extern void shannon_destroy_db(struct shannon_db *db, char **errptr);
int shannon_db_get_sequence(const char *dev_name, __u64 *sequence, char **err);
int shannon_db_set_sequence(const char *dev_name, __u64 sequence, char **err);

extern struct shannon_cf_handle *shannon_db_create_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err);
extern struct shannon_cf_handle *shannon_db_open_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err);
extern void shannon_db_drop_column_family(struct shannon_db *db, struct shannon_cf_handle *handle, char **err);
extern void shannon_db_column_family_handle_destroy(struct shannon_cf_handle *handle);
extern struct shannon_db *shannon_db_open_column_families(const struct db_options *options,
				const char *dev_name, const char *db_name,
				int num_column_families, const char **column_family_names,
				const struct db_options **column_family_options,
				struct shannon_cf_handle **column_family_handles, char **err);
extern char **shannon_db_list_column_families(const struct db_options *options, const char *dev_name, const char *dbname,
				unsigned int *cf_count, char **err);
extern void shannon_db_list_column_families_destroy(char **cf_list, unsigned int cf_count);

extern void shannon_db_put(struct shannon_db *db, const struct db_writeoptions *opt,
				const char *key, unsigned int key_len,
				const char *value, unsigned int value_len, char **err);
extern void shannon_db_put_cf(struct shannon_db *db, const struct db_writeoptions *options,
			struct shannon_cf_handle *column_family, const char *key, unsigned int keylen,
			const char *val, unsigned int vallen, char **err);
extern int shannon_db_delete(struct shannon_db *db, const struct db_writeoptions *opt,
				const char *key, unsigned int key_len, char **err);
extern int shannon_db_delete_cf(struct shannon_db *db, const struct db_writeoptions *options,
			struct shannon_cf_handle *column_family,
			const char *key, unsigned int keylen, char **err);
/*
 * the value buffer should be allocated before db_get() is invoked.
 * the vaue buffer size has to be set in opt->value_buf_size.
 * if (value_buf_size < value_len), only value_buf_size data is return in the value buffer.
 */
extern int shannon_db_get(struct shannon_db *db, const struct db_readoptions *opt,
				const char *key, unsigned int key_len,
				char *value_buf, unsigned int buf_size, unsigned int *value_len,
				char **err);
extern int shannon_db_get_cf(struct shannon_db *db, const struct db_readoptions *options,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int keylen, char *value_buf, unsigned int buf_size, unsigned int *value_len,
			char **err);
/* return 1: exist, return 0: do not exist */
extern int shannon_db_key_exist(struct shannon_db *db, const struct db_readoptions *opt,
				const char *key, unsigned int key_len, char **err);
extern int shannon_db_key_exist_cf(struct shannon_db *db, const struct db_readoptions *options,
			struct shannon_cf_handle *column_family,
			const char *key, unsigned int keylen, char **err);


/* snapshot */
extern __u64 shannon_db_create_snapshot(struct shannon_db *db, char **err);
extern void shannon_db_release_snapshot(struct shannon_db *db, __u64 snapshot, char **err);
extern void shannon_db_readoptions_set_snapshot(struct db_readoptions *opt, __u64 snap);

/* write batch */
typedef void db_writebatch_t;
extern int writebatch_is_valid(db_writebatch_t *batch);
extern db_writebatch_t *shannon_db_writebatch_create();
extern void shannon_db_writebatch_destroy(db_writebatch_t *batch);
extern void shannon_db_writebatch_clear(db_writebatch_t *batch);
/*
 * only the value buffer address is saved, so different kv in
 * a writebatch can not use the same value buffer.
 */
extern int shannon_db_writebatch_put(db_writebatch_t *batch,
			const char *key, unsigned int key_len, const char *value,
			unsigned int value_len, char **err);
extern int shannon_db_writebatch_put_cf(db_writebatch_t *batch, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val,
			unsigned int vlen, char **err);
extern int shannon_db_writebatch_delete(db_writebatch_t *batch,
			const char *key, unsigned int key_len, char **err);
extern int shannon_db_writebatch_delete_cf(db_writebatch_t *batch,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int klen, char **err);
extern void shannon_db_write(struct shannon_db *db, const struct db_writeoptions *opt,
				db_writebatch_t *batch, char **err);

/* write batch nonatomic */
extern int writebatch_is_valid_nonatomic(db_writebatch_t *batch);
extern db_writebatch_t *shannon_db_writebatch_create_nonatomic();
extern void shannon_db_writebatch_destroy_nonatomic(db_writebatch_t *batch);
extern void shannon_db_writebatch_clear_nonatomic(db_writebatch_t *batch);
extern int shannon_db_writebatch_put_nonatomic(db_writebatch_t *batch,
			const char *key, unsigned int key_len, const char *value,
			unsigned int value_len, char **err);
extern int shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *batch, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val,
			unsigned int vlen, char **err);
extern int shannon_db_writebatch_delete_nonatomic(db_writebatch_t *batch,
			const char *key, unsigned int key_len, char **err);
extern int shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *batch,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int klen, char **err);
extern void shannon_db_write_nonatomic(struct shannon_db *db, const struct db_writeoptions *opt,
				db_writebatch_t *batch, char **err);

/* read batch */
/* the return status of read batch cmd */
#define READBATCH_SUCCESS      0
#define READBATCH_NO_KEY       1
#define READBATCH_DATA_ERR     2
#define READBATCH_VAL_BUF_ERR  3
typedef void db_readbatch_t;
extern db_readbatch_t *shannon_db_readbatch_create();
extern int shannon_db_readbatch_get(db_readbatch_t *batch, const char *key, unsigned int key_len,
			const char *val, unsigned int buf_size, unsigned int *value_len, unsigned int *status, char **err);
extern int shannon_db_readbatch_get_cf(db_readbatch_t *batch, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val_buf, unsigned int buf_size,
			unsigned int *value_len, unsigned int *status, char **err);
extern void shannon_db_read(struct shannon_db *db, const struct db_readoptions *options, db_readbatch_t *batch,
			unsigned int *failed_cmd_count, char **err);
extern void shannon_db_readbatch_destroy(db_readbatch_t *batch);
extern void shannon_db_readbatch_clear(db_readbatch_t *batch);

/* Iterator */
struct shannon_db_iterator {
	struct shannon_db db;
	int cf_index;
	int prefix_len;
	char *err;
	__u64 snapshot;
	int iter_index;
	int valid;
	unsigned char prefix[128];
};

extern struct shannon_db_iterator *shannon_db_create_iterator(struct shannon_db *db, const struct db_readoptions *opt);
extern struct shannon_db_iterator *shannon_db_create_prefix_iterator(struct shannon_db *db, const struct db_readoptions *opt,
								const char *prefix, unsigned int prefix_len);

extern struct shannon_db_iterator *shannon_db_create_iterator_cf(struct shannon_db *db, const struct db_readoptions *options,
					struct shannon_cf_handle *column_family);
extern struct shannon_db_iterator *shannon_db_create_prefix_iterator_cf(struct shannon_db *db, const struct db_readoptions *options,
					struct shannon_cf_handle *column_family, const char *prefix, unsigned int prefix_len);
extern void shannon_db_create_iterators(struct shannon_db *db, const struct db_readoptions *opts,
				struct shannon_cf_handle **column_families,
				struct shannon_db_iterator **iterators, int num_column_families, char **err);
extern void shannon_db_create_prefix_iterators(struct shannon_db *db, const struct db_readoptions *opts,
				struct shannon_cf_handle **column_families,
				struct shannon_db_iterator **iterators, int num_column_families,
				const char *prefix, unsigned int prefix_len, char **err);

extern void shannon_db_iter_destroy(struct shannon_db_iterator *iterator);
extern void shannon_db_free_iter(struct shannon_db_iterator *iterator);
extern void shannon_db_iter_get_error(const struct shannon_db_iterator *iter, char **err);
extern int  shannon_db_iter_valid(const struct shannon_db_iterator *iter);
extern void shannon_db_iter_seek_to_first(struct shannon_db_iterator *iter);
extern void shannon_db_iter_seek_to_last(struct shannon_db_iterator *iter);
extern void shannon_db_iter_seek(struct shannon_db_iterator *iter, const char *key, unsigned int key_len);
extern void shannon_db_iter_seek_for_prev(struct shannon_db_iterator *iter, const char *key, unsigned int key_len);
extern void shannon_db_iter_move_next(struct shannon_db_iterator *iter);
extern void shannon_db_iter_move_prev(struct shannon_db_iterator *iter);
extern void shannon_db_iter_cur_key(struct shannon_db_iterator *iter, char *key, unsigned int buf_size, unsigned int *key_len);
extern void shannon_db_iter_cur_value(struct shannon_db_iterator *iter, char *value, unsigned int buf_size, unsigned int *value_len);
extern void shannon_db_iter_cur_kv(struct shannon_db_iterator *iter, char *key, unsigned int keybuf_size, char *value,
				unsigned int valbuf_size, unsigned int *value_len, unsigned int *key_len, __u64 *timestamp);
/* Options */
extern struct db_options *shannon_db_options_create();
extern void shannon_db_options_destroy(struct db_options *opt);
extern void shannon_db_options_set_create_if_missing(struct db_options *opt, unsigned char v);

/* Read Options */
extern struct db_readoptions *shannon_db_readoptions_create();
extern void shannon_db_readoptions_destroy(struct db_readoptions *opt);
extern void shannon_db_readoptions_set_fill_cache(struct db_readoptions *opt, unsigned char v);
extern void shannon_db_readoptions_set_only_iter_key(struct db_readoptions *opt, unsigned char v);

/* Write options */
extern struct db_writeoptions *shannon_db_writeoptions_create();
extern void shannon_db_writeoptions_set_sync(struct db_writeoptions *opt, unsigned char v);
extern void shannon_db_writeoptions_set_fill_cache(struct db_writeoptions *opt, unsigned char v);
extern void shannon_db_writeoptions_destroy(struct db_writeoptions *opt);

/*Cache*/
void shannon_db_set_cache_size(struct shannon_db *db, unsigned long size, char **err);

/*** analyze sst table and put kv to ssd ***/
extern int shannondb_analyze_sst(char *filename, int verify, char *dev_name, char *db_name);
extern int shannon_db_build_sst(char *devname , struct shannon_db *db, struct db_options *options, struct db_readoptions *roptions, struct db_sstoptions *soptions);
extern struct db_sstoptions *shannon_db_sstoptions_create();
extern void shannon_db_sstoptions_destroy(struct db_sstoptions *opt);
extern void shannon_db_sstoptions_set_compression(struct db_sstoptions *opt, unsigned char v);
extern void shannon_db_sstoptions_set_data_interval(struct db_sstoptions *opt, int interval);
extern void shannon_db_sstoptions_set_is_rocksdb(struct db_sstoptions *opt, unsigned char v);
extern void shannon_db_sstoptions_set_block_size(struct db_sstoptions *opt, unsigned long size);

/* Property */
extern char *shannon_db_property_value(shannon_db_t *db, const char *propname);
extern int shannon_db_property_int(shannon_db_t *db, const char *propname, __u64 *out_val);
extern char *shannon_db_property_value_cf(shannon_db_t *db, shannon_cf_handle_t *column_family, const char *propname);

#endif /* end of __SHANNON_DB_H__ */
