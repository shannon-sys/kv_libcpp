#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <sstream>

using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Options;
using shannon::Slice;
using shannon::WriteOptions;
using shannon::WriteBatch;
using shannon::WriteBatchNonatomic;
using shannon::ReadOptions;
using shannon::Snapshot;
using shannon::Iterator;
using shannon::ColumnFamilyHandle;
using shannon::ColumnFamilyDescriptor;
using shannon::ColumnFamilyDescriptor;

std::string toString1(int t)
{
	stringstream oss;
	oss<<t;
	return oss.str();
}

void usage()
{
	printf("usage1: ./sst_test /path/to/sstfile\n");
	printf("        write kv to cf_name=default\n");
	printf("usage2: ./sst_test /path/to/sstfile decode_cfname\n");
	printf("        decode cf_name from sstfile and write kv to it\n");
}

int main (int argc,char * argv[])
{
	shannon::DB* db;
	shannon::Options options;
	shannon::DBOptions db_options;
	shannon::Status status;
	std::string dbname = "test_sst.db";
	std::string device = "/dev/kvdev0";
	int verify = 1;
	char *filename;
	int decode_cfname = 0;
	std::vector<ColumnFamilyHandle*> handles;
	std::vector<ColumnFamilyDescriptor> cf_des;
	ColumnFamilyOptions cf_option;

	if (argc != 2 && argc != 3) {
		usage();
		return -1;
	}
	filename = argv[1];
	if (argc == 3) {
		if (strcmp(argv[2], "decode_cfname") == 0)
			decode_cfname = 1;
		else {
			usage();
			return -1;
		}
	}

	options.create_if_missing = true;
	status = shannon::DB::Open(options, dbname, device, &db);
	if (status.ok()) {
		ColumnFamilyHandle *cf1;
		status = db->CreateColumnFamily(cf_option, "sst_cf1", &cf1);
		assert(status.ok());
		delete cf1;
		delete db;
	}
	cf_des.push_back(ColumnFamilyDescriptor("default", cf_option));
	cf_des.push_back(ColumnFamilyDescriptor("sst_cf1", cf_option));
	status = shannon::DB::Open(db_options, dbname, device, cf_des, &handles, &db);
	assert(status.ok());
	printf("db:%p\n",db);

	if (decode_cfname == 0) {
		status = db->IngestExternFile(filename, verify);
	} else {
		status = db->IngestExternFile(filename, verify, &handles);
	}
	assert(status.ok());

	delete db;
}
