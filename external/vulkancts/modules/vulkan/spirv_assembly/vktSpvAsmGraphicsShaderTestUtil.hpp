#ifndef _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
#define _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmUtils.hpp"
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

typedef vk::Unique<VkBuffer>										BufferHandleUp;
typedef vk::Unique<VkImage>											ImageHandleUp;
typedef vk::Unique<VkImageView>										ImageViewHandleUp;
typedef vk::Unique<VkSampler>										SamplerHandleUp;
typedef de::SharedPtr<BufferHandleUp>								BufferHandleSp;
typedef de::SharedPtr<ImageHandleUp>								ImageHandleSp;
typedef de::SharedPtr<ImageViewHandleUp>							ImageViewHandleSp;
typedef de::SharedPtr<SamplerHandleUp>								SamplerHandleSp;
typedef vk::Unique<vk::VkShaderModule>								ModuleHandleUp;
typedef de::SharedPtr<ModuleHandleUp>								ModuleHandleSp;
typedef std::pair<std::string, vk::VkShaderStageFlagBits>			EntryToStage;
typedef std::map<std::string, std::vector<EntryToStage> >			ModuleMap;
typedef std::map<vk::VkShaderStageFlagBits, SpecConstants >			StageToSpecConstantMap;

enum NumberType
{
	NUMBERTYPE_INT32,
	NUMBERTYPE_UINT32,
	NUMBERTYPE_FLOAT32,
	NUMBERTYPE_END32,		// Marks the end of 32-bit scalar types
	NUMBERTYPE_INT16,
	NUMBERTYPE_UINT16,
	NUMBERTYPE_FLOAT16,
	NUMBERTYPE_END16,		// Marks the end of 16-bit scalar types
	NUMBERTYPE_FLOAT64,
};

typedef enum RoundingModeFlags_e
{
	ROUNDINGMODE_RTE = 0x1,	// Round to nearest even
	ROUNDINGMODE_RTZ = 0x2,	// Round to zero
} RoundingModeFlags;

typedef bool (*GraphicsVerifyBinaryFunc) (const ProgramBinary&	binary);

// Resources used by graphics-pipeline-based tests.
struct GraphicsResources
{
	// Resources used as inputs.
	std::vector<Resource>		inputs;
	// Input resource format if used
	VkFormat					inputFormat;
	// Resources used as outputs. The data supplied will be used as
	// the expected outputs for the corresponding bindings by default.
	// If other behaviors are needed, please provide a custom verifyIO.
	std::vector<Resource>		outputs;
	// If null, a default verification will be performed by comparing the
	// memory pointed to by outputAllocations  and the contents of
	// expectedOutputs. Otherwise the function pointed to by verifyIO will
	// be called. If true is returned, then the test case is assumed to
	// have passed, if false is returned, then the test case is assumed
	// to have failed.
	VerifyIOFunc				verifyIO;
	GraphicsVerifyBinaryFunc	verifyBinary;
	SpirvVersion				spirvVersion;

							GraphicsResources()
								: inputFormat		(VK_FORMAT_R32G32B32A32_SFLOAT)
								, verifyIO			(DE_NULL)
								, verifyBinary		(DE_NULL)
								, spirvVersion		(SPIRV_VERSION_1_0)
							{}
};

// Interface data type.
struct IFDataType
{
						IFDataType			(deUint32 numE, NumberType elementT)
							: numElements	(numE)
							, elementType	(elementT)
						{
							DE_ASSERT(numE > 0 && numE < 5);
							DE_ASSERT(elementT != NUMBERTYPE_END32 && elementT != NUMBERTYPE_END16);
						}

						IFDataType			(const IFDataType& that)
							: numElements	(that.numElements)
							, elementType	(that.elementType)
						{}

	deUint32			getElementNumBytes	(void) const;
	deUint32			getNumBytes			(void) const { return numElements * getElementNumBytes(); }

	vk::VkFormat		getVkFormat			(void) const;

	tcu::TextureFormat	getTextureFormat	(void) const;

