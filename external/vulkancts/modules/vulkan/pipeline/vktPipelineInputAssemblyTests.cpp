/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Input Assembly Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineInputAssemblyTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMath.h"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

enum class RestartType
{
	NORMAL,
	NONE,
	ALL,
};

namespace
{

class InputAssemblyTest : public vkt::TestCase
{
public:
	const static VkPrimitiveTopology	s_primitiveTopologies[];
	const static deUint32				s_restartIndex32;
	const static deUint16				s_restartIndex16;
	const static deUint8				s_restartIndex8;

										InputAssemblyTest		(tcu::TestContext&					testContext,
																 const std::string&					name,
																 const std::string&					description,
																 const PipelineConstructionType		pipelineConstructionType,
																 VkPrimitiveTopology				primitiveTopology,
																 int								primitiveCount,
																 bool								testPrimitiveRestart,
																 VkIndexType						indexType);
	virtual								~InputAssemblyTest		(void) {}
	virtual void						initPrograms			(SourceCollections& sourceCollections) const;
	virtual void						checkSupport			(Context& context) const;
	virtual TestInstance*				createInstance			(Context& context) const;
	static bool							isRestartIndex			(VkIndexType indexType, deUint32 indexValue);
#ifndef CTS_USES_VULKANSC
	static deUint32						getRestartIndex			(VkIndexType indexType);
#endif // CTS_USES_VULKANSC

protected:
	virtual void						createBufferData		(VkPrimitiveTopology		topology,
																 int						primitiveCount,
																 VkIndexType				indexType,
																 std::vector<deUint32>&		indexData,
																 std::vector<Vertex4RGBA>&	vertexData) const = 0;
	VkPrimitiveTopology					m_primitiveTopology;
	const int							m_primitiveCount;

private:
	const PipelineConstructionType		m_pipelineConstructionType;
	bool								m_testPrimitiveRestart;
	VkIndexType							m_indexType;
};

class PrimitiveTopologyTest : public InputAssemblyTest
{
public:
										PrimitiveTopologyTest	(tcu::TestContext&			testContext,
																 const std::string&			name,
																 const std::string&			description,
																 PipelineConstructionType	pipelineConstructionType,
																 VkPrimitiveTopology		primitiveTopology,
																 VkIndexType				indexType);
	virtual								~PrimitiveTopologyTest	(void) {}

protected:
	virtual void						createBufferData		(VkPrimitiveTopology		topology,
																 int						primitiveCount,
																 VkIndexType				indexType,
																 std::vector<deUint32>&		indexData,
																 std::vector<Vertex4RGBA>&	vertexData) const;

private:
};

#ifndef CTS_USES_VULKANSC
class PrimitiveRestartTest : public InputAssemblyTest
{
public:
										PrimitiveRestartTest	(tcu::TestContext&					testContext,
																 const std::string&					name,
																 const std::string&					description,
																 const PipelineConstructionType		pipelineConstructionType,
																 VkPrimitiveTopology				primitiveTopology,
																 VkIndexType						indexType,
																 RestartType						restartType);
	virtual								~PrimitiveRestartTest	(void) {}
	virtual void						checkSupport			(Context& context) const;

protected:
	virtual void						createBufferData		(VkPrimitiveTopology		topology,
																 int						primitiveCount,
																 VkIndexType				indexType,
																 std::vector<deUint32>&		indexData,
																 std::vector<Vertex4RGBA>&	vertexData) const;

private:
	bool								isRestartPrimitive		(int primitiveIndex) const;
	void								createListPrimitives	(int						primitiveCount,
																 float						originX,
																 float						originY,
																 float						primitiveSizeX,
																 float						primitiveSizeY,
																 int						verticesPerPrimitive,
																 VkIndexType				indexType,
																 std::vector<deUint32>&		indexData,
																 std::vector<Vertex4RGBA>&	vertexData,
																 std::vector<deUint32>		adjacencies) const;

	std::vector<deUint32>				m_restartPrimitives;
	RestartType							m_restartType;
};
#endif // CTS_USES_VULKANSC


class InputAssemblyInstance : public vkt::TestInstance
{
public:
										InputAssemblyInstance	(Context&							context,
																 const PipelineConstructionType		pipelineConstructionType,
																 const VkPrimitiveTopology			primitiveTopology,
																 bool								testPrimitiveRestart,
																 VkIndexType						indexType,
																 const std::vector<deUint32>&		indexBufferData,
																 const std::vector<Vertex4RGBA>&	vertexBufferData);
	virtual								~InputAssemblyInstance	(void);
	virtual tcu::TestStatus				iterate					(void);

private:
	tcu::TestStatus						verifyImage				(void);
	void								uploadIndexBufferData16	(deUint16* destPtr, const std::vector<deUint32>& indexBufferData);
	void								uploadIndexBufferData8	(deUint8* destPtr, const std::vector<deUint32>& indexBufferData);

