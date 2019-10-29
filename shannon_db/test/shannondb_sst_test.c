#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <error.h>
#include <shannon_db.h>

void usage()
{
	printf("use default cf_name: ./sst_test /path/to/sstfile\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	char *filename;
	char *cf_name;
	int verify = 1;

	if (argc != 2) {
		usage();
		return -1;
	}
	filename = argv[1];

	if (argc == 2) {
		printf("=== test analyze_sst start\n");
		ret = shannondb_analyze_sst(filename, verify, "/dev/kvdev0", "test_sst.db");
		if (ret != 0) {
			printf("analyze_sst failed\n");
			exit(-1);
		}
	}

	printf("=== PASS!\n");
	return ret;
}
