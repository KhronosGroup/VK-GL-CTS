
include_directories(
	..
	)

set(DEQP_VK_PIPELINE_SRCS
	vktPipelineBlendTests.cpp
	vktPipelineBlendTests.hpp
	vktPipelineClearUtil.cpp
	vktPipelineClearUtil.hpp
	vktPipelineDepthTests.cpp
	vktPipelineDepthTests.hpp
	vktPipelineImageSamplingInstance.cpp
	vktPipelineImageSamplingInstance.hpp
	vktPipelineImageTests.cpp
	vktPipelineImageTests.hpp
	vktPipelineImageUtil.cpp
	vktPipelineImageUtil.hpp
	vktPipelineImageViewTests.cpp
	vktPipelineImageViewTests.hpp
	vktPipelineMultisampleTests.cpp
	vktPipelineMultisampleTests.hpp
	vktPipelineReferenceRenderer.cpp
	vktPipelineReferenceRenderer.hpp
	vktPipelineSamplerTests.cpp
	vktPipelineSamplerTests.hpp
	vktPipelineStencilTests.cpp
	vktPipelineStencilTests.hpp
	vktPipelineTests.cpp
	vktPipelineTests.hpp
	vktPipelineVertexUtil.cpp
	vktPipelineVertexUtil.hpp
	)

set(DEQP_VK_PIPELINE_LIBS
	tcutil
	vkutil
	referencerenderer
	)

add_library(deqp-vk-pipeline STATIC ${DEQP_VK_PIPELINE_SRCS})
target_link_libraries(deqp-vk-pipeline ${DEQP_VK_PIPELINE_LIBS})

