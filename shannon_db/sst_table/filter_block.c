#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "filter_block.h"

#define KFILTER_BASE_LG 11
#define KFILTER_BASE    (1 << KFILTER_BASE_LG)

static int resize_filter_offset(filter_block_builder_t *build)
{
	char **new;
	char **old = build->filter_offset;
	uint32_t quantums = build->filter_offset_quantums;
	int i;

	quantums += FILTER_OFFSET_QUANTUMS;
	new = (char **)malloc(quantums * sizeof(char *));
	if (new == NULL) {
		DEBUG("resize filter offset fail\n");
		return KCORRUPTION;
	}
	memset(new, 0, quantums * sizeof(char *));
	for (i = 0; i < quantums - FILTER_OFFSET_QUANTUMS; ++i)
		new[i] = old[i];
	build->filter_offset = new;
	build->filter_offset_quantums = quantums;
	if (old)
		free(old);

	return KOK;
}

static int filter_offset_append(filter_block_builder_t *build, uint32_t input)
{
	uint32_t quantum = build->filter_offset_nums / FILTER_OFFSET_QUANTUM;
	uint32_t index = build->filter_offset_nums % FILTER_OFFSET_QUANTUM;
	char *q;
	int ret = KOK;

	if (quantum >= build->filter_offset_quantums)
		ret = resize_filter_offset(build);
	if (ret != KOK)
		return ret;
	q = (build->filter_offset)[quantum];
	if (q == NULL) {
		q = (char *)malloc(FILTER_OFFSET_QUANTUM * sizeof(uint32_t));
		if (q == NULL) {
			DEBUG("malloc filter offset quantum fail\n");
			return KCORRUPTION;
		}
		(build->filter_offset)[quantum] = q;
	}
	encode_fixed32((q + index * sizeof(uint32_t)) , input);
	(build->filter_offset_nums)++;

	return ret;
}

void free_filter_offset(filter_block_builder_t *build)
{
	int i;
	char **filter_offset = build->filter_offset;

	if (filter_offset) {
		for (i = 0; i < build->filter_offset_quantums; ++i) {
			if (filter_offset[i])
				free(filter_offset[i]);
		}
		free(filter_offset);
	}
}

static int resize_filters(filter_block_builder_t *build)
{
	slice_t **new;
	slice_t **old = build->filters;
	uint32_t quantums = build->filters_quantums;
	int i;

	quantums += FILTERS_QUANTUMS;
	new = (slice_t **)malloc(quantums * sizeof(slice_t *));
	if (new == NULL) {
		DEBUG("resize filters fail\n");
		return KCORRUPTION;
	}
	memset(new, 0, quantums * sizeof(slice_t *));
	for (i = 0; i < quantums - FILTERS_QUANTUMS; ++i)
		new[i] = old[i];
	build->filters = new;
	build->filters_quantums = quantums;
	if (old)
		free(old);

	return KOK;
}

static int filters_append(filter_block_builder_t *build, slice_t *input)
{
	uint32_t quantum = build->filters_nums / FILTERS_QUANTUM;
	uint32_t index = build->filters_nums % FILTERS_QUANTUM;
	slice_t *q;
	int ret = KOK;

	if (quantum >= build->filters_quantums)
		ret = resize_filters(build);
	if (ret != KOK)
		return ret;
	q = (build->filters)[quantum];
	if (q == NULL) {
		q = (slice_t *)malloc(FILTERS_QUANTUM * sizeof(slice_t));
		if (q == NULL) {
			DEBUG("malloc filters quantum fail\n");
			return KCORRUPTION;
		}
		memset(q, 0, FILTERS_QUANTUM * sizeof(slice_t));
		(build->filters)[quantum] = q;
	}
	set_slice(q + index, input->data, input->size);
	(build->filters_nums)++;
	build->total_filters_len += input->size;

	return ret;
}

