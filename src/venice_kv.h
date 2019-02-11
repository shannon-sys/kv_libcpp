#ifndef __VENICE_KV__
#define __VENICE_KV__

#include <linux/types.h>
#include "src/venice_macro.h"

struct uapi_snapshot {
	int db;
	int reserved;
	__u64 timestamp;
};

struct uapi_checkpoint {
	int db;
	int reserved;
	__u64 timestamp;
};

#define CMD_START_MARK	0x1234567890abcdef
struct batch_cmd {
	__u64 watermark;
	__u64 timestamp;
#define DELETE_TYPE      2
#define ADD_TYPE         3
	int cmd_type;
	int cf_index;
	int key_len;
	int value_len;
	char *value;
	char key[0];
};

struct readbatch_cmd {
	__u64 watermark;
#define GET_TYPE         4
	int cmd_type;
	int cf_index;
	int value_buf_size;
	char *key;
	int key_len;
	char *value;
	int value_len;
};

struct write_batch_header {
	unsigned long size;
	unsigned long value_size;
#define MAX_BATCH_COUNT             1000
#define MAX_BATCH_NONATOMIC_COUNT   2000
	int count;
	int db_index;
	int fill_cache;
	int reserved;
	char data[0];
};

struct read_batch_header {
	unsigned long size;
	unsigned long value_size;
#define MAX_READBATCH_COUNT		20000
	int count;
	int db_index;
	int fill_cache;
	int reserved;
	char data[0];
};

struct venice_kv {
	__u64 timestamp;
	int db;
	int cf_index;
	int sync;
	/* used for kv_get command */
	int value_buf_size;
	int fill_cache;
	int key_len;
	int value_len;
	char *key;
	char *value;
	char pad[12];
};

struct uapi_key_status {
	__u64 timestamp;
	int db_index;
	int cf_index;
	int key_len;
	int value_len;
	int exist;
	int reserved;
	char *key;
};

struct uapi_cache_size {
	int db;
	int cf_index;
	unsigned long size;
};

struct uapi_snapshot_list {
	int db;
	int count;
	__u64 timestamp[MAX_SNAPSHOT_COUNT];
};

struct uapi_checkpoint_list {
	int db;
	int count;
	__u64 timestamp[MAX_CHECKPOINT_COUNT];
};

struct uapi_db_status {
	int db_index;
	int cf_count;
	int checkpoint_count;
	int reserved;
	unsigned long total_kv_count;
	unsigned long total_disk_usage;
	unsigned long total_cache_size;
	unsigned long use_cache_size;
};

struct uapi_cf_status {
	int db_index;
	int cf_index;
	int checkpoint_count;
	int reserved;
	unsigned long total_kv_count;
	unsigned long total_disk_usage;
	unsigned long use_cache_size;
	unsigned long total_cache_size;
};

struct uapi_dev_status {
	char tgttype[48];	/* target type name */
	char tgtname[32];		/* dev to expose target as */

	__u64 total_write_bytes;
	__u64 host_read_bytes;
	__u64 host_write_bytes;
	__u64 power_on_seconds;
	__u64 power_cycle_count;
	__u64 cache_read_bytes;
	__u64 reserved1;

	__u32 host_write_bandwidth;	/* KB/s */
	__u32 host_write_iops;

	__u32 host_write_latency;	/* us */
	__u32 total_write_bandwidth;	/* KB/s */

	__u32 host_read_bandwidth;	/* KB/s */
	__u32 host_read_iops;

	__u32 host_read_latency;	/* us */

	__u32 write_amplifier;
	__u32 write_amplifier_lifetime;


	__u64 total_gc_sectors;
	__u64 total_wl_sectors;
	__u64 total_err_recover_sectors;

	__u32 gc_bandwidth;
	__u32 wl_bandwidth;
	__u32 err_recover_bandwidth;

	__u32 overprovision;
	__u64 capacity;
	__u64 physical_capacity;
	__u64 disk_usage;

};

