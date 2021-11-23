/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 ARM Ltd.
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
 * \brief VK_ARM_rasterization_order_attachment_access tests.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vktRasterizationOrderAttachmentAccessTests.hpp"
#include "vktRasterizationTests.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;
using namespace std;
using namespace vkt;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{

namespace rasterization
{

namespace
{

class AttachmentAccessOrderTestCase : public TestCase
{
public:
	enum
	{
		ELEM_NUM = 6
	};

							AttachmentAccessOrderTestCase(	tcu::TestContext& context, const std::string& name, const std::string& description,
															bool explicitSync, bool overlapDraws, bool overlapPrimitives, bool overlapInstances,
															VkSampleCountFlagBits sampleCount, deUint32 inputAttachmentNum, bool integerFormat);
	virtual					~AttachmentAccessOrderTestCase	(void);

	virtual void			initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;
	virtual void			checkSupport				(Context& context) const;

	virtual deUint32				getInputAttachmentNum() const = 0;
	virtual deUint32				getColorAttachmentNum() const = 0;
	virtual bool					hasDepthStencil() const = 0;
	virtual VkImageAspectFlagBits	getDSAspect() const = 0;




	deUint32				m_inputAttachmentNum;
	const bool				m_explicitSync;
	const bool				m_overlapDraws;
	const bool				m_overlapPrimitives;
	const bool				m_overlapInstances;
	VkSampleCountFlagBits	m_sampleCount;
	const deUint32			m_sampleNum;
	const bool				m_integerFormat;
	static deUint32			getSampleNum(VkSampleCountFlagBits sampleCount);

	VkFormat getColorFormat() const
	{
		return m_integerFormat ? VK_FORMAT_R32G32_UINT : VK_FORMAT_R32G32_SFLOAT;
	}
	VkFormat getDSFormat() const
	{
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	}

	VkPipelineColorBlendStateCreateFlags getBlendStateFlags() const
	{
		return m_explicitSync ? 0 : VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_ARM;
	}
	virtual VkPipelineDepthStencilStateCreateFlags getDSStateFlags() const
	{
		return 0;
	}
	virtual bool hasDepth() const
	{
		return false;
	}
	virtual bool hasStencil() const
	{
		return false;
	}

protected:
	virtual void			addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const = 0;
	void					addSimpleVertexShader(SourceCollections& programCollection, const std::string &dest) const;
	virtual void			checkAdditionalRasterizationFlags(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM &rasterizationAccess) const
	{
		// unused parameter
		DE_UNREF(rasterizationAccess);
	}
};

class AttachmentAccessOrderColorTestCase : public AttachmentAccessOrderTestCase
{
public:
	AttachmentAccessOrderColorTestCase(	tcu::TestContext& context, const std::string& name, const std::string& description,
										bool explicitSync, bool overlapDraws, bool overlapPrimitives, bool overlapInstances,
										VkSampleCountFlagBits sampleCount, deUint32 inputAttachmentNum, bool integerFormat)
		:AttachmentAccessOrderTestCase(	context, name, description, explicitSync, overlapDraws, overlapPrimitives, overlapInstances, sampleCount,
										inputAttachmentNum, integerFormat)
	{}

	virtual deUint32	getInputAttachmentNum() const
	{
		return m_inputAttachmentNum;
	}

	virtual deUint32	getColorAttachmentNum() const
	{
		return m_inputAttachmentNum;
	}

	virtual bool		hasDepthStencil() const
	{
		return false;
	}

	virtual VkImageAspectFlagBits	getDSAspect() const
	{
		/* not relevant, this return value will not be used */
		return VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
	}
protected:
	virtual void			addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const;
};

class AttachmentAccessOrderDepthTestCase : public AttachmentAccessOrderTestCase
{
public:
	AttachmentAccessOrderDepthTestCase(	tcu::TestContext& context, const std::string& name, const std::string& description,
										bool explicitSync, bool overlapDraws, bool overlapPrimitives, bool overlapInstances,
										VkSampleCountFlagBits sampleCount)
		:AttachmentAccessOrderTestCase(	context, name, description, explicitSync, overlapDraws, overlapPrimitives, overlapInstances, sampleCount,
										1, false)
	{}
	virtual deUint32	getInputAttachmentNum() const
	{
		return m_inputAttachmentNum + 1;
	}

	virtual deUint32	getColorAttachmentNum() const
	{
		return m_inputAttachmentNum;
	}

	virtual bool		hasDepth() const
	{
		return true;
	}
	virtual bool		hasDepthStencil() const
	{
		return true;
	}

	virtual VkImageAspectFlagBits getDSAspect() const
	{
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	virtual VkPipelineDepthStencilStateCreateFlags getDSStateFlags() const
	{
		return m_explicitSync ? 0 : VK_PIPELINE_DEPTH_STENCIL_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_ARM;
	}
protected:
	virtual void			addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const;
	virtual void			checkAdditionalRasterizationFlags(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM &rasterizationAccess) const
	{
		if (!m_explicitSync && !rasterizationAccess.rasterizationOrderDepthAttachmentAccess)
		{
			TCU_THROW(NotSupportedError , "Implicit attachment access rasterization order not guaranteed for depth attachments");
		}
	}
};

class AttachmentAccessOrderStencilTestCase : public AttachmentAccessOrderTestCase
{
public:
	AttachmentAccessOrderStencilTestCase(	tcu::TestContext& context, const std::string& name, const std::string& description,
											bool explicitSync, bool overlapDraws, bool overlapPrimitives, bool overlapInstances,
											VkSampleCountFlagBits sampleCount)
		:AttachmentAccessOrderTestCase(	context, name, description, explicitSync, overlapDraws, overlapPrimitives, overlapInstances, sampleCount,
										1, true)
	{}
	virtual deUint32	getInputAttachmentNum() const
	{
		return m_inputAttachmentNum + 1;
	}

	virtual deUint32	getColorAttachmentNum() const
	{
		return m_inputAttachmentNum;
	}

	virtual bool		hasStencil() const
	{
		return true;
	}
	virtual bool		hasDepthStencil() const
	{
		return true;
	}

	virtual VkImageAspectFlagBits getDSAspect() const
	{
		return VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	virtual VkPipelineDepthStencilStateCreateFlags getDSStateFlags() const
	{
		return m_explicitSync ? 0 : VK_PIPELINE_DEPTH_STENCIL_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_ARM;
	}
protected:
	virtual void			addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const;
	virtual void			checkAdditionalRasterizationFlags(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM &rasterizationAccess) const
	{
		if (!m_explicitSync && !rasterizationAccess.rasterizationOrderStencilAttachmentAccess)
		{
			TCU_THROW(NotSupportedError , "Implicit attachment access rasterization order not guaranteed for stencil attachments");
		}
	}
};


class AttachmentAccessOrderTestInstance : public TestInstance
{
public:
									AttachmentAccessOrderTestInstance	(Context& context, const AttachmentAccessOrderTestCase *testCase);
	virtual							~AttachmentAccessOrderTestInstance	(void);
	virtual tcu::TestStatus			iterate								(void);
protected:
	class RenderSubpass
	{
	public:
		const AttachmentAccessOrderTestCase		*m_testCase;
		deUint32								m_subpass;
		VkSampleCountFlagBits					m_sampleCount;
		Move<VkPipeline>						m_pipeline;
		Move<VkPipelineLayout>					m_pipelineLayout;
		deUint32								m_colorAttNum;
		std::vector<Move<VkImage> >				m_inputAtt;
		std::vector<MovePtr<Allocation> >		m_inputAttMemory;
		std::vector<Move<VkImageView> >			m_inputAttView;
		std::vector<VkAttachmentReference>		m_attachmentReferences;

		void createAttachments(	int subpass, deUint32 inputAttachmentNum, deUint32 colorAttachmentNum, VkSampleCountFlagBits sampleCount,
								Context &context, vector<VkImageView> &views, VkDescriptorSetLayout *pDsetLayout, const AttachmentAccessOrderTestCase *tc);
		void createPipeline(VkRenderPass renderPass, Context &context);

		deUint32	getColorAttachmentNum()
		{
			return m_colorAttNum;
		}
		deUint32	getInputAttachmentNum()
		{
			return static_cast<deUint32>(m_inputAtt.size());
		}
		VkAttachmentReference*	getDepthStencilAttachment()
		{
			return (getColorAttachmentNum() == getInputAttachmentNum()) ? DE_NULL : &m_attachmentReferences[m_colorAttNum];
		}
	};
	void							addPipelineBarrier(	VkCommandBuffer			cmdBuffer,
														VkImage					image,
														VkImageLayout			oldLayout,
														VkImageLayout			newLayout,
														VkAccessFlags			srcAccessMask,
														VkAccessFlags			dstAccessMask,
														VkPipelineStageFlags	srcStageMask,
														VkPipelineStageFlags	dstStageMask,
														VkImageAspectFlags		aspect = VK_IMAGE_ASPECT_COLOR_BIT);

	void							addClearColor(VkCommandBuffer cmdBuffer, VkImage image);
	void							addClearDepthStencil(VkCommandBuffer cmdBuffer, VkImage image);

	void							writeDescriptorSets();
	Move<VkRenderPass>				createRenderPass(VkFormat attFormat);
	void							createVertexBuffer();
	void							createResultBuffer();
	void							addDependency(	vector<VkSubpassDependency> &dependencies, deUint32 source, deUint32 dst,
													VkPipelineStageFlags pipelineFlags, VkAccessFlags accessFlags);

	tcu::TestStatus					validateResults(deUint32 numDraws, deUint32 numPrimitives, deUint32 numInstances);

	const AttachmentAccessOrderTestCase					*m_testCase;


	const DeviceInterface&								m_vk;
	vector<RenderSubpass>								m_subpasses;
	Move<VkRenderPass>									m_renderPass;
	Move<VkFramebuffer>									m_framebuffer;
	Move<VkBuffer>										m_vertexBuffer;
	MovePtr<Allocation>									m_vertexBufferMemory;
	Move<VkCommandPool>									m_cmdPool;
	Move<VkCommandBuffer>								m_cmdBuffer;
	Move<VkSampler>										m_sampler;
	Move<VkDescriptorSetLayout>							m_descSetLayout;
	Move<VkDescriptorPool>								m_descPool;
	Move<VkDescriptorSet>								m_descSet;

	Move<VkBuffer>										m_resultBuffer;
	MovePtr<Allocation>									m_resultBufferMemory;

	enum
	{
		WIDTH = 8,
		HEIGHT = 8,
	};
};

AttachmentAccessOrderTestCase::AttachmentAccessOrderTestCase (	tcu::TestContext& context, const std::string& name, const std::string& description,
																bool explicitSync, bool overlapDraws, bool overlapPrimitives, bool overlapInstances,
																VkSampleCountFlagBits sampleCount, deUint32 inputAttachmentNum, bool integerFormat)
	: TestCase(context, name, description)
	, m_inputAttachmentNum(inputAttachmentNum)
	, m_explicitSync(explicitSync)
	, m_overlapDraws(overlapDraws)
	, m_overlapPrimitives(overlapPrimitives)
	, m_overlapInstances(overlapInstances)
	, m_sampleCount(sampleCount)
	, m_sampleNum(getSampleNum(sampleCount))
	, m_integerFormat(integerFormat)
{
}

AttachmentAccessOrderTestCase::~AttachmentAccessOrderTestCase (void)
{
}
void AttachmentAccessOrderTestCase::addSimpleVertexShader(SourceCollections& programCollection, const std::string &dest) const
{
	std::stringstream vertShader;
	vertShader	<< "#version 310 es\n"
				<< "layout(location = 0) in highp vec2 v_position;\n"
				<< "void main ()\n"
				<< "{\n"
				<< "	gl_Position = vec4(v_position, float(gl_InstanceIndex)/256.0, 1);\n"
				<< "}\n";
	programCollection.glslSources.add(dest) << glu::VertexSource(vertShader.str());
}

void AttachmentAccessOrderColorTestCase::addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const
{
	addSimpleVertexShader(programCollection, "vert1");
	addSimpleVertexShader(programCollection, "vert2");

	std::stringstream fragShader;
	fragShader	<< "#version 450\n"
				<< "precision highp ${SCALAR_NAME};\n"
				<< "precision highp ${SUBPASS_INPUT};\n";
	for (deUint32 i=0; i < m_inputAttachmentNum; i++)
	{
		fragShader	<< "layout( set = 0, binding = " << i << ", input_attachment_index = " << i << " ) uniform ${SUBPASS_INPUT} in" << i << ";\n"
					<< "layout( location = " << i << " ) out ${VEC_NAME}2 out" << i << ";\n";
	}


	fragShader	<< "layout( push_constant ) uniform ConstBlock\n"
				<< "{\n"
				<< "	uint drawCur;\n"
				<< "};\n"
				<< "void main()\n"
				<< "{\n"
				<< "	uint instanceCur = uint(round(gl_FragCoord.z * 256.0));\n"
				<< "	uint primitiveCur = uint(gl_PrimitiveID) / 2u;\n"
				<< "	uint primitiveNum = ${PRIMITIVE_NUM}u;\n"
				<< "	uint instanceNum = ${INSTANCE_NUM}u;\n"
				<< "	uint drawNum = ${DRAW_NUM}u;\n"
				<< "	uint curIndex = drawCur * instanceNum * primitiveNum + instanceCur * primitiveNum + primitiveCur;\n"
				<< "	uint total = drawNum * instanceNum * primitiveNum;\n"
				<< "	uint zero = curIndex / total;\n"
				<< "	uint index;\n"
				<< "	uint pre_fetch_loop = uint(gl_FragCoord.x) * uint(gl_FragCoord.y) * (drawNum * primitiveNum - drawCur * primitiveNum - primitiveCur);\n"
				<< "	uint post_fetch_loop = uint(gl_FragCoord.x) + uint(gl_FragCoord.y) + (drawNum * instanceNum - drawCur * instanceNum - instanceCur);\n"
				<< "	for(index = 0u; index < pre_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n"
				<< "	${VEC_NAME}2 previous[${ATT_NUM}];\n";

	for (deUint32 i=0; i < m_inputAttachmentNum; i++)
	{
		if (m_sampleCount == VK_SAMPLE_COUNT_1_BIT)
		{
			fragShader	<< "	previous[" << i << "] = subpassLoad( in" << i << ").xy;\n";
		}
		else
		{
			fragShader	<< "	previous[" << i << "] = subpassLoad( in" << i << ", gl_SampleID).xy;\n";
		}
	}
	fragShader	<< "	for(index = 0u; index < post_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n";
	for (deUint32 i=0; i < m_inputAttachmentNum; i++)
	{
		fragShader	<< "	if (previous[" << i << "].y == 0 && curIndex == 0)\n"
					<< "	{\n"
					<< "		out" << i << ".y = previous[" << i << "].y + (1u + zero + gl_SampleID + " << i << "u);\n"
					<< "		out" << i << ".x = previous[" << i << "].x;\n"
					<< "	}\n"
					<< "	else if (previous[" << i << "].y == curIndex + gl_SampleID + " << i << ")\n"
					<< "	{\n"
					<< "		out" << i << ".y = previous[" << i << "].y + 1 + zero;\n"
					<< "		out" << i << ".x = previous[" << i << "].x;\n"
					<< "	}\n"
					<< "	else\n"
					<< "	{\n"
					<< "		out" << i << ".y = 0u;\n"
					<< "		out" << i << ".x = 1u;\n"
					<< "	}\n";
	}
	fragShader	<< "}\n";

	tcu::StringTemplate fragShaderTpl(fragShader.str());

	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShaderTpl.specialize(params));
}
void AttachmentAccessOrderDepthTestCase::addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const
{
	std::stringstream vertShader;
	vertShader	<< "#version 460\n"
				<< "layout(location = 0) in highp vec2 v_position;\n"
				<< "layout(location = 1) flat out uint instance_index;"
				<< "layout( push_constant ) uniform ConstBlock\n"
				<< "{\n"
				<< "	uint drawCur;\n"
				<< "};\n"
				<< "void main ()\n"
				<< "{\n"
				<< "	uint primitiveCur = uint(gl_VertexIndex) / 6u;\n"
				<< "	uint instanceNum = ${INSTANCE_NUM};\n"
				<< "	uint primitiveNum = ${PRIMITIVE_NUM};\n"
				<< "	uint drawNum = ${DRAW_NUM};\n"
				<< "	uint curIndex = drawCur * instanceNum * primitiveNum + gl_InstanceIndex * primitiveNum + primitiveCur;\n"
				<< "	uint indexNum = drawNum * instanceNum * primitiveNum;\n"
				<< "	instance_index = gl_InstanceIndex;\n"
				<< "	gl_Position = vec4(v_position, 0.125 * float(curIndex) / float(indexNum), 1);\n"
				<< "}\n";

	tcu::StringTemplate vertShaderTpl(vertShader.str());
	programCollection.glslSources.add("vert1") << glu::VertexSource(vertShaderTpl.specialize(params));
	addSimpleVertexShader(programCollection, "vert2");

	std::stringstream fragShader;
	fragShader	<< "#version 450\n"
				<< "precision highp ${SCALAR_NAME};\n"
				<< "precision highp ${SUBPASS_INPUT};\n"
				<< "layout( set = 0, binding = 0, input_attachment_index = 0 ) uniform ${SUBPASS_INPUT} in_color;\n"
				<< "layout( set = 0, binding = 1, input_attachment_index = 1 ) uniform ${SUBPASS_INPUT} in_ds;\n"
				<< "layout( location = 0 ) out ${VEC_NAME}2 out0;\n"
				<< "layout( location = 1 ) flat in uint instance_index;\n"
				<< "layout( push_constant ) uniform ConstBlock\n"
				<< "{\n"
				<< "	uint drawCur;\n"
				<< "};\n"
				<< "void main()\n"
				<< "{\n"
				<< "	uint instanceCur = instance_index;\n"
				<< "	uint primitiveCur = uint(gl_PrimitiveID) / 2u;\n"
				<< "	uint primitiveNum = ${PRIMITIVE_NUM}u;\n"
				<< "	uint instanceNum = ${INSTANCE_NUM}u;\n"
				<< "	uint drawNum = ${DRAW_NUM}u;\n"
				<< "	uint curIndex = drawCur * instanceNum * primitiveNum + instanceCur * primitiveNum + primitiveCur;\n"
				<< "	uint total = drawNum * instanceNum * primitiveNum;\n"
				<< "	uint zero = curIndex / total;\n"
				<< "	uint index;\n"
				<< "	uint pre_fetch_loop = uint(gl_FragCoord.x) * uint(gl_FragCoord.y) * (drawNum * primitiveNum - drawCur * primitiveNum - primitiveCur);\n"
				<< "	uint post_fetch_loop = uint(gl_FragCoord.x) + uint(gl_FragCoord.y) + (drawNum * instanceNum - drawCur * instanceNum - instanceCur);\n"
				<< "	for(index = 0u; index < pre_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n";
	if (m_sampleCount == VK_SAMPLE_COUNT_1_BIT)
	{
		fragShader	<< "	vec2 ds = subpassLoad( in_ds ).xy;\n"
					<< "	${VEC_NAME}2 color = subpassLoad( in_color ).xy;\n";
	}
	else
	{
		fragShader	<< "	vec2 ds = subpassLoad( in_ds, gl_SampleID ).xy;\n"
					<< "	${VEC_NAME}2 color = subpassLoad( in_color, gl_SampleID ).xy;\n";
	}
	fragShader	<< "	for(index = 0u; index < post_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n"
				<< "	if (curIndex == 0 && ds.x == 0)\n"
				<< "	{\n"
				<< "		out0.x = color.x;\n"
				<< "		out0.y = curIndex + 1 + gl_SampleID + zero;\n";
	if (m_sampleCount != VK_SAMPLE_COUNT_1_BIT)
	{
		fragShader	<< "	gl_FragDepth = 0.125 * (float(curIndex) / float(total)) + gl_SampleID / 128.0;\n";
	}
	fragShader	<< "	}\n"
				<< "	else if (ds.x == 0.125 * float(curIndex - 1) / float(total) + gl_SampleID / 128.0)\n"
				<< "	{\n"
				<< "		out0.x = color.x;\n"
				<< "		out0.y = curIndex + 1 + gl_SampleID + zero;\n";
	if (m_sampleCount != VK_SAMPLE_COUNT_1_BIT)
	{
		fragShader	<< "	gl_FragDepth = 0.125 * (float(curIndex) / float(total)) + gl_SampleID / 128.0;\n";
	}
	fragShader	<< "	}\n"
				<< "	else\n"
				<< "	{\n"
				<< "		out0.y = 0;\n"
				<< "		out0.x = 1u;\n"
				<< "	}\n"
				<< "}\n";

	tcu::StringTemplate fragShaderTpl(fragShader.str());

	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShaderTpl.specialize(params));
}
void AttachmentAccessOrderStencilTestCase::addShadersInternal(SourceCollections& programCollection, const std::map<std::string, std::string> &params) const
{
	std::stringstream vertShader;
	vertShader	<< "#version 460\n"
				<< "layout(location = 0) in highp vec2 v_position;\n"
				<< "layout(location = 1) flat out uint instance_index;"
				<< "layout( push_constant ) uniform ConstBlock\n"
				<< "{\n"
				<< "	uint drawCur;\n"
				<< "};\n"
				<< "void main ()\n"
				<< "{\n"
				<< "	uint primitiveCur = uint(gl_VertexIndex) / 6u;\n"
				<< "	uint instanceNum = ${INSTANCE_NUM};\n"
				<< "	uint primitiveNum = ${PRIMITIVE_NUM};\n"
				<< "	uint drawNum = ${DRAW_NUM};\n"
				<< "	uint curIndex = drawCur * instanceNum * primitiveNum + gl_InstanceIndex * primitiveNum + primitiveCur;\n"
				<< "	uint indexNum = drawNum * instanceNum * primitiveNum;\n"
				<< "	instance_index = gl_InstanceIndex;\n"
				<< "	gl_Position = vec4(v_position, 0.125 * float(curIndex) / float(indexNum), 1);\n"
				<< "}\n";

	tcu::StringTemplate vertShaderTpl(vertShader.str());
	programCollection.glslSources.add("vert1") << glu::VertexSource(vertShaderTpl.specialize(params));
	addSimpleVertexShader(programCollection, "vert2");

	std::stringstream fragShader;
	fragShader	<< "#version 450\n"
				<< "precision highp ${SCALAR_NAME};\n"
				<< "precision highp ${SUBPASS_INPUT};\n"
				<< "layout( set = 0, binding = 0, input_attachment_index = 0 ) uniform ${SUBPASS_INPUT} in_color;\n"
				<< "layout( set = 0, binding = 1, input_attachment_index = 1 ) uniform ${SUBPASS_INPUT} in_ds;\n"
				<< "layout( location = 0 ) out ${VEC_NAME}2 out0;\n"
				<< "layout( location = 1 ) flat in uint instance_index;\n"
				<< "layout( push_constant ) uniform ConstBlock\n"
				<< "{\n"
				<< "	uint drawCur;\n"
				<< "};\n"
				<< "void main()\n"
				<< "{\n"
				<< "	uint instanceCur = instance_index;\n"
				<< "	uint primitiveCur = uint(gl_PrimitiveID) / 2u;\n"
				<< "	uint primitiveNum = ${PRIMITIVE_NUM}u;\n"
				<< "	uint instanceNum = ${INSTANCE_NUM}u;\n"
				<< "	uint drawNum = ${DRAW_NUM}u;\n"
				<< "	uint curIndex = drawCur * instanceNum * primitiveNum + instanceCur * primitiveNum + primitiveCur;\n"
				<< "	uint total = drawNum * instanceNum * primitiveNum;\n"
				<< "	uint zero = curIndex / total;\n"
				<< "	uint index;\n"
				<< "	uint pre_fetch_loop = uint(gl_FragCoord.x) * uint(gl_FragCoord.y) * (drawNum * primitiveNum - drawCur * primitiveNum - primitiveCur);\n"
				<< "	uint post_fetch_loop = uint(gl_FragCoord.x) + uint(gl_FragCoord.y) + (drawNum * instanceNum - drawCur * instanceNum - instanceCur);\n"
				<< "	for(index = 0u; index < pre_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n";
	if (m_sampleCount == VK_SAMPLE_COUNT_1_BIT)
	{
		fragShader	<< "	${VEC_NAME}2 ds = subpassLoad( in_ds ).xy;\n"
					<< "	${VEC_NAME}2 color = subpassLoad( in_color ).xy;\n";
	}
	else
	{
		fragShader	<< "	${VEC_NAME}2 ds = subpassLoad( in_ds, gl_SampleID).xy;\n"
					<< "	${VEC_NAME}2 color = subpassLoad( in_color, gl_SampleID).xy;\n";
	}
	fragShader	<< "	for(index = 0u; index < post_fetch_loop; index++)\n"
				<< "	{\n"
				<< "		zero = uint(sin(float(zero)));\n"
				<< "	}\n"
				<< "	if (ds.x == curIndex)\n"
				<< "	{\n"
				<< "		out0.x = color.x;\n"
				<< "		out0.y = curIndex + 1 + gl_SampleID + zero;\n"
				<< "	}\n"
				<< "	else\n"
				<< "	{\n"
				<< "		out0.y = 0;\n"
				<< "		out0.x = 1u;\n"
				<< "	}\n"
				<< "}\n";

	tcu::StringTemplate fragShaderTpl(fragShader.str());

	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShaderTpl.specialize(params));
}

void AttachmentAccessOrderTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::map<std::string, std::string> params;

	params["PRIMITIVE_NUM"] = std::to_string(m_overlapPrimitives?ELEM_NUM:1);
	params["INSTANCE_NUM"] = std::to_string(m_overlapInstances?ELEM_NUM:1);
	params["DRAW_NUM"] = std::to_string(m_overlapDraws?ELEM_NUM:1);
	params["ATT_NUM"] = std::to_string(m_inputAttachmentNum);
	params["SAMPLE_NUM"] = std::to_string(m_sampleNum);

	if (m_integerFormat)
	{
		params["SUBPASS_INPUT"] = "usubpassInput";
		params["SCALAR_NAME"] = "int";
		params["VEC_NAME"] = "uvec";
	}
	else
	{
		params["SUBPASS_INPUT"] = "subpassInput";
		params["SCALAR_NAME"] = "float";
		params["VEC_NAME"] = "vec";
	}
	if (m_sampleCount != VK_SAMPLE_COUNT_1_BIT)
	{
		params["SUBPASS_INPUT"] = params["SUBPASS_INPUT"] + "MS";
	}

	/* add the vertex (for both renderpasses) and fragment shaders for first renderpass */
	addShadersInternal(programCollection, params);

	std::stringstream fragShaderResolve;
	fragShaderResolve	<< "#version 450\n"
						<< "precision highp ${SCALAR_NAME};\n"
						<< "precision highp ${SUBPASS_INPUT};\n";
	for (deUint32 i=0; i < m_inputAttachmentNum; i++)
	{
		fragShaderResolve	<< "layout( set = 0, binding = " << i << ", input_attachment_index = " << i << " ) uniform ${SUBPASS_INPUT} in" << i << ";\n";
	}
	fragShaderResolve << "layout( location = 0 ) out ${VEC_NAME}2 out0;\n";

	fragShaderResolve	<< "void main()\n"
						<< "{\n"
						<< "	uint primitiveNum = ${PRIMITIVE_NUM}u;\n"
						<< "	uint instanceNum = ${INSTANCE_NUM}u;\n"
						<< "	uint drawNum = ${DRAW_NUM}u;\n"
						<< "	uint sampleNum = ${SAMPLE_NUM}u;\n"
						<< "	uint totalNum = primitiveNum * instanceNum * drawNum;\n"
						<< "	out0.y = totalNum;\n"
						<< "	out0.x = 0u;\n"
						<< "	${VEC_NAME}2 val;\n"
						<< "	int i;\n";

	for (deUint32 i=0; i < m_inputAttachmentNum; i++)
	{
		if (m_sampleNum == 1)
		{
			fragShaderResolve	<< "	val = subpassLoad(in" << i << ").xy;\n"
								<< "	if (val.x != 0u || val.y != totalNum + " << i << "){\n"
								<< "		out0.y = val.y;\n"
								<< "		out0.x = val.x;\n"
								<< "	}\n";
		}
		else
		{
			fragShaderResolve	<< "	for (i = 0; i < sampleNum; i++) {\n"
								<< "		val = subpassLoad(in" << i << ", i).xy;\n"
								<< "		if (val.x != 0u || val.y != totalNum + i + " << i << "){\n"
								<< "			out0.y = val.y;\n"
								<< "			out0.x = val.x;\n"
								<< "		}\n"
								<< "	}\n";
		}
	}

	fragShaderResolve << "}\n";

	tcu::StringTemplate fragShaderResolveTpl(fragShaderResolve.str());

	programCollection.glslSources.add("frag_resolve") << glu::FragmentSource(fragShaderResolveTpl.specialize(params));
}


TestInstance* AttachmentAccessOrderTestCase::createInstance (Context& context) const
{
	return new AttachmentAccessOrderTestInstance(context, this);
}

void AttachmentAccessOrderTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_ARM_rasterization_order_attachment_access");

	VkPhysicalDeviceVulkan12Properties vulkan12Properties = {};
	vulkan12Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

	VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM rasterizationAccess = {};
	rasterizationAccess.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_ARM;
	rasterizationAccess.pNext = &vulkan12Properties;

	VkPhysicalDeviceProperties2 properties = {};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &rasterizationAccess;

	VkPhysicalDeviceFeatures features = {};

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);
	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

	if ( m_integerFormat )
	{
		if ((vulkan12Properties.framebufferIntegerColorSampleCounts & m_sampleCount) == 0 ||
			(properties.properties.limits.sampledImageIntegerSampleCounts & m_sampleCount) == 0)
		{
			TCU_THROW(NotSupportedError, "Sample count not supported");
		}
	}
	else
	{
		if ((properties.properties.limits.framebufferColorSampleCounts & m_sampleCount) == 0 ||
			(properties.properties.limits.sampledImageColorSampleCounts & m_sampleCount) == 0)
		{
			TCU_THROW(NotSupportedError , "Sample count not supported");
		}
	}

	/* sampleRateShading must be enabled to call fragment shader for all the samples in multisampling */
	if ( (m_sampleCount != VK_SAMPLE_COUNT_1_BIT && !features.sampleRateShading) )
	{
		TCU_THROW(NotSupportedError , "sampleRateShading feature not supported");
	}

	/* Needed for gl_PrimitiveID */
	if ( !features.geometryShader )
	{
		TCU_THROW(NotSupportedError , "geometryShader feature not supported");
	}

	if (properties.properties.limits.maxFragmentOutputAttachments < m_inputAttachmentNum ||
		properties.properties.limits.maxPerStageDescriptorInputAttachments < m_inputAttachmentNum)
	{
		TCU_THROW(NotSupportedError , "Feedback attachment number not supported");
	}

	if (!m_explicitSync && !rasterizationAccess.rasterizationOrderColorAttachmentAccess)
	{
		TCU_THROW(NotSupportedError , "Implicit attachment access rasterization order not guaranteed for color attachments");
	}

	checkAdditionalRasterizationFlags(rasterizationAccess);
}

