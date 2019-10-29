#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <snappy-c.h>
#include "../sst_table.h"

int compress(slice_t *output, slice_t *input)
{
	int ret = KOK;
	char *data = input->data;
	size_t size = input->size;
	char *ptr;
	size_t ptr_size;

	ptr_size = snappy_max_compressed_length(size);
	ptr = (char *)malloc(ptr_size + KBLOCK_TRAILER_SIZE + 1);
	ret = snappy_compress(data, size, ptr, &ptr_size);
	if (ret == KOK) {
		DEBUG("input_len=%d, output_size=%d\n", (int)size, (int)ptr_size);
		ptr[ptr_size] = KSNAPPY_COMPRESSION;
		*((int *)(ptr + ptr_size + 1)) = 0;
		ptr[ptr_size + KBLOCK_TRAILER_SIZE] = '\0';
		set_slice(output, ptr, ptr_size + KBLOCK_TRAILER_SIZE);
	} else {
		DEBUG("snappy_compress failed\n");
		free(ptr);
		set_slice(output, NULL, 0);
	}

	return ret;
}

void usage() {
	printf("uncompress_test usage:\n");
	printf("    ./uncompress_test /path/to/inputfile\n");
	printf("example:\n");
	printf("    ./uncompress_test uncompress_test.c\n");
}

int read_compress_and_uncompress_check(char *filename)
{
	int ret = KOK;
	size_t file_size, read_file_size;
	slice_t file_content;
	slice_t compress_content;
	slice_t uncompress_content;

	set_slice(&file_content, NULL, 0);
	set_slice(&compress_content, NULL, 0);
	set_slice(&uncompress_content, NULL, 0);

	// readfile
	DEBUG("=== begin check file=%s\n", filename);
	get_filesize(filename, &file_size);
	if (file_size == 0) {
		DEBUG("file len is 0, pelease use another\n");
		return -1;
	}
	read_file_size = read_file(filename, 0, file_size, &file_content);
	if (read_file_size != file_size) {
		DEBUG("read file fail\n");
		return -1;
	}

	// compress file_content
	ret = compress(&compress_content, &file_content);
	if (ret != KOK) {
		goto free_file_content;
	}

	// uncompress compress_content
	ret = check_and_uncompress_block(&uncompress_content, &compress_content, KNO_CHECKSUM);
	if (ret != KOK) {
		DEBUG("uncompress failed \n");
		goto free_file_content;
	}

	// compare
	if (uncompress_content.size != file_content.size) {
		DEBUG("uncompress failed, file_size error\n");
		ret = KCORRUPTION;
		goto free_uncompress_content;
	}
	if (strncmp(file_content.data, uncompress_content.data, file_size) == 0) {
		DEBUG("==== file=%s uncompress PASS!\n", filename);
		ret = KOK;
	} else {
		DEBUG("file=%s uncompress mismatch!\n", filename);
		ret = KCORRUPTION;
	}

free_uncompress_content:
	if (uncompress_content.data)
		free(uncompress_content.data);
free_compress_content:
	if (compress_content.data)
		free(compress_content.data);
free_file_content:
	if (file_content.data)
		free(file_content.data);
	return ret;
}


int main(int argc, char *argv[])
{
	int ret = KOK;
	size_t file_size, read_file_size;
	slice_t file_content;
	slice_t compress_content;
	slice_t uncompress_content;
	char *filename;

	set_slice(&file_content, NULL, 0);
	set_slice(&compress_content, NULL, 0);
	set_slice(&uncompress_content, NULL, 0);
	if (argc != 2) {
		usage();
		return -1;
	}
	filename = argv[1];

	ret = read_compress_and_uncompress_check(filename);
	if (ret == KOK) {
		DEBUG("=== uncompress PASS!\n");
	}

	return ret;
}
