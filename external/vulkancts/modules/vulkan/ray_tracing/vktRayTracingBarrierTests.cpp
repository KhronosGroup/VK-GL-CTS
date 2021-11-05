/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Ray Tracing barrier tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBarrierTests.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkQueryUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deUniquePtr.hpp"

#include <string>
#include <sstream>
#include <memory>
#include <numeric>
#include <vector>
#include <algorithm>

namespace vkt
{
namespace RayTracing
{

namespace
{

using namespace vk;

constexpr deUint32				kBufferElements	= 1024u;
constexpr deUint32				kBufferSize		= kBufferElements * static_cast<deUint32>(sizeof(tcu::UVec4));	// std140
constexpr deUint32				kBufferSize430	= kBufferElements * static_cast<deUint32>(sizeof(deUint32));	// std430
constexpr deUint32				kValuesOffset	= 2048u;
constexpr deUint32				kImageDim		= 32u;					// So that kImageDim*kImageDim == kBufferElements.
constexpr VkFormat				kImageFormat	= VK_FORMAT_R32_UINT;	// So that each pixel has the same size as a deUint32.
const auto						kImageExtent	= makeExtent3D(kImageDim, kImageDim, 1u);
const std::vector<tcu::Vec4>	kFullScreenQuad	=
{
	tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
	tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f),
	tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
	tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
	tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
	tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
};


enum class Stage
{
	HOST = 0,
	TRANSFER,
	RAYGEN,
	INTERSECT,
	ANY_HIT,
	CLOSEST_HIT,
	MISS,
	CALLABLE,
	COMPUTE,
	FRAGMENT,
};

VkImageLayout getOptimalReadLayout (Stage stage)
{
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

	switch (stage)
	{
	case Stage::HOST:
		break; // Images will not be read directly from the host.
	case Stage::TRANSFER:
		layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		break;
	case Stage::RAYGEN:
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
	case Stage::COMPUTE:
	case Stage::FRAGMENT:
		layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return layout;
}

VkPipelineStageFlagBits getPipelineStage (Stage stage)
{
	VkPipelineStageFlagBits bits = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

	switch (stage)
	{
	case Stage::HOST:
		bits = VK_PIPELINE_STAGE_HOST_BIT;
		break;
	case Stage::TRANSFER:
		bits = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case Stage::RAYGEN:
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
		bits = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
		break;
	case Stage::COMPUTE:
		bits = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case Stage::FRAGMENT:
		bits = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return bits;
}

VkAccessFlagBits getWriterAccessFlag (Stage stage)
{
	VkAccessFlagBits bits = VK_ACCESS_FLAG_BITS_MAX_ENUM;

	switch (stage)
	{
	case Stage::HOST:
		bits = VK_ACCESS_HOST_WRITE_BIT;
		break;
	case Stage::TRANSFER:
		bits = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	case Stage::RAYGEN:
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
	case Stage::COMPUTE:
	case Stage::FRAGMENT:
		bits = VK_ACCESS_SHADER_WRITE_BIT;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return bits;
}

VkAccessFlagBits getReaderAccessFlag (Stage stage, VkDescriptorType resourceType)
{
	VkAccessFlagBits bits = VK_ACCESS_FLAG_BITS_MAX_ENUM;

	switch (stage)
	{
	case Stage::HOST:
		bits = VK_ACCESS_HOST_READ_BIT;
		break;
	case Stage::TRANSFER:
		bits = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case Stage::RAYGEN:
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
	case Stage::COMPUTE:
	case Stage::FRAGMENT:
		bits = ((resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? VK_ACCESS_UNIFORM_READ_BIT : VK_ACCESS_SHADER_READ_BIT);
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return bits;
}

// Translate a stage to the corresponding single stage flag.
VkShaderStageFlagBits getShaderStageFlagBits (Stage stage)
{
	VkShaderStageFlagBits bits = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

	switch (stage)
	{
	case Stage::RAYGEN:			bits = VK_SHADER_STAGE_RAYGEN_BIT_KHR;			break;
	case Stage::INTERSECT:		bits = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;	break;
	case Stage::ANY_HIT:		bits = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;			break;
	case Stage::CLOSEST_HIT:	bits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
	case Stage::MISS:			bits = VK_SHADER_STAGE_MISS_BIT_KHR;			break;
	case Stage::CALLABLE:		bits = VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
	case Stage::COMPUTE:		bits = VK_SHADER_STAGE_COMPUTE_BIT;				break;
	case Stage::FRAGMENT:		bits = VK_SHADER_STAGE_FRAGMENT_BIT;			break;
	default:					DE_ASSERT(false);								break;
	}

	return bits;
}

// Gets shader stage flags that will be used when choosing a given stage.
VkShaderStageFlags getStageFlags (Stage stage)
{
	VkShaderStageFlags flags = 0u;

	switch (stage)
	{
	case Stage::HOST:			break;
	case Stage::TRANSFER:		break;
	case Stage::RAYGEN:			flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR);											break;
	case Stage::INTERSECT:		flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR);	break;
	case Stage::ANY_HIT:		flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);		break;
	case Stage::CLOSEST_HIT:	flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);	break;
	case Stage::MISS:			flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);			break;
	case Stage::CALLABLE:		flags |= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);		break;
	case Stage::COMPUTE:		flags |= (VK_SHADER_STAGE_COMPUTE_BIT);												break;
	case Stage::FRAGMENT:		flags |= (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);				break;
	default:					DE_ASSERT(false);																	break;
	}

	return flags;
}

bool isRayTracingStage (Stage stage)
{
	bool isRT = false;

	switch (stage)
	{
	case Stage::HOST:
	case Stage::TRANSFER:
	case Stage::COMPUTE:
	case Stage::FRAGMENT:
		break;

	case Stage::RAYGEN:
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
		isRT = true;
		break;

	default:
		DE_ASSERT(false);
		break;
	}

	return isRT;
}

enum class BarrierType
{
	GENERAL		= 0,
	SPECIFIC	= 1,
};

struct TestParams
{
	VkDescriptorType	resourceType;
	Stage				writerStage;
	Stage				readerStage;
	BarrierType			barrierType;