deUint32 AttachmentAccessOrderTestCase::getSampleNum(VkSampleCountFlagBits sampleCount)
{
	deUint32 ret = 0;
	switch(sampleCount)
	{
		case VK_SAMPLE_COUNT_1_BIT: ret = 1; break;
		case VK_SAMPLE_COUNT_2_BIT: ret = 2; break;
		case VK_SAMPLE_COUNT_4_BIT: ret = 4; break;
		case VK_SAMPLE_COUNT_8_BIT: ret = 8; break;
		case VK_SAMPLE_COUNT_16_BIT: ret = 16; break;
		case VK_SAMPLE_COUNT_32_BIT: ret = 32; break;
		case VK_SAMPLE_COUNT_64_BIT: ret = 64; break;
		default: DE_ASSERT(false);
	};
	return ret;
}

void AttachmentAccessOrderTestInstance::RenderSubpass::createAttachments(	int subpass, deUint32 inputAttachmentNum, deUint32 colorAttachmentNum,
																			VkSampleCountFlagBits sampleCount,
																			Context &context, vector<VkImageView> &views, VkDescriptorSetLayout *pDsetLayout,
																			const AttachmentAccessOrderTestCase *tc)
{
	m_testCase = tc;
	m_subpass = subpass;
	m_sampleCount = sampleCount;
	m_colorAttNum = colorAttachmentNum;
	const DeviceInterface& vk = context.getDeviceInterface();
	const VkDevice device = context.getDevice();
	const deUint32 queueFamilyIndex = context.getUniversalQueueFamilyIndex();
	Allocator& allocator = context.getDefaultAllocator();

	// Pipeline Layout
	{
		VkPushConstantRange pushConstantsInfo =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
			0,								// uint32_t				offset;
			4,								// uint32_t				size;
		};
		if (m_testCase->hasDepthStencil())
		{
			pushConstantsInfo.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		const VkPipelineLayoutCreateInfo		pipelineLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,												// const void*					pNext;
			0u,														// VkPipelineLayoutCreateFlags	flags;
			1u,														// deUint32						descriptorSetCount;
			pDsetLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
			m_subpass == 0 ? 1u : 0u,								// deUint32						pushConstantRangeCount;
			m_subpass == 0 ? &pushConstantsInfo : nullptr			// const VkPushConstantRange*	pPushConstantRanges;
		};
		m_pipelineLayout					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	}
	VkFormat attFormat = m_testCase->getColorFormat();

	/* Same create info for all the color attachments */
	VkImageCreateInfo colorImageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		attFormat,										// VkFormat					format;
		{ WIDTH,	HEIGHT, 1u },						// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		sampleCount,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,			// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		1u,												// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout			initialLayout;
	};

	for (deUint32 i = 0; i < inputAttachmentNum; i++)
	{
		VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		/* Image for the DS attachment */
		if (i >= colorAttachmentNum)
		{
			attFormat = m_testCase->getDSFormat();
			colorImageCreateInfo.format = attFormat;
			colorImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			colorImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			aspect = m_testCase->getDSAspect();
		}

		m_inputAtt.push_back(createImage(vk, device, &colorImageCreateInfo, DE_NULL));
		VkImageViewCreateInfo colorTargetViewInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkImageViewCreateFlags	flags;
			*m_inputAtt.back(),							// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
			attFormat,									// VkFormat					format;
			makeComponentMappingRGBA(),					// VkComponentMapping		components;
			{
				aspect,										// VkImageAspectFlags			aspectMask;
				0u,											// deUint32						baseMipLevel;
				1u,											// deUint32						mipLevels;
				0u,											// deUint32						baseArrayLayer;
				1u,											// deUint32						arraySize;
			},											// VkImageSubresourceRange		subresourceRange;
		};
		m_inputAttMemory.push_back(allocator.allocate(getImageMemoryRequirements(vk, device, *m_inputAtt.back()), MemoryRequirement::Any));
		VK_CHECK(vk.bindImageMemory(device, *m_inputAtt.back(), m_inputAttMemory.back()->getMemory(), m_inputAttMemory.back()->getOffset()));
		m_inputAttView.push_back(createImageView(vk, device, &colorTargetViewInfo));

		m_attachmentReferences.push_back({(deUint32)views.size(), VK_IMAGE_LAYOUT_GENERAL});
		views.push_back(*m_inputAttView.back());
	}
}

