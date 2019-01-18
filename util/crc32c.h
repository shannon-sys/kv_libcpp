// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#ifndef STORAGE_SHANNONDB_INCLUDE_UTIL_CRC32C_H_
#define STORAGE_SHANNONDB_INCLUDE_UTIL_CRC32C_H_

#include <stddef.h>
#include <stdint.h>

namespace shannon {
namespace crc32c {

// Return the crc32c of concat(A, data[0,n-1]) where init_crc is the
// crc32c of some string A.  Extend() is often used to maintain the
// crc32c of a stream of data.
extern uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

// Return the crc32c of data[0,n-1]
inline uint32_t Value(const char* data, size_t n) {
  return Extend(0, data, n);
}

static const uint32_t kMaskDelta = 0xa282ead8ul;

inline uint32_t Mask(uint32_t crc) {
  return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

inline uint32_t Unmask(uint32_t masked_crc) {
  uint32_t rot = masked_crc - kMaskDelta;
  return ((rot >> 17) | (rot << 15));
}

}  // namespace crc32c
}  // namespace shannon

#endif  // STORAGE_SHANNONDB_INCLUDE_UTIL_CRC32C_H_
