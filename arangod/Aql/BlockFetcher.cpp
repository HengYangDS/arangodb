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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcher.h"

using namespace arangodb::aql;

std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>> BlockFetcher::fetchBlock() {
  ExecutionState state;
  std::unique_ptr<AqlItemBlock> block;
  std::tie(state, block) = upstreamBlock().getSome(ExecutionBlock::DefaultBatchSize());
  if (block != nullptr) {
    auto shell = std::make_shared<InputAqlItemBlockShell>(itemBlockManager(),
                                                          std::move(block), _inputRegisters);
    return {state, shell};
  } else {
    return {state, nullptr};
  }
}

std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>> BlockFetcher::fetchBlockOfDependency(
    size_t dependencyIndex) {
  ExecutionState state;
  std::unique_ptr<AqlItemBlock> block;
  TRI_ASSERT(dependencyIndex < _dependencies.size());
  std::tie(state, block) =
      _dependencies[dependencyIndex]->getSome(ExecutionBlock::DefaultBatchSize());
  if (block != nullptr) {
    auto shell = std::make_shared<InputAqlItemBlockShell>(itemBlockManager(),
                                                          std::move(block), _inputRegisters);
    return {state, shell};
  } else {
    return {state, nullptr};
  }
}
