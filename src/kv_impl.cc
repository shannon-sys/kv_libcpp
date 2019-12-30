// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <algorithm>
#include <sstream>
#include <assert.h>
#include "src/kv_impl.h"
#include "src/venice_kv.h"
#include "src/venice_ioctl.h"
#include "src/write_batch_internal.h"
#include "src/read_batch_internal.h"
#include "src/iter.h"
#include "src/snapshot.h"
#include "swift/env.h"
#include "swift/read_batch.h"
#include "table/sst_table.h"

using namespace std;
namespace shannon {
  const std::string kDefaultColumnFamilyName("default");
  KVImpl::~KVImpl() {
  }
  KVImpl::KVImpl(const DBOptions& options, const std::string& dbname, const std::string& device)
      :env_(options.env),
       options_(options),
       dbname_(dbname),
       device_(device) {
       default_cf_handle_ = NULL;
       is_default_open_ = false;
    }

  Status KVImpl::Open() {
    Status s;
    std::vector<ColumnFamilyDescriptor> column_family_descriptors;
    std::vector<ColumnFamilyHandle*> column_family_handles;
    column_family_descriptors.push_back(ColumnFamilyDescriptor(kDefaultColumnFamilyName,
                cf_options_));
    s = this->Open(options_, column_family_descriptors, &column_family_handles);
    if (s.ok()) {
        default_cf_handle_ = dynamic_cast<ColumnFamilyHandleImpl* >(column_family_handles[0]);
        is_default_open_ = true;
    }
    return s;
  }

  Status KVImpl::Open(const DBOptions& db_options,
                    const std::vector<ColumnFamilyDescriptor>& column_families,
                    std::vector<ColumnFamilyHandle*>* handles) {
    Status s;
    struct uapi_db_handle handle;
    struct uapi_cache_size cache;
    struct uapi_cf_list list;
    int ret;
    bool find_column_family_name = false;
    if (handles == NULL) {
        std::cout<<"invalid argument!"<<std::endl;
        return Status::InvalidArgument(strerror(errno));
    }
    /* check column_family name length  */
    for (auto descriptor : column_families) {
        if (descriptor.name.length() >= CF_NAME_LEN) {
            return Status::InvalidArgument(descriptor.name+" is too longer.");
        }
    }
    /* open database */
    fd_ = open(device_.data(), O_RDWR);
    if (fd_ < 0) {
        std::cout<<"open device failed!"<<std::endl;
        return Status::NotFound(device_.data(), strerror(errno));
    }
    memset(&handle, 0, sizeof(handle));
    handle.flags = db_options.create_if_missing ? handle.flags | O_DB_CREATE : handle.flags;
    memcpy(handle.name, dbname_.data(), DB_NAME_LEN);
    ret = ioctl(fd_, OPEN_DATABASE, &handle);
    if (ret < 0) {
        std::cout<<"ioctl open database failed!"<<std::endl;
        return Status::IOError(strerror(errno));
    }
    this->db_ = handle.db_index;
    /* get column_family name list */
    list.db_index = this->db_;
    ret = ioctl(fd_, LIST_COLUMNFAMILY, &list);
    if (ret < 0) {
        std::cout<<"ioctl list columnfamily failed!"<<std::endl;
        return Status::IOError(strerror(errno));
    }
    /* match column_family */
    for (int i = 0; i < list.cf_count; ++i) {
        find_column_family_name = false;
        for (int j = 0; j < column_families.size(); ++j) {
            if (strcmp(list.cfs[i].name, column_families[j].name.data()) == 0) {
                find_column_family_name = true;
                break;
            }
        }
        if (!find_column_family_name) {
            return Status::InvalidArgument
            ("Invalid Argument:You have to open all column families. not open:"+string(list.cfs[i].name));
        }
    }
    /* open column_family */
    struct uapi_cf_handle cfhandle;
    cfhandle.db_index = handle.db_index;
    for (auto column_family_descriptor : column_families) {
        memcpy(cfhandle.name, column_family_descriptor.name.data(),
                column_family_descriptor.name.length());
        cfhandle.name[column_family_descriptor.name.length()] = '\0';
        int ret = ioctl(fd_, OPEN_COLUMNFAMILY, &cfhandle);
        if (ret < 0) {
            std::cout<<"open columnfamily failed. cf_name:"<<cfhandle.name<<std::endl;
            /*
             * open column_family failed !
             * release the handle that has been opened !
             * */
            for (ColumnFamilyHandle *handle : *handles) {
                delete handle;
            }
            handles->clear();
            return Status::IOError(strerror(errno));
        }
        ColumnFamilyHandle *column_family_handle = new ColumnFamilyHandleImpl(
                cfhandle.db_index, cfhandle.cf_index, column_family_descriptor.name);
        handles->push_back(column_family_handle);
        /* save a copy in the db object */
        handles_.push_back(column_family_handle);
        /* set cache size */
        if (column_family_descriptor.options.cache_size > 0) {
            cache.db = this->db_;
            cache.cf_index = cfhandle.cf_index;
            cache.size = column_family_descriptor.options.cache_size;
            ret = ioctl(fd_, SET_CACHE, &cache);
            if (ret < 0) {
                std::cout<<"ioctl set cache failed!"<<std::endl;
            }
        }
    }
    /* set default handle */
    if (handles->size() > 0)
      default_cf_handle_ = dynamic_cast<ColumnFamilyHandleImpl*>((*handles)[0]);
    for (int i = 0; i < (*handles).size(); ++i) {
        (*handles)[i]->SetDescriptor(column_families[i]);
    }
    return s;
  }

