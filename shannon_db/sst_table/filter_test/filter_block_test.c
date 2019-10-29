#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "../sst_table.h"
#include "../filter_policy.h"
#include "../filter_block.h"

int build_test1(filter_policy_t *policy, char *filename)
{
	int fd = 0;
	int ret = KOK;
	size_t file_size;
	slice_t key;
	filter_block_builder_t builder;
	filter_block_builder_t *build = &builder;
	filter_build_init(build, policy);

	// FIXME should check if error
	ret = filter_build_start_block(build, 100);
	set_slice(&key, "foo", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);
	set_slice(&key, "bar", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);
	set_slice(&key, "box", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	if (ret == KOK)
		ret = filter_build_start_block(build, 200);
	set_slice(&key, "box", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);


	if (ret == KOK)
		ret = filter_build_start_block(build, 300);
	set_slice(&key, "hello", 5);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	if (ret == KOK) {
		ret = filter_build_finish(build);
	}

	// there do not have real data block
	// so do not need O_APPEND
	if (ret == KOK) {
		DEBUG("==== open file \n");
		fd = open(filename, O_RDWR | O_CREAT, 0666);
		if (fd < 0)
			ret = KIO_ERROR;
	}
	if (ret == KOK)
		ret = filter_build_flush(build, fd);
	close(fd);
	DEBUG("===== blk_len=%d\n", (int)build->filter_block_len);
	get_filesize(filename, &file_size);
	DEBUG("===== file_len=%d\n", (int)file_size);

	filter_build_clear(build);
	if (ret != KOK)
		exit(-1);
	return ret;
}

int read_test1(filter_policy_t *policy, char *filename)
{
	int ret = KOK;
	slice_t file_content;
	slice_t tmp_content;
	size_t file_size;
	filter_block_reader_t reader;
	slice_t key;

	// get filter block content
	get_filesize(filename, &file_size);
	read_file(filename, 0, file_size, &file_content);
	tmp_content.data = file_content.data;
	tmp_content.size = file_content.size - 5;

	// init reader and check
	filter_block_reader_init(&reader, policy, &tmp_content);
	set_slice(&key, "foo", 3);
	if (filter_block_reader_key_match(&reader, 100, &key) != 1) {
		DEBUG("100 foo should exit\n");
		exit(-1);
	}
	set_slice(&key, "bar", 3);
	if (filter_block_reader_key_match(&reader, 100, &key) != 1) {
		DEBUG("100 bar should exit\n");
		exit(-1);
	}
	set_slice(&key, "box", 3);
	if (filter_block_reader_key_match(&reader, 100, &key) != 1) {
		DEBUG("100 box should exit\n");
		exit(-1);
	}
	set_slice(&key, "hello", 5);
	if (filter_block_reader_key_match(&reader, 100, &key) != 1) {
		DEBUG("100 hello should exit\n");
		exit(-1);
	}
	set_slice(&key, "missing", 7);
	if (filter_block_reader_key_match(&reader, 100, &key) != 0) {
		DEBUG("100 --missing-- should not exit\n");
		exit(-1);
	}
	set_slice(&key, "other", 5);
	if (filter_block_reader_key_match(&reader, 100, &key) != 0) {
		DEBUG("100 --other-- should not exit\n");
		exit(-1);
	}

free_file_content:
	if (file_content.data)
		free(file_content.data);
	return 0;
}

void test_case1(filter_policy_t *policy, char *filename)
{
	build_test1(policy, filename);
	read_test1(policy, filename);
}

int build_test2(filter_policy_t *policy, char *filename)
{
	int fd = 0;
	int ret = KOK;
	size_t file_size;
	slice_t key;
	filter_block_builder_t builder;
	filter_block_builder_t *build = &builder;
	filter_build_init(build, policy);

	// first filter
	ret = filter_build_start_block(build, 0);
	set_slice(&key, "foo", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	if (ret == KOK)
		ret = filter_build_start_block(build, 2000);
	set_slice(&key, "bar", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	// second filter
	if (ret == KOK)
		ret = filter_build_start_block(build, 3100);
	set_slice(&key, "box", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	// third filter is empty

	// last filter
	if (ret == KOK)
		ret = filter_build_start_block(build, 9000);
	set_slice(&key, "box", 3);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);
	set_slice(&key, "hello", 5);
	if (ret == KOK)
		ret = filter_build_add_key(build, &key);

	if (ret == KOK) {
		ret = filter_build_finish(build);
	}

	// there do not have real data block
	// so do not need O_APPEND
	if (ret == KOK) {
		DEBUG("==== open file \n");
		fd = open(filename, O_RDWR | O_CREAT, 0666);
		if (fd < 0)
			ret = KIO_ERROR;
	}
	if (ret == KOK)
		ret = filter_build_flush(build, fd);
	close(fd);
	DEBUG("===== blk_len=%d\n", (int)build->filter_block_len);
	get_filesize(filename, &file_size);
	DEBUG("===== file_len=%d\n", (int)file_size);

	filter_build_clear(build);
	if (ret != KOK)
		exit(-1);
	return ret;
}

int read_test2(filter_policy_t *policy, char *filename)
{
	int ret = KOK;
	slice_t file_content;
	slice_t tmp_content;
	size_t file_size;
	filter_block_reader_t reader;
	slice_t key[8];
	set_slice(key + 0, "foo", 3);
	set_slice(key + 1, "bar", 3);
	set_slice(key + 2, "box", 3);
	set_slice(key + 3, "hello", 5);

	// get filter block content
	get_filesize(filename, &file_size);
	read_file(filename, 0, file_size, &file_content);
	tmp_content.data = file_content.data;
	tmp_content.size = file_content.size - 5;

	filter_block_reader_init(&reader, policy, &tmp_content);
	// check first filter
	if (filter_block_reader_key_match(&reader, 0, key) != 1) {
		DEBUG("0 %s should exit\n", key[0].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 2000, key + 1) != 1) {
		DEBUG("2000 %s should exit\n", key[1].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 0, key + 2) != 0) {
		DEBUG("0 %s should not exit\n", key[2].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 0, key + 3) != 0) {
		DEBUG("0 %s should not exit\n", key[3].data);
		exit(-1);
	}

	// check second filter
	if (filter_block_reader_key_match(&reader, 3100, key) != 0) {
		DEBUG("3100 %s should not exit\n", key[0].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 3100, key + 1) != 0) {
		DEBUG("3100 %s should not exit\n", key[1].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 3100, key + 2) != 1) {
		DEBUG("3100 %s should exit\n", key[2].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 3100, key + 3) != 0) {
		DEBUG("3100 %s should not exit\n", key[3].data);
		exit(-1);
	}

	// check third filter (empty)
	if (filter_block_reader_key_match(&reader, 4100, key) != 0) {
		DEBUG("4100 %s should not exit\n", key[0].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 4100, key + 1) != 0) {
		DEBUG("4100 %s should not exit\n", key[1].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 4100, key + 2) != 0) {
		DEBUG("4100 %s should not exit\n", key[2].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 4100, key + 3) != 0) {
		DEBUG("4100 %s should not exit\n", key[3].data);
		exit(-1);
	}

	// check last filter
	if (filter_block_reader_key_match(&reader, 9000, key) != 0) {
		DEBUG("9000 %s should not exit\n", key[0].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 9000, key + 1) != 0) {
		DEBUG("9000 %s should not exit\n", key[1].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 9000, key + 2) != 1) {
		DEBUG("9000 %s should exit\n", key[2].data);
		exit(-1);
	}
	if (filter_block_reader_key_match(&reader, 9000, key + 3) != 1) {
		DEBUG("9000 %s should exit\n", key[3].data);
		exit(-1);
	}

free_file_content:
	if (file_content.data)
		free(file_content.data);
	return 0;
}

void test_case2(filter_policy_t *policy, char *filename)
{
	build_test2(policy, filename);
	read_test2(policy, filename);
}

int main()
{
	char *filename = "test_filter.txt";
	filter_policy_t policy;
	bloom_filter_init(&policy, 10);

	printf("==== start test_case1\n");
	test_case1(&policy, filename);
	printf("==== start test_case2\n");
	test_case2(&policy, filename);

	printf("==== PASS!\n");
	return 0;
}
