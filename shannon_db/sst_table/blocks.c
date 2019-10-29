#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <error.h>
#include <snappy-c.h>
#include "sst_table.h"

extern int little_endian;
extern char* const kvdb_err_msg[];

static void kv_node_destroy(kv_node_t* kv)
{
	if (kv) {
		if (kv->key)
			free(kv->key);
		if (kv->value)
			free(kv->value);
		free(kv);
	}
}

int block_handle_decode_from(block_handle_t *dst, slice_t *input)
{
	if (get_varint64(input, &dst->offset) &&
		get_varint64(input, &dst->size) && dst->size > 0) {
		//DEBUG("offset=%llu, size=%llu\n", dst->offset, dst->size);
		return KOK;
	} else {
		DEBUG("Error: bad block handle\n");
		return KCORRUPTION;
	}
}

int foot_decode_from(foot_t *dst, slice_t *input, int verify)
{
	int ret = 0;
	char* magic_ptr = input->data + KNEW_VERSION_ENCODED_LEN_FOOT - 8;
	size_t delta = KNEW_VERSION_ENCODED_LEN_FOOT - KENCODED_LEN_FOOT;
	uint32_t magic_lo = decode_fixed32(magic_ptr);
	uint32_t magic_hi = decode_fixed32(magic_ptr + 4);
	uint64_t magic = ((uint64_t)(magic_hi) << 32) |
							((uint64_t)(magic_lo));
	char *end = magic_ptr + 8;

	if (magic == KBASED_TABLE_MAGIC_NUMBER) {
		dst->legacy_footer_format = 0;
		dst->checksum_type = (verify) ? input->data[0] : KNO_CHECKSUM;
		if (dst->checksum_type > KXXHASH_CHECKSUM) {
			DEBUG("checksum type not support= %d\n", dst->checksum_type);
			return KNOT_SUPPORTED;
		}
		input->data += 1; // shift input->data
		input->size -= 1;
		ret = block_handle_decode_from(&dst->metaindex_handle, input);
		if (ret == KOK)
			ret = block_handle_decode_from(&dst->index_handle, input);
		if (ret == KOK) {
			dst->version = decode_fixed32(magic_ptr - 4);
			uint8_t *tmp = (uint8_t *)&dst->version;
		}
	} else if (magic == KLEGACY_TABLE_MAGIC_NUMBER) {
		dst->legacy_footer_format = 1;
		dst->checksum_type = (verify) ? KCRC32C_CHECKSUM : KNO_CHECKSUM;
		set_slice(input, input->data + delta, KENCODED_LEN_FOOT);
		ret = block_handle_decode_from(&dst->metaindex_handle, input);
		if (ret == KOK)
			ret = block_handle_decode_from(&dst->index_handle, input);
	} else {
		DEBUG("Unknown magic number, magic=0x%lx\n", magic);
		return KNOT_SUPPORTED;
	}

	return ret;
}

int read_foot(slice_t *result, char *filename)
{
	ssize_t read_size = 0;
	size_t file_size = 0;
	uint64_t offset = 0;
	uint64_t size = KNEW_VERSION_ENCODED_LEN_FOOT;
	int ret = KOK;

	set_slice(result, NULL, 0);
	get_filesize(filename, &file_size);
	if (file_size == 0 || file_size <= size) {
		DEBUG("corruption sst file, file len is too small\n");
		ret = KCORRUPTION;
		goto out;
	}
	offset = (uint64_t)(file_size) - size;
	read_size = read_file(filename, offset, size, result);
	if (read_size != size) {
		ret = KIO_ERROR;
	}

out:
	return ret;
}

static int check_block_checksum(slice_t *input, uint8_t checksum_type) {
	char *data = input->data;
	size_t n = input->size - KBLOCK_TRAILER_SIZE;
	uint32_t value, actual;

	switch (checksum_type) {
		case KNO_CHECKSUM:
			break;
		case KCRC32C_CHECKSUM:
			value = crc32c_unmask(decode_fixed32(data + n + 1));
			actual = crc32c_value(data, n + 1);
			break;
		case KXXHASH_CHECKSUM:
			value = decode_fixed32(data + n + 1);
			actual = xxhash32(data, (int)n + 1, 0);
			break;
		default:
			DEBUG("unknown checksum type:%u!\n", (uint32_t)checksum_type);
			return KNOT_SUPPORTED;
	}

	if (checksum_type == KNO_CHECKSUM || actual == value) {
		return KOK;
	} else {
		DEBUG("Error: checksum mismatch!value=%0x, actual=%0x\n", value, actual);
		return KCORRUPTION;
	}
}