void free_filters(filter_block_builder_t *build)
{
	int i, j;
	slice_t **filters = build->filters;
	slice_t *q;

	if (build == NULL || filters == NULL)
		return;

	for (i = 0; i < build->filters_quantums; ++i) {
		q = filters[i];
		if (q == NULL)
			break;
		for (j = 0; j < FILTERS_QUANTUM; ++j)
			if (q[j].data)
				free(q[j].data);
		free(q);
	}
	free(filters);
}

int filter_build_init(filter_block_builder_t *build, filter_policy_t *policy)
{
	int ret = KOK;

	if (build == NULL) {
		DEBUG("build is NULL\n");
		return KCORRUPTION;
	}
	memset(build, 0, sizeof(filter_block_builder_t));

	build->policy = policy;
	build->keys.data = (char *)malloc(BLOCK_KEYS_MAX_LEN);
	if (build->keys.data == NULL) {
		DEBUG("malloc build->keys.data fail\n");
		return KCORRUPTION;
	}
	build->starts = (uint32_t *)malloc(BLOCK_STARTS_MAX_NUMS * sizeof(uint32_t));
	if (build->starts == NULL) {
		DEBUG("malloc build->starts fail\n");
		return KCORRUPTION;
	}
	ret = resize_filter_offset(build);
	if (ret == KOK)
		ret = resize_filters(build);

	return ret;
}

void filter_build_clear(filter_block_builder_t *build)
{
	if (build == NULL)
		return;
	if (build->keys.data)
		free(build->keys.data);
	if (build->starts)
		free(build->starts);

	free_filter_offset(build);
	free_filters(build);
}

int filter_build_add_key(filter_block_builder_t *build, slice_t *key)
{
	int ret = KOK;
	if (build->starts_nums >= BLOCK_STARTS_MAX_NUMS ||
		build->keys.size + key->size >= BLOCK_KEYS_MAX_LEN) {
		DEBUG("keys length of a data block is too big\n");
		return KNOT_SUPPORTED;
	}

	// update keys and starts
	build->starts[build->starts_nums] = build->keys.size;
	build->starts_nums++;
	memcpy(build->keys.data + build->keys.size, key->data, key->size);
	build->keys.size += key->size;
	return ret;
}

int filter_build_generate_filter(filter_block_builder_t *build)
{
	int ret = KOK, i;
	slice_t *tmp_keys = NULL;
	slice_t bitmaps;
	slice_t *keys = &build->keys;
	uint32_t num_keys = build->starts_nums;
	uint32_t *starts = build->starts;
	filter_policy_t *policy = build->policy;

	// fast path if there are no keys for this filter
	if (num_keys == 0)
		return filter_offset_append(build, build->total_filters_len);

	// make list of keys from flattened key structure
	tmp_keys = (slice_t *)malloc(num_keys * sizeof(slice_t));
	if (tmp_keys == NULL) {
		DEBUG("malloc tmp_keys for generate filter fail\n");
		return KCORRUPTION;
	}
	tmp_keys[num_keys-1].data = keys->data + starts[num_keys-1];
	tmp_keys[num_keys-1].size = keys->size - starts[num_keys-1];
	for (i = 0; i < num_keys - 1; ++i) {
		tmp_keys[i].data = keys->data + starts[i];
		tmp_keys[i].size = starts[i+1] - starts[i];
	}

	// generate filter for current set of keys
	set_slice(&bitmaps, NULL, 0);
	ret = filter_offset_append(build, build->total_filters_len);
	if (ret == KOK)
		ret = policy->create_filter(policy, tmp_keys, num_keys, &bitmaps);
	if (ret == KOK)
		ret = filters_append(build, &bitmaps);
	// clean keys and restarts
	build->starts_nums = 0;
	build->keys.size = 0;

free_tmp_keys:
	if (tmp_keys)
		free(tmp_keys);
	return ret;
}