  void KVImpl::Close() {
    if (fd_) {
      close(fd_);
      fd_ = 0;
    }
    if (is_default_open_ && default_cf_handle_) {
      delete default_cf_handle_;
      default_cf_handle_ = NULL;
    }
  }

  Status KVImpl::Delete(const WriteOptions& options, const Slice& key) {
    return this->Delete(options, default_cf_handle_, key);
  }

  Status KVImpl::Delete(const WriteOptions& options,
           ColumnFamilyHandle* column_family, const Slice& key) {
    Status s;
    struct venice_kv kv;
    int ret;

    if (column_family == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    memset(&kv, 0, sizeof(kv));
    kv.db = db_;
    kv.key = (char *)key.data();
    kv.key_len = key.size();
    kv.cf_index = ((const ColumnFamilyHandle* )column_family)->GetID();
    kv.sync = options.sync ? 1 : 0;
    kv.fill_cache = options.fill_cache ? 1 : 0;

    ret = ioctl(fd_, DEL_KV, &kv);
    if (ret < 0) {
        std::cout<<"ioctl del kv failed!"<<std::endl;
        return Status::NotFound(key.data());
    }
    return s;
  }

  Status KVImpl::Read(const ReadOptions& options, ReadBatch* my_batch,
		      std::vector<std::pair<shannon::Status, std::string>>* values) {
    Status s;
    unsigned int failed_cmd_count;
    if (my_batch == NULL || values == NULL) {
      return Status::InvalidArgument("batch or values not is null");
    }
    if (!ReadBatchInternal::Valid(my_batch)) {
      fprintf(stderr, "%s readbatch is not valid\n", __FUNCTION__);
      return Status::Corruption();
    }
    ReadBatchInternal::SetHandle(my_batch, db_);
    // set fill cache
    if (options.fill_cache) {
      ReadBatchInternal::SetFillCache(my_batch, 1);
    } else {
      ReadBatchInternal::SetFillCache(my_batch, 0);
    }
    // set snapshot
    ReadBatchInternal::SetSnapshot(my_batch, (options.snapshot != NULL
        ? options.snapshot->GetSequenceNumber() : 0));
    ReadBatchInternal::SetFailedCmdCount(my_batch, &failed_cmd_count);
    int ret = ioctl(fd_, READ_BATCH, ReadBatchInternal::Contents(my_batch).data());
    if (ret < 0) {
      return Status::IOError(strerror(errno));
    }
    const std::vector<char*> c_values = ReadBatchInternal::GetValues(my_batch);
    // get readbatch data
    ReadBatch read_batch;
    std::vector<int> reread_index;
    for (int i = 0; i < c_values.size(); i ++) {
      unsigned int return_status, value_len_addrs;
      int cmd_offset;
      char *cstr = c_values[i];
      char *v = cstr + sizeof(int) * 3;
      memcpy(&cmd_offset, cstr, sizeof(int));
      memcpy(&return_status, cstr + sizeof(int) , sizeof(int));
      memcpy(&value_len_addrs, cstr + sizeof(int) * 2, sizeof(int));
      struct readbatch_cmd *cmd = reinterpret_cast<struct readbatch_cmd*>(const_cast<char*>(my_batch->rep_.data() + cmd_offset));
      values->push_back(std::make_pair(shannon::Status::OK(), std::string("")));
      if (return_status == READBATCH_SUCCESS) {
        if (value_len_addrs > cmd->value_buf_size) {
          struct readbatch_cmd *cmd = reinterpret_cast<struct readbatch_cmd*>(const_cast<char*>(my_batch->rep_.data() + cmd_offset));
          read_batch.Get(cmd->cf_index, shannon::Slice(cmd->key, cmd->key_len), value_len_addrs);
          reread_index.push_back(i);
        } else {
          (*values)[i].second.assign(v, value_len_addrs);
        }
      } else if (return_status == READBATCH_NO_KEY) {
        (*values)[i].first = shannon::Status::NotFound();
      } else if (return_status == READBATCH_DATA_ERR) {
        (*values)[i].first = shannon::Status::Corruption("data error");
      } else if (return_status == READBATCH_VAL_BUF_ERR) {
        (*values)[i].first = shannon::Status::Corruption("value buffer error");
      }
    }
    // reread
    if (reread_index.size() > 0) {
      if (!ReadBatchInternal::Valid(&read_batch)) {
        fprintf(stderr, "%s readbatch is not valid\n", __FUNCTION__);
        return Status::Corruption();
      }
      ReadBatchInternal::SetHandle(&read_batch, db_);
      // set fill cache
      if (options.fill_cache) {
        ReadBatchInternal::SetFillCache(&read_batch, 1);
      } else {
        ReadBatchInternal::SetFillCache(&read_batch, 0);
      }
      // set snapshot
      ReadBatchInternal::SetSnapshot(&read_batch, (options.snapshot != NULL
          ? options.snapshot->GetSequenceNumber() : 0));
      ReadBatchInternal::SetFailedCmdCount(&read_batch, &failed_cmd_count);
      int ret = ioctl(fd_, READ_BATCH, ReadBatchInternal::Contents(&read_batch).data());
      if (ret < 0) {
        return Status::IOError(strerror(errno));
      }
      const std::vector<char*> n_c_values = ReadBatchInternal::GetValues(&read_batch);
      for (int i = 0; i < n_c_values.size(); i ++) {
        unsigned int return_status, value_len_addrs;
        int cmd_offset;
        char *cstr = n_c_values[i];
        char *v = cstr + sizeof(int) * 3;
        memcpy(&cmd_offset, cstr, sizeof(int));
        memcpy(&return_status, cstr + sizeof(int), sizeof(int));
        memcpy(&value_len_addrs, cstr + sizeof(int) * 2, sizeof(int));
        struct readbatch_cmd *cmd = reinterpret_cast<struct readbatch_cmd*>(const_cast<char*>(read_batch.rep_.data() + cmd_offset));
        assert(cmd->value_buf_size == value_len_addrs);
        if (return_status == READBATCH_SUCCESS) {
          (*values)[reread_index[i]].second.assign(v, value_len_addrs);
        } else if (return_status == READBATCH_NO_KEY) {
          (*values)[i].first = shannon::Status::NotFound();
        } else if (return_status == READBATCH_DATA_ERR) {
          (*values)[i].first = shannon::Status::Corruption("data error");
        } else if (return_status == READBATCH_VAL_BUF_ERR) {
          (*values)[i].first = shannon::Status::Corruption("value buffer error");
        }
      }
      read_batch.Clear();
    }
    return Status::OK();
  }

  Status KVImpl::Write(const WriteOptions& options, WriteBatch* my_batch) {
    if (my_batch == nullptr) {
      return Status::Corruption("Batch is nullptr!");
    }

    if (WriteBatchInternal::Count(my_batch) == 0) {
      return Status::OK();
    }

    if (!WriteBatchInternal::Valid(my_batch)) {
      fprintf(stderr, "%s writebatch is not valid\n", __FUNCTION__);
      return Status::Corruption();
    }

    WriteBatchInternal::SetHandle(my_batch, db_);
    if (options.fill_cache) {
      WriteBatchInternal::SetFillCache(my_batch, 1);
    } else {
      WriteBatchInternal::SetFillCache(my_batch, 0);
    }

    my_batch->SetOffset();
    int ret = ioctl(fd_, WRITE_BATCH, WriteBatchInternal::Contents(my_batch).data());
    if (ret < 0) {
      return Status::IOError(strerror(errno));
    }

    return Status::OK();
  }

  Status KVImpl::WriteNonatomic(const WriteOptions& options, WriteBatchNonatomic* my_batch) {
    if (my_batch == nullptr) {
      return Status::Corruption("Batch is nullptr!");
    }

    if (WriteBatchInternalNonatomic::Count(my_batch) == 0) {
      return Status::OK();
    }

    if (!WriteBatchInternalNonatomic::Valid(my_batch)) {
      fprintf(stderr, "%s writebatch is not valid\n", __FUNCTION__);
      return Status::Corruption();
    }
    WriteBatchInternalNonatomic::SetHandle(my_batch, db_);
    if (options.fill_cache) {
      WriteBatchInternalNonatomic::SetFillCache(my_batch, 1);
    }
    else {
      WriteBatchInternalNonatomic::SetFillCache(my_batch, 0);
    }
    my_batch->SetOffset();
    int ret = ioctl(fd_, WRITE_BATCH_NONATOMIC, WriteBatchInternalNonatomic::Contents(my_batch).data());
    if (ret < 0) {
      return Status::IOError(strerror(errno));
    }

    return Status::OK();
  }

  Status KVImpl::Put(const WriteOptions& options, const Slice& key, const Slice& value) {
    return this->Put(options, default_cf_handle_, key, value);
  }

  Status KVImpl::Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
                const Slice& key, const Slice& value) {
    Status s;
    struct venice_kv kv;

    if (column_family == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    memset(&kv, 0, sizeof(kv));
    kv.db = db_;
    kv.cf_index = (reinterpret_cast<const ColumnFamilyHandle* >(column_family))->GetID();
    kv.key = (char *)key.data();
    kv.key_len = key.size();
    kv.value = (char *)value.data();
    kv.value_len = value.size();
    kv.sync = options.sync ? 1 : 0;
    kv.fill_cache = options.fill_cache ? 1 : 0;
    int ret = ioctl(fd_, PUT_KV, &kv);
    if (ret < 0) {
        return Status::IOError(key.data());
    }
    return s;
  }

  Status KVImpl::Get(const ReadOptions& options, const Slice& key, std::string* value) {
    return this->Get(options, default_cf_handle_, key, value);
  }

  Status KVImpl::Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
                const Slice& key, std::string* value) {
    Status s;
    struct venice_kv kv;
    char *buf;

    buf = (char *)malloc(MAX_VALUE_SIZE);
    if (buf == NULL) {
        cerr << "malloc mem fail!" <<endl;
        return Status::IOError("malloc mem fail!\n");
    }
    kv.db = db_;
    kv.cf_index = (reinterpret_cast<const ColumnFamilyHandle* >(column_family))->GetID();
    kv.key = (char *)key.data();
    kv.key_len = key.size();
    kv.value = buf;
    kv.value_buf_size = MAX_VALUE_SIZE;
    kv.fill_cache = options.fill_cache ? 1 : 0;
    kv.snapshot_id = options.snapshot != NULL
        ? options.snapshot->GetSequenceNumber() : 0;
    int ret = ioctl(fd_, GET_KV, &kv);
    if (ret < 0) {
        free(buf);
        if (ENXIO == errno)
            return Status::NotFound(key.data());
        return Status::IOError(key.data());
    }
    value->assign(buf, kv.value_len);
    free(buf);
    return s;
  }

