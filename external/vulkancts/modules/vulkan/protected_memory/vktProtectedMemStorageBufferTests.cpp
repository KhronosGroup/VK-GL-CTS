/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Protected memory storage buffer tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemStorageBufferTests.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuStringTemplate.hpp"

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

#include "vktProtectedMemBufferValidator.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemContext.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

enum {
	RENDER_HEIGHT	= 128,
	RENDER_WIDTH	= 128,
};

enum {
	RANDOM_TEST_COUNT	= 10,
};

enum SSBOTestType {
	SSBO_READ,
	SSBO_WRITE,
	SSBO_ATOMIC
};

enum SSBOAtomicType {
	ATOMIC_ADD,
	ATOMIC_MIN,
	ATOMIC_MAX,
	ATOMIC_AND,
	ATOMIC_OR,
	ATOMIC_XOR,
	ATOMIC_EXCHANGE,
	ATOMIC_COMPSWAP
};


const char* getSSBOTestDescription (SSBOTestType type)
{
	switch (type) {
		case SSBO_READ:		return "Test for read storage buffer on protected memory.";
		case SSBO_WRITE:	return "Test for write storage buffer on protected memory.";
		case SSBO_ATOMIC:	return "Test for atomic storage buffer on protected memory.";
		default: DE_FATAL("Invalid SSBO test type"); return "";
	}
}

const char* getSSBOTypeString (SSBOTestType type)
{
	switch (type) {
		case SSBO_READ:		return "read";
		case SSBO_WRITE:	return "write";
		case SSBO_ATOMIC:	return "atomic";
		default: DE_FATAL("Invalid SSBO test type"); return "";
	}
}

const char* getSSBOAtomicTypeString (SSBOAtomicType type)
{
	switch (type)
	{
		case ATOMIC_ADD:		return "add";
		case ATOMIC_MIN:		return "min";
		case ATOMIC_MAX:		return "max";
		case ATOMIC_AND:		return "and";
		case ATOMIC_OR:			return "or";
		case ATOMIC_XOR:		return "xor";
		case ATOMIC_EXCHANGE:	return "exchange";
		case ATOMIC_COMPSWAP:	return "compswap";
		default: DE_FATAL("Invalid SSBO atomic operation type"); return "";
	}
}

void static addBufferCopyCmd (const vk::DeviceInterface&	vk,
							  vk::VkCommandBuffer			cmdBuffer,
							  deUint32						queueFamilyIndex,
							  vk::VkBuffer					srcBuffer,
							  vk::VkBuffer					dstBuffer,
							  deUint32						copySize)
{
	const vk::VkBufferMemoryBarrier		dstWriteStartBarrier	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0,													// VkAccessFlags		srcAccessMask
		vk::VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
		queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
		dstBuffer,											// VkBuffer				buffer
		0u,													// VkDeviceSize			offset
		VK_WHOLE_SIZE,										// VkDeviceSize			size
	};

	vk.cmdPipelineBarrier(cmdBuffer,
						  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,	// srcStageMask
						  vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,	// dstStageMask
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  1, &dstWriteStartBarrier,
						  0, (const vk::VkImageMemoryBarrier*)DE_NULL);

	const vk::VkBufferCopy				copyRegion				=
	{
		0,					// VkDeviceSize	srcOffset
		0,					// VkDeviceSize	dstOffset
		copySize			// VkDeviceSize	size
	};
	vk.cmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	const vk::VkBufferMemoryBarrier		dstWriteEndBarrier		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		vk::VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags		srcAccessMask
		vk::VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags		dstAccessMask
		queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
		queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
		dstBuffer,											// VkBuffer				buffer
		0u,													// VkDeviceSize			offset
		VK_WHOLE_SIZE,										// VkDeviceSize			size
	};
	vk.cmdPipelineBarrier(cmdBuffer,
						  vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,		// srcStageMask
						  vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,	// dstStageMask
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  1, &dstWriteEndBarrier,
						  0, (const vk::VkImageMemoryBarrier*)DE_NULL);

}

