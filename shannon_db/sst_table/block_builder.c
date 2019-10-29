#include "block_builder.h"

void set_offset(struct block_handle *handle, uint64_t offset)
{
	assert(handle);
	assert(offset >= 0);
	handle->offset = offset;
}
void set_size(struct block_handle *handle, uint64_t size)
{
	assert(handle);
	assert(size >= 0);
	handle->size = size;
}

void set_last_key(char *last_key, char *key, size_t size)
{
	memset(last_key, 0, MAX_KEY_SIZE);
	memcpy(last_key, key, size);
}

struct block_builder *new_data_block(int block_restart_interval, int block_size)
{
	assert(block_restart_interval > 0);
	assert(block_size > 0);
	struct block_builder *builder = malloc(sizeof(struct block_builder));
	if (builder == NULL) {
		DEBUG("malloc data_block failed,\n");
		return NULL;
	}
	builder->buffer = malloc(block_size);
	if (builder->buffer == NULL) {
		DEBUG("malloc data_block->buffer failed,\n");
		return NULL;
	}
	memset(builder->buffer, 0, block_size);
	builder->finished = false;
	builder->offset = 0;
	builder->block_size = block_size;
	builder->num_restarts = 1;
	builder->block_restart_interval =  block_restart_interval;
	builder->last_key = malloc(MAX_KEY_SIZE);
	if (builder->last_key == NULL) {
		DEBUG("malloc data_block->last_key failed,\n");
		return NULL;
	}
	memset(builder->last_key, 0, MAX_KEY_SIZE);
	return builder;
}

void destroy_data_block(struct block_builder *block)
{
	if (block) {
		if (block->buffer)
			free(block->buffer);
		if (block->last_key)
			free(block->last_key);
		free(block);
	}
}

void pput_var32(struct properities_block *builder, uint32_t v)
{
	assert(builder);
	size_t offset = put_varint32(builder->buffer + builder->offset, v);
	builder->offset += offset;
}

void put_var32(struct block_builder *builder, uint32_t v)
{
	assert(builder);
	size_t offset = put_varint32(builder->buffer + builder->offset, v);
	builder->offset += offset;
}

void put_fix32(struct block_builder *builder, uint32_t v)
{
	assert(builder);
	size_t offset = put_fixed32(builder->buffer + builder->offset, v);
	builder->offset += offset;
}

size_t block_current_size(struct block_builder *block)
{
	assert(block);
	size_t size = (block->offset + block->num_restarts * sizeof(uint32_t) + sizeof(uint32_t));
	return size;
}

size_t index_block_current_size(struct index_block *block)
{
	assert(block);
	size_t size = (block->offset + block->num_restarts * sizeof(uint32_t) + sizeof(uint32_t));
	return size;
}

void append_buffer(struct block_builder *builder, char *buffer, size_t size)
{
	assert(builder);
	assert(buffer);
	memcpy(builder->buffer + builder->offset, buffer, size);
	builder->offset += size;
}

char *block_handle_encode_to(struct block_handle *handle)
{
	assert(handle);
	size_t offset = 0;
	char *buf = malloc(handle->encode_max_length);
	if (buf == NULL) {
		DEBUG("malloc handle failed ,length= %d.\n", handle->encode_max_length);
		return NULL;
	}
	offset = put_varint64(buf, handle->offset);
	put_varint64(buf + offset, handle->size);
	return buf;
}

size_t block_handle_length(struct block_handle *handle)
{
	assert(handle);
	size_t offset_length = varint_length(handle->offset);
	size_t size_length = varint_length(handle->size);
	return size_length + offset_length;
}

void properity_block_add_restart(struct properities_block *index_block)
{
	assert(index_block);
	size_t offset = 0;
	size_t shift = 0;
	size_t i = 0;
	size_t index_offset = index_block->offset;
	for (i = 0; i < index_block->restart_id; i++) {
		offset = put_fixed32(index_block->buffer + index_offset, index_block->restarts[i]);
		index_offset += offset;
	}
	offset = put_fixed32(index_block->buffer + index_offset, index_block->restart_id);
	index_offset += offset;
	index_block->offset = index_offset;
}

