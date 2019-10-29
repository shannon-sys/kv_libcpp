#Kvlib_C常用接口函数及使用说明 Version 2.2
### Version 2.2 2019/1/10 修订
#目录
[TOC]
##一、概述
Kvlib_c是为Shannon高性能key-value引擎开发的C语言库,仿照Rocksdb的功能实现，实现了kv引擎的基本功能，由于使用的是直接支持kv的ssd，中间省去了一层文件系统，性能会比Rocksdb高好几倍。
##二、使用
注意在下面实例之中没有都判断err正确性，在工程之中应当加上，**err由底层创建，不需要手动销毁，底层会自动将内存重新利用
###1、数据库的打开创建和删除
####什么是kv数据库
支持从key到value的映射，可以将key-value存入数据库中，再使用key将对应的数据库之中相应的key-value值读出来，也可以根据key删除相应的数据，这就是put/get/del操作
####数据库的基本操作
####创建数据库
在打开的时候设置options.create_if_missing = true;就会自动创建新的数据库
```
typedef struct db_options {
	/* default: 1 */
	int create_if_missing;
	/* not supported yet */
	int compression;
} db_options_t;
```
####打开数据库
```cpp
extern shannon_db_t *shannon_db_open(const struct db_options *opt,
			const char *dev_name, const char *db_name, char **errptr);
			//打开数据库需要传入相关参数，数据库操作、数据库名、设备名（默认为/dev/kvdev0），这个接口和下面的有所区别，这个会尝试打开默认只有一个名为define的ColumnFamily的数据库，成功或者失败都会返回相关状态，可以根据返回的errptr判断。如果打开成功，errptr为null，否则会返回相应信息。
open the kv deviece by dev_name and open the database by
db_name , so you must given the parameters.
Return:
struct shannon_db *db;
on success ,the db will contain shannon_db information .
```
####关闭数据库
```
extern void shannon_db_close(struct shannon_db *db);
//关闭数据并且释放内存 需要传入db
```
####删除数据库
```
extern void shannon_destroy_db(struct shannon_db *db, char **errptr);
//从数据库之中将数据库整个删除，如果成功，errptr为null，否则会返回相应信息。
destroy the database on kv deviece.
Return:void
Parameters:
db :the database of shannond_db that you want to destroy.
errptr:Message of destroy shannon_db.
```
####DBOptions
```
typedef struct db_options {
	int create_if_missing;
	// default: 1 
	//数据库不存在则创建，默认是1
	
	int compression;
	//暂不支持
} db_options_t;

```
####ColumnFamily的打开创建和删除
#####什么是ColumnFamily
ColumnFamily是数据库之中的一个结构，一个DB(数据库)下可以有多个ColumnFamily，但是一个ColumnFamily只对应一个DB，在输入数据的时候必须指定到是储存到哪个DB的哪个ColumnFamily，然后数据会储存到指定的位置。同样，获取或者删除数据的时候，也需要指定某个ColumnFamily，才能够对对应的数据进行操作。每个ColumnFamily有自己的配置，数据读写操作也相应的独立。
#####创建ColumnFamily
```cpp
extern struct shannon_cf_handle *shannon_db_create_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err);
//在数据库中创建一个ColumnFamily，根据column_family_name，并且返回column_family的指针
/*
Return:
if success,return shannon_cf_handle, err is null .
Parameters:
db :the database of shannon_db that you want to create column
db_options :the options of kv to create columnfamily.
column_family_name: name of columnfamily you want create
errptr:Message of create columnfamily for shannon_db.
*/
```
#####打开ColumnFamily
```cpp
extern struct shannon_cf_handle *shannon_db_open_column_family(struct shannon_db *db,
		const struct db_options *column_family_options, const char *column_family_name, char **err);
//打开一个ColumnFamily，根据传入的相关信息并且返回相应的结果，返回handle的指针
/*
Return:void
if success, the err is null .
Parameters:
  db :the database of shannon_db that you want to create column
  db_options :the options of kv to create columnfamily.
  column_family_name: name of columnfamily you want create
  errptr:Message of open the columnfamily shannon_db.
*/
extern struct shannon_db *shannon_db_open_column_families(const struct db_options *options,
				const char *dev_name, const char *db_name,
				int num_column_families, const char **column_family_names,
				const struct db_options **column_family_options,
				struct shannon_cf_handle **column_family_handles, char **err);
//打开一组column_family，根据传入的信息，返回shannon_db结构体的指针
/*
Return:struct shannon_db *db;
on success ,the db will contain db information , and the err is null
Parameters:  db_options :the options of kv to create columnfamily.
  devname :Path of the device to open e.g. “/dev/kvdev00”
  dbname :Path of the databse to open e.g. “test.db”
  num_column_families :num of columnfamilies you want open.
  column_family_names: name of columnfamily you want open
  column_family_handles :the handles of columnfamily you want
to open.
  errptr:Message of open all columnfamilies.
  */
```
#####删除ColumnFamily
```cpp
extern void shannon_db_drop_column_family(struct shannon_db *db, 
		struct shannon_cf_handle *handle, char **err);
//删除一个column_family
/*
Return:void
if success, the err is null .
Parameters:
   db :the database of shannon_db that you want to create column
   handle :the handle of columnfamily you want to drop . contains
information of columnfamily
   errptr:Message of drop columnfamilyof shannon_db.
*/

extern void shannon_db_column_family_handle_destroy(struct shannon_cf_handle *handle);
//释放column_family_handle_destroy所占用的内存，在删除column_family之后必须释放内存
```
#####ColumnFamilyOptions
```
typedef struct db_options ;
//就是上面的dboption，使用相同的options
```
#####获取数据库已经创建的数据库中的ColumnFamily列表
```cpp
extern char **shannon_db_list_column_families(const struct db_options *options,
	 const char *dev_name, const char *dbname,unsigned int *cf_count, char **err);
extern void shannon_db_list_column_families_destroy(char **cf_list, unsigned int cf_count);
//列出所有的column_families的名字，返回一个字符串数组
/*
Return:char **names
if success return names contains all columnfamily name.
Parameters:
    db_options :the options of kv to create columnfamily.
    devname :Path of the device to open e.g. “/dev/kvdev00”
    dbname :Path of the databse to open e.g. “test.db”
    cf_count : return the number of columnfamilies .
    errptr:Message of list all columnfamilies.
*/
```

