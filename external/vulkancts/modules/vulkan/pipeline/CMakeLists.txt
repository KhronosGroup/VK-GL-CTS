set(DEQP_VK_VKSC_PIPELINE_UTIL_SRCS
	vktPipelineImageUtil.cpp
	vktPipelineImageUtil.hpp
	)

set(DEQP_VK_VKSC_PIPELINE_SRCS
	vktPipelineBlendTests.cpp
	vktPipelineBlendTests.hpp
	vktPipelineClearUtil.cpp
	vktPipelineClearUtil.hpp
	vktPipelineColorWriteEnableTests.cpp
	vktPipelineColorWriteEnableTests.hpp
	vktPipelineCombinationsIterator.hpp
	vktPipelineDepthTests.cpp
	vktPipelineDepthTests.hpp
	vktPipelineDescriptorLimitsTests.cpp
	vktPipelineDescriptorLimitsTests.hpp
	vktPipelineDynamicOffsetTests.cpp
	vktPipelineDynamicOffsetTests.hpp
	vktPipelineDynamicVertexAttributeTests.cpp
	vktPipelineDynamicVertexAttributeTests.hpp
	vktPipelineEarlyDestroyTests.cpp
	vktPipelineEarlyDestroyTests.hpp
	vktPipelineImageSamplingInstance.cpp
	vktPipelineImageSamplingInstance.hpp
	vktPipelineImageTests.cpp
	vktPipelineImageTests.hpp
	vktPipelineLogicOpTests.cpp
	vktPipelineLogicOpTests.hpp
	vktPipelinePushConstantTests.cpp
	vktPipelinePushConstantTests.hpp
	#vktPipelinePushDescriptorTests.cpp
	#vktPipelinePushDescriptorTests.hpp
	vktPipelineSampleLocationsUtil.cpp
	vktPipelineSampleLocationsUtil.hpp
	vktPipelineSpecConstantTests.hpp
	vktPipelineSpecConstantTests.cpp
	vktPipelineSpecConstantUtil.hpp
	vktPipelineSpecConstantUtil.cpp
	${DEQP_VK_VKSC_PIPELINE_UTIL_SRCS}
	vktPipelineImageViewTests.cpp
	vktPipelineImageViewTests.hpp
	vktPipelineMatchedAttachmentsTests.cpp
	vktPipelineMatchedAttachmentsTests.hpp
	vktPipelineMultisampleTestsUtil.cpp
	vktPipelineMultisampleTestsUtil.hpp
	vktPipelineMultisampleTests.cpp
	vktPipelineMultisampleTests.hpp
	vktPipelineMultisampleBase.cpp
	vktPipelineMultisampleBase.hpp
	vktPipelineMultisampleBaseResolve.cpp
	vktPipelineMultisampleBaseResolve.hpp
	vktPipelineMultisampleBaseResolveAndPerSampleFetch.cpp
	vktPipelineMultisampleBaseResolveAndPerSampleFetch.hpp
	vktPipelineMultisampleInterpolationTests.cpp
	vktPipelineMultisampleInterpolationTests.hpp
	vktPipelineMultisampleShaderBuiltInTests.cpp
	vktPipelineMultisampleShaderBuiltInTests.hpp
	vktPipelineMultisampleImageTests.cpp
	vktPipelineMultisampleImageTests.hpp
	vktPipelineMultisampleMixedAttachmentSamplesTests.cpp
	vktPipelineMultisampleMixedAttachmentSamplesTests.hpp
	vktPipelineMultisampleResolveRenderAreaTests.cpp
	vktPipelineMultisampleResolveRenderAreaTests.hpp
	vktPipelineMultisampleSampleLocationsExtTests.cpp
	vktPipelineMultisampleSampleLocationsExtTests.hpp
	vktPipelineInputAssemblyTests.cpp
	vktPipelineInputAssemblyTests.hpp
	vktPipelineInterfaceMatchingTests.cpp
	vktPipelineInterfaceMatchingTests.hpp
	vktPipelineReferenceRenderer.cpp
	vktPipelineReferenceRenderer.hpp
	vktPipelineRenderToImageTests.cpp
	vktPipelineRenderToImageTests.hpp
	vktPipelineFramebufferAttachmentTests.cpp
	vktPipelineFramebufferAttachmentTests.hpp
	vktPipelineSamplerTests.cpp
	vktPipelineSamplerTests.hpp
	vktPipelineStencilExportTests.cpp
	vktPipelineStencilExportTests.hpp
	vktPipelineStencilTests.cpp
	vktPipelineStencilTests.hpp
	vktPipelineTests.cpp
	vktPipelineTests.hpp
	vktPipelineUniqueRandomIterator.hpp
	vktPipelineVertexInputTests.cpp
	vktPipelineVertexInputTests.hpp
	vktPipelineTimestampTests.cpp
	vktPipelineTimestampTests.hpp
	vktPipelineMiscTests.cpp
	vktPipelineMiscTests.hpp
	vktPipelineVertexUtil.cpp
	vktPipelineVertexUtil.hpp
	#vktPipelineCacheTests.cpp
	#vktPipelineCacheTests.hpp
	vktPipelineMakeUtil.cpp
	vktPipelineMakeUtil.hpp
	#vktPipelineCreationFeedbackTests.cpp
	#vktPipelineCreationFeedbackTests.hpp
	vktPipelineDepthRangeUnrestrictedTests.cpp
	vktPipelineDepthRangeUnrestrictedTests.hpp
	#vktPipelineExecutablePropertiesTests.cpp
	#vktPipelineExecutablePropertiesTests.hpp
	vktPipelineMaxVaryingsTests.cpp
	vktPipelineMaxVaryingsTests.hpp
	vktPipelineBlendOperationAdvancedTests.cpp
	vktPipelineBlendOperationAdvancedTests.hpp
	vktPipelineExtendedDynamicStateTests.cpp
	vktPipelineExtendedDynamicStateTests.hpp
	vktPipelineExtendedDynamicStateMiscTests.cpp
	vktPipelineExtendedDynamicStateMiscTests.hpp
	vktPipelineNoPositionTests.cpp
	vktPipelineNoPositionTests.hpp
	vktPipelineDynamicControlPoints.cpp
	vktPipelineDynamicControlPoints.hpp
	vktPipelineBindVertexBuffers2Tests.cpp
	vktPipelineBindVertexBuffers2Tests.hpp
	vktPipelineInputAttributeOffsetTests.cpp
	vktPipelineInputAttributeOffsetTests.hpp
	vktPipelineEmptyFSTests.cpp
	vktPipelineEmptyFSTests.hpp
	vktPipelineLegacyAttrTests.cpp
	vktPipelineLegacyAttrTests.hpp
	)