void AttachmentAccessOrderTestInstance::RenderSubpass::createPipeline(VkRenderPass renderPass, Context &context)
{

	const DeviceInterface& vk = context.getDeviceInterface();
	const VkDevice device = context.getDevice();
	const Unique<VkShaderModule> vs(createShaderModule(vk, device, context.getBinaryCollection().get(m_subpass == 0 ? "vert1" : "vert2"), 0));
	const Unique<VkShaderModule> fs(createShaderModule(vk, device, context.getBinaryCollection().get(m_subpass == 0 ? "frag" : "frag_resolve"), 0));

	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0,								// deUint32					binding;
		sizeof(tcu::Vec2),				// deUint32					strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputStepRate	stepRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescription =
	{
		0u,									// deUint32	location;
		0u,									// deUint32	binding;
		VK_FORMAT_R32G32_SFLOAT,			// VkFormat	format;
		0u,									// deUint32	offsetInBytes;
	};

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									bindingCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		1u,																// deUint32									attributeCount;
		&vertexInputAttributeDescription,								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const std::vector<VkViewport> viewports(1, {0, 0, WIDTH, HEIGHT, 0, 1});
	const std::vector<VkRect2D> scissors(1, { {0, 0}, {WIDTH, HEIGHT} });
	const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthClampEnable;
		VK_FALSE,													// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
		VK_FALSE,													// VkBool32									depthBiasEnable;
		0.0f,														// float									depthBiasConstantFactor;
		0.0f,														// float									depthBiasClamp;
		0.0f,														// float									depthBiasSlopeFactor;
		1.0f,														// float									lineWidth;
	};
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		m_sampleCount,												// VkSampleCountFlagBits					rasterizationSamples;
		VK_TRUE,													// VkBool32									sampleShadingEnable;
		1.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentState(m_colorAttNum,
		{
			false,								// VkBool32				blendEnable;
			VK_BLEND_FACTOR_ONE,				// VkBlend				srcBlendColor;
			VK_BLEND_FACTOR_ONE,				// VkBlend				destBlendColor;
			VK_BLEND_OP_ADD,					// VkBlendOp			blendOpColor;
			VK_BLEND_FACTOR_ONE,				// VkBlend				srcBlendAlpha;
			VK_BLEND_FACTOR_ONE,				// VkBlend				destBlendAlpha;
			VK_BLEND_OP_ADD,					// VkBlendOp			blendOpAlpha;
			(VK_COLOR_COMPONENT_R_BIT |
			 VK_COLOR_COMPONENT_G_BIT)			// VkChannelFlags		channelWriteMask;
		});

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		/* always needed */
		m_testCase->getBlendStateFlags(),							// VkPipelineColorBlendStateCreateFlags			flags;
		false,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		(deUint32)colorBlendAttachmentState.size(),					// deUint32										attachmentCount;
		colorBlendAttachmentState.data(),							// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	VkStencilOpState stencilOpState =
	{
		VK_STENCIL_OP_ZERO,					// VkStencilOp	failOp;
		VK_STENCIL_OP_INCREMENT_AND_WRAP,	// VkStencilOp	passOp;
		VK_STENCIL_OP_INCREMENT_AND_WRAP,	// VkStencilOp	depthFailOp;
		VK_COMPARE_OP_ALWAYS,				// VkCompareOp	compareOp;
		0xff,								// uint32_t		compareMask;
		0xff,								// uint32_t		writeMask;
		0,									// uint32_t		reference;
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
								// VkStructureType							sType;
		DE_NULL,				// const void*								pNext;
		m_testCase->getDSStateFlags(),
								// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,				// VkBool32									depthTestEnable;
		VK_TRUE,				// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,	// VkCompareOp								depthCompareOp;
		VK_FALSE,				// VkBool32									depthBoundsTestEnable;
		VK_TRUE,				// VkBool32									stencilTestEnable;
		stencilOpState,			// VkStencilOpState							front;
		stencilOpState,			// VkStencilOpState							back;
		0.0f,					// float									minDepthBounds;
		1.0f,					// float									maxDepthBounds;
	};


	m_pipeline = makeGraphicsPipeline(	vk,									// const DeviceInterface&							vk
										device,								// const VkDevice									device
										*m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
										*vs,								// const VkShaderModule								vertexShaderModule
										DE_NULL,							// const VkShaderModule								tessellationControlShaderModule
										DE_NULL,							// const VkShaderModule								tessellationEvalShaderModule
										DE_NULL,							// const VkShaderModule								geometryShaderModule
										*fs,								// const VkShaderModule								fragmentShaderModule
										renderPass,							// const VkRenderPass								renderPass
										viewports,							// const std::vector<VkViewport>&					viewports
										scissors,							// const std::vector<VkRect2D>&						scissors
										VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,// const VkPrimitiveTopology						topology
										m_subpass,							// const deUint32									subpass
										0u,									// const deUint32									patchControlPoints
										&vertexInputStateParams,			// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
										&rasterizationStateInfo,			// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
										&multisampleStateParams,			// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
										&depthStencilStateCreateInfo,		// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo,
										&colorBlendStateParams,				// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo,
										DE_NULL);							// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
}