####使用实例

#####创建options
```cpp
//创建db_options
struct db_options *options;
options = malloc(sizeof(struct db_options));
memset(options, 0, sizeof(struct db_options));
options->create_if_missing = 1;
options->compression = 0;

//创建db_writeoptions
struct db_writeoptions *options;
options = malloc(sizeof(struct db_writeoptions));
memset(options, 0, sizeof(struct db_writeoptions));
options->sync = 1;
options->fill_cache = 1;

//创建db_readoptions
struct db_readoptions *options;
options = malloc(sizeof(struct db_readoptions));
memset(options, 0, sizeof(struct db_readoptions));
options->fill_cache = 1;

```
#####数据库基本操作
```
struct shannon_db *db;
db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
//销毁数据库

shannon_cf_handle_t *cfh;
cfh = shannon_db_create_column_family(db, options, "cf1", &err);
shannon_db_column_family_handle_destroy(cfh);

shannon_db_close(db);
shannon_destroy_db(db, &err);

char **column_fams = shannon_db_list_column_families(options, devname, dbname, &cflen, &err);
shannon_db_list_column_families_destroy(column_fams, cflen);
```
#####ColumFamilies打开数据库
```
const char *cf_names[2] = {"default", "cf1"};//创建cf_names
const struct db_options *cf_opts[2] = {cf_options, cf_options};//创建cf_opts
int num_column_families = 2;
int num;
shannon_cf_handle_t **handles = (shannon_cf_handle_t **) malloc (sizeof (shannon_cf_handle_t *) * num_column_families);// 创建 handles 对每个 column_familes
for (num = 0; num < num_column_families; num++) {
	handles[num] = (shannon_cf_handle_t *) malloc (sizeof (shannon_cf_handle_t));// 将column_familes储存起来
}
db = shannon_db_open_column_families(options, devname, dbname, 2, cf_names, cf_opts, handles, &err);// 根据column_familes打开数据库
```
###2、数据的增删改查
数据是以key-value对的形式储存在数据库里的，在对数据的基本操作的时候需要指定对应的ColumnFamily和数据库，主要是用DB类的接口进行操作
####主要api
#####写操作的错误码
```
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
#####数据库的put/get/delete接口
```cpp
extern void shannon_db_put(struct shannon_db *db, const struct db_writeoptions *opt,
				const char *key, unsigned int key_len,
				const char *value, unsigned int value_len, char **err);
