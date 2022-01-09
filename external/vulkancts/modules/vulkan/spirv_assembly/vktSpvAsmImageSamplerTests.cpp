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
 * \brief SPIR-V Assembly Tests for images and samplers.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmImageSamplerTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"

#include "vkImageUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::RGBA;
using tcu::Vec4;

namespace
{
enum TestType
{
	TESTTYPE_LOCAL_VARIABLES = 0,
	TESTTYPE_PASS_IMAGE_TO_FUNCTION,
	TESTTYPE_PASS_SAMPLER_TO_FUNCTION,
	TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION,
	TESTTYPE_OPTYPEIMAGE_MISMATCH,

	TESTTYPE_LAST
};

enum ReadOp
{
	READOP_IMAGEREAD = 0,
	READOP_IMAGEFETCH,
	READOP_IMAGESAMPLE,
	READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD,
	READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD,

	READOP_LAST
};

enum DescriptorType
{
	DESCRIPTOR_TYPE_STORAGE_IMAGE = 0,								// Storage image
	DESCRIPTOR_TYPE_SAMPLED_IMAGE,									// Sampled image
	DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,							// Combined image sampler
	DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES,		// Combined image sampler with separate shader variables
	DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS,	// Combined image sampler where image and sampler variables are taken from two different desciptors

	DESCRIPTOR_TYPE_LAST
};

enum DepthProperty
{
	DEPTH_PROPERTY_NON_DEPTH = 0,
	DEPTH_PROPERTY_DEPTH,
	DEPTH_PROPERTY_UNKNOWN,

