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

#include "ExecutionBlockImpl.h"

#include "Basics/Common.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

#include "Aql/CalculationExecutor.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/FilterExecutor.h"
#include "Aql/IdExecutor.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SortExecutor.h"

#include "Aql/SortRegister.h"

#include <type_traits>

using namespace arangodb;
using namespace arangodb::aql;

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 typename Executor::Infos&& infos)
    : ExecutionBlock(engine, node),
      _blockFetcher(_dependencies, _engine->itemBlockManager(),
                    infos.getInputRegisters(), infos.numberOfInputRegisters(),
                    createPassThroughCallback(*this)),
      _rowFetcher(_blockFetcher),
      _infos(std::move(infos)),
      _executor(_rowFetcher, _infos),
      _outputItemRow(nullptr),
      _passThroughBlocks() {}

template <class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() {
  if (_outputItemRow) {
    std::unique_ptr<AqlItemBlock> block = _outputItemRow->stealBlock();
    if (block != nullptr) {
      _engine->itemBlockManager().returnBlock(std::move(block));
    }
  }
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlockImpl<Executor>::getSomeWithoutTrace(size_t atMost) {
  // silence tests -- we need to introduce new failure tests for fetchers
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!_outputItemRow) {
    ExecutionState state;
    std::unique_ptr<OutputAqlItemBlockShell> newBlock;
    std::tie(state, newBlock) =
        requestWrappedBlock(atMost, _infos.numberOfOutputRegisters());
    if (state != ExecutionState::HASMORE) {
      TRI_ASSERT(newBlock == nullptr);
      return {state, nullptr};
    }
    TRI_ASSERT(newBlock != nullptr);
    _outputItemRow = std::make_unique<OutputAqlItemRow>(std::move(newBlock));
  }

  // TODO It's not very obvious that `state` will be initialized, because
  // it's not obvious that the loop will run at least once (e.g. after a
  // WAITING). It should, but I'd like that to be clearer. Initializing here
  // won't help much because it's unclear whether the value will be correct.
  ExecutionState state = ExecutionState::HASMORE;
  ExecutorStats executorStats{};

  TRI_ASSERT(atMost > 0);

  while (!_outputItemRow->isFull()) {
    std::tie(state, executorStats) = _executor.produceRow(*_outputItemRow);
    // Count global but executor-specific statistics, like number of filtered
    // rows.
    _engine->_stats += executorStats;
    if (_outputItemRow && _outputItemRow->produced()) {
      _outputItemRow->advanceRow();
    }

    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }

    if (state == ExecutionState::DONE) {
      // TODO Does this work as expected when there was no row produced, or
      // we were DONE already, so we didn't build a single row?
      // We must return nullptr then, because empty AqlItemBlocks are not
      // allowed!
      auto outputBlock = _outputItemRow->stealBlock();
      // This is not strictly necessary here, as we shouldn't be called again
      // after DONE.
      _outputItemRow.reset(nullptr);

      return {state, std::move(outputBlock)};
    }
  }

  TRI_ASSERT(state == ExecutionState::HASMORE);
  TRI_ASSERT(_outputItemRow->numRowsWritten() == atMost);

  auto outputBlock = _outputItemRow->stealBlock();
  // TODO OutputAqlItemRow could get "reset" and "isValid" methods and be reused
  _outputItemRow.reset(nullptr);

  return {state, std::move(outputBlock)};
}

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(size_t atMost) {
  // TODO IMPLEMENT ME, this is a stub!

  traceSkipSomeBegin(atMost);

  auto res = getSomeWithoutTrace(atMost);

  size_t skipped = 0;
  if (res.second != nullptr) {
    skipped = res.second->size();
    AqlItemBlock* resultPtr = res.second.get();
    returnBlock(resultPtr);
    res.second.release();
  }

  return traceSkipSomeEnd(res.first, skipped);
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<Executor>::traceGetSomeEnd(
    ExecutionState state, std::unique_ptr<AqlItemBlock> result) {
  ExecutionBlock::traceGetSomeEnd(result.get(), state);
  return {state, std::move(result)};
}

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::traceSkipSomeEnd(
    ExecutionState state, size_t skipped) {
  ExecutionBlock::traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

template <class Executor>
std::pair<ExecutionState, Result> ExecutionBlockImpl<Executor>::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // destroy and re-create the BlockFetcher
  _blockFetcher.~BlockFetcher();
  new (&_blockFetcher)
      BlockFetcher(_dependencies, _engine->itemBlockManager(),
                   _infos.getInputRegisters(), _infos.numberOfInputRegisters(),
                   createPassThroughCallback(*this));

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_blockFetcher);

  // destroy and re-create the Executor
  _executor.~Executor();
  new (&_executor) Executor(_rowFetcher, _infos);
  // // use this with c++17 instead of specialisation below
  // if constexpr (std::is_same_v<Executor, IdExecutor>) {
  //   if (items != nullptr) {
  //     _executor._inputRegisterValues.reset(
  //         items->slice(pos, *(_executor._infos.registersToKeep())));
  //   }
  // }

  return ExecutionBlock::initializeCursor(items, pos);
}

