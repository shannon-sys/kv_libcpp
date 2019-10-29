#include "util.h"
#include "vector.h"
#define NAME_LEN  32
#define LOGARITHMIC_GROWTH

static const size_t kBlockTrailerSize = 5;
struct options_t {
	int block_restart_data_interval;
	int block_restart_index_interval;
	int block_size;
	int compression;
	int is_rocksdb;
};
struct block_builder{
	int	block_size;
	size_t offset;
	uint32_t restarts[32];
	int     restart_id;
	int     counter;
	bool    finished;
	int     num_restarts;
	int	block_restart_interval;
	char	*last_key;
	char 	*buffer;
};

struct index_block {
	int	block_size;
	int	block_restart_interval;
	int     num_restarts;
	int     factor;
	size_t  offset;
	char	*buffer;
	int 	*vector_restarts;
};

struct filter_block {
	int	block_restart_interval;
	int     num_restarts;
	uint32_t restarts[32];
	int     restart_id;
	size_t  offset;
	char	*buffer;
};
struct properities_block {
	uint64_t raw_key_size;
	uint64_t raw_value_size;
	uint64_t data_size;
	uint64_t index_size;
	uint64_t index_partitions;
	uint64_t top_level_index_size;
	uint64_t index_key_is_user_key;
	uint64_t index_value_is_delta_encoded;
	uint64_t num_entries;
	uint64_t num_data_blocks;
	uint64_t num_deletions;
	uint64_t num_merge_operands;
	uint64_t num_range_deletions;
	uint64_t format_version;
	uint64_t filter_size;
	uint64_t fixed_key_len;
	uint64_t column_family_id;
	uint64_t creation_time;
	uint64_t oldest_key_time;
	char filter_policy_name[NAME_LEN];
	char column_family_name[NAME_LEN];
	char compression_name[NAME_LEN];
	int     num_restarts;
	uint32_t restarts[32];
	int     restart_id;
	size_t  offset;
	char	*buffer;
};
struct index_block_record{
	char *key;
	struct block_handle *handle;
};

struct writable_file {
	char filename[10];
	int fd;
	size_t file_size;
};
enum { kMaxEncodedLength = 10 + 10 };
enum { kEncodedLength = 2*kMaxEncodedLength + 8};
static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;
static const uint64_t rTableMagicNumber = 0x88e241b785f4cff7ull;
struct block_handle {
	uint64_t offset;
	uint64_t size;
	int encode_max_length;
};
struct footer {
	struct block_handle *metaindex_handle;
	struct block_handle *index_handle;
	int encode_length;
	char *buffer;
	size_t  offset;
};

extern void index_block_add_restart(struct index_block *index_block);
extern struct index_block *new_index_block(int interval, int block_size);
extern size_t index_block_current_size(struct index_block *block);
extern int block_add_record(struct block_builder *builder, const slice_t *key, const  slice_t *value);
extern struct slice *meta_index_block_finish(struct index_block *block);
extern struct slice *index_block_finish(struct index_block *block);
extern struct slice *block_builder_finish(struct block_builder *block);
extern struct properities_block *new_properities_block();
extern struct index_block *new_meta_index_block();
extern struct block_handle *new_block_handle();
extern void properities_block_finish(struct properities_block *block);
extern struct block_builder *new_data_block(int block_restart_interval, int size);
extern void block_reset(struct block_builder *rep);
extern slice_t *new_slice(char *str, size_t n);
extern void delete_slice(struct slice *sli);
extern void block_builder_add(struct block_builder *builder, const slice_t *key, const slice_t *value);
extern void set_offset(struct block_handle *handle, uint64_t offset);
extern void set_size(struct block_handle *handle, uint64_t size);
extern size_t block_current_size(struct block_builder *block);
extern struct options_t *new_options(int data_interval, int index_interval, int size, int type, int i);
extern char *leveldb_footer_encode_to(struct footer *foot);
extern void set_metaindex_handle(struct footer *foot, struct block_handle *h);
extern struct footer *new_leveldb_footer();
extern void set_index_handle(struct footer *foot, struct block_handle *h);

extern void set_last_key(char *last_key, char *key, size_t size);
extern char *block_handle_encode_to(struct block_handle *handle);
extern int writefile_append(char *file, char *data, size_t n);
extern int index_block_add(struct index_block *index_block, const slice_t *key, const slice_t *value);
extern size_t block_handle_length(struct block_handle *handle);

extern void destroy_filter_block(struct filter_block *block);
extern void destroy_handle(struct block_handle *handle);
extern void destroy_index_block(struct index_block *block);
extern void destroy_prop_block(struct properities_block *block);
extern void destroy_data_block(struct block_builder *block);
extern struct footer *new_rocksdb_footer();
extern void footer_encode_to(struct footer *foot, int type);
extern void destroy_foot(struct footer *foot);
static char *kDataSize  = "rocksdb.data.size";
static char *kIndexSize = "rocksdb.index.size";
static char *kIndexPartitions = "rocksdb.index.partitions";
static char *kTopLevelIndexSize = "rocksdb.top-level.index.size";
static char *kIndexKeyIsUserKey = "rocksdb.index.key.is.user.key";
static char *kIndexValueIsDeltaEncoded = "rocksdb.index.value.is.delta.encoded";
static char *kFilterSize = "rocksdb.filter.size";
static char *kRawKeySize = "rocksdb.raw.key.size";
static char *kRawValueSize = "rocksdb.raw.value.size";
static char *kNumDataBlocks = "rocksdb.num.data.blocks";
static char *kNumEntries = "rocksdb.num.entries";
static char *kDeletedKeys = "rocksdb.deleted.keys";
static char *kMergeOperands = "rocksdb.merge.operands";
static char *kNumRangeDeletions = "rocksdb.num.range-deletions";
static char *kFilterPolicy = "rocksdb.filter.policy";
static char *kFormatVersion = "rocksdb.format.version";
static char *kFixedKeyLen = "rocksdb.fixed.key.length";
static char *kColumnFamilyId = "rocksdb.column.family.id";
static char *kColumnFamilyName = "rocksdb.column.family.name";
static char *kComparator = "rocksdb.comparator";
static char *kMergeOperator = "rocksdb.merge.operator";
static char *kPrefixExtractorName = "rocksdb.prefix.extractor.name";
static char *kPropertyCollectors = "rocksdb.property.collectors";
static char *kCompression = "rocksdb.compression";
static char *kCreationTime = "rocksdb.creation.time";
static char *kOldestKeyTime = "rocksdb.oldest.key.time";
static char *kVersion =  "rocksdb.external_sst_file.version";
static char *kGlobalSeqno = "rocksdb.external_sst_file.global_seqno";
static char *kPropertiesBlock = "rocksdb.properties";
static char *kPropertiesBlockOldName = "rocksdb.stats";
static char *kCompressionDictBlock = "rocksdb.compression_dict";
static char *kRangeDelBlock = "rocksdb.range_del";