	DEPTH_PROPERTY_LAST
};

bool isValidTestCase (TestType testType, DescriptorType descriptorType, ReadOp readOp)
{
	// Check valid descriptor type and test type combinations
	switch (testType)
	{
		case TESTTYPE_PASS_IMAGE_TO_FUNCTION:
			if (descriptorType != DESCRIPTOR_TYPE_STORAGE_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_SAMPLED_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES		&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
					return false;
			break;

		case TESTTYPE_PASS_SAMPLER_TO_FUNCTION:
			if (descriptorType != DESCRIPTOR_TYPE_SAMPLED_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES		&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
				return false;
			break;

		case TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION:
			if (descriptorType != DESCRIPTOR_TYPE_SAMPLED_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES		&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
				return false;
			break;

		default:
			break;
	}

	// Check valid descriptor type and read operation combinations
	switch (readOp)
	{
		case READOP_IMAGEREAD:
			if (descriptorType != DESCRIPTOR_TYPE_STORAGE_IMAGE)
				return false;
			break;

		case READOP_IMAGEFETCH:
			if (descriptorType != DESCRIPTOR_TYPE_SAMPLED_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER						&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES		&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
				return false;
			break;

		case READOP_IMAGESAMPLE:
		case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
		case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
			if (descriptorType != DESCRIPTOR_TYPE_SAMPLED_IMAGE									&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER						&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES		&&
				descriptorType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
				return false;
			break;

		default:
			break;
	}

	return true;
}

const char* getTestTypeName (TestType testType)
{
	switch (testType)
	{
		case TESTTYPE_LOCAL_VARIABLES:
			return "all_local_variables";

		case TESTTYPE_PASS_IMAGE_TO_FUNCTION:
			return "pass_image_to_function";

		case TESTTYPE_PASS_SAMPLER_TO_FUNCTION:
			return "pass_sampler_to_function";

		case TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION:
			return "pass_image_and_sampler_to_function";

		case TESTTYPE_OPTYPEIMAGE_MISMATCH:
			return "optypeimage_mismatch";

		default:
			DE_FATAL("Unknown test type");
			return "";
	}
}

const char* getReadOpName (ReadOp readOp)
{
	switch (readOp)
	{
		case READOP_IMAGEREAD:
			return "imageread";

		case READOP_IMAGEFETCH:
			return "imagefetch";

		case READOP_IMAGESAMPLE:
			return "imagesample";

		case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
			return "imagesample_dref_implicit_lod";

		case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
			return "imagesample_dref_explicit_lod";

		default:
			DE_FATAL("Unknown readop");
			return "";
	}
}

const char* getDescriptorName (DescriptorType descType)
{
	switch (descType)
	{
		case DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return "storage_image";

		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return "sampled_image";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return "combined_image_sampler";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
			return "combined_image_sampler_separate_variables";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
			return "combined_image_sampler_separate_descriptors";

		default:
			DE_FATAL("Unknown descriptor type");
			return "";
	}
}

const char* getDepthPropertyName (DepthProperty depthProperty)
{
	switch (depthProperty)
	{
		case DEPTH_PROPERTY_NON_DEPTH:
			return "non_depth";

		case DEPTH_PROPERTY_DEPTH:
			return "depth";

		case DEPTH_PROPERTY_UNKNOWN:
			return "unknown";

		default:
			DE_FATAL("Unknown depth property");
			return "";
	}
}

VkDescriptorType getVkDescriptorType (DescriptorType descType)
{
	switch (descType)
	{
		case DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		default:
			DE_FATAL("Unknown descriptor type");
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	}
}

VkFormat getImageFormat (ReadOp readOp)
{
	switch (readOp)
	{
		case READOP_IMAGEREAD:
		case READOP_IMAGEFETCH:
		case READOP_IMAGESAMPLE:
			return VK_FORMAT_R32G32B32A32_SFLOAT;

		case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
		case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
			return VK_FORMAT_D32_SFLOAT;

		default:
			DE_FATAL("Unknown readop");
			return VK_FORMAT_UNDEFINED;
	}
}

// Get variables that are declared in the read function, ie. not passed as parameters
std::string getFunctionDstVariableStr (ReadOp readOp, DescriptorType descType, TestType testType)
{
	const bool passNdx = (testType == TESTTYPE_LOCAL_VARIABLES)					|| (testType == TESTTYPE_OPTYPEIMAGE_MISMATCH);
	const bool passImg = ((testType == TESTTYPE_PASS_IMAGE_TO_FUNCTION)			|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));
	const bool passSmp = ((testType == TESTTYPE_PASS_SAMPLER_TO_FUNCTION)		|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));

	std::string result = "";

	switch (descType)
	{
		case DESCRIPTOR_TYPE_STORAGE_IMAGE:
		{
			switch (readOp)
			{
				case READOP_IMAGEREAD:
					if (passNdx)
						return	"           %func_img = OpLoad %Image %InputData\n";
					break;

				default:
					DE_FATAL("Not possible");
					break;
			}
			break;
		}
		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
		{
			switch (readOp)
			{
				case READOP_IMAGEFETCH:
					if (passNdx)
						return	"           %func_img = OpLoad %Image %InputData\n";

					if (passSmp && !passImg)
						return	"           %func_tmp = OpLoad %Image %InputData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_tmp %func_smp\n"
								"           %func_img = OpImage %Image %func_smi\n";

					if (passSmp && passImg)
						return	"           %func_smi = OpSampledImage %SampledImage %func_tmp %func_smp\n"
								"           %func_img = OpImage %Image %func_smi\n";
					break;

				case READOP_IMAGESAMPLE:
				case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
				case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
					if (passNdx)
						return	"           %func_img = OpLoad %Image %InputData\n"
								"           %func_smp = OpLoad %Sampler %SamplerData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passImg && !passSmp)
						return	"           %func_smp = OpLoad %Sampler %SamplerData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passSmp && !passImg)
						return	"           %func_img = OpLoad %Image %InputData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passSmp && passImg)
						return	"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		{
			switch (readOp)
			{
				case READOP_IMAGEFETCH:
					if (passNdx)
						return	"           %func_smi = OpLoad %SampledImage %InputData\n"
								"           %func_img = OpImage %Image %func_smi\n";
					break;

				case READOP_IMAGESAMPLE:
				case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
				case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
					if (passNdx)
						return	"           %func_smi = OpLoad %SampledImage %InputData\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
		{
			switch (readOp)
			{
				case READOP_IMAGEFETCH:
					if (passNdx)
						return	"           %func_img = OpLoad %Image %InputData2\n";

					if (passSmp && !passImg)
						return	"           %func_tmp = OpLoad %Image %InputData2\n"
								"           %func_smi = OpSampledImage %SampledImage %func_tmp %func_smp\n"
								"           %func_img = OpImage %Image %func_smi\n";

					if (passSmp && passImg)
						return	"           %func_smi = OpSampledImage %SampledImage %func_tmp %func_smp\n"
								"           %func_img = OpImage %Image %func_smi\n";
					break;

				case READOP_IMAGESAMPLE:
				case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
				case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
					if (passNdx)
						return	"           %func_img = OpLoad %Image %InputData2\n"
								"           %func_smp = OpLoad %Sampler %SamplerData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passImg && !passSmp)
						return	"           %func_smp = OpLoad %Sampler %SamplerData\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passSmp && !passImg)
						return	"           %func_img = OpLoad %Image %InputData2\n"
								"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";

					if (passSmp && passImg)
						return	"           %func_smi = OpSampledImage %SampledImage %func_img %func_smp\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}

		default:
			DE_FATAL("Unknown descriptor type");
	}

	return result;
}

// Get variables that are passed to the read function
std::string getFunctionSrcVariableStr (ReadOp readOp, DescriptorType descType, TestType testType)
{
	const bool passImg = ((testType == TESTTYPE_PASS_IMAGE_TO_FUNCTION)			|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));
	const bool passSmp = ((testType == TESTTYPE_PASS_SAMPLER_TO_FUNCTION)		|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));

	string result = "";

	switch (descType)
	{
		case DESCRIPTOR_TYPE_STORAGE_IMAGE:
		{
			switch (readOp)
			{
				case READOP_IMAGEREAD:
					if (passImg)
						result +=	"           %call_img = OpLoad %Image %InputData\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}
		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
		{
			switch (readOp)
			{
				case READOP_IMAGEFETCH:
				case READOP_IMAGESAMPLE:
				case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
				case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
					if (passImg)
						result +=	"           %call_img = OpLoad %Image %InputData\n";

					if (passSmp)
						result +=	"           %call_smp = OpLoad %Sampler %SamplerData\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		{
			break;
		}
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
		{
			switch (readOp)
			{
				case READOP_IMAGEFETCH:
				case READOP_IMAGESAMPLE:
				case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
				case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
					if (passImg)
						result +=	"           %call_img = OpLoad %Image %InputData2\n";

					if (passSmp)
						result +=	"           %call_smp = OpLoad %Sampler %SamplerData\n";
					break;

				default:
					DE_FATAL("Not possible");
			}
			break;
		}
		default:
			DE_FATAL("Unknown descriptor type");
	}

	return result;
}

// Get parameter types for OpTypeFunction
std::string getFunctionParamTypeStr (TestType testType)
{
	const bool passImg = ((testType == TESTTYPE_PASS_IMAGE_TO_FUNCTION)			|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));
	const bool passSmp = ((testType == TESTTYPE_PASS_SAMPLER_TO_FUNCTION)		|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));

	string result = "";

	if (passImg)
		result += " %Image";

	if (passSmp)
		result += " %Sampler";

	return result;
}

// Get argument names for OpFunctionCall
std::string getFunctionSrcParamStr (TestType testType)
{
	const bool passImg = ((testType == TESTTYPE_PASS_IMAGE_TO_FUNCTION)			|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));
	const bool passSmp = ((testType == TESTTYPE_PASS_SAMPLER_TO_FUNCTION)		|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));

	string result = "";

	if (passImg)
		result += " %call_img";

	if (passSmp)
		result += " %call_smp";

	return result;
}

// Get OpFunctionParameters
std::string getFunctionDstParamStr (ReadOp readOp, TestType testType)
{
	const bool passImg = ((testType == TESTTYPE_PASS_IMAGE_TO_FUNCTION)			|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));
	const bool passSmp = ((testType == TESTTYPE_PASS_SAMPLER_TO_FUNCTION)		|| (testType == TESTTYPE_PASS_IMAGE_AND_SAMPLER_TO_FUNCTION));

	string result = "";

	if (readOp == READOP_IMAGESAMPLE)
	{
		if (passImg)
			result +=	"           %func_img = OpFunctionParameter %Image\n";

		if (passSmp)
			result +=	"           %func_smp = OpFunctionParameter %Sampler\n";
	}
	else
	{
		if (passImg && !passSmp)
			result +=	"           %func_img = OpFunctionParameter %Image\n";

		if (passSmp && !passImg)
			result +=	"           %func_smp = OpFunctionParameter %Sampler\n";

		if (passImg && passSmp)
			result +=	"           %func_tmp = OpFunctionParameter %Image\n"
						"           %func_smp = OpFunctionParameter %Sampler\n";
	}

	return result;
}

// Get read operation
std::string getImageReadOpStr (ReadOp readOp, bool useNontemporal = false)
{
	std::string imageOperand = useNontemporal ? " Nontemporal" : "";

	switch (readOp)
	{
		case READOP_IMAGEREAD:
			return std::string("OpImageRead %v4f32 %func_img %coord") + imageOperand;

		case READOP_IMAGEFETCH:
			return std::string("OpImageFetch %v4f32 %func_img %coord") + imageOperand;

		case READOP_IMAGESAMPLE:
			if (useNontemporal)
				return "OpImageSampleExplicitLod %v4f32 %func_smi %normalcoordf Lod|Nontemporal %c_f32_0";
			return "OpImageSampleExplicitLod %v4f32 %func_smi %normalcoordf Lod %c_f32_0";

		case READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD:
			return "OpImageSampleDrefImplicitLod %f32 %func_smi %normalcoordf %c_f32_0_5 Bias %c_f32_0";

		case READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD:
			return "OpImageSampleDrefExplicitLod %f32 %func_smi %normalcoordf %c_f32_0_5 Lod %c_f32_0";

		default:
			DE_FATAL("Unknown readop");
			return "";
	}
}

bool isImageSampleDrefReadOp (ReadOp readOp)
{
	return (readOp == READOP_IMAGESAMPLE_DREF_IMPLICIT_LOD) || (readOp == READOP_IMAGESAMPLE_DREF_EXPLICIT_LOD);
}

static const VkFormat optypeimageFormatMismatchVkFormat[] =
{
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8B8A8_SNORM,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R8G8B8A8_SINT,
	VK_FORMAT_R16G16B16A16_UINT,
	VK_FORMAT_R16G16B16A16_SINT,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_FORMAT_R32_UINT,
	VK_FORMAT_R32_SINT,
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R32G32B32A32_SINT,
	VK_FORMAT_R32G32B32A32_SFLOAT
};

static const size_t optypeimageFormatMismatchFormatCount = sizeof(optypeimageFormatMismatchVkFormat) / sizeof(VkFormat);

static const char *optypeimageFormatMismatchSpirvFormat[] =
{
	"Rgba8",
	"Rgba8Snorm",
	"Rgba8ui",
	"Rgba8i",
	"Rgba16ui",
	"Rgba16i",
	"Rgba16f",
	"R32ui",
	"R32i",
	"Rgba32ui",
	"Rgba32i",
	"Rgba32f"
};

static const char *optypeimageFormatMismatchCase[] =
{
	"rgba8",
	"rgba8snorm",
	"rgba8ui",
	"rgba8i",
	"rgba16ui",
	"rgba16i",
	"rgba16f",
	"r32ui",
	"r32i",
	"rgba32ui",
	"rgba32i",
	"rgba32f"
};

// Get types and pointers for input images and samplers
std::string getImageSamplerTypeStr (DescriptorType descType, ReadOp readOp, deUint32 depthProperty, TestType testType, int formatIndex)
{
	const string imageFormat =	(testType == TESTTYPE_OPTYPEIMAGE_MISMATCH) ? optypeimageFormatMismatchSpirvFormat[formatIndex] :
								isImageSampleDrefReadOp(readOp) ? "R32f" : "Rgba32f";

	switch (descType)
	{
		case DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return	"              %Image = OpTypeImage %f32 2D " + de::toString(depthProperty) + " 0 0 2 " + imageFormat + "\n"
					"           %ImagePtr = OpTypePointer UniformConstant %Image\n"
					"          %InputData = OpVariable %ImagePtr UniformConstant\n";

		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return	"              %Image = OpTypeImage %f32 2D " + de::toString(depthProperty) + " 0 0 1 " + imageFormat + "\n"
					"           %ImagePtr = OpTypePointer UniformConstant %Image\n"
					"          %InputData = OpVariable %ImagePtr UniformConstant\n"

					"            %Sampler = OpTypeSampler\n"
					"         %SamplerPtr = OpTypePointer UniformConstant %Sampler\n"
					"        %SamplerData = OpVariable %SamplerPtr UniformConstant\n"
					"       %SampledImage = OpTypeSampledImage %Image\n";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return	"              %Image = OpTypeImage %f32 2D " + de::toString(depthProperty) + " 0 0 1 " + imageFormat + "\n"
					"       %SampledImage = OpTypeSampledImage %Image\n"
					"         %SamplerPtr = OpTypePointer UniformConstant %SampledImage\n"
					"          %InputData = OpVariable %SamplerPtr UniformConstant\n";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
			return	"              %Image = OpTypeImage %f32 2D " + de::toString(depthProperty) + " 0 0 1 " + imageFormat + "\n"
					"           %ImagePtr = OpTypePointer UniformConstant %Image\n"
					"          %InputData = OpVariable %ImagePtr UniformConstant\n"

					"            %Sampler = OpTypeSampler\n"
					"         %SamplerPtr = OpTypePointer UniformConstant %Sampler\n"
					"        %SamplerData = OpVariable %SamplerPtr UniformConstant\n"
					"       %SampledImage = OpTypeSampledImage %Image\n";

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
			return	"              %Image = OpTypeImage %f32 2D " + de::toString(depthProperty) + " 0 0 1 " + imageFormat + "\n"
					"           %ImagePtr = OpTypePointer UniformConstant %Image\n"
					"          %InputData = OpVariable %ImagePtr UniformConstant\n"
					"         %InputData2 = OpVariable %ImagePtr UniformConstant\n"

					"            %Sampler = OpTypeSampler\n"
					"         %SamplerPtr = OpTypePointer UniformConstant %Sampler\n"
					"        %SamplerData = OpVariable %SamplerPtr UniformConstant\n"
					"       %SamplerData2 = OpVariable %SamplerPtr UniformConstant\n"
					"       %SampledImage = OpTypeSampledImage %Image\n";

		default:
			DE_FATAL("Unknown descriptor type");
			return "";
	}
}

std::string getInterfaceList (DescriptorType descType)
{
	std::string list = " %InputData %OutputData";
	switch (descType)
	{
		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
			list += " %SamplerData";
			break;

		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
			list += " %SamplerData %InputData2 %SamplerData2";
			break;

		default:
			break;
	}

	return list;
}

std::string getSamplerDecoration (DescriptorType descType)
{
	switch (descType)
	{
		// Separate image and sampler
		case DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return	"                       OpDecorate %SamplerData DescriptorSet 0\n"
					"                       OpDecorate %SamplerData Binding 1\n";

		// Combined image sampler with separate variables
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_VARIABLES:
			return	"                       OpDecorate %SamplerData DescriptorSet 0\n"
					"                       OpDecorate %SamplerData Binding 0\n";

		// Two combined image samplers with separate variables
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS:
			return	"                       OpDecorate %SamplerData DescriptorSet 0\n"
					"                       OpDecorate %SamplerData Binding 0\n"
					"                       OpDecorate %InputData2 DescriptorSet 0\n"
					"                       OpDecorate %InputData2 Binding 1\n"
					"                       OpDecorate %SamplerData2 DescriptorSet 0\n"
					"                       OpDecorate %SamplerData2 Binding 1\n";

		default:
			return "";
	}
}

// no-operation verify functon to ignore test results (optypeimage_mismatch)
bool nopVerifyFunction (const std::vector<Resource>&,
						const std::vector<AllocationSp>&,
						const std::vector<Resource>&,
						tcu::TestLog&)
{
	return true;
}

void addComputeImageSamplerTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	de::Random				rnd					(deStringHash(group->getName()));
	const deUint32			numDataPoints		= 64;
	RGBA					defaultColors[4];
	vector<tcu::Vec4>		inputData;

	inputData.reserve(numDataPoints);

	for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
		inputData.push_back(tcu::randomVec4(rnd));

	struct SpirvData
	{
		SpirvVersion	version;
		std::string		postfix;
	};
	const std::vector<SpirvData> spirvDataVect
	{
		{ SPIRV_VERSION_1_0, "" },
		{ SPIRV_VERSION_1_6, "_nontemporal" },
	};

	for (deUint32 opNdx = 0u; opNdx <= READOP_IMAGESAMPLE; opNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> readOpGroup	(new tcu::TestCaseGroup(testCtx, getReadOpName((ReadOp)opNdx), ""));

		for (deUint32 descNdx = 0u; descNdx < DESCRIPTOR_TYPE_LAST; descNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> descGroup (new tcu::TestCaseGroup(testCtx, getDescriptorName((DescriptorType)descNdx), ""));

			for (deUint32 testNdx = 0u; testNdx < TESTTYPE_LAST; testNdx++)
			{
				if (!isValidTestCase((TestType)testNdx, (DescriptorType)descNdx, (ReadOp)opNdx))
					continue;

				deUint32 numFormats = 1;
				if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
					numFormats = optypeimageFormatMismatchFormatCount;

				for (deUint32 formatIndex = 0; formatIndex < numFormats; formatIndex++)
				{
					const std::string	imageSamplerTypes = getImageSamplerTypeStr((DescriptorType)descNdx, (ReadOp)opNdx, DEPTH_PROPERTY_NON_DEPTH, (TestType)testNdx, formatIndex);
					const std::string	functionParamTypes = getFunctionParamTypeStr((TestType)testNdx);

					const std::string	functionSrcVariables = getFunctionSrcVariableStr((ReadOp)opNdx, (DescriptorType)descNdx, (TestType)testNdx);
					const std::string	functionDstVariables = getFunctionDstVariableStr((ReadOp)opNdx, (DescriptorType)descNdx, (TestType)testNdx);

					const std::string	functionSrcParams = getFunctionSrcParamStr((TestType)testNdx);
					const std::string	functionDstParams = getFunctionDstParamStr((ReadOp)opNdx, (TestType)testNdx);

					getDefaultColors(defaultColors);

					ComputeShaderSpec	spec;

					spec.numWorkGroups = IVec3(numDataPoints, 1, 1);

					spec.inputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), getVkDescriptorType((DescriptorType)descNdx)));

					// Separate sampler for sampled images
					if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_SAMPLED_IMAGE)
					{
						vector<tcu::Vec4> unusedData;
						spec.inputs.push_back(Resource(BufferSp(new Vec4Buffer(unusedData))));
						spec.inputs[1].setDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLER);
					}

					// Second combined image sampler with different image data
					if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
					{
						for (size_t i = 0; i < inputData.size(); i++)
							inputData[i] = tcu::Vec4(1.0f) - inputData[i];

						spec.inputs.push_back(BufferSp(new Vec4Buffer(inputData)));
						spec.inputs[1].setDescriptorType(getVkDescriptorType((DescriptorType)descNdx));
					}

					// Shader is expected to pass the input image data to the output buffer
					spec.outputs.push_back(BufferSp(new Vec4Buffer(inputData)));

					const std::string	samplerDecoration	= getSamplerDecoration((DescriptorType)descNdx);

					for (auto spirvData : spirvDataVect)
					{
						spec.spirvVersion = spirvData.version;

						bool		useSpirV16			(spirvData.version == SPIRV_VERSION_1_6);
						std::string	interfaceList		("");
						std::string	outputDecoration	("BufferBlock");
						std::string	outputType			("Uniform");
						std::string	imageReadOp			(getImageReadOpStr((ReadOp)opNdx, useSpirV16));

						// adjust shader code to spv16
						if (useSpirV16)
						{
							interfaceList		= getInterfaceList((DescriptorType)descNdx);
							outputDecoration	= "Block";
							outputType			= "StorageBuffer";
						}

						const string shaderSource =
							"                       OpCapability Shader\n"
							"                  %1 = OpExtInstImport \"GLSL.std.450\"\n"
							"                       OpMemoryModel Logical GLSL450\n"
							"                       OpEntryPoint GLCompute %main \"main\" %id" + interfaceList + "\n"
							"                       OpExecutionMode %main LocalSize 1 1 1\n"
							"                       OpSource GLSL 430\n"
							"                       OpDecorate %id BuiltIn GlobalInvocationId\n"
							"                       OpDecorate %_arr_v4f_u32_64 ArrayStride 16\n"
							"                       OpMemberDecorate %Output 0 Offset 0\n"
							"                       OpDecorate %Output " + outputDecoration + "\n"
							"                       OpDecorate %InputData DescriptorSet 0\n"
							"                       OpDecorate %InputData Binding 0\n"

							+ samplerDecoration +

							"                       OpDecorate %OutputData DescriptorSet 0\n"
							"                       OpDecorate %OutputData Binding " + de::toString(spec.inputs.size()) + "\n"

							"               %void = OpTypeVoid\n"
							"                  %3 = OpTypeFunction %void\n"
							"                %u32 = OpTypeInt 32 0\n"
							"                %i32 = OpTypeInt 32 1\n"
							"                %f32 = OpTypeFloat 32\n"
							" %_ptr_Function_uint = OpTypePointer Function %u32\n"
							"              %v3u32 = OpTypeVector %u32 3\n"
							"   %_ptr_Input_v3u32 = OpTypePointer Input %v3u32\n"
							"                 %id = OpVariable %_ptr_Input_v3u32 Input\n"
							"            %c_f32_0 = OpConstant %f32 0.0\n"
							"            %c_u32_0 = OpConstant %u32 0\n"
							"            %c_i32_0 = OpConstant %i32 0\n"
							"    %_ptr_Input_uint = OpTypePointer Input %u32\n"
							"              %v2u32 = OpTypeVector %u32 2\n"
							"              %v2f32 = OpTypeVector %f32 2\n"
							"              %v4f32 = OpTypeVector %f32 4\n"
							"           %uint_128 = OpConstant %u32 128\n"
							"           %c_u32_64 = OpConstant %u32 64\n"
							"            %c_u32_8 = OpConstant %u32 8\n"
							"            %c_f32_8 = OpConstant %f32 8.0\n"
							"        %c_v2f32_8_8 = OpConstantComposite %v2f32 %c_f32_8 %c_f32_8\n"
							"    %_arr_v4f_u32_64 = OpTypeArray %v4f32 %c_u32_64\n"
							"   %_ptr_Uniform_v4f = OpTypePointer " + outputType + " %v4f32\n"
							"             %Output = OpTypeStruct %_arr_v4f_u32_64\n"
							"%_ptr_Uniform_Output = OpTypePointer " + outputType + " %Output\n"
							"         %OutputData = OpVariable %_ptr_Uniform_Output " + outputType + "\n"

							+ imageSamplerTypes +

							"     %read_func_type = OpTypeFunction %void %u32" + functionParamTypes + "\n"

							"          %read_func = OpFunction %void None %read_func_type\n"
							"           %func_ndx = OpFunctionParameter %u32\n"

							+ functionDstParams +

							"          %funcentry = OpLabel\n"
							"                %row = OpUMod %u32 %func_ndx %c_u32_8\n"
							"                %col = OpUDiv %u32 %func_ndx %c_u32_8\n"
							"              %coord = OpCompositeConstruct %v2u32 %row %col\n"
							"             %coordf = OpConvertUToF %v2f32 %coord\n"
							"       %normalcoordf = OpFDiv %v2f32 %coordf %c_v2f32_8_8\n"

							+ functionDstVariables +

							"              %color = " + imageReadOp + "\n"
							"                 %36 = OpAccessChain %_ptr_Uniform_v4f %OutputData %c_u32_0 %func_ndx\n"
							"                       OpStore %36 %color\n"
							"                       OpReturn\n"
							"                       OpFunctionEnd\n"

							"               %main = OpFunction %void None %3\n"
							"                  %5 = OpLabel\n"
							"                  %i = OpVariable %_ptr_Function_uint Function\n"
							"                 %14 = OpAccessChain %_ptr_Input_uint %id %c_u32_0\n"
							"                 %15 = OpLoad %u32 %14\n"
							"                       OpStore %i %15\n"
							"              %index = OpLoad %u32 %14\n"

							+ functionSrcVariables +

							"                %res = OpFunctionCall %void %read_func %index" + functionSrcParams + "\n"
							"                       OpReturn\n"
							"                       OpFunctionEnd\n";

						spec.assembly = shaderSource;

						string testname = getTestTypeName((TestType)testNdx);

						if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
						{
							// If testing for mismatched optypeimage, ignore the
							// result (we're only interested to see if we crash)
							spec.verifyIO = nopVerifyFunction;

							testname = testname + string("_") + string(optypeimageFormatMismatchCase[formatIndex]);
						}

						testname += spirvData.postfix;
						descGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testname.c_str(), "", spec));
					}
				}
			}
			readOpGroup->addChild(descGroup.release());
		}
		group->addChild(readOpGroup.release());
	}
}

