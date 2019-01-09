#Kvlib_C++常用接口函数及使用说明 Version 2.0
### Version 2.0 2019/1/8 修订
#目录
[TOC]
##一、概述
Kvlib_c++是为Shannon高性能key-value引擎开发的C++库,仿照Rocksdb的功能实现，实现了kv引擎的基本功能，由于使用的是直接支持kv的ssd，中间省去了一层文件系统，性能会比Rocksdb高好几倍。
##二、使用
###1、数据库的打开创建和删除
####什么是kv数据库
支持从key到value的映射，可以将key-value存入数据库中，再使用key将对应的数据库之中相应的key-value值读出来，也可以根据key删除相应的数据，这就是put/get/del操作
####数据库的基本操作
####创建数据库
在打开的时候设置options.create_if_missing = true;就会自动创建新的数据库
####打开数据库
```cpp
namespace shannon {
static Status Open(const Options& options,
		const std::string& name,
		const std::string& device,
		DB** dbptr);
//打开数据库需要传入相关参数，数据库操作、数据库名、设备名，这个接口和下面的有所区别，这个会尝试打开默认只有一个名为define的ColumnFamily的数据库，如果相应数据库ColumnFamily不是默认，就会失败，成功或者失败都会返回相关状态，可以根据返回的Status判断。如果打开成功，会给DB*赋值成相应的数据库指针并且在函数内为其分配内存（注意在不需要时删除，防止内存泄露），之后就可以用这个指针操作数据库。
}
```
``` cpp
namespace shannon {
  static Status Open(const DBOptions& db_options,
		const std::string& name, const std::string& device,
		const std::vector<ColumnFamilyDescriptor>& column_families,
		std::vector<ColumnFamilyHandle*>* handles, DB** dbptr);
//和上面的的函数功能相同，打开数据库，但是需要指定ColumnFamilyHandle，并且数目和名字需要对应，否则会打开失败。对于创建过新ColumnFamily的数据库需要用这个打开。
}
```
####删除数据库
```
namespace shannon {
 Status DestroyDB(const std::string& device, const std::string& name, const Options& options);
}
//从数据库之中将数据库整个删除
```
####DBOptions
```
struct DBOptions {
//这里只列举部分
//在创建数据库的时候设置以下变量再传入可以实现相应功能

	bool create_if_missing = false;
	//数据库不存在则创建，默认否
	bool create_missing_column_families = false;
	//创建不存在的ColumnFamily，默认否
	bool error_if_exists = false;
	//如果打开时数据库存在则报错，默认否
}
```
####ColumnFamily的打开创建和删除
#####什么是ColumnFamily
ColumnFamily是数据库之中的一个结构，一个DB(数据库)下可以有多个ColumnFamily，但是一个ColumnFamily只对应一个DB，在输入数据的时候必须指定到是储存到哪个DB的哪个ColumnFamily，然后数据会储存到指定的位置。同样，获取或者删除数据的时候，也需要指定某个ColumnFamily，才能够对对应的数据进行操作。每个ColumnFamily有自己的配置，数据读写操作也相应的独立。
#####创建ColumnFamily
```cpp
namespace shannon {
class KVImpl : public DB {
  virtual Status CreateColumnFamily(const ColumnFamilyOptions& options,
				const std::string& column_family_name,
				ColumnFamilyHandle** handle) override;
//在数据库中创建一个ColumnFamily，根据column_family_name(会创建相应的内存在handle)

  virtual Status CreateColumnFamilies(const ColumnFamilyOptions& options,
			const std::vector<std::string>& column_family_names,
			std::vector<ColumnFamilyHandle*>* handles) override;
//在数据库中创建一组ColumnFamily,根据column_family_name(会创建相应的内存在handle)

  virtual Status CreateColumnFamilies(
		const std::vector<ColumnFamilyDescriptor>& column_families,
		std::vector<ColumnFamilyHandle*>* handles) override;
//在数据库中创建一组ColumnFamily,根据ColumnFamilyDescriptor
	}
}
```
#####获取ColumnFamily
用相应的名字构造一个ColumnFamilyDescriptor的对象，再传入Opendb函数之中，会返回相应的ColumnFamilyHandle指针
```cpp
  vector<string> familie_names;
  vector<ColumnFamilyDescriptor> column_families;
  column_families.push_back(ColumnFamilyDescriptor(familyname, ColumnFamilyOptions()));
```
#####删除ColumnFamily
```cpp
namespace shannon {
class KVImpl : public DB {
  virtual Status DropColumnFamily(ColumnFamilyHandle* column_family) override;
//从数据库中删除一个ColumnFamilyHandle

  virtual Status DropColumnFamilies(
		const std::vector<ColumnFamilyHandle*>& column_families) override;
//从数据库中删除一组ColumnFamilyHandle

  virtual Status DestroyColumnFamilyHandle(ColumnFamilyHandle* column_family) override;
//销毁column_family指针所指向的内存（同delete column_family），并不删除数据库之中的数据
	}
}
```
#####ColumnFamilyOptions
```
//一般使用默认的就好了，具体看源码

namespace shannon {
struct WriteOptions {
	  // Default: true
	  //是否需要立即写到盘上
	  bool sync;

	  //是否需要在内存里面缓存一份
	  // Default: true
	  bool fill_cache;

	  WriteOptions()
	      : sync(true),
	        fill_cache(true) {}
	  //一般调用这个接口构造一个默认的就好了
  };
}
```
#####获取数据库已经创建的数据库中的ColumnFamily列表
```cpp
namespace shannon {
  static Status DB::ListColumnFamilies(const DBOptions& db_options,
            const std::string& name,
            const std::string& device,
            std::vector<std::string>* column_families);
//根据数据库名和设备名，可以数据库之中获取所有的column_families的名字，并且用一个string的vector储存起来，可以利用这个函数获取所有的columnfamily名字，再打开数据库，若没有这个数据库，将返回失败。
}
```

