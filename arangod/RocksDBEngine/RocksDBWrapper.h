////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// based upon stackable_db.h and transaction_db.h from Facebook rocksdb
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "rocksdb/utilities/transaction_db.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
///
/// @brief The restore portion of RocksDBHotBackup needs the ability to pause
///  API calls to rocksdb while closing, replacing, and reopening rocksdb.  This
///  class creates that capability by wrapping the rocksdbTransactionDB class.
///
////////////////////////////////////////////////////////////////////////////////

class RocksDBWrapper : public rocksdb::TransactionDB {
 public:
  RocksDBWrapper() = delete;
  RocksDBWrapper(const RocksDBWrapper &) = delete;
//  RocksDBWrapper & operator(const RocksDBWrapper &) = delete;

  RocksDBWrapper(const rocksdb::DBOptions& db_options,
                 const rocksdb::TransactionDBOptions& txn_db_options,
                 const std::string& dbname,
                 const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                 std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                 rocksdb::TransactionDB * trans);

  ~RocksDBWrapper() {
/// use DestroyColumnFamilyHandle to close handles prior to db close/delete
#if 0
    if (shared_db_ptr_ == nullptr) {
      delete _db;
    } else {
      assert(shared_db_ptr_.get() == _db);
    }
    _db = nullptr;
#endif
  }

  static rocksdb::Status Open(const rocksdb::Options& options,
                              const rocksdb::TransactionDBOptions& txn_db_options,
                     const std::string& dbname, RocksDBWrapper** dbptr);

  static rocksdb::Status Open(const rocksdb::DBOptions& db_options,
                              const rocksdb::TransactionDBOptions& txn_db_options,
                              const std::string& dbname,
                              const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                              std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                              RocksDBWrapper** dbptr);

  virtual rocksdb::Status Close() override { return _db->Close(); }

  virtual rocksdb::Transaction* BeginTransaction(
    const rocksdb::WriteOptions& write_options,
    const rocksdb::TransactionOptions& txn_options = rocksdb::TransactionOptions(),
    rocksdb::Transaction* old_txn = nullptr) {
    return _db->BeginTransaction(write_options, txn_options, old_txn);
  }

  virtual rocksdb::Transaction* GetTransactionByName(const rocksdb::TransactionName& name) {
    return _db->GetTransactionByName(name);
  }

  virtual void GetAllPreparedTransactions(std::vector<rocksdb::Transaction*>* trans) {
    return _db->GetAllPreparedTransactions(trans);
  }

  // Returns set of all locks held.
  //
  // The mapping is column family id -> KeyLockInfo
  virtual std::unordered_multimap<uint32_t, rocksdb::KeyLockInfo> GetLockStatusData() {
    return _db->GetLockStatusData();
  }

  virtual std::vector<rocksdb::DeadlockPath> GetDeadlockInfoBuffer() {
    return _db->GetDeadlockInfoBuffer();
  }

  virtual void SetDeadlockInfoBufferSize(uint32_t target_size) {
    return _db->SetDeadlockInfoBufferSize(target_size);
  }

  virtual rocksdb::Status CreateColumnFamily(const rocksdb::ColumnFamilyOptions& options,
                                    const std::string& column_family_name,
                                    rocksdb::ColumnFamilyHandle** handle) override {
    return _db->CreateColumnFamily(options, column_family_name, handle);
  }

  virtual rocksdb::Status CreateColumnFamilies(
      const rocksdb::ColumnFamilyOptions& options,
      const std::vector<std::string>& column_family_names,
      std::vector<rocksdb::ColumnFamilyHandle*>* handles) override {
    return _db->CreateColumnFamilies(options, column_family_names, handles);
  }

  virtual rocksdb::Status CreateColumnFamilies(
      const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
      std::vector<rocksdb::ColumnFamilyHandle*>* handles) override {
    return _db->CreateColumnFamilies(column_families, handles);
  }

  virtual rocksdb::Status DropColumnFamily(rocksdb::ColumnFamilyHandle* column_family) override {
    return _db->DropColumnFamily(column_family);
  }

  virtual rocksdb::Status DropColumnFamilies(
      const std::vector<rocksdb::ColumnFamilyHandle*>& column_families) override {
    return _db->DropColumnFamilies(column_families);
  }