static int uncompress_block(slice_t *result, slice_t *input) {
	char *data = input->data, *ubuf = NULL;
	size_t n = input->size - KBLOCK_TRAILER_SIZE, ulength = 0;

	set_slice(result, NULL, 0);
	switch (data[n]) {
		case KNO_COMPRESSION:
			set_slice(result, data, n);
			break;
		case KSNAPPY_COMPRESSION:
			if (snappy_uncompressed_length(data, n, &ulength) != SNAPPY_OK) {
				DEBUG("corrupted uncompressed block contents\n");
				return KCORRUPTION;
			}
			ubuf = (char *)malloc(ulength);
			if (ubuf == NULL) {
				DEBUG("uncompress fail, malloc error\n");
				return KCORRUPTION;
			}
			if (snappy_uncompress(data, n, ubuf, &ulength) != SNAPPY_OK) {
				if (ubuf)
					free(ubuf);
				DEBUG("corrupted uncompressed block contents\n");
				return KCORRUPTION;
			}
			set_slice(result, ubuf, ulength);
			break;
		default:
			DEBUG("unknown block compress type, type=%d\n", (int)data[n]);
			return KNOT_SUPPORTED;
	}
	return KOK;
}

int check_and_uncompress_block(slice_t *result, slice_t *input, uint8_t checksum_type)
{
	int ret = KOK;
	ret = check_block_checksum(input, checksum_type);
	if (ret == KOK)
		ret = uncompress_block(result, input);
	return ret;
}

// block is data block, meta block, meta index block or index block
int read_block(slice_t *result, char *filename, block_handle_t *handle, uint8_t checksum_type)
{
	ssize_t read_size = 0;
	slice_t file_content;
	int ret = KOK;

	set_slice(&file_content, NULL, 0);
	read_size = read_file(filename, handle->offset, handle->size + KBLOCK_TRAILER_SIZE, &file_content);
	if (read_size == 0 || read_size != handle->size + KBLOCK_TRAILER_SIZE) {
		ret = KIO_ERROR;
		goto out;
	}
	ret = check_and_uncompress_block(result, &file_content, checksum_type);
	if (ret != KOK) {
		goto free_file_content;
	}
	if (file_content.data != result->data) {
		free(file_content.data);
	}
	return ret;

free_file_content:
	if (file_content.data)
		free(file_content.data);
out:
	return ret;
}

static int get_data_block_restart_nums(uint32_t *nums, slice_t *data_block)
{
	char *data = data_block->data;
	size_t n = data_block->size - 4;

	if (n < 0) {
		DEBUG("block length is too small, length=%u\n", (uint32_t)n);
		return KCORRUPTION;
	}
	*nums = decode_fixed32(data + n);
	if (*nums < 0) {
		DEBUG("block restart nums is error, nums=%u\n", *nums);
		return KCORRUPTION;
	}

	return KOK;
}

int get_data_block_restart_offset(block_rep_t *rep)
{
	slice_t data_block;
	uint32_t *restarts = &rep->restart_nums;
	uint32_t **restart_offset = &rep->restart_offset;
	uint32_t offset_tmp;
	int ret = KOK, i;
	char *data;
	size_t n;

	set_slice(&data_block, rep->block.data, rep->block.size);
	data = data_block.data;
	n = data_block.size - 4;
	ret = get_data_block_restart_nums(restarts, &data_block);
	if (ret != KOK)
		return ret;
	n -= (*restarts) * 4;
	if (n <= 0) {
		DEBUG("block length is smaller than restarts\n");
		return KCORRUPTION;
	}

	*restart_offset = (uint32_t *)malloc((*restarts) * sizeof(uint32_t));
	if (*restart_offset == NULL) {
		DEBUG("corrupted malloc restart_offset\n");
		return KCORRUPTION;
	}
	memset(*restart_offset, 0, (*restarts) * sizeof(uint32_t));
	for (i = 0; i < (*restarts); ++i) {
		offset_tmp = decode_fixed32(data + n + 4 * i);
		if (offset_tmp < 0 || offset_tmp >= n) {
			DEBUG("block restart offset overstep the boundary\n");
			ret = KCORRUPTION;
			goto free_restart_offset;
		}
		*((*restart_offset) + i) = offset_tmp;
	}
	rep->data_size = n;
	return ret;

free_restart_offset:
	if (*restart_offset)
		free(*restart_offset);
	return ret;
}

