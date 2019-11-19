#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "venice_kv.h"
#include "venice_ioctl.h"
#include "shannon_db.h"

char *const kvdb_err_msg[] = {
	"normal",
	"no such key",
	"key is too long",
	"value_len is too big",
	"malloc failed",
	"batch command is full",
	"batch is invalid",
	"invalid parameter",
};

#define set_err_and_return(err, v)	do { if (err) *err = v; return; } while (0)

struct shannon_db *shannon_db_open(const struct db_options *db_opt, const char *dev_name, const char *db_name, char **err)
{
	struct shannon_db *db;
	int cf_cnt = 1;
	const char *cf_name = "default";
	const struct db_options *cf_option = shannon_db_options_create();
	struct shannon_cf_handle *cf_handle = NULL;
	*err = NULL;
	db = shannon_db_open_column_families(db_opt, dev_name, db_name, cf_cnt, &cf_name, &cf_option, &cf_handle, err);
	return db;
}

struct shannon_db *shannon_db_open_column_families(const struct db_options *db_opt,
		const char *dev_name, const char *db_name,
		int cf_num, const char **cf_names,
		const struct db_options **cf_opts,
		struct shannon_cf_handle **cf_hds, char **err)
{
	int i;
	int dev_fd, ret;
	struct shannon_db *db = NULL;
	struct uapi_db_handle kv_hd;
	struct uapi_cf_handle kv_cf_hd;
	char *p_err;

	if (db_opt == NULL || dev_name == NULL || db_name == NULL || cf_names == NULL || cf_opts == NULL || cf_hds == NULL || err == NULL)
		return NULL;
	*err = NULL;

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		p_err = strerror(errno);
		printf("%s open kvdev failed:%s\n", __FUNCTION__, p_err);
		*err = p_err;
		return NULL;
	}

	memset(&kv_hd, 0, sizeof(struct uapi_db_handle));
	strcpy(kv_hd.name, db_name);
	kv_hd.flags = db_opt->create_if_missing;
	ret = ioctl(dev_fd, OPEN_DATABASE, &kv_hd);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s open database failed:%s\n", __FUNCTION__, p_err);
		*err = p_err;
		goto failed;
	}

	db = malloc(sizeof(struct shannon_db));
	if (db == NULL) {
		p_err = kvdb_err_msg[KVERR_MALLOC_FAIL - KVERR_BASE];
		*err = p_err;
		goto failed;
	}
	db->dev_fd = dev_fd;
	db->db_index = kv_hd.db_index;
	strcpy(db->dbname, db_name);

	for (i = 0; i <= cf_num-1; ++i) {
		memset(&kv_cf_hd, 0, sizeof(struct uapi_cf_handle));
		strcpy(kv_cf_hd.name, cf_names[i]);
		kv_cf_hd.db_index = db->db_index;

		ret = ioctl(db->dev_fd, OPEN_COLUMNFAMILY, &kv_cf_hd);
		if (ret < 0) {
			p_err = strerror(errno);
			printf("%s ioctl failed:%s\n", __FUNCTION__, p_err);
			*err = p_err;
			goto failed;
		}

		cf_hds[i] = malloc(sizeof(struct shannon_cf_handle));
		cf_hds[i]->db_index = db->db_index;
		cf_hds[i]->cf_index = kv_cf_hd.cf_index;
		strcpy(cf_hds[i]->cf_name, cf_names[i]);
	}
	return db;
failed:
	if (db)
		shannon_db_close(db);
	return NULL;
}

void shannon_db_close(struct shannon_db *db)
{
	if (db != NULL && db->dev_fd)
		close(db->dev_fd);
	if (db)
		free(db);
	return;
}

void shannon_destroy_db(struct shannon_db *db, char **err)
{
	struct uapi_db_handle handle;
	char *p_err;
	int ret = 0;

	if (db == NULL)
		return;
	*err = NULL;

	memset(&handle, 0, sizeof(handle));
	handle.db_index = db->db_index;
	strcpy(handle.name, db->dbname);
	ret = ioctl(db->dev_fd, REMOVE_DATABASE, &handle);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s remove_database ioctl failed.\n", __FUNCTION__);
		*err = p_err;
	}
}