void index_block_add_restart(struct index_block *index_block)
{
	assert(index_block);
	size_t offset = 0;
	size_t i = 0;
	size_t index_offset = index_block->offset;
	if (index_block->vector_restarts) {
		size_t i;
		for (i = 0; i < index_block->num_restarts; ++i) {
			offset = put_fixed32(index_block->buffer + index_offset, index_block->vector_restarts[i]);
			index_offset += offset;
		}
	}
	offset = put_fixed32(index_block->buffer + index_offset, index_block->num_restarts);
	index_offset += offset;
	index_block->offset = index_offset;
}

void data_block_add_restart(struct block_builder *data_block)
{
	assert(data_block);
	size_t offset = 0;
	size_t i = 0;
	size_t index_offset = data_block->offset;
	for (i = 0; i < data_block->num_restarts; i++) {
		offset = put_fixed32(data_block->buffer + index_offset, data_block->restarts[i]);
		index_offset += offset;
	}
	offset = put_fixed32(data_block->buffer + index_offset, data_block->num_restarts);
	index_offset += offset;
	data_block->offset = index_offset;
}

void index_block_resize(struct index_block *block)
{
	assert(block);
	char *temp = block->buffer;
	int offset = block->offset;
	char *new_buffer = malloc(block->block_size * block->factor);
	if (new_buffer == NULL) {
		DEBUG("malloc new_buffer failed .\n");
	}
	memset(new_buffer, 0, block->block_size * block->factor);
	memcpy(new_buffer, temp, offset);
	if (temp)
		free(temp);
	block->buffer = new_buffer;
	block->block_size = block->block_size * block->factor;
}

int index_block_add(struct index_block *index_block, const slice_t *key, const slice_t *value)
{
	if (index_block == NULL || key == NULL || value == NULL || key->size == 0 || value->size == 0) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	size_t offset = 0;
	const size_t estimated_block_size = index_block_current_size(index_block);
	if (estimated_block_size >=  index_block->block_size) {
		index_block->factor++;
		index_block_resize(index_block);
	}
	offset = put_varint32(index_block->buffer + index_block->offset, 0);
	index_block->offset += offset;
	offset = put_varint32(index_block->buffer + index_block->offset, key->size);
	index_block->offset += offset;
	offset = put_varint32(index_block->buffer + index_block->offset, value->size);
	index_block->offset += offset;
	memcpy(index_block->buffer + index_block->offset, key->data, key->size);
	index_block->offset += key->size;
	memcpy(index_block->buffer + index_block->offset, value->data, value->size);
	index_block->offset += value->size;
	index_block->num_restarts++;
	if (index_block->vector_restarts == NULL) {
		DEBUG("the index_block->vector_restarts is NULL,\n");
		return -KCORRUPTION;
	}
	vector_push_back(index_block->vector_restarts, index_block->offset);
	return KOK;
}

struct index_block *new_index_block(int interval, int block_size)
{
	assert(interval > 0);
	assert(block_size > 0);
	struct index_block *builder = malloc(sizeof(struct index_block));
	if (builder == NULL) {
		DEBUG("malloc index_block failed,\n");
		return NULL;
	}
	builder->buffer = malloc(block_size);
	if (builder->buffer == NULL) {
		DEBUG("malloc index_block->buffer failed,\n");
		return NULL;
	}
	memset(builder->buffer, 0, block_size);
	builder->block_restart_interval = interval;
	builder->num_restarts = 0;
	builder->offset = 0;
	builder->factor = 1;
	builder->block_size = block_size;
	builder->vector_restarts = NULL;
	vector_push_back(builder->vector_restarts, 0);
	return builder;
}
void destroy_index_block(struct index_block *block)
{
	if (block->vector_restarts)
		vector_free(block->vector_restarts);
	if (block->buffer)
		free(block->buffer);
	free(block);
}