static int decode_data_block_one_record(kv_node_t *kv, slice_t *input,
             kv_node_t *last_kv, int key_is_user_key)
{
	uint32_t internal_shared_key_len, internal_non_shared_key_len;
	uint32_t internal_key_len, value_len;
	char *last_key = (last_kv) ? last_kv->key : NULL;
	size_t last_key_len = (last_kv) ? last_kv->key_len : 0;
	char *internal_key = NULL;
	uint64_t sequence;
	int ret = KOK;

	get_varint32(input, &internal_shared_key_len);
	get_varint32(input, &internal_non_shared_key_len);
	get_varint32(input, &value_len);

	// get internal key
	if (internal_shared_key_len > last_key_len) {
		DEBUG("shared_key_len is too big\n");
		return KCORRUPTION;
	}
	internal_key_len = internal_shared_key_len + internal_non_shared_key_len;
	if ((!key_is_user_key && internal_key_len <= 8) ||
		(key_is_user_key && internal_key_len <= 0)) {
		DEBUG("key len is too small, key_len=%u\n", (uint32_t)internal_key_len);
		return KCORRUPTION;
	}

	internal_key = (char *)malloc(internal_key_len);
	if (internal_key == NULL) {
		DEBUG("corrupted malloc internal key\n");
		return KCORRUPTION;
	}
	if (internal_shared_key_len > 0) {
		if (last_key == NULL) {
			DEBUG("here shared_key_len must be zero\n");
			ret = KCORRUPTION;
			goto free_internal_key;
		}
		memcpy(internal_key, last_key, internal_shared_key_len);
	}
	memcpy(internal_key + internal_shared_key_len, input->data, internal_non_shared_key_len);

	// update input
	input->data += internal_non_shared_key_len;
	input->size -= internal_non_shared_key_len;

	// decode internal key
	if (!key_is_user_key) {
		kv->key_len = internal_key_len - 8;
		sequence = decode_fixed64(internal_key + kv->key_len);
		kv->type = (uint32_t)(sequence & 0xffull);
		kv->sequence = sequence >> 8;
	} else
		kv->key_len = internal_key_len;
	kv->key = (char *)malloc(kv->key_len + 1);
	if (kv->key == NULL) {
		DEBUG("corrupted malloc kv->key\n");
		ret = KCORRUPTION;
		goto free_internal_key;
	}
	memcpy(kv->key, internal_key, kv->key_len);
	kv->key[kv->key_len] = '\0';
	if (internal_key) {
		free(internal_key);
		internal_key = NULL;
	}

	// get value
	kv->value_len = value_len;
	if (!key_is_user_key && kv->type == KTYPE_DELETION) {
		kv->value = NULL;
		if (kv->value_len != 0) { //FIXME
			DEBUG("value_len must be zero for delete type\n");
			ret = KCORRUPTION;
			goto free_kv_key;
		}
	} else {
		if (kv->value_len < 0 || kv->value_len > input->size) {
			DEBUG("value_len overstep the boundary for value type\n");
			ret = KCORRUPTION;
			goto free_kv_key;
		}
		kv->value = (char *)malloc(kv->value_len + 1);
		if (kv->value == NULL) {
			DEBUG("corrupted malloc kv->value\n");
			ret = KCORRUPTION;
			goto free_kv_key;
		}
		memcpy(kv->value, input->data, kv->value_len);
		kv->value[kv->value_len] = '\0';
		// update input
		input->data += kv->value_len;
		input->size -= kv->value_len;
	}
	return ret;

free_kv_key:
	if (kv->key) {
		free(kv->key);
		kv->key = NULL;
	}
free_internal_key:
	if (internal_key)
		free(internal_key);
out:
	return ret;
}

static int batch_write_and_clear_nonatomic(database_options_t *opt, char **err)
{
	shannon_db_write_nonatomic(opt->db, opt->woptions, opt->wb, err);
	if (*err)
		return KIO_ERROR;
	shannon_db_writebatch_clear_nonatomic(opt->wb);

	return KOK;
}

extern void __shannon_db_writebatch_put_cf_nonatomic(db_writebatch_t *, shannon_cf_handle_t *, const char *key,
		unsigned int klen, const char *val, unsigned int vlen, __u64 timestamp, char **err);
extern void __shannon_db_writebatch_delete_cf_nonatomic(db_writebatch_t *, shannon_cf_handle_t *,
		const char *key, unsigned int klen, __u64 timestamp, char **err);

