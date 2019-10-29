**shannon_db驱动程序C接口函数及使用说明**

**Version: 1.8.2**

[TOC]

# 概述

The goal of shannon_db is to develop a SSD specifically designed for high performance Key-Value store. This is C implementation for shannon_db.



# 常量

## Constants

```c++
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
```

### READBATCH

```c++
#define READBATCH_SUCCESS      0
#define READBATCH_NO_KEY       1
#define READBATCH_DATA_ERR     2
#define READBATCH_VAL_BUF_ERR  3
```

### kvdb_err_msg

```c++
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
```

the status of operation returned; and you can get the message from kvdb_err_msg by index.

## Enum Constants

### error_type

```c++
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
```



# 结构体

## shannon_db_t

```c++
typedef struct shannon_db {
	int 	dev_fd;			//!< 对应的设备文件描述符
	int 	db_index;		//!< 该数据库在系统内部的编号
	char	dbname[32];		//!< 数据库名字
} shannon_db_t;
```

shannon_db_t数据结构用来描述系统内一个database实例，比如该数据库实例的名字。

shannon系统支持同时多个db实例，一个db可包含多个Column Family（列簇）。

db_index用来标记db实例在shannon内部的编号。

可使用接口shannon_db_open()或shannon_db_open_column_families()来打开一个数据库，成功打开后则返回shannon_db_t实例。

## shannon_cf_handle_t

```c++
typedef struct shannon_cf_handle {
	int 	db_index;		//!< 所属db
	int		cf_index;		//!< cf在db中的位置
	char 	cf_name[32];	//!< cf名字
} shannon_cf_handle_t;
```

the data_structure of shannon_db to open the cilumnfamily.

## db_options_t

```c++
typedef struct db_options {
	/** default: 1 靠初始化接口保证 */
	int create_if_missing;
	/** 数据压缩选项。not supported yet */
	int compression;
} db_options_t;
```

the options to open the database:

* **create_if_missing**: if not exist will create database
* **compression**: compression method

## db_readoptions_t

```c++
typedef struct db_readoptions {
	/* default: 1 */
	int 	fill_cache;
	/* default: 0 */
	int 	only_read_key;
	__u64 	snapshot;
} db_readoptions_t;
```

the ReadOptions of kv for get data to the device.

* **snapshot**: the timestamp that you want to read from. Default: 0
* **fill_cache**: Should the data read/write for this databse be cached in memory? Default: true

## db_writeoptions_t

```c++
typedef struct db_writeoptions {
	/* only support sync=1 */
	int sync;
	/* default: 1 */
	int fill_cache;
} db_writeoptions_t;
```

the writeoptions of kv for put data to the device.

- **sync**: If true, the write will be flushed from the operating system buffer cache (by calling WritableFile::Sync()) before the write is considered complete. Default: true.
- **fill_cache**: Should the data read/write for this databse be cached in memory? Default: true

## db_readbatch_t

```c++
typedef void db_readbatch_t;
```

ReadBatch holds a collection of updates to apply atomically to aDB. The updates are applied in the order in which they are get.

## db_writebatch_t

```c++
typedef void db_writebatch_t;
```

WriteBatch holds a collection of updates to apply atomically to a DB. The updates are applied in the order in which they are added.


## db_sstoptions_t

```c++
typedef struct db_sstoptions {
	int block_restart_data_interval;
	int block_restart_index_interval;
	unsigned long block_size;
	int compression;
	int is_rocksdb;
} db_sstoptions_t;
```

the Options of kv to generate sst file for back-up data.

- **block_restart_data_interval**: default16,
- **block_restart_index_interval**: default 1
- **block_size**: default 4096
- **compression**: 1
- **is_rocksdb**: 0


## shannon_db_iterator

```c++
struct shannon_db_iterator {
	struct shannon_db db;
	int 	cf_index;
	int 	prefix_len;
	char 	*err;
	__u64 	snapshot;
	int 	iter_index;
	int 	valid;
	unsigned char prefix[128];
};
```

the iterator of database to iterate all data in database.

- **db**: the database of iterator that you want to iterate.
- **cf_index**: the index of columnfamily to operate
- **Snapshot**: the timestamp that you want to read from. Default: 0
- **Iter_index**: the index that you create the iterator.
- **Valid**: An iterator is either positioned at a key/value pair, or not valid
- **Prefix**: the prefix of iterator that you want to read from database.
- **prefix_len**: the length of the prefix

# 接口函数

## ShannonDB APIs

### shannon_db_open

```c++
shannon_db_t *shannon_db_open(const struct db_options *opt,
                              const char *dev_name, 
                              const char *db_name,
                              char **errptr);
```

open the kv deviece by dev_name and open the database by db_name , so you must given the parameters.

**PARAMETERS**

- **options**: Options of the kvdevice to open
- **devname**: Path of the device to open e.g. “/dev/kvdev00”
- **dbname**: Path of the databse to open e.g. “test.db”
- **errptr**: Message of open shannon_db.

**RETURNS**

* shannon_db on success , the db will contain shannon_db information .
* NULL on fail