//向数据库的default column_families 写入一条key-value数据，如果数据已经存在，则覆盖，返回相应状态。

extern int shannon_db_delete(struct shannon_db *db, const struct db_writeoptions *opt,
				const char *key, unsigned int key_len, char **err);
  //从数据库的default column_families删除一条key对应的数据
  
extern int shannon_db_get(struct shannon_db *db, const struct db_readoptions *opt,
				const char *key, unsigned int key_len,
				char *value_buf, unsigned int buf_size, unsigned int *value_len,
				char **err);
//从数据库的default column_families获取一条key对应的数据
//get的返回值表示获取数据的状态，状态表如下：
//返回值为负数则代表出现error，可以用来快速判断是否获取成功，而err仍旧会返回相应的错误信息，比如如果没有找到key则返回 -501

extern void shannon_db_put_cf(struct shannon_db *db, const struct db_writeoptions *options,
			struct shannon_cf_handle *column_family, const char *key, unsigned int keylen,
			const char *val, unsigned int vallen, char **err);
//向数据库的对应的column_families 写入一条key-value数据，如果数据已经存在，则覆盖，返回相应状态。

extern int shannon_db_delete_cf(struct shannon_db *db, const struct db_writeoptions *options,
			struct shannon_cf_handle *column_family,
			const char *key, unsigned int keylen, char **err);
//从数据库之中对应的column_families删除一条key对应的数据

extern int shannon_db_get_cf(struct shannon_db *db, const struct db_readoptions *options,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int keylen, char *value_buf, unsigned int buf_size, unsigned int *value_len,
			char **err);
//从数据库之中对应的column_families获取一条key对应的数据，赋值给value，返回值和上面的get一样
```
####ReadOptions和ReadOptions
```cpp

typedef struct db_readoptions {
	/* default: 1 */
	int fill_cache;
	//是否创建相应缓存
	
	__u64 snapshot;
	//设置读取数据的时间点
} db_readoptions_t;

typedef struct db_writeoptions {

	int sync;
	//是否立刻写入磁盘，只支持立刻写入，因为是使用的高性能kv
	/* only support sync=1 */
	
	int fill_cache;
	/* default: 1 */
	//是否创建相应缓存
} db_writeoptions_t;
```
####实例，数据的增删改查
```cpp
memset(keybuf, 0 , 100);
memset(valbuf, 0 , 100);
memset(buffer, 0 , 100);
snprintf(keybuf, sizeof(keybuf), "key%d", i);
snprintf(valbuf, sizeof(valbuf), "value%d", i);
shannon_db_put(db, woptions, keybuf, strlen(keybuf), valbuf, strlen(valbuf), &err); // put操作
shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);// get操作
if (err) { // err的使用
	printf("get ioctl failed\n");
	exit(-1);
}
if (strcmp(valbuf, buffer)) {
	printf("dismatch value:%s buffer:%s\n", valbuf, buffer);
	exit(-1);
}
shannon_db_delete(db, woptions, keybuf, strlen(keybuf), &err);// delete操作
```
###3、WriteBatch 和 ReadBatch 的使用
####什么是WriteBatch
WriteBatch就是大量的写（删除）操作在一起作为一个整体，具有原子性的特点，它们是一同处理的命令，一同成功一同失败，如果一批操作之中有一个失败了，那么这一批操作都会失败，要不然全部成功要不然全部失败（如果写入中途断电，那么断电之前写入的一部分数据驱动会认为其为无效的数据）。而WriteBatch是先把需要做的一批操作放到一个WriteBatch之中储存，然后调用Write接口，同时处理这一批操作。目前每个WriteBatch最多只支持8M的命令总长度。
####什么是WriteBatch  nonatomic
在WriteBatch原子性接口的需求之外，可能还有非原子性的需求，在其中一条条命令失败的时候，后面的命令不会执行，而前面已经执行的命令写入卡中的数据是有效的，仍然会返回异常，由于WriteBatch底层采用多线程写入，无法确定数据的执行顺序，在出错的情况下无法得知哪些key已经被写入，需要getkv的接口查找，其中非原子性的接口在某些的情况下会更节省空间一些。
####什么是ReadBatch
ReadBatch是一个将一系列读，封装成一个批量读的接口，可以提升读的效率，这个接口也是一个原子性接口，保证数据返回的一致性
####主要api
#####WriteBatch相关接口
```cpp
typedef void db_writebatch_t;
//db_writebatch_t的指针结构，一般为char*，void*方便转换

