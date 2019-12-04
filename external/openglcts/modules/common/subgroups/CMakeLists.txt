include_directories(..)

set(DEQP_GL_SUBGROUPS_SRCS
  glcSubgroupsTests.cpp
  glcSubgroupsTests.hpp
  glcSubgroupsBuiltinVarTests.cpp
  glcSubgroupsBuiltinVarTests.hpp
  glcSubgroupsBuiltinMaskVarTests.cpp
  glcSubgroupsBuiltinMaskVarTests.hpp
  glcSubgroupsBasicTests.cpp
  glcSubgroupsBasicTests.hpp
  glcSubgroupsVoteTests.cpp
  glcSubgroupsVoteTests.hpp
  glcSubgroupsBallotTests.cpp
  glcSubgroupsBallotTests.hpp
  glcSubgroupsBallotBroadcastTests.cpp
  glcSubgroupsBallotBroadcastTests.hpp
  glcSubgroupsBallotOtherTests.cpp
  glcSubgroupsBallotOtherTests.hpp
  glcSubgroupsArithmeticTests.cpp
  glcSubgroupsArithmeticTests.hpp
  glcSubgroupsClusteredTests.cpp
  glcSubgroupsClusteredTests.hpp
  glcSubgroupsPartitionedTests.cpp
  glcSubgroupsPartitionedTests.hpp
  glcSubgroupsShuffleTests.cpp
  glcSubgroupsShuffleTests.hpp
  glcSubgroupsQuadTests.cpp
  glcSubgroupsQuadTests.hpp
  glcSubgroupsShapeTests.cpp
  glcSubgroupsShapeTests.hpp
  glcSubgroupsTestsUtils.cpp
  glcSubgroupsTestsUtils.hpp
  )

set(DEQP_GL_SUBGROUPS_LIBS
  glutil
  tcutil
  glcts-common
  )

PCH(DEQP_GL_SUBGROUPS_SRCS ../../pch.cpp)

add_library(deqp-gl-subgroups STATIC ${DEQP_GL_SUBGROUPS_SRCS})
target_link_libraries(deqp-gl-subgroups ${DEQP_GL_SUBGROUPS_LIBS})