template<typename T>
class StorageBufferTestInstance : public ProtectedTestInstance
{
public:
								StorageBufferTestInstance	(Context&					ctx,
															 const SSBOTestType			testType,
															 const glu::ShaderType		shaderType,
															 const tcu::UVec4			testInput,
															 const BufferValidator<T>&	validator);
	virtual tcu::TestStatus		iterate						(void);

private:
	tcu::TestStatus				executeFragmentTest			(void);
	tcu::TestStatus				executeComputeTest			(void);

	const SSBOTestType			m_testType;
	const glu::ShaderType		m_shaderType;
	const tcu::UVec4			m_testInput;
	const BufferValidator<T>&	m_validator;
	const vk::VkFormat			m_imageFormat;
};

template<typename T>
class StorageBufferTestCase : public TestCase
{
public:
								StorageBufferTestCase	(tcu::TestContext&			testctx,
														 const SSBOTestType			testType,
														 const glu::ShaderType		shaderType,
														 const char*				name,
														 const tcu::UVec4			testInput,
														 ValidationDataStorage<T>	validationData,
														 const std::string&			extraShader = "")
									: TestCase		(testctx, name, getSSBOTestDescription(testType))
									, m_testType	(testType)
									, m_shaderType	(shaderType)
									, m_testInput	(testInput)
									, m_validator	(validationData)
									, m_extraShader	(extraShader)
								{
								}
	virtual TestInstance*		createInstance			(Context& ctx) const
								{
									return new StorageBufferTestInstance<T>(ctx, m_testType, m_shaderType, m_testInput, m_validator);
								}
	virtual void				initPrograms			(vk::SourceCollections& programCollection) const;
	virtual void				checkSupport			(Context& context) const
								{
									checkProtectedQueueSupport(context);
								}

	virtual						~StorageBufferTestCase	(void) {}

private:
	const SSBOTestType			m_testType;
	const glu::ShaderType		m_shaderType;
	const tcu::UVec4			m_testInput;
	const BufferValidator<T>	m_validator;
	const std::string			m_extraShader;
};

template<typename T>
StorageBufferTestInstance<T>::StorageBufferTestInstance	 (Context&					ctx,
														  const SSBOTestType		testType,
														  const glu::ShaderType		shaderType,
														  const tcu::UVec4			testInput,
														  const BufferValidator<T>&	validator)
	: ProtectedTestInstance	(ctx)
	, m_testType			(testType)
	, m_shaderType			(shaderType)
	, m_testInput			(testInput)
	, m_validator			(validator)
	, m_imageFormat			(vk::VK_FORMAT_R8G8B8A8_UNORM)
{
}

