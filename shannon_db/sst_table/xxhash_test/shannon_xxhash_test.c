/*
 * test xxhash32() at util.c
 * with rocksdb XXH32()
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "../sst_table.h"
#include "xxhash.h"

extern int little_endian;

#define MAX 6
char *ch[MAX] = {
	"aiakdjflaglagakgaglskajgjksa;lgjeoiqgisaflsakglgo",
	"abcde,abcde,abcde,abcde,bdfadfaaf,abcde,daffag",
	"abccc,abcc,abc,ab,a,bb,ccc,dddd,eeeee,ffff",
	"abcd,,,,,,adddd,d,,d,a,d,af,ds,afafefhuhdfafa",
	"ab",
	"abcdefg,hijklmn,opqrst,uvwxyz,1234567890"
};

int main() {
	char *data;
	int i, len;
	unsigned int value_xxhash32;
	unsigned int value_XXH32;

	calculate_little_endian();

	printf("==== start\n");
	for (i = 0; i < MAX; ++i) {
		data = ch[i];
		len = (int)strlen(data);
		value_xxhash32 = xxhash32(data, len, 0);
		value_XXH32 = XXH32(data, len, 0);
		if (value_xxhash32 != value_XXH32) {
			printf("len=%d, data=%s!\n", len, data);
			printf("data[%d]: xxhash32=%u\n", i, value_xxhash32);
			printf("data[%d]:    XXH32=%u\n", i, value_XXH32);
			exit(-1);
		}
	}

	printf("==== PASS\n");
}