// TODO -- remove this when cpp 17 becomes available
template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<IdExecutor>::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // destroy and re-create the BlockFetcher
  _blockFetcher.~BlockFetcher();
  new (&_blockFetcher)
      BlockFetcher(_dependencies, _engine->itemBlockManager(),
                   _infos.getInputRegisters(), _infos.numberOfInputRegisters(),
                   createPassThroughCallback(*this));

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_blockFetcher);

  std::unique_ptr<AqlItemBlock> block;
  if (items != nullptr) {
    block = std::unique_ptr<AqlItemBlock>(
        items->slice(pos, *(_executor._infos.registersToKeep())));
  } else {
    block = std::unique_ptr<AqlItemBlock>(
        _engine->itemBlockManager().requestBlock(1, _executor._infos.numberOfOutputRegisters()));
  }
  auto shell = std::make_shared<AqlItemBlockShell>(_engine->itemBlockManager(),
                                                   std::move(block));
  InputAqlItemBlockShell inputShell(shell, _executor._infos.getInputRegisters());
  _rowFetcher.injectBlock(std::make_shared<InputAqlItemBlockShell>(std::move(inputShell)));

  // destroy and re-create the Executor
  _executor.~IdExecutor();
  new (&_executor) IdExecutor(_rowFetcher, _infos);

  return ExecutionBlock::initializeCursor(items, pos);
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<OutputAqlItemBlockShell>>
ExecutionBlockImpl<Executor>::requestWrappedBlock(size_t nrItems, RegisterId nrRegs) {
  AqlItemBlock* block = requestBlock(nrItems, nrRegs);

  std::shared_ptr<AqlItemBlockShell> blockShell;
  if (Executor::Properties::allowsBlockPassthrough) {
    // If blocks can be passed through, we do not create new blocks.
    // Instead, we take the input blocks from _passThroughBlocks which is pushed onto by
    // the BlockFetcher whenever it fetches an input block from upstream and reuse them.
    if (_passThroughBlocks.empty()) {
      ExecutionState state = _blockFetcher.prefetchBlock();
      if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
        return {state, nullptr};
      }
      TRI_ASSERT(state == ExecutionState::HASMORE);
    }
    // Now we must have a block.
    TRI_ASSERT(!_passThroughBlocks.empty());
    // Even more than that, there must be exactly one block here. As for all
    // actual Executor implementations there can never be more than one; and I
    // don't expect this to change for new Executors.
    TRI_ASSERT(_passThroughBlocks.size() == 1);
    blockShell = _passThroughBlocks.front();
    _passThroughBlocks.pop();
    // Assert that the block has enough registers. This must be guaranteed by
    // the register planning.
    TRI_ASSERT(blockShell->block().getNrRegs() == nrRegs);
  } else {
    blockShell =
        std::make_shared<AqlItemBlockShell>(_engine->itemBlockManager(),
                                            std::unique_ptr<AqlItemBlock>{block});
  }

  std::unique_ptr<OutputAqlItemBlockShell> outputBlockShell =
      std::make_unique<OutputAqlItemBlockShell>(blockShell, _infos.getOutputRegisters(),
                                                _infos.registersToKeep());
  return {ExecutionState::HASMORE, std::move(outputBlockShell)};
}

template <class Executor>
std::function<void(std::shared_ptr<AqlItemBlockShell>)>
ExecutionBlockImpl<Executor>::createPassThroughCallback(ExecutionBlockImpl& that) {
  if (Executor::Properties::allowsBlockPassthrough) {
    return [&that](std::shared_ptr<AqlItemBlockShell> shell) -> void {
      that.pushPassThroughBlock(shell);
    };
  } else {
    return nullptr;
  }
}

template class ::arangodb::aql::ExecutionBlockImpl<CalculationExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<EnumerateListExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<FilterExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<NoResultsExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ReturnExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortExecutor>;
