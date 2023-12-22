#ifndef _VKTMESHSHADERUTIL_HPP
#define _VKTMESHSHADERUTIL_HPP
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

#include "vktTestCase.hpp"

#include "vkDefs.hpp"

#include "deSTLUtil.hpp"

#include <vector>

namespace vkt
{
namespace MeshShader
{

// Primitive shading rate cases.
enum class FragmentSize
{
	SIZE_2X2	= 0,
	SIZE_2X1	= 1,
	SIZE_1X1	= 2,
	SIZE_COUNT	= 3,
};

using FragmentSizeVector = std::vector<FragmentSize>;

// Get the block extent according to the fragment size.
vk::VkExtent2D getShadingRateSize (FragmentSize fragmentSize);

// Returns a shading rate size that does not match the given fragment sizes.
template <typename Iterator>
FragmentSize getBadShadingRateSize (Iterator itBegin, Iterator itEnd)
{
	const auto fsCount = static_cast<int>(FragmentSize::SIZE_COUNT);

	for (int i = 0; i < fsCount; ++i)
	{
		const auto fs = static_cast<FragmentSize>(i);
		if (!de::contains(itBegin, itEnd, fs))
			return fs;
	}

	DE_ASSERT(false);
	return FragmentSize::SIZE_COUNT;
}

// GLSL representation of the given fragment size.
std::string getGLSLShadingRateMask (FragmentSize fragmentSize);

// GLSL/SPV value of the given mask.
int getSPVShadingRateValue (FragmentSize fragmentSize);

// Basic feature check (NV version)
void checkTaskMeshShaderSupportNV (Context& context, bool requireTask, bool requireMesh);

// Basic feature check (EXT version)
void checkTaskMeshShaderSupportEXT (Context& context, bool requireTask, bool requireMesh);

// Get the right SPIR-V build options for the EXT.
vk::ShaderBuildOptions		getMinMeshEXTBuildOptions		(uint32_t vulkanVersion, uint32_t flags = 0u);
vk::SpirVAsmBuildOptions	getMinMeshEXTSpvBuildOptions	(uint32_t vulkanVersion, bool allowMaintenance4 = false);

}
}

#endif // _VKTMESHSHADERUTIL_HPP