  Status KVImpl::KeyExist(const ReadOptions& options, const Slice& key) {
    return this->KeyExist(options, default_cf_handle_, key);
  }

  Status KVImpl::KeyExist(const ReadOptions& options,
                ColumnFamilyHandle* column_family, const Slice& key) {
    Status s;
    struct uapi_key_status status;

    if (key.size() > MAX_KEY_SIZE)
      return Status::Corruption("the length of key is invalid !!!");
    memset(&status, 0, sizeof(struct uapi_key_status));

    status.db_index = db_;
    status.cf_index =
        (reinterpret_cast<const ColumnFamilyHandle* >(column_family))->GetID();
    status.key = (char *)key.data();
    status.key_len = key.size();
    status.snapshot_id = options.snapshot != NULL
        ? options.snapshot->GetSequenceNumber() : 0;
    int ret = ioctl(fd_, IOCTL_KEY_STATUS, &status);
    if (ret < 0)
      return Status::IOError(key.data());
    if (status.exist == 0)
      return Status::NotFound(key.data());
    return s;
  }

  Status KVImpl::IngestExternFile(char *sst_filename, int verify,
                   std::vector<ColumnFamilyHandle*>* handles)
  {
    if (handles == NULL) {
      cerr << "handles is NULL!" <<endl;
      cerr << "If you do not want to get column family name from sst file," <<endl;
      cerr << "please use function: IngestExternFile(char *, int)!" <<endl;
      return Status::Corruption("handles is NULL!\n");
    }
    return AnalyzeSst(sst_filename, verify, this, handles);
  }

