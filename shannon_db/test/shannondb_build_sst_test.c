#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <error.h>
#include <shannon_db.h>

char *phase = "";
#define BUF_LEN 128

static void StartPhase(char *name)
{
	fprintf(stderr, "=== Test %s\n", name);
	phase = name;
}

char key[5000][256];
char value[5000][256];
static int gkeylen;
static char gkey[256];

int gen_key_value(char *key, int *key_len, char *value, int *value_len, int position)
{
	int i = 0;
	int flage = 3;
	int ret = 0;
	for (i = gkeylen - 1; i >= 0; i--) {
		if (i == gkeylen - 1)
			gkey[i] = gkey[i] + 1;
		else
			gkey[i] = gkey[i] + flage;
		if (position == i)
			gkey[i] = gkey[i] + 1;
		if (gkey[i] > 'z') {
			flage = 1;
			gkey[i] = 'a';
		} else {
			flage = 0;
			break;
		}

	}
	strncpy(key, gkey, gkeylen);
	strncpy(value, gkey, gkeylen);
	*key_len = gkeylen;
	*value_len = gkeylen;
	if (flage == 1)
		ret = -1;
	return ret;
}

void print_key_value(char *key, int key_len, char *value, int value_len)
{
	char tkey[256];
	char tvalue[256];

	strncpy(tkey, key, key_len);
	strncpy(tvalue, value, value_len);
	tkey[key_len] = '\0';
	tvalue[value_len] = '\0';
	printf("%s:%s\n", tkey, tvalue);
}

void test_init_key(void)
{
	int i = 0;
	for (i = 0; i < gkeylen; i++)
		gkey[i] = 'a';
	gkey[i - 1] = gkey[i - 1] - 1;
}

void set_interval_key(int key_len)
{
	int keylen = key_len;
	int position = 1;
	gkeylen = keylen;
	test_init_key();
}
int main()
{
	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char *err = NULL;
	char *dbname = "test_sst.db";
	char *devname = "/dev/kvdev0";
	int cfcount;
	char keybuffer[128];
	char valuebuffer[128];
	int keylen = 0;
	int valen = 0;
	struct shannon_cf_handle *cfh1 = NULL;
	struct shannon_cf_handle *cfh2 = NULL;
	char keybuf[100];
	char valuebuf[100];
	char buffer[100];
	int key_len = 0;
	int ret = 0;
	int position = 0;
	set_interval_key(10);
	int value_len = 0;
	int i = 0;
	StartPhase("create_objects \n");
	{
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
	}
	db = shannon_db_open(options, devname, dbname, &err);
	shannon_destroy_db(db, &err);

	db = shannon_db_open(options, devname, dbname, &err);
	cfh1 = shannon_db_create_column_family(db, options, "cf1", &err);
	for (i = 0; i < 100; i++) {
		ret = gen_key_value(key[i], &key_len, value[i], &value_len, 1);
		if (ret < 0) {
			printf("gen_key_value failed.\n");
		}
		shannon_db_put_cf(db, woptions, cfh1, key[i], key_len, value[i], value_len, &err);
	}
	cfh2 = shannon_db_create_column_family(db, options, "cf2", &err);
	for (i = 100; i < 200; i++) {
		ret = gen_key_value(key[i], &key_len, value[i], &value_len, 1);
		if (ret < 0) {
			printf("gen_key_value failed.\n");
		}
		shannon_db_put_cf(db, woptions, cfh2, key[i], key_len, value[i], value_len, &err);
	}
	for (i = 200; i < 300; i++) {
		ret = gen_key_value(key[i], &key_len, value[i], &value_len, 1);
		if (ret < 0) {
			printf("gen_key_value failed.\n");
		}
		shannon_db_put(db, woptions, key[i], key_len, value[i], value_len, &err);
	}
	printf("fill db succ\n");
	struct db_sstoptions *soptions = shannon_db_sstoptions_create();
	shannon_db_build_sst(devname, db, options, roptions, soptions);
/*	struct table_rep  *rep = shannon_db_sstfilewriter_create(dbname, "sstfile", 0, toptions, &err);
	if (rep == NULL) {
		DEBUG("shannon_db_sstfilewriter_create failed.\n");
	}
	for (i = 0; i < 100; i++) {
		ret = gen_key_value(key[i], &key_len, value[i], &value_len, 1);
		if (ret < 0) {
			printf("gen_key_value failed%\n");
		}
		shannon_db_sstfilewriter_add(rep, key[i], key_len, value[i], value_len, 0, 1, &err);
	}
	shannon_db_sstfilewriter_finish(rep, &err);
*/
	printf("cleanup\n");
	{
		shannon_destroy_db(db, &err);
		if (err != NULL) {
			printf("open_column_family failed\n");
			exit(-1);
		}
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}
	fprintf(stderr, "PASS\n");
}
