#ifndef _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
#define _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief Graphics pipeline and helper functions for SPIR-V assembly tests
 *//*--------------------------------------------------------------------*/

#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"

#include "vkPrograms.hpp"
#include "vktTestCaseUtil.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"

#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace vkt
{
namespace SpirVAssembly
{

typedef vk::Unique<vk::VkShaderModule>								ModuleHandleUp;
typedef de::SharedPtr<ModuleHandleUp>								ModuleHandleSp;
typedef std::pair<std::string, vk::VkShaderStageFlagBits>			EntryToStage;
typedef std::map<std::string, std::vector<EntryToStage> >			ModuleMap;
typedef std::map<vk::VkShaderStageFlagBits, std::vector<deInt32> >	StageToSpecConstantMap;

// Context for a specific test instantiation. For example, an instantiation
// may test colors yellow/magenta/cyan/mauve in a tesselation shader
// with an entry point named 'main_to_the_main'
struct InstanceContext
{
	// Map of modules to what entry_points we care to use from those modules.
	ModuleMap								moduleMap;
	tcu::RGBA								inputColors[4];
	tcu::RGBA								outputColors[4];
	// Concrete SPIR-V code to test via boilerplate specialization.
	std::map<std::string, std::string>		testCodeFragments;
	StageToSpecConstantMap					specConstants;
	bool									hasTessellation;
	vk::VkShaderStageFlagBits				requiredStages;
	qpTestResult							failResult;
	std::string								failMessageTemplate;	//!< ${reason} in the template will be replaced with a detailed failure message

	InstanceContext (const tcu::RGBA							(&inputs)[4],
					 const tcu::RGBA							(&outputs)[4],
					 const std::map<std::string, std::string>&	testCodeFragments_,
					 const StageToSpecConstantMap&				specConstants_);

	InstanceContext (const InstanceContext& other);

	std::string getSpecializedFailMessage (const std::string&	failureReason);
};

// A description of a shader to be used for a single stage of the graphics pipeline.
struct ShaderElement
{
	// The module that contains this shader entrypoint.
	std::string					moduleName;

	// The name of the entrypoint.
	std::string					entryName;

	// Which shader stage this entry point represents.
	vk::VkShaderStageFlagBits	stage;

	ShaderElement (const std::string& moduleName_, const std::string& entryPoint_, vk::VkShaderStageFlagBits shaderStage_);
};

template <typename T>
const std::string numberToString (T number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

// Performs a bitwise copy of source to the destination type Dest.
template <typename Dest, typename Src>
Dest bitwiseCast(Src source)
{
  Dest dest;
  DE_STATIC_ASSERT(sizeof(source) == sizeof(dest));
  deMemcpy(&dest, &source, sizeof(dest));
  return dest;
}

template<typename T>	T			randomScalar	(de::Random& rnd, T minValue, T maxValue);
template<> inline		float		randomScalar	(de::Random& rnd, float minValue, float maxValue)		{ return rnd.getFloat(minValue, maxValue);	}
template<> inline		deInt32		randomScalar	(de::Random& rnd, deInt32 minValue, deInt32 maxValue)	{ return rnd.getInt(minValue, maxValue);	}


void getDefaultColors (tcu::RGBA (&colors)[4]);

void getHalfColorsFullAlpha (tcu::RGBA (&colors)[4]);

void getInvertedDefaultColors (tcu::RGBA (&colors)[4]);

// Creates fragments that specialize into a simple pass-through shader (of any kind).
std::map<std::string, std::string> passthruFragments(void);

void createCombinedModule(vk::SourceCollections& dst, InstanceContext);

// This has two shaders of each stage. The first
// is a passthrough, the second inverts the color.
void createMultipleEntries(vk::SourceCollections& dst, InstanceContext);

// Turns a statically sized array of ShaderElements into an instance-context
// by setting up the mapping of modules to their contained shaders and stages.
// The inputs and expected outputs are given by inputColors and outputColors
template<size_t N>
InstanceContext createInstanceContext (const ShaderElement							(&elements)[N],
									   const tcu::RGBA								(&inputColors)[4],
									   const tcu::RGBA								(&outputColors)[4],
									   const std::map<std::string, std::string>&	testCodeFragments,
									   const StageToSpecConstantMap&				specConstants,
									   const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									   const std::string&							failMessageTemplate	= std::string())
{
	InstanceContext ctx (inputColors, outputColors, testCodeFragments, specConstants);
	for (size_t i = 0; i < N; ++i)
	{
		ctx.moduleMap[elements[i].moduleName].push_back(std::make_pair(elements[i].entryName, elements[i].stage));
		ctx.requiredStages = static_cast<vk::VkShaderStageFlagBits>(ctx.requiredStages | elements[i].stage);
	}
	ctx.failResult				= failResult;
	if (!failMessageTemplate.empty())
		ctx.failMessageTemplate	= failMessageTemplate;
	return ctx;
}

template<size_t N>
inline InstanceContext createInstanceContext (const ShaderElement						(&elements)[N],
											  tcu::RGBA									(&inputColors)[4],
											  const tcu::RGBA							(&outputColors)[4],
											  const std::map<std::string, std::string>&	testCodeFragments)
{
	return createInstanceContext(elements, inputColors, outputColors, testCodeFragments, StageToSpecConstantMap());
}

// The same as createInstanceContext above, but with default colors.
template<size_t N>
InstanceContext createInstanceContext (const ShaderElement							(&elements)[N],
									   const std::map<std::string, std::string>&	testCodeFragments)
{
	tcu::RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	return createInstanceContext(elements, defaultColors, defaultColors, testCodeFragments);
}

void createTestsForAllStages (const std::string&						name,
							  const tcu::RGBA							(&inputColors)[4],
							  const tcu::RGBA							(&outputColors)[4],
							  const std::map<std::string, std::string>&	testCodeFragments,
							  const std::vector<deInt32>&				specConstants,
							  tcu::TestCaseGroup*						tests,
							  const qpTestResult						failResult			= QP_TEST_RESULT_FAIL,
							  const std::string&						failMessageTemplate	= std::string());

inline void createTestsForAllStages (const std::string&							name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 tcu::TestCaseGroup*						tests,
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string())
{
	std::vector<deInt32> noSpecConstants;
	createTestsForAllStages(name, inputColors, outputColors, testCodeFragments, noSpecConstants, tests, failResult, failMessageTemplate);
}

// Sets up and runs a Vulkan pipeline, then spot-checks the resulting image.
// Feeds the pipeline a set of colored triangles, which then must occur in the
// rendered image.  The surface is cleared before executing the pipeline, so
// whatever the shaders draw can be directly spot-checked.
tcu::TestStatus runAndVerifyDefaultPipeline (Context& context, InstanceContext instance);

// Adds a new test to group using custom fragments for the tessellation-control
// stage and passthrough fragments for all other stages.  Uses default colors
// for input and expected output.
void addTessCtrlTest(tcu::TestCaseGroup* group, const char* name, const std::map<std::string, std::string>& fragments);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