### shannon_destroy_db

```c++
void shannon_destroy_db(struct shannon_db *db, char **errptr);
```

destroy the database on kv deviece.

**PARAMETERS**

- **db**: the database of shannond_db that you want to destroy.
- **errptr**: Message of destroy shannon_db.

**RETURNS**

* void

### shannon_db_close

```c++
void shannon_db_close(struct shannon_db *db);
```

**PARAMETERS**

* **db**: the database of shannond_db that you want to close.

**RETURNS**

* void

### shannon_db_put

```c++
void shannon_db_put(struct shannon_db *db, const struct db_writeoptions *opt,
                    const char *key, unsigned int key_len, 
					const char *value, unsigned int value_len, 
					char **err);
```

put kv data to the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to put kvdata
* **opt**: the writeoptions of kv for put data to the device.
* **key**: the value of data key you want to add
* **key_len**: the length of data key you want to add
* **value**: the value of data value you want to add
* **value_len**: the length of data value you want to add
* **errptr**: Message of put kvdata to default columnfamilies.

**RETURNS**

* void if success, the err is null .

### shannon_db_put_cf

```C++
void shannon_db_put_cf(struct shannon_db *db, 
                       const struct db_writeoptions *options,
                       struct shannon_cf_handle *column_family, 
                       const char *key, unsigned int keylen, 
                       const char *val, unsigned int vallen, 
                       char **err);
```

put kv data to the columnfamily that you give, so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to put kvdata
* **options**: the writeoptions of kv for put data to the device.
* **column_family**: the handle of columnfamily you want to put. contains information of columnfamily
* **key**: the value of data key you want to add
* **key_len**: the length of data key you want to add
* **value**: the value of data value you want to add
* **value_len**: the length of data value you want to add
* **errptr**: Message of put kv data to columnfamilies.

**RETURNS**

* void if success, the err is null .

### shannon_db_delete

```c++
int shannon_db_delete(struct shannon_db *db, const struct db_writeoptions *opt,
                      const char *key, unsigned int key_len, 
                      char **err);
```

delete kv data to the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **opt**: the writeoptions of kv for put data to the device.
* **key**: the value of data key you want to add
* **key_len**: the length of data key you want to add
* **errptr**: Message of delete database.

**RETURNS**

* void if success, the err is null.

### shannon_db_delete_cf

```c++
int shannon_db_delete_cf(struct shannon_db *db,
                         const struct db_writeoptions *options,
                         struct shannon_cf_handle *column_family,
                         const char *key, unsigned int keylen, 
                         char **err);
```

delete kv data to the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to put kvdata
* **opt**: the writeoptions of kv for put data to the device.
* **column_family**: the handle of columnfamily you want to put. contains information of columnfamily
* **key**: the value of data key you want to add
* **key_len**: the length of data key you want to add
* **errptr**: Message of open columnfamilies.

**RETURNS**

* void if success, the err is null.

### shannon_db_get

```c++
int shannon_db_get(struct shannon_db *db, const struct db_readoptions *opt,
                   const char *key, unsigned int key_len, 
                   char *value_buf, unsigned int buf_size, 
                   unsigned int *value_len, 
                   char **err);
```

get kv data to the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to get
* **opt**: the s of kv for put data to the device.
* **key**: the value of data key you want to get
* **key_len**: the length of data key you want to get
* **value_buf**: the buffer of data that you get from database.
* **buf_size**: the length of buf.
* **value_len**: the length of data value you get.
* **errptr**: Message of get data form database.

**RETURNS**

* void if success, the err is null .

### shannon_db_get_cf

```c++
int shannon_db_get_cf(struct shannon_db *db,
                      const struct db_readoptions *options,
                      struct shannon_cf_handle *column_family, 
                      const char *key, unsigned int keylen, 
                      char *value_buf, unsigned int buf_size, 
                      unsigned int *value_len,
                      char **err);
```

get kv data from columnfamily of the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to get
* **opt**: the s of kv for put data to the device.
* **column_family**: the handle of columnfamily you want to put. contains information of columnfamily
* **key**: ​the value of data key you want to get
* **key_len**: the length of data key you want to get
* **value**: the buffer of data that you get from database.
* **buf_size**: the length of buf.
* **value_len**: the length of data value you get.
* **errptr**: Message of get data from columnfamilies.

**RETURNS**

* if success,return 0; others status set to indicate the error.

### shannon_db_key_exist

```c++
int shannon_db_key_exist(struct shannon_db *db,
                         const struct db_readoptions *opt, 
                         const char *key, unsigned int key_len, 
                         char **err);
```

judeg tht key is exist in the database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to get
* **opt**: the s of kv for put data to the device.
* **key**: the value of data key you want to get
* **key_len**: the length of data key you want to get
* **errptr**: Message of get data form database.

**RETURNS**

* if exist return true, if not ,return 0

### shannon_db_key_exist_cf

```c++
int shannon_db_key_exist_cf(struct shannon_db *db,
                            const struct db_readoptions *options,
                            struct shannon_cf_handle *column_family,
                            const char *key, unsigned int keylen, 
                            char **err);
```

