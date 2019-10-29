#include "table_builder.h"
static char *phase = "";
#define CheckNoError(err) \
	({if ((err) != 0) { \
		fprintf(stderr, "%s: %d: %s: %s\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })

int write_foot(struct table_rep *rep, struct block_handle *index_handle, struct block_handle *meta_handle)
{
	int ret = 0;
	if (rep == NULL || index_handle == NULL || meta_handle == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	struct footer *foot =  (rep->is_rocksdb) ? new_rocksdb_footer() : new_leveldb_footer();
	if (foot == NULL) {
		DEBUG("new foot failed, is_rocksdb = %d\n", rep->is_rocksdb);
		return -KCORRUPTION;
	}
	set_offset(foot->metaindex_handle, meta_handle->offset);
	set_size(foot->metaindex_handle, meta_handle->size);
	set_offset(foot->index_handle, index_handle->offset);
	set_size(foot->index_handle, index_handle->size);
	footer_encode_to(foot, rep->verify);
	ret = writefile_append(rep->filename, foot->buffer, foot->encode_length);
	if (ret < 0) {
		DEBUG("table writefile_append foot failed\n");
	}
	destroy_foot(foot);
	return ret;
}

void filter_block_addkey(const slice_t *key)
{
	printf("filter add block key %s - %d\n", key->data, (int)key->size);
}

int write_raw_block(struct table_rep *rep, const slice_t *block_contents, struct block_handle *handle, int type, int block_type)
{
	int ret;
	set_offset(handle, rep->cur_offset);
	set_size(handle, block_contents->size);
	ret = writefile_append(rep->filename , block_contents->data, block_contents->size);
	if (ret ==  0) {
		unsigned char trailer[kBlockTrailerSize];
		trailer[0] = type;
		uint32_t crc = crc32c_value(block_contents->data, block_contents->size);
		crc = crc32c_extend(crc, trailer, 1);
		encode_fixed32(trailer + 1, crc32c_mask(crc));
		int ret = writefile_append(rep->filename, trailer, kBlockTrailerSize);
		if (ret < 0) {
			DEBUG("writeblock failed ret = %d\n", ret);
			return -KCORRUPTION;
		}
	} else {
		DEBUG("writeblock failed ret= %d\n", ret);
		return -KCORRUPTION;
	}
	switch (block_type) {
	case DATA_BLOCK:
		rep->cur_offset += (block_contents->size + kBlockTrailerSize);
		block_reset(rep->data_block);
		break;
	case INDEX_BLOCK:
		rep->cur_offset += (block_contents->size + kBlockTrailerSize);
		rep->index_block->offset += (kBlockTrailerSize);
		break;
	case META_BLOCK:
		rep->cur_offset += (block_contents->size + kBlockTrailerSize);
		break;
	default:
		DEBUG("unknow block_type=%d\n", block_type);
		ret =  -KCORRUPTION;
		break;
	}
	return KOK;
}

int write_metaindex_block(struct table_rep *rep, struct index_block *metaindex_block, struct block_handle *handle)
{
	slice_t raw;
	if (rep == NULL || metaindex_block == NULL || handle == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	meta_index_block_finish(metaindex_block);
	set_slice(&raw, metaindex_block->buffer, metaindex_block->offset);
	return write_raw_block(rep, &raw, handle, KNO_COMPRESSION, META_BLOCK);
}

int write_data_block(struct table_rep *rep, struct block_builder *block, struct block_handle *handle)
{
	int ret;
	if (rep == NULL || block == NULL || handle == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	slice_t *raw = block_builder_finish(block);
	slice_t block_contents;
	int type  = rep->compression;
	ret = compress_block(&block_contents, raw, type);
	if (ret != KOK) {
		DEBUG("Compression[%d] not supported\n", type);
		type = KNO_COMPRESSION;
		set_slice(&block_contents, raw->data, raw->size);
	}
	ret = write_raw_block(rep, &block_contents, handle, type, DATA_BLOCK);
	if (ret != KOK) {
		DEBUG("write_raw_block failed.\n");
	}
	delete_slice(raw);
	return ret;
}

int write_index_block(struct table_rep *rep, struct index_block *block, struct block_handle *handle)
{
	slice_t raw;
	if (rep == NULL || block == NULL || handle == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	index_block_add_restart(block);
	set_slice(&raw, block->buffer, block->offset);
	return write_raw_block(rep, &raw, handle, KNO_COMPRESSION, INDEX_BLOCK);
}

void build_properities_block(struct table_rep *rep, struct properities_block *props)
{
	props->column_family_id = rep->column_family_id;
	strcpy(props->compression_name, compression_type_to_string(rep->compression));
	strcpy(props->column_family_name, rep->column_family_name);
	(rep->filter_policy != NULL) ? strcpy(props->filter_policy_name, rep->filter_policy_name) : strcpy(props->filter_policy_name, "");
	props->index_partitions  = 0;
	props->top_level_index_size = 0;
	props->index_value_is_delta_encoded = true;
	props->index_key_is_user_key = true;
	props->creation_time = rep->creation_time;
	props->oldest_key_time = rep->oldest_key_time;
	props->raw_key_size = rep->raw_key_size;
	props->raw_value_size = rep->raw_value_size;
	props->data_size = rep->data_block->offset + kBlockTrailerSize;
	props->index_size = rep->index_block->offset + kBlockTrailerSize;
	props->num_entries = rep->num_entries;
	props->num_deletions = 0;
	props->num_merge_operands = 0;
	props->num_range_deletions = 0;
	props->num_data_blocks = rep->data_block_count;
	props->filter_size = 0;
	props->format_version = 0;
	props->fixed_key_len = 0;
}

int write_meta_compression_block(struct table_rep *rep, struct properities_block *props, struct block_handle *handle)
{
	int ret;
	slice_t block_contents;
	char *compression_dict_data = "abcdefghigk";
	slice_t *dict = new_slice(compression_dict_data, strlen(compression_dict_data));
	int type  = rep->compression;
	ret = compress_block(&block_contents, dict, type);
	if (ret != KOK) {
		DEBUG("xxCompression[%d] not supported\n", type);
		type = KNO_COMPRESSION;
		set_slice(&block_contents, dict->data, dict->size);
	}
	return write_raw_block(rep, dict, handle, type, 2);
}

int write_meta_properties_block(struct table_rep *rep, struct properities_block *props, struct block_handle *handle)
{
	slice_t raw;
	if (rep == NULL || props == NULL || handle == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	build_properities_block(rep, props);
	properities_block_finish(props);
	set_slice(&raw, props->buffer, props->offset);
	return write_raw_block(rep, &raw, handle, KNO_COMPRESSION, 2);
}

struct table_rep *new_table_builder(char *fname, char *cfname, struct db_sstoptions *options, int cf_id)
{
	if (fname == NULL || cfname == NULL || options == NULL) {
		DEBUG("the parameter is NULL.\n");
		goto out;
	}
	struct table_rep *rep = malloc(sizeof(struct table_rep));
	if (rep == NULL) {
		DEBUG("malloc table_rep failed.\n");
		goto out;
	}
	rep->block_restart_index_interval = options->block_restart_index_interval;
	rep->block_restart_data_interval = options->block_restart_data_interval;
	rep->is_rocksdb = options->is_rocksdb;
	rep->compression = options->compression;
	rep->block_size = options->block_size;
	rep->offset = 0;
	rep->raw_key_size = 0;
	rep->raw_value_size = 0;
	rep->cur_offset = 0;
	rep->creation_time = 0;
	rep->oldest_key_time = 0;
	rep->verify = 1;
	rep->num_entries = 0;
	rep->closed = false;
	rep->pending_index_entry  = false;
	rep->column_family_id = cf_id;
	strcpy(rep->filename, fname);
	strcpy(rep->column_family_name, cfname);
	rep->last_key = malloc(128);
	if (rep->last_key == NULL) {
		DEBUG("malloc rep->last_key failed.\n");
		goto free_rep;
	}
	rep->data_block = new_data_block(rep->block_restart_data_interval, rep->block_size);
	if (rep->data_block == NULL) {
		DEBUG("malloc rep->data_block failed.\n");
		goto free_last_key;
	}
	rep->index_block = new_index_block(rep->block_restart_index_interval, rep->block_size);
	if (rep->index_block == NULL) {
		DEBUG("malloc rep->index_block failed.\n");
		goto free_data_block;
	}
	rep->meta_index_block = new_index_block(rep->block_restart_index_interval, rep->block_size);
	if (rep->meta_index_block == NULL) {
		DEBUG("malloc rep->meta_index_block failed.\n");
		goto free_index_block;
	}
	rep->prop_block = new_properities_block(rep->block_size);
	if (rep->prop_block == NULL) {
		DEBUG("malloc rep->last_key failed.\n");
		goto free_meta_index_block;
	}
	rep->pending_handle = new_block_handle();
	if (rep->pending_handle == NULL) {
		DEBUG("malloc rep->pending_handle failed.\n");
		goto free_prop_block;
	}
	rep->filter_policy = NULL;
	rep->filter_block = NULL;
	return rep;

free_prop_block:
	if (rep->prop_block)
		destroy_prop_block(rep->prop_block);
free_meta_index_block:
	if (rep->meta_index_block)
		destroy_index_block(rep->meta_index_block);
free_index_block:
	if (rep->index_block)
		destroy_index_block(rep->index_block);
free_data_block:
	if (rep->data_block)
		destroy_data_block(rep->data_block);
free_last_key:
	if (rep->last_key)
		free(rep->last_key);
free_rep:
	if (rep)
		free(rep);
out:
	return NULL;
}

void destroy_table_builder(struct table_rep *rep)
{
	if (rep == NULL)
		return;
	if (rep->prop_block)
		destroy_prop_block(rep->prop_block);
	if (rep->meta_index_block)
		destroy_index_block(rep->meta_index_block);
	if (rep->index_block)
		destroy_index_block(rep->index_block);
	if (rep->pending_handle)
		destroy_handle(rep->pending_handle);
	if (rep->filter_block)
		destroy_filter_block(rep->filter_block);
	if (rep->filter_policy)
		free(rep->filter_policy);
	if (rep->last_key)
		free(rep->last_key);
	free(rep);
}

void block_finish(struct table_rep *rep)
{
	int ret = write_data_block(rep, rep->data_block, rep->pending_handle);
	if (ret == KOK) {
		rep->pending_index_entry = true;
		rep->data_block->finished = true;
		rep->data_block_count++;
	}
}

struct slice *generate_slice_key(char *start_data, struct slice *limit)
{
	if (start_data == NULL || limit == NULL || limit->data == NULL || limit->size < 8) {
		DEBUG("the parameter is NULL,\n");
		goto out;
	}
	struct slice *slice_key = NULL;
	char *key = NULL;
	int start_size = strlen(start_data);
	struct slice *user_key = extract_userkey(limit);
	if (user_key ==  NULL) {
		DEBUG("extract_userkey failed.\n");
		goto out;
	}
	char *limit_data = user_key->data;
	int limit_size = user_key->size;
	key = find_key_between_start_limit(start_data, start_size, limit_data, limit_size);
	if (key ==  NULL) {
		DEBUG("find_key_between_start_limit failed.\n");
		goto free_user_key;
	}
	char *internal_key = append_key(key, strlen(key), extract_seq(limit), extract_type(limit));
	if (internal_key ==  NULL) {
		DEBUG("append_key failed.\n");
		goto free_key;
	}
	slice_key = new_slice(internal_key, strlen(key) + INTERNAL_KEY_TRAILER_SIZE);
	if (slice_key ==  NULL) {
		DEBUG("new_slice failed.\n");
		goto free_internal_key;
	}
	if (user_key)
		delete_slice(user_key);
	if (key)
		free(key);
	if (internal_key)
		free(internal_key);
	return slice_key;
free_user_key:
	if (user_key)
		delete_slice(user_key);
free_key:
	if (key)
		free(key);
free_internal_key:
	if (internal_key)
		free(internal_key);
out:
	return NULL;
}

int table_builder_add(struct table_rep *rep, struct slice *key, struct slice *value)
{
	int ret;
	struct slice index_value;
	struct slice *index_key = NULL;
	if (rep->num_entries > 0) {
		assert(compare(rep->last_key, strlen(rep->last_key), key->data, key->size - INTERNAL_KEY_TRAILER_SIZE) <  0);
	}
	if (rep->pending_index_entry) {
		index_key = generate_slice_key(rep->last_key, key);
		if (index_key == NULL) {
			DEBUG("generate_slice_key failed.\n");
			return -KCORRUPTION;
		}
		char *handle_encoding = block_handle_encode_to(rep->pending_handle);
		if (handle_encoding == NULL) {
			DEBUG("block_handle_encode_to failed.\n");
			return -KCORRUPTION;
		}
		set_slice(&index_value, handle_encoding, block_handle_length(rep->pending_handle));
		ret = index_block_add(rep->index_block, index_key, &index_value);
		if (ret < 0) {
			DEBUG("index_block_add failed.\n");
			return -KCORRUPTION;
		}
		rep->pending_index_entry = false;
	}
	if (rep->filter_block != NULL) {
		filter_block_addkey(key);
	}
	set_last_key(rep->last_key, key->data, key->size);
	ret = block_add_record(rep->data_block, key, value);
	if (ret < 0) {
		DEBUG("block_add_record  key=%s value=%s failed !!!\n", key->data, value->data);
		return -KCORRUPTION;
	}
	const size_t estimated_block_size = block_current_size(rep->data_block);
	if (estimated_block_size >=  rep->data_block->block_size) {
		rep->pending_index_entry = true;
		block_finish(rep);
		rep->data_block->restart_id++;
		rep->data_block->num_restarts++;
		rep->data_block->restarts[rep->data_block->restart_id] = rep->data_block->offset;
	}
	return KOK;
}

int table_flush(struct table_rep *rep)
{
	struct block_handle index_handle;
	struct block_handle meta_handle;
	struct slice index_value;
	struct slice index_key;
	int ret;
	char *handle_encoding = NULL;
	char *mhandle_encoding = NULL;
	if (rep->closed != true) {
		ret = write_data_block(rep, rep->data_block, rep->pending_handle);
		if (ret < 0) {
			DEBUG("table flush  write data block failed\n");
		}
		char *find_key = find_key_between_start_limit(rep->last_key, strlen(rep->last_key), rep->last_key, strlen(rep->last_key));
		char *look_key = append_key(find_key, strlen(find_key), 0 , rep->oldest_key_time);
		set_slice(&index_key, look_key, strlen(look_key));
		if (index_key.data == NULL) {
			DEBUG("generate_slice_key failed.\n");
			ret =  -KCORRUPTION;
		}
		handle_encoding = block_handle_encode_to(rep->pending_handle);
		if (handle_encoding == NULL) {
			DEBUG("block_handle_encode_to failed.\n");
			ret =  -KCORRUPTION;
		}
		set_slice(&index_value, handle_encoding, block_handle_length(rep->pending_handle));
		ret = index_block_add(rep->index_block, &index_key, &index_value);
		if (ret < 0) {
			DEBUG("index_block_add failed.\n");
		}
		rep->closed = true;
		rep->pending_index_entry = true;
		rep->data_block->finished = true;
		rep->data_block_count++;
		rep->pending_index_entry = false;
	}
	if (rep->is_rocksdb) {
		ret = write_meta_properties_block(rep, rep->prop_block, rep->pending_handle);
		if (ret < 0) {
			DEBUG("table flush  write properities_block failed\n");
		}
		set_slice(&index_key, kPropertiesBlock, strlen(kPropertiesBlock));
		mhandle_encoding =  block_handle_encode_to(rep->pending_handle);
		set_slice(&index_value, mhandle_encoding, block_handle_length(rep->pending_handle));
		ret = index_block_add(rep->meta_index_block, &index_key, &index_value);
		if (ret < 0) {
			DEBUG("index_block_add failed.\n");
		}
		ret = write_metaindex_block(rep, rep->meta_index_block, &meta_handle);
		if (ret < 0) {
			DEBUG("table flush  write indexblock failed\n");
		}
	} else {
		set_offset(&meta_handle, 3);
		set_size(&meta_handle, 3);
	}
	ret = write_index_block(rep, rep->index_block, &index_handle);
	if (ret < 0) {
		DEBUG("table flush  write indexblock failed\n");
	}
	ret = write_foot(rep, &index_handle, &meta_handle);
	if (ret < 0) {
		DEBUG("table flush  write foot failed\n");
	}
	free(handle_encoding);
	free(mhandle_encoding);
	return ret;
}

int build_table(struct sst_file_writer *sst_file , struct db_sstoptions *option, struct shannon_db_iterator *iter, int is_rocksdb)
{
	int meta_index_offset, meta_index_size, index_offset, index_size;
	FILE *fp;
	size_t file_size;
	int ret = 0;
	char keybuffer[MAX_KEY_SIZE];
	char valuebuffer[MAX_VALUE_SIZE];
	int keylen = 0;
	int valuelen = 0;
	__u64 timestamp;
	char *err = NULL;
	struct slice slice_key;
	struct slice slice_value;
	shannon_db_iter_seek_to_first(iter);
	shannon_db_iter_get_error(iter, &err);
	CheckNoError(err);

	char *fname = make_table_filename(sst_file->cur_db_name, sst_file->column_family_names[sst_file->cur_cf_index], sst_file->cur_cf_index, is_rocksdb);
	if (fname == NULL) {
		return -KCORRUPTION;
	}
	if (shannon_db_iter_valid(iter)) {
		fp = fopen(fname, "w");
		if (!fp) {
			printf("error %s %d %s\n", __func__,  __LINE__, fname);
			return -KCORRUPTION;
		}
	}
	struct table_rep *rep = new_table_builder(fname, sst_file->cur_cf_name, option, sst_file->cur_cf_index);
	shannon_db_iter_cur_kv(iter, keybuffer, MAX_KEY_SIZE, valuebuffer, MAX_VALUE_SIZE, &keylen, &valuelen, &timestamp);
	rep->creation_time = timestamp;
	while (shannon_db_iter_valid(iter)) {
		shannon_db_iter_cur_kv(iter, keybuffer, MAX_KEY_SIZE, valuebuffer, MAX_VALUE_SIZE, &keylen, &valuelen, &timestamp);
		keybuffer[keylen] = '\0';
		valuebuffer[valuelen] = '\0';
		set_slice(&slice_key, append_key(keybuffer, keylen, timestamp, KTYPE_VALUE), keylen + INTERNAL_KEY_TRAILER_SIZE);
		set_slice(&slice_value, valuebuffer, valuelen);
		ret = table_builder_add(rep, &slice_key, &slice_value);
		if (ret < 0) {
			DEBUG("table_builder_add key= %s value=%s failed !!!\n", keybuffer, valuebuffer);
			break;
		}
		rep->raw_key_size += keylen;
		rep->raw_value_size += valuelen;
		rep->num_entries++;
		shannon_db_iter_move_next(iter);
	}
	shannon_db_iter_cur_kv(iter, keybuffer, MAX_KEY_SIZE, valuebuffer, MAX_VALUE_SIZE, &keylen, &valuelen, &timestamp);
	rep->oldest_key_time = timestamp;
	table_flush(rep);
	destroy_table_builder(rep);
	return 0;
}

struct sst_file_writer *create_sst_file_writer(char *devname, struct shannon_db *db, const struct db_options *opt, const struct db_readoptions *ropt)
{
	int cfcount = 0;
	int i = 0;
	char *err = NULL;

	if (devname == NULL || db == NULL || opt == NULL || ropt == NULL) {
		DEBUG("the parameter is NULL,\n");
		goto out;
	}
	struct sst_file_writer *sst_writer = malloc(sizeof (struct sst_file_writer));
	if (sst_writer == NULL) {
		DEBUG("malloc sst_writer failed\n");
		goto out;
	}
	char **column_fams = (char **)shannon_db_list_column_families(opt, devname, db->dbname, &cfcount, &err);
	CheckNoError(err);

	shannon_cf_handle_t **handles = (shannon_cf_handle_t **) malloc (sizeof (shannon_cf_handle_t *) * cfcount);
	if (handles == NULL) {
		DEBUG("malloc handles failed\n");
		goto free_sst_writer;
	}

	struct db_options **cf_opts = (struct db_options **) malloc(sizeof(struct db_options *) * cfcount);
	if (cf_opts == NULL) {
		DEBUG("malloc cf_opts failed\n");
		goto free_handles;
	}
	for (i = 0; i < cfcount; i++) {
		cf_opts[i] = shannon_db_options_create();
		if (cf_opts[i] == NULL) {
			DEBUG("create_db_options cf_opts[%d] failed\n", i);
			goto free_cf_opts;
		}
	}
	shannon_db_open_column_families(opt, devname, db->dbname, cfcount, (const char **)column_fams, (const struct db_options **)cf_opts, handles, &err);
	CheckNoError(err);

	struct shannon_db_iterator **iters_handles = (struct shannon_db_iterator **) malloc (sizeof (struct shannon_db_iterator *) * cfcount);
	if (iters_handles == NULL) {
		DEBUG("malloc iter_handles failed\n");
		goto free_cf_opts;
	}
	shannon_db_create_iterators(db, ropt, handles, iters_handles, cfcount, &err);
	CheckNoError(err);

	strcpy(sst_writer->dev_name, devname);
	sst_writer->num_cf =  cfcount;
	sst_writer->num_db = 1;
	sst_writer->column_family_names = malloc(sizeof(char *) * cfcount);
	if (sst_writer->column_family_names == NULL) {
		DEBUG("malloc sst_writer->column_family_names failed\n");
		goto free_iter_handles;
	}
	sst_writer->iters_handles = (struct shannon_db_iterator **) malloc (sizeof (struct shannon_db_iterator *) * cfcount);
	if (sst_writer->iters_handles == NULL) {
		DEBUG("malloc sst_writer->iters_handles failed\n");
		goto free_column_family_names;
	}
	for (i = 0; i < cfcount; i++) {
		sst_writer->iters_handles[i] = iters_handles[i];
		sst_writer->column_family_names[i] = (char *)column_fams[i];
	}
	strcpy(sst_writer->cur_db_name, db->dbname);
	strcpy(sst_writer->cur_cf_name, column_fams[0]);
	sst_writer->cur_db_index = db->db_index;
	sst_writer->cur_cf_iter = NULL;
	sst_writer->cur_cf_index = 0;
	for (i = 0; i < cfcount; i++) {
		shannon_db_options_destroy(cf_opts[i]);
		shannon_db_column_family_handle_destroy(handles[i]);
	}
	free(cf_opts);
	free(handles);
	return sst_writer;

free_column_family_names:
	if (sst_writer->column_family_names)
		free(sst_writer->column_family_names);
free_iter_handles:
	if (iters_handles)
		free(iters_handles);
free_cf_opts:
	for (i = 0; i < cfcount; i++) {
		if (cf_opts[i])
			shannon_db_options_destroy(cf_opts[i]);
	}
	free(cf_opts);
free_handles:
	if (sst_writer)
		free(sst_writer);
free_sst_writer:
	if (sst_writer)
		free(sst_writer);
out:
	return NULL;
}

void destroy_sst_file_writer(struct sst_file_writer *sst_writer)
{
	int i;
	for (i = 0; i < sst_writer->num_cf; i++) {
		if (sst_writer->column_family_names[i])
			free(sst_writer->column_family_names[i]);
		if (sst_writer->iters_handles[i])
			free(sst_writer->iters_handles[i]);
	}
	if (sst_writer->iters_handles)
		free(sst_writer->iters_handles);
	if (sst_writer->column_family_names)
		free(sst_writer->column_family_names);
	if (sst_writer)
		free(sst_writer);
}

int shannon_db_sstfilewriter_add(struct table_rep *rep, char *key, size_t keylen, char *val, size_t vallen, uint64_t time, int type)
{
	if (rep == NULL || key == NULL || val == NULL || keylen == 0 || vallen == 0) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	int ret;
	struct slice slice_key;
	struct slice slice_value;
	set_slice(&slice_key, append_key(key, keylen, time, type), keylen + INTERNAL_KEY_TRAILER_SIZE);
	set_slice(&slice_value, val, vallen);
	ret = table_builder_add(rep, &slice_key, &slice_value);
	if (ret < 0) {
		DEBUG("table_builder_add key= %s value=%s failed !!!\n", key, val);
		return -KCORRUPTION;
	}
	rep->raw_key_size += keylen;
	rep->raw_value_size += vallen;
	rep->num_entries++;
	return KOK;
}

struct table_rep  *shannon_db_sstfilewriter_create(char *db_name, char *cf_name, int cf_index, struct db_sstoptions *option)
{
	if (db_name == NULL || cf_name == NULL || option == NULL) {
		DEBUG("the parameter is NULL,\n");
		goto out;
	}
	char *fname = make_table_filename(db_name, cf_name, cf_index, option->is_rocksdb);
	if (fname == NULL) {
		DEBUG("make_table_filename failed.\n");
		goto out;
	}
	if (!fopen(fname, "w")) {
		DEBUG("open file failed. name = %s .\n", fname);
		goto out;
	}
	struct table_rep *rep = new_table_builder(fname, cf_name, option, cf_index);
	if (rep == NULL) {
		DEBUG("new_table_builder failed.\n");
		goto out;
	}
	if (fname)
		free(fname);
	return rep;
out:
	if (fname)
		free(fname);
	return NULL;
}

void shannon_db_sstfilewriter_finish(struct table_rep *rep)
{
	if (rep)
		table_flush(rep);
	destroy_table_builder(rep);
}

int shannon_db_build_sst(char *devname, struct shannon_db *db, struct db_options *options, struct db_readoptions *roptions, struct db_sstoptions *soptions)
{
	int ret;
	char *p;
	int j;

	calculate_little_endian();

	if (devname == NULL || db == NULL || options == NULL || roptions == NULL || soptions == NULL) {
		DEBUG("the parameter is NULL,\n");
		return -KCORRUPTION;
	}
	struct sst_file_writer *sst_writer = create_sst_file_writer(devname, db, options, roptions);
	if (sst_writer == NULL) {
		DEBUG("create_sst_file_writer failed.\n");
		return -KCORRUPTION;
	}
	for (j = 0; j <  sst_writer->num_cf; ++j) {
		DEBUG("start build cf[%d]\n", j);
		sst_writer->cur_cf_iter = sst_writer->iters_handles[sst_writer->cur_cf_index];
		strcpy(sst_writer->cur_cf_name, sst_writer->column_family_names[sst_writer->cur_cf_index]);
		ret = build_table(sst_writer, soptions, sst_writer->cur_cf_iter, soptions->is_rocksdb);
		if (ret < 0) {
			DEBUG("build_table failed.cur_db_name[%s] cur_cf_name[%s]\n", sst_writer->cur_db_name, sst_writer->cur_cf_name);
		}
		sst_writer->cur_cf_index++;
		DEBUG("done build cf[%d]\n", j);
	}
	destroy_sst_file_writer(sst_writer);
}