int process_one_kv(block_rep_t *rep, kv_node_t *kv)
{
	char *err = NULL;
	database_options_t *opt = NULL;
	int ret = KOK;

	if (rep && rep->opt) {
		opt = rep->opt;
	} else {
		DEBUG("Error data_block_rep options\n");
		return KCORRUPTION;
	}

	// flush all kv in batch
	if (rep->last_data_block && opt->end) {
		if (writebatch_is_valid_nonatomic(opt->wb)) {
			ret = batch_write_and_clear_nonatomic(opt, &err);
			if (err)
				DEBUG("the last write to ssd fail: %s\n", err);
		}
		goto out;
	}

	if (kv->type == KTYPE_VALUE && kv->key && kv->value) {
writebatch_put_nonatomic_try:
		__shannon_db_writebatch_put_cf_nonatomic(opt->wb, opt->cf_handle, kv->key,
				kv->key_len, kv->value, kv->value_len, kv->sequence, &err);
		if (err) {
			if (strcmp(err, kvdb_err_msg[KVERR_BATCH_FULL - KVERR_BASE]) == 0) {
				err = NULL;
				ret = batch_write_and_clear_nonatomic(opt, &err);
				if (err) {
					DEBUG("write to ssd fail: %s\n", err);
					goto out;
				}
				goto writebatch_put_nonatomic_try;
			} else {
				DEBUG("write batch put fail: %s\n", err);
				ret = KCORRUPTION;
				goto out;
			}
		}
		rep->kv_put_count++;
	} else if (kv->type == KTYPE_DELETION && kv->key) {
writebatch_delete_nonatomic_try:
		__shannon_db_writebatch_delete_cf_nonatomic(opt->wb, opt->cf_handle, kv->key,
				kv->key_len, kv->sequence, &err);
		if (err) {
			if (strcmp(err, kvdb_err_msg[KVERR_BATCH_FULL] - KVERR_BASE) == 0) {
				err = NULL;
				ret = batch_write_and_clear_nonatomic(opt, &err);
				if (err) {
					DEBUG("delete from ssd fail: %s\n", err);
					goto out;
				}
				goto writebatch_delete_nonatomic_try;
			} else {
				DEBUG("write batch put fail: %s\n", err);
				ret = KCORRUPTION;
				goto out;
			}
		}
		rep->kv_del_count++;
	} else {
		DEBUG("Error do not support kv->type=%d\n", kv->type);
		ret = KNOT_SUPPORTED;
		goto out;
	}

out:
	return ret;
}

int decode_data_block_restart_interval(block_rep_t *rep, int index)
{
	slice_t interval;
	kv_node_t *kv, *last_kv, *tmp_kv;
	int ret = KOK;

	kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *kv\n");
		return KCORRUPTION;
	}
	memset((void *)kv, 0, sizeof(kv_node_t));

	last_kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (last_kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *last_kv\n");
		ret = KCORRUPTION;
		goto free_kv;
	}
	memset((void *)last_kv, 0, sizeof(kv_node_t));

	interval.data = rep->block.data + rep->restart_offset[index];
	if (index + 1 < rep->restart_nums) {
		interval.size = rep->restart_offset[index+1] - rep->restart_offset[index];
	} else {
		interval.size = rep->data_size - rep->restart_offset[index];
	}

	while (interval.size > 0) {
		ret = decode_data_block_one_record(kv, &interval, last_kv, 0);
		if (ret != KOK)
			goto free_last_kv;
		ret = process_one_kv(rep, kv);
		if (ret != KOK) {
			goto free_last_kv;
		}
		tmp_kv = last_kv;
		last_kv = kv;
		kv = tmp_kv;
		if (kv->key)
			free(kv->key);
		if (kv->value)
			free(kv->value);
		memset(kv, 0, sizeof(kv_node_t));
	}

free_last_kv:
	kv_node_destroy(last_kv);
free_kv:
	kv_node_destroy(kv);
	return ret;
}

int decode_data_block(block_rep_t *rep)
{
	int nums = 0;
	int i, ret = KOK;

	ret = get_data_block_restart_offset(rep);
	if (ret != KOK) {
		return ret;
	}
	nums = rep->restart_nums;
	for (i = 0; i < nums; i++) {
		ret = decode_data_block_restart_interval(rep, i);
		if (ret != KOK) {
			DEBUG("decode restart interval index=%d fail\n", i);
			return ret;
		}
	}
	if (rep->last_data_block) {
		rep->opt->end = 1; // write kvs in writebatch to SSD
		process_one_kv(rep, NULL);
	}
	return ret;
}