judeg tht key is exist in the columnfamily of database , so you must given the parameters.

**PARAMETERS**

* **db**: the database of shannon_db that you want to get
* **column_family**: the columnfamily of shannon_db that you want to get
* **opt**: the s of kv for put data to the device.
* **key**: the value of data key you want to get
* **key_len**: the length of data key you want to get
* **errptr**: Message of get data form database.

**RETURNS**

- if exist return true, if not ,return 0.

### shannon_db_set_cache_size

```c++
void shannon_db_set_cache_size(struct shannon_db *db, unsigned long size, char **err);
```

set cache_size for device.

**PARAMETERS**

* **db**: the database of shannon_db that you want to set cache
* **size**: the size of cache that you want to set.

**RETURNS**

* if success,return 0; others status set to indicate the error.

### shannon_db_get_sequence

```c++
int shannon_db_get_sequence(const char *dev_name, __u64 *sequence, char **err);
```

get cur_timestamp of device.

**PARAMETERS**

* **dev_name**: the devname of shannon_db that you want to get sequence.
* **sequence**: the timestamp of db that you want to get.
* **errptr**: Message of get_sequence to database.

**RETURNS**

* if success,return 0; others status set to indicate the error.

### shannon_db_set_sequence

```c++
int shannon_db_set_sequence(const char *dev_name, __u64 sequence, char **err);
```

set timestamp of device.

**PARAMETERS**

* **dev_name**: the devname of shannon_db that you want to set sequence.
* **sequence**: the timestamp of db that you want to set.
* **errptr**: Message of get_sequence to database.

**RETURNS**

* if success,return 0; others status set to indicate the error.

## ColumnFamily APIs

### shannon_db_create_column_family

```c++
struct shannon_cf_handle *shannon_db_create_column_family(
    struct shannon_db *db, 
    const struct db_options *column_family_options,
    const char *column_family_name, 
    char **err);
```

create columnfamily for the database , so you must give the parameters.

**PARAMETERS**

- **db**: the database of shannon_db that you want to create column
- **db_options**: the options of kv to create columnfamily.
- **column_family_name**: name of columnfamily you want create
- **errptr**: Message of create columnfamily for shannon_db.

**RETURNS**

* if success,return shannon_cf_handle, err is null .

### shannon_db_open_column_family

```c++
struct shannon_cf_handle *shannon_db_open_column_family(
    struct shannon_db *db, 
	const struct db_options *column_family_options,
    const char *column_family_name, 
	char **err);
```

open columnfamily of the database , so you must give the parameters.

**PARAMETERS**

- **db**: the database of shannon_db that you want to create column

- **db_options**: the options of kv to create columnfamily.
- **column_family_name**: name of columnfamily you want create
- **errptr**: Message of open the columnfamily shannon_db.

**RETURNS**

* void if success, the err is null.

### shannon_db_open_column_families

```c++
struct shannon_db *shannon_db_open_column_families(
    const struct db_options *options, 
	const char *dev_name, 
	const char *db_name,
    int num_column_families, 
	const char **column_family_names,
    const struct db_options **column_family_options,
    struct shannon_cf_handle **column_family_handles, 
	char **err);
```

open all columnfamilies of the database , so you must give the parameters.

**PARAMETERS**

* **db_options**: the options of kv to create columnfamily.
* **devname**: Path of the device to open e.g. “/dev/kvdev00”
* **dbname**: Path of the databse to open e.g. “test.db”
* **num_column_families**: num of columnfamilies you want open.
* **column_family_names**: name of columnfamily you want open
* **column_family_options**: the options of columnfamily to be opened.
* **column_family_handles**: the handles of columnfamily you want to open.
* **errptr**: Message of open all columnfamilies.

**RETURNS**

* struct shannon_db *db; on success ,the db will contain db information , and the err is null

### shannon_db_drop_column_family

```c++
void shannon_db_drop_column_family(struct shannon_db *db,
                                   struct shannon_cf_handle *handle,
                                   char **err);
```

drop columnfamily of the database , so you must give the parameters.

**PARAMETERS**

- **db**: the database of shannon_db that you want to create column
- **handle**: the handle of columnfamily you want to drop . contains information of columnfamily
- **errptr**: Message of drop columnfamilyof shannon_db.

**RETURNS**

- void if success, the err is null.

### shannon_db_column_family_handle_destroy

```c++
void shannon_db_column_family_handle_destroy(struct shannon_cf_handle *handle);
```

after drop columnfamily , you must free the handle

**PARAMETERS**

* **handle**: the handle of columnfamily you want to free . contains information of columnfamily

**RETURNS**

* void

### shannon_db_list_column_families

```c++
char **shannon_db_list_column_families(const struct db_options *options,
                                       const char *dev_name, 
									   const char *dbname,
                                       unsigned int *cf_count, 
									   char **err);
```

list all columnfamilies of the database , so you must give the parameters.

**PARAMETERS**

* **db_options**: the options of kv to create columnfamily.
* **devname**: Path of the device to open e.g. “/dev/kvdev00”
* **dbname**: Path of the databse to open e.g. “test.db”
* **cf_count**: return the number of columnfamilies .
* **errptr**: Message of list all columnfamilies.