	std::string			str					(void) const;

	bool				elementIs32bit		(void) const { return elementType < NUMBERTYPE_END32; }
	bool				elementIs64bit		(void) const { return elementType > NUMBERTYPE_END16; }

	bool				isVector			(void) const { return numElements > 1; }

	deUint32			numElements;
	NumberType			elementType;
};

typedef std::pair<IFDataType, BufferSp>			Interface;

// Interface variables used by graphics-pipeline-based tests.
class GraphicsInterfaces
{
public:
						GraphicsInterfaces	()
							: rndMode	(static_cast<RoundingModeFlags>(0))
						{}

						GraphicsInterfaces	(const GraphicsInterfaces& that)
							: inputs	(that.inputs)
							, outputs	(that.outputs)
							, rndMode	(that.rndMode)
						{}

	void				setInputOutput		(const Interface& input, const Interface&  output)
						{
							inputs.clear();
							outputs.clear();
							inputs.push_back(input);
							outputs.push_back(output);
						}

	const IFDataType&	getInputType		(void) const
						{
							DE_ASSERT(inputs.size() == 1);
							return inputs.front().first;
						}

	const IFDataType&	getOutputType		(void) const
						{
							DE_ASSERT(outputs.size() == 1);
							return outputs.front().first;
						}

	const BufferSp&		getInputBuffer		(void) const
						{
							DE_ASSERT(inputs.size() == 1);
							return inputs.front().second;
						}

	const BufferSp&		getOutputBuffer		(void) const
						{
							DE_ASSERT(outputs.size() == 1);
							return outputs.front().second;
						}

	bool				empty				(void) const
						{
							return inputs.size() == 0;
						}

	void				setRoundingMode		(RoundingModeFlags flag)
						{
							rndMode = flag;
						}
	RoundingModeFlags	getRoundingMode		(void) const
						{
							return rndMode;
						}
private:
	// vector<Interface> acts as a null-able Interface here. Canonically we should use
	// std::unique_ptr, but sadly we cannot leverage C++11 in dEQP. dEQP has its own
	// de::UniquePtr, but still cumbersome to use in InstanceContext and do copies
	// at various places.
	// Public methods should make sure that there are less than two elements in both
	// members and both members have the same number of elements.
	std::vector<Interface>	inputs;
	std::vector<Interface>	outputs;
	RoundingModeFlags		rndMode;

};

struct PushConstants
{
public:
							PushConstants (void)
							{}

							PushConstants (const PushConstants& that)
								: pcs	(that.pcs)
							{}

	void					setPushConstant	(const BufferSp& pc)
							{
								pcs.clear();
								pcs.push_back(pc);
							}

	bool					empty (void) const
							{
								return pcs.empty();
							}

	const BufferSp&			getBuffer(void) const
							{
								DE_ASSERT(pcs.size() == 1);
								return pcs[0];
							}

private:
	// Right now we only support one field in the push constant block.
	std::vector<BufferSp>	pcs;
};

// Returns the corresponding buffer usage flag bit for the given descriptor type.
VkBufferUsageFlagBits getMatchingBufferUsageFlagBit (VkDescriptorType dType);

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
	std::vector<std::string>				requiredDeviceExtensions;
	VulkanFeatures							requestedFeatures;
	PushConstants							pushConstants;
	// Specifies the (one or more) stages that use a customized shader code.
	VkShaderStageFlags						customizedStages;
	// Possible resources used by the graphics pipeline.
	// If it is not empty, a single descriptor set (number 0) will be allocated
	// to point to all resources specified. Binding numbers are allocated in
	// accord with the resources' order in the vector; outputs are allocated
	// after inputs.
	GraphicsResources						resources;
	// Possible interface variables use by the graphics pipeline.
	// If it is not empty, input/output variables will be set up for shader stages
	// in the test. Both the input and output variable will take location #2 in the
	// pipeline for all stages, except that the output variable in the fragment
	// stage will take location #1.
	GraphicsInterfaces						interfaces;
	qpTestResult							failResult;
	std::string								failMessageTemplate;	//!< ${reason} in the template will be replaced with a detailed failure message
	bool									renderFullSquare;		// Forces to render whole render area, though with background color
	bool									splitRenderArea;		// Split render into multiple submissions.

	InstanceContext (const tcu::RGBA							(&inputs)[4],
					 const tcu::RGBA							(&outputs)[4],
					 const std::map<std::string, std::string>&	testCodeFragments_,
					 const StageToSpecConstantMap&				specConstants_,
					 const PushConstants&						pushConsants_,
					 const GraphicsResources&					resources_,
					 const GraphicsInterfaces&					interfaces_,
					 const std::vector<std::string>&			extensions_,
					 VulkanFeatures								vulkanFeatures_,
					 VkShaderStageFlags							customizedStages_);

	InstanceContext (const InstanceContext& other);

	std::string getSpecializedFailMessage (const std::string& failureReason);
};