template<typename T>
void StorageBufferTestCase<T>::initPrograms (vk::SourceCollections& programCollection) const
{
	const char* vertexShader =
		"#version 450\n"
		"layout(location=0) out vec4 vIndex;\n"
		"void main() {\n"
		"    vec2 pos[4] = vec2[4]( vec2(-0.7, 0.7), vec2(0.7, 0.7), vec2(0.0, -0.7), vec2(-0.7, -0.7) );\n"
		"    vIndex = vec4(gl_VertexIndex);\n"
		"    gl_PointSize = 1.0;\n"
		"    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);\n"
		"}";

	//  set = 0, location = 0 -> buffer ProtectedTestBuffer (uvec4)
	//  set = 0, location = 2 -> buffer ProtectedTestBufferSource (uvec4)
	const char* readShaderTemplateStr =
		"#version 450\n"
		"${INPUT_DECLARATION}\n"
		"\n"
		"layout(set=0, binding=0, std140) buffer ProtectedTestBuffer\n"
		"{\n"
		"    highp uvec4 protectedTestResultBuffer;\n"
		"};\n"
		"\n"
		"layout(set=0, binding=2, std140) buffer ProtectedTestBufferSource\n"
		"{\n"
		"    highp uvec4 protectedTestBufferSource;\n"
		"};\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    protectedTestResultBuffer = protectedTestBufferSource;\n"
		"    ${FRAGMENT_OUTPUT}\n"
		"}\n";

	//  set = 0, location = 0 -> buffer ProtectedTestBuffer (uvec4)
	//  set = 0, location = 1 -> uniform Data (uvec4)
	const char* writeShaderTemplateStr =
		"#version 450\n"
		"${INPUT_DECLARATION}\n"
		"\n"
		"layout(set=0, binding=0, std140) buffer ProtectedTestBuffer\n"
		"{\n"
		"    highp uvec4 protectedTestResultBuffer;\n"
		"};\n"
		"\n"
		"layout(set=0, binding=1, std140) uniform Data\n"
		"{\n"
		"    highp uvec4 testInput;\n"
		"};\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    protectedTestResultBuffer = testInput;\n"
		"    ${FRAGMENT_OUTPUT}\n"
		"}\n";

	//  set = 0, location = 0 -> buffer ProtectedTestBuffer (uint [4])
	const char* atomicTestShaderTemplateStr =
		"#version 450\n"
		"${INPUT_DECLARATION}\n"
		"\n"
		"layout(set=0, binding=0, std430) buffer ProtectedTestBuffer\n"
		"{\n"
		"    highp uint protectedTestResultBuffer[4];\n"
		"};\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    uint i = uint(${INVOCATION_ID});\n"
		"    ${ATOMIC_FUNCTION_CALL}\n"
		"    ${FRAGMENT_OUTPUT}\n"
		"}\n";

	const char*							shaderTemplateStr;
	std::map<std::string, std::string>	shaderParam;
	switch (m_testType) {
		case SSBO_READ:		shaderTemplateStr = readShaderTemplateStr; break;
		case SSBO_WRITE:	shaderTemplateStr = writeShaderTemplateStr; break;
		case SSBO_ATOMIC:	{
			shaderTemplateStr = atomicTestShaderTemplateStr;
			shaderParam["ATOMIC_FUNCTION_CALL"] = m_extraShader;
			break;
		}
		default: DE_FATAL("Incorrect SSBO test type"); return;
	}

	if (m_shaderType == glu::SHADERTYPE_FRAGMENT)
	{
		shaderParam["INPUT_DECLARATION"]		= "layout(location=0) out mediump vec4 o_color;\n"
												  "layout(location=0) in vec4 vIndex;\n";
		shaderParam["FRAGMENT_OUTPUT"]			= "o_color = vec4( 0.0, 0.4, 1.0, 1.0 );\n";
		shaderParam["INVOCATION_ID"]			= "vIndex.x";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertexShader);
		programCollection.glslSources.add("TestShader") << glu::FragmentSource(tcu::StringTemplate(shaderTemplateStr).specialize(shaderParam));
	}
	else if (m_shaderType == glu::SHADERTYPE_COMPUTE)
	{
		shaderParam["INPUT_DECLARATION"]		= "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
		shaderParam["FRAGMENT_OUTPUT"]			= "";
		shaderParam["INVOCATION_ID"]			= "gl_GlobalInvocationID.x";
		programCollection.glslSources.add("TestShader") << glu::ComputeSource(tcu::StringTemplate(shaderTemplateStr).specialize(shaderParam));
	}
	else
		DE_FATAL("Incorrect shader type");

	m_validator.initPrograms(programCollection);
}

