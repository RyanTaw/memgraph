// Copyright 2023 Memgraph Ltd.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.txt; by using this file, you agree to be bound by the terms of the Business Source
// License, and you may not use this file except in compliance with the Business Source License.
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0, included in the file
// licenses/APL.txt.

#pragma once

#include "utils/exceptions.hpp"

#include <fmt/format.h>

namespace memgraph::query {

/**
 * @brief Base class of all query language related exceptions. All exceptions
 * derived from this one will be interpreted as ClientError-s, i. e. if client
 * executes same query again without making modifications to the database data,
 * query will fail again.
 */
class QueryException : public utils::BasicException {
  using utils::BasicException::BasicException;
};

class LexingException : public QueryException {
 public:
  using QueryException::QueryException;
  LexingException() : QueryException("") {}
};

class SyntaxException : public QueryException {
 public:
  using QueryException::QueryException;
  SyntaxException() : QueryException("") {}
};

// TODO: Figure out what information to put in exception.
// Error reporting is tricky since we get stripped query and position of error
// in original query is not same as position of error in stripped query. Most
// correct approach would be to do semantic analysis with original query even
// for already hashed queries, but that has obvious performance issues. Other
// approach would be to report some of the semantic errors in runtime of the
// query and only report line numbers of semantic errors (not position in the
// line) if multiple line strings are not allowed by grammar. We could also
// print whole line that contains error instead of specifying line number.
class SemanticException : public QueryException {
 public:
  using QueryException::QueryException;
  SemanticException() : QueryException("") {}
};

class UnboundVariableError : public SemanticException {
 public:
  explicit UnboundVariableError(const std::string &name) : SemanticException("Unbound variable: " + name + ".") {}
};

class RedeclareVariableError : public SemanticException {
 public:
  explicit RedeclareVariableError(const std::string &name) : SemanticException("Redeclaring variable: " + name + ".") {}
};

class TypeMismatchError : public SemanticException {
 public:
  TypeMismatchError(const std::string &name, const std::string &datum, const std::string &expected)
      : SemanticException(fmt::format("Type mismatch: {} already defined as {}, expected {}.", name, datum, expected)) {
  }
};

class UnprovidedParameterError : public QueryException {
 public:
  using QueryException::QueryException;
};

class ProfileInMulticommandTxException : public QueryException {
 public:
  using QueryException::QueryException;
  ProfileInMulticommandTxException() : QueryException("PROFILE not allowed in multicommand transactions.") {}
};

class IndexInMulticommandTxException : public QueryException {
 public:
  using QueryException::QueryException;
  IndexInMulticommandTxException() : QueryException("Index manipulation not allowed in multicommand transactions.") {}
};

class ConstraintInMulticommandTxException : public QueryException {
 public:
  using QueryException::QueryException;
  ConstraintInMulticommandTxException()
      : QueryException(
            "Constraint manipulation not allowed in multicommand "
            "transactions.") {}
};

class InfoInMulticommandTxException : public QueryException {
 public:
  using QueryException::QueryException;
  InfoInMulticommandTxException() : QueryException("Info reporting not allowed in multicommand transactions.") {}
};

/**
 * An exception for an illegal operation that can not be detected
 * before the query starts executing over data.
 */
class QueryRuntimeException : public QueryException {
 public:
  using QueryException::QueryException;
};

enum class AbortReason : uint8_t {
  NO_ABORT = 0,

  // transaction has been requested to terminate, ie. "TERMINATE TRANSACTIONS ..."
  TERMINATED = 1,

  // server is gracefully shutting down
  SHUTDOWN = 2,

  // the transaction timeout has been reached. Either via "--query-execution-timeout-sec", or a per-transaction timeout
  TIMEOUT = 3,
};

// This one is inherited from BasicException and will be treated as
// TransientError, i. e. client will be encouraged to retry execution because it
// could succeed if executed again.
class HintedAbortError : public utils::BasicException {
 public:
  using utils::BasicException::BasicException;
  explicit HintedAbortError(AbortReason reason) : utils::BasicException(AsMsg(reason)), reason_{reason} {}

  auto Reason() const -> AbortReason { return reason_; }