	TestParams (VkDescriptorType resourceType_, Stage writerStage_, Stage readerStage_, BarrierType barrierType_)
		: resourceType	(resourceType_)
		, writerStage	(writerStage_)
		, readerStage	(readerStage_)
		, barrierType	(barrierType_)
	{
		DE_ASSERT(resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
				  resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
				  resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	}
};

bool resourceNeedsHostVisibleMemory (const TestParams& params)
{
	return (params.writerStage == Stage::HOST || params.readerStage == Stage::HOST);
}

bool needsAccelerationStructure (Stage stage)
{
	bool needed;

	switch (stage)
	{
	case Stage::INTERSECT:
	case Stage::ANY_HIT:
	case Stage::CLOSEST_HIT:
	case Stage::MISS:
	case Stage::CALLABLE:
		needed = true;
		break;
	default:
		needed = false;
		break;
	}

	return needed;
}

// The general idea is having a resource like a buffer or image that is generated from a given pipeline stage (including host,
// transfer and all ray shader stages) and read from another stage, using a barrier to synchronize access to the resource. Read
// values are copied to an output host-visible buffer for verification.

class BarrierTestCase : public vkt::TestCase
{
public:
							BarrierTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams);
	virtual					~BarrierTestCase	(void) {}

	virtual void			initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
	virtual void			checkSupport		(Context& context) const;

private:
	TestParams				m_params;
};

class BarrierTestInstance : public vkt::TestInstance
{
public:
								BarrierTestInstance		(Context& context, const TestParams& testParams);
	virtual						~BarrierTestInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

private:
	TestParams					m_params;
};

BarrierTestCase::BarrierTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(testParams)
{
}

void BarrierTestCase::initPrograms (SourceCollections& programCollection) const
{
	const auto&					wstage					= m_params.writerStage;
	const auto&					rstage					= m_params.readerStage;
	const bool					readNeedAS				= needsAccelerationStructure(rstage);
	const deUint32				readerVerifierBinding	= (readNeedAS ? 2u : 1u); // 0 is the barrier resource, 1 may be the AS.
	const ShaderBuildOptions	buildOptions			(programCollection.usedVulkanVersion, SPIRV_VERSION_1_4, 0u, true);
	const std::string			valStatement			= "  const uint  val  = id1d + " + de::toString(kValuesOffset) + ";\n";
	const std::string			readerSaveStatement		= "  verificationBuffer.data[id1d] = val;\n";

	// Common for all ray tracing shaders.
	std::stringstream rayTracingIdsStream;
	rayTracingIdsStream
		<< "  const uint  id1d = gl_LaunchIDEXT.y * " << kImageDim << " + gl_LaunchIDEXT.x;\n"
		<< "  const ivec2 id2d = ivec2(gl_LaunchIDEXT.xy);\n"
		;
	const std::string rayTracingIds = rayTracingIdsStream.str();

	// Common for all compute shaders.
	std::stringstream computeIdsStream;
	computeIdsStream
		<< "  const uint  id1d = gl_GlobalInvocationID.y * " << kImageDim << " + gl_GlobalInvocationID.x;\n"
		<< "  const ivec2 id2d = ivec2(gl_GlobalInvocationID.xy);\n"
		;
	const std::string computeIds = computeIdsStream.str();

	// Common for all fragment shaders.
	std::stringstream fragIdsStream;
	fragIdsStream
		<< "  const uint  id1d = uint(gl_FragCoord.y) * " << kImageDim << " + uint(gl_FragCoord.x);\n"
		<< "  const ivec2 id2d = ivec2(gl_FragCoord.xy);\n"
		;
	const std::string fragIds = fragIdsStream.str();

	// Statements to declare the resource in the writer and reader sides, as well as writing to and reading from it.
	std::stringstream writerResourceDecl;
	std::stringstream readerResourceDecl;
	std::stringstream writeStatement;
	std::stringstream readStatement;

	switch (m_params.resourceType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		writerResourceDecl << "layout(set = 0, binding = 0, std140) uniform ubodef { uint data[" << kBufferElements << "]; } ubo;\n";
		readerResourceDecl << "layout(set = 0, binding = 0, std140) uniform ubodef { uint data[" << kBufferElements << "]; } ubo;\n";
		// No writes can happen from shaders in this case.
		readStatement << "  const uint  val  = ubo.data[id1d];\n";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		writerResourceDecl << "layout(set = 0, binding = 0, std140) buffer ssbodef { uint data[" << kBufferElements << "]; } ssbo;\n";
		readerResourceDecl << "layout(set = 0, binding = 0, std140) buffer ssbodef { uint data[" << kBufferElements << "]; } ssbo;\n";
		writeStatement << "  ssbo.data[id1d] = val;\n";
		readStatement << "  const uint  val  = ssbo.data[id1d];\n";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		writerResourceDecl << "layout(r32ui, set = 0, binding = 0) uniform uimage2D simage;\n";
		readerResourceDecl << "layout(r32ui, set = 0, binding = 0) uniform uimage2D simage;\n";
		writeStatement << "  imageStore(simage, id2d, uvec4(val, 0, 0, 0));\n";
		readStatement << "  const uint  val  = imageLoad(simage, id2d).x;\n";
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	// This extra buffer will be used to copy values from the resource as obtained by the reader and will later be verified on the host.
	std::stringstream readerVerifierDeclStream;
	readerVerifierDeclStream << "layout(set = 0, binding = " << readerVerifierBinding << ") buffer vssbodef { uint data[" << kBufferElements << "]; } verificationBuffer;\n";
	const std::string readerVerifierDecl = readerVerifierDeclStream.str();

	// These are always used together in writer shaders.
	const std::string writerCalcAndWrite = valStatement + writeStatement.str();

	// Add shaders that will be used to write to the resource.
	if (wstage == Stage::HOST || wstage == Stage::TRANSFER)
		; // Nothing to do here.
	else if (wstage == Stage::RAYGEN)
	{
		std::stringstream rgen;
		rgen
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	}
	else if (wstage == Stage::INTERSECT)
	{
		programCollection.glslSources.add("writer_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream isect;
		isect
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "hitAttributeEXT vec3 hitAttribute;\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
			<< "  reportIntersectionEXT(1.0f, 0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("writer_isect") << glu::IntersectionSource(updateRayTracingGLSL(isect.str())) << buildOptions;
	}
	else if (wstage == Stage::ANY_HIT)
	{
		programCollection.glslSources.add("writer_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream ahit;
		ahit
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_ahit") << glu::AnyHitSource(updateRayTracingGLSL(ahit.str())) << buildOptions;
	}
	else if (wstage == Stage::CLOSEST_HIT)
	{
		programCollection.glslSources.add("writer_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream chit;
		chit
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;
	}
	else if (wstage == Stage::MISS)
	{
		programCollection.glslSources.add("writer_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream miss;
		miss
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
	}
	else if (wstage == Stage::CALLABLE)
	{
		{
			std::stringstream rgen;
			rgen
				<< "#version 460 core\n"
				<< "#extension GL_EXT_ray_tracing : require\n"
				<< "layout(location = 0) callableDataEXT float unusedCallableData;"
				<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< "  executeCallableEXT(0, 0);\n"
				<< "}\n"
				;
			programCollection.glslSources.add("writer_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
		}

		std::stringstream callable;
		callable
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) callableDataInEXT float unusedCallableData;\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_callable") << glu::CallableSource(updateRayTracingGLSL(callable.str())) << buildOptions;
	}
	else if (wstage == Stage::COMPUTE)
	{
		std::stringstream compute;
		compute
			<< "#version 460 core\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< computeIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_comp") << glu::ComputeSource(compute.str());
	}
	else if (wstage == Stage::FRAGMENT)
	{
		{
			std::stringstream vert;
			vert
				<< "#version 460 core\n"
				<< "layout(location = 0) in highp vec4 position;\n"
				<< "void main()\n"
				<< "{\n"
				<< "  gl_Position = position;\n"
				<< "}\n"
				;
			programCollection.glslSources.add("writer_aux_vert") << glu::VertexSource(vert.str());
		}

		std::stringstream frag;
		frag
			<< "#version 460 core\n"
			<< writerResourceDecl.str()
			<< "void main()\n"
			<< "{\n"
			<< fragIds
			<< writerCalcAndWrite
			<< "}\n"
			;
		programCollection.glslSources.add("writer_frag") << glu::FragmentSource(frag.str());
	}
	else
	{
		DE_ASSERT(false);
	}

	// These are always used together by reader shaders.
	const std::string readerAllDecls	= readerResourceDecl.str() + readerVerifierDecl;
	const std::string readerReadAndSave	= readStatement.str() + readerSaveStatement;

	// Add shaders that will be used to read from the resource.
	if (rstage == Stage::HOST || rstage == Stage::TRANSFER)
		; // Nothing to do here.
	else if (rstage == Stage::RAYGEN)
	{
		std::stringstream rgen;
		rgen
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	}
	else if (rstage == Stage::INTERSECT)
	{
		programCollection.glslSources.add("reader_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream isect;
		isect
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "hitAttributeEXT vec3 hitAttribute;\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
			<< "  reportIntersectionEXT(1.0f, 0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("reader_isect") << glu::IntersectionSource(updateRayTracingGLSL(isect.str())) << buildOptions;
	}
	else if (rstage == Stage::ANY_HIT)
	{
		programCollection.glslSources.add("reader_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream ahit;
		ahit
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_ahit") << glu::AnyHitSource(updateRayTracingGLSL(ahit.str())) << buildOptions;
	}
	else if (rstage == Stage::CLOSEST_HIT)
	{
		programCollection.glslSources.add("reader_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream chit;
		chit
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;
	}
	else if (rstage == Stage::MISS)
	{
		programCollection.glslSources.add("reader_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

		std::stringstream miss;
		miss
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
	}
	else if (rstage == Stage::CALLABLE)
	{
		{
			std::stringstream rgen;
			rgen
				<< "#version 460 core\n"
				<< "#extension GL_EXT_ray_tracing : require\n"
				<< "layout(location = 0) callableDataEXT float unusedCallableData;"
				<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< "  executeCallableEXT(0, 0);\n"
				<< "}\n"
				;
			programCollection.glslSources.add("reader_aux_rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
		}

		std::stringstream callable;
		callable
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) callableDataInEXT float unusedCallableData;\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< rayTracingIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_callable") << glu::CallableSource(updateRayTracingGLSL(callable.str())) << buildOptions;
	}
	else if (rstage == Stage::COMPUTE)
	{
		std::stringstream compute;
		compute
			<< "#version 460 core\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< computeIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_comp") << glu::ComputeSource(compute.str());
	}
	else if (rstage == Stage::FRAGMENT)
	{
		{
			std::stringstream vert;
			vert
				<< "#version 460 core\n"
				<< "layout(location = 0) in highp vec4 position;\n"
				<< "void main()\n"
				<< "{\n"
				<< "  gl_Position = position;\n"
				<< "}\n"
				;
			programCollection.glslSources.add("reader_aux_vert") << glu::VertexSource(vert.str());
		}

		std::stringstream frag;
		frag
			<< "#version 460 core\n"
			<< readerAllDecls
			<< "void main()\n"
			<< "{\n"
			<< fragIds
			<< readerReadAndSave
			<< "}\n"
			;
		programCollection.glslSources.add("reader_frag") << glu::FragmentSource(frag.str());
	}
	else
	{
		DE_ASSERT(false);
	}
}

TestInstance* BarrierTestCase::createInstance (Context& context) const
{
	return new BarrierTestInstance(context, m_params);
}

void BarrierTestCase::checkSupport (Context& context) const
{
	if (m_params.writerStage == Stage::FRAGMENT)
	{
		const auto& features = context.getDeviceFeatures();

		if (!features.fragmentStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Fragment shader does not support stores");
	}

	if (isRayTracingStage(m_params.readerStage) || isRayTracingStage(m_params.writerStage))
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

		const auto& rtFeatures = context.getRayTracingPipelineFeatures();
		if (!rtFeatures.rayTracingPipeline)
			TCU_THROW(NotSupportedError, "Ray Tracing pipelines not supported");

		const auto& asFeatures = context.getAccelerationStructureFeatures();
		if (!asFeatures.accelerationStructure)
			TCU_FAIL("VK_KHR_acceleration_structure supported without accelerationStructure support");
	}
}

BarrierTestInstance::BarrierTestInstance (Context& context, const TestParams& testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
}

// Creates a buffer with kBufferElements elements of type deUint32 and std140 padding.
std::unique_ptr<BufferWithMemory> makeStd140Buffer (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, VkBufferUsageFlags flags, MemoryRequirement memReq)
{
	std::unique_ptr<BufferWithMemory> buffer;

	const auto bufferCreateInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(kBufferSize), flags);
	buffer.reset(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, memReq));

	return buffer;
}

// Fill buffer with data using std140 padding rules.
void fillStd140Buffer (const DeviceInterface& vkd, VkDevice device, const BufferWithMemory& buffer)
{
	// Buffer host ptr.
	auto& bufferAlloc	= buffer.getAllocation();
	auto* bufferPtr		= bufferAlloc.getHostPtr();

	// Fill buffer with data. This uses the same strategy as the writer shaders.
	std::vector<tcu::UVec4> bufferData(kBufferElements, tcu::UVec4(kValuesOffset, 0u, 0u, 0u));
	for (size_t i = 0; i < bufferData.size(); ++i)
		bufferData[i].x() += static_cast<deUint32>(i);
	deMemcpy(bufferPtr, bufferData.data(), static_cast<size_t>(kBufferSize));
	flushAlloc(vkd, device, bufferAlloc);
}

// Fill buffer with data using std430 padding rules (compact integers).
void fillStd430Buffer (const DeviceInterface& vkd, VkDevice device, const BufferWithMemory& buffer)
{
	// Buffer host ptr.
	auto& bufferAlloc	= buffer.getAllocation();
	auto* bufferPtr		= bufferAlloc.getHostPtr();

	// Fill buffer with data. This uses the same strategy as the writer shaders.
	std::vector<deUint32> bufferData(kBufferElements);
	std::iota(begin(bufferData), end(bufferData), kValuesOffset);
	deMemcpy(bufferPtr, bufferData.data(), static_cast<size_t>(kBufferSize430));
	flushAlloc(vkd, device, bufferAlloc);
}

// Creates a host-visible std430 buffer with kBufferElements elements of type deUint32. If requested, fill buffer with values
// starting at kValuesOffset.
std::unique_ptr<BufferWithMemory> makeStd430BufferImpl (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, VkBufferUsageFlags flags, bool fill)
{
	std::unique_ptr<BufferWithMemory> buffer;

	const auto bufferCreateInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(kBufferSize430), flags);
	buffer.reset(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));

	if (fill)
		fillStd430Buffer(vkd, device, *buffer);

	return buffer;
}

std::unique_ptr<BufferWithMemory> makeStd430Buffer (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, VkBufferUsageFlags flags)
{
	return makeStd430BufferImpl(vkd, device, alloc, flags, false);
}

std::unique_ptr<BufferWithMemory> makeStd430BufferFilled (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, VkBufferUsageFlags flags)
{
	return makeStd430BufferImpl(vkd, device, alloc, flags, true);
}

// Helper struct to group data related to the writer or reader stages.
// Not every member will be used at the same time.
struct StageData
{
	Move<VkDescriptorSetLayout>						descriptorSetLayout;
	Move<VkPipelineLayout>							pipelineLayout;

	Move<VkDescriptorPool>							descriptorPool;
	Move<VkDescriptorSet>							descriptorSet;

	Move<VkPipeline>								pipeline;
	Move<VkRenderPass>								renderPass;
	Move<VkFramebuffer>								framebuffer;
	std::unique_ptr<BufferWithMemory>				vertexBuffer;

	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure;
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure;

	de::MovePtr<BufferWithMemory>					raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>					missShaderBindingTable;
	de::MovePtr<BufferWithMemory>					hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>					callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR					raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					callableShaderBindingTableRegion;

	StageData ()
		: descriptorSetLayout				()
		, pipelineLayout					()
		, descriptorPool					()
		, descriptorSet						()
		, pipeline							()
		, renderPass						()
		, framebuffer						()
		, vertexBuffer						()
		, bottomLevelAccelerationStructure	()
		, topLevelAccelerationStructure		()
		, raygenShaderBindingTable			()
		, missShaderBindingTable			()
		, hitShaderBindingTable				()
		, callableShaderBindingTable		()
		, raygenShaderBindingTableRegion	(makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0))
		, missShaderBindingTableRegion		(makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0))
		, hitShaderBindingTableRegion		(makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0))
		, callableShaderBindingTableRegion	(makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0))
	{
	}