extern int writebatch_is_valid(db_writebatch_t *bat);
//判断writebatch是否有效

extern db_writebatch_t *shannon_db_writebatch_create();、
//创建writebatch

extern void shannon_db_writebatch_destroy(db_writebatch_t *);
//销毁writebatch，并且清除内存
extern void shannon_db_writebatch_clear(db_writebatch_t *);
//只清除writebatch之中的数据

/*
 * only the value buffer address is saved, so different kv in
 * a writebatch can not use the same value buffer.
 */
extern int shannon_db_writebatch_put(db_writebatch_t *,
			const char *key, unsigned int key_len, const char *value,
			unsigned int value_len, char **err);
//writebatch的写入操作，写入默认的column_family，将命令加入writebatch之中，返回错误码

extern int shannon_db_writebatch_put_cf(db_writebatch_t *, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val,
			unsigned int vlen, char **err);
//writebatch，写入指定的column_family，将命令加入writebatch之中，返回错误码

extern int shannon_db_writebatch_delete(db_writebatch_t *,
			const char *key, unsigned int key_len, char **err);
//writebatch的删除操作，删除默认的column_family之中的数据，将命令加入writebatch之中，返回错误码

extern int shannon_db_writebatch_delete_cf(db_writebatch_t *,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int klen, char **err);
//writebatch的删除操作，删除指定的column_family之中的数据，将命令加入writebatch之中，返回错误码

extern void shannon_db_write(struct shannon_db *db, const struct db_writeoptions *opt,
				db_writebatch_t *, char **err);
//执行writebatch之中的所有命令
```
#####WriteBatch nonatomic相关接口
```
/* write batch nonatomic */
//代表非原子性的writebatch操作，使用方法和原子性的writebatch相同
extern int writebatch_is_valid_nonatomic(db_writebatch_t *bat);
extern db_writebatch_t *shannon_db_writebatch_create_nonatomic();
extern void shannon_db_writebatch_destroy_nonatomic(db_writebatch_t *);
extern void shannon_db_writebatch_clear_nonatomic(db_writebatch_t *);
extern int shannon_db_writebatch_put_nonatomic(db_writebatch_t *,
			const char *key, unsigned int key_len, const char *value,
			unsigned int value_len, char **err);
extern int shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val,
			unsigned int vlen, char **err);
extern int shannon_db_writebatch_delete_nonatomic(db_writebatch_t *,
			const char *key, unsigned int key_len, char **err);
extern int shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *,
			struct shannon_cf_handle *column_family, const char *key,
			unsigned int klen, char **err);
extern void shannon_db_write_nonatomic(struct shannon_db *db, const struct db_writeoptions *opt,
				db_writebatch_t *, char **err);
```
#####read batch相关接口
```
/* read batch */
typedef void db_readbatch_t;
//readbatch的结构指针
extern db_readbatch_t *shannon_db_readbatch_create();
//创建readbatch
extern int shannon_db_readbatch_get(db_writebatch_t *,
			const char *key, unsigned int key_len, const char *val, unsigned int value_len, char **err);