int get_index_block_restart_offset(block_rep_t *rep)
{
	return get_data_block_restart_offset(rep);
}

static int decode_index_block_one_record(kv_node_t *kv, slice_t *input) {
	// if key is not user key, we do not use sequence
	// so can force key_is_user_key=1
	return decode_data_block_one_record(kv, input, NULL, 1);
}

int process_one_block_handle(block_rep_t *rep, kv_node_t *kv)
{
	block_handle_t block_handle;
	slice_t value;
	block_rep_t * data_block_rep;
	int ret = KOK;

	set_slice(&value, kv->value, kv->value_len);
	ret = block_handle_decode_from(&block_handle, &value);
	if (ret != KOK) {
		DEBUG("Error decode data block handle\n");
		return ret;
	}

	data_block_rep = (block_rep_t *)malloc(sizeof(block_rep_t));
	if (data_block_rep == NULL) {
		DEBUG("corrupted malloc data_block_rep\n");
		return KCORRUPTION;
	}
	memset(data_block_rep, 0, sizeof(block_rep_t));
	data_block_rep->filename = rep->filename;
	data_block_rep->opt = rep->opt;
	data_block_rep->checksum_type = rep->checksum_type;
	data_block_rep->last_data_block = rep->last_data_block;
	ret = read_block(&data_block_rep->block, data_block_rep->filename,
			&block_handle, data_block_rep->checksum_type);
	if (ret != KOK) {
		goto free_data_block_rep;
	}
	ret = decode_data_block(data_block_rep);
	if (ret == KOK) {
		//DEBUG("data_block=%u: kv_put=%u, kv_del=%u\n", rep->data_block_count, data_block_rep->kv_put_count, data_block_rep->kv_del_count);
		rep->data_block_count++;
		rep->kv_put_count += data_block_rep->kv_put_count;
		rep->kv_del_count += data_block_rep->kv_del_count;
	}

free_block_data:
	if (data_block_rep && data_block_rep->block.data) {
		free(data_block_rep->block.data);
	}
free_block_rep_restart:
	if (data_block_rep && data_block_rep->restart_offset) {
		free(data_block_rep->restart_offset);
	}
free_data_block_rep:
	if (data_block_rep)
		free(data_block_rep);
	return ret;
}

int decode_index_block_restart_interval(block_rep_t *rep, int index)
{
	slice_t interval;
	kv_node_t *kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	int ret = KOK;

	if (kv == NULL) {
		DEBUG("corrupted malloc kv\n");
		return KCORRUPTION;
	}
	memset(kv, 0, sizeof(kv_node_t));
	interval.data = rep->block.data + rep->restart_offset[index];
	if (index + 1 < rep->restart_nums) {
		interval.size = rep->restart_offset[index+1] - rep->restart_offset[index];
	} else {
		interval.size = rep->data_size - rep->restart_offset[index];
	}

	if (interval.size > 0) {
		ret = decode_index_block_one_record(kv, &interval);
		if (ret != KOK)
			goto free_kv;
		ret = process_one_block_handle(rep, kv);
		if (ret != KOK)
			goto free_kv;
	} else {
		DEBUG("Error decode index block\n");
		ret = KCORRUPTION;
	}

free_kv:
	if (kv) {
		if (kv->key)
			free(kv->key);
		if (kv->value)
			free(kv->value);
		free(kv);
	}
	return ret;
}

int decode_index_block(block_rep_t *rep)
{
	int nums = 0;
	int i, ret = KOK;

	ret = get_index_block_restart_offset(rep);
	if (ret != KOK) {
		return ret;
	}
	nums = rep->restart_nums;
	for (i = 0; i < nums; i++) {
		if (i == nums - 1)
			rep->last_data_block = 1;
		ret = decode_index_block_restart_interval(rep, i);
		if (ret != KOK) {
			DEBUG("decode data_block[%d] fail\n", i);
			return ret;
		}
	}
	return ret;
}

static int get_property_block_restart_offset(block_rep_t *rep)
{
	return get_data_block_restart_offset(rep);
}

static int decode_property_block_one_record(kv_node_t *kv, slice_t *input, kv_node_t *last_kv)
{
	return decode_data_block_one_record(kv, input, last_kv, 1);
}