	VkPrimitiveTopology					m_primitiveTopology;
	bool								m_primitiveRestartEnable;
	VkIndexType							m_indexType;

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4RGBA>			m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkBuffer>						m_indexBuffer;
	std::vector<deUint32>				m_indices;
	de::MovePtr<Allocation>				m_indexBufferAlloc;

	const tcu::UVec2					m_renderSize;

	const VkFormat						m_colorFormat;
	VkImageCreateInfo					m_colorImageCreateInfo;
	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	RenderPassWrapper					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	ShaderWrapper						m_vertexShaderModule;
	ShaderWrapper						m_fragmentShaderModule;
	ShaderWrapper						m_tcsShaderModule;
	ShaderWrapper						m_tesShaderModule;

	PipelineLayoutWrapper				m_pipelineLayout;
	GraphicsPipelineWrapper				m_graphicsPipeline;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
};


// InputAssemblyTest

const VkPrimitiveTopology InputAssemblyTest::s_primitiveTopologies[] =
{
	VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
	VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
	VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
};

const deUint32 InputAssemblyTest::s_restartIndex32	= ~((deUint32)0u);
const deUint16 InputAssemblyTest::s_restartIndex16	= ~((deUint16)0u);
const deUint8 InputAssemblyTest::s_restartIndex8	= ~((deUint8)0u);

InputAssemblyTest::InputAssemblyTest (tcu::TestContext&					testContext,
									  const std::string&				name,
									  const std::string&				description,
									  const PipelineConstructionType	pipelineConstructionType,
									  VkPrimitiveTopology				primitiveTopology,
									  int								primitiveCount,
									  bool								testPrimitiveRestart,
									  VkIndexType						indexType)
	: vkt::TestCase				(testContext, name, description)
	, m_primitiveTopology		(primitiveTopology)
	, m_primitiveCount(primitiveCount)
	, m_pipelineConstructionType(pipelineConstructionType)
	, m_testPrimitiveRestart	(testPrimitiveRestart)
	, m_indexType				(indexType)
{
}

void InputAssemblyTest::checkSupport (Context& context) const
{
	if (m_indexType == VK_INDEX_TYPE_UINT8_EXT)
		context.requireDeviceFunctionality("VK_EXT_index_type_uint8");

	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
			break;

		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
			break;

		default:
			break;
	}

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
	if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
#endif // CTS_USES_VULKANSC
}

TestInstance* InputAssemblyTest::createInstance (Context& context) const
{
	std::vector<deUint32>		indexBufferData;
	std::vector<Vertex4RGBA>	vertexBufferData;

	createBufferData(m_primitiveTopology, m_primitiveCount, m_indexType, indexBufferData, vertexBufferData);

	return new InputAssemblyInstance(context, m_pipelineConstructionType, m_primitiveTopology, m_testPrimitiveRestart, m_indexType, indexBufferData, vertexBufferData);
}

void InputAssemblyTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream vertexSource;

	vertexSource <<
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		<< (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? "	gl_PointSize = 3.0;\n"
																	: "" )
		<< "	vtxColor = color;\n"
		"}\n";

	sourceCollections.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());

	sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n");

	sourceCollections.glslSources.add("color_tcs") << glu::TessellationControlSource(
		"#version 310 es\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(vertices = 3) out;\n"
		"layout(location = 0) in highp vec4 vtxColorIn[];\n"
		"layout(location = 0) out highp vec4 vtxColorOut[];\n"
		"#define ID gl_InvocationID\n"
		"void main (void)\n"
		"{\n"
		"	vtxColorOut[ID] = vtxColorIn[ID];\n"
		"	gl_out[ID].gl_Position = gl_in[ID].gl_Position;\n"
		"	if (ID == 0)\n"
		"	{\n"
		"		gl_TessLevelInner[0] = 5.0;\n"
		"		gl_TessLevelOuter[0] = 4.0;\n"
		"		gl_TessLevelOuter[1] = 5.0;\n"
		"		gl_TessLevelOuter[2] = 6.0;\n"
		"	}\n"
		"}\n");

	sourceCollections.glslSources.add("color_tes") << glu::TessellationEvaluationSource(
		"#version 310 es\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(triangles) in;\n"
		"layout(location = 0) in vec4 vtxColorIn[];\n"
		"layout(location = 0) out vec4 vtxColorOut;\n"
		"void main (void)\n"
		"{\n"
		"	vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
		"	vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
		"	vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
		"	gl_Position = p0 + p1 + p2;\n"
		"	vec4 q0 = gl_TessCoord.x * vtxColorIn[0];\n"
		"	vec4 q1 = gl_TessCoord.y * vtxColorIn[1];\n"
		"	vec4 q2 = gl_TessCoord.z * vtxColorIn[2];\n"
		"	vtxColorOut = q0 + q1 + q2;\n"
		"}\n");
}

bool InputAssemblyTest::isRestartIndex (VkIndexType indexType, deUint32 indexValue)
{
	if (indexType == VK_INDEX_TYPE_UINT16)
		return indexValue == s_restartIndex16;
	else if (indexType == VK_INDEX_TYPE_UINT8_EXT)
		return indexValue == s_restartIndex8;
	else
		return indexValue == s_restartIndex32;
}