int block_add_record(struct block_builder *builder, const slice_t *key, const  slice_t *value)
{
	if (builder == NULL || key == NULL || value == NULL || key->size == 0 || value->size == 0) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	size_t shared = 0;
	if (builder->finished || (builder->counter == 0)) {
		put_var32(builder, 0);
		put_var32(builder, key->size);
		put_var32(builder, value->size);
		append_buffer(builder, key->data, key->size);
		append_buffer(builder, value->data, value->size);
		set_last_key(builder->last_key, key->data, key->size);
		builder->finished = false;
		builder->counter++;
		return KOK;
	}
	if (builder->counter < 16) {
		const size_t min_length = min(strlen(builder->last_key), key->size);
		while ((shared < min_length) && (builder->last_key[shared] == key->data[shared])) {
			shared++;
		}
	} else {
		builder->restart_id++;
		builder->num_restarts++;
		builder->restarts[builder->restart_id] = builder->offset;
		builder->counter = 0;
		put_var32(builder, 0);
		put_var32(builder, key->size);
		put_var32(builder, value->size);
		append_buffer(builder, key->data, key->size);
		append_buffer(builder, value->data, value->size);
		set_last_key(builder->last_key, key->data, key->size);
		return KOK;
	}
	const size_t non_shared = key->size - shared;
	put_var32(builder, shared);
	put_var32(builder, non_shared);
	put_var32(builder, value->size);
	append_buffer(builder, key->data + shared, non_shared);
	append_buffer(builder, value->data, value->size);
	set_last_key(builder->last_key, key->data, key->size);
	builder->counter++;
	return KOK;
}

int writefile_append(char *file, char *data, size_t n)
{
	int ret = 0;
	int fd = open(file, O_RDWR);
	if (fd < 0) {
		DEBUG("open file fail %s\n", file);
		return -1;
	}
	lseek(fd, 0, SEEK_END);
	ret = write_raw(fd, data, n);
	if (ret < 0) {
		DEBUG("write_raw failed. file=%s.\n", file);
		return -1;
	}
	close(fd);
	return ret;
}

char *leveldb_footer_encode_to(struct footer *foot)
{
	assert(foot);
	size_t offset = 0;
	size_t index_offset = 0;
	offset = put_varint64(foot->buffer + index_offset, foot->metaindex_handle->offset);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->metaindex_handle->size);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->index_handle->offset);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->index_handle->size);
	index_offset += offset;
	size_t pad_size = foot->encode_length - 8 - index_offset;
	memset(foot->buffer + index_offset, 'x', pad_size);
	index_offset += pad_size;
	offset = put_fixed32(foot->buffer + index_offset, (uint32_t)(kTableMagicNumber & 0xffffffffu));
	index_offset += offset;
	offset = put_fixed32(foot->buffer + index_offset, (uint32_t)(kTableMagicNumber >> 32));
	index_offset += offset;
	assert(index_offset == 48);
}

void footer_encode_to(struct footer *foot, int type)
{
	assert(foot);
	size_t offset = 0;
	size_t index_offset = 0;
	memcpy(foot->buffer, &type, 1);
	index_offset += 1;
	offset = put_varint64(foot->buffer + index_offset, foot->metaindex_handle->offset);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->metaindex_handle->size);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->index_handle->offset);
	index_offset += offset;
	offset = put_varint64(foot->buffer + index_offset, foot->index_handle->size);
	index_offset += offset;
	size_t pad_size = foot->encode_length - 12 - index_offset;
	memset(foot->buffer + index_offset, 'x', pad_size);
	index_offset += pad_size;
	offset = put_fixed32(foot->buffer + index_offset, (uint32_t)1);
	index_offset += offset;
	offset = put_fixed32(foot->buffer + index_offset, (uint32_t)(rTableMagicNumber & 0xffffffffu));
	index_offset += offset;
	offset = put_fixed32(foot->buffer + index_offset, (uint32_t)(rTableMagicNumber >> 32));
	index_offset += offset;
	assert(index_offset == 53);
}
struct block_handle *new_block_handle()
{
	struct block_handle *handle = malloc(sizeof (struct block_handle));
	if (handle == NULL) {
		DEBUG("malloc handle failed,\n");
		return NULL;
	}
	set_offset(handle, 0);
	set_size(handle, 0);
	handle->encode_max_length = kMaxEncodedLength;
	return handle;
}

void destroy_handle(struct block_handle *handle)
{
	if (handle)
		free(handle);
}

void destroy_filter_block(struct filter_block *block)
{

}

struct footer *new_leveldb_footer()
{
	struct footer *foot = malloc(sizeof(struct footer));
	if (foot == NULL) {
		DEBUG("malloc foot failed,\n");
		return NULL;
	}
	foot->metaindex_handle = new_block_handle();
	if (foot->metaindex_handle == NULL) {
		DEBUG("malloc metaindex_handle failed,\n");
		return NULL;
	}
	foot->index_handle = new_block_handle();
	if (foot->index_handle == NULL) {
		DEBUG("malloc index_handle failed,\n");
		return NULL;
	}
	foot->encode_length = LEVELDB_FOOT_SIZE;
	foot->buffer = malloc(foot->encode_length);
	if (foot->buffer == NULL) {
		DEBUG("malloc foot buffer failed,\n");
		return NULL;
	}
	foot->offset = 0;
	return foot;
}

