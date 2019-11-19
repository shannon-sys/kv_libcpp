// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef SHANNON_DB_INCLUDE_DB_H_
#define SHANNON_DB_INCLUDE_DB_H_

#include <stdint.h>
#include <stdio.h>
#include "swift/options.h"
#include "swift/write_batch.h"
#include "swift/read_batch.h"
#include "swift/status.h"
#include "swift/iterator.h"
#include "swift/env.h"
#include "swift/table.h"
#include "swift/comparator.h"
#include "swift/types.h"
#include "swift/transaction_log.h"
#include "swift/write_batch_with_index.h"

namespace shannon {

class Snapshot {
public:
  virtual SequenceNumber GetSequenceNumber() const = 0;
  virtual void SetSequenceNumber(SequenceNumber seq) = 0;
  virtual ~Snapshot() { }
};

struct Options;
struct DBOptions;

class DB {
 public:
  // Open the database with the specified "name".
  // Stores a pointer to a heap-allocated database in *dbptr and returns
  // OK on success.
  // Stores NULL in *dbptr and returns a non-OK status on error.
  // Caller should delete *dbptr when it is no longer needed.
  static Status Open(const Options& options,
                     const std::string& name,
                     const std::string& device,
                     DB** dbptr);

  // Open ColumnFamily Interface
  static Status Open(const DBOptions& db_options,
            const std::string& name, const std::string& device,
            const std::vector<ColumnFamilyDescriptor>& column_families,
            std::vector<ColumnFamilyHandle*>* handles, DB** dbptr);
  DB() { }
  virtual ~DB();

  // Set the database entry for "key" to "value".  Returns OK on success,
  // and a non-OK status on error.
  // Note: consider setting options.sync = true.
  virtual Status Put(const WriteOptions& options,
                     const Slice& key,
                     const Slice& value) = 0;

  // Remove the database entry (if any) for "key".  Returns OK on
  // success, and a non-OK status on error.  It is not an error if "key"
  // did not exist in the database.
  // Note: consider setting options.sync = true.
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

  // Apply the specified updates to the database.
  // Returns OK on success, non-OK on failure.
  // Note: consider setting options.sync = true.
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

  //
  virtual Status Read(const ReadOptions& options, ReadBatch* batch,
		  std::vector<std::string> *values) = 0;
  virtual Status WriteNonatomic(const WriteOptions& options,
                                WriteBatchNonatomic* updates) = 0;

  // If the database contains an entry for "key" store the
  // corresponding value in *value and return OK.
  //
  // If there is no entry for "key" leave *value unchanged and return
  // a status for which Status::IsNotFound() returns true.
  //
  // May return some other Status on an error.
  virtual Status Get(const ReadOptions& options,
                     const Slice& key, std::string* value) = 0;

  // on success returns kOk: exist, kNotFound: do not exist
  // on failure returns kIOError or kCorruption
  virtual Status KeyExist(const ReadOptions& options,
                          const Slice& key) = 0;

  // Analyze sst file and write kv to SSD of the same column family
  // verify: read block from sst file, then check it.
  //   verify != 0 crc32c or xxhash check; verify = 0 do not check.
  // handles: when you open database, you can get all handles
  //   get cf_name from sst file, then find the same name from handles.
  //   if there can not find cf_name, will ingest sst file fail.
  virtual Status IngestExternFile(char *sst_filename, int verify,
                   std::vector<ColumnFamilyHandle*>* handles) = 0;
  // Analyze sst file and write kv to SSD
  // kvs will be writen to the column family name of "default".
  virtual Status IngestExternFile(char *sst_filename, int verify) = 0;

  virtual Status BuildTable(const std::string& dirname, std::vector<ColumnFamilyHandle*> &handles, std::vector<Iterator*> &iterators) = 0;
  virtual Status BuildSstFile(const std::string& dirname, const std::string& filename, ColumnFamilyHandle *handle, Iterator *iter, uint64_t file_size) = 0;
  // Return a heap-allocated iterator over the contents of the database.
  // The result of NewIterator() is initially invalid (caller must
  // call one of the Seek methods on the iterator before using it).
  //
  // Caller should delete the iterator when it is no longer needed.
  // The returned iterator should be deleted before this db is deleted.
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;

  // Return a handle to the current DB state.  Iterators created with
  // this handle will all observe a stable snapshot of the current DB
  // state.  The caller must call ReleaseSnapshot(result) when the
  // snapshot is no longer needed.
  virtual const Snapshot* GetSnapshot() = 0;

  // Release a previously acquired snapshot.  The caller must not
  // use "snapshot" after this call.
  virtual Status ReleaseSnapshot(const Snapshot* snapshot) = 0;

  //ColumnFamily Interface
  virtual Status CreateColumnFamily(const ColumnFamilyOptions& options,
               const std::string& column_family_name,
               ColumnFamilyHandle** handle) = 0;