#ifndef CTS_USES_VULKANSC
deUint32 InputAssemblyTest::getRestartIndex (VkIndexType indexType)
{
	if (indexType == VK_INDEX_TYPE_UINT16)
		return InputAssemblyTest::s_restartIndex16;
	else if (indexType == VK_INDEX_TYPE_UINT8_EXT)
		return InputAssemblyTest::s_restartIndex8;
	else
		return InputAssemblyTest::s_restartIndex32;
}
#endif // CTS_USES_VULKANSC

// PrimitiveTopologyTest

PrimitiveTopologyTest::PrimitiveTopologyTest (tcu::TestContext&			testContext,
											  const std::string&		name,
											  const std::string&		description,
											  PipelineConstructionType	pipelineConstructionType,
											  VkPrimitiveTopology		primitiveTopology,
											  VkIndexType				indexType)
	: InputAssemblyTest	(testContext, name, description, pipelineConstructionType, primitiveTopology, 10, false, indexType)
{
}

void PrimitiveTopologyTest::createBufferData (VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType, std::vector<deUint32>& indexData, std::vector<Vertex4RGBA>& vertexData) const
{
	DE_ASSERT(primitiveCount > 0);
	DE_UNREF(indexType);

	const tcu::Vec4				red						(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4				green					(0.0f, 1.0f, 0.0f, 1.0f);
	const float					border					= 0.2f;
	const float					originX					= -1.0f + border;
	const float					originY					= -1.0f + border;
	const Vertex4RGBA			defaultVertex			= { tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), green };
	float						primitiveSizeY			= (2.0f - 2.0f * border);
	float						primitiveSizeX;
	std::vector<deUint32>		indices;
	std::vector<Vertex4RGBA>	vertices;


	// Calculate primitive size
	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2 - 1);
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount - 1);
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount + primitiveCount / 2 + primitiveCount % 2 - 1);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			primitiveSizeX = 1.0f - border;
			primitiveSizeY = 1.0f - border;
			break;

		default:
			primitiveSizeX = 0.0f; // Garbage
			DE_ASSERT(false);
	}

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				const Vertex4RGBA vertex =
				{
					tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX, originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
					red
				};

				vertices.push_back(vertex);
				indices.push_back(primitiveNdx);
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx * 2 + vertexNdx) / 2) * primitiveSizeX, originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((primitiveNdx * 2 + vertexNdx));
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (primitiveNdx == 0)
				{
					Vertex4RGBA vertex =
					{
						tcu::Vec4(originX, originY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(0);

					vertex.position = tcu::Vec4(originX, originY + primitiveSizeY, 0.0f, 1.0f);
					vertices.push_back(vertex);
					indices.push_back(1);
				}
				else
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX, originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx + 1);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx * 3 + vertexNdx) / 2) * primitiveSizeX, originY + float((primitiveNdx * 3 + vertexNdx)% 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx * 3 + vertexNdx);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (primitiveNdx == 0)
				{
					for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
					{
						const Vertex4RGBA vertex =
						{
							tcu::Vec4(originX + float(vertexNdx / 2) * primitiveSizeX, originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
							red
						};

						vertices.push_back(vertex);
						indices.push_back(vertexNdx);
					}
				}
				else
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX, originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx + 2);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			const float stepAngle = de::min(DE_PI * 0.5f, (2 * DE_PI) / float(primitiveCount));

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (primitiveNdx == 0)
				{
					Vertex4RGBA vertex =
					{
						tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(0);

					vertex.position = tcu::Vec4(primitiveSizeX, 0.0f, 0.0f, 1.0f);
					vertices.push_back(vertex);
					indices.push_back(1);

					vertex.position = tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle), primitiveSizeY * deFloatSin(stepAngle), 0.0f, 1.0f);
					vertices.push_back(vertex);
					indices.push_back(2);
				}
				else
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx + 1)), primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx + 1)), 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx + 2);
				}
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				indices.push_back(0);

				for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx * 2 + vertexNdx) / 2) * primitiveSizeX, originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx * 2 + vertexNdx + 1);
				}

				indices.push_back(0);
			}
			break;


		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);
			indices.push_back(0);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (primitiveNdx == 0)
				{
					Vertex4RGBA vertex =
					{
						tcu::Vec4(originX, originY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(1);

					vertex.position = tcu::Vec4(originX, originY + primitiveSizeY, 0.0f, 1.0f);
					vertices.push_back(vertex);
					indices.push_back(2);
				}
				else
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX, originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx + 2);
				}
			}

			indices.push_back(0);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx * 3 + vertexNdx) / 2) * primitiveSizeX, originY + float((primitiveNdx * 3 + vertexNdx)% 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx * 3 + vertexNdx + 1);
					indices.push_back(0);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (primitiveNdx == 0)
				{
					for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
					{
						const Vertex4RGBA vertex =
						{
							tcu::Vec4(originX + float(vertexNdx / 2) * primitiveSizeX, originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
							red
						};

						vertices.push_back(vertex);
						indices.push_back(vertexNdx + 1);
						indices.push_back(0);
					}
				}
				else
				{
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX, originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back(primitiveNdx + 2 + 1);
					indices.push_back(0);
				}
			}
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	vertexData	= vertices;
	indexData	= indices;
}