int shannon_db_get_sequence(const char *dev_name, __u64 *sequence, char **err)
{
	int ret = 0, dev_fd;
	struct uapi_ts_get_option option;
	char *p_err;
	*sequence = 0xffffffffffffffff;
	*err = NULL;

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		p_err = strerror(errno);
		ret = -errno;
		printf("%s open kvdev failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
		return ret;
	}

	memset(&option, 0, sizeof(option));
	option.get_type = GET_DEV_CUR_TIMESTAMP;
	ret = ioctl(dev_fd, IOCTL_GET_TIMESTAMP, &option);
	if (ret < 0) {
		p_err = strerror(errno);
		ret = -errno;
		printf("%s ioctl failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
		close(dev_fd);
		return ret;
	}
	*sequence = option.timestamp;
	close(dev_fd);
	return ret;
}

int shannon_db_set_sequence(const char *dev_name, __u64 sequence, char **err)
{
	int ret = 0, dev_fd;
	struct uapi_ts_set_option option;
	char *p_err;
	*err = NULL;

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		p_err = strerror(errno);
		ret = -errno;
		printf("%s open kvdev failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
		return ret;
	}

	memset(&option, 0, sizeof(option));
	option.set_type = SET_DEV_CUR_TIMESTAMP;
	option.timestamp = sequence;
	ret = ioctl(dev_fd, IOCTL_SET_TIMESTAMP, &option);
	if (ret < 0) {
		p_err = strerror(errno);
		ret = -errno;
		printf("%s ioctl failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
	}
	close(dev_fd);
	return ret;
}

/*options*/
struct db_options *shannon_db_options_create()
{
	struct db_options *options;
	options = malloc(sizeof(struct db_options));
	if (!options) {
		printf("%s malloc failed.\n", __FUNCTION__);
		return NULL;
	}
	memset(options, 0, sizeof(struct db_options));
	options->create_if_missing = 1;
	options->compression = 0;
	return options;
}

void shannon_db_options_destroy(struct db_options *options)
{
	if (options)
		free(options);
}

void shannon_db_options_set_create_if_missing(struct db_options *opt, unsigned char v)
{
	opt->create_if_missing = v;
}

/*read options*/
struct db_readoptions *shannon_db_readoptions_create()
{
	struct db_readoptions *options;
	options = malloc(sizeof(struct db_readoptions));
	if (!options) {
		printf("%s malloc failed.\n", __FUNCTION__);
		return NULL;
	}
	memset(options, 0, sizeof(struct db_readoptions));
	options->fill_cache = 1;
	return options;
}

void shannon_db_readoptions_destroy(struct db_readoptions *options)
{
	if (options)
		free(options);
}

void shannon_db_readoptions_set_fill_cache(struct db_readoptions *opt, unsigned char v)
{
	opt->fill_cache = v;
}

void shannon_db_readoptions_set_only_iter_key(struct db_readoptions *opt, unsigned char v)
{
	opt->only_read_key = v;
}

void shannon_db_readoptions_set_snapshot(struct db_readoptions *opt, __u64 snap)
{
	opt->snapshot = snap;
}

/*write options*/
struct db_writeoptions *shannon_db_writeoptions_create()
{
	struct db_writeoptions *options;
	options = malloc(sizeof(struct db_writeoptions));
	if (!options) {
		printf("%s malloc failed.\n", __FUNCTION__);
		return NULL;
	}
	memset(options, 0, sizeof(struct db_writeoptions));
	options->sync = 1;
	options->fill_cache = 1;
	return options;
}

void shannon_db_writeoptions_destroy(struct db_writeoptions *options)
{
	if (options)
		free(options);
}

void shannon_db_writeoptions_set_sync(struct db_writeoptions *opt, unsigned char v)
{
	opt->sync = v;
}

void shannon_db_writeoptions_set_fill_cache(struct db_writeoptions *opt, unsigned char v)
{
	opt->fill_cache = v;
}

/*basic function*/
void shannon_db_put(struct shannon_db *db, const struct db_writeoptions *w_opt,
		const char *key, unsigned int key_len, const char *val, unsigned int val_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	shannon_db_put_cf(db, w_opt, &s_cf_hd, key, key_len, val, val_len, err);
}

int shannon_db_get(struct shannon_db *db, const struct db_readoptions *r_opt,
		const char *key, unsigned int key_len, char *val_buf, unsigned int buf_size,
		unsigned int *val_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_get_cf(db, r_opt, &s_cf_hd, key, key_len, val_buf, buf_size, val_len, err);
}

int shannon_db_key_exist(struct shannon_db *db, const struct db_readoptions *r_opt,
		const char *key, unsigned int key_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_key_exist_cf(db, r_opt, &s_cf_hd, key, key_len, err);
}

int shannon_db_delete(struct shannon_db *db, const struct db_writeoptions *w_opt,
		const char *key, unsigned int key_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_delete_cf(db, w_opt, &s_cf_hd, key, key_len, err);
}

int writebatch_is_valid(db_writebatch_t *bat)
{
	struct write_batch_header *batch_header = (struct write_batch_header *)bat;
	if ((batch_header->size <= sizeof(struct write_batch_header)) || (batch_header->value_size < 0) ||
		(batch_header->size + batch_header->value_size > MAX_BATCH_SIZE) ||
		(batch_header->count > MAX_BATCH_COUNT) || (batch_header->count <= 0))
		return 0;
	else
		return 1;
}

int writebatch_is_valid_nonatomic(db_writebatch_t *bat)
{
	struct write_batch_header *batch_header = (struct write_batch_header *)bat;
	if ((batch_header->size <= sizeof(struct write_batch_header)) || (batch_header->value_size < 0) ||
		(batch_header->size + batch_header->value_size > MAX_BATCH_NONATOMIC_SIZE) ||
		(batch_header->count > MAX_BATCH_NONATOMIC_COUNT) || (batch_header->count <= 0))
		return 0;
	else
		return 1;
}

void shannon_db_write(struct shannon_db *db, const struct db_writeoptions *options,
	db_writebatch_t *bat, char **err)
{
	int fd, ret;
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	char *p;

	*err = NULL;
	if (!writebatch_is_valid(bat)) {
		p = kvdb_err_msg[KVERR_BATCH_INVALID - KVERR_BASE];
		printf("%s writebatch is not valid, size=%ld count=%d\n", __FUNCTION__, batch->size, batch->count);
		set_err_and_return(err, p);
	}
	batch->db_index = db->db_index;
	batch->fill_cache = options->fill_cache;
	ret = ioctl(db->dev_fd, WRITE_BATCH, batch);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl writebatch failed .\n", __FUNCTION__);
		set_err_and_return(err, p);
	}
	set_err_and_return(err, NULL);
}

void shannon_db_write_nonatomic(struct shannon_db *db, const struct db_writeoptions *options,
	db_writebatch_t *bat, char **err)
{
	int fd, ret;
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	char *p;

	*err = NULL;
	if (!writebatch_is_valid_nonatomic(bat)) {
		p = kvdb_err_msg[KVERR_BATCH_INVALID - KVERR_BASE];
		printf("%s writebatch nonatomic is not valid, size=%ld count=%d\n",
				__FUNCTION__, batch->size, batch->count);
		set_err_and_return(err, p);
	}
	batch->db_index = db->db_index;
	batch->fill_cache = options->fill_cache;
	ret = ioctl(db->dev_fd, WRITE_BATCH_NONATOMIC, batch);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl writebatch nonatomic failed .\n", __FUNCTION__);
		set_err_and_return(err, p);
	}
	set_err_and_return(err, NULL);
}

/*snapshot*/
__u64 shannon_db_create_snapshot(struct shannon_db *db, char **err)
{
	struct uapi_snapshot opt;
	int ret = 0;
	char *p;

	*err = NULL;
	memset(&opt, 0, sizeof(struct uapi_snapshot));
	opt.db = db->db_index;
	ret = ioctl(db->dev_fd, CREATE_SNAPSHOT, &opt);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl failed .\n", __FUNCTION__);
		*err = p;
		return (__u64)0;
	}
	return opt.snapshot_id;
}

void shannon_db_release_snapshot(struct shannon_db *db, __u64 snapshot_id, char **err)
{
	int fd, ret;
	struct uapi_snapshot opt;
	char *p;

	*err = NULL;
	memset(&opt, 0, sizeof(struct uapi_snapshot));
	opt.db = db->db_index;
	opt.snapshot_id = snapshot_id;
	ret = ioctl(db->dev_fd, RELEASE_SNAPSHOT, &opt);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl failed  time=%lld.\n", __FUNCTION__, snapshot_id);
		set_err_and_return(err, p);
	}
	set_err_and_return(err, NULL);
}

/*writebatch*/
db_writebatch_t *shannon_db_writebatch_create()
{
	db_writebatch_t *batch;
	struct write_batch_header *bat;
	batch = malloc(MAX_BATCH_SIZE);
	if (!batch) {
		perror("shannon_db_writebatch_create malloc batch failed!!");
		return NULL;
	}
	bat = (struct write_batch_header *)batch;
	bat->size = sizeof(struct write_batch_header);
	bat->value_size = 0;
	bat->count = 0;
	bat->fill_cache = 1;
	return (db_writebatch_t *)bat;
}

void shannon_db_writebatch_destroy(db_writebatch_t *batch)
{
	if (batch)
		free(batch);
}

void shannon_db_writebatch_clear(db_writebatch_t *batch)
{
	struct write_batch_header *batch_header;
	batch_header = (struct write_batch_header *)batch;
	batch_header->size = sizeof(struct write_batch_header);
	batch_header->value_size = 0;
	batch_header->count = 0;
	batch_header->fill_cache = 1;
}

