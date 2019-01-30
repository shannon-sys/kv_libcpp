#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <sstream>
#include <swift/log_iter.h>
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Slice;
using shannon::LogIterator;

int main (int argc,char * argv[])
{
	shannon::Status status;
	std::string device = "/dev/kvdev0";
	shannon::LogIterator *iter = NULL;
	int32_t db, cf;
	uint32_t optype;
	uint64_t timestamp = 1;
	int count = 1;

	status = shannon::GetSequenceNumber(device, &timestamp);
	assert(status.ok());
	printf("==== get dev timestamp=%lu\n", timestamp);

	if (timestamp - 2 > 0)
		timestamp -= 2;
	if (timestamp < 1)
		timestamp = 1;
	printf("==== start log_iter timestamp=%lu\n", timestamp);
	status = NewLogIterator(device, timestamp, &iter);
	assert(status.ok());

	while (1) {
		iter->Next();
		if (!iter->Valid()) {
			if (iter->status().IsCorruption()) {
				printf("log iter is destroyed by kernel!\n");
				break;
			}
			printf("===== move next fail, can not find new cmd!\n");
			//sleep(5);
			//continue;
			break;
		}
		printf("====== count=%d\n", count);
		Slice tmp_key = iter->key();
		assert(iter->status().ok());
		printf("key=%s.\n", tmp_key.data());

		//Slice tmp_value = iter->value();
		//assert(iter->status().ok());
		//printf("value=%s.\n", tmp_value.data());

		optype = iter->optype();
		db = iter->db();
		cf = iter->cf();
		timestamp = iter->timestamp();
		printf("optype=%u, db=%d, cf=%d, timestamp=%lu.\n", optype, db, cf, timestamp);
		//sleep(1);
		count++;
	}

	delete iter;
}