#ifndef CTS_USES_VULKANSC
// PrimitiveRestartTest

PrimitiveRestartTest::PrimitiveRestartTest (tcu::TestContext&			testContext,
											const std::string&			name,
											const std::string&			description,
											PipelineConstructionType	pipelineConstructionType,
											VkPrimitiveTopology			primitiveTopology,
											VkIndexType					indexType,
											RestartType					restartType)

	: InputAssemblyTest	(testContext, name, description, pipelineConstructionType, primitiveTopology, 10, true, indexType)
	, m_restartType(restartType)
{
	deUint32 restartPrimitives[] = { 1, 5 };

	if (restartType == RestartType::NORMAL)
	{
		m_restartPrimitives = std::vector<deUint32>(restartPrimitives, restartPrimitives + sizeof(restartPrimitives) / sizeof(deUint32));
	}
	else if (restartType == RestartType::NONE)
	{
		m_restartPrimitives = std::vector<deUint32>{};
	}
	else
	{
		deUint32 count = 1;
		switch (primitiveTopology)
		{
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
				count = 2;
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
				count = 3;
				break;
			default:
				break;
		}
		for (deUint32 i = 0; i < (deUint32)m_primitiveCount; ++i)
		{
			if (i % count == count - 1)
			{
				m_restartPrimitives.push_back(i);
			}
		}
	}
}

void PrimitiveRestartTest::checkSupport (Context& context) const
{
	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			context.requireDeviceFunctionality("VK_EXT_primitive_topology_list_restart");

			const auto& features = context.getPrimitiveTopologyListRestartFeaturesEXT();
			if (!features.primitiveTopologyListRestart)
				TCU_THROW(NotSupportedError, "Primitive topology list restart feature not supported");
			if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && !features.primitiveTopologyPatchListRestart)
				TCU_THROW(NotSupportedError, "Primitive topology patch list restart feature not supported");
		}
		break;

		default:
		break;
	}

	InputAssemblyTest::checkSupport(context);
}

