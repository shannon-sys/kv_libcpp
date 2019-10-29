#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <snappy-c.h>
#include <zlib.h>
#include <bzlib.h>
#define DEBUG_CONFIG
#ifdef DEBUG_CONFIG
#define DEBUG(format, arg...) \
	printf("%s(), line=%d: " format, __FUNCTION__, __LINE__, ##arg)
#else
#define DEBUG(format, arg...)
#endif /* end of #ifdef DEBUG_CONFIG */
typedef signed char           int8_t;
typedef signed short          int16_t;
typedef signed int            int32_t;
typedef long int              int64_t;
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef __ssize_t ssize_t;
#define  bool int
#define true 1
#define false 0
typedef uint64_t SequenceNumber;

#define BLOCK_SIZE 4096
#define MAX_KEY_SIZE 128
#define ROCKSDB_FOOT_SIZE 53
#define LEVELDB_FOOT_SIZE 48
#define INTERNAL_KEY_TRAILER_SIZE 8
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED do { } while (0)
#endif

#define max(a, b)    (((a) > (b)) ? (a) : (b))
#define min(a, b)    (((a) < (b)) ? (a) : (b))

struct slice{
	char *data;
	size_t size;
};
typedef struct slice slice_t;

static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

enum {
	KOK = 0,
	KNOT_FOUND = 1,
	KCORRUPTION = 2,
	KNOT_SUPPORTED = 3,
	KINVALID_ARGUMENT = 4,
	KIO_ERROR = 5,
	KNO_COMPRESSION = 0,
	KSNAPPY_COMPRESSION = 1,
	KZLIB_COMPRESSION = 2,
	KBZIP2_COMPRESSION = 3,
	KNO_CHECKSUM = 0,
	KCRC32C_CHECKSUM = 1,
	KXXHASH_CHECKSUM = 2,
	KTYPE_DELETION = 0,
	KTYPE_VALUE = 1,
	DATA_BLOCK = 0,
	INDEX_BLOCK = 1,
	META_BLOCK = 2,
	KBINARY_SEARCH = 0,
	KHASH_SEARCH = 1,
	KTWO_LEVEL_INDEX_SEARCH = 2
};

struct internalkey_t{
	int size;
	uint64_t sequence;
	int type;
	char *data;
};
extern char *compression_type_to_string(int compression_type);
extern int compress_block(slice_t *output, slice_t *input, int type);
extern bool good_compression_ratio(size_t compressed_size, size_t raw_size);
extern int create_dir(const char *sPathName);
extern int varint_length(uint64_t v);
extern size_t put_fixed32(char *dst, uint32_t value);
extern size_t put_fixed64(char *dst, uint64_t value);
extern size_t put_varint64(char *dst, uint64_t v);
extern size_t put_varint32(char *dst, uint32_t v);
extern uint32_t crc32c_extend(uint32_t crc, char *buf, size_t size);
extern uint32_t crc32c_value(char *data, size_t n);
extern uint32_t crc32c_mask(uint32_t crc);
extern uint32_t crc32c_unmask(uint32_t masked_crc);
extern char *make_table_filename(char *name, char *cfname, uint64_t number, int is_rocksdb);
extern void get_filesize(char *filename, size_t *file_size);
extern int write_raw(int fd, const char *p, size_t n);
extern void encode_fixed32(char *buf, uint32_t value);
extern struct slice *new_slice(char *str, size_t n);
extern void set_slice(slice_t *slice, char *start, size_t n);
extern struct internalkey_t *internal_key(char *str, int n, uint64_t seq, int type);
extern char *append_internalkey(struct internalkey_t *key);
extern size_t internal_key_size(struct internalkey_t *key);
extern struct slice *extract_userkey(struct slice *internal_key);
extern char *append_key(char *key, int keylen, uint64_t time, int type);
extern uint64_t extract_seq(struct slice *internal_key);
extern uint32_t extract_type(struct slice *internal_key);
extern char *find_key_between_start_limit(char *start_data, int start_size,  char *limit_data, int limit_size);
/*** kxxhash32 ***/
extern uint32_t xxhash32(char *data, int len, uint32_t seed);
extern uint32_t decode_fixed32(char *ptr);
extern uint64_t decode_fixed64(char *ptr);
extern int get_varint32(slice_t *input, uint32_t *value);
extern int get_varint64(slice_t *input, uint64_t *value);
extern int compare(char *a, int a_len, char *b, int b_len);

/*** endian ***/
extern void calculate_little_endian();

/*** file operater ***/
extern void get_filesize(char *filename, size_t *file_size);
extern size_t read_file(char *filename, uint64_t offset, uint64_t size, slice_t *result);