struct properities_block *new_properities_block(int block_size)
{
	struct properities_block *props = malloc(sizeof(struct properities_block));
	if (props == NULL) {
		DEBUG("malloc props failed,\n");
		return NULL;
	}
	props->buffer = malloc(block_size);
	if (props->buffer == NULL) {
		DEBUG("malloc props buffer failed,\n");
		return NULL;
	}
	props->offset = 0;
	memset(props->restarts, 0, 32 * sizeof(uint32_t));
	props->num_restarts = 1;
	props->restart_id = 0;
	return props;
}

void destroy_prop_block(struct properities_block *block)
{
	if (block->buffer)
		free(block->buffer);
	free(block);
}

struct footer *new_rocksdb_footer()
{
	struct footer *foot = malloc(sizeof(struct footer));
	if (foot == NULL) {
		DEBUG("malloc foot failed,\n");
		return NULL;
	}
	foot->metaindex_handle = new_block_handle();
	if (foot->metaindex_handle == NULL) {
		DEBUG("malloc metaindex_handle failed,\n");
		return NULL;
	}
	foot->index_handle = new_block_handle();
	if (foot->index_handle == NULL) {
		DEBUG("malloc index_handle failed,\n");
		return NULL;
	}
	foot->encode_length = ROCKSDB_FOOT_SIZE;
	foot->buffer = malloc(foot->encode_length);
	if (foot->buffer == NULL) {
		DEBUG("malloc foot buffer failed,\n");
		return NULL;
	}
	foot->offset = 0;
	return foot;
}

void destroy_foot(struct footer *foot)
{
	if (foot->metaindex_handle)
		destroy_handle(foot->metaindex_handle);
	if (foot->index_handle)
		destroy_handle(foot->index_handle);
	if (foot->buffer)
		free(foot->buffer);
	free(foot);
}

void block_reset(struct block_builder *rep)
{
	assert(rep);
	memset(rep->buffer, 0, rep->block_size);
	rep->offset = 0;
	rep->finished = false;
	rep->counter = 0;
	rep->restart_id = 0;
	rep->num_restarts = 1;
}

struct slice *block_builder_finish(struct block_builder *block)
{
	assert(block);
	data_block_add_restart(block);
	return new_slice(block->buffer, block->offset);
}

struct slice *meta_index_block_finish(struct index_block *block)
{
	assert(block);
	size_t offset = put_fixed32(block->buffer + block->offset, 0);
	block->offset += offset;
	offset = put_fixed32(block->buffer + block->offset, 1);
	block->offset += offset;
}

void add_properities(struct properities_block *builder, char *key, int key_size, char *value, int value_size)
{
	if (builder == NULL || key == NULL || value == NULL || key_size == 0 || value_size == 0) {
		DEBUG("the parameter is NULL. %s %d\n", key, value_size);
	}
	pput_var32(builder, 0);
	pput_var32(builder, key_size);
	pput_var32(builder, value_size);
	memcpy(builder->buffer + builder->offset, key, key_size);
	builder->offset += key_size;
	memcpy(builder->buffer + builder->offset, value, value_size);
	builder->offset += value_size;
}

void add_properities_number(struct properities_block *builder, char *key, int key_size, uint64_t number)
{
	if (builder == NULL || key == NULL || key_size == 0) {
		DEBUG("the parameter is NULL,\n");
	}
	pput_var32(builder, 0);
	pput_var32(builder, key_size);
	int value_size = varint_length(number);
	pput_var32(builder, value_size);
	memcpy(builder->buffer + builder->offset, key, key_size);
	builder->offset += key_size;
	size_t offset = put_varint64(builder->buffer + builder->offset, number);
	builder->offset += offset;
	builder->restart_id++;
	builder->num_restarts++;
	builder->restarts[builder->restart_id] = builder->offset;
}
void add_properities_fix32_number(struct properities_block *builder, char *key, int key_size, uint32_t number)
{
	if (builder == NULL || key == NULL || key_size == 0) {
		DEBUG("the parameter is NULL,\n");
	}
	pput_var32(builder, 0);
	pput_var32(builder, key_size);
	int value_size = varint_length(number);
	pput_var32(builder, 4);
	memcpy(builder->buffer + builder->offset, key, key_size);
	builder->offset += key_size;
	size_t offset = put_fixed32(builder->buffer + builder->offset, number);
	builder->offset += offset;
	builder->restart_id++;
	builder->num_restarts++;
	builder->restarts[builder->restart_id] = builder->offset;
}
void add_properities_fix64_number(struct properities_block *builder, char *key, int key_size, uint64_t number)
{
	if (builder == NULL || key == NULL || key_size == 0) {
		DEBUG("the parameter is NULL,\n");
	}
	pput_var32(builder, 0);
	pput_var32(builder, key_size);
	int value_size = varint_length(number);
	pput_var32(builder, 8);
	memcpy(builder->buffer + builder->offset, key, key_size);
	builder->offset += key_size;
	size_t offset = put_fixed64(builder->buffer + builder->offset, number);
	builder->offset += offset;
	builder->restart_id++;
	builder->num_restarts++;
	builder->restarts[builder->restart_id] = builder->offset;
}

