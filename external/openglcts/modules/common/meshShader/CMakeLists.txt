include_directories(..)


set(DEQP_GL_MESHSHADER_SRCS
  glcMeshShaderTests.cpp
  glcMeshShaderTests.hpp
  glcMeshShaderSmokeTests.cpp
  glcMeshShaderSmokeTests.hpp
  glcMeshShaderPropertyTests.cpp
  glcMeshShaderPropertyTests.hpp
  glcMeshShaderApiTests.cpp
  glcMeshShaderApiTests.hpp
  glcMeshShaderTestsUtils.cpp
  glcMeshShaderTestsUtils.hpp
  )

set(DEQP_GL_MESHSHADER_LIBS
  glutil
  tcutil
  glcts-common
  )

PCH(DEQP_GL_MESHSHADER_SRCS ../../pch.cpp)

add_library(deqp-gl-meshShader STATIC ${DEQP_GL_MESHSHADER_SRCS})
target_link_libraries(deqp-gl-meshShader ${DEQP_GL_MESHSHADER_LIBS})
