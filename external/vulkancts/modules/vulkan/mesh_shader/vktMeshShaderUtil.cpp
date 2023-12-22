/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Utility Code
 *//*--------------------------------------------------------------------*/
#include "vktMeshShaderUtil.hpp"
#include "vkDefs.hpp"

namespace vkt
{
namespace MeshShader
{

using namespace vk;

VkExtent2D getShadingRateSize (FragmentSize fragmentSize)
{
	VkExtent2D result {0u, 0u};
	switch (fragmentSize)
	{
	case FragmentSize::SIZE_2X2:		result.width = 2; result.height = 2; break;
	case FragmentSize::SIZE_2X1:		result.width = 2; result.height = 1; break;
	case FragmentSize::SIZE_1X1:		result.width = 1; result.height = 1; break;
	default:							DE_ASSERT(false); break;
	}

	return result;
}

std::string getGLSLShadingRateMask (FragmentSize fragmentSize)
{
	std::string shadingRateMask;

	switch (fragmentSize)
	{
	case FragmentSize::SIZE_2X2:	shadingRateMask = "(gl_ShadingRateFlag2HorizontalPixelsEXT|gl_ShadingRateFlag2VerticalPixelsEXT)";	break;
	case FragmentSize::SIZE_2X1:	shadingRateMask = "gl_ShadingRateFlag2HorizontalPixelsEXT";											break;
	case FragmentSize::SIZE_1X1:	shadingRateMask = "0";																				break;
	default:						DE_ASSERT(false);																					break;
	}

	return shadingRateMask;
}

int getSPVShadingRateValue (FragmentSize fragmentSize)
{
#if 0
      const int gl_ShadingRateFlag2VerticalPixelsEXT = 1;
      const int gl_ShadingRateFlag4VerticalPixelsEXT = 2;
      const int gl_ShadingRateFlag2HorizontalPixelsEXT = 4;
      const int gl_ShadingRateFlag4HorizontalPixelsEXT = 8;
#endif
	int shadingRateValue = 0;

	switch (fragmentSize)
	{
	case FragmentSize::SIZE_2X2:	shadingRateValue = 5;	break;	// (gl_ShadingRateFlag2HorizontalPixelsEXT|gl_ShadingRateFlag2VerticalPixelsEXT)
	case FragmentSize::SIZE_2X1:	shadingRateValue = 4;	break;	// gl_ShadingRateFlag2HorizontalPixelsEXT
	case FragmentSize::SIZE_1X1:	shadingRateValue = 0;	break;
	default:						DE_ASSERT(false);		break;
	}

	return shadingRateValue;
}

void checkTaskMeshShaderSupportNV (Context& context, bool requireTask, bool requireMesh)
{
	context.requireDeviceFunctionality("VK_NV_mesh_shader");

	DE_ASSERT(requireTask || requireMesh);

	const auto& meshFeatures = context.getMeshShaderFeatures();

	if (requireTask && !meshFeatures.taskShader)
		TCU_THROW(NotSupportedError, "Task shader not supported");

	if (requireMesh && !meshFeatures.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shader not supported");
}

void checkTaskMeshShaderSupportEXT (Context& context, bool requireTask, bool requireMesh)
{
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");

	DE_ASSERT(requireTask || requireMesh);

	const auto& meshFeatures = context.getMeshShaderFeaturesEXT();

	if (requireTask && !meshFeatures.taskShader)
		TCU_THROW(NotSupportedError, "Task shader not supported");

	if (requireMesh && !meshFeatures.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shader not supported");
}

vk::ShaderBuildOptions getMinMeshEXTBuildOptions (uint32_t vulkanVersion, uint32_t flags)
{
	return vk::ShaderBuildOptions(vulkanVersion, vk::SPIRV_VERSION_1_4, flags, true);
}

vk::SpirVAsmBuildOptions getMinMeshEXTSpvBuildOptions (uint32_t vulkanVersion, bool allowMaintenance4)
{
	return vk::SpirVAsmBuildOptions(vulkanVersion, vk::SPIRV_VERSION_1_4, true/*allowSpirv14*/, allowMaintenance4);
}

}
}