  virtual rocksdb::Status DestroyColumnFamilyHandle(
      rocksdb::ColumnFamilyHandle* column_family) override {
    return _db->DestroyColumnFamilyHandle(column_family);
  }

  using DB::Put;
  virtual rocksdb::Status Put(const rocksdb::WriteOptions& options,
                     rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Slice& key,
                     const rocksdb::Slice& val) override {
    return _db->Put(options, column_family, key, val);
  }

  using DB::Get;
  virtual rocksdb::Status Get(const rocksdb::ReadOptions& options,
                     rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Slice& key,
                     rocksdb::PinnableSlice* value) override {
    return _db->Get(options, column_family, key, value);
  }

  using DB::MultiGet;
  virtual std::vector<rocksdb::Status> MultiGet(
      const rocksdb::ReadOptions& options,
      const std::vector<rocksdb::ColumnFamilyHandle*>& column_family,
      const std::vector<rocksdb::Slice>& keys,
      std::vector<std::string>* values) override {
    return _db->MultiGet(options, column_family, keys, values);
  }

  using DB::IngestExternalFile;
  virtual rocksdb::Status IngestExternalFile(
      rocksdb::ColumnFamilyHandle* column_family,
      const std::vector<std::string>& external_files,
      const rocksdb::IngestExternalFileOptions& options) override {
    return _db->IngestExternalFile(column_family, external_files, options);
  }

  virtual rocksdb::Status VerifyChecksum() override { return _db->VerifyChecksum(); }

  using DB::KeyMayExist;
  virtual bool KeyMayExist(const rocksdb::ReadOptions& options,
                           rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Slice& key,
                           std::string* value,
                           bool* value_found = nullptr) override {
    return _db->KeyMayExist(options, column_family, key, value, value_found);
  }

  using DB::Delete;
  virtual rocksdb::Status Delete(const rocksdb::WriteOptions& wopts,
                        rocksdb::ColumnFamilyHandle* column_family,
                        const rocksdb::Slice& key) override {
    return _db->Delete(wopts, column_family, key);
  }

  using DB::SingleDelete;
  virtual rocksdb::Status SingleDelete(const rocksdb::WriteOptions& wopts,
                              rocksdb::ColumnFamilyHandle* column_family,
                              const rocksdb::Slice& key) override {
    return _db->SingleDelete(wopts, column_family, key);
  }

  using DB::Merge;
  virtual rocksdb::Status Merge(const rocksdb::WriteOptions& options,
                       rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Slice& key,
                       const rocksdb::Slice& value) override {
    return _db->Merge(options, column_family, key, value);
  }


  virtual rocksdb::Status Write(const rocksdb::WriteOptions& opts, rocksdb::WriteBatch* updates)
    override {
      return _db->Write(opts, updates);
  }

  using DB::NewIterator;
  virtual rocksdb::Iterator* NewIterator(const rocksdb::ReadOptions& opts,
                                rocksdb::ColumnFamilyHandle* column_family) override {
    return _db->NewIterator(opts, column_family);
  }

  virtual rocksdb::Status NewIterators(
      const rocksdb::ReadOptions& options,
      const std::vector<rocksdb::ColumnFamilyHandle*>& column_families,
      std::vector<rocksdb::Iterator*>* iterators) override {
    return _db->NewIterators(options, column_families, iterators);
  }


  virtual const rocksdb::Snapshot* GetSnapshot() override {
    return _db->GetSnapshot();
  }

  virtual void ReleaseSnapshot(const rocksdb::Snapshot* snapshot) override {
    return _db->ReleaseSnapshot(snapshot);
  }

  using DB::GetMapProperty;
  using DB::GetProperty;
  virtual bool GetProperty(rocksdb::ColumnFamilyHandle* column_family,
                           const rocksdb::Slice& property, std::string* value) override {
    return _db->GetProperty(column_family, property, value);
  }
  virtual bool GetMapProperty(
      rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Slice& property,
      std::map<std::string, std::string>* value) override {
    return _db->GetMapProperty(column_family, property, value);
  }