int filter_build_start_block(filter_block_builder_t *build, uint64_t block_offset)
{
	int ret = KOK;
	uint64_t filter_index = (block_offset / KFILTER_BASE);
	if (filter_index < build->filter_offset_nums) {
		DEBUG("block_offset is too small\n");
		return KCORRUPTION;
	}
	while (filter_index > build->filter_offset_nums) {
		ret = filter_build_generate_filter(build);
		if (ret != KOK)
			break;
	}
	return ret;
}

static void calculate_bitmaps_crc(filter_block_builder_t *build)
{
	int quantum = build->filters_nums / FILTERS_QUANTUM;
	int index = build->filters_nums % FILTERS_QUANTUM;
	uint32_t crc = 0;
	int i, j, max_j;
	char *data;
	size_t size;
	slice_t *bitmap;

	for (i = 0; i <= quantum; i++) {
		max_j = (i == quantum) ? index : FILTERS_QUANTUM;
		bitmap = (build->filters)[i];
		for (j = 0; j < max_j; ++j) {
			data = (bitmap + j)->data;
			size = (bitmap + j)->size;
			crc = crc32c_extend(crc, data, size); //TODO xxhash
		}
	}
	build->crc_value = crc;
}

static void calculate_filter_offset_crc(filter_block_builder_t *build)
{
	int quantum = build->filter_offset_nums / FILTER_OFFSET_QUANTUM;
	int index = build->filter_offset_nums % FILTER_OFFSET_QUANTUM;
	uint32_t crc = build->crc_value;
	int i;
	char *data;
	size_t size;

	for (i = 0; i <= quantum; i++) {
		data = (build->filter_offset)[i];
		size = (i == quantum) ? index : FILTER_OFFSET_QUANTUM;
		size *= sizeof(uint32_t);
		crc = crc32c_extend(crc, data, size);
	}
	build->crc_value = crc;
}

static void filter_build_calculate_crc(filter_block_builder_t *build)
{
	char trailer[1 + KBLOCK_TRAILER_SIZE];

	calculate_bitmaps_crc(build);       // must first
	calculate_filter_offset_crc(build); // must second
	trailer[0] = (char)KFILTER_BASE_LG;
	trailer[1] = (char)KNO_COMPRESSION;
	build->crc_value = crc32c_extend(build->crc_value, trailer, 2);
	build->crc_value = crc32c_mask(build->crc_value);
}

int filter_build_finish(filter_block_builder_t *build)
{
	int ret = KOK;

	if (build->starts_nums != 0)
		ret = filter_build_generate_filter(build);
	// append total_filters_len to filter_offset
	if (ret == KOK)
		ret = filter_offset_append(build, build->total_filters_len);
	if (ret != KOK)
		return ret;

	// calculate crc
	filter_build_calculate_crc(build);
	return ret;
}

static size_t my_write(int fd, void *buf, size_t count)
{
	size_t tmp_write =0, write_size = 0;

	while (write_size < count) {
		tmp_write = write(fd, buf + write_size, count - write_size);
		if (tmp_write <= 0) {
			DEBUG("write filter block fail, error=%s\n", strerror(errno));
			break;
		}
		write_size += tmp_write;
	}

	return write_size;
}

static size_t write_bitmaps(filter_block_builder_t *build)
{
	int quantum = build->filters_nums / FILTERS_QUANTUM;
	int index = build->filters_nums % FILTERS_QUANTUM;
	int i, j, max_j, fd = build->fd;
	char *data;
	size_t size, total_write_size = 0, tmp_write_size = 0;
	slice_t *bitmap;

	for (i = 0; i <= quantum; i++) {
		max_j = (i == quantum) ? index : FILTERS_QUANTUM;
		bitmap = (build->filters)[i];
		for (j = 0; j < max_j; ++j) {
			data = (bitmap + j)->data;
			size = (bitmap + j)->size;
			tmp_write_size = my_write(fd, data, size);
			total_write_size += tmp_write_size;
			if (tmp_write_size != size) {
				DEBUG("write bitmaps fail, i=%d, j=%d\n", i, j);
				goto out;
			}
		}
	}

out:
	if (total_write_size != build->total_filters_len) {
		DEBUG("write bitmaps fail, expect_total_len=%u, write_total_len=%u\n", (uint32_t)build->total_filters_len, (uint32_t)total_write_size);
	}
	return total_write_size;
}