  Status KVImpl::IngestExternFile(char *sst_filename, int verify)
  {
    return AnalyzeSst(sst_filename, verify, this, NULL);
  }

  Status KVImpl::BuildTable(const std::string& dirname,
                          std::vector<ColumnFamilyHandle *> &handles,
                          std::vector<Iterator *> &iterators)
  {
    Status s;
    if ((handles.size() == 0) || (iterators.size() == 0)) {
      cerr << "handles  or iterators  size is 0!" << endl;
      return Status::Corruption("handles is NULL!\n");
    }
    if (handles.size() != iterators.size()) {
      cerr << "handles.size = " << handles.size()
		  << "iterators size = " << iterators.size() << "is err!" << endl;
      return Status::Corruption("size is err!\n");
    }
    if (!env_->FileExists(string(dirname))) {
      s = env_->CreateDir(dirname);
    }
    if (!s.ok()) {
      return s;
    }
    for (int i = 0; i <= handles.size() - 1; i++) {
		iterators[i]->SeekToFirst();
		s = BuildSst(dirname, handles[i]->GetName().data(), env_, handles[i],
					 iterators[i], options_.target_file_size_base, 1);
    }
    return s;
  }

Status KVImpl::BuildSstFile(const std::string &dirname, const std::string &filename,
                            ColumnFamilyHandle *handle, Iterator *iter, uint64_t file_size)
  {
    Status s;
    if ((handle == NULL) || (iter == NULL)) {
      cerr << "handles or iter is NULL!" << endl;
      return Status::Corruption("handles or iter is NULL!\n");
    }
    if (!iter->Valid()) {
      cerr << "iter is not valid!" << endl;
      return Status::Corruption("iter is not valid!\n");
    }
    if (!env_->FileExists(dirname)) {
      s = env_->CreateDir(dirname);
    }
    if (!s.ok()) {
      return s;
    }
    s = BuildSst(dirname, filename.data(), env_, handle, iter, file_size, 0);
    return s;
  }
  const Snapshot* KVImpl::GetSnapshot() {
    struct uapi_snapshot snap;
    int ret = 0;
    Status s;
    snap.db = db_;
    ret = ioctl(fd_, CREATE_SNAPSHOT, &snap);
    if (ret < 0) {
      status_ = Status::IOError("ioctl create_snapshot failed!!!\n");
      return NULL;
    }
    Snapshot* snapshot = new SnapshotImpl;
    snapshot->SetSequenceNumber(snap.snapshot_id);
    return snapshot;
  }