static Move<VkSampler> makeSampler (const DeviceInterface& vk, const VkDevice& device)
{
	const VkSamplerCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,							// VkFilter					magFilter;
		VK_FILTER_NEAREST,							// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_LINEAR,				// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeW;
		0.0f,										// float					mipLodBias;
		VK_FALSE,									// VkBool32					anisotropyEnable;
		1.0f,										// float					maxAnisotropy;
		VK_FALSE,									// VkBool32					compareEnable;
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp				compareOp;
		0.0f,										// float					minLod;
		0.0f,										// float					maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor			borderColor;
		VK_FALSE									// VkBool32					unnormalizedCoordinates;
	};

	return createSampler(vk, device, &createInfo);
}

static Move<VkDescriptorSetLayout> makeDescriptorSetLayout(const DeviceInterface &vk, const VkDevice device, deUint32 attNum)
{
	vector<VkDescriptorSetLayoutBinding>	bindings(attNum,
	{
		0,												// binding
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			// descriptorType
		1u,												// descriptorCount
		VK_SHADER_STAGE_FRAGMENT_BIT,					// stageFlags
		DE_NULL,										// pImmutableSamplers
	});
	for (deUint32 i = 0; i < attNum; i++)
	{
		bindings[i].binding = i;
	}

	const VkDescriptorSetLayoutCreateInfo	layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,										// pNext
		0u,												// flags
		attNum,											// bindingCount
		bindings.data(),								// pBindings
	};

	return vk::createDescriptorSetLayout(vk, device, &layoutCreateInfo);
}
void AttachmentAccessOrderTestInstance::writeDescriptorSets()
{
	deUint32 attNum = m_testCase->getInputAttachmentNum();
	for (deUint32 i = 0 ; i < attNum ; i++ )
	{
		VkDescriptorImageInfo img_info = {};
		img_info.sampler = *m_sampler;
		img_info.imageView = *m_subpasses[0].m_inputAttView[i];
		img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = *m_descSet;
		write.dstBinding = i;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		write.pImageInfo = &img_info;

		m_vk.updateDescriptorSets(m_context.getDevice(), 1u, &write, 0u, DE_NULL);
	}
}

