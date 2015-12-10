#ifndef _VKTDRAWBASECLASS_HPP
#define _VKTDRAWBASECLASS_HPP
/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2015 The Khronos Group Inc.
* Copyright (c) 2015 Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and/or associated documentation files (the
* "Materials"), to deal in the Materials without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Materials, and to
* permit persons to whom the Materials are furnished to do so, subject to
* the following conditions:
*
* The above copyright notice(s) and this permission notice shall be included
* in all copies or substantial portions of the Materials.
*
* The Materials are Confidential Information as defined by the
* Khronos Membership Agreement until designated non-confidential by Khronos,
* at which point this condition clause shall be removed.
*
* THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*
*//*!
* \file
* \brief Command draw Tests - Base Class
*//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"

#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"

#include "deSharedPtr.hpp"

#include "vkPrograms.hpp"

#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"

namespace vkt
{
namespace Draw
{

inline tcu::Vec4 vec4Red(void)
{
	return tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

inline tcu::Vec4 vec4Green(void)
{
	return tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
}

inline tcu::Vec4 vec4Blue(void)
{
	return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

struct Vec4RGBA
{
	Vec4RGBA(tcu::Vec4 p, tcu::Vec4 c)
	: position	(p)
	, color		(c)
	{}
	tcu::Vec4 position;
	tcu::Vec4 color;
};

class DrawTestsBaseClass : public TestInstance
{
public:
								DrawTestsBaseClass	(Context &context, const char* vertexShaderName, const char* fragmentShaderName);

protected:
	void						initialize			(void);
	virtual void				initPipeline		(const vk::VkDevice device);
	void						beginRenderPass		(void);
	virtual tcu::TestStatus		iterate				(void)						{ TCU_FAIL("Implement iterate() method!");	}

	enum
	{
		WIDTH = 128,
		HEIGHT = 128
	};

	vk::VkFormat									m_colorAttachmentFormat;

	vk::VkPrimitiveTopology							m_topology;

	const vk::DeviceInterface&						m_vk;

	vk::Move<vk::VkPipeline>						m_pipeline;
	vk::Move<vk::VkPipelineLayout>					m_pipelineLayout;
	
	de::SharedPtr<Image>							m_colorTargetImage;
	vk::Move<vk::VkImageView>						m_colorTargetView;

	de::SharedPtr<Buffer>							m_vertexBuffer;
	PipelineCreateInfo::VertexInputState			m_vertexInputState;

	vk::Move<vk::VkCommandPool>						m_cmdPool;
	vk::Move<vk::VkCommandBuffer>					m_cmdBuffer;

	vk::Move<vk::VkFramebuffer>						m_framebuffer;
	vk::Move<vk::VkRenderPass>						m_renderPass;

	const std::string								m_vertexShaderName;
	const std::string								m_fragmentShaderName;

	std::vector<Vec4RGBA>							m_data;
	std::vector<deUint32>							m_indexes;
	de::SharedPtr<Buffer>							m_indexBuffer;
};

}	//Draw
}	//vkt

#endif	//_VKTDRAWBASECLASS_HPP
