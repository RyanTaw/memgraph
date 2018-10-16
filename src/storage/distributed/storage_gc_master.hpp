/// @file
#pragma once

#include <mutex>

#include "distributed/coordination_master.hpp"
#include "distributed/storage_gc_rpc_messages.hpp"
#include "storage/distributed/storage_gc.hpp"

namespace database {
class StorageGcMaster : public StorageGc {
 public:
  using StorageGc::StorageGc;
  StorageGcMaster(Storage &storage, tx::Engine &tx_engine, int pause_sec,
                  distributed::MasterCoordination *coordination)
      : StorageGc(storage, tx_engine, pause_sec),
        coordination_(coordination) {
    coordination_->Register<distributed::RanLocalGcRpc>(
        [this](const auto &req_reader, auto *res_builder) {
          distributed::RanLocalGcReq req;
          Load(&req, req_reader);
          std::unique_lock<std::mutex> lock(worker_safe_transaction_mutex_);
          worker_safe_transaction_[req.worker_id] = req.local_oldest_active;
        });
  }

  ~StorageGcMaster() {
    // We have to stop scheduler before destroying this class because otherwise
    // a task might try to utilize methods in this class which might cause pure
    // virtual method called since they are not implemented for the base class.
    CHECK(!scheduler_.IsRunning())
        << "You must call Stop on database::StorageGcMaster!";
  }

  void Stop() {
    scheduler_.Stop();
  }

  void CollectCommitLogGarbage(tx::TransactionId oldest_active) final {
    // Workers are sending information when it's safe to delete every
    // transaction older than oldest_active from their perspective i.e. there
    // won't exist another transaction in the future with id larger than or
    // equal to oldest_active that might trigger a query into a commit log about
    // the state of transactions which we are deleting.
    auto safe_transaction = GetClogSafeTransaction(oldest_active);
    if (safe_transaction) {
      tx::TransactionId min_safe = *safe_transaction;
      {
        std::unique_lock<std::mutex> lock(worker_safe_transaction_mutex_);
        for (auto worker_id : coordination_->GetWorkerIds()) {
          // Skip itself
          if (worker_id == 0) continue;
          min_safe = std::min(min_safe, worker_safe_transaction_[worker_id]);
        }
      }
      // All workers reported back at least once
      if (min_safe > 0) {
        tx_engine_.GarbageCollectCommitLog(min_safe);
        LOG(INFO) << "Clearing master commit log with tx: " << min_safe;
      }
    }
  }

  distributed::MasterCoordination *coordination_;
  // Mapping of worker ids and oldest active transaction which is safe for
  // deletion from worker perspective
  std::unordered_map<int, tx::TransactionId> worker_safe_transaction_;
  std::mutex worker_safe_transaction_mutex_;
};
}  // namespace database