  using DB::GetIntProperty;
  virtual bool GetIntProperty(rocksdb::ColumnFamilyHandle* column_family,
                              const rocksdb::Slice& property, uint64_t* value) override {
    return _db->GetIntProperty(column_family, property, value);
  }

  using DB::GetAggregatedIntProperty;
  virtual bool GetAggregatedIntProperty(const rocksdb::Slice& property,
                                        uint64_t* value) override {
    return _db->GetAggregatedIntProperty(property, value);
  }

  using DB::GetApproximateSizes;
  virtual void GetApproximateSizes(rocksdb::ColumnFamilyHandle* column_family,
                                   const rocksdb::Range* r, int n, uint64_t* sizes,
                                   uint8_t include_flags
                                   = INCLUDE_FILES) override {
    return _db->GetApproximateSizes(column_family, r, n, sizes,
                                    include_flags);
  }

  using DB::GetApproximateMemTableStats;
  virtual void GetApproximateMemTableStats(rocksdb::ColumnFamilyHandle* column_family,
                                           const rocksdb::Range& range,
                                           uint64_t* const count,
                                           uint64_t* const size) override {
    return _db->GetApproximateMemTableStats(column_family, range, count, size);
  }

  using DB::CompactRange;
  virtual rocksdb::Status CompactRange(const rocksdb::CompactRangeOptions& options,
                              rocksdb::ColumnFamilyHandle* column_family,
                              const rocksdb::Slice* begin, const rocksdb::Slice* end) override {
    return _db->CompactRange(options, column_family, begin, end);
  }

  using DB::CompactFiles;
  virtual rocksdb::Status CompactFiles(
      const rocksdb::CompactionOptions& compact_options,
      rocksdb::ColumnFamilyHandle* column_family,
      const std::vector<std::string>& input_file_names,
      const int output_level, const int output_path_id = -1,
      std::vector<std::string>* const output_file_names = nullptr) override {
    return _db->CompactFiles(
        compact_options, column_family, input_file_names,
        output_level, output_path_id, output_file_names);
  }

  virtual rocksdb::Status PauseBackgroundWork() override {
    return _db->PauseBackgroundWork();
  }
  virtual rocksdb::Status ContinueBackgroundWork() override {
    return _db->ContinueBackgroundWork();
  }

  virtual rocksdb::Status EnableAutoCompaction(
      const std::vector<rocksdb::ColumnFamilyHandle*>& column_family_handles) override {
    return _db->EnableAutoCompaction(column_family_handles);
  }

  using DB::NumberLevels;
  virtual int NumberLevels(rocksdb::ColumnFamilyHandle* column_family) override {
    return _db->NumberLevels(column_family);
  }

  using DB::MaxMemCompactionLevel;
  virtual int MaxMemCompactionLevel(rocksdb::ColumnFamilyHandle* column_family)
      override {
    return _db->MaxMemCompactionLevel(column_family);
  }

  using DB::Level0StopWriteTrigger;
  virtual int Level0StopWriteTrigger(rocksdb::ColumnFamilyHandle* column_family)
      override {
    return _db->Level0StopWriteTrigger(column_family);
  }

  virtual const std::string& GetName() const override {
    return _db->GetName();
  }

  virtual rocksdb::Env* GetEnv() const override {
    return _db->GetEnv();
  }

  using DB::GetOptions;
  virtual rocksdb::Options GetOptions(rocksdb::ColumnFamilyHandle* column_family) const override {
    return _db->GetOptions(column_family);
  }

  using DB::GetDBOptions;
  virtual rocksdb::DBOptions GetDBOptions() const override {
    return _db->GetDBOptions();
  }

  using DB::Flush;
  virtual rocksdb::Status Flush(const rocksdb::FlushOptions& fopts,
                       rocksdb::ColumnFamilyHandle* column_family) override {
    return _db->Flush(fopts, column_family);
  }

  virtual rocksdb::Status SyncWAL() override {
    return _db->SyncWAL();
  }

  virtual rocksdb::Status FlushWAL(bool sync) override { return _db->FlushWAL(sync); }

#ifndef ROCKSDB_LITE

  virtual rocksdb::Status DisableFileDeletions() override {
    return _db->DisableFileDeletions();
  }

  virtual rocksdb::Status EnableFileDeletions(bool force) override {
    return _db->EnableFileDeletions(force);
  }

