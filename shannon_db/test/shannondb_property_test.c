#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shannon_db.h>

char *phase = "";

static void StartPhase(char *name)
{
	fprintf(stderr, "======== Test %s\n", name);
	phase = name;
}

#define CheckCondition(cond) \
	({ if (!(cond)) { \
		fprintf(stderr, "%s: %d: %s: %s\n", __FILE__, __LINE__, phase, #cond); \
		abort(); \
	} })


#define CheckError(err) \
	({ if ((err) != 0) { \
		fprintf(stderr, "%s: %d: %s: %s\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })

int open_db_cf(char *db_name, char *cf_name, shannon_db_t **db_ptr, shannon_cf_handle_t **cf_ptr, char **err)
{
	shannon_db_t *db;
	shannon_cf_handle_t *cf;
	struct db_options *options = shannon_db_options_create();

	db = shannon_db_open(options, "/dev/kvdev0", db_name, err);
	if (db == NULL)
		return -1;

	if (cf_name != NULL) {
		cf = shannon_db_open_column_family(db, options, cf_name, err);
		if (cf == NULL && options->create_if_missing) {
			*err = NULL;
			cf = shannon_db_create_column_family(db, options, cf_name, err);
			if (cf == NULL)
				return -1;
		}
	}

	*db_ptr = db;
	*cf_ptr = cf;

	return 0;
}

void delete_db(shannon_db_t *db, char **err)
{
	shannon_destroy_db(db, err);
	shannon_db_close(db);
}

int test_shannondb_property_sys()
{
	shannon_db_t fake_db;
	int dev_fd;
	char *dev_name = "/dev/kvdev0";
	char *prop_name = NULL;
	char *prop_val = NULL;
	__u64 out_val;
	int ret = 0;

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		fprintf(stderr, "failed to open %s\n", dev_name);
		return -1;
	}
	memset(&fake_db, 0, sizeof(fake_db));
	fake_db.dev_fd = dev_fd;
	fake_db.db_index = -1;

	StartPhase("test_shannondb_property_sys");

	prop_name = "shannon.name";
	prop_val = shannon_db_property_value_cf(&fake_db, NULL, prop_name);
	if (prop_val == NULL) {
		fprintf(stderr, "failed to get property=%s.\n", prop_name);
		ret = -1;
		goto out;
	}
	printf("%s=%s\n", prop_name, prop_val);
	free(prop_val);

	prop_name = "shannon.version";
	prop_val = shannon_db_property_value_cf(&fake_db, NULL, prop_name);
	if (prop_val == NULL) {
		fprintf(stderr, "failed to get property=%s.\n", prop_name);
		ret = -1;
		goto out;
	}
	printf("%s=%s\n", prop_name, prop_val);
	free(prop_val);

	prop_name = "shannon.capacity";
	ret = shannon_db_property_int(&fake_db, prop_name, &out_val);
	if (ret) {
		fprintf(stderr, "failed to get property=%s.\n", prop_name);
		ret = -1;
		goto out;
	}
	printf("%s=%lld\n", prop_name, out_val);

	prop_name = "shannon.overprovision";
	ret = shannon_db_property_int(&fake_db, prop_name, &out_val);
	if (ret) {
		fprintf(stderr, "failed to get property=%s.\n", prop_name);
		ret = -1;
		goto out;
	}
	printf("%s=%lld\n", prop_name, out_val);

	prop_name = "shannon.nosuchproperty";
	prop_val = shannon_db_property_value_cf(&fake_db, NULL, prop_name);
	CheckCondition(prop_val == NULL);

out:
	return ret;
}

int test_shannondb_property_db()
{
	struct shannon_db *db = NULL;
	struct shannon_cf_handle *cf = NULL;
	char *err = NULL;
	char *db_name = "test_property.db";
	char *cf_name = NULL;
	char *prop_name = NULL;
	char *prop_val = NULL;
	int ret = 0;

	StartPhase("test_shannondb_property_db");

	ret = open_db_cf(db_name, cf_name, &db, &cf, &err);
	if (ret) {
		fprintf(stderr, "open db=%s, cf=%s failed. err=%s\n", db_name, cf_name, err);
		return -1;
	}

	prop_name = "shannon.name";
	prop_val = shannon_db_property_value_cf(db, NULL, prop_name);
	if (prop_val == NULL) {
		fprintf(stderr, "failed to get property=%s of db=%s.\n", prop_name, db_name);
		ret = -1;
		goto out;
	}
	CheckCondition(strcmp(db_name, prop_val) == 0);
	free(prop_val);

	prop_name = "shannon.nosuchproperty";
	prop_val = shannon_db_property_value_cf(db, NULL, prop_name);
	CheckCondition(prop_val == NULL);

out:
	delete_db(db, &err);
	return ret;
}

int test_shannondb_property_cf()
{
	struct shannon_db *db = NULL;
	struct shannon_cf_handle *cf = NULL;
	char *err = NULL;
	char *db_name = "test_property.db";
	char *cf_name = "property";
	char *prop_name = NULL;
	char *prop_val = NULL;
	int ret = 0;

	StartPhase("test_shannondb_property_cf");

	ret = open_db_cf(db_name, cf_name, &db, &cf, &err);
	if (ret) {
		fprintf(stderr, "open db=%s, cf=%s failed. err=%s\n", db_name, cf_name, err);
		return -1;
	}

	prop_name = "shannon.name";
	prop_val = shannon_db_property_value_cf(db, cf, prop_name);
	if (prop_val == NULL) {
		fprintf(stderr, "failed to get property=%s of db=%s, cf=%s.\n", prop_name, db_name, cf_name);
		ret = -1;
		goto out;
	}
	CheckCondition(strcmp(cf_name, prop_val) == 0);
	free(prop_val);

	prop_name = "shannon.nosuchproperty";
	prop_val = shannon_db_property_value_cf(db, NULL, prop_name);
	CheckCondition(prop_val == NULL);

out:
	delete_db(db, &err);
	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;

	ret |= test_shannondb_property_sys();
	ret |= test_shannondb_property_db();
	ret |= test_shannondb_property_cf();

	if (ret) {
		fprintf(stderr, "TEST FAILED.\n");
	} else {
		fprintf(stderr, "TEST PASSED.\n");
	}

	return ret;
}