void properities_block_finish(struct properities_block *block)
{
/*	add_properities_number(block, kIndexPartitions, strlen(kIndexPartitions), block->index_partitions);
	add_properities_number(block, kTopLevelIndexSize, strlen(kTopLevelIndexSize), block->top_level_index_size);
	add_properities_number(block, kIndexKeyIsUserKey, strlen(kIndexKeyIsUserKey), block->index_key_is_user_key);
	add_properities_number(block, kIndexValueIsDeltaEncoded, strlen(kIndexValueIsDeltaEncoded), block->index_value_is_delta_encoded);
	add_properities_number(block, kDeletedKeys, strlen(kDeletedKeys), block->num_deletions);
	add_properities_number(block, kMergeOperands, strlen(kMergeOperands), block->num_merge_operands);
	add_properities_number(block, kNumRangeDeletions, strlen(kNumRangeDeletions), block->num_range_deletions);
	add_properities_number(block, kFixedKeyLen, strlen(kFixedKeyLen), block->fixed_key_len);
	add_properities(block, kFilterPolicy, strlen(kFilterPolicy), block->filter_policy_name, strlen(block->filter_policy_name));
	add_properities(block, kComparator, strlen(kComparator), block->comparator_name, strlen(block->comparator_name));
	add_properities(block, kMergeOperator, strlen(kMergeOperator), block->merge_operator_name, strlen(block->merge_operator_name));
	add_properities(block, kPrefixExtractorName, strlen(kPrefixExtractorName), block->prefix_extractor_name, strlen(block->prefix_extractor_name));
	add_properities(block, kFilterPolicy, strlen(kFilterPolicy), block->filter_policy_name, strlen(block->filter_policy_name));
	add_properities(block, kPropertyCollectors, strlen(kPropertyCollectors), block->user_collected_properties, block->c_offset);
	add_properities_number(block, kFilterSize, strlen(kFilterSize), block->filter_size);
*/
	add_properities_number(block, kColumnFamilyId, strlen(kColumnFamilyId), block->column_family_id);
	add_properities(block, kColumnFamilyName, strlen(kColumnFamilyName), block->column_family_name, strlen(block->column_family_name));
	add_properities(block, kCompression, strlen(kCompression), block->compression_name, strlen(block->compression_name));
	add_properities_number(block, kCreationTime, strlen(kCreationTime), block->creation_time);
	add_properities_number(block, kDataSize, strlen(kDataSize), block->data_size);
	add_properities_fix64_number(block, kGlobalSeqno, strlen(kGlobalSeqno), 0);
	add_properities_fix32_number(block, kVersion, strlen(kVersion), 2);
	add_properities_number(block, kFormatVersion, strlen(kFormatVersion), block->format_version);
	add_properities_number(block, kIndexSize, strlen(kIndexSize), block->index_size);
	add_properities_number(block, kNumDataBlocks, strlen(kNumDataBlocks), block->num_data_blocks);
	add_properities_number(block, kNumEntries, strlen(kNumEntries), block->num_entries);
	add_properities_number(block, kOldestKeyTime, strlen(kOldestKeyTime), block->oldest_key_time);
	add_properities_number(block, kRawKeySize, strlen(kRawKeySize), block->raw_key_size);
	add_properities_number(block, kRawValueSize, strlen(kRawValueSize), block->raw_value_size);
	properity_block_add_restart(block);
}