enum ShaderTask
{
	SHADER_TASK_NONE = 0,
	SHADER_TASK_NORMAL,
	SHADER_TASK_UNUSED_VAR,
	SHADER_TASK_UNUSED_FUNC,
	SHADER_TASK_LAST
};

enum ShaderTaskIndex
{
	SHADER_TASK_INDEX_VERTEX		= 0,
	SHADER_TASK_INDEX_GEOMETRY		= 1,
	SHADER_TASK_INDEX_TESS_CONTROL	= 2,
	SHADER_TASK_INDEX_TESS_EVAL		= 3,
	SHADER_TASK_INDEX_FRAGMENT		= 4,
	SHADER_TASK_INDEX_LAST			= 5
};

typedef ShaderTask ShaderTaskArray[SHADER_TASK_INDEX_LAST];

struct UnusedVariableContext
{
	InstanceContext		instanceContext;
	ShaderTaskArray		shaderTasks;
	VariableLocation	variableLocation;

	UnusedVariableContext(const InstanceContext& ctx, const ShaderTaskArray& tasks, const VariableLocation& location)
		: instanceContext(ctx), shaderTasks(), variableLocation(location)
	{
		deMemcpy(shaderTasks, tasks, sizeof(tasks));
	}
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

void getDefaultColors (tcu::RGBA (&colors)[4]);

void getHalfColorsFullAlpha (tcu::RGBA (&colors)[4]);

void getInvertedDefaultColors (tcu::RGBA (&colors)[4]);

// Creates fragments that specialize into a simple pass-through shader (of any kind).
std::map<std::string, std::string> passthruFragments (void);

// Creates a combined shader module based on VkShaderStageFlagBits defined in InstanceContext.
void createCombinedModule (vk::SourceCollections& dst, InstanceContext ctx);

// Creates shaders with unused variables based on the UnusedVariableContext.
void createUnusedVariableModules (vk::SourceCollections& dst, UnusedVariableContext ctx);

// This has two shaders of each stage. The first
// is a passthrough, the second inverts the color.
void createMultipleEntries (vk::SourceCollections& dst, InstanceContext);

// Turns a vector of ShaderElements into an instance-context by setting up the mapping of modules
// to their contained shaders and stages. The inputs and expected outputs are given by inputColors
// and outputColors
InstanceContext createInstanceContext (const std::vector<ShaderElement>&			elements,
									   const tcu::RGBA								(&inputColors)[4],
									   const tcu::RGBA								(&outputColors)[4],
									   const std::map<std::string, std::string>&	testCodeFragments,
									   const StageToSpecConstantMap&				specConstants,
									   const PushConstants&							pushConstants,
									   const GraphicsResources&						resources,
									   const GraphicsInterfaces&					interfaces,
									   const std::vector<std::string>&				extensions,
									   VulkanFeatures								vulkanFeatures,
									   VkShaderStageFlags							customizedStages,
									   const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									   const std::string&							failMessageTemplate	= std::string());

// Same as above but using a statically sized array.
template <size_t N>
inline InstanceContext createInstanceContext (const ShaderElement							(&elements)[N],
											  const tcu::RGBA								(&inputColors)[4],
											  const tcu::RGBA								(&outputColors)[4],
											  const std::map<std::string, std::string>&		testCodeFragments,
											  const StageToSpecConstantMap&					specConstants,
											  const PushConstants&							pushConstants,
											  const GraphicsResources&						resources,
											  const GraphicsInterfaces&						interfaces,
											  const std::vector<std::string>&				extensions,
											  VulkanFeatures								vulkanFeatures,
											  VkShaderStageFlags							customizedStages,
											  const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
											  const std::string&							failMessageTemplate	= std::string())
{
	std::vector<ShaderElement> elementsVector(elements, elements + N);
	return createInstanceContext(elementsVector, inputColors, outputColors, testCodeFragments, specConstants, pushConstants,
								 resources, interfaces, extensions, vulkanFeatures, customizedStages, failResult, failMessageTemplate);
}

// The same as createInstanceContext above, without extensions, spec constants, and resources.
InstanceContext createInstanceContext (const std::vector<ShaderElement>&			elements,
									   tcu::RGBA									(&inputColors)[4],
									   const tcu::RGBA								(&outputColors)[4],
									   const std::map<std::string, std::string>&	testCodeFragments);

// Same as above, but using a statically sized array.
template <size_t N>
inline InstanceContext createInstanceContext (const ShaderElement							(&elements)[N],
											  tcu::RGBA										(&inputColors)[4],
											  const tcu::RGBA								(&outputColors)[4],
											  const std::map<std::string, std::string>&		testCodeFragments)
{
	std::vector<ShaderElement> elementsVector(elements, elements + N);
	return createInstanceContext(elementsVector, inputColors, outputColors, testCodeFragments);
}

// The same as createInstanceContext above, but with default colors.
InstanceContext createInstanceContext (const std::vector<ShaderElement>&			elements,
									   const std::map<std::string, std::string>&	testCodeFragments);

// Same as above, but using a statically sized array.
template<size_t N>
inline InstanceContext createInstanceContext (const ShaderElement						(&elements)[N],
											  const std::map<std::string, std::string>&	testCodeFragments)
{
	std::vector<ShaderElement> elementsVector(elements, elements + N);
	return createInstanceContext(elementsVector, testCodeFragments);
}


// Create an unused variable context for the given combination.
UnusedVariableContext createUnusedVariableContext(const ShaderTaskArray& shaderTasks, const VariableLocation& location);

void addShaderCodeCustomVertex (vk::SourceCollections& dst, InstanceContext& context, const SpirVAsmBuildOptions* spirVAsmBuildOptions);
void addShaderCodeCustomTessControl (vk::SourceCollections& dst, InstanceContext& context, const SpirVAsmBuildOptions* spirVAsmBuildOptions);
void addShaderCodeCustomTessEval (vk::SourceCollections& dst, InstanceContext& context, const SpirVAsmBuildOptions* spirVAsmBuildOptions);
void addShaderCodeCustomGeometry (vk::SourceCollections& dst, InstanceContext& context, const SpirVAsmBuildOptions* spirVAsmBuildOptions);
void addShaderCodeCustomFragment (vk::SourceCollections& dst, InstanceContext& context, const SpirVAsmBuildOptions* spirVAsmBuildOptions);

void createTestForStage (vk::VkShaderStageFlagBits					stage,
						 const std::string&							name,
						 const tcu::RGBA							(&inputColors)[4],
						 const tcu::RGBA							(&outputColors)[4],
						 const std::map<std::string, std::string>&	testCodeFragments,
						 const SpecConstants&						specConstants,
						 const PushConstants&						pushConstants,
						 const GraphicsResources&					resources,
						 const GraphicsInterfaces&					interfaces,
						 const std::vector<std::string>&			extensions,
						 VulkanFeatures								vulkanFeatures,
						 tcu::TestCaseGroup*						tests,
						 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
						 const std::string&							failMessageTemplate = std::string(),
						 const bool									renderFullSquare	= false,
						 const bool									splitRenderArea		= false);

void createTestsForAllStages (const std::string&						name,
							  const tcu::RGBA							(&inputColors)[4],
							  const tcu::RGBA							(&outputColors)[4],
							  const std::map<std::string, std::string>&	testCodeFragments,
							  const SpecConstants&						specConstants,
							  const PushConstants&						pushConstants,
							  const GraphicsResources&					resources,
							  const GraphicsInterfaces&					interfaces,
							  const std::vector<std::string>&			extensions,
							  VulkanFeatures							vulkanFeatures,
							  tcu::TestCaseGroup*						tests,
							  const qpTestResult						failResult			= QP_TEST_RESULT_FAIL,
							  const std::string&						failMessageTemplate	= std::string(),
							  const bool								splitRenderArea		= false);

inline void createTestsForAllStages (const std::string&							name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 tcu::TestCaseGroup*						tests,
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string())
{
	SpecConstants				noSpecConstants;
	PushConstants				noPushConstants;
	GraphicsResources			noResources;
	GraphicsInterfaces			noInterfaces;
	std::vector<std::string>	noExtensions;

	createTestsForAllStages(
			name, inputColors, outputColors, testCodeFragments, noSpecConstants, noPushConstants,
			noResources, noInterfaces, noExtensions, VulkanFeatures(),
			tests, failResult, failMessageTemplate);
}

inline void createTestsForAllStages (const std::string&							name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 const SpecConstants&						specConstants,
									 tcu::TestCaseGroup*						tests,
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string())
{
	PushConstants					noPushConstants;
	GraphicsResources				noResources;
	GraphicsInterfaces				noInterfaces;
	std::vector<std::string>		noExtensions;

	createTestsForAllStages(
			name, inputColors, outputColors, testCodeFragments, specConstants, noPushConstants,
			noResources, noInterfaces, noExtensions, VulkanFeatures(),
			tests, failResult, failMessageTemplate);
}

inline void createTestsForAllStages (const std::string&							name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 const GraphicsResources&					resources,
									 const std::vector<std::string>&			extensions,
									 tcu::TestCaseGroup*						tests,
									 VulkanFeatures								vulkanFeatures		= VulkanFeatures(),
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string(),
									 const bool									splitRenderArea = false)
{
	SpecConstants				noSpecConstants;
	PushConstants				noPushConstants;
	GraphicsInterfaces			noInterfaces;

	createTestsForAllStages(
			name, inputColors, outputColors, testCodeFragments, noSpecConstants, noPushConstants,
			resources, noInterfaces, extensions, vulkanFeatures,
			tests, failResult, failMessageTemplate, splitRenderArea );
}

inline void createTestsForAllStages (const std::string& name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 const GraphicsInterfaces					interfaces,
									 const std::vector<std::string>&			extensions,
									 tcu::TestCaseGroup*						tests,
									 VulkanFeatures								vulkanFeatures		= VulkanFeatures(),
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string())
{
	GraphicsResources			noResources;
	SpecConstants				noSpecConstants;
	PushConstants				noPushConstants;

	createTestsForAllStages(
			name, inputColors, outputColors, testCodeFragments, noSpecConstants, noPushConstants,
			noResources, interfaces, extensions, vulkanFeatures,
			tests, failResult, failMessageTemplate);
}

inline void createTestsForAllStages (const std::string& name,
									 const tcu::RGBA							(&inputColors)[4],
									 const tcu::RGBA							(&outputColors)[4],
									 const std::map<std::string, std::string>&	testCodeFragments,
									 const PushConstants&						pushConstants,
									 const GraphicsResources&					resources,
									 const std::vector<std::string>&			extensions,
									 tcu::TestCaseGroup*						tests,
									 VulkanFeatures								vulkanFeatures		= VulkanFeatures(),
									 const qpTestResult							failResult			= QP_TEST_RESULT_FAIL,
									 const std::string&							failMessageTemplate	= std::string())
{
	SpecConstants					noSpecConstants;
	GraphicsInterfaces				noInterfaces;

	createTestsForAllStages(
			name, inputColors, outputColors, testCodeFragments, noSpecConstants, pushConstants,
			resources, noInterfaces, extensions, vulkanFeatures,
			tests, failResult, failMessageTemplate);
}

// Sets up and runs a Vulkan pipeline, then spot-checks the resulting image.
// Feeds the pipeline a set of colored triangles, which then must occur in the
// rendered image.  The surface is cleared before executing the pipeline, so
// whatever the shaders draw can be directly spot-checked.
tcu::TestStatus runAndVerifyDefaultPipeline (Context& context, InstanceContext instance);

// Use the instance context in the UnusedVariableContext to run the function above.
tcu::TestStatus runAndVerifyUnusedVariablePipeline (Context &context, UnusedVariableContext unusedVariableContext);

// Adds a new test to group using custom fragments for the tessellation-control
// stage and passthrough fragments for all other stages.  Uses default colors
// for input and expected output.
void addTessCtrlTest (tcu::TestCaseGroup* group, const char* name, const std::map<std::string, std::string>& fragments);

// Given the original 32-bit float value, computes the corresponding 16-bit
// float value under the given rounding mode flags and compares with the
// returned 16-bit float value. Returns true if they are considered as equal.
//
// The following equivalence criteria are respected:
// * Positive and negative zeros are considered equivalent.
// * Denormalized floats are allowed to be flushed to zeros, including
//   * Inputted 32bit denormalized float
//   * Generated 16bit denormalized float
// * Different bit patterns of NaNs are allowed.
// * For the rest, require exactly the same bit pattern.
bool compare16BitFloat (float original, deUint16 returned, RoundingModeFlags flags, tcu::TestLog& log);

// Given the original 16-bit float value, computes the corresponding 32-bit
// float value and compares with the returned 32-bit float value.
// Returns true if they are considered as equal.
//
// The following equivalence criteria are respected:
// * Positive and negative zeros are considered equivalent.
// * Denormalized floats are allowed to be flushed to zeros, including
//   * Inputted 16bit denormalized float
//   * Generated 32bit denormalized float
// * Different bit patterns of NaNs are allowed.
// * For the rest, require exactly the same bit pattern.
bool compare16BitFloat (deUint16 returned, float original, tcu::TestLog& log);
bool compare16BitFloat (deFloat16 original, deFloat16 returned, std::string& error);

// Given the original 64-bit float value, computes the corresponding 16-bit
// float value under the given rounding mode flags and compares with the
// returned 16-bit float value. Returns true if they are considered as equal.
//
// The following equivalence criteria are respected:
// * Positive and negative zeros are considered equivalent.
// * Denormalized floats are allowed to be flushed to zeros, including
//   * Inputted 64bit denormalized float
//   * Generated 16bit denormalized float
// * Different bit patterns of NaNs are allowed.
// * For the rest, require exactly the same bit pattern.
bool compare16BitFloat64 (double original, deUint16 returned, RoundingModeFlags flags, tcu::TestLog& log);

// Compare the returned 32-bit float against its expected value.
//
// The following equivalence criteria are respected:
// * Denormalized floats are allowed to be flushed to zeros, including
//   * The expected value itself is a denormalized float
//   * The expected value is a denormalized float if converted to 16bit
// * Different bit patterns of NaNs/Infs are allowed.
// * For the rest, use C++ float equivalence check.
bool compare32BitFloat (float expected, float returned, tcu::TestLog& log);

// Compare the returned 64-bit float against its expected value.
//
// The following equivalence criteria are respected:
// * Denormalized floats are allowed to be flushed to zeros, including
//   * The expected value itself is a denormalized float
//   * The expected value is a denormalized float if converted to 16bit
// * Different bit patterns of NaNs/Infs are allowed.
// * For the rest, use C++ float equivalence check.
bool compare64BitFloat (double expected, double returned, tcu::TestLog& log);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMGRAPHICSSHADERTESTUTIL_HPP
