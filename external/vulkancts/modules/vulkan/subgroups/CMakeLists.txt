include_directories(..)

set(DEQP_VK_SUBGROUPS_SRCS
  vktSubgroupsTests.cpp
  vktSubgroupsTests.hpp
  vktSubgroupsBuiltinVarTests.cpp
  vktSubgroupsBuiltinVarTests.hpp
  vktSubgroupsBuiltinMaskVarTests.cpp
  vktSubgroupsBuiltinMaskVarTests.hpp
  vktSubgroupsBasicTests.cpp
  vktSubgroupsBasicTests.hpp
  vktSubgroupsVoteTests.cpp
  vktSubgroupsVoteTests.hpp
  vktSubgroupsBallotTests.cpp
  vktSubgroupsBallotTests.hpp
  vktSubgroupsBallotBroadcastTests.cpp
  vktSubgroupsBallotBroadcastTests.hpp
  vktSubgroupsBallotOtherTests.cpp
  vktSubgroupsBallotOtherTests.hpp
  vktSubgroupsArithmeticTests.cpp
  vktSubgroupsArithmeticTests.hpp
  vktSubgroupsClusteredTests.cpp
  vktSubgroupsClusteredTests.hpp
  vktSubgroupsPartitionedTests.cpp
  vktSubgroupsPartitionedTests.hpp
  vktSubgroupsShuffleTests.cpp
  vktSubgroupsShuffleTests.hpp
  vktSubgroupsQuadTests.cpp
  vktSubgroupsQuadTests.hpp
  vktSubgroupsShapeTests.cpp
  vktSubgroupsShapeTests.hpp
  vktSubgroupsTestsUtils.cpp
  vktSubgroupsTestsUtils.hpp
  vktSubgroupsBallotMasksTests.cpp
  vktSubgroupsBallotMasksTests.hpp
  )

set(DEQP_VK_SUBGROUPS_LIBS
  tcutil
  vkutil
  )

PCH(DEQP_VK_SUBGROUPS_SRCS ../pch.cpp)

add_library(deqp-vk-subgroups STATIC ${DEQP_VK_SUBGROUPS_SRCS})
target_link_libraries(deqp-vk-subgroups ${DEQP_VK_SUBGROUPS_LIBS})
