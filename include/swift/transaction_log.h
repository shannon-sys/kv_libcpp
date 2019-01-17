// Copyright (c) 2018, Shannon, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License

#ifndef STORAGE_SHANNON_INCLUDE_TRANSACTION_LOG_ITERATOR_H_
#define STORAGE_SHANNON_INCLUDE_TRANSACTION_LOG_ITERATOR_H_

#include "swift/status.h"
#include "swift/types.h"
#include "swift/write_batch.h"
#include <memory>
#include <vector>

namespace shannon {

class LogFile;
typedef std::vector<std::unique_ptr<LogFile>> VectorLogPtr;

class LogFile {
};

} //  namespace shannon

#endif  // STORAGE_SHANNON_INCLUDE_TRANSACTION_LOG_ITERATOR_H_