####使用实例

#####创建ColumnFamily
```cpp
using namespace shannon;//命名空间
Status s;//收集返回的状态
std::string dbname = "testdb";// db的名字
std::string driverpath = "/dev/kvdev0";// 加载驱动的位置，一般默认为"/dev/kvdev0"设备
std::string name = "new_cf";// 新ColumnFamily的名字
DB *db;  // 创建储存db指针
Options options; // 构造默认的Options
options.create_if_missing = true;// 如果数据库不存在则创建数据库,创建数据库的方法
ColumnFamilyHandle *dle; // 创建储存的handles
s = DB::Open(options, dbname, driverpath, columnfamilies, db);//打开只有默认columnfamilies的数据库，只有在数据库还没有创建的时候可以使用
s = db->CreateColumnFamily(options, name, &dle);//创建一个columnfamilies在数据库之中
assert(s.ok());//输出结果状态
delete db;// 使用过后清除db
delete dle;//清除handle
```
#####打开旧的数据库并且销毁
```
using namespace shannon;//命名空间
Status s;//收集返回的状态
//这段函数的功能是打开已经创建过的数据库
std::string dbname = "testdb";// db的名字
std::string driverpath = "/dev/kvdev0";// 加载驱动的位置，一般默认为"/dev/kvdev0"设备
std::string name = "new_cf";// 新ColumnFamily的名字
DB *db;// 创建储存db指针
Options options;// 构造默认的Options
std::vector<ColumnFamilyHandle *> *handles;// 创建储存handles的指针数组
vector<string> familie_names; // 创建获取ColumnFamily名字列表的数组
vector<ColumnFamilyDescriptor> column_families; // 创建储存ColumnFamilyDescriptor的数组
s = DB::ListColumnFamilies(options, dbname, driverpath, &familie_names);// 获取现有dbname数据库里所有的ColumnFamilies的名字，并且返回给familie_names数组
for (auto familyname : familie_names)
{
   column_families.push_back(ColumnFamilyDescriptor(familyname, ColumnFamilyOptions()));// 为所有的名字构造一个ColumnFamilyDescriptor实例并且储存起来，之后打开db的时候需要使用
}
s = DB::Open(options, dbname, driverpath, column_families, handles, &db);// 传入ColumnFamilyDescriptor打开数据库，并且把ColumnFamily的信息获取出来储存到handles里面，可以进行接下来的操作
db->DropColumnFamilies(*handles) ;//删除handles里所有的ColumnFamily
shannon::DestroyDB(const std::string& device, const std::string& name, const Options& options);
```
###2、数据的增删改查
数据是以key-value对的形式储存在数据库里的，在对数据的基本操作的时候需要指定对应的ColumnFamily和数据库，主要是用DB类的接口进行操作
####主要api
```cpp
class KVImpl :class DB{
//只列出相关接口

  virtual Status Put(const WriteOptions& options,const Slice& key,const Slice& value);
  //向数据库的default column_families 写入一条key-value数据，如果数据已经存在，则覆盖，返回相应状态。

  virtual Status Delete(const WriteOptions& options, const Slice& key);
  //从数据库的default column_families删除一条key对应的数据

  virtual Status Get(const ReadOptions& options,const Slice& key,std::string* value);
//从数据库的default column_families获取一条key对应的数据

 virtual Status Put(const WriteOptions& options,
		ColumnFamilyHandle* column_family, const Slice& key,
		const Slice& value) override;
//向数据库的对应的column_families 写入一条key-value数据，如果数据已经存在，则覆盖，返回相应状态。

virtual Status Delete(const WriteOptions& options,
		ColumnFamilyHandle* column_family, const Slice& key) override;
//从数据库之中对应的column_families删除一条key对应的数据

  virtual Status Get(const ReadOptions& options,
		ColumnFamilyHandle* column_family, const Slice& key,
		std::string* value) override;
//从数据库之中对应的column_families获取一条key对应的数据，赋值给value
}
```
####ReadOptions和ReadOptions
```cpp
struct ReadOptions {

  bool verify_checksums;
  //如果为true，那么所有的key都会进行corr校验
  //默认false

  bool fill_cache;
  //这个数据是应该储存在缓存中
  //默认是true

  const Snapshot* snapshot;
  // 如果snapshot不为空，则根据snapshot查询数据（谁创建由谁删除，否则会占用大量资源）
  //如果snapshot为空，则创建当前时刻的临时snapshot查询数据，snapshot的时刻为当前时刻
  // 后面会提到snapshot的相关事宜

  ReadOptions()
      : verify_checksums(false),
        fill_cache(true),
        snapshot(NULL) {
  }
  //用这个接口获取一个默认的snapshot
};

// Options that control write operations
struct WriteOptions {
  bool sync;
  // 默认true
  // 是否需要立即写到盘上

  bool fill_cache;
  // 这个数据是应该储存在缓存中
  // 默认是true

 //ues this interface to make a default WriteOptions
  WriteOptions()
      : sync(true),
        fill_cache(true) {
  }
};
```
####实例，数据的增删改查
```
using namespace shannon;//命名空间
Status s;//收集返回的状态
std::string dbname = "testdb";// db的名字
std::string driverpath = "/dev/kvdev0";// 加载驱动的位置，一般默认为"/dev/kvdev0"设备
std::string name = "new_cf";// 新ColumnFamily的名字
DB *db;  // 创建储存db指针
Options options; // 构造默认的Options
options.create_if_missing = false;// 关闭创建数据库
ColumnFamilyHandle *dle; // 创建储存的handles
s = DB::Open(options, dbname, driverpath, columnfamilies, db);//打开只有默认columnfamilies的数据库
s = db->CreateColumnFamily(options, name, &dle);//创建一个columnfamilies在数据库之中
string key = "key";// 设置key
string value("value");// 设置value
s = db->Put(shannon::WriteOptions(), handel, key, value);// 储存key-value
value = "";// 重置value
s = db->Get(ReadOptions(), handel, key, &value);// 获取value
s = db->Delete(WriteOptions(), handel, slice_key);// 删除key
// 以上三个是从指定的handle之中操作数据

s = db->Put(shannon::WriteOptions(), key, value);// 储存key-value
value = "";
s = db->Get(ReadOptions(), key, &value);获取value
s = db->Delete(WriteOptions(), slice_key);// 删除key
// 以上三个是从默认的ColumnFamily之中操作数据
```
###3、WriteBatch 的使用
####什么是WriteBatch
WriteBatch就是大量的操作在一起作为一个整体，具有原子性的特点，它们是一同处理的命令，一同成功一同失败，如果一批操作之中有一个失败了，那么这一批操作都会失败，要不然全部成功要不然全部失败。而WriteBatch是先把需要做的一批操作放到一个WriteBatch之中储存，然后调用Write接口，同时处理这一批操作。目前每个WriteBatch最多只支持1000个命令。
####主要api
```cpp
class WriteBatch{
   void Put(ColumnFamilyHandle* column_family, const Slice& key,
          const Slice& value);
//  增加一个对column_family写"key->value" 的操作加入WriteBatch之中

  void Put(const Slice& key, const Slice& value);
//  增加一个对default cf写"key->value" 的操作加入WriteBatch之中

  void Delete(ColumnFamilyHandle* column_family, const Slice& key);
//  增加一个对column_family删除"key->value" 的操作加入WriteBatch之中

  void Delete(const Slice& key);
//  增加一个对default cf删除"key->value" 的操作加入WriteBatch之中

// Clear all updates buffered in this batch.
  void Clear();
//清除当前所有已经插入的所有操作
}
class KVImpl :class DB{
  virtual Status Write(const WriteOptions& options, WriteBatch* updates);
  //从数据库的default column_families 执行一个批量的WriteBatch操作，执行updates之中的所有操作，并且保持整个WriteBatch操作的原子性，一起成功或一起失败
}
```
####实例，WriteBatch 的使用
```
    WriteBatch batch;
    for (int i = 0; i < 20; ++i) {
        stringstream fs_value;
        stringstream fs_key;
        fs_key << "batch_key" << i;//构造 key
        fs_value << "batch_val" << i;//构造 value
        string key = fs_key.str();
        batch.Put(handle ,fs_key.str(),fs_value.str());// 存储插入数据的指令
        //batch.Delete(handle ,fs_key.str());//存储删除数据的指令
        batch.Put(fs_key.str(),fs_value.str());// 存储插入数据的指令 （对default的操作）
        // batch.Delete(fs_key.str());// 存储删除数据的指令 （对default的操作）
    }
        stringstream fs_value;
        stringstream fs_key;
        fs_key << "batch_key" << i;
        string key = fs_key.str();
        batch.Delete(handle ,fs_key.str());// 存储删除数据的指令 （对default的操作）
        db->Write(WriteOptions(),&batch);// 执行WriteBatch
```
###4、Snapshot 的使用
####什么是Snapshot
Snapshot是关于指定数据集合的一个完全可用拷贝，该拷贝包括相应数据在某个时间点（拷贝开始的时间点）的映像。在这里Snapshot是一个时间点，通过这个的信息，可以获取在Snapshot创建时刻的数据，而不会被Snapshot创建时刻之后来的命令影响。在很多时刻都会用到，获取数据的时候需要保证数据的一致性，不会出现一半新数据一半是旧数据的情况。但是由于Snapshot需要占用旧数据不让其释放，会相当的占用内存，这里就需要Snapshot在不使用的时候及时释放，这里Snapshot是一个非持久的，断电就会消失，需要持久化的Snapshot可以使用checkpoint。
####主要api