  virtual void GetLiveFilesMetaData(
      std::vector<rocksdb::LiveFileMetaData>* metadata) override {
    _db->GetLiveFilesMetaData(metadata);
  }

  virtual void GetColumnFamilyMetaData(
      rocksdb::ColumnFamilyHandle *column_family,
      rocksdb::ColumnFamilyMetaData* cf_meta) override {
    _db->GetColumnFamilyMetaData(column_family, cf_meta);
  }

#endif  // ROCKSDB_LITE

  virtual rocksdb::Status GetLiveFiles(std::vector<std::string>& vec, uint64_t* mfs,
                              bool flush_memtable = true) override {
      return _db->GetLiveFiles(vec, mfs, flush_memtable);
  }

  virtual rocksdb::SequenceNumber GetLatestSequenceNumber() const override {
    return _db->GetLatestSequenceNumber();
  }

  virtual bool SetPreserveDeletesSequenceNumber(rocksdb::SequenceNumber seqnum) override {
    return _db->SetPreserveDeletesSequenceNumber(seqnum);
  }

  virtual rocksdb::Status GetSortedWalFiles(rocksdb::VectorLogPtr& files) override {
    return _db->GetSortedWalFiles(files);
  }

  virtual rocksdb::Status DeleteFile(std::string name) override {
    return _db->DeleteFile(name);
  }

  virtual rocksdb::Status GetDbIdentity(std::string& identity) const override {
    return _db->GetDbIdentity(identity);
  }

  using DB::SetOptions;
  virtual rocksdb::Status SetOptions(rocksdb::ColumnFamilyHandle* column_family_handle,
                            const std::unordered_map<std::string, std::string>&
                                new_options) override {
    return _db->SetOptions(column_family_handle, new_options);
  }

  virtual rocksdb::Status SetDBOptions(
      const std::unordered_map<std::string, std::string>& new_options)
      override {
    return _db->SetDBOptions(new_options);
  }

  using DB::ResetStats;
  virtual rocksdb::Status ResetStats() override { return _db->ResetStats(); }

  using DB::GetPropertiesOfAllTables;
  virtual rocksdb::Status GetPropertiesOfAllTables(
      rocksdb::ColumnFamilyHandle* column_family,
      rocksdb::TablePropertiesCollection* props) override {
    return _db->GetPropertiesOfAllTables(column_family, props);
  }

  using DB::GetPropertiesOfTablesInRange;
  virtual rocksdb::Status GetPropertiesOfTablesInRange(
      rocksdb::ColumnFamilyHandle* column_family, const rocksdb::Range* range, std::size_t n,
      rocksdb::TablePropertiesCollection* props) override {
    return _db->GetPropertiesOfTablesInRange(column_family, range, n, props);
  }

  virtual rocksdb::Status GetUpdatesSince(
      rocksdb::SequenceNumber seq_number, std::unique_ptr<rocksdb::TransactionLogIterator>* iter,
      const rocksdb::TransactionLogIterator::ReadOptions& read_options) override {
    return _db->GetUpdatesSince(seq_number, iter, read_options);
  }

  virtual rocksdb::Status SuggestCompactRange(rocksdb::ColumnFamilyHandle* column_family,
                                     const rocksdb::Slice* begin,
                                     const rocksdb::Slice* end) override {
    return _db->SuggestCompactRange(column_family, begin, end);
  }

  virtual rocksdb::Status PromoteL0(rocksdb::ColumnFamilyHandle* column_family,
                           int target_level) override {
    return _db->PromoteL0(column_family, target_level);
  }

  virtual rocksdb::ColumnFamilyHandle* DefaultColumnFamily() const override {
    return _db->DefaultColumnFamily();
  }

 protected:
  /// copies of the Open parameters
  const rocksdb::DBOptions _db_options;
  const rocksdb::TransactionDBOptions _txn_db_options;
  const std::string _dbname;
  const std::vector<rocksdb::ColumnFamilyDescriptor> _column_families;
  const std::vector<rocksdb::ColumnFamilyHandle*>* _handlesPtr; // special case, this is an output pointer


  TransactionDB * _db;
//  std::shared_ptr<DB> shared_db_ptr_;
};

} //  namespace rocksdb