	// Make sure we don't mistakenly pass one of these by value.
	StageData (const StageData&)	= delete;
	StageData (StageData&&)			= delete;
};

// Auxiliar function to update the descriptor set for the writer or reader stages.
void updateDescriptorSet (const DeviceInterface& vkd, VkDevice device, VkCommandBuffer cmdBuffer, Allocator& alloc, VkDescriptorType resourceType, Stage stage, StageData& stageData, BufferWithMemory* resourceBuffer, VkImageView resourceImgView, VkImageLayout layout, bool asNeeded, BufferWithMemory* verificationBuffer)
{
	DescriptorSetUpdateBuilder						updateBuilder;
	VkWriteDescriptorSetAccelerationStructureKHR	writeASInfo;

	if (resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
	{
		const auto descriptorBufferInfo = makeDescriptorBufferInfo(resourceBuffer->get(), 0ull, VK_WHOLE_SIZE);
		updateBuilder.writeSingle(stageData.descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), resourceType, &descriptorBufferInfo);
	}
	else if (resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	{
		const auto descriptorImageInfo = makeDescriptorImageInfo(DE_NULL, resourceImgView, layout);
		updateBuilder.writeSingle(stageData.descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), resourceType, &descriptorImageInfo);
	}
	else
	{
		DE_ASSERT(false);
	}

	// Create top and bottom level acceleration structures if needed.
	if (asNeeded)
	{
		stageData.bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		stageData.bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageFlagBits(stage));
		stageData.bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

		stageData.topLevelAccelerationStructure = makeTopLevelAccelerationStructure();
		stageData.topLevelAccelerationStructure->setInstanceCount(1);
		stageData.topLevelAccelerationStructure->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(stageData.bottomLevelAccelerationStructure.release()));
		stageData.topLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

		writeASInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		writeASInfo.pNext						= nullptr;
		writeASInfo.accelerationStructureCount	= 1u;
		writeASInfo.pAccelerationStructures		= stageData.topLevelAccelerationStructure.get()->getPtr();

		updateBuilder.writeSingle(stageData.descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeASInfo);
	}

	if (verificationBuffer)
	{
		const deUint32	bindingNumber			= (asNeeded ? 2u : 1u);
		const auto		descriptorBufferInfo	= makeDescriptorBufferInfo(verificationBuffer->get(), 0ull, VK_WHOLE_SIZE);

		updateBuilder.writeSingle(stageData.descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingNumber), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
	}

	updateBuilder.update(vkd, device);
}