  Status KVImpl::ReleaseSnapshot(const Snapshot* snapshot) {
    struct uapi_snapshot snap;
    int ret = 0;
    Status s;
    snap.db = db_;
    snap.snapshot_id = snapshot->GetSequenceNumber();
    ret = ioctl(fd_, RELEASE_SNAPSHOT, &snap);
    if (ret < 0) {
      return Status::IOError("ioctl release_snapshot failed!!!\n");
    }
    delete snapshot;
    return s;
  }

  Iterator* KVImpl::NewIterator(const ReadOptions& options) {
      return this->NewIterator(options, default_cf_handle_);
  }

  Iterator* KVImpl::NewIterator(const ReadOptions& options,
                      ColumnFamilyHandle* column_family) {
    struct uapi_db_iterator *iter = NULL;
    int ret = 0;

    if (column_family == NULL) {
        status_ = Status::InvalidArgument(strerror(errno));
        return NULL;
    }
    iter = (struct uapi_db_iterator *)(new uint8_t[sizeof(struct uapi_db_iterator)+sizeof(struct uapi_cf_iterator)]);
    if (iter == NULL) {
        status_ = Status::InvalidArgument("malloc memory failed!");
        return NULL;
    }
    iter->db_index = this->db_;
    iter->timestamp = options.snapshot != NULL
        ? options.snapshot->GetSequenceNumber() : 0;
    iter->only_read_key = options.only_read_key ? 1 : 0;
    iter->iters[0].cf_index = column_family->GetID();
    iter->iters[0].db_index = this->db_;
    iter->iters[0].timestamp = iter->timestamp;
    iter->iters[0].only_read_key = iter->only_read_key;
    iter->count = 1;
    ret = ioctl(fd_, IOCTL_CREATE_ITERATOR, iter);
    if (ret < 0) {
        status_ = Status::IOError("ioctl create_iterator failed!!!\n");
        delete iter;
        return NULL;
    }

    Iterator* iterator = NewDBIterator(this, iter->iters[0].iter_index, iter->iters[0].cf_index, iter->timestamp);
    delete iter;
    return iterator;
  }

