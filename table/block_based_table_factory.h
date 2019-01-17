// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#ifndef BLOCK_BASED_TABLE_FACTORY_H_
#define BLOCK_BASED_TABLE_FACTORY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include "swift/table.h"

namespace shannon {

class BlockBasedTableFactory : public TableFactory {
};

}  // namespace shannon

#endif  // BLOCK_BASED_TABLE_FACTORY_H_