// Auxiliar function to create the writer or reader compute pipeline
void createComputePipeline (const DeviceInterface& vkd, VkDevice device, Context& context, const char* shaderName, StageData& stageData)
{
	const auto shaderModule = createShaderModule(vkd, device, context.getBinaryCollection().get(shaderName), 0u);

	const VkPipelineShaderStageCreateInfo stageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
		shaderModule.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineCreateFlags			flags;
		stageInfo,										//	VkPipelineShaderStageCreateInfo	stage;
		stageData.pipelineLayout.get(),					//	VkPipelineLayout				layout;
		DE_NULL,										//	VkPipeline						basePipelineHandle;
		0,												//	deInt32							basePipelineIndex;
	};

	// Compute pipeline.
	stageData.pipeline = createComputePipeline(vkd, device, DE_NULL, &createInfo);
}

// Auxiliar function to record commands using the compute pipeline.
void useComputePipeline (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, StageData& stageData)
{
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, stageData.pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, stageData.pipelineLayout.get(), 0u, 1u, &stageData.descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, kImageDim, kImageDim, 1u);
}

// Auxiliar function to create graphics pipeline objects for writer or reader stages.
void createGraphicsPipelineObjects (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, Context& context, const char* vertShaderName, const char* fragShaderName, StageData& stageData)
{
	const auto vertShader = createShaderModule(vkd, device, context.getBinaryCollection().get(vertShaderName), 0u);
	const auto fragShader = createShaderModule(vkd, device, context.getBinaryCollection().get(fragShaderName), 0u);

	// Render pass.
	const auto subpassDescription = makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 0u, nullptr, nullptr, nullptr, 0u, nullptr);
	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,									//	const void*						pNext;
		0u,											//	VkRenderPassCreateFlags			flags;
		0u,											//	deUint32						attachmentCount;
		nullptr,									//	const VkAttachmentDescription*	pAttachments;
		1u,											//	deUint32						subpassCount;
		&subpassDescription,						//	const VkSubpassDescription*		pSubpasses;
		0u,											//	deUint32						dependencyCount;
		nullptr,									//	const VkSubpassDependency*		pDependencies;
	};
	stageData.renderPass = createRenderPass(vkd, device, &renderPassInfo);

	// Viewport.
	const auto viewport = makeViewport(kImageExtent);
	const std::vector<VkViewport> viewports(1u, viewport);

	// Scissor.
	const auto scissor = makeRect2D(kImageExtent);
	const std::vector<VkRect2D> scissors(1u, scissor);

	// Pipeline.
	stageData.pipeline = makeGraphicsPipeline(vkd, device, stageData.pipelineLayout.get(), vertShader.get(), DE_NULL, DE_NULL, DE_NULL, fragShader.get(), stageData.renderPass.get(), viewports, scissors);

	// Framebuffer.
	stageData.framebuffer = makeFramebuffer(vkd, device, stageData.renderPass.get(), 0u, nullptr, kImageDim, kImageDim);

	// Vertex buffer with full-screen quad.
	const auto			vertexBufferSize	= static_cast<VkDeviceSize>(kFullScreenQuad.size() * sizeof(decltype(kFullScreenQuad)::value_type));
	const auto			vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	stageData.vertexBuffer.reset(new BufferWithMemory(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible));
	const auto&			vertexBufferAlloc	= stageData.vertexBuffer->getAllocation();

	deMemcpy(vertexBufferAlloc.getHostPtr(), kFullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
	flushAlloc(vkd, device, vertexBufferAlloc);
}

