#ifndef SHANNONDB_INCLUDE_FILTER_BLOCK_H_
#define SHANNONDB_INCLUDE_FILTER_BLOCK_H_

#include "sst_table.h"
#include "filter_policy.h"

struct filter_block_builder {
	filter_policy_t *policy;
// FIXME BLOCK_KEYS_MAX_LEN please same as data block size
#define BLOCK_KEYS_MAX_LEN 4096
	slice_t keys;             // flattended key contents
#define BLOCK_STARTS_MAX_NUMS (BLOCK_KEYS_MAX_LEN / 4)
	uint32_t *starts;         // starting index in keys of each key
	uint32_t starts_nums;     // start index nums

	char **filter_offset;
#define FILTER_OFFSET_QUANTUM 1024
#define FILTER_OFFSET_QUANTUMS 512
	uint32_t filter_offset_quantums; // num of (*filter_offset)
	uint32_t filter_offset_nums;     // total offsets
	slice_t **filters;
#define FILTERS_QUANTUM  1024
#define FILTERS_QUANTUMS 512
	uint32_t filters_quantums;       // num of (*filter_offset)
	uint32_t filters_nums;           // total filters
	size_t total_filters_len;        // the sum of bitmaps

	// the len of filter_block len, except KBLOCK_TRAILER_SIZE
	size_t filter_block_len;
	uint32_t crc_value;
	int fd;
};
typedef struct filter_block_builder filter_block_builder_t;

// after define, you must init struct
extern int filter_build_init(filter_block_builder_t *build, filter_policy_t *policy);

// when add a new key to data block
extern int filter_build_add_key(filter_block_builder_t *build, slice_t *key);

// after flush a data block to file, you can call it.
// block_offset: the start of new block in file
// example: you have 2 data blocks; you can call it 3 times
// start_block(start_offset_datablock1) start_offset_datablock1 = 0
// start_block(start_offset_datablock2)
// start_block(start_offset_nextblock)
extern int filter_build_start_block(filter_block_builder_t *build, uint64_t block_offset);

// after the last start_block(), you can call finish
// in this function, will generate crc32c and do some other thing
extern int filter_build_finish(filter_block_builder_t *build);

// after call finish(), you can write filter block to file
// maybe you should open file with O_APPEND flag
// write format: ([bitmaps, offsets, kfilter_base_lg] 0, crc)
// will update build->filter_block_len
extern int filter_build_flush(filter_block_builder_t *build, int fd);

// free space
// error or after flush, you should call it
extern void filter_build_clear(filter_block_builder_t *build);

// FIXME a simple example maybe:
/*
 * filter_policy_t policy;
 * bloom_filter_init(&policy, 10);  // important
 * filter_block_builder_t build;
 * filter_build_init(&build, &policy); // important
 *
 * filter_build_start_block(&build, 0);
 *  // data_block*_key* is type of (slice_t *)
 * filter_build_add_key(&build, data_block1_key1);
 * filter_build_add_key(&build, data_block1_key2);
 * filter_build_add_key(&build, data_block1_key3);
 * filter_build_add_key(&build, data_block1_key4);
 *
 * after flush data block 1 to file, file_len = 100;
 * filter_build_start_block(&build, 100);
 *
 * filter_build_add_key(&build, data_block2_key1);
 * filter_build_add_key(&build, data_block2_key2);
 * filter_build_add_key(&build, data_block2_key3);
 * filter_build_add_key(&build, data_block2_key4);
 * filter_build_add_key(&build, data_block2_key5);
 * filter_build_add_key(&build, data_block2_key6);
 *
 * after flush data block 2 to file, file_len = 260;
 * filter_build_start_block(&build, 260);
 *
 * filter_build_finish(&build);
 * filter_build_flush(&build, fd);
 * filter_build_clear(&build);
 *
 */

// FIXME filter_block_reader just for test, do not use!
struct filter_block_reader {
	filter_policy_t *policy;
	char *data;     // pointer to filter data (at block-start)
	char *offset;   // pointer to beginning of offset array (at block-end)
	size_t num;     // number of entries in offset array
	size_t base_lg; // encodeing parameter KFILTER_BASE_LG
};
typedef struct filter_block_reader filter_block_reader_t;

extern void filter_block_reader_init(filter_block_reader_t *reader,
		filter_policy_t *policy, slice_t *content);
extern int filter_block_reader_key_match(filter_block_reader_t *reader,
		uint64_t block_offset, slice_t *key);

#endif