int shannon_db_writebatch_put(db_writebatch_t *bat, const char *key, unsigned int key_len,
		const char *val, unsigned int val_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_writebatch_put_cf(bat, &s_cf_hd, key, key_len, val, val_len, err);
}

int shannon_db_writebatch_delete(db_writebatch_t *bat, const char *key, unsigned int key_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_writebatch_delete_cf(bat, &s_cf_hd, key, key_len, err);
}

/* writebatch nonatomic */
db_writebatch_t *shannon_db_writebatch_create_nonatomic()
{
	db_writebatch_t *batch;
	struct write_batch_header *bat;
	batch = malloc(MAX_BATCH_NONATOMIC_SIZE);
	if (!batch) {
		perror("writebatch_create_nonatomic malloc batch failed!!");
		return NULL;
	}
	bat = (struct write_batch_header *)batch;
	bat->size = sizeof(struct write_batch_header);
	bat->value_size = 0;
	bat->count = 0;
	bat->fill_cache = 1;
	return (db_writebatch_t *)bat;
}

void shannon_db_writebatch_destroy_nonatomic(db_writebatch_t *batch)
{
	if (batch)
		free(batch);
}

void shannon_db_writebatch_clear_nonatomic(db_writebatch_t *batch)
{
	struct write_batch_header *batch_header;
	batch_header = (struct write_batch_header *)batch;
	batch_header->size = sizeof(struct write_batch_header);
	batch_header->value_size = 0;
	batch_header->count = 0;
	batch_header->fill_cache = 1;
}

int __shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *bat, struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, const char *val,
		unsigned int val_len, __u64 timestamp, char **err);
int __shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *bat,
		struct shannon_cf_handle *s_cf_hd, const char *key,
		unsigned int key_len, __u64 timestamp, char **err);

int __shannon_db_writebatch_put_nonatomic(db_writebatch_t *bat, const char *key,
		unsigned int key_len, const char *val, unsigned int val_len, __u64 timestamp, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return __shannon_db_writebatch_put_cf_nonatomic(bat, &s_cf_hd, key, key_len, val, val_len, timestamp, err);
}

int __shannon_db_writebatch_delete_nonatomic(db_writebatch_t *bat, const char *key,
		unsigned int key_len, __u64 timestamp, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return __shannon_db_writebatch_delete_cf_nonatomic(bat, &s_cf_hd, key, key_len, timestamp, err);
}

int shannon_db_writebatch_put_nonatomic(db_writebatch_t *bat, const char *key,
		unsigned int key_len, const char *val, unsigned int val_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_writebatch_put_cf_nonatomic(bat, &s_cf_hd, key, key_len, val, val_len, err);
}

int shannon_db_writebatch_delete_nonatomic(db_writebatch_t *bat, const char *key, unsigned int key_len, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_writebatch_delete_cf_nonatomic(bat, &s_cf_hd, key, key_len, err);
}

/*Cache*/
void shannon_db_set_cache_size(struct shannon_db *db, unsigned long size, char **err)
{
	struct uapi_cache_size cache;
	int fd, ret = 0;
	unsigned long index;
	char *p;

	*err = NULL;
	memset(&cache, 0, sizeof(struct uapi_cache_size));
	cache.db = db->db_index;
	cache.size = size;
	ret = ioctl(db->dev_fd, SET_CACHE, &cache);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s  ioctl failed .\n", __FUNCTION__);
		set_err_and_return(err, p);
	}
	set_err_and_return(err, NULL);
}

/*iterator*/
struct shannon_db_iterator *shannon_db_create_iterator(struct shannon_db *db, const struct db_readoptions *r_opt)
{
	struct shannon_db_iterator *iter_ret = NULL;
	struct shannon_cf_handle s_cf_hd;

	if (db == NULL || r_opt == NULL)
		return NULL;

	s_cf_hd.db_index = db->db_index;
	s_cf_hd.cf_index = 0;
	iter_ret = shannon_db_create_iterator_cf(db, r_opt, &s_cf_hd);
	return iter_ret;
}

void shannon_db_iter_get_error(const struct shannon_db_iterator *iter, char **err)
{
	set_err_and_return(err, iter->err);
}