```
class KVImpl :class DB{
  virtual const Snapshot* GetSnapshot();
//创建数据库的一个快照并且返回快照,数据库的快照会导致大量的旧数据残留，需要及时清理不用的快照

  virtual Status ReleaseSnapshot(const Snapshot* snapshot);
//释放快照
  }
```
####实例，Snapshot 的使用
```
	const Snapshot* snapshot  = db->GetSnapshot();// 获取当前时刻的Snapshot
	assert(status.ok());
    status = db->Put(shannon::WriteOptions(), "hello", "mytest");// 插入数据
    assert(status.ok());

    ReadOptions readoptions; //创建options
    readoptions.snapshot = snapshot;// 指定Snapshot读取数据
    status = db->Get(readoptions, "hello", &value);//获取数据
    assert(status.ok());
    std::cout << value << std::endl;// 验证

	status = db->ReleaseSnapshot(snapshot);// 释放Snapshot
	assert(status.ok());
```
###5、Iterator 的使用
####什么是iterator
Iterator是数据库的迭代器，每个columnfamily可以对应生成一个，用Iterator和c++容器的Iterator一样，可以遍历columnfamily中的数据，按照字符串的顺序遍历，支持遍历首个，最后一个，下一个上一个和跳到某个字符串开头的key处（以某个字符串为前缀)。Iterator会自动生成一个Snapshot，来指定遍历数据的时间点，会占用资源，用完应当及时释放。
####主要api
```
class KVImpl {
  virtual Iterator* NewIterator(const ReadOptions&);
//为数据库default column_families创建一个iterator，用来遍历数据

  virtual Iterator* NewIterator(const ReadOptions& options,
		ColumnFamilyHandle* column_family) override;
//为对应的column_families创建一个iterator，用于遍历数据(会创建相应的内存并且返回，用完需要delete)

  virtual Status NewIterators(const ReadOptions& options,
		const std::vector<ColumnFamilyHandle*>& column_families,
		std::vector<Iterator*>* iterators) override;
//为一组的column_families创建一组iterator，用于遍历数据，每个iterator对应一个column_families(会创建相应的内存在iterators之中，用完需要delete)
}
```

