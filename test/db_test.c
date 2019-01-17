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
			<< iter->value().ToString() << std::endl;
	}
	iter->Seek("batch2");
	value = iter->value().ToString();
	std::cout << value << std::endl;

	batch.Clear();
    //DestroyDB(dbpath, "testdb", options);
    delete iter;
	delete db;
}

