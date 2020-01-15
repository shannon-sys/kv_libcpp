// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "src/venice_kv.h"
#include "src/venice_ioctl.h"
#include "swift/shannon_db.h"
#include "src/kv_impl.h"

using namespace std;
using shannon::Status;
using shannon::Options;

namespace shannon {
DB::~DB() {
  KVImpl *impl = reinterpret_cast<KVImpl *>(this);
  impl->Close();
}

Status DB::Open(const Options& options, const std::string& dbname,
                const std::string& device, DB** dbptr) {
  *dbptr = NULL;
  DBOptions db_options(options);
  KVImpl* impl = new KVImpl(db_options, dbname, device);
  impl->SetCfOptoions(options);
  Status s = impl->Open();
  if (s.ok()) {
    *dbptr = impl;
  } else {
    delete impl;
  }
  return s;
}

Status DB::Open(const DBOptions& db_options,
       const std::string& dbname, const std::string& device,
       const std::vector<ColumnFamilyDescriptor>& column_family_descriptor,
       std::vector<ColumnFamilyHandle*>* handles, DB** dbptr) {
    *dbptr = NULL;
    KVImpl* impl;
    impl = new KVImpl(Options(), dbname, device);
    Status s = impl->Open(db_options, column_family_descriptor, handles);
    if (s.ok()) {
      *dbptr = impl;
    } else {
      delete impl;
    }
    return s;
}

Status DB::ListColumnFamilies(const DBOptions& db_options,
                const std::string& name,
                const std::string& device,
                std::vector<std::string>* column_families) {
    Status s;
    struct uapi_cf_list list;
    struct uapi_db_handle handle;
    int ret;
    int fd;

    if (name.length() >= DB_NAME_LEN) {
        return Status::InvalidArgument("database name is too longer.");
    }
    fd = open(device.data(), O_RDWR);
    if (fd < 0) {
        return Status::NotFound(strerror(errno));
    }
    /* open database */
    memset(&handle, 0, sizeof(handle));
    handle.flags = db_options.create_if_missing ? handle.flags | O_DB_CREATE : handle.flags;
    memcpy(handle.name, name.data(), name.length());
    ret = ioctl(fd, OPEN_DATABASE, &handle);
    if (ret < 0) {
        std::cout<<"ioctl open database failed!"<<std::endl;
        return Status::IOError(strerror(errno));
    }
    list.db_index = handle.db_index;
    ret = ioctl(fd, LIST_COLUMNFAMILY, &list);
    if (ret < 0) {
        std::cout<<"ioctl list columnfamily failed!"<<std::endl;
        return Status::IOError(strerror(errno));
    }
    for (int i = 0; i < list.cf_count; i ++) {
        column_families->push_back(std::string(list.cfs[i].name));
    }
    close(fd);
    return s;
}

Status GetSequenceNumber(std::string& device, uint64_t *sequence)
{
  Status s;
  int ret, fd;
  struct uapi_ts_get_option option;
  *sequence = 0xffffffffffffffff;

  fd = open(device.data(), O_RDWR);
  if (fd < 0) {
    return Status::NotFound(strerror(errno));
  }

  memset(&option, 0, sizeof(option));
  option.get_type = GET_DEV_CUR_TIMESTAMP;
  ret = ioctl(fd, IOCTL_GET_TIMESTAMP, &option);
  if (ret < 0) {
    std::cout<<"ioctl get sequence number failed!"<<std::endl;
    close(fd);
    return Status::IOError(strerror(errno));
  }
  *sequence = option.timestamp;
  close(fd);
  return s;
}

Status SetSequenceNumber(std::string& device, uint64_t sequence)
{
  Status s;
  int ret, fd;
  struct uapi_ts_set_option option;

  fd = open(device.data(), O_RDWR);
  if (fd < 0) {
    return Status::NotFound(strerror(errno));
  }

  memset(&option, 0, sizeof(option));
  option.timestamp = sequence;
  option.set_type = SET_DEV_CUR_TIMESTAMP;
  ret = ioctl(fd, IOCTL_SET_TIMESTAMP, &option);
  if (ret < 0) {
    std::cout<<"ioctl set sequence number failed!"<<std::endl;
    close(fd);
    return Status::IOError(strerror(errno));
  }
  close(fd);
  return s;
}

Status ListDatabase(const std::string& device, DatabaseList* db_list) {
  Status s;
  int ret, fd;
  struct uapi_db_list list;

  if (db_list == NULL) {
    return Status::InvalidArgument("db_list is null");
  }
  fd = open(device.data(), O_RDWR);
  if (fd < 0) {
    return Status::NotFound(strerror(errno));
  }
  memset(&list, 0, sizeof(list));
  list.list_all = 0;
  ret = ioctl(fd, LIST_DATABASE, &list);
  if (ret) {
    close(fd);
    return Status::IOError(strerror(errno));
  }

  for (int i = 0; i < list.count; i ++) {
    DatabaseInfo info;
    info.index = list.dbs[i].db_index;
    info.timestamp = list.dbs[i].timestamp;
    info.name.assign(list.dbs[i].name, strlen(list.dbs[i].name));
    db_list->push_back(info);
  }
  close(fd);
  return Status::OK();
}

}