```cpp
class Iterator {
	 virtual bool Valid() const = 0;
	//判断当前的位置是否是一个有效的位置

	 virtual void SeekToFirst() = 0;
	//使Iterator移动到第一个数据，若没有找到则结果为Valid() = false

	 virtual void SeekToLast() = 0;
	//使Iterator移动到最后一个数据，若没有找到则结果为Valid() = false

	 virtual void Seek(const Slice& target) = 0;
	//使Iterator移动到target为前缀的数据，若没有找到则结果为Valid() = false

	 virtual void Next() = 0;
	//使Iterator移动到下一个数据处，若没有找到则结果为Valid() = false

	 virtual void Prev() = 0;
	//使Iterator移动到上一个数据处，若没有找到则结果为Valid() = false

	 virtual Slice key() = 0;
	//获取当前位置的key值，

	 virtual Slice value() = 0;
	//获取当前位置value的值

	 virtual Status status() const = 0;
	//返回当前Iterator的状态

	 virtual void SeekForPrev(const Slice& target) = 0;
	//寻找小于或等于目标键的最后一个键Seek()

	 virtual void SetPrefix(const Slice &prefix) = 0;
	//设置前缀
}
```
####实例，Iterator 的使用
```cpp
	Iterator *iter = db->NewIterator(shannon::ReadOptions());// 创建一个Iterator
	for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {// 循环
		std::cout << iter->key().ToString() << "->"
			<< iter->value().ToString() << std::endl;//打印结果
	}
	iter->Seek("batch2");//转跳
	value = iter->value().ToString();// 获取结果
	std::cout << value << std::endl;// 打印结果
	delete iter;// 清除内存，解除占用
```
```cpp
vector<Iterator *> iters;//需要接收的指针数组
DB *db；//需要打开过的db
const vector<ColumnFamilyHandle *> handles；//需要输入的指针
s = db->NewIterators(ReadOptions(), handles, &iters);// 创建一组Iterator
int i = 0;
for (auto iter : iters)// 对每一个Iterator进行操作
{
    for (iter->SeekToFirst(); iter->Valid(); iter->Next())// 循环
    {
        Slice slice_key = iter->key();
        Slice slice_value = iter->value();
        std::cout << " : " << slice_key.ToString() << " : "
                  << slice_value.ToString() << std::endl;
        //获取并且打印结果
    }
    ++i;
}
for (auto iter : iters)
{
    delete iter; //清除内存，解除占用
}
```
###6、Slice和Status 的使用
####什么是Slice
kv自定义的数据类型，实现了一些对字符串的操作(可以支持储存'/0')，可以用string，char* 等多种方式初始化
####Slice 常用接口