void AttachmentAccessOrderTestInstance::addDependency(	vector<VkSubpassDependency> &dependencies, deUint32 source, deUint32 dst,
														VkPipelineStageFlags pipelineFlags, VkAccessFlags accessFlags)
{
	dependencies.push_back({
		source,											//uint32_t					srcSubpass;
		dst,											//uint32_t					dstSubpass;
		pipelineFlags,									//VkPipelineStageFlags		srcStageMask;
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			//VkPipelineStageFlags		dstStageMask;
		accessFlags,									//VkAccessFlags				srcAccessMask;
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			//VkAccessFlags				dstAccessMask;
		0,												//VkDependencyFlags			dependencyFlags;
	});
}
Move<VkRenderPass> AttachmentAccessOrderTestInstance::createRenderPass(VkFormat attFormat)
{
	const VkDevice device = m_context.getDevice();
	vector<VkAttachmentDescription> attachmentDescs;
	for (deUint32 subpass = 0; subpass < 2; subpass ++)
	{
		for (deUint32 i = 0 ; i < m_subpasses[subpass].getInputAttachmentNum(); i++)
		{
			VkFormat format = attFormat;
			if (i >= m_subpasses[subpass].getColorAttachmentNum())
			{
				format = m_testCase->getDSFormat();
			}
			attachmentDescs.push_back({
				0,											// VkAttachmentDescriptionFlags		flags;
				format,										// VkFormat							format;
				m_subpasses[subpass].m_sampleCount,			// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					finalLayout;
			});
		}
	}

	std::vector<VkSubpassDescription> subpasses(2, VkSubpassDescription{});

	subpasses[0].pipelineBindPoint =	VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].inputAttachmentCount = m_subpasses[0].getInputAttachmentNum();
	subpasses[0].pInputAttachments =	m_subpasses[0].m_attachmentReferences.data();
	subpasses[0].colorAttachmentCount = m_subpasses[0].getColorAttachmentNum();
	subpasses[0].pColorAttachments =	m_subpasses[0].m_attachmentReferences.data();
	subpasses[0].pDepthStencilAttachment = m_subpasses[0].getDepthStencilAttachment();

	subpasses[1].pipelineBindPoint =	VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].inputAttachmentCount = m_subpasses[0].getColorAttachmentNum();
	subpasses[1].pInputAttachments =	m_subpasses[0].m_attachmentReferences.data();
	subpasses[1].colorAttachmentCount = m_subpasses[1].getColorAttachmentNum();
	subpasses[1].pColorAttachments =	m_subpasses[1].m_attachmentReferences.data();

	/* self dependency for subpass 0 and dependency from subpass 0 to 1 */
	vector<VkSubpassDependency> dependencies;
	addDependency(dependencies, 0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	if (m_testCase->m_explicitSync)
	{
		addDependency(dependencies, 0, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		if (m_testCase->hasDepthStencil())
		{
			addDependency(dependencies, 0, 0, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		}
	}
	else
	{
		subpasses[0].flags = VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_ARM;
		if (m_testCase->hasDepth())
		{
			subpasses[0].flags |= VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_ARM;
		}
		else if (m_testCase->hasStencil())
		{
			subpasses[0].flags |= VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_ARM;
		}
	}

	VkRenderPassCreateInfo renderPassCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// VkStructureType					sType;
		NULL,											// const void*						pNext;
		0,												// VkRenderPassCreateFlags			flags;
		(deUint32)attachmentDescs.size(),				// uint32_t							attachmentCount;
		attachmentDescs.data(),							// const VkAttachmentDescription*	pAttachments;
		(deUint32)subpasses.size(),						// uint32_t							subpassCount;
		subpasses.data(),								// const VkSubpassDescription*		pSubpasses;
		(deUint32)dependencies.size(),					// uint32_t							dependencyCount;
		dependencies.data(),							// const VkSubpassDependency*		pDependencies;
	};

	return ::createRenderPass(m_vk, device, &renderPassCreateInfo);
}

void AttachmentAccessOrderTestInstance::createVertexBuffer()
{
	deUint32 primitiveNum = m_testCase->m_overlapPrimitives ? AttachmentAccessOrderTestCase::ELEM_NUM * 2: 2;
	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
	const VkDevice device = m_context.getDevice();
	Allocator& allocator = m_context.getDefaultAllocator();
	std::vector<tcu::Vec2> vbo(3*primitiveNum);
	for (deUint32 i=0; i < primitiveNum/2; i++)
	{
		vbo[i*6 + 0] = {-1, -1};
		vbo[i*6 + 1] = { 1, -1};
		vbo[i*6 + 2] = {-1,  1};
		vbo[i*6 + 3] = { 1,  1};
		vbo[i*6 + 4] = {-1,  1};
		vbo[i*6 + 5] = { 1, -1};
	}

	const size_t dataSize = vbo.size() * sizeof(tcu::Vec2);
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags;
			dataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};
		m_vertexBuffer = createBuffer(m_vk, device, &vertexBufferParams);
		m_vertexBufferMemory = allocator.allocate(getBufferMemoryRequirements(m_vk, device, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_vk.bindBufferMemory(device, *m_vertexBuffer, m_vertexBufferMemory->getMemory(), m_vertexBufferMemory->getOffset()));
	}

	/* Load vertices into vertex buffer */
	deMemcpy(m_vertexBufferMemory->getHostPtr(), vbo.data(), dataSize);
	flushAlloc(m_vk, device, *m_vertexBufferMemory);
}

