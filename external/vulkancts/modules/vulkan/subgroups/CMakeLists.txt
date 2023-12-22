include_directories(
  ..
  ${DEQP_INL_DIR}
  )

set(DEQP_VK_VKSC_SUBGROUPS_SRCS
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
  vktSubgroupsShuffleTests.cpp
  vktSubgroupsShuffleTests.hpp
  vktSubgroupsQuadTests.cpp
  vktSubgroupsQuadTests.hpp
  vktSubgroupsScanHelpers.cpp
  vktSubgroupsScanHelpers.hpp
  vktSubgroupsShapeTests.cpp
  vktSubgroupsShapeTests.hpp
  vktSubgroupsTestsUtils.cpp
  vktSubgroupsTestsUtils.hpp
  vktSubgroupsBallotMasksTests.cpp
  vktSubgroupsBallotMasksTests.hpp
  vktSubgroupsSizeControlTests.cpp
  vktSubgroupsSizeControlTests.hpp
  vktSubgroupsMultipleDispatchesUniformSubgroupSizeTests.cpp
  vktSubgroupsMultipleDispatchesUniformSubgroupSizeTests.hpp
  )

set(DEQP_VK_SUBGROUPS_SRCS
  vktSubgroupsPartitionedTests.cpp
  vktSubgroupsPartitionedTests.hpp
  vktSubgroupUniformControlFlowTests.cpp
  vktSubgroupUniformControlFlowTests.hpp
  )

PCH(DEQP_VK_SUBGROUPS_SRCS ../pch.cpp)
include_directories("../amber")
include_directories("../../../../amber/src/include")

add_library(deqp-vk-subgroups STATIC ${DEQP_VK_VKSC_SUBGROUPS_SRCS} ${DEQP_VK_SUBGROUPS_SRCS})
target_link_libraries(deqp-vk-subgroups tcutil vkutil)

add_library(deqp-vksc-subgroups STATIC ${DEQP_VK_VKSC_SUBGROUPS_SRCS})
target_link_libraries(deqp-vksc-subgroups PUBLIC deqp-vksc-util tcutil vkscutil)