```cpp
// Create an empty slice.
  Slice() : data_(""), size_(0) { }

  // Create a slice that refers to d[0,n-1].
  Slice(const char* d, size_t n) : data_(d), size_(n) { }

  // Create a slice that refers to the contents of "s"
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

  // Create a slice that refers to s[0,strlen(s)-1]
  Slice(const char* s) : data_(s), size_(strlen(s)) { }

  // Return a pointer to the beginning of the referenced data
  const char* data() const { return data_; }

  // Return the length (in bytes) of the referenced data
  size_t size() const { return size_; }

  // Return true iff the length of the referenced data is zero
  bool empty() const { return size_ == 0; }

  // Drop the first "n" bytes from this slice.
  void remove_prefix(size_t n)

  // Return a string that contains the copy of the referenced data.
  std::string ToString() const { return std::string(data_, size_); }

```
####实例，创建Slice的多种方式以及使用
```
int key_len = strlen(key),value_len = strlen("value");//获取长度
Slice key = Slice("k\0ey", key_len+1);// 含有/0需要传入长度参数
Slice value = Slice("v\0alue", value_len+1);// 含有/0需要传入长度参数
db->Put(options,key,value);

Slice key = Slice("key"); // 字符串初始化
Slice value = Slice("value");// 字符串初始化
db->Put(options,key,value);

char* keyptr = "k\0ey";
cahr* valueptr "v\0alue";
Slice key = Slice(keyptr,key_len+1); // char*初始化
Slice value = Slice(valueptr,key_len+1);// char*初始化
db->Put(options,key,value);

std::cout <<  value.ToString() << endl;// 输出
```
####什么是Status
####Status 相关接口
	用来定义函数的返回状态,包括错误字符串等相关信息