  Status KVImpl::NewIterators(const ReadOptions& options,
                 const std::vector<ColumnFamilyHandle*>& column_families,
                 std::vector<Iterator*>* iterators) {
    Status s;
    int ret;
    struct uapi_db_iterator *iter = NULL;

    if (iterators == NULL) {
        return Status::InvalidArgument("iterator is not null");
    }
    iter = (struct uapi_db_iterator *)malloc(
            sizeof(struct uapi_db_iterator) +
            sizeof(struct uapi_cf_iterator) * column_families.size());
    if (iter == NULL) {
        return Status::InvalidArgument("malloc failed!");
    }
    /* set parameter and create iterator */
    iter->timestamp = options.snapshot != NULL
        ? options.snapshot->GetSequenceNumber() : 0;
    iter->db_index = (unsigned int)this->db_;
    iter->count = column_families.size();
    iter->only_read_key = options.only_read_key ? 1 : 0;
    for (int i = 0; i < column_families.size(); i ++) {
        iter->iters[i].timestamp = iter->timestamp;
        iter->iters[i].db_index = (unsigned int)this->db_;
        iter->iters[i].cf_index = (reinterpret_cast<ColumnFamilyHandleImpl* >
                (column_families[i]))->GetID();
        iter->iters[i].only_read_key = iter->only_read_key;
    }
    ret = ioctl(fd_, IOCTL_CREATE_ITERATOR, iter);
    /* create iterator failed! */
    if (ret < 0) {
        free(iter);
        return Status::InvalidArgument("ioctl create iterator failed!");
    }
    /* generate Iterator object*/
    for (int i = 0; i < column_families.size(); i ++) {
        Iterator *iterator = NewDBIterator(this, iter->iters[i].iter_index,
                iter->iters[i].cf_index, iter->iters[i].timestamp);
        if (iterator == NULL) {
            for (auto iterator : *iterators) {
                delete iterator;
            }
            iterators->clear();
            return status_;
        }
        iterators->push_back(iterator);
    }
    return s;
  }