struct uapi_db_handle {
/* if the database doesn't exist, create it */
#define O_DB_CREATE       0x1
	unsigned long flags;
	int db_index;
	int reserved;
	char name[DB_NAME_LEN];
};

struct uapi_db_list {
	int list_all;
	int count;
	struct uapi_db_handle dbs[MAX_DATABASE_COUNT];
};

struct uapi_cf_handle {
	int db_index;
	int cf_index;
	char name[CF_NAME_LEN];
};

struct uapi_cf_list {
	int db_index;
	int cf_count;
	struct uapi_cf_handle cfs[MAX_CF_COUNT];
};

struct uapi_cf_iterator {
	__u64 timestamp;
	unsigned int db_index;
	unsigned int cf_index;
	int iter_index;
	int valid_key;
};

struct uapi_db_iterator {
	__u64 timestamp;
	unsigned int db_index;
	unsigned int count;
	struct uapi_cf_iterator iters[0];
};

struct uapi_iter_seek_option {
	struct uapi_cf_iterator iter;

#define SEEK_FIRST         1
#define SEEK_LAST          2
#define SEEK_KEY           3
#define SEEK_FOR_PREV      4
	int seek_type;
	int reserved;
	int key_len;
	char key[256];
};

struct uapi_iter_move_option {
	struct uapi_cf_iterator iter;
#define MOVE_NEXT     1
#define MOVE_PREV    2
	int move_direction;
};

struct uapi_iter_get_option {
	struct uapi_cf_iterator iter;
#define ITER_GET_KEY	0x1
#define ITER_GET_VALUE	0x2
	unsigned long get_type;
	char *key;
	int key_buf_len;
	int key_len;
	char *value;
	int value_buf_len;
	int value_len;
	__u64 timestamp;
};

struct uapi_ts_get_option {
	__u64 timestamp;
#define GET_DEV_CUR_TIMESTAMP       1
	unsigned long get_type;
};

struct uapi_ts_set_option {
	__u64 timestamp;
#define SET_DEV_CUR_TIMESTAMP       1
	unsigned long set_type;
};

struct uapi_log_iterator {
	int iter_index;
	int valid_iter;
	__u64 iter_sequence;
};

struct uapi_log_iter_create {
	struct uapi_log_iterator iter;
	__u64 timestamp;
};

struct uapi_log_iter_move_option {
	struct uapi_log_iterator iter;
	__u64 timestamp;
#define LOG_MOVE_NEXT     0x01
	int move_direction;
	int valid_key;
};

struct uapi_log_iter_get_option {
	struct uapi_log_iterator iter;
	__u64 timestamp;
#define LOG_ITER_GET_KEY     0x01
#define LOG_ITER_GET_VALUE   0x02
	unsigned char get_type;
	unsigned char valid_key;
	unsigned char db_index;
	unsigned char cf_index;
#define LOG_ADD_KEY          0x01
#define LOG_DELETE_KEY       0x02
#define LOG_CREATE_DB        0x03
#define LOG_DELETE_DB        0x04
	unsigned char optype;
	unsigned char reserved[3];
	int key_buf_len;
	int key_len;
	char *key;
	int value_buf_len;
	int value_len;
	char *value;
};

struct uapi_test_mtable {
	unsigned char name[32];
	int type;
	int db_index;
	int cf_index;
	int key_len;
	int value_len;
	__u64 pba;
	__u64 timestamp;
	char *key;
};

struct uapi_test_mtable_iter {
	unsigned char mt_name[32];
	int fd;
	int iter_index;
	int valid_key;
};

struct uapi_miter_move_option {
	struct uapi_test_mtable_iter iter;
	int move_direction;
};

struct uapi_miter_seek_option {
	struct uapi_test_mtable_iter iter;
	int seek_type;
	int key_len;
	char key[256];
};

struct uapi_miter_get_option {
	struct uapi_test_mtable_iter iter;
	int db_index;
	int cf_index;
	int key_len;
	int value_len;
	__u64 pba;
	__u64 timestamp;
	char key[256];
};

#endif /* end of __VENICE_KV__ */
