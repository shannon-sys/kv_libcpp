#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <sstream>

using namespace std;
using shannon::Status;
using shannon::Options;
using shannon::Slice;
using shannon::WriteOptions;
using shannon::WriteBatch;
using shannon::WriteBatchNonatomic;
using shannon::ReadOptions;
using shannon::Snapshot;
using shannon::Iterator;

std::string toString1(int t)
{
	stringstream oss;
	oss<<t;
	return oss.str();
}
int main (int argc,char * argv[])
{
	shannon::DB* db;
	shannon::Options options;
	std::string dbpath = "/dev/kvdev0";
	std::string value("");
	options.create_if_missing = true;
	shannon::Status status = shannon::DB::Open (options, "testdb", dbpath, &db);
	assert(status.ok());

	shannon::DatabaseList database_list;
	status = shannon::ListDatabase(dbpath, &database_list);
	assert(status.ok());
	for (int i = 0; i < database_list.size(); i ++) {
		std::cout<<"db_name:"<<database_list[i].name<<" db_index:"<<database_list[i].index<<" timestamp:"<<database_list[i].timestamp<<std::endl;
	}

	uint64_t ts1 = 0, ts2 = 0;
	status = shannon::GetSequenceNumber(dbpath, &ts1);
	assert(status.ok());
	std::cout << "timestamp1=" << ts1 << std::endl;
	ts1 += 100;
	status = shannon::SetSequenceNumber(dbpath, ts1);
	assert(status.ok());

	printf("db:%p\n",db);
	status = db->Put(shannon::WriteOptions(), "hello", "world");
	assert(status.ok());
	status = db->Get(shannon::ReadOptions(), "hello", &value);
	assert(status.ok());
	std::cout << value << std::endl;
	assert (status.ok());
	status = db->Put(shannon::WriteOptions(), "hello", "myworld");
	status = db->Get(shannon::ReadOptions(), "hello", &value);
	assert(status.ok());
	std::cout << value << std::endl;

	status = shannon::GetSequenceNumber(dbpath, &ts2);
	assert(status.ok() && (ts2 - ts1 == 2));
	std::cout << "timestamp2=" << ts2 << std::endl;
	status = shannon::SetSequenceNumber(dbpath, ts2);
	assert(!status.ok());

	WriteBatch batch;
	batch.Put("batch1", Slice("mybatchtest"));
	batch.Put("batch2", Slice("mybatchtest2"));
	status  = db->Write(shannon::WriteOptions(), &batch);
	assert(status.ok());
	status = db->Get(shannon::ReadOptions(), "batch2", &value);
	assert(status.ok());
    std::cout << value << std::endl;

	status = db->KeyExist(shannon::ReadOptions(), "batch1");
	assert(status.ok());
	std::cout << "batch1 is exist" << std::endl;
	status = db->KeyExist(shannon::ReadOptions(), "batch1_ha");
	assert(status.IsNotFound());
	std::cout << "batch1_ha is not exist" << std::endl;

	WriteBatchNonatomic batch_non;
	batch_non.Put("batch1_nonatomic", Slice("mybatchtest_nonatomic"));
	batch_non.Put("batch2_nonatomic", Slice("mybatchtest2_nonatomic"));
	status  = db->WriteNonatomic(shannon::WriteOptions(), &batch_non);
	assert(status.ok());
	status = db->Get(shannon::ReadOptions(), "batch2_nonatomic", &value);
	assert(status.ok());
    std::cout << value << std::endl;

	const Snapshot* snapshot  = db->GetSnapshot();
	assert(status.ok());
    status = db->Put(shannon::WriteOptions(), "hello", "mytest");
    assert(status.ok());

    ReadOptions readoptions;
    readoptions.snapshot = snapshot;
    status = db->Get(readoptions, "hello", &value);
    assert(status.ok());
    std::cout << value << std::endl;

	status = db->ReleaseSnapshot(snapshot);
	assert(status.ok());
	Iterator *iter = db->NewIterator(shannon::ReadOptions());
	for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
		std::cout << iter->key().ToString() << "->"
			<< iter->value().ToString() << "->"<< iter->timestamp()<< std::endl;
	}
	iter->Seek("batch2");
	value = iter->value().ToString();
	std::cout << value << std::endl;

	batch.Clear();

    status = db->Put(shannon::WriteOptions(), "key_zero", "");
    assert(status.ok());
    status = db->Get(shannon::ReadOptions(), "key_zero", &value);
    assert(status.ok());
    assert(value.size() == 0);
    
    batch.Clear();
    batch.Put("batch_key", "batch_value");
    batch.Put("batch_key_zero", "");
    status = db->Write(shannon::WriteOptions(), &batch);
    assert(status.ok());
    status = db->Get(shannon::ReadOptions(), "batch_key", &value);
    assert(status.ok());
    assert(value.size() == 11 && memcmp(value.data(), "batch_value", 11) == 0);
    status = db->Get(shannon::ReadOptions(), "batch_key_zero", &value);
    assert(status.ok());
    assert(value.size() == 0);


    //DestroyDB(dbpath, "testdb", options);
    delete iter;
	delete db;
}

