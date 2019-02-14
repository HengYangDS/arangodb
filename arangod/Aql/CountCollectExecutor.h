////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H

#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/LimitStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
template <bool>
class SingleRowFetcher;

class CountCollectExecutorInfos : public ExecutorInfos {
 public:
  CountCollectExecutorInfos(RegisterId collectRegister, RegisterId nrInputRegisters,
                            RegisterId nrOutputRegisters,
                            std::unordered_set<RegisterId> registersToClear);

  CountCollectExecutorInfos() = delete;
  CountCollectExecutorInfos(CountCollectExecutorInfos&&) = default;
  CountCollectExecutorInfos(CountCollectExecutorInfos const&) = delete;
  ~CountCollectExecutorInfos() = default;

 public:
  RegisterId getOutputRegisterId() const { return _collectRegister; }

 private:
  RegisterId _collectRegister;
};

/**
 * @brief Implementation of Count Collect Executor
 */

class CountCollectExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CountCollectExecutorInfos;
  using Stats = NoStats;

  CountCollectExecutor() = delete;
  CountCollectExecutor(CountCollectExecutor&&) = default;
  CountCollectExecutor(CountCollectExecutor const&) = delete;
  CountCollectExecutor(Fetcher& fetcher, Infos&);
  ~CountCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  void incrCount() noexcept { _count++; };
  size_t getCount() noexcept { return _count; };

 private:
  Infos const& infos() const noexcept { return _infos; };

 private:
  Infos const& _infos;
  Fetcher& _fetcher;

  size_t _count;
};

}  // namespace aql
}  // namespace arangodb

#endif