// Auxiliar function to record commands using the graphics pipeline.
void useGraphicsPipeline (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, StageData& stageData)
{
	const VkDeviceSize	vertexBufferOffset	= 0ull;
	const auto			scissor				= makeRect2D(kImageExtent);

	beginRenderPass(vkd, cmdBuffer, stageData.renderPass.get(), stageData.framebuffer.get(), scissor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stageData.pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stageData.pipelineLayout.get(), 0u, 1u, &stageData.descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &stageData.vertexBuffer->get(), &vertexBufferOffset);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(kFullScreenQuad.size()), 1u, 0u, 0u);
	endRenderPass(vkd, cmdBuffer);
}

// Auxiliar function to create ray tracing pipelines for the writer or reader stages.
void createRayTracingPipelineData (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, Context& context,
								   Stage stage, StageData& stageData, deUint32 shaderGroupHandleSize, deUint32 shaderGroupBaseAlignment,
								   const char* rgenAuxName, const char* rgenName, const char* isectName, const char* ahitName, const char* chitName, const char* missName, const char* callableName)
{
	// Ray tracing stage
	DE_ASSERT(isRayTracingStage(stage));

	if (stage == Stage::RAYGEN)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(rgenName), 0), 0);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (stage == Stage::INTERSECT)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(rgenAuxName), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(isectName), 0), 1);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		stageData.hitShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		stageData.hitShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (stage == Stage::ANY_HIT)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(rgenAuxName), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(ahitName), 0), 1);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		stageData.hitShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		stageData.hitShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (stage == Stage::CLOSEST_HIT)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(rgenAuxName), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(chitName), 0), 1);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		stageData.hitShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		stageData.hitShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (stage == Stage::MISS)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(rgenAuxName), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(missName), 0), 1);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		stageData.missShaderBindingTable		= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		stageData.missShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else if (stage == Stage::CALLABLE)
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(rgenAuxName), 0), 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(callableName), 0), 1);

		stageData.pipeline = rayTracingPipeline->createPipeline(vkd, device, stageData.pipelineLayout.get());

		stageData.raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		stageData.raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		stageData.callableShaderBindingTable		= rayTracingPipeline->createShaderBindingTable(vkd, device, stageData.pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		stageData.callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, stageData.callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}
	else
	{
		DE_ASSERT(false);
	}
}

// Auxiliar function to record commands using the ray tracing pipeline for the writer or reader stages.
void useRayTracingPipeline (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, StageData& stageData)
{
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, stageData.pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, stageData.pipelineLayout.get(), 0u, 1u, &stageData.descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &stageData.raygenShaderBindingTableRegion, &stageData.missShaderBindingTableRegion, &stageData.hitShaderBindingTableRegion, &stageData.callableShaderBindingTableRegion, kImageDim, kImageDim, 1u);
}