void AttachmentAccessOrderTestInstance::createResultBuffer()
{
	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
	const VkDevice device = m_context.getDevice();
	Allocator& allocator = m_context.getDefaultAllocator();
	/* result buffer */
	const VkBufferCreateInfo					resultBufferInfo		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkBufferCreateFlags	flags;
		WIDTH * HEIGHT * sizeof(tcu::UVec2),	// VkDeviceSize			size;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyCount;
		&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
	};
	m_resultBuffer = createBuffer(m_vk, device, &resultBufferInfo);
	m_resultBufferMemory = allocator.allocate(	getBufferMemoryRequirements(m_vk, device, *m_resultBuffer), MemoryRequirement::HostVisible);

	VK_CHECK(m_vk.bindBufferMemory(device, *m_resultBuffer, m_resultBufferMemory->getMemory(), m_resultBufferMemory->getOffset()));
}

AttachmentAccessOrderTestInstance::AttachmentAccessOrderTestInstance( Context& context, const AttachmentAccessOrderTestCase *testCase)
	: TestInstance(context)
	, m_testCase (testCase)
	, m_vk (m_context.getDeviceInterface())
	, m_subpasses(2)
{
	const VkDevice device = m_context.getDevice();
	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

	m_descSetLayout = makeDescriptorSetLayout(m_vk, device, m_testCase->getInputAttachmentNum());

	m_descPool = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, m_testCase->getInputAttachmentNum())
		.build(m_vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descSet = makeDescriptorSet(m_vk, device, *m_descPool, *m_descSetLayout, nullptr);

	vector<VkImageView> attachmentHandles;
	VkDescriptorSetLayout dsetLayout = *m_descSetLayout;

	m_subpasses[0].createAttachments(	0, m_testCase->getInputAttachmentNum(), m_testCase->getColorAttachmentNum(), m_testCase->m_sampleCount,
										m_context, attachmentHandles, &dsetLayout, m_testCase);
	m_subpasses[1].createAttachments(1, 1, 1, VK_SAMPLE_COUNT_1_BIT, m_context, attachmentHandles, &dsetLayout, m_testCase);

	m_sampler = makeSampler(m_vk, device);

	writeDescriptorSets();
	m_renderPass = createRenderPass(m_testCase->getColorFormat());

	m_framebuffer = makeFramebuffer(m_vk, device, *m_renderPass, (deUint32)attachmentHandles.size(), attachmentHandles.data(), WIDTH, HEIGHT, 1);

	m_subpasses[0].createPipeline(*m_renderPass, m_context);
	m_subpasses[1].createPipeline(*m_renderPass, m_context);

	createVertexBuffer();

	createResultBuffer();

	m_cmdPool = createCommandPool(m_vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(m_vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

AttachmentAccessOrderTestInstance::~AttachmentAccessOrderTestInstance (void)
{
}

void AttachmentAccessOrderTestInstance::addPipelineBarrier(	VkCommandBuffer			cmdBuffer,
															VkImage					image,
															VkImageLayout			oldLayout,
															VkImageLayout			newLayout,
															VkAccessFlags			srcAccessMask,
															VkAccessFlags			dstAccessMask,
															VkPipelineStageFlags	srcStageMask,
															VkPipelineStageFlags	dstStageMask,
															VkImageAspectFlags		aspect)
{
	VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		srcAccessMask,								// VkAccessFlags			srcAccessMask;
		dstAccessMask,								// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{
			aspect,						//VkImageAspectFlags	aspectMask;
			0u,							//uint32_t				baseMipLevel;
			1u,							//uint32_t				levelCount;
			0u,							//uint32_t				baseArrayLayer;
			1u,							//uint32_t				layerCount;
		},											// VkImageSubresourceRange	subresourceRange;
	};

	m_vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL,
							0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &barrier);
}
void AttachmentAccessOrderTestInstance::addClearColor(VkCommandBuffer cmdBuffer, VkImage image)
{
	VkClearColorValue clearColor;
	clearColor.float32[0] = 0.0;
	clearColor.float32[1] = 0.0;
	clearColor.float32[2] = 0.0;
	clearColor.float32[3] = 1.0;

	const VkImageSubresourceRange subresourceRange =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask;
		0u,							//uint32_t				baseMipLevel;
		1u,							//uint32_t				levelCount;
		0u,							//uint32_t				baseArrayLayer;
		1u,							//uint32_t				layerCount;
	};

	m_vk.cmdClearColorImage(cmdBuffer, image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);
}

