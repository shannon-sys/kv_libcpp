// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "table/block_based_table_factory.h"
#include <inttypes.h>
#include <stdint.h>
#include <memory>
#include <string>
#include "swift/cache.h"

namespace shannon {
  TableFactory* NewBlockBasedTableFactory(
          const BlockBasedTableOptions& _table_options) {
      return NULL;
  }

}  // namespace shannon