tcu::TestStatus BarrierTestInstance::iterate (void)
{
	const auto& vki						= m_context.getInstanceInterface();
	const auto	physicalDevice			= m_context.getPhysicalDevice();
	const auto&	vkd						= m_context.getDeviceInterface();
	const auto	device					= m_context.getDevice();
	const auto	queue					= m_context.getUniversalQueue();
	const auto	familyIndex				= m_context.getUniversalQueueFamilyIndex();
	auto&		alloc					= m_context.getDefaultAllocator();
	const auto	imageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const bool	rtInUse					= (isRayTracingStage(m_params.readerStage) || isRayTracingStage(m_params.writerStage));

	// Stage data for the writer and reader stages.
	StageData writerStageData;
	StageData readerStageData;

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	if (rtInUse)
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Shader stages involved.
	const auto writerStages		= getStageFlags(m_params.writerStage);
	const auto readerStages		= getStageFlags(m_params.readerStage);
	const auto allStages		= (writerStages | readerStages);
	const bool writerNeedsAS	= needsAccelerationStructure(m_params.writerStage);
	const bool readerNeedsAS	= needsAccelerationStructure(m_params.readerStage);

	// Command buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, familyIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	std::unique_ptr<ImageWithMemory>	resourceImg;
	Move<VkImageView>					resourceImgView;
	VkImageLayout						resourceImgLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	const auto							resourceImgSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	std::unique_ptr<BufferWithMemory>	stagingBuffer;
	std::unique_ptr<BufferWithMemory>	resourceBuffer;
	std::unique_ptr<BufferWithMemory>	verificationBuffer;
	const VkBufferUsageFlags			stagingBufferFlags			= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	// Create verification buffer for later use.
	{
		VkBufferUsageFlags verificationBufferFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (m_params.readerStage == Stage::TRANSFER)
			verificationBufferFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		verificationBuffer = makeStd430Buffer(vkd, device, alloc, verificationBufferFlags);
	}

	// Create resource buffer or resource image.
	if (m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
	{
		if (m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			DE_ASSERT(m_params.writerStage == Stage::HOST || m_params.writerStage == Stage::TRANSFER);

		VkBufferUsageFlags bufferFlags	= ((m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
										? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
										: VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		if (m_params.writerStage == Stage::TRANSFER)
			bufferFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		if (m_params.readerStage == Stage::TRANSFER)
			bufferFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		MemoryRequirement bufferMemReq = (resourceNeedsHostVisibleMemory(m_params) ? MemoryRequirement::HostVisible : MemoryRequirement::Any);
		resourceBuffer = makeStd140Buffer(vkd, device, alloc, bufferFlags, bufferMemReq);
	}
	else if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	{
		DE_ASSERT(m_params.writerStage != Stage::HOST);

		VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;

		if (m_params.writerStage == Stage::TRANSFER)
			imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (m_params.readerStage == Stage::TRANSFER)
			imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		const VkImageCreateInfo resourceImageInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			kImageFormat,							//	VkFormat				format;
			kImageExtent,							//	VkExtent3D				extent;
			1u,										//	deUint32				mipLevels;
			1u,										//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			imageUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	deUint32				queueFamilyIndexCount;
			nullptr,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		resourceImg.reset(new ImageWithMemory(vkd, device, alloc, resourceImageInfo, MemoryRequirement::Any));
		resourceImgLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// Image view.
		resourceImgView = makeImageView(vkd, device, resourceImg->get(), VK_IMAGE_VIEW_TYPE_2D, kImageFormat, resourceImgSubresourceRange);
	}
	else
	{
		DE_ASSERT(false);
	}

	// Populate resource from the writer stage.
	if (m_params.writerStage == Stage::HOST)
	{
		DE_ASSERT(m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		// Fill buffer data from the host.
		fillStd140Buffer(vkd, device, *resourceBuffer);
	}
	else if (m_params.writerStage == Stage::TRANSFER)
	{
		// Similar to the previous one, but using a staging buffer.
		if (m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			// Create and fill staging buffer.
			stagingBuffer = makeStd140Buffer(vkd, device, alloc, stagingBufferFlags, MemoryRequirement::HostVisible);
			fillStd140Buffer(vkd, device, *stagingBuffer);

			// Fill resource buffer using a transfer operation.
			const auto region	= makeBufferCopy(0u, 0u, static_cast<VkDeviceSize>(kBufferSize));
			const auto barrier	= makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);
			vkd.cmdCopyBuffer(cmdBuffer, stagingBuffer->get(), resourceBuffer->get(), 1u, &region);
		}
		else if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			// Prepare staging buffer with packed pixels.
			stagingBuffer = makeStd430BufferFilled(vkd, device, alloc, stagingBufferFlags);

			// Barrier for the staging buffer.
			const auto stagingBufferBarrier = makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &stagingBufferBarrier, 0u, nullptr, 0u, nullptr);

			// Transition image to the proper layout.
			const auto expectedLayout = ((m_params.barrierType == BarrierType::SPECIFIC) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);
			if (expectedLayout != resourceImgLayout)
			{
				const auto imgBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, resourceImgLayout, expectedLayout, resourceImg->get(), resourceImgSubresourceRange);
				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &imgBarrier);
				resourceImgLayout = expectedLayout;
			}

			// Copy buffer to image.
			const auto bufferImageCopy = makeBufferImageCopy(kImageExtent, imageSubresourceLayers);
			vkd.cmdCopyBufferToImage(cmdBuffer, stagingBuffer->get(), resourceImg->get(), resourceImgLayout, 1u, &bufferImageCopy);
		}
		else
		{
			DE_ASSERT(false);
		}
	}
	else
	{
		// Other cases use pipelines and a shader to fill the resource.

		// Descriptor set layout.
		DescriptorSetLayoutBuilder dslBuilder;
		dslBuilder.addBinding(m_params.resourceType, 1u, allStages, nullptr);	// The resource is used in the writer and reader stages.
		if (writerNeedsAS)
			dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u, writerStages, nullptr);
		writerStageData.descriptorSetLayout = dslBuilder.build(vkd, device);

		// Pipeline layout.
		writerStageData.pipelineLayout = makePipelineLayout(vkd, device, writerStageData.descriptorSetLayout.get());

		// Descriptor pool and set.
		DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(m_params.resourceType);
		if (writerNeedsAS)
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
		writerStageData.descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		writerStageData.descriptorSet = makeDescriptorSet(vkd, device, writerStageData.descriptorPool.get(), writerStageData.descriptorSetLayout.get());

		// Update descriptor set.
		updateDescriptorSet(vkd, device, cmdBuffer, alloc, m_params.resourceType, m_params.writerStage, writerStageData, resourceBuffer.get(), resourceImgView.get(), VK_IMAGE_LAYOUT_GENERAL, writerNeedsAS, nullptr);

		if (m_params.writerStage == Stage::COMPUTE)
		{
			createComputePipeline(vkd, device, m_context, "writer_comp", writerStageData);

			if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				// Make sure the image is in the proper layout for shader writes.
				const auto expectedLayout = VK_IMAGE_LAYOUT_GENERAL;
				if (expectedLayout != resourceImgLayout)
				{
					const auto imgBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, resourceImgLayout, expectedLayout, resourceImg->get(), resourceImgSubresourceRange);
					vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &imgBarrier);
					resourceImgLayout = expectedLayout;
				}
			}

			// Generate the resource using the pipeline.
			useComputePipeline(vkd, cmdBuffer, writerStageData);
		}
		else if (m_params.writerStage == Stage::FRAGMENT)
		{
			createGraphicsPipelineObjects(vkd, device, alloc, m_context, "writer_aux_vert", "writer_frag", writerStageData);

			if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				// Make sure the image is in the proper layout for shader writes.
				const auto expectedLayout = VK_IMAGE_LAYOUT_GENERAL;
				if (expectedLayout != resourceImgLayout)
				{
					const auto imgBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, resourceImgLayout, expectedLayout, resourceImg->get(), resourceImgSubresourceRange);
					vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &imgBarrier);
					resourceImgLayout = expectedLayout;
				}
			}

			useGraphicsPipeline(vkd, cmdBuffer, writerStageData);
		}
		else
		{
			createRayTracingPipelineData(vkd, device, alloc, m_context, m_params.writerStage, writerStageData, shaderGroupHandleSize, shaderGroupBaseAlignment,
										 "writer_aux_rgen", "writer_rgen", "writer_isect", "writer_ahit", "writer_chit", "writer_miss", "writer_callable");

			if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				// Make sure the image is in the proper layout for shader writes.
				const auto expectedLayout = VK_IMAGE_LAYOUT_GENERAL;
				if (expectedLayout != resourceImgLayout)
				{
					const auto imgBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, resourceImgLayout, expectedLayout, resourceImg->get(), resourceImgSubresourceRange);
					vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0u, 0u, nullptr, 0u, nullptr, 1u, &imgBarrier);
					resourceImgLayout = expectedLayout;
				}
			}

			useRayTracingPipeline(vkd, cmdBuffer, writerStageData);
		}
	}

	// Main barrier to synchronize the writer stage to the reader stage.
	const auto writerPipelineStage	= getPipelineStage(m_params.writerStage);
	const auto readerPipelineStage	= getPipelineStage(m_params.readerStage);
	const auto writerAccessFlag		= getWriterAccessFlag(m_params.writerStage);
	const auto readerAccessFlag		= getReaderAccessFlag(m_params.readerStage, m_params.resourceType);

	if (m_params.barrierType == BarrierType::GENERAL)
	{
		const auto memoryBarrier = makeMemoryBarrier(writerAccessFlag, readerAccessFlag);
		vkd.cmdPipelineBarrier(cmdBuffer, writerPipelineStage, readerPipelineStage, 0u, 1u, &memoryBarrier, 0u, nullptr, 0u, nullptr);
		// Note the image will remain in the general layout in this case.
		if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			DE_ASSERT(resourceImgLayout == VK_IMAGE_LAYOUT_GENERAL);
	}
	else if (m_params.barrierType == BarrierType::SPECIFIC)
	{
		if (m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			const auto bufferBarrier = makeBufferMemoryBarrier(writerAccessFlag, readerAccessFlag, resourceBuffer->get(), 0ull, VK_WHOLE_SIZE);
			vkd.cmdPipelineBarrier(cmdBuffer, writerPipelineStage, readerPipelineStage, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
		}
		else if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			// We'll switch the image layout from the current layout to the one the reader expects.
			const auto newLayout	= getOptimalReadLayout(m_params.readerStage);
			const auto imageBarrier	= makeImageMemoryBarrier(writerAccessFlag, readerAccessFlag, resourceImgLayout, newLayout, resourceImg->get(), resourceImgSubresourceRange);

			vkd.cmdPipelineBarrier(cmdBuffer, writerPipelineStage, readerPipelineStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
			resourceImgLayout = newLayout;
		}
		else
		{
			DE_ASSERT(false);
		}
	}
	else
	{
		DE_ASSERT(false);
	}

	// Read resource from the reader stage copying it to the verification buffer.
	if (m_params.readerStage == Stage::HOST)
	{
		// This needs to wait until we have submitted the command buffer. See below.
	}
	else if (m_params.readerStage == Stage::TRANSFER)
	{
		if (m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			// This is a bit tricky because the resource buffer is in std140 format and the verification buffer is in std430 format.
			std::vector<VkBufferCopy> regions;
			regions.reserve(kBufferElements);
			for (deUint32 i = 0; i < kBufferElements; ++i)
			{
				const VkBufferCopy region =
				{
					static_cast<VkDeviceSize>(i * sizeof(tcu::UVec4)),	//	VkDeviceSize	srcOffset;
					static_cast<VkDeviceSize>(i * sizeof(deUint32)),	//	VkDeviceSize	dstOffset;
					static_cast<VkDeviceSize>(sizeof(deUint32)),		//	VkDeviceSize	size;
				};
				regions.push_back(region);
			}
			vkd.cmdCopyBuffer(cmdBuffer, resourceBuffer->get(), verificationBuffer->get(), static_cast<deUint32>(regions.size()), regions.data());
		}
		else if (m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			const auto bufferImageCopyRegion = makeBufferImageCopy(kImageExtent, imageSubresourceLayers);
			vkd.cmdCopyImageToBuffer(cmdBuffer, resourceImg->get(), resourceImgLayout, verificationBuffer->get(), 1u, &bufferImageCopyRegion);
		}
		else
		{
			DE_ASSERT(false);
		}
	}
	else
	{
		// All other stages use shaders to read the resource into the verification buffer.

		// Descriptor set layout.
		DescriptorSetLayoutBuilder dslBuilder;
		dslBuilder.addBinding(m_params.resourceType, 1u, allStages, nullptr);					// Resource accessed in writers and readers.
		if (readerNeedsAS)
			dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u, readerStages, nullptr);
		dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, readerStages, nullptr);	// Verification buffer.
		readerStageData.descriptorSetLayout = dslBuilder.build(vkd, device);

		// Pipeline layout.
		readerStageData.pipelineLayout = makePipelineLayout(vkd, device, readerStageData.descriptorSetLayout.get());

		// Descriptor pool and set.
		DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(m_params.resourceType);
		if (readerNeedsAS)
			poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		readerStageData.descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		readerStageData.descriptorSet = makeDescriptorSet(vkd, device, readerStageData.descriptorPool.get(), readerStageData.descriptorSetLayout.get());

		// Update descriptor set.
		updateDescriptorSet(vkd, device, cmdBuffer, alloc, m_params.resourceType, m_params.readerStage, readerStageData, resourceBuffer.get(), resourceImgView.get(), resourceImgLayout, readerNeedsAS, verificationBuffer.get());

		if (m_params.readerStage == Stage::COMPUTE)
		{
			createComputePipeline(vkd, device, m_context, "reader_comp", readerStageData);
			useComputePipeline(vkd, cmdBuffer, readerStageData);
		}
		else if (m_params.readerStage == Stage::FRAGMENT)
		{
			createGraphicsPipelineObjects(vkd, device, alloc, m_context, "reader_aux_vert", "reader_frag", readerStageData);
			useGraphicsPipeline(vkd, cmdBuffer, readerStageData);
		}
		else
		{
			createRayTracingPipelineData(vkd, device, alloc, m_context, m_params.readerStage, readerStageData, shaderGroupHandleSize, shaderGroupBaseAlignment,
										 "reader_aux_rgen", "reader_rgen", "reader_isect", "reader_ahit", "reader_chit", "reader_miss", "reader_callable");
			useRayTracingPipeline(vkd, cmdBuffer, readerStageData);
		}
	}

	// Sync verification buffer.
	{
		const auto readerVerificationFlags	= getWriterAccessFlag(m_params.readerStage);
		const auto barrier					= makeBufferMemoryBarrier(readerVerificationFlags, VK_ACCESS_HOST_READ_BIT, verificationBuffer->get(), 0ull, VK_WHOLE_SIZE);
		vkd.cmdPipelineBarrier(cmdBuffer, readerPipelineStage, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &barrier, 0u, nullptr);
	}

	// Submit all recorded commands.
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, verificationBuffer->getAllocation());

	// If the reader stage is the host, we have to wait until the commands have been submitted and the work has been done.
	if (m_params.readerStage == Stage::HOST)
	{
		DE_ASSERT(m_params.resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || m_params.resourceType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		auto& resourceBufferAlloc = resourceBuffer->getAllocation();
		auto* resourceBufferPtr = resourceBufferAlloc.getHostPtr();

		std::vector<tcu::UVec4> resourceData(kBufferElements);
		invalidateAlloc(vkd, device, resourceBufferAlloc);
		deMemcpy(resourceData.data(), resourceBufferPtr, static_cast<size_t>(kBufferElements) * sizeof(tcu::UVec4));

		// Convert from std140 to std430 on the host.
		std::vector<deUint32> verificationData(kBufferElements);
		std::transform(begin(resourceData), end(resourceData), begin(verificationData),
			[](const tcu::UVec4 &v) -> deUint32 { return v.x(); });

		auto& verificationBufferAlloc = verificationBuffer->getAllocation();
		auto* verificationBufferPtr = verificationBufferAlloc.getHostPtr();
		deMemcpy(verificationBufferPtr, verificationData.data(), static_cast<size_t>(kBufferElements) * sizeof(deUint32));
		flushAlloc(vkd, device, verificationBufferAlloc);
	}

	// Check verification buffer on the host.
	{
		auto& verificationAlloc = verificationBuffer->getAllocation();
		auto* verificationPtr = verificationAlloc.getHostPtr();
		std::vector<deUint32> verificationData(kBufferElements);
		deMemcpy(verificationData.data(), verificationPtr, static_cast<size_t>(kBufferElements) * sizeof(deUint32));

		for (size_t i = 0; i < verificationData.size(); ++i)
		{
			const auto&	value		= verificationData[i];
			const auto	expected	= kValuesOffset + i;

			if (value != expected)
			{
				std::ostringstream msg;
				msg << "Unexpected value found at position " << i << ": found " << value << " and expected " << expected;
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous.

tcu::TestCaseGroup*	createBarrierTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "barrier", "Tests involving pipeline barriers and ray tracing"));

	const struct
	{
		VkDescriptorType	type;
		const char*			name;
	} resourceTypes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	"ubo"	},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	"ssbo"	},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,		"simg"	},
	};

	const struct
	{
		Stage		stage;
		const char*	name;
	} stageList[] =
	{
		{ Stage::HOST,			"host"	},
		{ Stage::TRANSFER,		"xfer"	},
		{ Stage::RAYGEN,		"rgen"	},
		{ Stage::INTERSECT,		"isec"	},
		{ Stage::ANY_HIT,		"ahit"	},
		{ Stage::CLOSEST_HIT,	"chit"	},
		{ Stage::MISS,			"miss"	},
		{ Stage::CALLABLE,		"call"	},
		{ Stage::COMPUTE,		"comp"	},
		{ Stage::FRAGMENT,		"frag"	},
	};

	const struct
	{
		BarrierType	barrierType;
		const char*	name;
	} barrierTypes[] =
	{
		{ BarrierType::GENERAL,		"memory_barrier"	},
		{ BarrierType::SPECIFIC,	"specific_barrier"	},
	};

	for (int resourceTypeIdx = 0; resourceTypeIdx < DE_LENGTH_OF_ARRAY(resourceTypes); ++resourceTypeIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> resourceTypeGroup(new tcu::TestCaseGroup(testCtx, resourceTypes[resourceTypeIdx].name, ""));

		for (int barrierTypeIdx = 0; barrierTypeIdx < DE_LENGTH_OF_ARRAY(barrierTypes); ++barrierTypeIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> barrierTypeGroup(new tcu::TestCaseGroup(testCtx, barrierTypes[barrierTypeIdx].name, ""));

			for (int writerStageIdx = 0; writerStageIdx < DE_LENGTH_OF_ARRAY(stageList); ++writerStageIdx)
			for (int readerStageIdx = 0; readerStageIdx < DE_LENGTH_OF_ARRAY(stageList); ++readerStageIdx)
			{
				const auto resourceType	= resourceTypes[resourceTypeIdx].type;
				const auto barrierType	= barrierTypes[barrierTypeIdx].barrierType;
				const auto readerStage	= stageList[readerStageIdx].stage;
				const auto writerStage	= stageList[writerStageIdx].stage;

				// Skip tests that do not involve ray tracing.
				if (!isRayTracingStage(readerStage) && !isRayTracingStage(writerStage))
					continue;

				// Skip tests which require host acess to images.
				if (resourceType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && (writerStage == Stage::HOST || readerStage == Stage::HOST))
					continue;

				// Skip tests that would require writes from shaders to an UBO.
				if (resourceType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && writerStage != Stage::HOST && writerStage != Stage::TRANSFER)
					continue;

				const std::string testName = std::string("from_") + stageList[writerStageIdx].name + "_to_" + stageList[readerStageIdx].name;
				barrierTypeGroup->addChild(new BarrierTestCase(testCtx, testName, "", TestParams(resourceType, writerStage, readerStage, barrierType)));
			}
			resourceTypeGroup->addChild(barrierTypeGroup.release());
		}
		group->addChild(resourceTypeGroup.release());
	}
	return group.release();
}

} // RayTracing
} // vkt