 private:
  static auto AsMsg(AbortReason reason) -> std::string_view {
    using namespace std::string_view_literals;
    switch (reason) {
      case AbortReason::TERMINATED:
        return "Transaction was asked to abort by another user."sv;
      case AbortReason::SHUTDOWN:
        return "Transaction was asked to abort because of database shutdown."sv;
      case AbortReason::TIMEOUT:
        return "Transaction was asked to abort because of transaction timeout."sv;
      default:
        // should never happen
        return "Transaction was asked to abort for an unknown reason."sv;
    }
  }
  AbortReason reason_;
};

class ExplicitTransactionUsageException : public QueryRuntimeException {
 public:
  using QueryRuntimeException::QueryRuntimeException;
};

class WriteVertexOperationInEdgeImportModeException : public QueryException {
 public:
  WriteVertexOperationInEdgeImportModeException()
      : QueryException("Write operations on vertices are forbidden while the edge import mode is active.") {}
};

class TransactionSerializationException : public QueryException {
 public:
  using QueryException::QueryException;
  TransactionSerializationException()
      : QueryException(
            "Cannot resolve conflicting transactions. You can retry this transaction when the conflicting transaction "
            "is finished") {}
};

class ReconstructionException : public QueryException {
 public:
  ReconstructionException()
      : QueryException(
            "Record invalid after WITH clause. Most likely deleted by a "
            "preceeding DELETE.") {}
};

class RemoveAttachedVertexException : public QueryRuntimeException {
 public:
  RemoveAttachedVertexException()
      : QueryRuntimeException(
            "Failed to remove node because of it's existing "
            "connections. Consider using DETACH DELETE.") {}
};

class UserModificationInMulticommandTxException : public QueryException {
 public:
  UserModificationInMulticommandTxException()
      : QueryException("Authentication clause not allowed in multicommand transactions.") {}
};

class InvalidArgumentsException : public QueryException {
 public:
  InvalidArgumentsException(const std::string &argument_name, const std::string &message)
      : QueryException(fmt::format("Invalid arguments sent: {} - {}", argument_name, message)) {}
};

class ReplicationModificationInMulticommandTxException : public QueryException {
 public:
  ReplicationModificationInMulticommandTxException()
      : QueryException("Replication clause not allowed in multicommand transactions.") {}
};

class ReplicationDisabledOnDiskStorage : public QueryException {
 public:
  ReplicationDisabledOnDiskStorage() : QueryException("Replication is not supported while in on-disk storage mode.") {}
};

class LockPathModificationInMulticommandTxException : public QueryException {
 public:
  LockPathModificationInMulticommandTxException()
      : QueryException("Lock path query not allowed in multicommand transactions.") {}
};

class LockPathDisabledOnDiskStorage : public QueryException {
 public:
  LockPathDisabledOnDiskStorage()
      : QueryException("Lock path disabled on disk storage since all data is already persisted. ") {}
};

class FreeMemoryModificationInMulticommandTxException : public QueryException {
 public:
  FreeMemoryModificationInMulticommandTxException()
      : QueryException("Free memory query not allowed in multicommand transactions.") {}
};

class FreeMemoryDisabledOnDiskStorage : public QueryException {
 public:
  FreeMemoryDisabledOnDiskStorage() : QueryException("Free memory does nothing when using disk storage. ") {}
};

class ShowConfigModificationInMulticommandTxException : public QueryException {
 public:
  ShowConfigModificationInMulticommandTxException()
      : QueryException("Show config query not allowed in multicommand transactions.") {}
};

class TriggerModificationInMulticommandTxException : public QueryException {
 public:
  TriggerModificationInMulticommandTxException()
      : QueryException("Trigger queries not allowed in multicommand transactions.") {}
};

class StreamQueryInMulticommandTxException : public QueryException {
 public:
  StreamQueryInMulticommandTxException()
      : QueryException("Stream queries are not allowed in multicommand transactions.") {}
};

class IsolationLevelModificationInMulticommandTxException : public QueryException {
 public:
  IsolationLevelModificationInMulticommandTxException()
      : QueryException("Isolation level cannot be modified in multicommand transactions.") {}
};

class IsolationLevelModificationInAnalyticsException : public QueryException {
 public:
  IsolationLevelModificationInAnalyticsException()
      : QueryException(
            "Isolation level cannot be modified when storage mode is set to IN_MEMORY_ANALYTICAL."
            "IN_MEMORY_ANALYTICAL mode doesn't provide any isolation guarantees, "
            "you can think about it as an equivalent to READ_UNCOMMITED.") {}
};

class StorageModeModificationInMulticommandTxException : public QueryException {
 public:
  StorageModeModificationInMulticommandTxException()
      : QueryException("Storage mode cannot be modified in multicommand transactions.") {}
};

class EdgeImportModeModificationInMulticommandTxException : public QueryException {
 public:
  EdgeImportModeModificationInMulticommandTxException()
      : QueryException("Edge import mode cannot be modified in multicommand transactions.") {}
};

class CreateSnapshotInMulticommandTxException final : public QueryException {
 public:
  CreateSnapshotInMulticommandTxException()
      : QueryException("Snapshot cannot be created in multicommand transactions.") {}
};

class CreateSnapshotDisabledOnDiskStorage final : public QueryException {
 public:
  CreateSnapshotDisabledOnDiskStorage() : QueryException("In the on-disk storage mode data is already persistent.") {}
};

class EdgeImportModeQueryDisabledOnDiskStorage final : public QueryException {
 public:
  EdgeImportModeQueryDisabledOnDiskStorage()
      : QueryException("Edge import mode is only allowed for on-disk storage mode.") {}
};

class SettingConfigInMulticommandTxException final : public QueryException {
 public:
  SettingConfigInMulticommandTxException()
      : QueryException("Settings cannot be changed or fetched in multicommand transactions.") {}
};

class VersionInfoInMulticommandTxException : public QueryException {
 public:
  VersionInfoInMulticommandTxException()
      : QueryException("Version info query not allowed in multicommand transactions.") {}
};

class AnalyzeGraphInMulticommandTxException : public QueryException {
 public:
  AnalyzeGraphInMulticommandTxException()
      : QueryException("Analyze graph query not allowed in multicommand transactions.") {}
};

class ReplicationException : public utils::BasicException {
 public:
  using utils::BasicException::BasicException;
  explicit ReplicationException(const std::string &message)
      : utils::BasicException("Replication Exception: {} Check the status of the replicas using 'SHOW REPLICAS' query.",
                              message) {}
};

class TransactionQueueInMulticommandTxException : public QueryException {
 public:
  TransactionQueueInMulticommandTxException()
      : QueryException("Transaction queue queries not allowed in multicommand transactions.") {}
};

class IndexPersistenceException : public QueryException {
 public:
  IndexPersistenceException() : QueryException("Persisting index on disk failed.") {}
};

class ConstraintsPersistenceException : public QueryException {
 public:
  ConstraintsPersistenceException() : QueryException("Persisting constraints on disk failed.") {}
};

class MultiDatabaseQueryInMulticommandTxException : public QueryException {
 public:
  MultiDatabaseQueryInMulticommandTxException()
      : QueryException("Multi-database queries are not allowed in multicommand transactions.") {}
};

}  // namespace memgraph::query