void shannon_db_noprefix_iter_seek_to_first(struct shannon_db_iterator *iter)
{
	struct uapi_iter_seek_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_seek_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.cf_index = iter->cf_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.seek_type = SEEK_FIRST;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_SEEK, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_noprefix_iter_seek_to_last(struct shannon_db_iterator *iter)
{
	struct uapi_iter_seek_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_seek_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.cf_index = iter->cf_index;
	option.iter.iter_index = iter->iter_index;
	option.seek_type = SEEK_LAST;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_SEEK, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_iter_seek(struct shannon_db_iterator *iter, const char *key, unsigned int key_len)
{
	struct uapi_iter_seek_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_seek_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.cf_index = iter->cf_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.seek_type = SEEK_KEY;
	option.key_len = key_len;
	memcpy(option.key, key, key_len);
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_SEEK, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_iter_seek_for_prev(struct shannon_db_iterator *iter, const char *key, unsigned int key_len)
{
	struct uapi_iter_seek_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_seek_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.cf_index = iter->cf_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.seek_type = SEEK_FOR_PREV;
	option.key_len = key_len;
	memcpy(option.key, key, key_len);
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_SEEK, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_noprefix_iter_move_next(struct shannon_db_iterator *iter)
{
	struct uapi_iter_move_option option;
	int ret = 0;
	int dev_fd;
	char *p;
	char key[256];

	memset(&option, 0, sizeof(struct uapi_iter_move_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.cf_index = iter->cf_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.move_direction = MOVE_NEXT;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_MOVE, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_noprefix_iter_move_prev(struct shannon_db_iterator *iter)
{
	struct uapi_iter_move_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_move_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.iter.cf_index = iter->cf_index;
	option.move_direction = MOVE_PREV;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_MOVE, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
}

void shannon_db_iter_cur_key(struct shannon_db_iterator *iter, char *key, unsigned int buf_size, unsigned int *key_len)
{
	struct uapi_iter_get_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_get_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.iter.cf_index = iter->cf_index;
	option.key = key;
	option.key_buf_len = buf_size;
	option.get_type = ITER_GET_KEY;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_GET, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
	*key_len = option.key_len;
}

void shannon_db_iter_cur_value(struct shannon_db_iterator *iter, char *value, unsigned int buf_size, unsigned int *value_len)
{
	struct uapi_iter_get_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_get_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.iter.cf_index = iter->cf_index;
	option.value = value;
	option.value_buf_len = buf_size;
	option.get_type = ITER_GET_VALUE;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_ITERATOR_GET, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
	*value_len = option.value_len;
}

void shannon_db_iter_cur_kv(struct shannon_db_iterator *iter, char *key, unsigned int keybuf_size, char *value, unsigned int valbuf_size, unsigned int *value_len, unsigned int *key_len, __u64 *timestamp)
{
	struct uapi_iter_get_option option;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&option, 0, sizeof(struct uapi_iter_get_option));
	option.iter.db_index = iter->db.db_index;
	option.iter.timestamp = iter->snapshot;
	option.iter.iter_index = iter->iter_index;
	option.iter.cf_index = iter->cf_index;
	dev_fd = iter->db.dev_fd;
	option.get_type = ITER_GET_KEY | ITER_GET_VALUE;
	option.key = key;
	option.key_buf_len = keybuf_size;
	option.value = value;
	option.value_buf_len = valbuf_size;

	ret = ioctl(dev_fd, IOCTL_ITERATOR_GET, &option);
	p = strerror(errno);
	iter->valid = option.iter.valid_key;
	if (ret < 0) {
		printf("%s get kv ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
	*value_len = option.value_len;
	*key_len = option.key_len;
	*timestamp = option.timestamp;
}

void shannon_db_iter_destroy(struct shannon_db_iterator *iter)
{
	struct uapi_cf_iterator iterator;
	int ret = 0;
	int dev_fd;
	char *p;

	memset(&iterator, 0, sizeof(struct uapi_db_iterator));
	iterator.db_index = iter->db.db_index;
	iterator.timestamp = iter->snapshot;
	iterator.cf_index = iter->cf_index;
	iterator.iter_index = iter->iter_index;
	dev_fd = iter->db.dev_fd;
	ret = ioctl(dev_fd, IOCTL_DESTROY_ITERATOR, &iterator);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl failed .\n", __FUNCTION__);
		iter->err = p;
	}
	free(iter);
}

int shannon_db_iter_valid(const struct shannon_db_iterator *iter)
{
	if (!iter)
		return 0;
	if (iter->valid == 0)
		return 0;
	return 1;
}

struct shannon_db_iterator *shannon_db_create_prefix_iterator(struct shannon_db *db,
		const struct db_readoptions *r_opt, const char *prefix, unsigned int prefix_len)
{
	struct shannon_db_iterator *iter_ret = NULL;
	struct shannon_cf_handle s_cf_hd;
	char *err = NULL;

	if (db == NULL || r_opt == NULL || prefix == NULL || prefix_len <= 0)
		return NULL;

	s_cf_hd.db_index = db->db_index;
	s_cf_hd.cf_index = 0;
	iter_ret = shannon_db_create_prefix_iterator_cf(db, r_opt, &s_cf_hd, prefix, prefix_len);
	return iter_ret;
}

void shannon_db_iter_seek_to_first(struct shannon_db_iterator *iter)
{
	unsigned char key[256];
	unsigned int key_len = 0;

	if (iter->prefix_len <= 0) {
		shannon_db_noprefix_iter_seek_to_first(iter);
		return;
	}
	shannon_db_iter_seek(iter, iter->prefix, iter->prefix_len);
	shannon_db_iter_cur_key(iter, key, 256, &key_len);
	if (memcmp(iter->prefix, key, iter->prefix_len) != 0 || key_len < iter->prefix_len)
		iter->valid = 0;
}

/**
 * @brief 对prefix数组做加1操作。返回最后的进位carry。
 * 如果返回1，说明prefix数组全是字节0xFF。
 *
 * @param prefix
 * @param len
 * @return int
 */
static int prefix_increase_one(unsigned char *prefix, unsigned int len)
{

	unsigned int carry = 1;
	unsigned int temp;
	int i;

	assert(prefix != NULL && len > 0);

	for (i = len - 1; i >= 0 && carry != 0; i--) {
		temp = ((unsigned int) prefix[i]) + carry;
		prefix[i] = temp & 0xFF;
		carry = (temp >> 8) & 0x1;
	}
	return carry;
}

void shannon_db_iter_seek_to_last(struct shannon_db_iterator *iter)
{
	unsigned char key[256];
	unsigned char prefix_plus[256];
	unsigned int key_len = 0;

	if (iter == NULL)
		return;

	if (iter->prefix_len <= 0) {
		shannon_db_noprefix_iter_seek_to_last(iter);
		return;
	}

	memset(prefix_plus, 0, 256);
	memcpy(prefix_plus, iter->prefix, iter->prefix_len);

	if (prefix_increase_one(prefix_plus, iter->prefix_len)) {
		// 最终有进位，说明字节全是FF
		shannon_db_noprefix_iter_seek_to_last(iter);
		return;
	} else {
		// 若iter->prefix为"AAA"，prefix_plus此时为"AAB"。
		// 以前缀"AAA"寻找last，也就是寻找"AAB"的前一个。
		// seek_for_prev包含hit情况，miss才返回prev。
		shannon_db_iter_seek_for_prev(iter, prefix_plus, iter->prefix_len);
		if (!iter->valid)
			return;

		shannon_db_iter_cur_key(iter, key, 256, &key_len);
		if (memcmp(prefix_plus, key, iter->prefix_len) == 0) {  // hit
			shannon_db_noprefix_iter_move_prev(iter);
			shannon_db_iter_cur_key(iter, key, 256, &key_len);
		}
		// memcmp代价较大，放最后。
		iter->valid = (iter->valid != 0)
						&& (key_len >= iter->prefix_len)
						&& (memcmp(iter->prefix, key, iter->prefix_len) == 0);
	}
}

void shannon_db_iter_move_next(struct shannon_db_iterator *iter)
{
	unsigned char key[256];
	unsigned int key_len = 0;

	if (iter == NULL)
		return;
	shannon_db_noprefix_iter_move_next(iter);
	if (iter->prefix_len <= 0) {
		return;
	}
	shannon_db_iter_cur_key(iter, key, 256, &key_len);
	if (key_len < iter->prefix_len || memcmp(key, iter->prefix, iter->prefix_len) != 0)
		iter->valid = 0;
}

void shannon_db_iter_move_prev(struct shannon_db_iterator *iter)
{
	unsigned char key[256];
	unsigned int key_len = 0;

	if (iter == NULL)
		return;
	shannon_db_noprefix_iter_move_prev(iter);
	if (iter->prefix_len <= 0) {
		return;
	}
	shannon_db_iter_cur_key(iter, key, 256, &key_len);
	if (memcmp(key, iter->prefix, iter->prefix_len) != 0 || key_len < iter->prefix_len)
		iter->valid = 0;
}

struct shannon_cf_handle *shannon_db_create_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err)
{
	char *p;
	struct shannon_cf_handle *cfh;
	struct uapi_cf_handle handle;
	int ret = 0;
	*err = NULL;
	cfh = malloc(sizeof(struct shannon_cf_handle));
	if (!cfh) {
		p = kvdb_err_msg[KVERR_MALLOC_FAIL - KVERR_BASE];
		printf("%s malloc shannon_cf_handle failed .\n", __FUNCTION__);
		*err = p;
		goto failed;
	}
	memset(cfh, 0, sizeof(struct shannon_cf_handle));
	memset(&handle, 0, sizeof(struct uapi_cf_handle));
	strcpy(handle.name, column_family_name);
	handle.db_index = db->db_index;
	ret = ioctl(db->dev_fd, CREATE_COLUMNFAMILY, &handle);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s  ioctl failed .\n", __FUNCTION__);
		*err = p;
		goto failed;
	}
	cfh->db_index = handle.db_index;
	cfh->cf_index = handle.cf_index;
	strcpy(cfh->cf_name, handle.name);
	return cfh;
failed:
	if (cfh)
		free(cfh);
	return NULL;
}

struct shannon_cf_handle *shannon_db_open_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err)
{
	char *p;
	struct shannon_cf_handle *cfh;
	struct uapi_cf_handle handle;
	int ret = 0;
	*err = NULL;
	cfh = malloc(sizeof(struct shannon_cf_handle));
	if (!cfh) {
		p = kvdb_err_msg[KVERR_MALLOC_FAIL - KVERR_BASE];
		printf("%s malloc failed .\n", __FUNCTION__);
		*err = p;
		goto failed;
	}
	memset(cfh, 0, sizeof(struct shannon_cf_handle));
	memset(&handle, 0, sizeof(struct uapi_cf_handle));
	strcpy(handle.name, column_family_name);
	handle.db_index = db->db_index;
	ret = ioctl(db->dev_fd, OPEN_COLUMNFAMILY, &handle);
	if (ret < 0) {
		p = strerror(errno);
		*err = p;
		goto failed;
	}
	cfh->db_index = handle.db_index;
	cfh->cf_index = handle.cf_index;
	strcpy(cfh->cf_name, handle.name);
	return cfh;
failed:
	if (cfh)
		free(cfh);
	return NULL;
}

void shannon_db_drop_column_family(struct shannon_db *db, struct shannon_cf_handle *handle, char **err)
{
	char *p;
	int ret = 0;
	struct uapi_cf_handle cfh;
	*err = NULL;
	memset(&cfh, 0, sizeof(struct uapi_cf_handle));
	cfh.db_index = handle->db_index;
	cfh.cf_index = handle->cf_index;
	strcpy(cfh.name, handle->cf_name);
	ret = ioctl(db->dev_fd, REMOVE_COLUMNFAMILY, &cfh);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s  ioctl failed .\n", __FUNCTION__);
		set_err_and_return(err, p);
	}
	set_err_and_return(err, NULL);
}

void shannon_db_column_family_handle_destroy(struct shannon_cf_handle *handle)
{
	if (handle)
		free(handle);
}

char **shannon_db_list_column_families(const struct db_options *opt, const char *dev_name, const char *db_name,
		unsigned int *cf_cnt, char **err)
{
	int i;
	char **cf_names = NULL;
	char *p_err;
	int dev_fd, ret;

	if (opt == NULL || dev_name == NULL || db_name == NULL || cf_cnt == NULL || err == NULL)
		return NULL;
	*err = NULL;

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		p_err = strerror(errno);
		printf("%s open kvdev failed:%s\n", __FUNCTION__, p_err);
		*err = p_err;
		return NULL;
	}

	struct uapi_db_handle kv_hd;
	memset(&kv_hd, 0, sizeof(struct uapi_db_handle));
	strcpy(kv_hd.name, db_name);
	ret = ioctl(dev_fd, OPEN_DATABASE, &kv_hd);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s open database failed:%s\n", __FUNCTION__,  p_err);
		*err = p_err;
		goto failed;
	}

	struct uapi_cf_list kv_cf_lst;
	memset(&kv_cf_lst, 0, sizeof(struct uapi_cf_list));
	kv_cf_lst.db_index = kv_hd.db_index;
	ret = ioctl(dev_fd, LIST_COLUMNFAMILY, &kv_cf_lst);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s  ioctl failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
		goto failed;
	}

	*cf_cnt = kv_cf_lst.cf_count;
	cf_names = malloc(sizeof(char *) * (*cf_cnt));
	if (cf_names == NULL) {
		p_err = kvdb_err_msg[KVERR_MALLOC_FAIL - KVERR_BASE];
		printf("%s malloc failed:%s.\n", __FUNCTION__, p_err);
		*err = p_err;
		goto failed;
	}

	for (i = 0; i < (*cf_cnt); i++) {
		cf_names[i] = NULL;
	}
	for (i = 0; i < (*cf_cnt); i++) {
		cf_names[i] = strdup(kv_cf_lst.cfs[i].name);
	}
	close(dev_fd);
	return cf_names;
failed:
	close(dev_fd);
	for (i = 0; i < kv_cf_lst.cf_count; i++) {
		if (cf_names[i] != NULL) {
			free(cf_names[i]);
			cf_names[i] = NULL;
		}
	}
	if (cf_names) {
		free(cf_names);
	}
	*cf_cnt = 1;
	cf_names = malloc(sizeof(char *));
	cf_names[0] = strdup("default");
	return cf_names;
}