void PrimitiveRestartTest::createBufferData (VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType, std::vector<deUint32>& indexData, std::vector<Vertex4RGBA>& vertexData) const
{
	DE_ASSERT(primitiveCount > 0);
	DE_UNREF(indexType);

	const tcu::Vec4				red						(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4				green					(0.0f, 1.0f, 0.0f, 1.0f);
	const float					border					= 0.2f;
	const float					originX					= -1.0f + border;
	const float					originY					= -1.0f + border;
	const Vertex4RGBA			defaultVertex			= { tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), green };
	float						primitiveSizeY			= (2.0f - 2.0f * border);
	float						primitiveSizeX;
	bool						primitiveStart			= true;
	std::vector<deUint32>		indices;
	std::vector<Vertex4RGBA>	vertices;

	// Calculate primitive size
	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2 - 1);
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
			primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			primitiveSizeX = 1.0f - border;
			primitiveSizeY = 1.0f - border;
			break;

		default:
			primitiveSizeX = 0.0f; // Garbage
			DE_ASSERT(false);
	}

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (isRestartPrimitive(primitiveNdx))
				{
					indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
					primitiveStart = true;
				}
				else
				{
					if (primitiveStart && m_restartType != RestartType::ALL)
					{
						const Vertex4RGBA vertex =
						{
							tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX, originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
							red
						};

						vertices.push_back(vertex);
						indices.push_back((deUint32)vertices.size() - 1);

						primitiveStart = false;
					}

					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX, originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((deUint32)vertices.size() - 1);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (isRestartPrimitive(primitiveNdx))
				{
					indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
					primitiveStart = true;
				}
				else
				{
					if (primitiveStart && m_restartType != RestartType::ALL)
					{
						for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
						{
							const Vertex4RGBA vertex =
							{
								tcu::Vec4(originX + float((primitiveNdx + vertexNdx) / 2) * primitiveSizeX, originY + float((primitiveNdx + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
								red
							};

							vertices.push_back(vertex);
							indices.push_back((deUint32)vertices.size() - 1);
						}

						primitiveStart = false;
					}
					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX, originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((deUint32)vertices.size() - 1);
				}
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			const float stepAngle = de::min(DE_PI * 0.5f, (2 * DE_PI) / float(primitiveCount));

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (isRestartPrimitive(primitiveNdx))
				{
					indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
					primitiveStart = true;
				}
				else
				{
					if (primitiveStart && m_restartType != RestartType::ALL)
					{
						Vertex4RGBA vertex =
						{
							tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
							red
						};

						vertices.push_back(vertex);
						indices.push_back((deUint32)vertices.size() - 1);

						vertex.position = tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx)), primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx)), 0.0f, 1.0f);
						vertices.push_back(vertex);
						indices.push_back((deUint32)vertices.size() - 1);

						primitiveStart = false;
					}

					const Vertex4RGBA vertex =
					{
						tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx + 1)), primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx + 1)), 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((deUint32)vertices.size() - 1);
				}
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (isRestartPrimitive(primitiveNdx))
				{
					indices.push_back(0);
					indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
					primitiveStart = true;
				}
				else
				{
					if (primitiveStart && m_restartType != RestartType::ALL)
					{
						indices.push_back(0);

						const Vertex4RGBA vertex =
						{
							tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX, originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
							red
						};

						vertices.push_back(vertex);
						indices.push_back((deUint32)vertices.size() - 1);

						primitiveStart = false;
					}

					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX, originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((deUint32)vertices.size() - 1);
				}
			}

			indices.push_back(0);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vertices.push_back(defaultVertex);

			for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
			{
				if (isRestartPrimitive(primitiveNdx))
				{
					indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
					primitiveStart = true;
				}
				else
				{
					if (primitiveStart && m_restartType != RestartType::ALL)
					{
						for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
						{
							const Vertex4RGBA vertex =
							{
								tcu::Vec4(originX + float((primitiveNdx + vertexNdx) / 2) * primitiveSizeX, originY + float((primitiveNdx + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
								red
							};

							vertices.push_back(vertex);
							indices.push_back((deUint32)vertices.size() - 1);
							indices.push_back(0);
						}

						primitiveStart = false;
					}

					const Vertex4RGBA vertex =
					{
						tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX, originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f, 1.0f),
						red
					};

					vertices.push_back(vertex);
					indices.push_back((deUint32)vertices.size() - 1);
					indices.push_back(0);
				}
			}
			break;

		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY,
								 1, indexType, indices, vertices, std::vector<deUint32>());
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY,
								 2, indexType, indices, vertices, std::vector<deUint32>());
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		{
			std::vector<deUint32> adjacencies = { 0, 3 };

			createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY,
								 4, indexType, indices, vertices, adjacencies);
		}
		break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
			createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY,
						3, indexType, indices, vertices, std::vector<deUint32>());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			std::vector<deUint32> adjacencies = { 1, 3, 5 };

			createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY,
								 6, indexType, indices, vertices, adjacencies);
		}
		break;

		default:
			DE_ASSERT(false);
			break;
	}

	vertexData	= vertices;
	indexData	= indices;
}