//批量读接口，从默认的column_family，将命令加入readbatch之中
extern int shannon_db_readbatch_get_cf(db_writebatch_t *, struct shannon_cf_handle *column_family,
			const char *key, unsigned int klen, const char *val,
			unsigned int vlen, char **err);
//批量读接口，从指定的column_family读取，将命令加入readbatch之中

extern void shannon_db_read(struct shannon_db *db, const struct db_readoptions *options,
			db_readbatch_t *bat, unsigned int *valen, char **err);
//执行批量读的操作

extern void shannon_db_readbatch_destroy(db_readbatch_t *);
//删除readbatch并且清理内存

extern void shannon_db_readbatch_clear(db_readbatch_t *batch);
//清除readbatch之中的命令数据

```
####实例，WriteBatch 的使用
```	
int test_writebatch(int times, struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int i;
	int ret;
	int value_len;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *value;
	char *err = NULL;
	for (i = 0; i < times; i++) {
		db_writebatch_t *wb = shannon_db_writebatch_create();
		memset(key, 0 , BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		value = randomstr();
		shannon_db_writebatch_put(wb, key, strlen(key), value, strlen(value), &err);
		shannon_db_write(db, woptions, wb, &err);
		memset(buffer, 0 , BUF_LEN);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		free(value);
		shannon_db_writebatch_destroy(wb);
	}
}
```
####实例，ReadBatch 的使用
```	
int test_readbatch(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 100;
	int i;
	int ret;
	int total_value_len;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *value;
	char *err = NULL;

	for (i = 0; i < times; i++) {
		db_readbatch_t *rb = shannon_db_readbatch_create();
		memset(key, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_readbatch_get(rb, key, strlen(key), buffer, BUF_LEN, &err);
		if (err) {
			printf("shannon_db_readbatch_get() ioctl failed\n");
			exit(-1);
		}
		shannon_db_read(db, roptions, rb, &total_value_len, &err);
		if (err) {
			printf("test_readbatch_case1() failed\n");
			exit(-1);
		}
		memset(buffer, 0 , BUF_LEN);
		shannon_db_readbatch_destroy(rb);
	}

	return 0;
}
```
###4、Snapshot 的使用
####什么是Snapshot
Snapshot是关于指定数据集合的一个完全可用拷贝，该拷贝包括相应数据在某个时间点（拷贝开始的时间点）的映像。在这里Snapshot是一个时间点，通过这个的信息，可以获取在Snapshot创建时刻的数据，而不会被Snapshot创建时刻之后来的命令影响。在很多时刻都会用到，获取数据的时候需要保证数据的一致性，不会出现一半新数据一半是旧数据的情况。但是由于Snapshot需要占用旧数据不让其释放，会相当的占用内存，这里就需要Snapshot在不使用的时候及时释放，这里Snapshot是一个非持久的，断电就会消失，需要持久化的Snapshot可以使用checkpoint。
####主要api

```
extern __u64 shannon_db_create_snapshot(struct shannon_db *db, char **err);
//创建snapshot

extern void shannon_db_release_snapshot(struct shannon_db *db, __u64 snapshot, char **err);
//释放snapshot

extern void shannon_db_readoptions_set_snapshot(struct db_readoptions *opt, __u64 snap);
//为opt设置snapshot
```
####实例，Snapshot 的使用
```
int loop_times = 100;
int i;
int ret;
int value_len;
__u64 timestamp;
char key[BUF_LEN];
char value1[BUF_LEN];
char value2[BUF_LEN];
char buffer[BUF_LEN];
char *err = NULL;
for (i = 0; i < loop_times; i++) {
	memset(key, 0, sizeof(key));
	memset(buffer, 0, sizeof(buffer));
	memset(value1, 0, sizeof(value1));
	memset(value2, 0, sizeof(value2));
	snprintf(key, BUF_LEN, "%d", i);
	snprintf(value1, BUF_LEN, "%d", i);
	snprintf(value2, BUF_LEN, "%d", i + 1);
	shannon_db_put(db, woptions, key, strlen(key), value1, strlen(value1), &err);

	timestamp = shannon_db_create_snapshot(db, &err);

	shannon_db_put(db, woptions, key, strlen(key), value2, strlen(value2), &err);

	shannon_db_readoptions_set_snapshot(roptions, timestamp);
	
	shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);

	shannon_db_release_snapshot(db, timestamp, &err);
}
```
###5、Iterator 的使用
####什么是iterator
Iterator是数据库的迭代器，每个columnfamily可以对应生成一个，用Iterator和容器的Iterator一样，可以遍历columnfamily中的数据，按照字符串的顺序遍历，支持遍历首个，最后一个，下一个上一个和跳到某个字符串开头的key处（以某个字符串为前缀)。Iterator会自动生成一个Snapshot，来指定遍历数据的时间点，会占用资源，用完应当及时释放。
####主要api
#####iterator创建删除相关接口
```cpp
struct shannon_db_iterator {
	struct shannon_db db;
	//指定某个db
	
	int cf_index;
	//指定column_family
	
	int prefix_len;
	//结果长度
	
	char *err;
	//错误信息
	
	__u64 snapshot;
	//创建的快照
	
	int iter_index;
	//自身编号
	
	int valid;
	//是否在有效位置
	
	unsigned char prefix[128];
	//结果
};

extern struct shannon_db_iterator *shannon_db_create_iterator(struct shannon_db *db, const struct db_readoptions *opt);
//创建一个shannon_db_iterator，针对默认的column_family，返回指针

extern struct shannon_db_iterator *shannon_db_create_prefix_iterator(struct shannon_db *db, const struct db_readoptions *opt,const char *prefix, unsigned int prefix_len);
//根据prefix创建一个的shannon_db_iterator，返回指针

extern struct shannon_db_iterator *shannon_db_create_iterator_cf(struct shannon_db *db, const struct db_readoptions *options,struct shannon_cf_handle *column_family);
//创建一个shannon_db_iterator，针对指定的column_family，返回指针

extern struct shannon_db_iterator *shannon_db_create_prefix_iterator_cf(struct shannon_db *db, const struct db_readoptions *options,struct shannon_cf_handle *column_family, const char *prefix, unsigned int prefix_len);
//根据prefix创建一个shannon_db_iterator，针对指定的column_family，返回指针

extern void shannon_db_create_iterators(struct shannon_db *db, const struct db_readoptions *opts,
				struct shannon_cf_handle **column_families,
				struct shannon_db_iterator **iterators, int num_column_families, char **err);
//创建一组shannon_db_iterator，针对指定的column_family，返回指针数组

extern void shannon_db_create_prefix_iterators(struct shannon_db *db, const struct db_readoptions *opts,
				struct shannon_cf_handle **column_families,
				struct shannon_db_iterator **iterators, int num_column_families,
				const char *prefix, unsigned int prefix_len, char **err);
//根据prefix创建一组shannon_db_iterator，针对指定的column_family，返回指针数组
```
#####iterator移动查找数据的API接口
```cpp
extern void shannon_db_iter_destroy(struct shannon_db_iterator *iterator);
//销毁数据库之内的iterator
extern void shannon_db_free_iter(struct shannon_db_iterator *iterator);
//释放iterator内存
extern void shannon_db_iter_get_error(const struct shannon_db_iterator *, char **err);
//查看iterator的状态
extern int  shannon_db_iter_valid(const struct shannon_db_iterator *iter);
//判断iterator现在位置的key是否有效
extern void shannon_db_iter_seek_to_first(struct shannon_db_iterator *iter);
//iterator移动到第一条数据
extern void shannon_db_iter_seek_to_last(struct shannon_db_iterator *iter);
//iterator移动到最后一条数据
extern void shannon_db_iter_seek(struct shannon_db_iterator *iter, const char *key, unsigned int key_len);
//iterator移动到key为前缀的位置
extern void shannon_db_iter_seek_for_prev(struct shannon_db_iterator *iter, const char *key, unsigned int key_len);
//iterator移动到key为前缀的位置，从后向前移动
extern void shannon_db_iter_move_next(struct shannon_db_iterator *iter);
//iterator移动到下一条记录位置
extern void shannon_db_iter_move_prev(struct shannon_db_iterator *iter);
//iterator移动到上一条记录位置
extern void shannon_db_iter_cur_key(struct shannon_db_iterator *iter, char *key, unsigned int buf_size, unsigned int *key_len);
//iterator获取当前位置key的值
extern void shannon_db_iter_cur_value(struct shannon_db_iterator *iter, char *value, unsigned int buf_size, unsigned int *value_len);
//iterator获取当前位置value的值
```
####实例，Iterator 的使用
#####简单使用next
```cpp
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	shannon_db_iter_seek_to_first(iter);
	while (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		printf("key:%s len:%d val:%s len:%d\n", keybuf, keylen, valbuf, valuelen);
		shannon_db_iter_move_next(iter);
	}
```
#####计算数目
```
	shannon_db_iter_seek_to_first(iter);
	while (shannon_db_iter_valid(iter)) {
		count++;
		shannon_db_iter_move_next(iter);
	}
	return count;
```
#####反向遍历
```
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	shannon_db_iter_seek_to_last(iter);
	while (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		printf("key:%s len:%d val:%s len:%d\n", keybuf, keylen, valbuf, valuelen);
		shannon_db_iter_move_prev(iter);
	}
```
###6、构造sst 与解析sst 的功能介绍
####什么是sst
LevelDB 和RocksDB 数据库有几个重要的文件，包括内存数据的Memtable，分层数据存储的SST文件，版本控制的Manifest、Current文件，以及写Memtable前的WALlog.
其中磁盘数据存储文件格式为sst.从Level 0到Level N多层，每一层包含多个SST文件；单个SST文件容量随层次增加成倍增长；文件内数据有序， .sst文件内部布局都是一样的。
.sst文件内部布局都是一样的,不同的Block物理上的存储方式一致.但在逻辑上可能存储不同的内容，包括存储数据的Block，存储索引信息的Block，存储Filter的Block。
shannon_db为了进行数据的备份以及更好的移植和兼容leveldb(rocksdb), 提供了shannon_db_analyze_sst 和shannon_db_build_sst 接口.主要用来生成解析sst 和生成sst.

####主要api
#####shannon_db_analyze_sst 和shannon_db_build_sst 接口相关接口
```cpp
extern int shannondb_analyze_sst(char *filename, int verify, char *dev_name, char *db_name);
//解析sst 时，传入的参数为sst 所在的文件路径， 是否需要校验， 创建的设备devname 和数据库 的db_name.
	int ret = shannondb_analyze_sst(filename, verify, "/dev/kvdev0", "test_sst.db");
	if (ret != 0) {
		printf("analyze_sst failed\n");
		exit(-1);
	}

extern int shannon_db_build_sst(char *devname , struct shannon_db *db, struct db_options *options, struct db_readoptions *roptions, struct db_sstoptions *soptions);
//构造sst 时，传入的参数为  创建的设备devname， 需要备份的数据库 ， 以及db_options db_readoptions ，和生成sst需要的options.
	{
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
	}
	db = shannon_db_open(options,devname, dbname, &err);

	struct db_sstoptions *soptions = shannon_db_sstoptions_create();
	shannon_db_build_sst(devname, db, options, roptions, soptions);

	struct db_sstoptions *soptions = shannon_db_sstoptions_create();
生成的sst 在需要导入到rocksdb 或者 leveldb 数据库时，只需要调用相关的api 就可以直接导入。

	StartPhase("create_objects");
	options = rocksdb_options_create();
	roptions = rocksdb_readoptions_create();
	woptions = rocksdb_writeoptions_create();

	StartPhase("open");
	rocksdb_options_set_create_if_missing(options, 1);

	db = rocksdb_open(options, dbname, &err);
	CheckNoError(err);

	const char *sstfilename1 = "/tmp/rocksdb/test_sst.db/default000000.sst";
	const char* file_list[1] = {sstfilename1};

	rocksdb_ingestexternalfileoptions_t *ing_opt = rocksdb_ingestexternalfileoptions_create();
	rocksdb_ingest_external_file(db, file_list, 1, ing_opt, &err);