**RETURNS**

* char **names if success return names contains all columnfamily name.

### shannon_db_list_column_families_destroy

```c++
void shannon_db_list_column_families_destroy(char **cf_list,
                                             unsigned int cf_count);
```

free columnfamily names of the database that you malloc, so you must give the parameters.

**PARAMETERS**

* **cf_list**: the collection of columnfamily names that you want to free.
* **cf_count**: the count of columnfamily you want to free .

**RETURNS**

* void

## Options APIs

### DB Options

#### shannon_db_options_create

```c++
struct db_options *shannon_db_options_create();
```

create the option for the database

**PARAMETERS**

无

**RETURNS**

* the option to operate database

#### shannon_db_options_destroy

```c++
void shannon_db_options_destroy(struct db_options *opt);
```

release the options for the database to given the options

**PARAMETERS**

* **opt**: the options of kv to operate the database.

**RETURNS**

* void

#### shannon_db_options_set_create_if_missing

```c++
void shannon_db_options_set_create_if_missing(struct db_options *opt, unsigned char v);
```

set the create_if_missing value of options.

**PARAMETERS**

* **options**: options of user to set create_if_missing for device
* **v**: the value that you want to set: default true.

**RETURNS**

* void

### Read Options

#### shannon_db_readoptions_create

```c++
struct db_readoptions *shannon_db_readoptions_create();
```

create the readoption for the database

**PARAMETERS**

无

**RETURNS**

* readoption to get data from database

#### shannon_db_readoptions_destroy

```c++
void shannon_db_readoptions_destroy(struct db_readoptions *opt);
```

destroy the readoptions for the database to given the options

**PARAMETERS**

* **opt**: the readoptions of kv to get data from the database.

**RETURNS**

* void

#### shannon_db_readoptions_set_fill_cache

```c++
void shannon_db_readoptions_set_fill_cache(struct db_readoptions *opt, unsigned char v);
```

set the fill_cache of readoptions.

**PARAMETERS**

* **options**: read options of user to set fill_cache for device
* **v**: the value that you want to set: default true.

**RETURNS**

* void

#### shannon_db_readoptions_set_only_iter_key

```c++
void shannon_db_readoptions_set_only_iter_key(struct db_readoptions *opt, unsigned char v);
```

set the only_read_key of readoptions when create iterator.

**PARAMETERS**

* **options**: read options of user to set only_read_key for device
* **v**: the value that you want to set: default 0.

**RETURNS**

* void

### Write Options

#### shannon_db_writeoptions_create

```c++
struct db_writeoptions *shannon_db_writeoptions_create();
```

create the writeoption for the database

**PARAMETERS**

无

**RETURNS**

* writeoption to get data from database

#### shannon_db_writeoptions_destroy

```c++
void shannon_db_writeoptions_destroy(struct db_writeoptions *opt);
```

destroy the writeoptions for the database to given the options

**PARAMETERS**

* **opt**: the writeoptions of kv to put data from the database.

**RETURNS**

* void

#### shannon_db_writeoptions_set_fill_cache

```c++
void shannon_db_writeoptions_set_fill_cache(struct db_writeoptions *opt, unsigned char v);
```

set the fill_cache of writeoptions.

**PARAMETERS**

* **options**: writeoptions of user to set fill_cache for device
* **v**: the value that you want to set: default true.

**RETURNS**

* void

#### shannon_db_writeoptions_set_sync

```c++
void shannon_db_writeoptions_set_sync(struct db_writeoptions *opt, unsigned char v);
```

set the sync of writeoptions.

**PARAMETERS**

* options: writeoptions of user to set sync for device
* v: the value that you want to set: default true.

**RETURNS**

* void

## Snapshot APIs

### shannon_db_create_snapshot

```c++
__u64 shannon_db_create_snapshot(struct shannon_db *db, char **err);
```

create the snapshot for the database to given the db

**PARAMETERS**

* **db**: the database of shannon_db that you create snapshot
* **errptr**: Message of create_snapshot from database.

**RETURNS**

* if success, return timestamp ,the err is null ,others set to indicate the error.

### shannon_db_release_snapshot

```c++
void shannon_db_release_snapshot(struct shannon_db *db, __u64 snapshot, char **err);
```

release the snapshot for the database to given the timestamp

**PARAMETERS**

* **db**: the database of shannon_db that you create snapshot
* **snapshot**: the timestamp of snapshot that you want to release
* **errptr**: Message of release_snapshot from database.

**RETURNS**

* void

### shannon_db_readoptions_set_snapshot

```c++
void shannon_db_readoptions_set_snapshot(struct db_readoptions *opt, __u64 snap);
```

set the timestamp for readoptions.

**PARAMETERS**

* **options**: readoptions of user to set timestamp for device
* **snap**: the timestamp of snapshot that you want to release

**RETURNS**

* void

## ReadBatch APIs

### shannon_db_readbatch_create

```c++
db_readbatch_t *shannon_db_readbatch_create();
```