  virtual Status CreateColumnFamilies(const ColumnFamilyOptions& options,
               const std::vector<std::string>& column_family_names,
               std::vector<ColumnFamilyHandle*>* handles) = 0;

  virtual Status CreateColumnFamilies(
               const std::vector<ColumnFamilyDescriptor>& column_families,
               std::vector<ColumnFamilyHandle*>* handles) = 0;

  virtual Status DropColumnFamily(ColumnFamilyHandle* column_family) = 0;

  virtual Status DropColumnFamilies(
               const std::vector<ColumnFamilyHandle*>& column_families) = 0;

  virtual Status DestroyColumnFamilyHandle(ColumnFamilyHandle* column_family) = 0;

  virtual Status Put(const WriteOptions& options,
               ColumnFamilyHandle* column_family, const Slice& key,
               const Slice& value) = 0;

  virtual Status Delete(const WriteOptions& options,
               ColumnFamilyHandle* column_family,
               const Slice& key) = 0;

  virtual Status Get(const ReadOptions& options,
               ColumnFamilyHandle* column_family, const Slice& key,
               std::string *value) = 0;

  virtual Status KeyExist(const ReadOptions& options,
               ColumnFamilyHandle* column_family, const Slice& key) = 0;

  virtual Iterator* NewIterator(const ReadOptions& options,
               ColumnFamilyHandle* column_family) = 0;

  virtual Status NewIterators(const ReadOptions& options,
               const std::vector<ColumnFamilyHandle*>& column_families,
               std::vector<Iterator*>* iterators) = 0;

  virtual ColumnFamilyHandle* DefaultColumnFamily() const = 0;

  static Status ListColumnFamilies(const DBOptions& db_options,
            const std::string& name,
            const std::string& device,
            std::vector<std::string>* column_families);


  virtual Status CompactRange(const CompactRangeOptions& options,
                                ColumnFamilyHandle* column_family,
                                const Slice* begin, const Slice* end) = 0;
  virtual Status CompactRange(const CompactRangeOptions& options,
          const Slice* begin, const Slice* end) {
      return CompactRange(options, DefaultColumnFamily(), begin, end);
  }
  // DB implementations can export properties about their state via this method.
  // If "property" is a valid property understood by this DB implementation (see
  // Properties struct above for valid options), fills "*value" with its current
  // value and returns true. Otherwise, returns false.
  virtual bool GetProperty(ColumnFamilyHandle* column_family,
                            const Slice& property, std::string* value) = 0;

  virtual bool GetProperty(const Slice& property, std::string* value) {
      return GetProperty(DefaultColumnFamily(), property, value);
  }

  virtual SequenceNumber GetLatestSequenceNumber() const = 0;

  virtual Status DisableFileDeletions() = 0;

  virtual Status GetLiveFiles(std::vector<std::string>& vec,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true) = 0;

  virtual Status EnableFileDeletions(bool force = true) = 0;

  virtual Status GetSortedWalFiles(VectorLogPtr& files) = 0;

  virtual Options GetOptions(ColumnFamilyHandle* handle) const = 0;
  virtual Options GetOptions() {
      return GetOptions(DefaultColumnFamily());
  }
  virtual const std::string& GetName() const = 0;

  virtual const ColumnFamilyHandle* GetColumnFamilyHandle(
          int32_t column_family_id) const = 0;

  virtual int32_t GetIndex() const = 0;

  virtual Env* GetEnv() const = 0;

 private:
  DB(const DB&);
  void operator=(const DB&);
};


// Destroy the contents of the specified database.
// Be very careful using this method.
Status DestroyDB(const std::string& device, const std::string& name, const Options& options);

struct ColumnFamilyOptions;
extern const std::string kDefaultColumnFamilyName;
struct ColumnFamilyDescriptor {
  std::string name;
  ColumnFamilyOptions options;
  ColumnFamilyDescriptor()
    : name(kDefaultColumnFamilyName), options(ColumnFamilyOptions()) {}
  ColumnFamilyDescriptor(const std::string& _name,
                         const ColumnFamilyOptions& _options)
    : name(_name), options(_options) {}
};

class ColumnFamilyHandle {
  public:
    virtual ~ColumnFamilyHandle() {}
    virtual const std::string& GetName() const = 0;
    virtual uint32_t GetID() const = 0;
    virtual Status GetDescriptor(ColumnFamilyDescriptor* desc) = 0;
    virtual Status SetDescriptor(const ColumnFamilyDescriptor& desc)=0;
    ColumnFamilyDescriptor cf_descriptor_;
};

Status GetSequenceNumber(std::string& device, uint64_t *sequence);
Status SetSequenceNumber(std::string& device, uint64_t sequence);

}  // namespace shannon

#endif // SHANNON_DB_INCLUDE_DB_H_
