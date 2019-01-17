// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#include "swift/shannon_db.h"
#include "column_family.h"
#include <string>

namespace shannon {

  uint32_t ColumnFamilyHandleImpl::GetID() const {
    return cf_index;
  }

  const std::string& ColumnFamilyHandleImpl::GetName() const {
    return name;
  }

  Status ColumnFamilyHandleImpl::GetDescriptor(ColumnFamilyDescriptor* desc) {
    Status s;
    *desc = cf_descriptor_;
    return Status::OK();
  }

  Status ColumnFamilyHandleImpl::SetDescriptor(const  ColumnFamilyDescriptor& desc) {
    Status s;
    cf_descriptor_ = desc;
    return Status::OK();
  }

  ColumnFamilyHandleImpl::ColumnFamilyHandleImpl(int db_index_, int cf_index_,
          std::string &name_) : db_index(db_index_), cf_index(cf_index_), name(name_) {
  }

  ColumnFamilyHandleImpl::ColumnFamilyHandleImpl() {
  }

}  // namespace shannon