void PrimitiveRestartTest::createListPrimitives (int						primitiveCount,
												 float						originX,
												 float						originY,
												 float						primitiveSizeX,
												 float						primitiveSizeY,
												 int						verticesPerPrimitive,
												 VkIndexType				indexType,
												 std::vector<deUint32>&		indexData,
												 std::vector<Vertex4RGBA>&	vertexData,
												 std::vector<deUint32>		adjacencies) const
{
	const tcu::Vec4	red		(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	green	(0.0f, 1.0f, 0.0f, 1.0f);
	// Tells which vertex of a primitive is used as a restart index.
	// This is decreased each time a restart primitive is used.
	int restartVertexIndex = verticesPerPrimitive - 1;

	for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
	{
		deUint32 nonAdjacentVertexNdx = 0;

		for (int vertexNdx = 0; vertexNdx < verticesPerPrimitive; vertexNdx++)
		{
			if (isRestartPrimitive(primitiveNdx) && vertexNdx == restartVertexIndex)
			{
				indexData.push_back(InputAssemblyTest::getRestartIndex(indexType));

				restartVertexIndex--;
				if (restartVertexIndex < 0) restartVertexIndex = verticesPerPrimitive - 1;

				break;
			}

			if (std::find(adjacencies.begin(), adjacencies.end(), vertexNdx) != adjacencies.end())
			{
				// This is an adjacency vertex index. Add a green vertex that should never end up to the framebuffer.
				const Vertex4RGBA vertex =
				{
					tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
					green
				};
				vertexData.push_back(vertex);
				indexData.push_back((deUint32) vertexData.size() - 1);
				continue;
			}

			const Vertex4RGBA vertex =
			{
				tcu::Vec4(originX + float((primitiveNdx + nonAdjacentVertexNdx) / 2) * primitiveSizeX,
						  originY + float((primitiveNdx + nonAdjacentVertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
						  red
			};

			vertexData.push_back(vertex);
			indexData.push_back((deUint32) vertexData.size() - 1);
			nonAdjacentVertexNdx++;
		}
	}
}

bool PrimitiveRestartTest::isRestartPrimitive (int primitiveIndex) const
{
	return std::find(m_restartPrimitives.begin(), m_restartPrimitives.end(), primitiveIndex) != m_restartPrimitives.end();
}
#endif // CTS_USES_VULKANSC

// InputAssemblyInstance

InputAssemblyInstance::InputAssemblyInstance (Context&							context,
											  PipelineConstructionType			pipelineConstructionType,
											  VkPrimitiveTopology				primitiveTopology,
											  bool								testPrimitiveRestart,
											  VkIndexType						indexType,
											  const std::vector<deUint32>&		indexBufferData,
											  const std::vector<Vertex4RGBA>&	vertexBufferData)

	: vkt::TestInstance			(context)
	, m_primitiveTopology		(primitiveTopology)
	, m_primitiveRestartEnable	(testPrimitiveRestart)
	, m_indexType				(indexType)
	, m_vertices				(vertexBufferData)
	, m_indices					(indexBufferData)
	, m_renderSize				((primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) ? tcu::UVec2(32, 32) : tcu::UVec2(64, 16))
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_graphicsPipeline		(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
{
	const DeviceInterface&			vk						= context.getDeviceInterface();
	const VkDevice					vkDevice				= context.getDevice();
	const deUint32					queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator					memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const bool						patchList				= m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		m_colorImageCreateInfo	= colorImageParams;
		m_colorImage			= createImage(vk, vkDevice, &m_colorImageCreateInfo);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*m_colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_colorFormat,										// VkFormat					format;
			componentMappingRGBA,								// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat);

	// Create framebuffer
	{
		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			1u,													// deUint32					attachmentCount;
			&m_colorAttachmentView.get(),						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32					width;
			(deUint32)m_renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	if (patchList)
	{
		m_tcsShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_tcs"), 0);
		m_tesShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_tes"), 0);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,								// deUint32					binding;
			sizeof(Vertex4RGBA),			// deUint32					stride;
			VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputRate		inputRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offset;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType;
			DE_NULL,														// const void*									pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags		flags;
			1u,																// deUint32										vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
			2u,																// deUint32										vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
			m_primitiveTopology,											// VkPrimitiveTopology						topology;
			m_primitiveRestartEnable										// VkBool32									primitiveRestartEnable;
		};

		const std::vector<VkViewport>	viewport	{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>		scissor		{ makeRect2D(m_renderSize) };

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,															// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,												// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_ONE,											// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,												// VkBlendOp				alphaBlendOp;
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |			// VkColorComponentFlags	colorWriteMask;
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			false,														// VkBool32									depthTestEnable;
			false,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			false,														// VkBool32									depthBoundsTestEnable;
			false,														// VkBool32									stencilTestEnable;
			// VkStencilOpState	front;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			// VkStencilOpState	back;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			0.0f,														// float			minDepthBounds;
			1.0f														// float			maxDepthBounds;
		};

		m_graphicsPipeline.setDefaultRasterizationState()
						  .setDefaultMultisampleState()
						  .setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
						  .setupPreRasterizationShaderState(viewport,
											scissor,
											m_pipelineLayout,
											*m_renderPass,
											0u,
											m_vertexShaderModule,
											DE_NULL,
											m_tcsShaderModule,
											m_tesShaderModule)
						  .setupFragmentShaderState(m_pipelineLayout,
											*m_renderPass,
											0u,
											m_fragmentShaderModule,
											&depthStencilStateParams)
						  .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
						  .setMonolithicPipelineLayout(m_pipelineLayout)
						  .buildPipeline();
	}

	// Create vertex and index buffer
	{
		const VkBufferCreateInfo indexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_indices.size() * sizeof(deUint32),		// VkDeviceSize			size;
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_vertices.size() * sizeof(Vertex4RGBA),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_indexBuffer		= createBuffer(vk, vkDevice, &indexBufferParams);
		m_indexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_indexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_indexBuffer, m_indexBufferAlloc->getMemory(), m_indexBufferAlloc->getOffset()));

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into index buffer
		if (m_indexType == VK_INDEX_TYPE_UINT32)
		{
			deMemcpy(m_indexBufferAlloc->getHostPtr(), m_indices.data(), m_indices.size() * sizeof(deUint32));
		}
		else if (m_indexType == VK_INDEX_TYPE_UINT8_EXT)
		{
			uploadIndexBufferData8((deUint8*)m_indexBufferAlloc->getHostPtr(), m_indices);
		}
		else // m_indexType == VK_INDEX_TYPE_UINT16
		{
			uploadIndexBufferData16((deUint16*)m_indexBufferAlloc->getHostPtr(), m_indices);
		}

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));

		flushAlloc(vk, vkDevice, *m_indexBufferAlloc);
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		const VkImageMemoryBarrier attachmentLayoutBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			*m_colorImage,									// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, 1u, &attachmentLayoutBarrier);

		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);

		const VkDeviceSize vertexBufferOffset = 0;

		m_graphicsPipeline.bind(*m_cmdBuffer);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indexBuffer, 0, m_indexType);
		vk.cmdDrawIndexed(*m_cmdBuffer, (deUint32)m_indices.size(), 1, 0, 0, 0);

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

InputAssemblyInstance::~InputAssemblyInstance (void)
{
}

tcu::TestStatus InputAssemblyInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus InputAssemblyInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat		= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuStencilFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader		(tcuColorFormat, tcuStencilFormat);
	const rr::Program			program				(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer			(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuStencilFormat, &program);
	bool						compareOk			= false;

	// Render reference image
	{
		// The reference for tessellated patches are drawn using ordinary triangles.
		const rr::PrimitiveType		topology	= m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST ? rr::PrimitiveType::PRIMITIVETYPE_TRIANGLES : mapVkPrimitiveTopology(m_primitiveTopology);
		rr::RenderState				renderState	(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits);

		if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
			renderState.point.pointSize = 3.0f;

		if (m_primitiveRestartEnable)
		{
			std::vector<deUint32> indicesRange;

			for (size_t indexNdx = 0; indexNdx < m_indices.size(); indexNdx++)
			{
				const bool isRestart = InputAssemblyTest::isRestartIndex(m_indexType, m_indices[indexNdx]);

				if (!isRestart)
					indicesRange.push_back(m_indices[indexNdx]);

				if (isRestart || indexNdx == (m_indices.size() - 1))
				{
					// Draw the range of indices found so far

					std::vector<Vertex4RGBA> nonIndexedVertices;
					for (size_t i = 0; i < indicesRange.size(); i++)
						nonIndexedVertices.push_back(m_vertices[indicesRange[i]]);

					refRenderer.draw(renderState, topology, nonIndexedVertices);
					indicesRange.clear();
				}
			}
		}
		else
		{
			std::vector<Vertex4RGBA> nonIndexedVertices;
			for (size_t i = 0; i < m_indices.size(); i++)
				nonIndexedVertices.push_back(m_vertices[m_indices[i]]);

			refRenderer.draw(renderState, topology, nonIndexedVertices);
		}
	}

	// Compare result with reference image
	{
		const DeviceInterface&				vk					= m_context.getDeviceInterface();
		const VkDevice						vkDevice			= m_context.getDevice();
		const VkQueue						queue				= m_context.getUniversalQueue();
		const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator						allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::UniquePtr<tcu::TextureLevel>	result				(readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize).release());

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

void InputAssemblyInstance::uploadIndexBufferData16	(deUint16* destPtr, const std::vector<deUint32>& indexBufferData)
{
	for (size_t i = 0; i < indexBufferData.size(); i++)
	{
		DE_ASSERT(indexBufferData[i] <= 0xFFFF);
		destPtr[i] = (deUint16)indexBufferData[i];
	}
}

void InputAssemblyInstance::uploadIndexBufferData8	(deUint8* destPtr, const std::vector<deUint32>& indexBufferData)
{
	for (size_t i = 0; i < indexBufferData.size(); i++)
	{
		DE_ASSERT(indexBufferData[i] <= 0xFF);
		destPtr[i] = (deUint8)indexBufferData[i];
	}
}


// Utilities for test names

std::string getPrimitiveTopologyCaseName (VkPrimitiveTopology topology)
{
	const std::string  fullName = getPrimitiveTopologyName(topology);

	DE_ASSERT(de::beginsWith(fullName, "VK_PRIMITIVE_TOPOLOGY_"));

	return de::toLower(fullName.substr(22));
}

de::MovePtr<tcu::TestCaseGroup> createPrimitiveTopologyTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> primitiveTopologyTests (new tcu::TestCaseGroup(testCtx, "primitive_topology", ""));

	de::MovePtr<tcu::TestCaseGroup> indexUint16Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint16", ""));
	de::MovePtr<tcu::TestCaseGroup> indexUint32Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint32", ""));
	de::MovePtr<tcu::TestCaseGroup> indexUint8Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint8", ""));

	for (int topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(InputAssemblyTest::s_primitiveTopologies); topologyNdx++)
	{
		const VkPrimitiveTopology topology = InputAssemblyTest::s_primitiveTopologies[topologyNdx];

		indexUint16Tests->addChild(new PrimitiveTopologyTest(testCtx,
															 getPrimitiveTopologyCaseName(topology),
															 "",
															 pipelineConstructionType,
															 topology,
															 VK_INDEX_TYPE_UINT16));

		indexUint32Tests->addChild(new PrimitiveTopologyTest(testCtx,
															 getPrimitiveTopologyCaseName(topology),
															 "",
															 pipelineConstructionType,
															 topology,
															 VK_INDEX_TYPE_UINT32));

		indexUint8Tests->addChild(new PrimitiveTopologyTest(testCtx,
															 getPrimitiveTopologyCaseName(topology),
															 "",
															 pipelineConstructionType,
															 topology,
															 VK_INDEX_TYPE_UINT8_EXT));
	}

	primitiveTopologyTests->addChild(indexUint16Tests.release());
	primitiveTopologyTests->addChild(indexUint32Tests.release());
	primitiveTopologyTests->addChild(indexUint8Tests.release());

	return primitiveTopologyTests;
}

#ifndef CTS_USES_VULKANSC
de::MovePtr<tcu::TestCaseGroup> createPrimitiveRestartTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	const VkPrimitiveTopology primitiveRestartTopologies[] =
	{
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,

		// Supported with VK_EXT_primitive_topology_list_restart
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
		VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	};

	de::MovePtr<tcu::TestCaseGroup> primitiveRestartTests (new tcu::TestCaseGroup(testCtx, "primitive_restart", "Restarts indices of "));

	de::MovePtr<tcu::TestCaseGroup> indexUint16Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint16", ""));
	de::MovePtr<tcu::TestCaseGroup> indexUint32Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint32", ""));
	de::MovePtr<tcu::TestCaseGroup> indexUint8Tests (new tcu::TestCaseGroup(testCtx, "index_type_uint8", ""));

	constexpr struct RestartTest
	{
		RestartType	type;
		const char* name;
	} restartTypes[] =
	{
		{ RestartType::NORMAL,	"",					},
		{ RestartType::NONE,	"no_restart_",		},
		{ RestartType::ALL,		"restart_all_"		},
	};

	for (int topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(primitiveRestartTopologies); topologyNdx++)
	{
		const VkPrimitiveTopology topology = primitiveRestartTopologies[topologyNdx];

		for (int useRestartNdx = 0; useRestartNdx < DE_LENGTH_OF_ARRAY(restartTypes); useRestartNdx++)
		{
			if (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && restartTypes[useRestartNdx].type == RestartType::ALL) {
				continue;
			}
			indexUint16Tests->addChild(new PrimitiveRestartTest(testCtx,
																restartTypes[useRestartNdx].name + getPrimitiveTopologyCaseName(topology),
																"",
																pipelineConstructionType,
																topology,
																VK_INDEX_TYPE_UINT16,
																restartTypes[useRestartNdx].type));

			indexUint32Tests->addChild(new PrimitiveRestartTest(testCtx,
																restartTypes[useRestartNdx].name + getPrimitiveTopologyCaseName(topology),
																"",
																pipelineConstructionType,
																topology,
																VK_INDEX_TYPE_UINT32,
																restartTypes[useRestartNdx].type));

			indexUint8Tests->addChild(new PrimitiveRestartTest(testCtx,
																restartTypes[useRestartNdx].name + getPrimitiveTopologyCaseName(topology),
																"",
																pipelineConstructionType,
																topology,
																VK_INDEX_TYPE_UINT8_EXT,
																restartTypes[useRestartNdx].type));
		}
	}

	// Tests that have primitive restart disabled, but have indices with restart index value.
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		struct
		{
			std::string	name;
			std::vector<std::string> requirements;
		} tests[] =
		{
			{ "line_list", { "VK_EXT_primitive_topology_list_restart" } },
			{ "line_list_with_adjacency", { "Features.geometryShader", "VK_EXT_primitive_topology_list_restart" } },
			{ "line_strip", {} },
			{ "line_strip_with_adjacency", {"Features.geometryShader"} },
			{ "patch_list", { "VK_EXT_primitive_topology_list_restart", "Features.tessellationShader" } },
			{ "point_list", { "VK_EXT_primitive_topology_list_restart" } },
			{ "triangle_fan", {} },
			{ "triangle_list", { "VK_EXT_primitive_topology_list_restart" } },
			{ "triangle_list_with_adjacency", { "Features.geometryShader", "VK_EXT_primitive_topology_list_restart" } },
			{ "triangle_strip", {} },
			{ "triangle_strip_with_adjacency", {"Features.geometryShader"} }
		};

		const std::string dataDir = "pipeline/input_assembly/primitive_restart";

		for (auto& test : tests)
		{
			std::string testName = "restart_disabled_" + test.name;
			indexUint16Tests->addChild(cts_amber::createAmberTestCase(testCtx, testName.c_str(), "", dataDir.c_str(), testName + "_uint16.amber", test.requirements));
			test.requirements.push_back("VK_EXT_index_type_uint8");
			indexUint8Tests->addChild(cts_amber::createAmberTestCase(testCtx, testName.c_str(), "", dataDir.c_str(), testName + "_uint8.amber", test.requirements));
		}
	}

	primitiveRestartTests->addChild(indexUint16Tests.release());
	primitiveRestartTests->addChild(indexUint32Tests.release());
	primitiveRestartTests->addChild(indexUint8Tests.release());

	return primitiveRestartTests;
}
#endif // CTS_USES_VULKANSC

} // anonymous

tcu::TestCaseGroup* createInputAssemblyTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup>		inputAssemblyTests (new tcu::TestCaseGroup(testCtx, "input_assembly", "Input assembly tests"));

	inputAssemblyTests->addChild(createPrimitiveTopologyTests(testCtx, pipelineConstructionType).release());
#ifndef CTS_USES_VULKANSC
	inputAssemblyTests->addChild(createPrimitiveRestartTests(testCtx, pipelineConstructionType).release());
#endif // CTS_USES_VULKANSC

	return inputAssemblyTests.release();
}

} // pipeline
} // vkt