template<typename T>
tcu::TestStatus StorageBufferTestInstance<T>::executeFragmentTest(void)
{
	ProtectedContext&						ctx					(m_protectedContext);
	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();

	const deUint32							testUniformSize		= sizeof(m_testInput);
	de::UniquePtr<vk::BufferWithMemory>		testUniform			(makeBuffer(ctx,
																		PROTECTION_DISABLED,
																		queueFamilyIndex,
																		testUniformSize,
																		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
																		 | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																		vk::MemoryRequirement::HostVisible));

	// Set the test input uniform data
	{
		deMemcpy(testUniform->getAllocation().getHostPtr(), &m_testInput, testUniformSize);
		vk::flushMappedMemoryRange(vk, device, testUniform->getAllocation().getMemory(), testUniform->getAllocation().getOffset(), testUniformSize);
	}
	const deUint32							testBufferSize		= sizeof(ValidationDataStorage<T>);
	de::MovePtr<vk::BufferWithMemory>		testBuffer			(makeBuffer(ctx,
																			PROTECTION_ENABLED,
																			queueFamilyIndex,
																			testBufferSize,
																			vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
																			 | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																			vk::MemoryRequirement::Protected));
    de::MovePtr<vk::BufferWithMemory>		testBufferSource			(makeBuffer(ctx,
																			PROTECTION_ENABLED,
																			queueFamilyIndex,
																			testBufferSize,
																			vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
																			 | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																			vk::MemoryRequirement::Protected));

	vk::Move<vk::VkShaderModule>			vertexShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("vert"), 0));
	vk::Unique<vk::VkShaderModule>			testShader			(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("TestShader"), 0));

	// Create descriptors
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_ALL)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_ALL)
		.build(vk, device));
	vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set information
	{
		vk::VkDescriptorBufferInfo	descTestBuffer			= makeDescriptorBufferInfo(**testBuffer, 0, testBufferSize);
		vk::VkDescriptorBufferInfo	descTestUniform			= makeDescriptorBufferInfo(**testUniform, 0, testUniformSize);
		vk::VkDescriptorBufferInfo	descTestBufferSource	= makeDescriptorBufferInfo(**testBufferSource, 0, testBufferSize);

		vk::DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descTestBuffer)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descTestUniform)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descTestBufferSource)
			.update(vk, device);
	}

	// Create output image
	de::MovePtr<vk::ImageWithMemory>		colorImage			(createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
																			   RENDER_WIDTH, RENDER_HEIGHT,
																			   m_imageFormat,
																			   vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|vk::VK_IMAGE_USAGE_SAMPLED_BIT));
	vk::Unique<vk::VkImageView>				colorImageView		(createImageView(ctx, **colorImage, m_imageFormat));
	vk::Unique<vk::VkRenderPass>			renderPass			(createRenderPass(ctx, m_imageFormat));
	vk::Unique<vk::VkFramebuffer>			framebuffer			(createFramebuffer(ctx, RENDER_WIDTH, RENDER_HEIGHT, *renderPass, *colorImageView));

	// Build pipeline
	vk::Unique<vk::VkPipelineLayout>		pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
	vk::Unique<vk::VkCommandPool>			cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>			cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Create pipeline
	vk::Unique<vk::VkPipeline>				graphicsPipeline	(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass,
																					  *vertexShader, *testShader,
																					  std::vector<vk::VkVertexInputBindingDescription>(),
																					  std::vector<vk::VkVertexInputAttributeDescription>(),
																					  tcu::UVec2(RENDER_WIDTH, RENDER_HEIGHT),
																					  vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST));

	beginCommandBuffer(vk, *cmdBuffer);

	if (m_testType == SSBO_READ || m_testType == SSBO_ATOMIC)
	{
		vk::VkBuffer targetBuffer = (m_testType == SSBO_ATOMIC) ? **testBuffer : **testBufferSource;
		addBufferCopyCmd(vk, *cmdBuffer, queueFamilyIndex, **testUniform, targetBuffer, testUniformSize);
	}

	// Start image barrier
	{
		const vk::VkImageMemoryBarrier	startImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			0,													// srcAccessMask
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// srcStageMask
							  vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &startImgBarrier);
	}

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, vk::makeRect2D(0, 0, RENDER_WIDTH, RENDER_HEIGHT), tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f));
	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	{
		const vk::VkImageMemoryBarrier	endImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// dstAccessMask
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// oldLayout
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};
		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// srcStageMask
							  vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,				// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &endImgBarrier);
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Execute Draw
	{
		const vk::Unique<vk::VkFence> fence (vk::createFence(vk, device));
		VK_CHECK(vk.resetFences(device, 1, &fence.get()));
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}

	// Log inputs
	ctx.getTestContext().getLog()
			<< tcu::TestLog::Message << "Input values: \n"
			<< "1: " << m_testInput << "\n"
			<< tcu::TestLog::EndMessage;

	// Validate buffer
	if (m_validator.validateBuffer(ctx, **testBuffer))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