create the readbatch for the database.

**PARAMETERS**

无

**RETURNS**

* the datastruct of db_readbatch_t * readbatch

### shannon_db_readbatch_destroy

```c++
void shannon_db_readbatch_destroy(db_readbatch_t *batch);
```

release the readbatch for the database

**PARAMETERS**

* **batch**: the readbatch of db that you destroy

**RETURNS**

* void

### shannon_db_readbatch_clear

```c++
void shannon_db_readbatch_clear(db_readbatch_t *batch);
```

clear the readbatch that you given;

**PARAMETERS**

* **batch**: the readbatch of db that you clear

**RETURNS**

* void

### shannon_db_readbatch_get

```c++
int shannon_db_readbatch_get(db_readbatch_t *batch, 
                             const char *key, unsigned int key_len, 
                             const char *val, unsigned int buf_size, 
                             unsigned int *value_len,
                             unsigned int *status, 
                             char **err);
```

get kvdata from the readbatch

**PARAMETERS**

* **batch**: the readbatch of db that you get
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **val**: the buffer of data that you get from database.
* **buf_size**: the size of val that you get from database.
* **value_len**:  return the len of value that you to get
* **status**: indicate different result
* **errptr**: Message of get kvdata to readbatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_readbatch_get_cf

```c++
int shannon_db_readbatch_get_cf(db_readbatch_t *batch,
                                struct shannon_cf_handle *column_family,
                                const char *key, unsigned int klen,
                                const char *val_buf, unsigned int buf_size,
                                unsigned int *value_len, 
                                unsigned int *status,
                                char **err);
```

get kvdata from the readbatch

**PARAMETERS**

* **batch**: the readbatch of db that you get
* **column_family**: the handle of columnfamily you want to operate. contains information of columnfamily
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **val**: the buffer of data that you get from database.
* **buf_size**: the size of val that you get from database.
* **value_len**:  return the len of value that you to get
* **errptr**: Message of get kvdata to readbatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_read

```c++
void shannon_db_read(struct shannon_db *db,
                     const struct db_readoptions *options, 
                     db_readbatch_t *batch,
                     unsigned int *failed_cmd_count, 
                     char **err);
```

在db上执行批量读操作。

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **options**: the readoptions of kv for put data to the device.
* **batch**: the readbatch of db that you write
* **failed_cmd_count**: number of failed operations in this batch
* **err**: Message of write batch to database.

**RETURNS**

* if success,th err is null ; others status set to indicate the error.

## WriteBatch APIs

### shannon_db_writebatch_create

```c++
db_writebatch_t *shannon_db_writebatch_create();
```

create the writebatch for the database.

**PARAMETERS**

无

**RETURNS**

* the datastruct of db_writebatch_t * writebatch

### shannon_db_writebatch_destroy

```c++
void shannon_db_writebatch_destroy(db_writebatch_t *batch);
```

release the writebatch for the database

**PARAMETERS**

* **batch**: the writebatch of db that you destroy

**RETURNS**

* void

### shannon_db_writebatch_clear

```c++
void shannon_db_writebatch_clear(db_writebatch_t *batch);
```

clear the writebatch that you given;

**PARAMETERS**

* **batch**: the writebatch of db that you clear

**RETURNS**

* void

### shannon_db_writebatch_put

```c++
int shannon_db_writebatch_put(db_writebatch_t *batch, 
                              const char *key, unsigned int key_len,
                              const char *value, unsigned int value_len, 
                              char **err);
```

put kvdata to the writebatch

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **value**: the buffer of data that you get from database.
* **value_len**:  the buffer of data value that you want to get
* **errptr**: Message of put kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_put_cf

```c++
int shannon_db_writebatch_put_cf(db_writebatch_t *batch,
                                 struct shannon_cf_handle *column_family,
                                 const char *key, unsigned int klen,
                                 const char *val, unsigned int vlen,
                                 char **err);
```

put kvdata to the writebatch of columnfamily

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **column_family**: the handle of columnfamily you want to put. contains information of columnfamily
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **value**: the buffer of data that you get from database.
* **value_len**:  the buffer of data value that you want to get
* **errptr**: Message of put kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_delete

```c++
int shannon_db_writebatch_delete(db_writebatch_t *batch, 
                                 const char *key, unsigned int key_len, 
                                 char **err);
```

delete kvdata to the writebatch

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **errptr**: Message of delete kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_delete_cf

```c++
int shannon_db_writebatch_delete_cf(db_writebatch_t *batch,
                                    struct shannon_cf_handle *column_family,
                                    const char *key, unsigned int klen,
                                    char **err);
```

delete kvdata to the writebatch of columnfamily

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **column_family**: the handle of columnfamily you want to operate. contains information of columnfamily
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **errptr**: Message of delete kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_write

```c++
void shannon_db_write(struct shannon_db *db, 
                      const struct db_writeoptions *opt,
                      db_writebatch_t *batch, 
                      char **err);
```

write writebatch to the database

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **opt**: the writeoptions of kv for put data to the device.
* **batch**: the writebatch of db that you write
* **errptr**: Message of write batch to database.