```

  // Returns true iff the status indicates success.
  bool ok() const { return code() == kOk; }

  // Returns true iff the status indicates a NotFound error.
  bool IsNotFound() const { return code() == kNotFound; }

  // Returns true iff the status indicates a Corruption error.
  bool IsCorruption() const { return code() == kCorruption; }

  // Returns true iff the status indicates a NotSupported error.
  bool IsNotSupported() const { return code() == kNotSupported; }

  // Returns true iff the status indicates an InvalidArgument error.
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  // Returns true iff the status indicates an IOError.
  bool IsIOError() const { return code() == kIOError; }

  // Returns true iff the status indicates an MergeInProgress.
  bool IsMergeInProgress() const { return code() == kMergeInProgress; }

  // Returns true iff the status indicates Incomplete
  bool IsIncomplete() const { return code() == kIncomplete; }

  // Returns true iff the status indicates Shutdown In progress
  bool IsShutdownInProgress() const { return code() == kShutdownInProgress; }

  // Returns true iff the status indicates TimedOut
  bool IsTimedOut() const { return code() == kTimedOut; }

  // Returns true iff the status indicates Aborted
  bool IsAborted() const { return code() == kAborted; }

  // Returns true iff the status indicates Aborted
  bool IsLockLimit() const {
    return code() == kAborted && subcode() == kLockLimit;
  }

  // Returns true iff the status indicates that a resource is Busy and
  // temporarily could not be acquired.
  bool IsBusy() const { return code() == kBusy; }

  // Returns true iff the status indicates Deadlock
  bool IsDeadlock() const { return code() == kBusy && subcode() == kDeadlock; }

  // Returns true iff the status indicated that the operation has Expired.
  bool IsExpired() const { return code() == kExpired; }

  // Returns true iff the status indicates a TryAgain error.
  // This usually means that the operation failed, but may succeed if
  // re-attempted.
  bool IsTryAgain() const { return code() == kTryAgain; }

  // Returns true iff the status indicates the proposed compaction is too large
  bool IsCompactionTooLarge() const { return code() == kCompactionTooLarge; }

  // Returns true iff the status indicates a NoSpace error
  // This is caused by an I/O error returning the specific "out of space"
  // error condition. Stricto sensu, an NoSpace error is an I/O error
  // with a specific subcode, enabling users to take the appropriate action
  // if needed
  bool IsNoSpace() const {
    return (code() == kIOError) && (subcode() == kNoSpace);
  }

  // Returns true iff the status indicates a memory limit error.  There may be
  // cases where we limit the memory used in certain operations (eg. the size
  // of a write batch) in order to avoid out of memory exceptions.
  bool IsMemoryLimit() const {
    return (code() == kAborted) && (subcode() == kMemoryLimit);
  }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;
```

####Status的简单使用场景
```
s = this->DropColumnFamily(column_family_handle);
if (!s.ok()) {
	cout << s.ToString();
    return s;
}
if (flag1){
    return Status::NoSpace("flag1");
}
else if (flag2){
     return Status::IsMemoryLimit();
}
else {
    return Status::NoSpace("flag2");
}
```