static size_t write_filter_offset(filter_block_builder_t *build)
{
	int quantum = build->filter_offset_nums / FILTER_OFFSET_QUANTUM;
	int index = build->filter_offset_nums % FILTER_OFFSET_QUANTUM;
	int i, fd = build->fd;
	char *data;
	size_t size, tmp_write_size = 0, total_write_size = 0;

	for (i = 0; i <= quantum; i++) {
		data = (build->filter_offset)[i];
		size = (i == quantum) ? index : FILTER_OFFSET_QUANTUM;
		size *= sizeof(uint32_t);
		tmp_write_size = my_write(fd, data, size);
		total_write_size += tmp_write_size;
		if (tmp_write_size != size) {
			DEBUG("write offset fail\n");
			goto out;
		}
	}
out:
	if (total_write_size != build->filter_offset_nums * sizeof(uint32_t)) {
		DEBUG("write offset fail, expect=%u, write=%u\n",
			(uint32_t)(build->filter_offset_nums * sizeof(uint32_t)),
			(uint32_t)total_write_size);
	}
	return total_write_size;
}

int filter_build_flush(filter_block_builder_t *build, int fd)
{
	int ret = KOK;
	char trailer[1 + KBLOCK_TRAILER_SIZE];
	size_t total_write_size = 0, tmp_write_size = 0;
	size_t expect_size = build->total_filters_len +
				build->filter_offset_nums * sizeof(uint32_t) + 1;

	build->filter_block_len = expect_size;
	expect_size += KBLOCK_TRAILER_SIZE;

	build->fd = fd;
	total_write_size += write_bitmaps(build);
	total_write_size += write_filter_offset(build);
	trailer[0] = (char)KFILTER_BASE_LG;
	trailer[1] = (char)KNO_COMPRESSION;
	encode_fixed32(trailer + 2, build->crc_value);
	// write 1 Byte KFILTER_BASE_LG, type, crc
	total_write_size += my_write(fd, trailer, 1 + KBLOCK_TRAILER_SIZE);
	if (total_write_size != expect_size) {
		DEBUG("write filter block fail, total_write_size=%u, expect_size=%u\n", (uint32_t)total_write_size, (uint32_t)expect_size);
		return KIO_ERROR;
	}

	return ret;
}

/* FIXME filter_block_reader just for test */
void filter_block_reader_init(filter_block_reader_t *reader,
		filter_policy_t *policy, slice_t *content)
{
	size_t n = content->size;
	char *data = content->data;
	uint32_t last_word;

	if (n < 5) {
		DEBUG("content is too small\n");
		return;
	}

	reader->policy = policy;
	reader->base_lg = data[n-1];
	last_word = decode_fixed32(data + n - 5);
	if (last_word > n - 5) {
		DEBUG("last_word is too large\n");
		return;
	}
	reader->data = data;
	reader->offset = data + last_word;
	reader->num = (n - 5 - last_word) / 4;
}

int filter_block_reader_key_match(filter_block_reader_t *reader,
		uint64_t block_offset, slice_t *key)
{
	uint64_t index = block_offset >> reader->base_lg;
	uint64_t start, limit;
	slice_t bloom_filter;
	if (index < reader->num) {
		start = decode_fixed32(reader->offset + index*4);
		limit = decode_fixed32(reader->offset + index*4 + 4);
		if (start <= limit && limit <= (size_t)(reader->offset - reader->data)) {
			set_slice(&bloom_filter, reader->data + start, limit - start);
			return reader->policy->key_may_match(reader->policy, key, &bloom_filter);
		} else if (start == limit) {
			// empty filters do not match any keys
			return 0;
		}
	}
	return 1; // errors are treated as potential matches
}