void shannon_db_list_column_families_destroy(char **cf_list, unsigned int cf_count)
{
	int i;
	for (i = 0; i < cf_count; i++) {
		free(cf_list[i]);
		cf_list[i] = NULL;
	}
	free(cf_list);
}

/*column_family basic function*/
void shannon_db_put_cf(struct shannon_db *db, const struct db_writeoptions *w_opt,
		struct shannon_cf_handle *s_cf_hd, const char *key, unsigned int key_len,
		const char *val, unsigned int val_len, char **err)
{
	struct venice_kv kv;
	int ret;
	char *p_err;

	if (db == NULL || w_opt == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || val == NULL || val_len <= 0 || err == NULL)
		return;
	*err = NULL;

	memset(&kv, 0, sizeof(kv));
	if (key_len > MAX_KEY_SIZE) {
		p_err = kvdb_err_msg[KVERR_OVERSIZE_KEY - KVERR_BASE];
		printf("%s the length of key is invalid !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	if (val_len > MAX_VALUE_SIZE) {
		p_err = kvdb_err_msg[KVERR_OVERSIZE_VALUE - KVERR_BASE];
		printf("%s the length of value is invalid !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	kv.key = (char *)key;
	kv.key_len = key_len;
	kv.value = (char *)val;
	kv.value_len = val_len;
	kv.db = db->db_index;
	kv.sync = w_opt->sync;
	kv.fill_cache = w_opt->fill_cache;
	kv.cf_index = s_cf_hd->cf_index;
	ret = ioctl(db->dev_fd, PUT_KV, &kv);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s ioctl putkv failed !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	return;
failed:
	return;
}

int shannon_db_delete_cf(struct shannon_db *db, const struct db_writeoptions *w_opt,
		struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, char **err)
{
	struct venice_kv kv;
	int ret;
	char *p_err;

	if (db == NULL || w_opt == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || err == NULL) {
		printf("Error: Invalid parameter!\n");
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	memset(&kv, 0, sizeof(kv));
	if (key_len > MAX_KEY_SIZE) {
		ret = -KVERR_OVERSIZE_KEY;
		p_err = kvdb_err_msg[KVERR_OVERSIZE_KEY - KVERR_BASE];
		printf("%s the length of key is invalid !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	kv.key = (char *)key;
	kv.key_len = key_len;
	kv.db = db->db_index;
	kv.sync = w_opt->sync;
	kv.cf_index = s_cf_hd->cf_index;
	ret = ioctl(db->dev_fd, DEL_KV, &kv);
	if (ret < 0) {
		if (errno == ENXIO) {
			ret = -KVERR_NO_KEY;
			p_err = kvdb_err_msg[KVERR_NO_KEY - KVERR_BASE];
			*err = p_err;
		} else {
			p_err = strerror(errno);
			printf("%s ioctl delkv failed, ret=%d, errno=%d\n",
					__FUNCTION__, ret, errno);
			*err = p_err;
			ret = -errno;
		}
		goto failed;
	}
	return 0;
failed:
	return ret;
}

int shannon_db_get_cf(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle *s_cf_hd, const char *key,
		unsigned int key_len, char *val_buf, unsigned int buf_size, unsigned int *val_len,
		char **err)
{
	struct venice_kv kv;
	char *p_err;
	int ret;

	if (db == NULL || r_opt == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || val_buf == NULL || buf_size <= 0 || err == NULL) {
		printf("Error: Invalid parameter!\n");
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	memset(&kv, 0, sizeof(struct venice_kv));
	if (key_len > MAX_KEY_SIZE) {
		ret = -KVERR_OVERSIZE_KEY;
		p_err = kvdb_err_msg[KVERR_OVERSIZE_KEY - KVERR_BASE];
		printf("%s the length of key is invalid !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	kv.key = (char *)key;
	kv.key_len = key_len;
	kv.value = val_buf;
	kv.value_buf_size = buf_size;
	kv.db = db->db_index;
	kv.snapshot_id = r_opt->snapshot;
	kv.cf_index = s_cf_hd->cf_index;
	ret = ioctl(db->dev_fd, GET_KV, &kv);
	if (ret < 0) {
		if (errno == ENXIO) {
			ret = -KVERR_NO_KEY;
			p_err = kvdb_err_msg[KVERR_NO_KEY - KVERR_BASE];
			*err = p_err;
		} else {
			p_err = strerror(errno);
			printf("%s ioctl getkv failed, ret=%d, errno=%d\n",
					__FUNCTION__, ret, errno);
			*err = p_err;
			ret = -errno;
		}
		goto failed;
	} else {
		*val_len = kv.value_len;
	}
	return 0;
failed:
	*val_len = 0;
	return ret;
}

int shannon_db_key_exist_cf(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle *cf_hd, const char *key,
		unsigned int key_len, char **err)
{
	struct uapi_key_status status;
	char *p_err;
	int ret;

	if (db == NULL || r_opt == NULL || cf_hd == NULL || key == NULL || key_len <= 0 || err == NULL) {
		printf("Error: Invalid parameter!\n");
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return 0;
	}
	*err = NULL;

	if (key_len > MAX_KEY_SIZE) {
		ret = -KVERR_OVERSIZE_KEY;
		p_err = kvdb_err_msg[KVERR_OVERSIZE_KEY - KVERR_BASE];
		printf("%s the length of key is invalid !!!\n", __FUNCTION__);
		*err = p_err;
		return 0;
	}
	memset(&status, 0, sizeof(struct uapi_key_status));
	status.key = (char *)key;
	status.key_len = key_len;
	status.db_index = db->db_index;
	status.snapshot_id = r_opt->snapshot;
	status.cf_index = cf_hd->cf_index;
	ret = ioctl(db->dev_fd, IOCTL_KEY_STATUS, &status);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s ioctl get key_status failed !!!\n", __FUNCTION__);
		*err = p_err;
		return 0;
	} else {
		return status.exist;
	}
}

int shannon_db_writebatch_put_cf(db_writebatch_t *bat, struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, const char *val,
		unsigned int val_len, char **err)
{
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	struct writebatch_cmd *cmd = (struct writebatch_cmd *)(batch->data + batch->size - sizeof(struct write_batch_header));
	char *p_err;
	int ret;

	if (bat == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || val == NULL || val_len <= 0 || err == NULL) {
		printf("%s(): Error: Invalid parameter!\n", __func__);
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	if ((batch->size + batch->value_size + sizeof(struct writebatch_cmd) + key_len + val_len > MAX_BATCH_SIZE) ||
		(batch->count >= MAX_BATCH_COUNT)) {
		ret = -KVERR_BATCH_FULL;
		p_err = kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE];
		*err = p_err;
		return ret;
	}
	cmd->watermark = CMD_START_MARK;
	cmd->cf_index = s_cf_hd->cf_index;
	cmd->cmd_type = ADD_TYPE;
	cmd->key_len = key_len;
	cmd->value_len = val_len;
	memcpy(cmd->key, key, key_len);
	batch->value_size += val_len;
	cmd->value = (char *)batch + MAX_BATCH_SIZE - batch->value_size;
	memcpy(cmd->value, val, val_len);
	batch->size += (sizeof(struct writebatch_cmd) + key_len);
	batch->count++;
	return 0;
}

int shannon_db_writebatch_delete_cf(db_writebatch_t *bat,
		struct shannon_cf_handle *s_cf_hd, const char *key,
		unsigned int key_len, char **err)
{
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	struct writebatch_cmd *cmd = (struct writebatch_cmd *)(batch->data + batch->size - sizeof(struct write_batch_header));
	char *p_err;
	int ret;

	if (bat == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || err == NULL) {
		printf("%s(): Error: Invalid parameter!\n", __func__);
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	if ((batch->size + batch->value_size + sizeof(struct writebatch_cmd) + key_len > MAX_BATCH_SIZE) ||
		(batch->count >= MAX_BATCH_COUNT)){
		ret = -KVERR_BATCH_FULL;
		p_err = kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE];
		*err = p_err;
		return ret;
	}
	cmd->watermark = CMD_START_MARK;
	cmd->cf_index = s_cf_hd->cf_index;
	cmd->cmd_type = DELETE_TYPE;
	cmd->key_len = key_len;
	cmd->value_len = 0;
	cmd->value = NULL;
	memcpy(cmd->key, key, key_len);
	batch->size += (sizeof(struct writebatch_cmd) + key_len);
	batch->count++;
	return 0;
}

int __shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *bat, struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, const char *val,
		unsigned int val_len, __u64 timestamp, char **err)
{
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	struct writebatch_cmd *cmd = (struct writebatch_cmd *)(batch->data + batch->size - sizeof(struct write_batch_header));
	char *p_err;
	int ret;

	if (bat == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || val == NULL || val_len <= 0 || err == NULL) {
		printf("%s(): Error: Invalid parameter!\n", __func__);
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	if ((batch->size + batch->value_size + sizeof(struct writebatch_cmd) + key_len + val_len > MAX_BATCH_NONATOMIC_SIZE) ||
		(batch->count >= MAX_BATCH_NONATOMIC_COUNT)) {
		ret = -KVERR_BATCH_FULL;
		p_err = kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE];
		*err = p_err;
		return ret;
	}
	cmd->watermark = CMD_START_MARK;
	cmd->timestamp = timestamp;
	cmd->cf_index = s_cf_hd->cf_index;
	cmd->cmd_type = ADD_TYPE;
	cmd->key_len = key_len;
	cmd->value_len = val_len;
	memcpy(cmd->key, key, key_len);
	batch->value_size += val_len;
	cmd->value = (char *)batch + MAX_BATCH_NONATOMIC_SIZE - batch->value_size;
	memcpy(cmd->value, val, val_len);
	batch->size += (sizeof(struct writebatch_cmd) + key_len);
	batch->count++;
	return 0;
}

int __shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *bat,
		struct shannon_cf_handle *s_cf_hd, const char *key,
		unsigned int key_len, __u64 timestamp, char **err)
{
	struct write_batch_header *batch = (struct write_batch_header *)bat;
	struct writebatch_cmd *cmd = (struct writebatch_cmd *)(batch->data + batch->size - sizeof(struct write_batch_header));
	char *p_err;
	int ret;

	if (bat == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 || err == NULL) {
		printf("%s(): Error: Invalid parameter!\n", __func__);
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	if ((batch->size + batch->value_size + sizeof(struct writebatch_cmd) + key_len > MAX_BATCH_NONATOMIC_SIZE) ||
		(batch->count >= MAX_BATCH_NONATOMIC_COUNT)){
		ret = -KVERR_BATCH_FULL;
		p_err = kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE];
		*err = p_err;
		return ret;
	}
	cmd->watermark = CMD_START_MARK;
	cmd->timestamp = timestamp;
	cmd->cf_index = s_cf_hd->cf_index;
	cmd->cmd_type = DELETE_TYPE;
	cmd->key_len = key_len;
	cmd->value_len = 0;
	cmd->value = NULL;
	memcpy(cmd->key, key, key_len);
	batch->size += (sizeof(struct writebatch_cmd) + key_len);
	batch->count++;
	return 0;
}

#define GENERATE_TIMESTAMP 0
int shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *bat, struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, const char *val,
		unsigned int val_len, char **err)
{
	return __shannon_db_writebatch_put_cf_nonatomic(bat, s_cf_hd, key, key_len, val, val_len, GENERATE_TIMESTAMP, err);
}

int shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *bat,
		struct shannon_cf_handle *s_cf_hd, const char *key,
		unsigned int key_len, char **err)
{
	return __shannon_db_writebatch_delete_cf_nonatomic(bat, s_cf_hd, key, key_len, GENERATE_TIMESTAMP, err);
}

/*column_family iterator*/
struct shannon_db_iterator *shannon_db_create_iterator_cf(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle *s_cf_hd)
{
	struct shannon_db_iterator *iter_ret = NULL;
	char *err = NULL;

	if (db == NULL || r_opt == NULL || s_cf_hd == NULL)
		return NULL;

	shannon_db_create_iterators(db, r_opt, &s_cf_hd, &iter_ret, 1, &err);
	return iter_ret;
}

struct shannon_db_iterator *shannon_db_create_prefix_iterator_cf(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle *s_cf_hd, const char *prefix, unsigned int prefix_len)
{
	struct shannon_db_iterator *iter_ret = NULL;
	char *err = NULL;

	if (db == NULL || r_opt == NULL || s_cf_hd == NULL || prefix == NULL || prefix_len <= 0)
		return NULL;

	shannon_db_create_prefix_iterators(db, r_opt, &s_cf_hd, &iter_ret, 1, prefix, prefix_len, &err);
	return iter_ret;
}

void shannon_db_create_iterators(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle **s_cf_hd,
		struct shannon_db_iterator **iter_ret, int size, char **err)
{
	int i;
	struct uapi_db_iterator *kv_iter = NULL;
	int ret;
	char *p_err;

	if (db == NULL || r_opt == NULL || s_cf_hd == NULL || iter_ret == NULL || size <= 0 || err == NULL)
		return;
	*err = NULL;

	kv_iter = malloc(sizeof(struct uapi_db_iterator) + sizeof(struct uapi_cf_iterator) * size);
	memset(kv_iter, 0, sizeof(struct uapi_db_iterator));
	kv_iter->timestamp = r_opt->snapshot;
	kv_iter->db_index = db->db_index;
	kv_iter->count = size;
	kv_iter->only_read_key = r_opt->only_read_key;
	for (i = 0; i < size; ++i) {
		kv_iter->iters[i].timestamp = kv_iter->timestamp;
		kv_iter->iters[i].db_index = kv_iter->db_index;
		kv_iter->iters[i].cf_index = s_cf_hd[i]->cf_index;
		kv_iter->iters[i].only_read_key = kv_iter->only_read_key;
	}
	ret = ioctl(db->dev_fd, IOCTL_CREATE_ITERATOR, kv_iter);
	if (ret < 0) {
		p_err = strerror(errno);
		printf("%s ioctl failed !!!\n", __FUNCTION__);
		*err = p_err;
		goto failed;
	}
	for (i = 0; i < size; ++i) {
		iter_ret[i] = malloc(sizeof(struct shannon_db_iterator));
		if (iter_ret[i] == NULL)
			goto failed;
		iter_ret[i]->db = *db;
		iter_ret[i]->cf_index = kv_iter->iters[i].cf_index;
		iter_ret[i]->prefix_len = 0;
		iter_ret[i]->snapshot = kv_iter->timestamp;
		iter_ret[i]->iter_index = kv_iter->iters[i].iter_index;
		iter_ret[i]->valid = kv_iter->iters[i].valid_key;
	}
	if (kv_iter != NULL)
		free(kv_iter);
	return;
failed:
	if (kv_iter != NULL)
		free(kv_iter);
}

void shannon_db_create_prefix_iterators(struct shannon_db *db, const struct db_readoptions *r_opt,
		struct shannon_cf_handle **s_cf_hd,
		struct shannon_db_iterator **iter_ret, int size,
		const char *prefix, unsigned int prefix_len, char **err)
{
	int i;

	if (db == NULL || r_opt == NULL || s_cf_hd == NULL || iter_ret == NULL || size <= 0 || prefix == NULL || prefix_len <= 0 || err == NULL)
		return;
	*err = NULL;

	shannon_db_create_iterators(db, r_opt, s_cf_hd, iter_ret, size, err);
	for (i = 0; i < size; ++i) {
		if (iter_ret[i] != NULL) {
			iter_ret[i]->prefix_len = prefix_len;
			memcpy(iter_ret[i]->prefix, prefix, prefix_len);
		}
	}
}

/*readbatch*/
db_readbatch_t *shannon_db_readbatch_create()
{
	db_readbatch_t *batch;
	struct read_batch_header *bat;
	batch = malloc(MAX_BATCH_SIZE);
	if (!batch) {
		perror("shannon_db_readbatch_create malloc batch failed!!");
		return NULL;
	}
	bat = (struct read_batch_header *)batch;
	bat->size = sizeof(struct read_batch_header);
	bat->count = 0;
	bat->fill_cache = 1;
	return (db_readbatch_t *)bat;
}

void shannon_db_readbatch_destroy(db_readbatch_t *batch)
{
	if (batch)
		free(batch);
}

int shannon_db_readbatch_get(db_readbatch_t *bat, const char *key, unsigned int key_len, const char *val_buf,
		unsigned int buf_size, unsigned int *value_len, unsigned int *status, char **err)
{
	struct shannon_cf_handle s_cf_hd;
	*err = NULL;
	s_cf_hd.cf_index = 0;
	return shannon_db_readbatch_get_cf(bat, &s_cf_hd, key, key_len, val_buf, buf_size, value_len, status, err);
}

int shannon_db_readbatch_get_cf(db_readbatch_t *bat, struct shannon_cf_handle *s_cf_hd,
		const char *key, unsigned int key_len, const char *val_buf,  unsigned int buf_size,
		unsigned int *value_len, unsigned int *status, char **err)
{
	struct read_batch_header *batch = (struct read_batch_header *)bat;
	struct readbatch_cmd *cmd = (struct readbatch_cmd *)(batch->data + batch->size - sizeof(struct read_batch_header));
	char *p_err;
	int ret;

	if (bat == NULL || s_cf_hd == NULL || key == NULL || key_len <= 0 ||  buf_size <= 0 || err == NULL) {
		printf("%s(): Error: Invalid parameter!\n", __func__);
		ret = -KVERR_INVALID_PARAMETER;
		p_err = kvdb_err_msg[KVERR_INVALID_PARAMETER - KVERR_BASE];
		if (err)
			*err = p_err;
		return ret;
	}
	*err = NULL;

	if ((batch->size + sizeof(struct readbatch_cmd) + key_len > MAX_BATCH_SIZE) ||
		(batch->count >= MAX_READBATCH_COUNT)){
		ret = -KVERR_BATCH_FULL;
		p_err = kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE];
		*err = p_err;
		return ret;
	}
	cmd->watermark = CMD_START_MARK;
	cmd->cf_index = s_cf_hd->cf_index;
	cmd->cmd_type = GET_TYPE;
	cmd->key_len = key_len;
	cmd->return_status = status;
	cmd->value_len_addr = value_len;
	cmd->value_buf_size = buf_size;
	memcpy(cmd->key, key, key_len);
	cmd->value = (char *)val_buf;
	batch->size += (sizeof(struct readbatch_cmd) + cmd->key_len);
	batch->count++;
	return 0;
}

int readbatch_is_valid(db_readbatch_t *bat)
{
	struct read_batch_header *batch_header = (struct read_batch_header *)bat;
	if ((batch_header->size <= sizeof(struct read_batch_header)) || (batch_header->size > MAX_BATCH_SIZE) ||
		(batch_header->count > MAX_READBATCH_COUNT) || (batch_header->count <= 0)) {
		printf("readbatch is not valid:MAX_READBATCH_COUNT=%d, MAX_BATCH_SIZE=%ld, batch->size=%ld, batch->count=%d\n",\
			 MAX_READBATCH_COUNT, MAX_BATCH_SIZE, batch_header->size, batch_header->count);
		return 0;
	} else {
		return 1;
	}
}

void shannon_db_read(struct shannon_db *db, const struct db_readoptions *options,
	db_readbatch_t *bat, unsigned int *failed_cmd_count, char **err)
{
	int fd, ret;
	char *p;
	struct read_batch_header *batch = (struct read_batch_header *)bat;
	*err = NULL;
	if (!readbatch_is_valid(bat)) {
		p = kvdb_err_msg[KVERR_BATCH_INVALID - KVERR_BASE];
		goto failed;
	}
	batch->db_index = db->db_index;
	batch->fill_cache = options->fill_cache;
	batch->snapshot = options->snapshot;
	batch->failed_cmd_count = failed_cmd_count;
	ret = ioctl(db->dev_fd, READ_BATCH, batch);
	if (ret < 0) {
		p = strerror(errno);
		printf("%s ioctl readbatch failed :%s.\n", __FUNCTION__, p);
		goto failed;
	}
	return;
failed:
	set_err_and_return(err, p);
}

void shannon_db_readbatch_clear(db_readbatch_t *batch)
{
	struct read_batch_header *batch_header;
	batch_header = (struct read_batch_header *)batch;
	batch_header->size = sizeof(struct read_batch_header);
	batch_header->count = 0;
	batch_header->fill_cache = 1;
}

struct db_sstoptions *shannon_db_sstoptions_create()
{
	struct db_sstoptions *option = malloc(sizeof(struct db_sstoptions));
	if (option == NULL) {
		printf("%s malloc db_sstoptions failed.\n", __FUNCTION__);
		return NULL;
	}
	option->block_restart_data_interval = 16;
	option->block_restart_index_interval = 1;
	option->block_size = BLOCK_SIZE;
	option->compression =  0;
	option->is_rocksdb =  1;
	return option;
}

void shannon_db_sstoptions_set_data_interval(struct db_sstoptions *opt, int interval)
{
	opt->block_restart_data_interval = interval;
}

void shannon_db_sstoptions_set_block_size(struct db_sstoptions *opt, unsigned long size)
{
	opt->block_size = size;
}

void shannon_db_sstoptions_set_compression(struct db_sstoptions *opt, unsigned char v)
{
	opt->compression = v;
}

void shannon_db_sstoptions_set_is_rocksdb(struct db_sstoptions *opt, unsigned char v)
{
	opt->is_rocksdb = v;
}

/**
 * @brief get property
 *
 * @param db
 * @param propname
 * @return char*
 */
char *shannon_db_property_value(shannon_db_t *db, const char *propname)
{
	shannon_cf_handle_t cf;
	cf.cf_index = 0;  /* default cf */
	return shannon_db_property_value_cf(db, &cf, propname);
}

/**
 * @brief get an int property
 *
 * @param db
 * @param propname
 * @param out_val
 * @return int return 0 on success
 */
int shannon_db_property_int(shannon_db_t *db, const char *propname, __u64 *out_val)
{
	char *s = shannon_db_property_value(db, propname);

	if (s) {
		*out_val = atoll(s);
		free(s);
		return 0;
	}
	return -1;
}

/**
 * @brief get property, a malloc()-ed null-terminated string.
 * It is caller's responsibility to free.
 *
 * @param db
 * @param cf
 * @param propname
 * @return char* return NULL if property name is unknown
 */
char *shannon_db_property_value_cf(shannon_db_t *db, shannon_cf_handle_t *cf, const char *propname)
{
	struct uapi_get_property prop;
	int ret = 0;

	if (db == NULL)
		return NULL;

	memset(&prop, 0, sizeof(prop));
	strncpy(prop.prop_name, propname, PROPERTY_BUF_SIZE);
	prop.db_index = db->db_index;
	prop.cf_index = cf ? cf->cf_index : -1;

	ret = ioctl(db->dev_fd, IOCTL_GET_PROPERTY, &prop);
	if (ret)
		return NULL;
	return strdup(prop.prop_val);
}
