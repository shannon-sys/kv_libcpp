// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef SHANNON_COLUMN_FAMILY_H_
#define SHANNON_COLUMN_FAMILY_H_

#include <string>
#include <atomic>
#include "swift/shannon_db.h"
#include "venice_macro.h"

namespace shannon{
 class ColumnFamilyHandleImpl : public ColumnFamilyHandle {
  public:
    ColumnFamilyHandleImpl();
    ColumnFamilyHandleImpl(int db_index_, int cf_index_, std::string &name_);
    virtual uint32_t GetID() const override;
    virtual const std::string& GetName() const override;
    virtual Status GetDescriptor(ColumnFamilyDescriptor* desc) override;
    virtual Status SetDescriptor(const ColumnFamilyDescriptor& desc) override;
  private:
    int db_index;
    int cf_index;
    std::string name;
 };
};	//namespace shannon

#endif  // SHANNON_COLUMN_FAMILY_H_