void AttachmentAccessOrderTestInstance::addClearDepthStencil(VkCommandBuffer cmdBuffer, VkImage image)
{
	VkClearDepthStencilValue clearValue;
	clearValue.depth = 0.0;
	clearValue.stencil = 0;

	const VkImageSubresourceRange subresourceRange =
	{
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	//VkImageAspectFlags	aspectMask;
		0u,															//uint32_t				baseMipLevel;
		1u,															//uint32_t				levelCount;
		0u,															//uint32_t				baseArrayLayer;
		1u,															//uint32_t				layerCount;
	};

	m_vk.cmdClearDepthStencilImage(cmdBuffer, image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &subresourceRange);
}


tcu::TestStatus AttachmentAccessOrderTestInstance::iterate (void)
{
	const VkQueue queue = m_context.getUniversalQueue();
	const VkDevice device = m_context.getDevice();

	beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

	for (deUint32 i=0; i < m_subpasses.size(); i++)
	{
		for (deUint32 j=0; j < m_subpasses[i].getColorAttachmentNum(); j++)
		{
			addPipelineBarrier(	*m_cmdBuffer, *m_subpasses[i].m_inputAtt[j], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
								0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			addClearColor( *m_cmdBuffer, *m_subpasses[i].m_inputAtt[j]);
		}
		for (deUint32 j=m_subpasses[i].getColorAttachmentNum(); j < m_subpasses[i].getInputAttachmentNum(); j++)
		{
			addPipelineBarrier(	*m_cmdBuffer, *m_subpasses[i].m_inputAtt[j], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
								0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
			addClearDepthStencil( *m_cmdBuffer, *m_subpasses[i].m_inputAtt[j]);
		}
	}

	const VkMemoryBarrier memBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	const VkRect2D renderArea = makeRect2D(WIDTH, HEIGHT);
	beginRenderPass(m_vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);

	const VkDeviceSize vertexBufferOffset = 0;
	const VkBuffer vertexBuffer = *m_vertexBuffer;

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpasses[0].m_pipeline);
	VkDescriptorSet dset = *m_descSet;
	m_vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpasses[0].m_pipelineLayout, 0, 1, &dset, 0, DE_NULL);

	deUint32 numDraws = m_testCase->m_overlapDraws ? AttachmentAccessOrderTestCase::ELEM_NUM : 1;
	deUint32 numPrimitives = m_testCase->m_overlapPrimitives ? 2 * AttachmentAccessOrderTestCase::ELEM_NUM : 2;
	deUint32 numInstances = m_testCase->m_overlapInstances ? AttachmentAccessOrderTestCase::ELEM_NUM : 1;

	for (deUint32 i=0; i < numDraws; i++)
	{
		m_vk.cmdPushConstants(	*m_cmdBuffer, *m_subpasses[0].m_pipelineLayout,
								VK_SHADER_STAGE_FRAGMENT_BIT | (m_testCase->hasDepthStencil() ? VK_SHADER_STAGE_VERTEX_BIT : 0),
								0, 4, &i);
		for (deUint32 j = 0; m_testCase->m_explicitSync && i != 0 && j < m_subpasses[0].getColorAttachmentNum(); j++)
		{
			addPipelineBarrier(	*m_cmdBuffer, *m_subpasses[0].m_inputAtt[j], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
								VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
								VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
								VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}
		for (deUint32 j = m_subpasses[0].getColorAttachmentNum(); m_testCase->m_explicitSync && i != 0 && j < m_subpasses[0].getInputAttachmentNum(); j++)
		{
			addPipelineBarrier(	*m_cmdBuffer, *m_subpasses[0].m_inputAtt[j], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
								VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
								VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
								VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		}
		m_vk.cmdDraw(*m_cmdBuffer, numPrimitives * 3, numInstances, 0, 0);
	}


	m_vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

	m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpasses[1].m_pipeline);

	m_vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpasses[1].m_pipelineLayout, 0, 1, &dset, 0, DE_NULL);

	m_vk.cmdDraw(*m_cmdBuffer, 6, 1, 0, 0);

	endRenderPass(m_vk, *m_cmdBuffer);

	copyImageToBuffer(	m_vk, *m_cmdBuffer, *m_subpasses[1].m_inputAtt[0], *m_resultBuffer, tcu::IVec2(WIDTH, HEIGHT), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						VK_IMAGE_LAYOUT_GENERAL);

	endCommandBuffer(m_vk, *m_cmdBuffer);

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	return validateResults(numDraws, numPrimitives, numInstances);
}

