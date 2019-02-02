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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "BlockFetcherHelper.h"
#include "MultiDependencyBlockFetcherMock.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockFetcher.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// TODO check that blocks are not returned to early (e.g. not before the next row
//      is fetched)
SCENARIO("MultiDependencySingleRowFetcher", "[AQL][EXECUTOR][FETCHER]") {
  ResourceMonitor monitor;
  ExecutionState state;
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  std::vector<ExecutionBlock*> dependencies;  // can be nullptrs

  GIVEN("there are no blocks upstream") {
    VPackBuilder input;
    dependencies.emplace_back(nullptr);
    MultiDependencyBlockFetcherMock blockFetcherMock{dependencies, monitor, 0};

    WHEN("the producer does not wait") {
      blockFetcherMock.shouldReturn(0, ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return DONE with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer waits") {
      blockFetcherMock.shouldReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return DONE with nullptr") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }
  }

  GIVEN("A single upstream block with a single row") {
    VPackBuilder input;
    ResourceMonitor monitor;
    dependencies.emplace_back(nullptr);
    MultiDependencyBlockFetcherMock blockFetcherMock{dependencies, monitor, 0};

    std::unique_ptr<AqlItemBlock> block = buildBlock<1>(&monitor, {{42}});

    WHEN("the producer returns DONE immediately") {
      blockFetcherMock.shouldReturn(0, ExecutionState::DONE, std::move(block));

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return the row with DONE") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == 42);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer returns HASMORE, then DONE with a nullptr") {
      blockFetcherMock
          .shouldReturn(0, ExecutionState::HASMORE, std::move(block))
          .andThenReturn(0, ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return the row with HASMORE") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == 42);

          AND_THEN("the fetcher shall return DONE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, then returns DONE") {
      blockFetcherMock.shouldReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::DONE, std::move(block));

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return the row with DONE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == 42);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, returns HASMORE, then DONE") {
      blockFetcherMock.shouldReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block))
          .andThenReturn(0, ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return the row with HASMORE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == 42);

            AND_THEN("the fetcher shall return DONE") {
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 3);
    }
  }

  // TODO the following tests should be simplified, a simple output
  // specification should be compared with the actual output.
  GIVEN("there are multiple blocks upstream") {
    ResourceMonitor monitor;
    dependencies.emplace_back(nullptr);
    MultiDependencyBlockFetcherMock blockFetcherMock{dependencies, monitor, 1};

    // three 1-column matrices with 3, 2 and 1 rows, respectively
    std::unique_ptr<AqlItemBlock> block1 =
                                      buildBlock<1>(&monitor, {{{1}}, {{2}}, {{3}}}),
                                  block2 = buildBlock<1>(&monitor, {{{4}}, {{5}}}),
                                  block3 = buildBlock<1>(&monitor, {{{6}}});

    WHEN("the producer does not wait") {
      blockFetcherMock
          .shouldReturn(0, ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(0, ExecutionState::DONE, std::move(block3));

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 3);
    }

    WHEN("the producer waits") {
      blockFetcherMock.shouldReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::DONE, std::move(block3));

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          size_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
              // wait at the beginning of the 1st and 2nd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(static_cast<size_t>(row.getValue(0).slice().getInt()) == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          // wait at the beginning of the 3rd block
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);
          // last row and DONE
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(static_cast<size_t>(row.getValue(0).slice().getInt()) == rowIdxAndValue);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 6);
    }

    WHEN("the producer waits and does not return DONE asap") {
      blockFetcherMock.shouldReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(0, ExecutionState::WAITING, nullptr)
          .andThenReturn(0, ExecutionState::HASMORE, std::move(block3))
          .andThenReturn(0, ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(blockFetcherMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          for (size_t rowIdxAndValue = 1; rowIdxAndValue <= 6; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4 || rowIdxAndValue == 6) {
              // wait at the beginning of the 1st, 2nd and 3rd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(static_cast<size_t>(row.getValue(0).slice().getInt()) == rowIdxAndValue);
          }
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 7);
    }
  }
}
}  // namespace aql
}  // namespace tests
}  // namespace arangodb