// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#include "sst_table.h"
#include "block_builder.h"
#include "dbformat.h"
#include "swift/env.h"
#include "swift/options.h"
#include "swift/slice.h"
#include "swift/status.h"
#include "swift/write_batch.h"
#include "table_builder.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/filename.h"
#include "util/fileoperate.h"
#include "util/xxhash.h"
#include <error.h>
#include <fcntl.h>
#include <iostream>
#include <snappy-c.h>
#include <stdio.h>
#include <stdlib.h>

namespace shannon {

static void KvNodeDestroy(KvNode *kv) {
  if (kv) {
    if (kv->key)
      free(kv->key);
    if (kv->value)
      free(kv->value);
    free(kv);
  }
}

Status BlockHandleDecodeFrom(BlockHandle *dst, Slice *input) {
  Status s;
  if (GetVarint64(input, &dst->offset) && GetVarint64(input, &dst->size) &&
      dst->size > 0) {
    // DEBUG("offset=%llu, size=%llu\n", dst->offset, dst->size);
    return s;
  } else {
    DEBUG("Error: bad block handle\n");
    return Status::Corruption("bad block handle");
  }
}

Status FootDecodeFrom(Foot *dst, Slice *input, int verify) {
  Status s;
  const char *magic_ptr = input->data() + kNewVersionEncodedLenFoot - 8;
  size_t delta = kNewVersionEncodedLenFoot - kEncodedLenFoot;
  uint32_t magic_lo = DecodeFixed32(magic_ptr);
  uint32_t magic_hi = DecodeFixed32(magic_ptr + 4);
  uint64_t magic = ((uint64_t)(magic_hi) << 32) | ((uint64_t)(magic_lo));
  const char *end = magic_ptr + 8;

  if (magic == kBasedTableMagicNumber) {
    dst->legacy_footer_format = 0;
    dst->checksum_type = (verify) ? input->data()[0] : kNoChecksum;
    if (dst->checksum_type > kxxHash) {
      DEBUG("checksum type not support=%d\n", dst->checksum_type);
      return Status::NotSupported("checksum type not support");
    }
    *input = Slice(input->data() + 1, input->size() - 1);
    s = BlockHandleDecodeFrom(&dst->metaindex_handle, input);
    if (s.ok())
      s = BlockHandleDecodeFrom(&dst->index_handle, input);
    if (s.ok())
      dst->version = DecodeFixed32(input->data());
  } else if (magic == kLegacyTableMagicNumber) {
    dst->legacy_footer_format = 1;
    dst->checksum_type = (verify) ? kCRC32c : kNoChecksum;
    *input = Slice(input->data() + delta, kEncodedLenFoot);
    s = BlockHandleDecodeFrom(&dst->metaindex_handle, input);
    if (s.ok())
      s = BlockHandleDecodeFrom(&dst->index_handle, input);
  } else {
    DEBUG("unknow magic number, magic=0x%lx\n", magic);
    return Status::NotSupported("unknow magic number\n");
  }

  return s;
}

Status ReadFoot(Slice *result, char *filename) {
  ssize_t read_size = 0;
  size_t file_size = 0;
  uint64_t offset = 0;
  uint64_t size = kNewVersionEncodedLenFoot;
  Status s;

  GetFilesize(filename, &file_size);
  if (file_size == 0 || file_size <= size) {
    DEBUG("corruption sst file, file len is too small\n");
    return Status::Corruption("sst file is too small");
  }
  offset = (uint64_t)(file_size) - size;
  read_size = ReadFile(filename, offset, size, result);
  if (read_size != size)
    s = Status::IOError("read sst file foot error");

  return s;
}

static Status CheckBlockChecksum(Slice *input, uint8_t checksum_type) {
  const char *data = input->data();
  size_t n = input->size() - kBlockTrailerSize;
  uint32_t value, actual;
  Status s;

  switch (checksum_type) {
  case kNoChecksum:
    break;
  case kCRC32c:
    value = crc32c::Unmask(DecodeFixed32(data + n + 1));
    actual = crc32c::Value(data, n + 1);
    break;
  case kxxHash:
    value = DecodeFixed32(data + n + 1);
    actual = XXH32(data, n + 1, 0);
    break;
  default:
    DEBUG("unknown checksum type:%u\n", (uint32_t)checksum_type);
    return Status::NotSupported("unknown checksum type");
  }

  if (checksum_type == kNoChecksum || actual == value)
    return s;
  else {
    DEBUG("Error: checksum mismatch!value=%u, actual=%u\n", value, actual);
    return Status::Corruption("checksum mismath!");
  }
}

static Status UncompressBlock(Slice *result, Slice *input) {
  char *data = (char *)input->data(), *ubuf = NULL;
  size_t n = input->size() - kBlockTrailerSize, ulength = 0;
  Status s;

  switch (data[n]) {
  case kNoCompression:
    *result = Slice(data, n);
    break;
  case kSnappyCompression:
    if (snappy_uncompressed_length(data, n, &ulength) != SNAPPY_OK) {
      DEBUG("uncompressed block contents\n");
      return Status::Corruption("uncompressed block fail");
    }
    ubuf = (char *)malloc(ulength);
    if (snappy_uncompress(data, n, ubuf, &ulength) != SNAPPY_OK) {
      if (ubuf)
        free(ubuf);
      DEBUG("corrupted uncompressed block contents\n");
      return Status::Corruption("uncompressed block fail");
    }
    *result = Slice(ubuf, ulength);
    break;
  default:
    DEBUG("unknown block compress type, type=%d\n", (int)data[n]);
    return Status::NotSupported("unknow block compress type");
  }
  return s;
}

Status CheckAndUncompressBlock(Slice *result, Slice *input,
                               uint8_t checksum_type) {
  Status s;
  s = CheckBlockChecksum(input, checksum_type);
  if (s.ok())
    s = UncompressBlock(result, input);
  return s;
}

// block is data block, meta block, meta index block or index block
Status ReadBlock(Slice *result, char *filename, BlockHandle *handle,
                 uint8_t checksum_type) {
  ssize_t read_size = 0;
  Slice file_content;
  Status s;

  read_size = ReadFile(filename, handle->offset,
                       handle->size + kBlockTrailerSize, &file_content);
  if (read_size == 0 || read_size != handle->size + kBlockTrailerSize) {
    s = Status::IOError();
    goto out;
  }
  s = CheckAndUncompressBlock(result, &file_content, checksum_type);
  if (!s.ok())
    goto free_file_content;
  if (file_content.data() != result->data())
    free((void *)file_content.data());
  return s;

free_file_content:
  if (file_content.data())
    free((void *)file_content.data());
out:
  return s;
}

static Status GetDataBlockRestartNums(uint32_t *nums, Slice *data_block) {
  const char *data = data_block->data();
  size_t n = data_block->size() - 4;

  if (n < 0) {
    DEBUG("block length is too small, length=%u\n", (uint32_t)n);
    return Status::Corruption("block length is too small");
  }
  *nums = DecodeFixed32(data + n);
  if (*nums < 0) {
    DEBUG("block restart nums is error, nums=%u\n", *nums);
    return Status::Corruption("block restart nums");
  }

  return Status::OK();
}

Status GetDataBlockRestartOffset(BlockRep *rep) {
  Slice data_block = Slice(rep->block.data(), rep->block.size());
  uint32_t *restarts = &rep->restart_nums;
  uint32_t **restart_offset = &rep->restart_offset;
  uint32_t offset_tmp;
  int i;
  const char *data = data_block.data();
  size_t n = data_block.size() - 4;
  Status s;

  s = GetDataBlockRestartNums(restarts, &data_block);
  if (!s.ok())
    return s;
  n -= (*restarts) * 4;
  if (n <= 0) {
    DEBUG("block length is smaller than restarts\n");
    return Status::Corruption("block length is smaller than restarts");
  }

  *restart_offset = (uint32_t *)malloc((*restarts) * sizeof(uint32_t));
  if (*restart_offset == NULL) {
    DEBUG("corrupted malloc restart_offset\n");
    return Status::Corruption("corrupted malloc restart offset");
  }
  memset(*restart_offset, 0, (*restarts) * sizeof(uint32_t));
  for (i = 0; i < (*restarts); ++i) {
    offset_tmp = DecodeFixed32(data + n + 4 * i);
    if (offset_tmp < 0 || offset_tmp >= n) {
      DEBUG("block restart offset overstep the boundary\n");
      s = Status::Corruption("block restart offset overstep the boundary");
      goto free_restart_offset;
    }
    *((*restart_offset) + i) = offset_tmp;
  }
  rep->data_size = n;
  return s;

free_restart_offset:
  if (*restart_offset)
    free(*restart_offset);
  return s;
}

static Status DecodeDataBlockOneRecord(KvNode *kv, Slice *input,
                                       KvNode *last_kv, bool key_is_user_key) {
  uint32_t internal_shared_key_len, internal_non_shared_key_len;
  uint32_t internal_key_len, value_len;
  char *last_key = (last_kv) ? last_kv->key : NULL;
  size_t last_key_len = (last_kv) ? last_kv->key_len : 0;
  char *internal_key = NULL;
  uint64_t sequence;
  Status s;

  GetVarint32(input, &internal_shared_key_len);
  GetVarint32(input, &internal_non_shared_key_len);
  GetVarint32(input, &value_len);

  // get internal_key
  if (internal_shared_key_len > last_key_len) {
    DEBUG("shared_key_len is too big\n");
    return Status::Corruption("shared key len is too big");
  }
  internal_key_len = internal_shared_key_len + internal_non_shared_key_len;
  if ((!key_is_user_key && internal_key_len <= 8) ||
      (key_is_user_key && internal_key_len <= 0)) {
    DEBUG("key len is too small, key_len=%u\n", (uint32_t)internal_key_len);
    return Status::Corruption("key len is too small");
  }

  internal_key = (char *)malloc(internal_key_len);
  if (internal_key == NULL) {
    DEBUG("corrupted malloc internal key\n");
    return Status::Corruption("malloc internal key");
  }
  if (internal_shared_key_len > 0) {
    if (last_key == NULL) {
      DEBUG("here shared_key_len must be zero!\n");
      s = Status::Corruption("here shared_key_len must be zero!");
      goto free_internal_key;
    }
    memcpy(internal_key, last_key, internal_shared_key_len);
  }
  memcpy(internal_key + internal_shared_key_len, input->data(),
         internal_non_shared_key_len);

  // update input
  *input = Slice(input->data() + internal_non_shared_key_len,
                 input->size() - internal_non_shared_key_len);

  // decode internal key
  if (!key_is_user_key) {
    kv->key_len = internal_key_len - 8;
    sequence = DecodeFixed64(internal_key + kv->key_len);
    kv->type = (uint32_t)(sequence & 0xffull);
    kv->sequence = sequence >> 8;
  } else
    kv->key_len = internal_key_len;
  kv->key = (char *)malloc(kv->key_len + 1);
  if (kv->key == NULL) {
    DEBUG("corrupted malloc kv->key\n");
    s = Status::Corruption("corrupted malloc kv->key");
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
  if (!key_is_user_key && kv->type == kSstTypeDeletion) {
    kv->value = NULL;
    if (kv->value_len != 0) { // FIXME
      DEBUG("corrupted value_len for delete type\n");
      s = Status::Corruption("value_len must be zero for delete type");
      goto free_kv_key;
    }
  } else {
    if (kv->value_len < 0 || kv->value_len > input->size()) {
      DEBUG("corrupted value_len for value type\n");
      s = Status::Corruption("value_len overstep the boundary");
      goto free_kv_key;
    }
    kv->value = (char *)malloc(kv->value_len + 1);
    if (kv->value == NULL) {
      DEBUG("corrupted malloc kv->value\n");
      s = Status::Corruption("corrupted malloc kv->value");
      goto free_kv_key;
    }
    memcpy(kv->value, input->data(), kv->value_len);
    kv->value[kv->value_len] = '\0';
    // update input
    *input =
        Slice(input->data() + kv->value_len, input->size() - kv->value_len);
  }
  return s;

free_kv_key:
  if (kv->key) {
    free(kv->key);
    kv->key = NULL;
  }
free_internal_key:
  if (internal_key)
    free(internal_key);
out:
  return s;
}

static Status BatchWriteAndClearNonatomic(DatabaseOptions *opt) {
  Status s;
  DB *db = opt->db;
  WriteBatchNonatomic *wb = opt->wb;

  s = db->WriteNonatomic(opt->write_opt, wb);
  wb->Clear();

  return s;
}

Status ProcessOneKv(BlockRep *rep, KvNode *kv) {
  Status s;
  DatabaseOptions *opt = NULL;
  Slice key, value;

  if (rep && rep->opt)
    opt = rep->opt;
  else {
    DEBUG("Error data_block_rep options\n");
    return Status::Corruption("error data block rep options");
  }

  // flush all kv in batch
  if (rep->last_data_block && opt->end) {
    s = BatchWriteAndClearNonatomic(opt);
    if (!s.ok())
      DEBUG("the last write to ssd fail\n");
    goto out;
  }

  if (kv == NULL) {
    DEBUG("Error input kv = NULL\n");
    s = Status::Corruption("Error input kv");
    goto out;
  }

  if (kv->type == kSstTypeValue && kv->key && kv->value) {
    key = Slice((const char *)kv->key, kv->key_len);
    value = Slice((const char *)kv->value, kv->value_len);
  batch_put_try:
    if (opt->cf == NULL)
      s = opt->wb->Put(key, value, kv->sequence);
    else
      s = opt->wb->Put(opt->cf, key, value, kv->sequence);
    if (s.IsBatchFull()) {
      s = BatchWriteAndClearNonatomic(opt);
      if (!s.ok()) {
        DEBUG("write to ssd fail\n");
        goto out;
      }
      goto batch_put_try;
    }
    if (!s.ok()) {
      DEBUG("write batch add put_cmd fail\n");
      goto out;
    }
    rep->kv_put_count++;
  } else if (kv->type == kSstTypeDeletion && kv->key) {
    key = Slice((const char *)kv->key, kv->key_len);
  batch_delete_try:
    if (opt->cf == NULL)
      s = opt->wb->Delete(key, kv->sequence);
    else
      s = opt->wb->Delete(opt->cf, key, kv->sequence);
    if (s.IsBatchFull()) {
      s = BatchWriteAndClearNonatomic(opt);
      if (!s.ok()) {
        DEBUG("write to ssd fail\n");
        goto out;
      }
      goto batch_delete_try;
    }
    if (!s.ok()) {
      DEBUG("write batch add delete_cmd fail\n");
      goto out;
    }
    rep->kv_del_count++;
  } else {
    DEBUG("Error do not support kv->type=%d\n", kv->type);
    s = Status::NotSupported("Error kv->type");
    goto out;
  }

out:
  return s;
}

Status DecodeDataBlockRestartInterval(BlockRep *rep, int index) {
  Slice interval;
  char *data;
  size_t size;
  KvNode *kv, *last_kv, *tmp_kv;
  Status s;

  kv = (KvNode *)malloc(sizeof(KvNode));
  if (kv == NULL) {
    DEBUG("corrupted malloc KvNode *kv\n");
    return Status::Corruption("corrupted malloc KvNode *kv");
  }
  memset((void *)kv, 0, sizeof(KvNode));

  last_kv = (KvNode *)malloc(sizeof(KvNode));
  if (last_kv == NULL) {
    DEBUG("corrupted malloc KvNode *last_kv\n");
    s = Status::Corruption("corrupted malloc KvNode *last_kv");
    goto free_kv;
  }
  memset((void *)last_kv, 0, sizeof(KvNode));

  data = (char *)rep->block.data() + rep->restart_offset[index];
  if (index + 1 < rep->restart_nums)
    size = rep->restart_offset[index + 1] - rep->restart_offset[index];
  else
    size = rep->data_size - rep->restart_offset[index];
  interval = Slice((const char *)data, size);

  while (interval.size() > 0) {
    s = DecodeDataBlockOneRecord(kv, &interval, last_kv, false);
    if (!s.ok())
      goto free_last_kv;
    s = ProcessOneKv(rep, kv);
    if (!s.ok())
      goto free_last_kv;
    tmp_kv = last_kv;
    last_kv = kv;
    kv = tmp_kv;
    if (kv->key)
      free(kv->key);
    if (kv->value)
      free(kv->value);
    memset(kv, 0, sizeof(KvNode));
  }

free_last_kv:
  KvNodeDestroy(last_kv);
free_kv:
  KvNodeDestroy(kv);
  return s;
}

Status DecodeDataBlock(BlockRep *rep) {
  int nums = 0, i;
  Status s;

  s = GetDataBlockRestartOffset(rep);
  if (!s.ok())
    return s;
  nums = rep->restart_nums;
  for (i = 0; i < nums; i++) {
    s = DecodeDataBlockRestartInterval(rep, i);
    if (!s.ok()) {
      DEBUG("decode restart interval index=%d fail\n", i);
      return s;
    }
  }
  if (rep->last_data_block) {
    rep->opt->end = 1;
    ProcessOneKv(rep, NULL);
  }
  return s;
}

Status GetIndexBlockRestartOffset(BlockRep *rep) {
  return GetDataBlockRestartOffset(rep);
}

static Status DecodeIndexBlockOneRecord(KvNode *kv, Slice *input) {
  // if key is not user key, we do not use sequence
  // so can force key_is_user_key=true
  return DecodeDataBlockOneRecord(kv, input, NULL, true);
}

Status ProcessOneBlockHandle(BlockRep *rep, KvNode *kv) {
  BlockHandle block_handle;
  Slice value(kv->value, kv->value_len);
  BlockRep *data_block_rep;
  Status s;

  s = BlockHandleDecodeFrom(&block_handle, &value);
  if (!s.ok()) {
    DEBUG("Error decode data block handle\n");
    return s;
  }

  data_block_rep = (BlockRep *)malloc(sizeof(BlockRep));
  if (data_block_rep == NULL) {
    DEBUG("corrupted malloc data_block_rep\n");
    return Status::Corruption("corrupted malloc data_block_rep");
  }
  memset(data_block_rep, 0, sizeof(BlockRep));
  data_block_rep->filename = rep->filename;
  data_block_rep->opt = rep->opt;
  data_block_rep->checksum_type = rep->checksum_type;
  data_block_rep->last_data_block = rep->last_data_block;
  s = ReadBlock(&data_block_rep->block, data_block_rep->filename, &block_handle,
                data_block_rep->checksum_type);
  if (!s.ok())
    goto free_data_block_rep;
  s = DecodeDataBlock(data_block_rep);
  if (s.ok()) {
    rep->data_block_count++;
    rep->kv_put_count += data_block_rep->kv_put_count;
    rep->kv_del_count += data_block_rep->kv_del_count;
  }

free_block_data:
  if (data_block_rep && data_block_rep->block.data())
    free((void *)data_block_rep->block.data());
free_block_rep_restart:
  if (data_block_rep && data_block_rep->restart_offset)
    free(data_block_rep->restart_offset);
free_data_block_rep:
  if (data_block_rep)
    free(data_block_rep);
  return s;
}

Status DecodeIndexBlockRestartInterval(BlockRep *rep, int index) {
  Slice interval;
  KvNode kv;
  Status s;
  char *data;
  size_t size;

  memset(&kv, 0, sizeof(KvNode));
  data = (char *)rep->block.data() + rep->restart_offset[index];
  if (index + 1 < rep->restart_nums)
    size = rep->restart_offset[index + 1] - rep->restart_offset[index];
  else
    size = rep->data_size - rep->restart_offset[index];
  interval = Slice((const char *)data, size);

  if (interval.size() > 0) {
    s = DecodeIndexBlockOneRecord(&kv, &interval);
    if (!s.ok())
      goto free_kv;
    s = ProcessOneBlockHandle(rep, &kv);
    if (!s.ok())
      goto free_kv;
  } else {
    DEBUG("Error decode index block\n");
    s = Status::Corruption("Error decode index block");
  }

free_kv:
  if (kv.key)
    free(kv.key);
  if (kv.value)
    free(kv.value);
  return s;
}

Status DecodeIndexBlock(BlockRep *rep) {
  int nums = 0, i;
  Status s;

  s = GetIndexBlockRestartOffset(rep);
  if (!s.ok())
    return s;
  nums = rep->restart_nums;
  for (i = 0; i < nums; i++) {
    if (i == nums - 1)
      rep->last_data_block = 1;
    s = DecodeIndexBlockRestartInterval(rep, i);
    if (!s.ok()) {
      DEBUG("decode data_block[%d] fail\n", i);
      return s;
    }
  }
  return s;
}

static Status GetPropertyBlockRestartOffset(BlockRep *rep) {
  return GetDataBlockRestartOffset(rep);
}

static Status DecodePropertyBlockOneRecord(KvNode *kv, Slice *input,
                                           KvNode *last_kv) {
  return DecodeDataBlockOneRecord(kv, input, last_kv, true);
}

static Status ProcessOnePropertyKv(BlockRep *rep, KvNode *kv, int *find) {
  Status s;

  // case: column family name
  if (strlen(kColumnFamilyName) == kv->key_len &&
      strncmp(kColumnFamilyName, kv->key, kv->key_len) == 0) {
    if (kv->value_len > CF_NAME_LEN) {
      DEBUG("column family name is too long. cf_len=%d.\n", kv->value_len);
      return Status::Corruption("column family name is too long.");
    }
    memcpy(rep->props->cf_name, kv->value, kv->value_len);
    rep->props->cf_name[kv->value_len] = '\0';
    (*find)++;
  }

  // case: index type
  if (strlen(kIndexType) == kv->key_len &&
      strncmp(kIndexType, kv->key, kv->key_len) == 0) {
    rep->props->index_type = ((char *)kv->value)[0];
    (*find)++;
  }

  return s;
}

Status DecodePropertyBlock(BlockRep *rep) {
  Slice my_block = Slice(rep->block.data(), rep->block.size());
  KvNode *kv, *last_kv, *tmp_kv;
  Status s;
  int find = 0;

  kv = (KvNode *)malloc(sizeof(KvNode));
  if (kv == NULL) {
    DEBUG("corrupted malloc KvNode *kv\n");
    return Status::Corruption("corrupted malloc KvNode *kv");
  }
  memset((void *)kv, 0, sizeof(KvNode));

  last_kv = (KvNode *)malloc(sizeof(KvNode));
  if (last_kv == NULL) {
    DEBUG("corrupted malloc KvNode *last_kv\n");
    s = Status::Corruption("corrupted malloc KvNode *last_kv");
    goto free_kv;
  }
  memset((void *)last_kv, 0, sizeof(KvNode));

  s = GetPropertyBlockRestartOffset(rep);
  if (!s.ok())
    goto free_last_kv;
  my_block = Slice(rep->block.data(), rep->data_size);

  while (my_block.size() > 0) {
    s = DecodePropertyBlockOneRecord(kv, &my_block, last_kv);
    if (!s.ok())
      goto free_last_kv;
    s = ProcessOnePropertyKv(rep, kv, &find);
    if (!s.ok())
      goto free_last_kv;
    if (find >= kProperties)
      break;
    tmp_kv = last_kv;
    last_kv = kv;
    kv = tmp_kv;
    if (kv->key)
      free(kv->key);
    if (kv->value)
      free(kv->value);
    memset(kv, 0, sizeof(KvNode));
  }

  if (s.ok() && find < kProperties && strlen(rep->props->cf_name) == 0) {
    DEBUG("Can not find cf_name in property meta block\n");
    s = Status::NotFound("Can not find cf_name in property meta block");
  }

free_last_kv:
  KvNodeDestroy(last_kv);
free_kv:
  KvNodeDestroy(kv);
  return s;
}

static Status GetMetaIndexBlockRestartOffset(BlockRep *rep) {
  return GetDataBlockRestartOffset(rep);
}

static Status DecodeMetaIndexBlockOneRecord(KvNode *kv, Slice *input,
                                            KvNode *last_kv) {
  return DecodeDataBlockOneRecord(kv, input, last_kv, true);
}

static bool IsPropertyBlock(char *key, int len) {
  if (strlen(kPropertiesBlock) == len &&
      strncmp(kPropertiesBlock, key, len) == 0)
    return true;
  if (strlen(kPropertiesBlockOldName) == len &&
      strncmp(kPropertiesBlockOldName, key, len) == 0)
    return true;
  return false;
}

static Status ProcessOneMetaIndexKv(BlockRep *rep, KvNode *kv, int *find) {
  BlockHandle block_handle;
  Slice value(kv->value, kv->value_len);
  BlockRep *meta_block_rep;
  Status s;

  // now just nead decode property meta block
  if (!IsPropertyBlock(kv->key, kv->key_len))
    return s;
  (*find)++; // find property block
  s = BlockHandleDecodeFrom(&block_handle, &value);
  if (!s.ok()) {
    DEBUG("Error decode meta block handle\n");
    return s;
  }

  meta_block_rep = (BlockRep *)malloc(sizeof(BlockRep));
  if (meta_block_rep == NULL) {
    DEBUG("corrupted malloc meta_block_rep\n");
    return Status::Corruption("corrupted malloc meta_block_rep");
  }
  memset(meta_block_rep, 0, sizeof(BlockRep));
  meta_block_rep->filename = rep->filename;
  meta_block_rep->opt = rep->opt;
  meta_block_rep->checksum_type = rep->checksum_type;
  meta_block_rep->props = rep->props;
  s = ReadBlock(&meta_block_rep->block, meta_block_rep->filename, &block_handle,
                meta_block_rep->checksum_type);
  if (!s.ok())
    goto free_meta_block_rep;
  s = DecodePropertyBlock(meta_block_rep);

free_block_data:
  if (meta_block_rep && meta_block_rep->block.data())
    free((void *)meta_block_rep->block.data());
free_block_rep_restart:
  if (meta_block_rep && meta_block_rep->restart_offset)
    free(meta_block_rep->restart_offset);
free_meta_block_rep:
  if (meta_block_rep)
    free(meta_block_rep);
  return s;
}

Status DecodeMetaIndexBlock(BlockRep *rep) {
  Slice my_block = Slice(rep->block.data(), rep->block.size());
  KvNode *kv, *last_kv, *tmp_kv;
  Status s;
  int find = 0;

  kv = (KvNode *)malloc(sizeof(KvNode));
  if (kv == NULL) {
    DEBUG("corrupted malloc KvNode *kv\n");
    return Status::Corruption("corrupted malloc KvNode *kv");
  }
  memset((void *)kv, 0, sizeof(KvNode));

  last_kv = (KvNode *)malloc(sizeof(KvNode));
  if (last_kv == NULL) {
    DEBUG("corrupted malloc KvNode *last_kv\n");
    s = Status::Corruption("corrupted malloc KvNode *last_kv");
    goto free_kv;
  }
  memset((void *)last_kv, 0, sizeof(KvNode));

  s = GetMetaIndexBlockRestartOffset(rep);
  if (!s.ok())
    goto free_last_kv;
  my_block = Slice(rep->block.data(), rep->data_size);

  while (my_block.size() > 0) {
    s = DecodeMetaIndexBlockOneRecord(kv, &my_block, last_kv);
    if (!s.ok())
      goto free_last_kv;
    s = ProcessOneMetaIndexKv(rep, kv, &find);
    if (!s.ok())
      goto free_last_kv;
    if (find >= kMetaNums)
      break;
    tmp_kv = last_kv;
    last_kv = kv;
    kv = tmp_kv;
    if (kv->key)
      free(kv->key);
    if (kv->value)
      free(kv->value);
    memset(kv, 0, sizeof(KvNode));
  }

  if (s.ok() && find < kMetaNums) {
    DEBUG("can not find property meta block\n");
    s = Status::NotFound("can not find property meta block");
  }

free_last_kv:
  KvNodeDestroy(last_kv);
free_kv:
  KvNodeDestroy(kv);
  return s;
}

Status ProcessMetaIndex(char *filename, MetaBlockProperties *props,
                        Foot *foot) {
  BlockHandle block_handle;
  BlockRep meta_index_block_rep;
  Status s;

  meta_index_block_rep.filename = filename;
  meta_index_block_rep.props = props;
  meta_index_block_rep.checksum_type = foot->checksum_type;
  // read meta index block and decode property block
  s = ReadBlock(&meta_index_block_rep.block, filename, &foot->metaindex_handle,
                foot->checksum_type);
  if (s.ok())
    s = DecodeMetaIndexBlock(&meta_index_block_rep);

  if (meta_index_block_rep.block.data())
    free((void *)meta_index_block_rep.block.data());
  if (meta_index_block_rep.restart_offset)
    free(meta_index_block_rep.restart_offset);
  return s;
}

Status FindCFHandleIndex(std::vector<ColumnFamilyHandle *> *handles, char *name,
                         int name_len, int *index) {
  Status s;
  int i = 0, n;
  std::string cf_name;
  *index = -1;

  n = (*handles).size();
  for (i = 0; i < n; ++i) {
    cf_name = (*handles)[i]->GetName();
    if (cf_name.size() != name_len)
      continue;
    if (cf_name.compare(name) != 0)
      continue;
    *index = i;
    break;
  }

  if (*index == -1) {
    DEBUG("can not find column_family=%s, please check your handles or create "
          "it before.\n",
          name);
    return Status::NotFound("can not find column family");
  }

  return s;
}

static Status IsIndexTypeSupport(IndexType index_type) {
  Status s;

  switch (index_type) {
  case kBinarySearch:
    return s;
  default:
    DEBUG("index_type=%d is not support.\n", (int)index_type);
    return Status::NotSupported("index type is not support");
  }
}

Status AnalyzeSst(char *filename, int verify, DB *db,
                  std::vector<ColumnFamilyHandle *> *handles) {
  Status s;
  Slice foot_content, tmp_foot_content;
  Foot foot;
  BlockRep index_block_rep;
  DatabaseOptions db_opt;
  WriteBatchNonatomic wb;
  MetaBlockProperties props;
  int cf_handle_index = -1;

  if (filename == NULL || db == NULL) {
    DEBUG("please provide filename and open db");
    return Status::Corruption("please provide filename and open db");
  }
  memset((void *)&index_block_rep, 0, sizeof(BlockRep));
  memset((void *)&db_opt, 0, sizeof(DatabaseOptions));
  memset((void *)&props, 0, sizeof(MetaBlockProperties));
  index_block_rep.filename = filename;
  index_block_rep.opt = &db_opt;

  // read foot_content from file and decode to foot
  s = ReadFoot(&foot_content, filename);
  if (!s.ok())
    goto out;
  tmp_foot_content = Slice(foot_content.data(), foot_content.size());
  s = FootDecodeFrom(&foot, &tmp_foot_content, verify);
  if (!s.ok())
    goto free_foot_content;
  index_block_rep.checksum_type = foot.checksum_type;

  // set db_opt
  db_opt.db = db;
  db_opt.wb = &wb;
  db_opt.write_opt.sync = true;
  db_opt.write_opt.fill_cache = true;

  // decode property block, get properties
  if (handles == NULL || foot.legacy_footer_format) {
    db_opt.cf = NULL;
    fprintf(stderr, "kv will write to cf_name=default.\n");
  } else {
    DEBUG("decode meta index block in file=%s\n", filename);
    s = ProcessMetaIndex(filename, &props, &foot);
    if (s.ok())
      s = FindCFHandleIndex(handles, props.cf_name, strlen(props.cf_name),
                            &cf_handle_index);
    if (s.ok())
      s = IsIndexTypeSupport(IndexType(props.index_type));
    if (!s.ok())
      goto free_foot_content;
    db_opt.cf = (*handles)[cf_handle_index];
    fprintf(stderr, "decode cf_name=%s, kv will write to it.\n", props.cf_name);
  }

  // read index block and decode each data block
  DEBUG("decode index block in file=%s\n", filename);
  s = ReadBlock(&index_block_rep.block, filename, &foot.index_handle,
                foot.checksum_type);
  if (!s.ok())
    goto free_foot_content;
  s = DecodeIndexBlock(&index_block_rep);
  DEBUG("total decode cout: data_block=%u, kv_put=%u, kv_del=%u\n",
        index_block_rep.data_block_count, index_block_rep.kv_put_count,
        index_block_rep.kv_del_count);

free_rep_restart_offset:
  if (index_block_rep.restart_offset)
    free(index_block_rep.restart_offset);
free_rep_block_data:
  if (index_block_rep.block.data())
    free((void *)index_block_rep.block.data());
free_foot_content:
  if (foot_content.data())
    free((void *)foot_content.data());
out:
  return s;
}

Status BuildSst(const std::string &dbname, const char *filename, Env *env,
                ColumnFamilyHandle *handle, Iterator *iter, uint64_t file_size,
                bool all_sync) {
  Status s;
  int number = 1;
  uint64_t creation_time = 0;
  uint64_t oldest_key_time = 0;
build_again:
  creation_time = iter->timestamp();
  std::string fname = TableFileName(dbname, number, filename);
  if (iter->Valid()) {
    WritableFile *file;
    s = env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      return s;
    }
    TableBuilder *builder =
        new TableBuilder(Options(), file, handle->GetName(), handle->GetID());
    for (; iter->Valid(); iter->Next()) {
      const Slice key = iter->key();
      const Slice value = iter->value();
      string result;
      AppendInternalKey(&result,
                        ParsedInternalKey(key, iter->timestamp(), kSstTypeValue));
      const Slice add_key(result);
      builder->Add(add_key, value);
      if (builder->CurFileSize() > file_size * number) {
        if (all_sync) {
          number++;
          oldest_key_time = iter->timestamp();
          s = builder->Finish(creation_time, oldest_key_time);
          if (s.ok()) {
            s = file->Sync();
          }
          if (s.ok()) {
            s = file->Close();
          }
          delete builder;
          file = NULL;
          goto build_again;
        } else {
          iter->Next();
          break;
        }
      }
    }
    oldest_key_time = iter->timestamp();
    s = builder->Finish(creation_time, oldest_key_time);
    if (s.ok()) {
      file_size = builder->FileSize();
    }
    delete builder;
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;
  }
  if (s.ok() && file_size > 0) {
    // keep it
  } else {
    env->DeleteFile(filename);
  }
  return s;
}

} // namespace shannon