  Status KVImpl::CreateColumnFamily(const ColumnFamilyOptions& options,
                 const std::string& column_family_name,
                 ColumnFamilyHandle** handle) {
    Status s;
    struct uapi_cf_handle cfhandle;
    int ret;
    std::string default_name(column_family_name);

    if (handle == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    cfhandle.db_index = db_;
    memcpy(cfhandle.name, column_family_name.data(), column_family_name.size());
    cfhandle.name[column_family_name.size()] = '\0';
    ret = ioctl(fd_, CREATE_COLUMNFAMILY, &cfhandle);
    if (ret < 0) {
        return Status::IOError("ioctl column family failed!");
    }
    *handle = new ColumnFamilyHandleImpl(db_, cfhandle.cf_index, default_name);
    if (*handle == NULL) {
        return Status::IOError("malloc mem failed!\n");
    }
    return s;
  }

  Status KVImpl::CreateColumnFamilies(const ColumnFamilyOptions& options,
                 const std::vector<std::string>& column_family_names,
                 std::vector<ColumnFamilyHandle*>* handles) {
    Status s;

    if (handles == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    for (std::string column_family_name : column_family_names) {
        ColumnFamilyHandle* column_family_handle = NULL;
        s = this->CreateColumnFamily(options,
                column_family_name, &column_family_handle);
        if (s.ok()) {
            handles->push_back(column_family_handle);
        } else {
            return s;
        }
    }
    return s;
  }

  Status KVImpl::CreateColumnFamilies(const std::vector<ColumnFamilyDescriptor>& column_families,
                 std::vector<ColumnFamilyHandle*>* handles) {
    Status s;

    if (handles == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    for (ColumnFamilyDescriptor column_family_descriptor : column_families) {
        ColumnFamilyHandle* column_family_handle = NULL;
        s = this->CreateColumnFamily(column_family_descriptor.options,
                column_family_descriptor.name, &column_family_handle);
        if (s.ok()) {
            handles->push_back(column_family_handle);
        } else {
            return s;
        }
    }
    return s;
  }

  Status KVImpl::DropColumnFamily(ColumnFamilyHandle* column_family) {
    struct uapi_cf_handle cfhandle;
    Status s;
    int ret;
    std::string cf_name;
    if (column_family == NULL) {
        return Status::InvalidArgument(strerror(errno));
    }
    cfhandle.db_index = db_;
    cfhandle.cf_index = (reinterpret_cast<const ColumnFamilyHandle* >(column_family))
                        ->GetID();
    cf_name = (reinterpret_cast<const ColumnFamilyHandle* >(column_family))->GetName();
    memcpy(cfhandle.name, cf_name.data(), cf_name.length());
    cfhandle.name[cf_name.length()] = '\0';
    ret = ioctl(fd_, REMOVE_COLUMNFAMILY, &cfhandle);
    if (ret < 0) {
        return Status::IOError("remove columnfamily failed!");
    }
    return s;
  }

  Status KVImpl::DropColumnFamilies(const std::vector<ColumnFamilyHandle*>& column_families) {
    Status s;
    for (ColumnFamilyHandle* column_family_handle : column_families) {
        s = this->DropColumnFamily(column_family_handle);
        if (!s.ok()) {
            return s;
        }
    }
    return s;
  }

  Status KVImpl::DestroyColumnFamilyHandle(ColumnFamilyHandle* column_family_handle) {
    delete column_family_handle;
    return Status();
  }

  ColumnFamilyHandle* KVImpl::DefaultColumnFamily() const {

    return default_cf_handle_;
  }

  Env* KVImpl::GetEnv() const {
      return env_;
  }

Status KVImpl::CompactRange(const CompactRangeOptions& options,
                              ColumnFamilyHandle* column_family,
                              const Slice* begin, const Slice* end) {
    Status ss = Status::OK();
    if ( begin == NULL && end == NULL ) {
        ColumnFamilyDescriptor  cf_descripitor ;
        column_family->GetDescriptor(&cf_descripitor);
        vector<Iterator *> iters;
        vector <ColumnFamilyHandle*>  column_familys;
        column_familys.push_back(column_family);
        ss = this->NewIterators(ReadOptions(), column_familys, &iters);
        if (!ss.ok()) return ss;
        CompactionFilter::Context context ;
        context.is_full_compaction = true;
        context.is_manual_compaction = false;
        context.column_family_id = column_family->GetID();
        std::shared_ptr<CompactionFilterFactory>cfd;
        if (nullptr == &cf_descripitor ){
            cout <<"cf_descripitor null error ! "<<endl;
            return Status::NoSpace("no filter factory found");
        }
        else if (nullptr != cf_descripitor.options.compaction_filter_factory){
             cfd = cf_descripitor.options.compaction_filter_factory;
        }
        else {
            cout <<"compaction_filter_factory null error ! "<<endl;
            return Status::NoSpace("no filter factory found");
        }
         std::unique_ptr<CompactionFilter>cf_filter = cfd->CreateCompactionFilter(context);
         WriteBatch batch ;
         bool flag = false ;
        for (iters[0]->SeekToFirst();iters[0]->Valid();iters[0]->Next()){
            string x;
            bool changed;
            if ( cf_filter->Filter(0, iters[0]->key(), iters[0]->value(), &x, &changed) ) {
                if (changed) ss=Status::NotSupported("Not supported operation in this mode.");
                 flag = true;
                // cout <<"CompactRange delete key  :  "<< iters[0]->key().ToString()  <<"---------------------------"  <<  endl;
                batch.Delete(column_family,iters[0]->key());
            }
        }
        if (flag){
        ss= this->Write(WriteOptions(),&batch);
        if (!ss.ok())cout<<"WriteBatch error"<<endl;
        }
        else ss =Status::OK();
    }
    return ss;
  }

  bool KVImpl::GetProperty(ColumnFamilyHandle* column_family,
          const Slice& property, std::string* value) {
    if (column_family == NULL) {
      return false;
    }
    struct uapi_cf_status cf_status;
    int ret = -1;
    cf_status.db_index = db_;
    cf_status.cf_index = column_family->GetID();
    ret = ioctl(fd_, IOCTL_CF_STATUS, &cf_status);
    if (ret < 0) {
      return false;
    }
    if (property.compare("shannon.cur-size-all-mem-tables") == 0) {
      stringstream ss;
      ss<<cf_status.total_cache_size;
      std::string s = ss.str();
      if (value != NULL) {
        value->assign(s);
      }
    }
    return true;
  }

  SequenceNumber KVImpl::GetLatestSequenceNumber() const {
      return (SequenceNumber)0;
  }

  Status KVImpl::DisableFileDeletions() {
      return Status();
  }
  Status KVImpl::GetLiveFiles(std::vector<std::string>& vec,
                              uint64_t* manifest_file_size,
                              bool flush_memtable) {
      return Status();
  }

  Status KVImpl::EnableFileDeletions(bool force) {
      return Status();
  }

  Status KVImpl::GetSortedWalFiles(VectorLogPtr& files) {
      return Status();
  }

  Options KVImpl::GetOptions(ColumnFamilyHandle* handle) const {
      return Options();
  }

  const std::string& KVImpl::GetName() const {
      return dbname_;
  }

  const ColumnFamilyHandle* KVImpl::GetColumnFamilyHandle(
          int32_t column_family_id) const {
    for (auto handle : handles_) {
      if (handle && handle->GetID() == column_family_id) {
        return handle;
      }
    }
    return NULL;
  }

  int32_t KVImpl::GetIndex() const {
      return db_;
  }

  Status DestroyDB(const std::string& device, const std::string& dbname, const Options& options) {
    struct uapi_db_handle handle;
    int fd, ret;
    Status s;

    fd = open(device.data(), O_RDWR);
    if (fd < 0) {
      return Status::NotFound(device.data(), strerror(errno));
    }
    memset(&handle, 0, sizeof(handle));
    memcpy(handle.name, dbname.data(), DB_NAME_LEN);
    ret = ioctl(fd, OPEN_DATABASE, &handle);
    if (ret < 0) {
      return Status::IOError(dbname.data(), strerror(errno));
    }
    ret = ioctl(fd, REMOVE_DATABASE, &handle);
    if (ret < 0) {
       return Status::IOError(dbname.data(), strerror(errno));
    }
    close(fd);
    return s;
  }

};