static int process_one_property_kv(block_rep_t *rep, kv_node_t *kv, int *find)
{
	int ret = KOK;

	if (strlen(KCOLUMN_FAMILY_NAME) == kv->key_len &&
		strncmp(KCOLUMN_FAMILY_NAME, kv->key, kv->key_len) == 0) {
		// case: column family name
		if (kv->value_len > CF_NAME_LEN) {
			DEBUG("column family name is too long. cf_len=%d.\n", kv->value_len);
			return KCORRUPTION;
		}
		memcpy(rep->props->cf_name, kv->value, kv->value_len);
		rep->props->cf_name[kv->value_len] = '\0';
		(*find)++;
	} else if (strlen(KINDEX_TYPE) == kv->key_len &&
		strncmp(KINDEX_TYPE, kv->key, kv->key_len) == 0) {
		// case: index type
		rep->props->index_type = ((char *)kv->value)[0];
		(*find)++;
	}

	return ret;
}

int decode_property_block(block_rep_t *rep)
{
	slice_t my_block;
	kv_node_t *kv, *last_kv, *tmp_kv;
	int ret = KOK;
	int find = 0;

	set_slice(&my_block, rep->block.data, rep->block.size);
	kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *kv\n");
		return KCORRUPTION;
	}
	memset((void *)kv, 0, sizeof(kv_node_t));

	last_kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (last_kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *last_kv\n");
		ret = KCORRUPTION;
		goto free_kv;
	}
	memset((void *)last_kv, 0, sizeof(kv_node_t));

	ret = get_property_block_restart_offset(rep);
	if (ret != KOK)
		goto free_last_kv;
	set_slice(&my_block, rep->block.data, rep->data_size);

	while (my_block.size > 0) {
		ret = decode_property_block_one_record(kv, &my_block, last_kv);
		if (ret != KOK)
			goto free_last_kv;
		ret = process_one_property_kv(rep, kv, &find);
		if (ret != KOK)
			goto free_last_kv;
		if (find >= KPROPERTIES)
			break;
		tmp_kv = last_kv;
		last_kv = kv;
		kv = tmp_kv;
		if (kv->key)
			free(kv->key);
		if (kv->value)
			free(kv->value);
		memset(kv, 0, sizeof(kv_node_t));
	}

	if (ret == KOK && find < KPROPERTIES && strlen(rep->props->cf_name) == 0) {
		DEBUG("can not find cf_name in property meta block\n");
		ret = KNOT_FOUND;
	}

free_last_kv:
	kv_node_destroy(last_kv);
free_kv:
	kv_node_destroy(kv);
	return ret;
}

static int get_meta_index_block_restart_offset(block_rep_t *rep)
{
	return get_data_block_restart_offset(rep);
}

static int decode_meta_index_block_one_record(kv_node_t *kv, slice_t *input, kv_node_t *last_kv)
{
	return decode_data_block_one_record(kv, input, last_kv, 1);
}

static int is_property_block(char *key, int len)
{
	if (strlen(KPROPERTIES_BLOCK) == len &&
		strncmp(KPROPERTIES_BLOCK, key, len) == 0)
		return 1;
	if (strlen(KPROPERTIES_BLOCK_OLD_NAME) == len &&
		strncmp(KPROPERTIES_BLOCK_OLD_NAME, key, len) == 0)
		return 1;
	return 0;
}

static int process_one_meta_index_kv(block_rep_t *rep, kv_node_t *kv, int *find)
{
	block_handle_t block_handle;
	slice_t value;
	block_rep_t *meta_block_rep;
	int ret = KOK;

	set_slice(&value, kv->value, kv->value_len);
	// now just need decode property meta block
	if (!is_property_block(kv->key, kv->key_len))
		return ret;
	(*find)++; // find property block
	ret = block_handle_decode_from(&block_handle, &value);
	if (ret != KOK) {
		DEBUG("Error decode meta block handle\n");
		return ret;
	}

	meta_block_rep = (block_rep_t *)malloc(sizeof(block_rep_t));
	if (meta_block_rep == NULL) {
		DEBUG("Error malloc meta_block_rep\n");
		return KCORRUPTION;
	}
	memset(meta_block_rep, 0, sizeof(block_rep_t));
	meta_block_rep->filename = rep->filename;
	meta_block_rep->opt = rep->opt;
	meta_block_rep->checksum_type = rep->checksum_type;
	meta_block_rep->props = rep->props;
	ret = read_block(&meta_block_rep->block, meta_block_rep->filename,
			&block_handle, meta_block_rep->checksum_type);
	if (ret != KOK)
		goto free_meta_block_rep;
	ret = decode_property_block(meta_block_rep);

free_block_data:
	if (meta_block_rep && meta_block_rep->block.data)
		free(meta_block_rep->block.data);
free_block_rep_restart:
	if (meta_block_rep && meta_block_rep->restart_offset)
		free(meta_block_rep->restart_offset);
free_meta_block_rep:
	if (meta_block_rep)
		free(meta_block_rep);
	return ret;
}