map<string, string> generateGraphicsImageSamplerSource (ReadOp readOp, DescriptorType descriptorType, TestType testType, DepthProperty depthProperty, deUint32 outputBinding, deUint32 formatIndex)
{
	map<string, string>	source;

	const std::string	imageReadOp				= getImageReadOpStr(readOp);
	const std::string	imageSamplerTypes		= getImageSamplerTypeStr(descriptorType, readOp, depthProperty, testType, formatIndex);
	const std::string	functionParamTypes		= getFunctionParamTypeStr(testType);
	const std::string	functionSrcVariables	= getFunctionSrcVariableStr(readOp, descriptorType, testType);
	const std::string	functionDstVariables	= getFunctionDstVariableStr(readOp, descriptorType, testType);
	const std::string	functionSrcParams		= getFunctionSrcParamStr(testType);
	const std::string	functionDstParams		= getFunctionDstParamStr(readOp, testType);
	const std::string	samplerDecoration		= getSamplerDecoration(descriptorType);
	const std::string	outputUniformPtr		= isImageSampleDrefReadOp(readOp) ? "%_ptr_Uniform_f32" : "%_ptr_Uniform_v4f32";
	const std::string	outputArrayStruct		= isImageSampleDrefReadOp(readOp) ? "%_arr_f32_u32_64" : "%_arr_v4f32_u32_64";

	source["pre_main"]	=
		"           %c_u32_64 = OpConstant %u32 64\n"
		"           %c_i32_64 = OpConstant %i32 64\n"
		"            %c_i32_8 = OpConstant %i32 8\n"
		"        %c_v2f32_8_8 = OpConstantComposite %v2f32 %c_f32_8 %c_f32_8\n"

		"    %_arr_f32_u32_64 = OpTypeArray %f32 %c_u32_64\n"
		"  %_arr_v4f32_u32_64 = OpTypeArray %v4f32 %c_u32_64\n"
		"   %_ptr_Uniform_f32 = OpTypePointer Uniform %f32\n"
		" %_ptr_Uniform_v4f32 = OpTypePointer Uniform %v4f32\n"

		"             %Output = OpTypeStruct " + outputArrayStruct + "\n"
		"%_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		"         %OutputData = OpVariable %_ptr_Uniform_Output Uniform\n"

		+ imageSamplerTypes +

		"     %read_func_type = OpTypeFunction %void %i32" + functionParamTypes + "\n";

	source["decoration"]	=
		"                       OpDecorate %_arr_f32_u32_64 ArrayStride 4\n"
		"                       OpDecorate %_arr_v4f32_u32_64 ArrayStride 16\n"
		"                       OpMemberDecorate %Output 0 Offset 0\n"
		"                       OpDecorate %Output BufferBlock\n"
		"                       OpDecorate %InputData DescriptorSet 0\n"
		"                       OpDecorate %InputData Binding 0\n"

		+ samplerDecoration +

		"OpDecorate %OutputData DescriptorSet 0\n"
		"OpDecorate %OutputData Binding " + de::toString(outputBinding) + "\n";

	source["testfun"]	=
		"          %read_func = OpFunction %void None %read_func_type\n"
		"           %func_ndx = OpFunctionParameter %i32\n"

		+ functionDstParams +

		"          %funcentry = OpLabel\n"

		"                %row = OpSRem %i32 %func_ndx %c_i32_8\n"
		"                %col = OpSDiv %i32 %func_ndx %c_i32_8\n"
		"              %coord = OpCompositeConstruct %v2i32 %row %col\n"
		"             %coordf = OpConvertSToF %v2f32 %coord\n"
		"       %normalcoordf = OpFDiv %v2f32 %coordf %c_v2f32_8_8\n"

		+ functionDstVariables +

		"              %color = " + imageReadOp + "\n"
		"                 %36 = OpAccessChain " + outputUniformPtr + " %OutputData %c_i32_0 %func_ndx\n"
		"                       OpStore %36 %color\n"

		"                       OpReturn\n"
		"                       OpFunctionEnd\n"

		"          %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"              %param = OpFunctionParameter %v4f32\n"

		"              %entry = OpLabel\n"

		"                  %i = OpVariable %fp_i32 Function\n"
		"                       OpStore %i %c_i32_0\n"
		"                       OpBranch %loop\n"

		"               %loop = OpLabel\n"
		"                 %15 = OpLoad %i32 %i\n"
		"                 %lt = OpSLessThan %bool %15 %c_i32_64\n"
		"                       OpLoopMerge %merge %inc None\n"
		"                       OpBranchConditional %lt %write %merge\n"

		"              %write = OpLabel\n"
		"              %index = OpLoad %i32 %i\n"

		+ functionSrcVariables +

		"                %res = OpFunctionCall %void %read_func %index" + functionSrcParams + "\n"
		"                       OpBranch %inc\n"

		"                %inc = OpLabel\n"

		"                 %37 = OpLoad %i32 %i\n"
		"                 %39 = OpIAdd %i32 %37 %c_i32_1\n"
		"                       OpStore %i %39\n"
		"                       OpBranch %loop\n"

		"              %merge = OpLabel\n"
		"                       OpReturnValue %param\n"
		"                       OpFunctionEnd\n";

	return source;
}

void addGraphicsImageSamplerTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&			testCtx				= group->getTestContext();

	de::Random					rnd					(deStringHash(group->getName()));
	const deUint32				numDataPoints		= 64;
	RGBA						defaultColors[4];

	SpecConstants				noSpecConstants;
	PushConstants				noPushConstants;
	GraphicsInterfaces			noInterfaces;
	std::vector<std::string>	noExtensions;
	VulkanFeatures				vulkanFeatures		= VulkanFeatures();

	vector<tcu::Vec4> inputData(numDataPoints);
	for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
		inputData[numIdx] = tcu::randomVec4(rnd);

	for (deUint32 opNdx = 0u; opNdx <= READOP_IMAGESAMPLE; opNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> readOpGroup	(new tcu::TestCaseGroup(testCtx, getReadOpName((ReadOp)opNdx), ""));

		for (deUint32 descNdx = 0u; descNdx < DESCRIPTOR_TYPE_LAST; descNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> descGroup (new tcu::TestCaseGroup(testCtx, getDescriptorName((DescriptorType)descNdx), ""));

			for (deUint32 testNdx = 0u; testNdx < TESTTYPE_LAST; testNdx++)
			{
				if (!isValidTestCase((TestType)testNdx, (DescriptorType)descNdx, (ReadOp)opNdx))
					continue;

				deUint32 formatCount = 1;
				if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
					formatCount = optypeimageFormatMismatchFormatCount;

				// this group is only used for optypeimage_mismatch case
				de::MovePtr<tcu::TestCaseGroup> testtypeGroup(new tcu::TestCaseGroup(testCtx, getTestTypeName((TestType)testNdx), ""));

				for (deUint32 formatIndex = 0; formatIndex < formatCount; formatIndex++)
				{
					// optypeimage_mismatch uses an additional level of test hierarchy
					const char *groupname = testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH ? optypeimageFormatMismatchCase[formatIndex] : getTestTypeName((TestType)testNdx);
					de::MovePtr<tcu::TestCaseGroup>	typeGroup(new tcu::TestCaseGroup(testCtx, groupname, ""));

					GraphicsResources				resources;

					resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), getVkDescriptorType((DescriptorType)descNdx)));

					// Separate sampler for sampled images
					if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_SAMPLED_IMAGE)
					{
						vector<tcu::Vec4> unusedData;
						resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(unusedData)), VK_DESCRIPTOR_TYPE_SAMPLER));
					}

					// Second combined image sampler with different image data
					if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
					{
						for (size_t i = 0; i < inputData.size(); i++)
							inputData[i] = tcu::Vec4(1.0f) - inputData[i];

						resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), getVkDescriptorType((DescriptorType)descNdx)));
					}

					// Shader is expected to pass the input image data to output buffer
					resources.outputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

					getDefaultColors(defaultColors);

					const map<string, string>		fragments = generateGraphicsImageSamplerSource((ReadOp)opNdx, (DescriptorType)descNdx, (TestType)testNdx, DEPTH_PROPERTY_NON_DEPTH, (deUint32)resources.inputs.size(), (deUint32)((formatIndex + 1) % optypeimageFormatMismatchFormatCount));

					// If testing for mismatched optypeimage, ignore the rendered
					// result (we're only interested to see if we crash)
					if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
					{
						resources.verifyIO = nopVerifyFunction;
						resources.inputFormat = optypeimageFormatMismatchVkFormat[formatIndex];
					}

					vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
					vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_FALSE;
					createTestForStage(VK_SHADER_STAGE_VERTEX_BIT, "shader_vert", defaultColors, defaultColors, fragments, noSpecConstants,
						noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, typeGroup.get());

					createTestForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "shader_tessc", defaultColors, defaultColors, fragments, noSpecConstants,
						noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, typeGroup.get());

					createTestForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "shader_tesse", defaultColors, defaultColors, fragments, noSpecConstants,
						noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, typeGroup.get());

					createTestForStage(VK_SHADER_STAGE_GEOMETRY_BIT, "shader_geom", defaultColors, defaultColors, fragments, noSpecConstants,
						noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, typeGroup.get());

					vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
					vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;
					createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shader_frag", defaultColors, defaultColors, fragments, noSpecConstants,
						noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, typeGroup.get());

					if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
						testtypeGroup->addChild(typeGroup.release());
					else
						descGroup->addChild(typeGroup.release());
				}
				if (testNdx == TESTTYPE_OPTYPEIMAGE_MISMATCH)
					descGroup->addChild(testtypeGroup.release());
			}
			readOpGroup->addChild(descGroup.release());
		}
		group->addChild(readOpGroup.release());
	}
}

