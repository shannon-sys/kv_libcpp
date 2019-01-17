// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef STORAGE_SHANNONDB_INCLUDE_UTIL_FILEOP_H_
#define STORAGE_SHANNONDB_INCLUDE_UTIL_FILEOP_H_

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "swift/slice.h"

namespace shannon {

extern void GetFilesize(char *filename, size_t *file_size);
extern size_t ReadFile(char *filename, uint64_t offset, uint64_t size, Slice *result);

} // namespace shannon

#endif // STORAGE_SHANNONDB_INCLUDE_UTIL_FILEOP_H_
