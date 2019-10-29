#ifndef SHANNONDB_INCLUDE_FILTER_POLICY_H_
#define SHANNONDB_INCLUDE_FILTER_POLICY_H_

#include "sst_table.h"

struct filter_policy;
typedef int (*create_filter_fn)(struct filter_policy *, slice_t *keys, int n, slice_t *dst);
typedef uint8_t (*key_may_match_fn)(struct filter_policy *, slice_t *key, slice_t *filter);
typedef const char * (*name_fn)();

struct filter_policy {
	create_filter_fn create_filter;
	key_may_match_fn key_may_match;
	name_fn name;

	size_t bits_per_key;  // for bloom filter
	size_t k;
};
typedef struct filter_policy filter_policy_t;

// look at bloom.c
extern uint32_t hash(char *data, size_t n, uint32_t seed);
extern void bloom_filter_init(filter_policy_t *policy, size_t bits_per_key);

#endif