set(DEQP_VK_PIPELINE_SRCS
	vktPipelineBinaryTests.cpp
	vktPipelineBinaryTests.hpp
	vktPipelineBindPointTests.cpp
	vktPipelineBindPointTests.hpp
	vktPipelinePushDescriptorTests.cpp
	vktPipelinePushDescriptorTests.hpp
	vktPipelineCacheTests.cpp
	vktPipelineCacheTests.hpp
	vktPipelineCreationFeedbackTests.cpp
	vktPipelineCreationFeedbackTests.hpp
	vktPipelineDerivativeTests.cpp
	vktPipelineDerivativeTests.hpp
	vktPipelineExecutablePropertiesTests.cpp
	vktPipelineExecutablePropertiesTests.hpp
	vktPipelineCreationCacheControlTests.cpp
	vktPipelineCreationCacheControlTests.hpp
	vktPipelineSamplerBorderSwizzleTests.cpp
	vktPipelineSamplerBorderSwizzleTests.hpp
	vktPipelineLibraryTests.cpp
	vktPipelineLibraryTests.hpp
	vktPipelineShaderModuleIdentifierTests.cpp
	vktPipelineShaderModuleIdentifierTests.hpp
	vktPipelineImage2DViewOf3DTests.cpp
	vktPipelineImage2DViewOf3DTests.hpp
	vktPipelineAttachmentFeedbackLoopLayoutTests.cpp
	vktPipelineAttachmentFeedbackLoopLayoutTests.hpp
	vktPipelineMultisampledRenderToSingleSampledTests.cpp
	vktPipelineMultisampledRenderToSingleSampledTests.hpp
	vktPipelineImageSlicedViewOf3DTests.cpp
	vktPipelineImageSlicedViewOf3DTests.hpp
	vktPipelineMultisampleShaderFragmentMaskTests.cpp
	vktPipelineMultisampleShaderFragmentMaskTests.hpp
	vktPipelineRobustnessCacheTests.cpp
	vktPipelineRobustnessCacheTests.hpp
	vktPipelineNoQueuesTests.cpp
	vktPipelineNoQueuesTests.hpp
	)

#vktPipelinePushDescriptorTests.cpp				- missing VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
#vktPipelineCacheTests.cpp						- missing VkShaderModuleCreateInfo missing chache
#vktPipelineCreationFeedbackTests.cpp			- missing VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT
#vktPipelineExecutablePropertiesTests.cpp		- missing lots of things like VkPipelineExecutableStatisticKHR VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR

PCH(DEQP_VK_PIPELINE_UTIL_SRCS ../pch.cpp)
PCH(DEQP_VK_PIPELINE_SRCS ../pch.cpp)

add_library(deqp-vk-pipeline-util STATIC ${DEQP_VK_VKSC_PIPELINE_UTIL_SRCS})
target_link_libraries(deqp-vk-pipeline-util libamber referencerenderer tcutil vkutil)

add_library(deqp-vksc-pipeline-util STATIC ${DEQP_VK_VKSC_PIPELINE_UTIL_SRCS})
target_link_libraries(deqp-vksc-pipeline-util PUBLIC referencerenderer deqp-vksc-util tcutil vkscutil)

add_library(deqp-vk-pipeline STATIC ${DEQP_VK_VKSC_PIPELINE_SRCS} ${DEQP_VK_PIPELINE_SRCS})
target_link_libraries(deqp-vk-pipeline libamber referencerenderer tcutil vkutil)

add_library(deqp-vksc-pipeline STATIC ${DEQP_VK_VKSC_PIPELINE_SRCS})
target_link_libraries(deqp-vksc-pipeline PUBLIC referencerenderer deqp-vksc-util tcutil vkscutil)

target_include_directories(deqp-vk-pipeline
	PRIVATE
		..
		../../../../amber/src/include
		../amber
		../util
		${DEQP_INL_DIR}
)
target_include_directories(deqp-vksc-pipeline
	PRIVATE
		..
		../../../../amber/src/include
		../amber
		../util
)