**RETURNS**

* if success,th err is null ; others status set to indicate the error.

### writebatch_is_valid

```c++
int writebatch_is_valid(db_writebatch_t *bat);
```

判断writebatch是否有效。

**PARAMETERS**

* **bat**: the writebatch of db that you write

**RETURNS**

* return true (non-zero) if bat is valid, else return false (0)

## WriteBatch (nonatomic) APIs

这组API表示的是非原子性操作的write batch，使用方法与原子性操作的write batch一样。

### shannon_db_writebatch_create_nonatomic

```c++
db_writebatch_t *shannon_db_writebatch_create_nonatomic();
```

create the writebatch for the database.

**PARAMETERS**

无

**RETURNS**

* the datastruct of db_writebatch_t * writebatch

### shannon_db_writebatch_destroy_nonatomic

```c++
void shannon_db_writebatch_destroy_nonatomic(db_writebatch_t *batch);
```

release the writebatch for the database

**PARAMETERS**

* **batch**: the writebatch of db that you destroy

**RETURNS**

* void

### shannon_db_writebatch_clear_nonatomic

```c++
void shannon_db_writebatch_clear_nonatomic(db_writebatch_t *batch);
```

clear the writebatch that you given;

**PARAMETERS**

* batch: the writebatch of db that you clear

**RETURNS**

* void

### shannon_db_writebatch_put_nonatomic

```c++
int shannon_db_writebatch_put_nonatomic(db_writebatch_t *batch, 
                                        const char *key, unsigned int key_len,
                                        const char *value, unsigned int value_len, 
                                        char **err);
```

put kvdata to the writebatch

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **value**: the buffer of data that you get from database.
* **value_len**:  the buffer of data value that you want to get
* **errptr**: Message of put kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_put_cf_nonatomic

```c++
int shannon_db_writebatch_put_cf_nonatomic(
    db_writebatch_t *batch, 
    struct shannon_cf_handle *column_family,
    const char *key, unsigned int klen, 
    const char *val, unsigned int vlen,
    char **err);
```

put kvdata to the writebatch of columnfamily

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **column_family**: the handle of columnfamily you want to put. contains information of columnfamily
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **value**: the buffer of data that you get from database.
* **value_len**:  the buffer of data value that you want to get
* **errptr**: Message of put kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_delete_nonatomic

```c++
int shannon_db_writebatch_delete_nonatomic(db_writebatch_t *batch, 
                                           const char *key, unsigned int key_len, 
                                           char **err);
```

delete kvdata to the writebatch

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **errptr**: Message of delete kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_writebatch_delete_cf_nonatomic

```c++
int shannon_db_writebatch_delete_cf_nonatomic(
    db_writebatch_t *batch,
    struct shannon_cf_handle *column_family,
    const char *key, unsigned int klen,
    char **err);
```

delete kvdata to the writebatch of columnfamily

**PARAMETERS**

* **batch**: the writebatch of db that you put
* **column_family**: the handle of columnfamily you want to operate. contains information of columnfamily
* **key**: abstract class of key from data string.
* **key_len**: the length of data key you want to get
* **errptr**: Message of delete kvdata to writebatch.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_write_nonatomic

```c++
void shannon_db_write_nonatomic(struct shannon_db *db, 
                                const struct db_writeoptions *opt,
                                db_writebatch_t *batch, 
                                char **err);
```

write writebatch to the database

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **opt**: the writeoptions of kv for put data to the device.
* **batch**: the writebatch of db that you write
* **errptr**: Message of write batch to database.

**RETURNS**

* if success,th err is null ; others status set to indicate the error.

### writebatch_is_valid_nonatomic

```c++
int writebatch_is_valid_nonatomic(db_writebatch_t *bat);
```

判断writebatch是否有效。

**PARAMETERS**

* **bat**: the writebatch of db that you write

**RETURNS**

* return true (non-zero) if bat is valid, else return false (0)

## SST APIs

### shannondb_analyze_sst

```c++
int shannondb_analyze_sst(char *filename, int verify, char *dev_name, char *db_name);
```

analyze sst file form rocksdb or leveldb and put in shannondb

**PARAMETERS**

* **filename**: the filename of sst that you want to analyze
* **verify**: if verify, default false.
* **dev_name**: the shannondb target name of data key you want to put
* **dbname**: the dbname of data key you want to put

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_build_sst

```c++
int shannon_db_build_sst(char *devname, 
                         struct shannon_db *db,
                         struct db_options *options,
                         struct db_readoptions *roptions,
                         struct db_sstoptions *soptions);
```

build sst file of shannondb for back-up data

**PARAMETERS**

* **devname**: the shannondb target name of data key you want to put
* **db**: the handle of database you want to operate. contains information of database
* **options**: the options of kv for open database to the device.
* **roptions**: the readoptions of kv for gett data to the device.
* **soptions**: the sst_options of kv for build_sst data to the device.

**RETURNS**

* status set to indicate different result. if success, return 0;others status set to indicate the error.

### shannon_db_sstoptions_create