template<typename T>
tcu::TestStatus StorageBufferTestInstance<T>::executeComputeTest(void)
{
	ProtectedContext&						ctx					(m_protectedContext);
	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();

	const deUint32							testUniformSize		= sizeof(m_testInput);
	de::UniquePtr<vk::BufferWithMemory>		testUniform			(makeBuffer(ctx,
																		PROTECTION_DISABLED,
																		queueFamilyIndex,
																		testUniformSize,
																		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
																		 | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																		vk::MemoryRequirement::HostVisible));

	// Set the test input uniform data
	{
		deMemcpy(testUniform->getAllocation().getHostPtr(), &m_testInput, testUniformSize);
		vk::flushMappedMemoryRange(vk, device, testUniform->getAllocation().getMemory(), testUniform->getAllocation().getOffset(), testUniformSize);
	}

	const deUint32							testBufferSize		= sizeof(ValidationDataStorage<T>);
	de::MovePtr<vk::BufferWithMemory>		testBuffer			(makeBuffer(ctx,
																			PROTECTION_ENABLED,
																			queueFamilyIndex,
																			testBufferSize,
																			vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
																			 | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																			vk::MemoryRequirement::Protected));
	de::MovePtr<vk::BufferWithMemory>		testBufferSource	(makeBuffer(ctx,
																			PROTECTION_ENABLED,
																			queueFamilyIndex,
																			testBufferSize,
																			vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
																			 | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																			vk::MemoryRequirement::Protected));

	vk::Unique<vk::VkShaderModule>			testShader			(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("TestShader"), 0));

	// Create descriptors
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));
	vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set information
	{
		vk::VkDescriptorBufferInfo	descTestBuffer			= makeDescriptorBufferInfo(**testBuffer, 0, testBufferSize);
		vk::VkDescriptorBufferInfo	descTestUniform			= makeDescriptorBufferInfo(**testUniform, 0, testUniformSize);
		vk::VkDescriptorBufferInfo	descTestBufferSource	= makeDescriptorBufferInfo(**testBufferSource, 0, testBufferSize);

		vk::DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descTestBuffer)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descTestUniform)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descTestBufferSource)
			.update(vk, device);
	}


	// Build and execute test
	{
		const vk::Unique<vk::VkFence>		fence				(vk::createFence(vk, device));
		vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
		vk::Unique<vk::VkPipeline>			SSBOPipeline		(makeComputePipeline(vk, device, *pipelineLayout, *testShader, DE_NULL));
		vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
		vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		deUint32							dispatchCount		= (m_testType == SSBO_ATOMIC) ? 4u : 1u;

		beginCommandBuffer(vk, *cmdBuffer);

		if (m_testType == SSBO_READ || m_testType == SSBO_ATOMIC)
		{
			vk::VkBuffer targetBuffer = (m_testType == SSBO_ATOMIC) ? **testBuffer : **testBufferSource;
			addBufferCopyCmd(vk, *cmdBuffer, queueFamilyIndex, **testUniform, targetBuffer, testUniformSize);
		}

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *SSBOPipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

		vk.cmdDispatch(*cmdBuffer, dispatchCount, 1u, 1u);

		endCommandBuffer(vk, *cmdBuffer);
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}

	ctx.getTestContext().getLog()
			<< tcu::TestLog::Message << "Input values: \n"
			<< "1: " << m_testInput << "\n"
			<< tcu::TestLog::EndMessage;

	// Validate buffer
	if (m_validator.validateBuffer(ctx, **testBuffer))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