tcu::TestStatus AttachmentAccessOrderTestInstance::validateResults(deUint32 numDraws, deUint32 numPrimitives, deUint32 numInstances)
{
	const VkDevice device = m_context.getDevice();
	qpTestResult res = QP_TEST_RESULT_PASS;

	invalidateAlloc(m_vk, device, *m_resultBufferMemory);
	if (m_testCase->m_integerFormat)
	{
		tcu::UVec2 *resBuf = static_cast<tcu::UVec2 *> (m_resultBufferMemory->getHostPtr());

		for (deUint32 y = 0; y < HEIGHT && res == QP_TEST_RESULT_PASS; y ++)
		{
			for (deUint32 x = 0; x < WIDTH && res == QP_TEST_RESULT_PASS; x ++)
			{
				tcu::UVec2 pixel = resBuf[y * WIDTH + x];
				if (pixel[0] != 0 || pixel[1] != numDraws * numPrimitives/2 * numInstances)
				{
					res = QP_TEST_RESULT_FAIL;
				}
			}
		}
	}
	else
	{
		tcu::Vec2 *resBuf = static_cast<tcu::Vec2 *> (m_resultBufferMemory->getHostPtr());

		for (deUint32 y = 0; y < HEIGHT && res == QP_TEST_RESULT_PASS; y ++)
		{
			for (deUint32 x = 0; x < WIDTH && res == QP_TEST_RESULT_PASS; x ++)
			{
				tcu::Vec2 pixel = resBuf[y * WIDTH + x];
				if (pixel[0] != 0 || pixel[1] != (float)(numDraws * numPrimitives/2 * numInstances))
				{
					res = QP_TEST_RESULT_FAIL;
				}
			}
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}


} // anonymous ns


static void createRasterizationOrderAttachmentAccessTestVariations(	tcu::TestContext& testCtx, tcu::TestCaseGroup *gr,
																	const string &prefix_name, const string &prefix_desc,
																	deUint32 inputNum, bool integerFormat, bool depth, bool stencil)
{
	const struct
	{
		const std::string name;
		const std::string description;
		bool explicitSync;
		bool overlapDraws;
		bool overlapPrimitives;
		bool overlapInstances;
	} leafTestCreateParams[] =
	{
		{ "multi_draw_barriers",	"Basic test with overlapping draw commands with barriers",								true,  true,  false, false,	},
		{ "multi_draw",				"Test with overlapping draw commands without barriers",									false, true,  false, false,	},
		{ "multi_primitives",		"Test with a draw command with overlapping primitives",									false, false, true,  false,	},
		{ "multi_instances",		"Test with a draw command with overlapping instances",									false, false, false, true,	},
		{ "all",					"Test with overlapping draw commands, each with overlapping primitives and instances",	false, true,  true,  true,	},
	};
	constexpr deUint32 leafTestCreateParamsNum = sizeof(leafTestCreateParams) / sizeof(leafTestCreateParams[0]);

	VkSampleCountFlagBits sampleCountValues[] =
	{
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT,
	};
	constexpr deUint32 sampleCountValuesNum = sizeof(sampleCountValues) / sizeof(sampleCountValues[0]);

	for (deUint32 i = 0; i < sampleCountValuesNum ; i++)
	{
		stringstream name;
		stringstream desc;
		name << prefix_name << "samples_" << AttachmentAccessOrderTestCase::getSampleNum(sampleCountValues[i]);
		desc << prefix_desc << AttachmentAccessOrderTestCase::getSampleNum(sampleCountValues[i]) << " samples per pixel";
		tcu::TestCaseGroup *subgr = new tcu::TestCaseGroup(testCtx, name.str().c_str(), desc.str().c_str());

		for (deUint32 k = 0; k < leafTestCreateParamsNum; k++)
		{
			if (depth)
			{
				subgr->addChild(new AttachmentAccessOrderDepthTestCase(	testCtx, leafTestCreateParams[k].name,
																		leafTestCreateParams[k].description,
																		leafTestCreateParams[k].explicitSync,
																		leafTestCreateParams[k].overlapDraws,
																		leafTestCreateParams[k].overlapPrimitives,
																		leafTestCreateParams[k].overlapInstances,
																		sampleCountValues[i]));
			}
			else if (stencil)
			{
				subgr->addChild(new AttachmentAccessOrderStencilTestCase(	testCtx, leafTestCreateParams[k].name,
																			leafTestCreateParams[k].description,
																			leafTestCreateParams[k].explicitSync,
																			leafTestCreateParams[k].overlapDraws,
																			leafTestCreateParams[k].overlapPrimitives,
																			leafTestCreateParams[k].overlapInstances,
																			sampleCountValues[i]));
			}
			else
			{
				subgr->addChild(new AttachmentAccessOrderColorTestCase(	testCtx, leafTestCreateParams[k].name,
																		leafTestCreateParams[k].description,
																		leafTestCreateParams[k].explicitSync,
																		leafTestCreateParams[k].overlapDraws,
																		leafTestCreateParams[k].overlapPrimitives,
																		leafTestCreateParams[k].overlapInstances,
																		sampleCountValues[i], inputNum, integerFormat));
			}
		}
		gr->addChild(subgr);
	}
}

static void createRasterizationOrderAttachmentAccessFormatTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *gr, bool integerFormat)
{
	const deUint32 inputNum[] = {1, 4, 8};
	const deUint32 inputNumSize = sizeof(inputNum) / sizeof(inputNum[0]);

	tcu::TestCaseGroup *formatGr;

	if (integerFormat)
	{
		formatGr = new tcu::TestCaseGroup(testCtx, "format_integer", "Tests with an integer format" );
	}
	else
	{
		formatGr = new tcu::TestCaseGroup(testCtx, "format_float", "Tests with an float format" );
	}

	for (deUint32 i = 0; i < inputNumSize; i++)
	{
		stringstream numName;
		stringstream numDesc;
		numName << "attachments_" << inputNum[i] << "_";
		numDesc << "Tests with " << inputNum[i] << " attachments and ";
		createRasterizationOrderAttachmentAccessTestVariations(testCtx, formatGr, numName.str(), numDesc.str(), inputNum[i], integerFormat, false, false);
	}
	gr->addChild(formatGr);
}

tcu::TestCaseGroup* createRasterizationOrderAttachmentAccessTests(tcu::TestContext& testCtx)
{
	/* Add the color tests */
	tcu::TestCaseGroup *gr = new tcu::TestCaseGroup(testCtx, "rasterization_order_attachment_access", "Rasterization Order Attachment access tests");
	createRasterizationOrderAttachmentAccessFormatTests(testCtx, gr, false);
	createRasterizationOrderAttachmentAccessFormatTests(testCtx, gr, true);

	/* Add the D/S tests */
	tcu::TestCaseGroup *depth_gr = new tcu::TestCaseGroup(testCtx, "depth", "Tests depth rasterization order" );
	tcu::TestCaseGroup *stencil_gr = new tcu::TestCaseGroup(testCtx, "stencil", "Tests stencil rasterization order" );
	string name_prefix = "";
	string desc_prefix = "Tests with ";
	createRasterizationOrderAttachmentAccessTestVariations(testCtx, depth_gr, name_prefix, desc_prefix, 1, false, true, false);
	createRasterizationOrderAttachmentAccessTestVariations(testCtx, stencil_gr, name_prefix, desc_prefix, 1, false, false, true);
	gr->addChild(depth_gr);
	gr->addChild(stencil_gr);

	return gr;
}

} // rasterization
} // vkt