```c++
struct db_sstoptions *shannon_db_sstoptions_create();
```

create the sstoption for the database

**PARAMETERS**

无

**RETURNS**

* the option to generate sstfile

### shannon_db_sstoptions_destroy

```c++
void shannon_db_sstoptions_destroy(struct db_sstoptions *opt);
```

release the options for the database to given the options

**PARAMETERS**

* **opt**: the options of kv to operate the database.

**RETURNS**

* void

### shannon_db_sstoptions_set_compression

```c++
void shannon_db_sstoptions_set_compression(struct db_sstoptions *opt, unsigned char v);
```

set the compression type of sstoptions.

**PARAMETERS**

* **options**: options of user to build sstfile
* **v**: the type that you want to set: default nocompression.

**RETURNS**

* void

### shannon_db_sstoptions_set_data_interval

```c++
void shannon_db_sstoptions_set_data_interval(struct db_sstoptions *opt, int interval);
```

set the interval data block of sstoptions.

**PARAMETERS**

* **options**: options of user to build sstfile
* **interval**: the interval that you want to build sst: default 16.

**RETURNS**

* void

### shannon_db_sstoptions_set_is_rocksdb

```c++
void shannon_db_sstoptions_set_is_rocksdb(struct db_sstoptions *opt, unsigned char v);
```

if the sst file to support rocksdb.

**PARAMETERS**

* **options**: options of user to build sstfile
* **v**: if the sst file to support rocksdb. default true.

**RETURNS**

* void

### shannon_db_sstoptions_set_block_size

```c++
void shannon_db_sstoptions_set_block_size(struct db_sstoptions *opt, unsigned long size);
```

set the size of data_block that you build of sstoptions.

**PARAMETERS**

* **options**: options of user to build sstfile
* **size**: the size of data_block sst that you want to set .

**RETURNS**

* void

## Iterator APIs

### shannon_db_create_iterator

```c++
struct shannon_db_iterator *shannon_db_create_iterator(
    struct shannon_db *db, 
    const struct db_readoptions *opt);
```

create the iterator for the database.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **opt**: the readoptions of kv for get data to the device.

**RETURNS**

* the datastruct of db_iterator * iterator

### shannon_db_create_iterator_cf

```c++
struct shannon_db_iterator *shannon_db_create_iterator_cf(
    struct shannon_db *db, 
    const struct db_readoptions *options,
    struct shannon_cf_handle *column_family);
```

create the iterator of columnfamily.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **options**: the readoptions of kv for get data to the device.
* **column_family**: the handle of columnfamily you want to operate. contains information of columnfamily

**RETURNS**

* the datastruct of db_iterator * iterator

### shannon_db_create_prefix_iterator

```c++
struct shannon_db_iterator *shannon_db_create_prefix_iterator(
    struct shannon_db *db, 
    const struct db_readoptions *opt, 
    const char *prefix,
    unsigned int prefix_len);
```

create the iterator with prefix.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **opt**: the readoptions of kv for get data to the device.
* **prefix**: the prefix of key that used to iterate the database.
* **prefix_len**: the length of prefix

**RETURNS**

* the datastruct of db_iterator * iterator

### shannon_db_create_prefix_iterator_cf

```c++
struct shannon_db_iterator *shannon_db_create_prefix_iterator_cf(
    struct shannon_db *db, 
    const struct db_readoptions *options,
    struct shannon_cf_handle *column_family, 
    const char *prefix,
    unsigned int prefix_len);
```

create the iterator for column family with prefix.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **options**: the readoptions of kv for get data to the device.
* **column_family**: the handle of columnfamily you want to operate. contains information of columnfamily
* **prefix**: the prefix of key that used to iterate the database.
* **prefix_len**: the length of prefix

**RETURNS**

* the datastruct of db_iterator * iterator

### shannon_db_create_iterators

```c++
void shannon_db_create_iterators(struct shannon_db *db,
                                 const struct db_readoptions *opts,
                                 struct shannon_cf_handle **column_families,
                                 struct shannon_db_iterator **iterators,
                                 int num_column_families, 
                                 char **err);
```

create the iterators for all columnfamilies of database.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **options**: the readoptions of kv for get data to the device.
* **column_families**: the collections of all columnfamilies you want to operate. contains information of columnfamily
* **iterators**: the collection of all iterators you want to create
* **num_column_families**: number of columnfamily you want to create iterator for it.
* **errptr**: Message of create_iterators to database.

**RETURNS**

* if success,th err is null ; others status set to indicate the error.

### shannon_db_create_prefix_iterators

```c++
void shannon_db_create_prefix_iterators(struct shannon_db *db, 
                                        const struct db_readoptions *opts,
                                        struct shannon_cf_handle **column_families,
                                        struct shannon_db_iterator **iterators, 
                                        int num_column_families,
                                        const char *prefix, 
                                        unsigned int prefix_len, 
                                        char **err);
```

create the iterator of columnfamily.

**PARAMETERS**