template<typename T>
tcu::TestStatus StorageBufferTestInstance<T>::iterate(void)
{
	switch (m_shaderType)
	{
		case glu::SHADERTYPE_FRAGMENT:	return executeFragmentTest();
		case glu::SHADERTYPE_COMPUTE:	return executeComputeTest();
		default:
			DE_FATAL("Incorrect shader type"); return tcu::TestStatus::fail("");
	}
}

tcu::TestCaseGroup* createSpecifiedStorageBufferTests (tcu::TestContext&						testCtx,
													   const std::string						groupName,
													   SSBOTestType								testType,
													   const glu::ShaderType					shaderType,
													   const ValidationDataStorage<tcu::UVec4>	testData[],
													   size_t									testCount)
{
	const std::string				testTypeStr		= getSSBOTypeString(testType);
	const std::string				description		= "Storage buffer " + testTypeStr + " tests";
	de::MovePtr<tcu::TestCaseGroup> testGroup		(new tcu::TestCaseGroup(testCtx, groupName.c_str(), description.c_str()));

	for (size_t ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string name = testTypeStr + "_" + de::toString(ndx + 1);
		testGroup->addChild(new StorageBufferTestCase<tcu::UVec4>(testCtx, testType, shaderType, name.c_str(), testData[ndx].values, testData[ndx]));
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createRandomizedBufferTests (tcu::TestContext& testCtx, SSBOTestType testType, const glu::ShaderType shaderType, size_t testCount)
{
	de::Random											rnd				(testCtx.getCommandLine().getBaseSeed());
	std::vector<ValidationDataStorage<tcu::UVec4> >		testData;
	testData.resize(testCount);

	for (size_t ndx = 0; ndx < testCount; ++ndx)
		for (deUint32 compIdx = 0; compIdx < 4; ++compIdx)
			testData[ndx].values[compIdx] = rnd.getUint32();

	return createSpecifiedStorageBufferTests(testCtx, "random", testType, shaderType, testData.data(), testData.size());
}

tcu::TestCaseGroup* createRWStorageBufferTests (tcu::TestContext&							testCtx,
												const std::string							groupName,
												const std::string							groupDescription,
												SSBOTestType								testType,
												const ValidationDataStorage<tcu::UVec4>		testData[],
												size_t										testCount)
{
	de::MovePtr<tcu::TestCaseGroup> ssboRWTestGroup (new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDescription.c_str()));

	glu::ShaderType					shaderTypes[] = {
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};

	for (int shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(shaderTypes); ++shaderNdx)
	{
		const glu::ShaderType				shaderType			= shaderTypes[shaderNdx];
		const std::string					shaderName			= glu::getShaderTypeName(shaderType);
		const std::string					shaderGroupDesc		= "Storage buffer tests for shader type: " + shaderName;
		de::MovePtr<tcu::TestCaseGroup>		testShaderGroup		(new tcu::TestCaseGroup(testCtx, shaderName.c_str(), shaderGroupDesc.c_str()));

		testShaderGroup->addChild(createSpecifiedStorageBufferTests(testCtx, "static", testType, shaderType, testData, testCount));
		testShaderGroup->addChild(createRandomizedBufferTests(testCtx, testType, shaderType, RANDOM_TEST_COUNT));
		ssboRWTestGroup->addChild(testShaderGroup.release());
	}

	return ssboRWTestGroup.release();
}

void calculateAtomicOpData (SSBOAtomicType		type,
							const tcu::UVec4&	inputValue,
							const deUint32		atomicArg,
							std::string&		atomicCall,
							tcu::UVec4&			refValue,
							const deUint32		swapNdx = 0)
{
	switch (type)
	{
		case ATOMIC_ADD:
		{
			refValue	= inputValue + tcu::UVec4(atomicArg);
			atomicCall	= "atomicAdd(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_MIN:
		{
			refValue	= tcu::UVec4(std::min(inputValue.x(), atomicArg), std::min(inputValue.y(), atomicArg), std::min(inputValue.z(), atomicArg), std::min(inputValue.w(), atomicArg));
			atomicCall	= "atomicMin(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_MAX:
		{
			refValue	= tcu::UVec4(std::max(inputValue.x(), atomicArg), std::max(inputValue.y(), atomicArg), std::max(inputValue.z(), atomicArg), std::max(inputValue.w(), atomicArg));
			atomicCall	= "atomicMax(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_AND:
		{
			refValue	= tcu::UVec4(inputValue.x() & atomicArg, inputValue.y() & atomicArg, inputValue.z() & atomicArg, inputValue.w() & atomicArg);
			atomicCall	= "atomicAnd(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_OR:
		{
			refValue	= tcu::UVec4(inputValue.x() | atomicArg, inputValue.y() | atomicArg, inputValue.z() | atomicArg, inputValue.w() | atomicArg);
			atomicCall	= "atomicOr(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_XOR:
		{
			refValue	= tcu::UVec4(inputValue.x() ^ atomicArg, inputValue.y() ^ atomicArg, inputValue.z() ^ atomicArg, inputValue.w() ^ atomicArg);
			atomicCall	= "atomicXor(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_EXCHANGE:
		{
			refValue	= tcu::UVec4(atomicArg);
			atomicCall	= "atomicExchange(protectedTestResultBuffer[i], " + de::toString(atomicArg) + "u);";
			break;
		}
		case ATOMIC_COMPSWAP:
		{
			int			selectedNdx		= swapNdx % 4;
			deUint32	selectedChange	= inputValue[selectedNdx];

			refValue				= inputValue;
			refValue[selectedNdx]	= atomicArg;
			atomicCall				= "atomicCompSwap(protectedTestResultBuffer[i], " + de::toString(selectedChange) + "u, " + de::toString(atomicArg) + "u);";
			break;
		}
		default: DE_FATAL("Incorrect atomic function type"); break;
	}

}

} // anonymous

tcu::TestCaseGroup* createReadStorageBufferTests (tcu::TestContext& testCtx)
{
	const ValidationDataStorage<tcu::UVec4> testData[] = {
		{ tcu::UVec4(0u, 0u, 0u, 0u) },	{ tcu::UVec4(1u, 0u, 0u, 0u) },
		{ tcu::UVec4(0u, 1u, 0u, 0u) },	{ tcu::UVec4(0u, 0u, 1u, 0u) },
		{ tcu::UVec4(0u, 0u, 0u, 1u) },	{ tcu::UVec4(1u, 1u, 1u, 1u) }
	};

	return createRWStorageBufferTests(testCtx, "ssbo_read", "Storage Buffer Read Tests", SSBO_READ, testData, DE_LENGTH_OF_ARRAY(testData));
}

tcu::TestCaseGroup* createWriteStorageBufferTests (tcu::TestContext& testCtx)
{
	const ValidationDataStorage<tcu::UVec4> testData[] = {
		{ tcu::UVec4(0u, 0u, 0u, 0u) }, { tcu::UVec4(1u, 0u, 0u, 0u) },
		{ tcu::UVec4(0u, 1u, 0u, 0u) }, { tcu::UVec4(0u, 0u, 1u, 0u) },
		{ tcu::UVec4(0u, 0u, 0u, 1u) }, { tcu::UVec4(1u, 1u, 1u, 1u) }
	};

	return createRWStorageBufferTests(testCtx, "ssbo_write", "Storage Buffer Write Tests", SSBO_WRITE, testData, DE_LENGTH_OF_ARRAY(testData));
}

tcu::TestCaseGroup* createAtomicStorageBufferTests (tcu::TestContext& testctx)
{
	struct {
		const tcu::UVec4	input;
		const deUint32		atomicArg;
		const deUint32		swapNdx;
	}								testData[]	= {
		{ tcu::UVec4(0u,		1u,			2u,			3u),		10u,	0u	},
		{ tcu::UVec4(10u,		20u,		30u,		40u),		3u,		2u	},
		{ tcu::UVec4(800u,		400u,		230u,		999u),		50u,	3u	},
		{ tcu::UVec4(100800u,	233400u,	22230u,		77999u),	800u,	1u	},
	};

	SSBOAtomicType					testTypes[]	= {
		ATOMIC_ADD,
		ATOMIC_MIN,
		ATOMIC_MAX,
		ATOMIC_AND,
		ATOMIC_OR,
		ATOMIC_XOR,
		ATOMIC_EXCHANGE,
		ATOMIC_COMPSWAP
	};

	glu::ShaderType					shaderTypes[] = {
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};

	de::Random						rnd				(testctx.getCommandLine().getBaseSeed());
	de::MovePtr<tcu::TestCaseGroup>	ssboAtomicTests (new tcu::TestCaseGroup(testctx, "ssbo_atomic", "Storage Buffer Atomic Tests"));

	for (int shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(shaderTypes); ++shaderNdx)
	{
		const glu::ShaderType				shaderType			= shaderTypes[shaderNdx];
		const std::string					shaderName			= glu::getShaderTypeName(shaderType);
		const std::string					shaderDesc			= "Storage Buffer Atomic Tests for shader type: " + shaderName;
		de::MovePtr<tcu::TestCaseGroup>		atomicShaderGroup	(new tcu::TestCaseGroup(testctx, shaderName.c_str(), shaderDesc.c_str()));

		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++typeNdx)
		{
			SSBOAtomicType					atomicType		= testTypes[typeNdx];
			const std::string				atomicTypeStr	= getSSBOAtomicTypeString(atomicType);
			const std::string				atomicDesc		= "Storage Buffer Atomic Tests: " + atomicTypeStr;

			de::MovePtr<tcu::TestCaseGroup>	staticTests		(new tcu::TestCaseGroup(testctx, "static", (atomicDesc + " with static input").c_str()));
			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
			{
				const std::string	name		= "atomic_" + atomicTypeStr + "_" + de::toString(ndx + 1);
				const tcu::UVec4&	inputValue	= testData[ndx].input;
				const deUint32&		atomicArg	= testData[ndx].atomicArg;
				std::string			atomicCall;
				tcu::UVec4			refValue;

				calculateAtomicOpData(atomicType, inputValue, atomicArg, atomicCall, refValue, testData[ndx].swapNdx);

				ValidationDataStorage<tcu::UVec4>	validationData	= { refValue };
				staticTests->addChild(new StorageBufferTestCase<tcu::UVec4>(testctx, SSBO_ATOMIC, shaderType, name.c_str(), inputValue, validationData, atomicCall));
			}

			de::MovePtr<tcu::TestCaseGroup>	randomTests		(new tcu::TestCaseGroup(testctx, "random", (atomicDesc + " with random input").c_str()));
			for (int ndx = 0; ndx < RANDOM_TEST_COUNT; ndx++)
			{
				const std::string					name			= "atomic_" + atomicTypeStr + "_" + de::toString(ndx + 1);
				deUint32							atomicArg		= rnd.getUint16();
				tcu::UVec4							inputValue;
				tcu::UVec4							refValue;
				std::string							atomicCall;

				for (int i = 0; i < 4; i++)
					inputValue[i] = rnd.getUint16();

				calculateAtomicOpData(atomicType, inputValue, atomicArg, atomicCall, refValue, ndx);

				ValidationDataStorage<tcu::UVec4>	validationData	= { refValue };
				randomTests->addChild(new StorageBufferTestCase<tcu::UVec4>(testctx, SSBO_ATOMIC, shaderType, name.c_str(), inputValue, validationData, atomicCall));

			}

			de::MovePtr<tcu::TestCaseGroup>	atomicTests		(new tcu::TestCaseGroup(testctx, atomicTypeStr.c_str(), atomicDesc.c_str()));
			atomicTests->addChild(staticTests.release());
			atomicTests->addChild(randomTests.release());
			atomicShaderGroup->addChild(atomicTests.release());
		}
		ssboAtomicTests->addChild(atomicShaderGroup.release());
	}

	return ssboAtomicTests.release();
}

} // ProtectedMem
} // vkt
