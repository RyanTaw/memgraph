#pragma once

#include <vector>

#include "distributed/coordination.hpp"
#include "data_structures/concurrent/concurrent_map.hpp"
#include "distributed/plan_rpc_messages.hpp"
#include "query/frontend/semantic/symbol_table.hpp"
#include "query/plan/operator.hpp"

namespace distributed {

/** Handles plan consumption from master. Creates and holds a local cache of
 * plans. Worker side. */
class PlanConsumer {
 public:
  struct PlanPack {
    PlanPack(std::shared_ptr<query::plan::LogicalOperator> plan,
             query::SymbolTable symbol_table, query::AstStorage storage)
        : plan(plan),
          symbol_table(std::move(symbol_table)),
          storage(std::move(storage)) {}

    std::shared_ptr<query::plan::LogicalOperator> plan;
    query::SymbolTable symbol_table;
    const query::AstStorage storage;
  };

  explicit PlanConsumer(distributed::Coordination *coordination);

  /** Return cached plan and symbol table for a given plan id. */
  PlanPack &PlanForId(int64_t plan_id) const;

  /** Return the ids of all the cached plans. For testing. */
  std::vector<int64_t> CachedPlanIds() const;

 private:
  // TODO remove unique_ptr. This is to get it to work, emplacing into a
  // ConcurrentMap is tricky.
  mutable ConcurrentMap<int64_t, std::unique_ptr<PlanPack>> plan_cache_;
};

}  // namespace distributed