int decode_meta_index_block(block_rep_t *rep)
{
	slice_t my_block;
	kv_node_t *kv, *last_kv, *tmp_kv;
	int ret = KOK, find = 0;

	set_slice(&my_block, rep->block.data, rep->block.size);
	kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *kv\n");
		return KCORRUPTION;
	}
	memset((void *)kv, 0, sizeof(kv_node_t));

	last_kv = (kv_node_t *)malloc(sizeof(kv_node_t));
	if (last_kv == NULL) {
		DEBUG("corrupted malloc kv_node_t *last_kv\n");
		ret = KCORRUPTION;
		goto free_kv;
	}
	memset((void *)last_kv, 0, sizeof(kv_node_t));

	ret = get_meta_index_block_restart_offset(rep);
	if (ret != KOK)
		goto free_last_kv;
	set_slice(&my_block, rep->block.data, rep->data_size);

	while (my_block.size > 0) {
		ret = decode_meta_index_block_one_record(kv, &my_block, last_kv);
		if (ret != KOK)
			goto free_last_kv;
		ret = process_one_meta_index_kv(rep, kv, &find);
		if (ret != KOK)
			goto free_last_kv;
		if (find >= KMETA_NUMS)
			break;
		tmp_kv = last_kv;
		last_kv = kv;
		kv = tmp_kv;
		if (kv->key)
			free(kv->key);
		if (kv->value)
			free(kv->value);
		memset(kv, 0, sizeof(kv_node_t));
	}

	if (ret == KOK && find < KMETA_NUMS) {
		DEBUG("can not find cf_name in property meta block\n");
		ret = KNOT_FOUND;
	}

free_last_kv:
	kv_node_destroy(last_kv);
free_kv:
	kv_node_destroy(kv);
	return ret;
}

int process_meta_index(char *filename, meta_block_properties_t *props, foot_t *foot)
{
	block_handle_t block_handle;
	block_rep_t meta_index_block_rep;
	int ret = KOK;

	meta_index_block_rep.filename = filename;
	meta_index_block_rep.props = props;
	meta_index_block_rep.checksum_type = foot->checksum_type;
	// read meta index block and decode property block
	ret = read_block(&meta_index_block_rep.block, filename,
			&foot->metaindex_handle, foot->checksum_type);
	if (ret == KOK)
		ret = decode_meta_index_block(&meta_index_block_rep);

	if (meta_index_block_rep.block.data)
		free(meta_index_block_rep.block.data);
	if (meta_index_block_rep.restart_offset)
		free(meta_index_block_rep.restart_offset);
	return ret;
}

static shannon_cf_handle_t *open_cf(shannon_db_t *db,
		db_options_t* option, char *cf_name, char **err)
{
	shannon_cf_handle_t *cf_handle;

	cf_handle = shannon_db_open_column_family(db, option, cf_name, err);
	if (*err && option->create_if_missing) {
		DEBUG("try to create cf=%s\n", cf_name);
		*err = NULL;
		cf_handle = shannon_db_create_column_family(db, option, cf_name, err);
	}
	return cf_handle;
}

static void db_opt_destroy(database_options_t *db_opt)
{
	shannon_db_writebatch_destroy(db_opt->wb);
	shannon_db_column_family_handle_destroy(db_opt->cf_handle);
	if (db_opt->db)
		shannon_db_close(db_opt->db);
	if (db_opt->woptions)
		shannon_db_writeoptions_destroy(db_opt->woptions);
	if (db_opt->options)
		shannon_db_options_destroy(db_opt->options);
}