* **db**: the database of shannon_db that you want to delete
* **options**: the readoptions of kv for get data to the device.
* **column_families**: the collections of all columnfamilies you want to operate. contains information of columnfamily
* **iterators**: the collection of all iterators you want to create
* **num_column_families**: number of columnfamily you want to create iterator for it.
* **prefix**: the prefix of key that used to iterate the database.
* **prefix_len**: the length of prefix
* **errptr**: Message of create_iterators to database.

**RETURNS**

* the datastruct of db_iterator * iterator

### shannon_db_iter_destroy

```c++
void shannon_db_iter_destroy(struct shannon_db_iterator *iterator);
```

destroy the iterator that you given;

**PARAMETERS**

* **iterator**: the iterator to be handled

**RETURNS**

* void

### shannon_db_free_iter

```c++
void shannon_db_free_iter(struct shannon_db_iterator *iterator);
```

free the iterator that you given;

**PARAMETERS**

* **iterator**: the iterator to be handled

**RETURNS**

* void

### shannon_db_iter_get_error

```c++
void shannon_db_iter_get_error(const struct shannon_db_iterator *iterator, char **err);
```



**PARAMETERS**

* **iterator**: the iterator to be handled
* **err**: store error message

**RETURNS**

* void

### shannon_db_iter_valid

```c++
int shannon_db_iter_valid(const struct shannon_db_iterator *iter);
```

check if the iterator is valid;

**PARAMETERS**

* **iterator**: the iterator to be handled

**RETURNS**

* if valid ,return 1; else return 0.

### shannon_db_iter_seek_to_first

```c++
void shannon_db_iter_seek_to_first(struct shannon_db_iterator *iter);
```

move the iterator to the first kvdata of database

**PARAMETERS**

* **iter**: the iterator to be handled

**RETURNS**

* if success,return 0; others status set to indicate the error.

### shannon_db_iter_seek_to_last

```c++
void shannon_db_iter_seek_to_last(struct shannon_db_iterator *iter);
```

move the iterator to the last kvdata of database

**PARAMETERS**

* **iter**: the iterator to be handled

**RETURNS**

* if success,return 0; others status set to indicate the error.

### shannon_db_iter_seek

```c++
void shannon_db_iter_seek(struct shannon_db_iterator *iter, 
                          const char *key,
                          unsigned int key_len);
```

move the iterator to the position of kvdata that you seek

**PARAMETERS**

* **iter**: the iterator to be handled
* **key**: the value of key that you want to seek
* **key_len**: the length of data key you want to get

**RETURNS**

* void

### shannon_db_iter_seek_for_prev

```c++
void shannon_db_iter_seek_for_prev(struct shannon_db_iterator *iter,
                                   const char *key, 
                                   unsigned int key_len);
```

move the iterator to prev position of kvdata that you seek

**PARAMETERS**

* **iter**: the iterator to be handled
* **key**: the value of key that you want to seek
* **key_len**: the length of data key you want to get

**RETURNS**

* void

### shannon_db_iter_move_next

```c++
void shannon_db_iter_move_next(struct shannon_db_iterator *iter);
```

move the iterator to the next position of database

**PARAMETERS**

* **iter**: the iterator to be handled

**RETURNS**

* void

### shannon_db_iter_move_prev

```c++
void shannon_db_iter_move_prev(struct shannon_db_iterator *iter);
```

move the iterator to the prev position of database

**PARAMETERS**

* **iter**: the iterator to be handled

**RETURNS**

* void

### shannon_db_iter_cur_key

```c++
void shannon_db_iter_cur_key(struct shannon_db_iterator *iter, 
                             char *key,
                             unsigned int buf_size, 
                             unsigned int *key_len);
```

get the key data from the cur position of iterator

**PARAMETERS**

* **iter**: the iterator to be handled
* **key**: the value of data key
* **buf_size**: the length of data key you want to get
* **key_len**: the pointer to the length of key get from database.

**RETURNS**

* void

### shannon_db_iter_cur_value

```c++
void shannon_db_iter_cur_value(struct shannon_db_iterator *iter, 
                               char *value,
                               unsigned int buf_size, 
                               unsigned int *value_len);
```

get the value data from the cur position of iterator

**PARAMETERS**

* **iter**: the iterator to be handled
* **value**: the buffer of value you get from iterator
* **buf_size**: the length of data value you want to get
* **value_len**: the pointer to the length of value get from database.

**RETURNS**

* void

### shannon_db_iter_cur_kv

```c++
void shannon_db_iter_cur_kv(struct shannon_db_iterator *iter, 
                            char *key, unsigned int keybuf_size, 
                            char *value, unsigned int valbuf_size, 
                            unsigned int *value_len, unsigned int *key_len, 
                            __u64 *timestamp);
```

获取当前iterator指向的键值对。

**PARAMETERS**

* **iter**: the iterator to be handled
* **key**: the value of data key
* **keybuf_size**: the length of data key you want to get
* **value**: the buffer of value you get from iterator
* **valbuf_size**: the length of data value you want to get
* **value_len**: the pointer to the length of value get from database.
* **key_len**: the pointer to the length of key get from database.
* **timestamp**

**RETURNS**

* void

# 使用示例

可参考相关test测试文件。
