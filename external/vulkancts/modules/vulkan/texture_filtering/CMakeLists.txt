include_directories(.. ../shaderexecutor)

set(DEQP_VK_TEXTURE_FILTERING_SRCS
  vktTextureFilteringTests.cpp
  vktTextureFilteringTests.hpp
  vktSampleVerifier.cpp
  vktSampleVerifier.hpp
  vktSampleVerifierUtil.cpp
  vktSampleVerifierUtil.hpp
  vktTextureFilteringExplicitLodTests.cpp
  vktTextureFilteringExplicitLodTests.hpp
)

set(DEQP_VK_TEXTURE_FILTERING_LIBS
  deqp-vk-common
  deqp-vk-shaderexecutor
  tcutil
  vkutil
)

add_library(deqp-vk-texture-filtering STATIC ${DEQP_VK_TEXTURE_FILTERING_SRCS})
target_link_libraries(deqp-vk-texture-filtering ${DEQP_VK_TEXTURE_FITLERING_LIBS})