static int prepare_db_opt(database_options_t *db_opt,
		char *dev_name, char *db_name, char *cf_name)
{
	int ret = KOK;
	char *err = NULL;

	DEBUG("start open db=%s at dev=%s\n", db_name, dev_name);
	memset((void *)db_opt, 0, sizeof(database_options_t));
	db_opt->dev_name = dev_name;
	db_opt->db_name = db_name;
	db_opt->cf_name = cf_name;
	if ((db_opt->options = shannon_db_options_create()) == NULL) {
		DEBUG("create db options fail\n");
		goto out;
	}

	shannon_db_options_set_create_if_missing(db_opt->options, 1);
	if ((db_opt->woptions = shannon_db_writeoptions_create()) == NULL) {
		DEBUG("create db writeoptions fail\n");
		goto out;
	}

	shannon_db_writeoptions_set_sync(db_opt->woptions, 1);
	db_opt->db = shannon_db_open(db_opt->options, db_opt->dev_name,
					db_opt->db_name, &err);
	if (db_opt->db == NULL) {
		DEBUG("Error: open db=%s at dev=%s, err=%s\n",
			db_name, dev_name, err);
		goto out;
	}

	err = NULL;
	db_opt->cf_handle = open_cf(db_opt->db,  db_opt->options,
				db_opt->cf_name, &err);
	if (db_opt->cf_handle == NULL) {
		DEBUG("open cf=%s fail, err=%s\n", cf_name, err);
		goto out;
	}

	db_opt->wb = shannon_db_writebatch_create_nonatomic();
	if (db_opt->wb == NULL) {
		DEBUG("Error: writebatch create fail\n");
		goto out;
	}
	return ret;

out:
	db_opt_destroy(db_opt);
	return KIO_ERROR;
}

static int is_index_type_support(uint8_t index_type)
{
	switch (index_type) {
	case KBINARY_SEARCH:
		return KOK;
	default:
		DEBUG("index_type=%d is not support.\n", (int)index_type);
		return KNOT_SUPPORTED;
	}
}

// example: dev_name = "/dev/kvdev0"; db_name = "test_sst.db";
int shannondb_analyze_sst(char *filename, int verify, char *dev_name, char *db_name)
{
	int ret = KOK;
	slice_t foot_content, tmp_foot_content;
	foot_t foot;
	block_rep_t index_block_rep;
	database_options_t db_opt;
	meta_block_properties_t props;
	char *cf_def = "default";

	calculate_little_endian();
	if (filename == NULL || dev_name == NULL || db_name == NULL) {
		DEBUG("please provide: filename, dev_name and db_name\n");
		DEBUG("example: filename=/path/to/tmp.sst, dev_name=/dev/kvdev0, db_name=test_sst.db\n");
		return KCORRUPTION;
	}

	memset((void *)&index_block_rep, 0, sizeof(block_rep_t));
	memset((void *)&db_opt, 0, sizeof(database_options_t));
	memset((void *)&props, 0, sizeof(meta_block_properties_t));
	index_block_rep.filename = filename;
	index_block_rep.opt = &db_opt;
	// read foot_content from file and decode to foot
	ret = read_foot(&foot_content, filename);
	if (ret != KOK)
		goto out;
	set_slice(&tmp_foot_content, foot_content.data, foot_content.size);
	ret = foot_decode_from(&foot, &tmp_foot_content, verify);
	if (ret != KOK)
		goto free_foot_content;
	index_block_rep.checksum_type = foot.checksum_type;

	// decode property block, get properties
	if (foot.legacy_footer_format) {
		memcpy(props.cf_name, cf_def, strlen(cf_def));
		props.cf_name[strlen(cf_def)] = '\0';
		fprintf(stderr, "kv will write to cf_name=%s\n", cf_def);
	} else {
		DEBUG("decode meta index block in file=%s\n", filename);
		ret = process_meta_index(filename, &props, &foot);
		if (ret == KOK)
			ret = is_index_type_support(props.index_type);
		if (ret != KOK)
			goto free_foot_content;
		fprintf(stderr, "decode cf_name=%s, kv will write to it.\n", props.cf_name);
	}

	// read index block and decode each data block
	ret = prepare_db_opt(&db_opt, dev_name, db_name, props.cf_name);
	if (ret == KOK) {
		DEBUG("decode index block in file=%s\n", filename);
		ret = read_block(&index_block_rep.block, filename,
				&foot.index_handle, foot.checksum_type);
	}
	if (ret != KOK)
		goto free_foot_content;
	ret = decode_index_block(&index_block_rep);
	DEBUG("total decode data_block_count=%u, kv_put_count=%u, kv_del_count=%u\n", index_block_rep.data_block_count, index_block_rep.kv_put_count, index_block_rep.kv_del_count);

free_rep_restart_offset:
	if (index_block_rep.restart_offset)
		free(index_block_rep.restart_offset);
free_rep_block_data:
	if (index_block_rep.block.data)
		free(index_block_rep.block.data);
free_foot_content:
	if (foot_content.data)
		free(foot_content.data);
out:
	db_opt_destroy(&db_opt);
	return ret;
}