bool verifyDepthCompareResult (const std::vector<Resource>&		originalFloats,
							   const std::vector<AllocationSp>&	outputAllocs,
							   const std::vector<Resource>&		expectedOutputs,
							   tcu::TestLog&)
{
	DE_UNREF(originalFloats);

	if (outputAllocs.size() != expectedOutputs.size())
		return false;

	vector<deUint8>	expectedBytes;
	expectedOutputs[0].getBytes(expectedBytes);

	const float*	returnedAsFloat	= static_cast<const float*>(outputAllocs[0]->getHostPtr());
	const float*	expectedAsFloat	= reinterpret_cast<const float*>(&expectedBytes.front());

	for (deUint32 elementNdx = 0; elementNdx < static_cast<deUint32>(expectedBytes.size() / sizeof(float)); ++elementNdx)
	{
		const float input	= expectedAsFloat[elementNdx];
		const float result	= returnedAsFloat[elementNdx];

		// VK_COMPARE_OP_LESS: D = 1.0 if D < Dref, otherwise D = 0.0
		if ((input < 0.5f && result != 0.0f) || (input >= 0.5f && result != 1.0f))
			return false;
	}

	return true;
}

void addGraphicsDepthPropertyTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&			testCtx				= group->getTestContext();

	de::Random					rnd					(deStringHash(group->getName()));
	const deUint32				numDataPoints		= 64;
	RGBA						defaultColors[4];
	vector<Vec4>				inputDataVec4;

	SpecConstants				noSpecConstants;
	PushConstants				noPushConstants;
	GraphicsInterfaces			noInterfaces;
	std::vector<std::string>	noExtensions;
	VulkanFeatures				vulkanFeatures		= VulkanFeatures();

	vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
	vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;

	inputDataVec4.reserve(numDataPoints);

	for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
		inputDataVec4.push_back(tcu::randomVec4(rnd));

	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "depth_property", ""));

	for (deUint32 propertyNdx = 0u; propertyNdx < DEPTH_PROPERTY_LAST; propertyNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> depthPropertyGroup (new tcu::TestCaseGroup(testCtx, getDepthPropertyName((DepthProperty)propertyNdx), ""));

		for (deUint32 opNdx = 0u; opNdx < READOP_LAST; opNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> readOpGroup	(new tcu::TestCaseGroup(testCtx, getReadOpName((ReadOp)opNdx), ""));

			for (deUint32 descNdx = DESCRIPTOR_TYPE_SAMPLED_IMAGE; descNdx < DESCRIPTOR_TYPE_LAST; descNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> descGroup (new tcu::TestCaseGroup(testCtx, getDescriptorName((DescriptorType)descNdx), ""));

				if (!isValidTestCase(TESTTYPE_LOCAL_VARIABLES, (DescriptorType)descNdx, (ReadOp)opNdx))
					continue;

				const VkFormat				imageFormat			= getImageFormat((ReadOp)opNdx);
				const bool					hasDpethComponent	= tcu::hasDepthComponent(vk::mapVkFormat(imageFormat).order);

				GraphicsResources			resources;
				resources.inputFormat = imageFormat;

				std::vector<Vec4>			inputData			= inputDataVec4;

				// Depth images have one channel, thus only needing 1/4 of the data
				if (hasDpethComponent)
					inputData.resize(numDataPoints / 4u);

				resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), getVkDescriptorType((DescriptorType)descNdx)));

				// Separate sampler for sampled images
				if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				{
					vector<Vec4> unusedData;
					resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(unusedData)), VK_DESCRIPTOR_TYPE_SAMPLER));
				}

				// Second combined image sampler with different image data
				if ((DescriptorType)descNdx == DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_SEPARATE_DESCRIPTORS)
				{
					for (size_t i = 0; i < inputData.size(); i++)
						inputData[i] = Vec4(1.0f) - inputData[i];

					resources.inputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), getVkDescriptorType((DescriptorType)descNdx)));
				}

				// Read image without depth reference: shader is expected to pass the input image data to output buffer
				resources.outputs.push_back(Resource(BufferSp(new Vec4Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				// Read image with depth reference: shader is expected to pass the depth comparison result to output buffer
				if (hasDpethComponent)
					resources.verifyIO = verifyDepthCompareResult;

				const map<string, string>	fragments			= generateGraphicsImageSamplerSource((ReadOp)opNdx, (DescriptorType)descNdx, TESTTYPE_LOCAL_VARIABLES, (DepthProperty)propertyNdx, (deUint32)resources.inputs.size(), 0);

				getDefaultColors(defaultColors);

				createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shader_frag", defaultColors, defaultColors, fragments, noSpecConstants,
								   noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, descGroup.get());

				readOpGroup->addChild(descGroup.release());
			}
			depthPropertyGroup->addChild(readOpGroup.release());
		}
		testGroup->addChild(depthPropertyGroup.release());
	}
	group->addChild(testGroup.release());
}
} // anonymous

tcu::TestCaseGroup* createImageSamplerComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "image_sampler", "Compute tests for combining images and samplers."));
	addComputeImageSamplerTest(group.get());

	return group.release();
}

tcu::TestCaseGroup* createImageSamplerGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "image_sampler", "Graphics tests for combining images and samplers."));

	addGraphicsImageSamplerTest(group.get());
	addGraphicsDepthPropertyTest(group.get());

	return group.release();
}

} // SpirVAssembly
} // vkt
