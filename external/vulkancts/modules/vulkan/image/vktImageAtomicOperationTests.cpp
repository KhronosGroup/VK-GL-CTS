/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktImageAtomicOperationTests.cpp
 * \brief Image atomic operation tests
 *//*--------------------------------------------------------------------*/

#include "vktImageAtomicOperationTests.hpp"
#include "vktImageAtomicSpirvShaders.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuStringTemplate.hpp"

namespace vkt
{
namespace image
{
namespace
{

using namespace vk;
using namespace std;
using de::toString;

using tcu::TextureFormat;
using tcu::IVec2;
using tcu::IVec3;
using tcu::UVec3;
using tcu::Vec4;
using tcu::IVec4;
using tcu::UVec4;
using tcu::CubeFace;
using tcu::Texture1D;
using tcu::Texture2D;
using tcu::Texture3D;
using tcu::Texture2DArray;
using tcu::TextureCube;
using tcu::PixelBufferAccess;
using tcu::ConstPixelBufferAccess;
using tcu::Vector;
using tcu::TestContext;

enum
{
	NUM_INVOCATIONS_PER_PIXEL = 5u
};

enum AtomicOperation
{
	ATOMIC_OPERATION_ADD = 0,
	ATOMIC_OPERATION_SUB,
	ATOMIC_OPERATION_INC,
	ATOMIC_OPERATION_DEC,
	ATOMIC_OPERATION_MIN,
	ATOMIC_OPERATION_MAX,
	ATOMIC_OPERATION_AND,
	ATOMIC_OPERATION_OR,
	ATOMIC_OPERATION_XOR,
	ATOMIC_OPERATION_EXCHANGE,
	ATOMIC_OPERATION_COMPARE_EXCHANGE,

	ATOMIC_OPERATION_LAST
};

enum class ShaderReadType
{
	NORMAL = 0,
	SPARSE,
};

enum class ImageBackingType
{
	NORMAL = 0,
	SPARSE,
};

static string getCoordStr (const ImageType		imageType,
						   const std::string&	x,
						   const std::string&	y,
						   const std::string&	z)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return x;
		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return string("ivec2(" + x + "," + y + ")");
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return string("ivec3(" + x + "," + y + "," + z + ")");
		default:
			DE_ASSERT(false);
			return "";
	}
}

static string getComponentTypeStr (deUint32 componentWidth, bool intFormat, bool uintFormat, bool floatFormat)
{
	DE_ASSERT(intFormat || uintFormat || floatFormat);

	const bool is64 = (componentWidth == 64);

	if (intFormat)
		return (is64 ? "int64_t" : "int");
	if (uintFormat)
		return (is64 ? "uint64_t" : "uint");
	if (floatFormat)
		return (is64 ? "double" : "float");

	return "";
}

static string getVec4TypeStr (deUint32 componentWidth, bool intFormat, bool uintFormat, bool floatFormat)
{
	DE_ASSERT(intFormat || uintFormat || floatFormat);

	const bool is64 = (componentWidth == 64);

	if (intFormat)
		return (is64 ? "i64vec4" : "ivec4");
	if (uintFormat)
		return (is64 ? "u64vec4" : "uvec4");
	if (floatFormat)
		return (is64 ? "f64vec4" : "vec4");

	return "";
}

static string getAtomicFuncArgumentShaderStr (const AtomicOperation	op,
											  const string&			x,
											  const string&			y,
											  const string&			z,
											  const IVec3&			gridSize)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:
		case ATOMIC_OPERATION_AND:
		case ATOMIC_OPERATION_OR:
		case ATOMIC_OPERATION_XOR:
			return string("(" + x + "*" + x + " + " + y + "*" + y + " + " + z + "*" + z + ")");
		case ATOMIC_OPERATION_MIN:
		case ATOMIC_OPERATION_MAX:
			// multiply by (1-2*(value % 2) to make half of the data negative
			// this will result in generating large numbers for uint formats
			return string("((1 - 2*(" + x + " % 2)) * (" + x + "*" + x + " + " + y + "*" + y + " + " + z + "*" + z + "))");
		case ATOMIC_OPERATION_EXCHANGE:
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:
			return string("((" + z + "*" + toString(gridSize.x()) + " + " + x + ")*" + toString(gridSize.y()) + " + " + y + ")");
		default:
			DE_ASSERT(false);
			return "";
	}
}

static string getAtomicOperationCaseName (const AtomicOperation op)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:				return string("add");
		case ATOMIC_OPERATION_SUB:				return string("sub");
		case ATOMIC_OPERATION_INC:				return string("inc");
		case ATOMIC_OPERATION_DEC:				return string("dec");
		case ATOMIC_OPERATION_MIN:				return string("min");
		case ATOMIC_OPERATION_MAX:				return string("max");
		case ATOMIC_OPERATION_AND:				return string("and");
		case ATOMIC_OPERATION_OR:				return string("or");
		case ATOMIC_OPERATION_XOR:				return string("xor");
		case ATOMIC_OPERATION_EXCHANGE:			return string("exchange");
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:	return string("compare_exchange");
		default:
			DE_ASSERT(false);
			return "";
	}
}

static string getAtomicOperationShaderFuncName (const AtomicOperation op)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:				return string("imageAtomicAdd");
		case ATOMIC_OPERATION_MIN:				return string("imageAtomicMin");
		case ATOMIC_OPERATION_MAX:				return string("imageAtomicMax");
		case ATOMIC_OPERATION_AND:				return string("imageAtomicAnd");
		case ATOMIC_OPERATION_OR:				return string("imageAtomicOr");
		case ATOMIC_OPERATION_XOR:				return string("imageAtomicXor");
		case ATOMIC_OPERATION_EXCHANGE:			return string("imageAtomicExchange");
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:	return string("imageAtomicCompSwap");
		default:
			DE_ASSERT(false);
			return "";
	}
}

template <typename T>
T getOperationInitialValue (const AtomicOperation op)
{
	switch (op)
	{
		// \note 18 is just an arbitrary small nonzero value.
		case ATOMIC_OPERATION_ADD:				return 18;
		case ATOMIC_OPERATION_INC:				return 18;
		case ATOMIC_OPERATION_SUB:				return (1 << 24) - 1;
		case ATOMIC_OPERATION_DEC:				return (1 << 24) - 1;
		case ATOMIC_OPERATION_MIN:				return (1 << 15) - 1;
		case ATOMIC_OPERATION_MAX:				return 18;
		case ATOMIC_OPERATION_AND:				return (1 << 15) - 1;
		case ATOMIC_OPERATION_OR:				return 18;
		case ATOMIC_OPERATION_XOR:				return 18;
		case ATOMIC_OPERATION_EXCHANGE:			return 18;
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:	return 18;
		default:
			DE_ASSERT(false);
			return 0xFFFFFFFF;
	}
}

template <>
deInt64 getOperationInitialValue<deInt64>(const AtomicOperation op)
{
	switch (op)
	{
		// \note 0x000000BEFFFFFF18 is just an arbitrary nonzero value.
		case ATOMIC_OPERATION_ADD:				return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_INC:				return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_SUB:				return (1ull << 56) - 1;
		case ATOMIC_OPERATION_DEC:				return (1ull << 56) - 1;
		case ATOMIC_OPERATION_MIN:				return (1ull << 47) - 1;
		case ATOMIC_OPERATION_MAX:				return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_AND:				return (1ull << 47) - 1;
		case ATOMIC_OPERATION_OR:				return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_XOR:				return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_EXCHANGE:			return 0x000000BEFFFFFF18;
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:	return 0x000000BEFFFFFF18;
		default:
			DE_ASSERT(false);
			return 0xFFFFFFFFFFFFFFFF;
	}
}

template <>
deUint64 getOperationInitialValue<deUint64>(const AtomicOperation op)
{
	return (deUint64)getOperationInitialValue<deInt64>(op);
}


template <typename T>
static T getAtomicFuncArgument (const AtomicOperation	op,
								const IVec3&			invocationID,
								const IVec3&			gridSize)
{
	const T x = static_cast<T>(invocationID.x());
	const T y = static_cast<T>(invocationID.y());
	const T z = static_cast<T>(invocationID.z());

	switch (op)
	{
		// \note Fall-throughs.
		case ATOMIC_OPERATION_ADD:
		case ATOMIC_OPERATION_SUB:
		case ATOMIC_OPERATION_AND:
		case ATOMIC_OPERATION_OR:
		case ATOMIC_OPERATION_XOR:
			return x*x + y*y + z*z;
		case ATOMIC_OPERATION_INC:
		case ATOMIC_OPERATION_DEC:
			return 1;
		case ATOMIC_OPERATION_MIN:
		case ATOMIC_OPERATION_MAX:
			// multiply half of the data by -1
			return (1-2*(x % 2))*(x*x + y*y + z*z);
		case ATOMIC_OPERATION_EXCHANGE:
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:
			return (z*static_cast<T>(gridSize.x()) + x)*static_cast<T>(gridSize.y()) + y;
		default:
			DE_ASSERT(false);
			return -1;
	}
}

//! An order-independent operation is one for which the end result doesn't depend on the order in which the operations are carried (i.e. is both commutative and associative).
static bool isOrderIndependentAtomicOperation (const AtomicOperation op)
{
	return	op == ATOMIC_OPERATION_ADD ||
			op == ATOMIC_OPERATION_SUB ||
			op == ATOMIC_OPERATION_INC ||
			op == ATOMIC_OPERATION_DEC ||
			op == ATOMIC_OPERATION_MIN ||
			op == ATOMIC_OPERATION_MAX ||
			op == ATOMIC_OPERATION_AND ||
			op == ATOMIC_OPERATION_OR ||
			op == ATOMIC_OPERATION_XOR;
}

//! Checks if the operation needs an SPIR-V shader.
static bool isSpirvAtomicOperation (const AtomicOperation op)
{
	return	op == ATOMIC_OPERATION_SUB ||
			op == ATOMIC_OPERATION_INC ||
			op == ATOMIC_OPERATION_DEC;
}

//! Returns the SPIR-V assembler name of the given operation.
static std::string getSpirvAtomicOpName (const AtomicOperation op)
{
	switch (op)
	{
	case ATOMIC_OPERATION_SUB:	return "OpAtomicISub";
	case ATOMIC_OPERATION_INC:	return "OpAtomicIIncrement";
	case ATOMIC_OPERATION_DEC:	return "OpAtomicIDecrement";
	default:					break;
	}

	DE_ASSERT(false);
	return "";
}

//! Returns true if the given SPIR-V operation does not need the last argument, compared to OpAtomicIAdd.
static bool isSpirvAtomicNoLastArgOp (const AtomicOperation op)
{
	switch (op)
	{
	case ATOMIC_OPERATION_SUB:	return false;
	case ATOMIC_OPERATION_INC:	// fallthrough
	case ATOMIC_OPERATION_DEC:	return true;
	default:					break;
	}

	DE_ASSERT(false);
	return false;
}

//! Computes the result of an atomic operation where "a" is the data operated on and "b" is the parameter to the atomic function.
template <typename T>
static T computeBinaryAtomicOperationResult (const AtomicOperation op, const T a, const T b)
{
	switch (op)
	{
		case ATOMIC_OPERATION_INC:				// fallthrough.
		case ATOMIC_OPERATION_ADD:				return a + b;
		case ATOMIC_OPERATION_DEC:				// fallthrough.
		case ATOMIC_OPERATION_SUB:				return a - b;
		case ATOMIC_OPERATION_MIN:				return de::min(a, b);
		case ATOMIC_OPERATION_MAX:				return de::max(a, b);
		case ATOMIC_OPERATION_AND:				return a & b;
		case ATOMIC_OPERATION_OR:				return a | b;
		case ATOMIC_OPERATION_XOR:				return a ^ b;
		case ATOMIC_OPERATION_EXCHANGE:			return b;
		case ATOMIC_OPERATION_COMPARE_EXCHANGE:	return (a == (sizeof(T) == 8 ? 0xBEFFFFFF18 : 18)) ? b : a;
		default:
			DE_ASSERT(false);
			return -1;
	}
}

VkImageUsageFlags getUsageFlags (bool useTransfer)
{
	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_STORAGE_BIT;

	if (useTransfer)
		usageFlags |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	return usageFlags;
}

void AddFillReadShader (SourceCollections&			sourceCollections,
						const ImageType&			imageType,
						const tcu::TextureFormat&	format,
						const string&				componentType,
						const string&				vec4Type)
{
	const string	imageInCoord			= getCoordStr(imageType, "gx", "gy", "gz");
	const string	shaderImageFormatStr	= getShaderImageFormatQualifier(format);
	const string	shaderImageTypeStr		= getShaderImageType(format, imageType);
	const auto		componentWidth			= getFormatComponentWidth(mapTextureFormat(format), 0u);
	const string	extensions				= ((componentWidth == 64u)
											?	"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
												"#extension GL_EXT_shader_image_int64 : require\n"
											:	"");


	const string fillShader =	"#version 450\n"
								+ extensions +
								"precision highp " + shaderImageTypeStr + ";\n"
								"\n"
								"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
								"layout (" + shaderImageFormatStr + ", binding=0) coherent uniform " + shaderImageTypeStr + " u_resultImage;\n"
								"\n"
								"layout(std430, binding = 1) buffer inputBuffer\n"
								"{\n"
								"	"+ componentType + " data[];\n"
								"} inBuffer;\n"
								"\n"
								"void main(void)\n"
								"{\n"
								"	int gx = int(gl_GlobalInvocationID.x);\n"
								"	int gy = int(gl_GlobalInvocationID.y);\n"
								"	int gz = int(gl_GlobalInvocationID.z);\n"
								"	uint index = gx + (gy * gl_NumWorkGroups.x) + (gz *gl_NumWorkGroups.x * gl_NumWorkGroups.y);\n"
								"	imageStore(u_resultImage, " + imageInCoord + ", " + vec4Type + "(inBuffer.data[index]));\n"
								"}\n";

	const string readShader =	"#version 450\n"
								+ extensions +
								"precision highp " + shaderImageTypeStr + ";\n"
								"\n"
								"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
								"layout (" + shaderImageFormatStr + ", binding=0) coherent uniform " + shaderImageTypeStr + " u_resultImage;\n"
								"\n"
								"layout(std430, binding = 1) buffer outputBuffer\n"
								"{\n"
								"	" + componentType + " data[];\n"
								"} outBuffer;\n"
								"\n"
								"void main(void)\n"
								"{\n"
								"	int gx = int(gl_GlobalInvocationID.x);\n"
								"	int gy = int(gl_GlobalInvocationID.y);\n"
								"	int gz = int(gl_GlobalInvocationID.z);\n"
								"	uint index = gx + (gy * gl_NumWorkGroups.x) + (gz *gl_NumWorkGroups.x * gl_NumWorkGroups.y);\n"
								"	outBuffer.data[index] = imageLoad(u_resultImage, " + imageInCoord + ").x;\n"
								"}\n";


	if ((imageType != IMAGE_TYPE_1D) &&
		(imageType != IMAGE_TYPE_1D_ARRAY) &&
		(imageType != IMAGE_TYPE_BUFFER))
	{
		const string readShaderResidency  = "#version 450\n"
											"#extension GL_ARB_sparse_texture2 : require\n"
											+ extensions +
											"precision highp " + shaderImageTypeStr + ";\n"
											"\n"
											"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
											"layout (" + shaderImageFormatStr + ", binding=0) coherent uniform " + shaderImageTypeStr + " u_resultImage;\n"
											"\n"
											"layout(std430, binding = 1) buffer outputBuffer\n"
											"{\n"
											"	" + componentType + " data[];\n"
											"} outBuffer;\n"
											"\n"
											"void main(void)\n"
											"{\n"
											"	int gx = int(gl_GlobalInvocationID.x);\n"
											"	int gy = int(gl_GlobalInvocationID.y);\n"
											"	int gz = int(gl_GlobalInvocationID.z);\n"
											"	uint index = gx + (gy * gl_NumWorkGroups.x) + (gz *gl_NumWorkGroups.x * gl_NumWorkGroups.y);\n"
											"	outBuffer.data[index] = imageLoad(u_resultImage, " + imageInCoord + ").x;\n"
											"	" + vec4Type + " sparseValue;\n"
											"	sparseImageLoadARB(u_resultImage, " + imageInCoord + ", sparseValue);\n"
											"	if (outBuffer.data[index] != sparseValue.x)\n"
											"		outBuffer.data[index] = " + vec4Type + "(1234).x;\n"
											"}\n";

		sourceCollections.glslSources.add("readShaderResidency") << glu::ComputeSource(readShaderResidency.c_str()) << vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}

	sourceCollections.glslSources.add("fillShader") << glu::ComputeSource(fillShader.c_str()) << vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	sourceCollections.glslSources.add("readShader") << glu::ComputeSource(readShader.c_str()) << vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
}

//! Prepare the initial data for the image
static void initDataForImage (const VkDevice			device,
							  const DeviceInterface&	deviceInterface,
							  const TextureFormat&		format,
							  const AtomicOperation		operation,
							  const tcu::UVec3&			gridSize,
							  BufferWithMemory&			buffer)
{
	Allocation&				bufferAllocation	= buffer.getAllocation();
	const VkFormat			imageFormat			= mapTextureFormat(format);
	tcu::PixelBufferAccess	pixelBuffer			(format, gridSize.x(), gridSize.y(), gridSize.z(), bufferAllocation.getHostPtr());

	if (imageFormat == VK_FORMAT_R64_UINT || imageFormat == VK_FORMAT_R64_SINT)
	{
		const deInt64 initialValue(getOperationInitialValue<deInt64>(operation));

		for (deUint32 z = 0; z < gridSize.z(); z++)
		for (deUint32 y = 0; y < gridSize.y(); y++)
		for (deUint32 x = 0; x < gridSize.x(); x++)
		{
			*((deInt64*)pixelBuffer.getPixelPtr(x, y, z)) = initialValue;
		}
	}
	else
	{
		const tcu::IVec4 initialValue(getOperationInitialValue<deInt32>(operation));

		for (deUint32 z = 0; z < gridSize.z(); z++)
		for (deUint32 y = 0; y < gridSize.y(); y++)
		for (deUint32 x = 0; x < gridSize.x(); x++)
		{
			pixelBuffer.setPixel(initialValue, x, y, z);
		}
	}

	flushAlloc(deviceInterface, device, bufferAllocation);
}

void commonCheckSupport (Context& context, const tcu::TextureFormat& tcuFormat, VkImageTiling tiling, ImageType imageType, const tcu::UVec3& imageSize, AtomicOperation operation, bool useTransfer, ShaderReadType readType, ImageBackingType backingType)
{
	const VkFormat				format				= mapTextureFormat(tcuFormat);
	const VkImageType			vkImgType			= mapImageType(imageType);
	const VkFormatFeatureFlags	texelBufferSupport	= (VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT);

	const auto& vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();
	const auto usageFlags = getUsageFlags(useTransfer);

	VkImageFormatProperties	vkImageFormatProperties;
	const auto result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, vkImgType, tiling, usageFlags, 0, &vkImageFormatProperties);
	if (result != VK_SUCCESS) {
		if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Format unsupported for tiling");
		else
			TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
	}

	if (vkImageFormatProperties.maxArrayLayers < (uint32_t)getNumLayers(imageType, imageSize)) {
		TCU_THROW(NotSupportedError, "This format and tiling combination does not support this number of aray layers");
	}

	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
																						context.getPhysicalDevice(), format);
	if ((imageType == IMAGE_TYPE_BUFFER) &&
		((formatProperties.bufferFeatures & texelBufferSupport) != texelBufferSupport))
		TCU_THROW(NotSupportedError, "Atomic storage texel buffers not supported");

	const VkFormatFeatureFlags requiredFeaturesLinear = (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
	if (tiling == vk::VK_IMAGE_TILING_LINEAR &&
			((formatProperties.linearTilingFeatures & requiredFeaturesLinear) != requiredFeaturesLinear)
	) {
		TCU_THROW(NotSupportedError, "Format doesn't support atomic storage with linear tiling");
	}

	if (imageType == IMAGE_TYPE_CUBE_ARRAY)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

#ifndef CTS_USES_VULKANSC
	if (backingType == ImageBackingType::SPARSE)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

		switch (vkImgType)
		{
		case VK_IMAGE_TYPE_2D:	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D); break;
		case VK_IMAGE_TYPE_3D:	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE3D); break;
		default:				DE_ASSERT(false); break;
		}

		if (!checkSparseImageFormatSupport(context.getPhysicalDevice(), context.getInstanceInterface(), format, vkImgType, VK_SAMPLE_COUNT_1_BIT, usageFlags, tiling))
			TCU_THROW(NotSupportedError, "Format does not support sparse images");
	}
#endif // CTS_USES_VULKANSC

	if (isFloatFormat(format))
	{
		context.requireDeviceFunctionality("VK_EXT_shader_atomic_float");

		const VkFormatFeatureFlags	requiredFeatures	= (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
		const auto&					atomicFloatFeatures	= context.getShaderAtomicFloatFeaturesEXT();

		if (!atomicFloatFeatures.shaderImageFloat32Atomics)
			TCU_THROW(NotSupportedError, "shaderImageFloat32Atomics not supported");

		if ((operation == ATOMIC_OPERATION_ADD) && !atomicFloatFeatures.shaderImageFloat32AtomicAdd)
			TCU_THROW(NotSupportedError, "shaderImageFloat32AtomicAdd not supported");

		if (operation == ATOMIC_OPERATION_MIN || operation == ATOMIC_OPERATION_MAX)
		{
			context.requireDeviceFunctionality("VK_EXT_shader_atomic_float2");
#ifndef CTS_USES_VULKANSC
			if (!context.getShaderAtomicFloat2FeaturesEXT().shaderImageFloat32AtomicMinMax)
			{
				TCU_THROW(NotSupportedError, "shaderImageFloat32AtomicMinMax not supported");
			}
#endif // CTS_USES_VULKANSC
		}

		if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
			TCU_FAIL("Required format feature bits not supported");

		if (backingType == ImageBackingType::SPARSE)
		{
			if (!atomicFloatFeatures.sparseImageFloat32Atomics)
				TCU_THROW(NotSupportedError, "sparseImageFloat32Atomics not supported");

			if (operation == ATOMIC_OPERATION_ADD && !atomicFloatFeatures.sparseImageFloat32AtomicAdd)
				TCU_THROW(NotSupportedError, "sparseImageFloat32AtomicAdd not supported");
		}

	}
	else if (format == VK_FORMAT_R64_UINT || format == VK_FORMAT_R64_SINT)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

		const VkFormatFeatureFlags	requiredFeatures	= (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
		const auto&					atomicInt64Features	= context.getShaderImageAtomicInt64FeaturesEXT();

		if (!atomicInt64Features.shaderImageInt64Atomics)
			TCU_THROW(NotSupportedError, "shaderImageInt64Atomics not supported");

		if (backingType == ImageBackingType::SPARSE && !atomicInt64Features.sparseImageInt64Atomics)
			TCU_THROW(NotSupportedError, "sparseImageInt64Atomics not supported");

		if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
			TCU_FAIL("Mandatory format features not supported");
	}

	if (useTransfer)
	{
		const VkFormatFeatureFlags transferFeatures = (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
		if ((formatProperties.optimalTilingFeatures & transferFeatures) != transferFeatures)
			TCU_THROW(NotSupportedError, "Transfer features not supported for this format");
	}

	if (readType == ShaderReadType::SPARSE)
	{
		DE_ASSERT(imageType != IMAGE_TYPE_1D && imageType != IMAGE_TYPE_1D_ARRAY && imageType != IMAGE_TYPE_BUFFER);
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY);
	}
}

class BinaryAtomicEndResultCase : public vkt::TestCase
{
public:
								BinaryAtomicEndResultCase	(tcu::TestContext&			testCtx,
															 const string&				name,
															 const string&				description,
															 const ImageType			imageType,
															 const tcu::UVec3&			imageSize,
															 const tcu::TextureFormat&	format,
															 const VkImageTiling		tiling,
															 const AtomicOperation		operation,
															 const bool					useTransfer,
															 const ShaderReadType		shaderReadType,
															 const ImageBackingType		backingType,
															 const glu::GLSLVersion		glslVersion);

	void						initPrograms				(SourceCollections&			sourceCollections) const;
	TestInstance*				createInstance				(Context&					context) const;
	virtual void				checkSupport				(Context&					context) const;

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
	const VkImageTiling			m_tiling;
	const AtomicOperation		m_operation;
	const bool					m_useTransfer;
	const ShaderReadType		m_readType;
	const ImageBackingType		m_backingType;
	const glu::GLSLVersion		m_glslVersion;
};

BinaryAtomicEndResultCase::BinaryAtomicEndResultCase (tcu::TestContext&			testCtx,
													  const string&				name,
													  const string&				description,
													  const ImageType			imageType,
													  const tcu::UVec3&			imageSize,
													  const tcu::TextureFormat&	format,
													  const VkImageTiling		tiling,
													  const AtomicOperation		operation,
													  const bool				useTransfer,
													  const ShaderReadType		shaderReadType,
													  const ImageBackingType	backingType,
													  const glu::GLSLVersion	glslVersion)
	: TestCase		(testCtx, name, description)
	, m_imageType	(imageType)
	, m_imageSize	(imageSize)
	, m_format		(format)
	, m_tiling		(tiling)
	, m_operation	(operation)
	, m_useTransfer	(useTransfer)
	, m_readType	(shaderReadType)
	, m_backingType	(backingType)
	, m_glslVersion	(glslVersion)
{
}

void BinaryAtomicEndResultCase::checkSupport (Context& context) const
{
	commonCheckSupport(context, m_format, m_tiling, m_imageType, m_imageSize, m_operation, m_useTransfer, m_readType, m_backingType);
}

void BinaryAtomicEndResultCase::initPrograms (SourceCollections& sourceCollections) const
{
	const VkFormat	imageFormat		= mapTextureFormat(m_format);
	const deUint32	componentWidth	= getFormatComponentWidth(imageFormat, 0);
	const bool		intFormat		= isIntFormat(imageFormat);
	const bool		uintFormat		= isUintFormat(imageFormat);
	const bool		floatFormat		= isFloatFormat(imageFormat);
	const string	type			= getComponentTypeStr(componentWidth, intFormat, uintFormat, floatFormat);
	const string	vec4Type		= getVec4TypeStr(componentWidth, intFormat, uintFormat, floatFormat);

	AddFillReadShader(sourceCollections, m_imageType, m_format, type, vec4Type);

	if (isSpirvAtomicOperation(m_operation))
	{
		const CaseVariant					caseVariant{m_imageType, m_format.order, m_format.type, CaseVariant::CHECK_TYPE_END_RESULTS};
		const tcu::StringTemplate			shaderTemplate{getSpirvAtomicOpShader(caseVariant)};
		std::map<std::string, std::string>	specializations;

		specializations["OPNAME"] = getSpirvAtomicOpName(m_operation);
		if (isSpirvAtomicNoLastArgOp(m_operation))
			specializations["LASTARG"] = "";

		sourceCollections.spirvAsmSources.add(m_name) << shaderTemplate.specialize(specializations);
	}
	else
	{
		const string	versionDecl				= glu::getGLSLVersionDeclaration(m_glslVersion);

		const UVec3		gridSize				= getShaderGridSize(m_imageType, m_imageSize);
		const string	atomicCoord				= getCoordStr(m_imageType, "gx % " + toString(gridSize.x()), "gy", "gz");

		const string	atomicArgExpr			= type + getAtomicFuncArgumentShaderStr(m_operation,
																						"gx", "gy", "gz",
																						IVec3(NUM_INVOCATIONS_PER_PIXEL*gridSize.x(), gridSize.y(), gridSize.z()));

		const string	compareExchangeStr		= (m_operation == ATOMIC_OPERATION_COMPARE_EXCHANGE) ?
												(componentWidth == 64 ?", 820338753304": ", 18") + string(uintFormat ? "u" : "") + string(componentWidth == 64 ? "l" : "")
												: "";
		const string	atomicInvocation		= getAtomicOperationShaderFuncName(m_operation) + "(u_resultImage, " + atomicCoord + compareExchangeStr + ", " + atomicArgExpr + ")";
		const string	shaderImageFormatStr	= getShaderImageFormatQualifier(m_format);
		const string	shaderImageTypeStr		= getShaderImageType(m_format, m_imageType);
		const string	extensions				= "#extension GL_EXT_shader_atomic_float : enable\n"
												  "#extension GL_EXT_shader_atomic_float2 : enable\n"
												  "#extension GL_KHR_memory_scope_semantics : enable";

		string source = versionDecl + "\n" + extensions + "\n";

		if (64 == componentWidth)
		{
			source +=	"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
						"#extension GL_EXT_shader_image_int64 : require\n";
		}

		source +=	"precision highp " + shaderImageTypeStr + ";\n"
					"\n"
					"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
					"layout (" + shaderImageFormatStr + ", binding=0) coherent uniform " + shaderImageTypeStr + " u_resultImage;\n"
					"\n"
					"void main (void)\n"
					"{\n"
					"	int gx = int(gl_GlobalInvocationID.x);\n"
					"	int gy = int(gl_GlobalInvocationID.y);\n"
					"	int gz = int(gl_GlobalInvocationID.z);\n"
					"	" + atomicInvocation + ";\n"
					"}\n";

		sourceCollections.glslSources.add(m_name) << glu::ComputeSource(source.c_str());
	}
}

class BinaryAtomicIntermValuesCase : public vkt::TestCase
{
public:
								BinaryAtomicIntermValuesCase	(tcu::TestContext&			testCtx,
																 const string&				name,
																 const string&				description,
																 const ImageType			imageType,
																 const tcu::UVec3&			imageSize,
																 const tcu::TextureFormat&	format,
																 const VkImageTiling		tiling,
																 const AtomicOperation		operation,
																 const bool					useTransfer,
																 const ShaderReadType		shaderReadType,
																 const ImageBackingType		backingType,
																 const glu::GLSLVersion		glslVersion);

	void						initPrograms					(SourceCollections&			sourceCollections) const;
	TestInstance*				createInstance					(Context&					context) const;
	virtual void				checkSupport					(Context&					context) const;

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
	const VkImageTiling			m_tiling;
	const AtomicOperation		m_operation;
	const bool					m_useTransfer;
	const ShaderReadType		m_readType;
	const ImageBackingType		m_backingType;
	const glu::GLSLVersion		m_glslVersion;
};

BinaryAtomicIntermValuesCase::BinaryAtomicIntermValuesCase (TestContext&			testCtx,
															const string&			name,
															const string&			description,
															const ImageType			imageType,
															const tcu::UVec3&		imageSize,
															const TextureFormat&	format,
															const VkImageTiling		tiling,
															const AtomicOperation	operation,
															const bool				useTransfer,
															const ShaderReadType	shaderReadType,
															const ImageBackingType	backingType,
															const glu::GLSLVersion	glslVersion)
	: TestCase		(testCtx, name, description)
	, m_imageType	(imageType)
	, m_imageSize	(imageSize)
	, m_format		(format)
	, m_tiling		(tiling)
	, m_operation	(operation)
	, m_useTransfer	(useTransfer)
	, m_readType	(shaderReadType)
	, m_backingType	(backingType)
	, m_glslVersion	(glslVersion)
{
}

void BinaryAtomicIntermValuesCase::checkSupport (Context& context) const
{
	commonCheckSupport(context, m_format, m_tiling, m_imageType, m_imageSize, m_operation, m_useTransfer, m_readType, m_backingType);
}

void BinaryAtomicIntermValuesCase::initPrograms (SourceCollections& sourceCollections) const
{
	const VkFormat	imageFormat		= mapTextureFormat(m_format);
	const deUint32	componentWidth	= getFormatComponentWidth(imageFormat, 0);
	const bool		intFormat		= isIntFormat(imageFormat);
	const bool		uintFormat		= isUintFormat(imageFormat);
	const bool		floatFormat		= isFloatFormat(imageFormat);
	const string	type			= getComponentTypeStr(componentWidth, intFormat, uintFormat, floatFormat);
	const string	vec4Type		= getVec4TypeStr(componentWidth, intFormat, uintFormat, floatFormat);

	AddFillReadShader(sourceCollections, m_imageType, m_format, type, vec4Type);

	if (isSpirvAtomicOperation(m_operation))
	{
		const CaseVariant					caseVariant{m_imageType, m_format.order, m_format.type, CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS};
		const tcu::StringTemplate			shaderTemplate{getSpirvAtomicOpShader(caseVariant)};
		std::map<std::string, std::string>	specializations;

		specializations["OPNAME"] = getSpirvAtomicOpName(m_operation);
		if (isSpirvAtomicNoLastArgOp(m_operation))
			specializations["LASTARG"] = "";

		sourceCollections.spirvAsmSources.add(m_name) << shaderTemplate.specialize(specializations);
	}
	else
	{
		const string	versionDecl				= glu::getGLSLVersionDeclaration(m_glslVersion);
		const UVec3		gridSize				= getShaderGridSize(m_imageType, m_imageSize);
		const string	atomicCoord				= getCoordStr(m_imageType, "gx % " + toString(gridSize.x()), "gy", "gz");
		const string	invocationCoord			= getCoordStr(m_imageType, "gx", "gy", "gz");
		const string	atomicArgExpr			= type + getAtomicFuncArgumentShaderStr(m_operation,
																						"gx", "gy", "gz",
																						IVec3(NUM_INVOCATIONS_PER_PIXEL*gridSize.x(), gridSize.y(), gridSize.z()));

		const string	compareExchangeStr		= (m_operation == ATOMIC_OPERATION_COMPARE_EXCHANGE) ?
												  (componentWidth == 64 ? ", 820338753304" : ", 18") + string(uintFormat ? "u" : "") + string(componentWidth == 64 ? "l" : "") :
												  "";
		const string	atomicInvocation		= getAtomicOperationShaderFuncName(m_operation) +
												"(u_resultImage, " + atomicCoord + compareExchangeStr + ", " + atomicArgExpr + ")";
		const string	shaderImageFormatStr	= getShaderImageFormatQualifier(m_format);
		const string	shaderImageTypeStr		= getShaderImageType(m_format, m_imageType);
		const string	extensions				= "#extension GL_EXT_shader_atomic_float : enable\n"
												  "#extension GL_EXT_shader_atomic_float2 : enable\n"
												  "#extension GL_KHR_memory_scope_semantics : enable";

		string source = versionDecl + "\n" + extensions + "\n"
						"\n";

		if (64 == componentWidth)
		{
			source +=	"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
						"#extension GL_EXT_shader_image_int64 : require\n";
		}

			source +=	"precision highp " + shaderImageTypeStr + "; \n"
						"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
						"layout (" + shaderImageFormatStr + ", binding=0) coherent uniform " + shaderImageTypeStr + " u_resultImage;\n"
						"layout (" + shaderImageFormatStr + ", binding=1) writeonly uniform " + shaderImageTypeStr + " u_intermValuesImage;\n"
						"\n"
						"void main (void)\n"
						"{\n"
						"	int gx = int(gl_GlobalInvocationID.x);\n"
						"	int gy = int(gl_GlobalInvocationID.y);\n"
						"	int gz = int(gl_GlobalInvocationID.z);\n"
						"	imageStore(u_intermValuesImage, " + invocationCoord + ", " + vec4Type + "(" + atomicInvocation + "));\n"
						"}\n";

		sourceCollections.glslSources.add(m_name) << glu::ComputeSource(source.c_str());
	}
}

class BinaryAtomicInstanceBase : public vkt::TestInstance
{
public:

								BinaryAtomicInstanceBase (Context&						context,
														  const string&					name,
														  const ImageType				imageType,
														  const tcu::UVec3&				imageSize,
														  const TextureFormat&			format,
														  const VkImageTiling			tiling,
														  const AtomicOperation			operation,
														  const bool					useTransfer,
														  const ShaderReadType			shaderReadType,
														  const ImageBackingType		backingType);

	tcu::TestStatus				iterate					 (void);

	virtual deUint32			getOutputBufferSize		 (void) const = 0;

	virtual void				prepareResources		 (const bool					useTransfer) = 0;
	virtual void				prepareDescriptors		 (const bool					isTexelBuffer) = 0;

	virtual void				commandsBeforeCompute	 (const VkCommandBuffer			cmdBuffer) const = 0;
	virtual void				commandsAfterCompute	 (const VkCommandBuffer			cmdBuffer,
														  const VkPipeline				pipeline,
														  const VkPipelineLayout		pipelineLayout,
														   const VkDescriptorSet		descriptorSet,
														  const VkDeviceSize&			range,
														  const bool					useTransfer) = 0;

	virtual bool				verifyResult			 (Allocation&					outputBufferAllocation,
														  const bool					is64Bit) const = 0;

protected:

	void						shaderFillImage			 (const VkCommandBuffer			cmdBuffer,
														  const VkBuffer&				buffer,
														  const VkPipeline				pipeline,
														  const VkPipelineLayout		pipelineLayout,
														  const VkDescriptorSet			descriptorSet,
														  const VkDeviceSize&			range,
														  const tcu::UVec3&				gridSize);

	void						createImageAndView		(VkFormat						imageFormat,
														 const tcu::UVec3&				imageExent,
														 bool							useTransfer,
														 de::MovePtr<Image>&			imagePtr,
														 Move<VkImageView>&				imageViewPtr);

	void						createImageResources	(const VkFormat&				imageFormat,
														 const bool						useTransfer);

	const string					m_name;
	const ImageType					m_imageType;
	const tcu::UVec3				m_imageSize;
	const TextureFormat				m_format;
	const VkImageTiling				m_tiling;
	const AtomicOperation			m_operation;
	const bool						m_useTransfer;
	const ShaderReadType			m_readType;
	const ImageBackingType			m_backingType;

	de::MovePtr<BufferWithMemory>	m_inputBuffer;
	de::MovePtr<BufferWithMemory>	m_outputBuffer;
	Move<VkBufferView>				m_descResultBufferView;
	Move<VkBufferView>				m_descIntermResultsBufferView;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorSet>			m_descriptorSet;

	Move<VkDescriptorSetLayout>		m_descriptorSetLayoutNoTransfer;
	Move<VkDescriptorPool>			m_descriptorPoolNoTransfer;

	de::MovePtr<Image>				m_resultImage;
	Move<VkImageView>				m_resultImageView;

	std::vector<VkSemaphore>		m_waitSemaphores;
};

BinaryAtomicInstanceBase::BinaryAtomicInstanceBase (Context&				context,
													const string&			name,
													const ImageType			imageType,
													const tcu::UVec3&		imageSize,
													const TextureFormat&	format,
													const VkImageTiling		tiling,
													const AtomicOperation	operation,
													const bool				useTransfer,
													const ShaderReadType	shaderReadType,
													const ImageBackingType	backingType)
	: vkt::TestInstance	(context)
	, m_name			(name)
	, m_imageType		(imageType)
	, m_imageSize		(imageSize)
	, m_format			(format)
	, m_tiling			(tiling)
	, m_operation		(operation)
	, m_useTransfer		(useTransfer)
	, m_readType		(shaderReadType)
	, m_backingType		(backingType)
{
}

tcu::TestStatus	BinaryAtomicInstanceBase::iterate (void)
{
	const VkDevice			device				= m_context.getDevice();
	const DeviceInterface&	deviceInterface		= m_context.getDeviceInterface();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	const VkDeviceSize		imageSizeInBytes	= tcu::getPixelSize(m_format) * getNumPixels(m_imageType, m_imageSize);
	const VkDeviceSize		outBuffSizeInBytes	= getOutputBufferSize();
	const VkFormat			imageFormat			= mapTextureFormat(m_format);
	const bool				isTexelBuffer		= (m_imageType == IMAGE_TYPE_BUFFER);

	if (!isTexelBuffer)
	{
		createImageResources(imageFormat, m_useTransfer);
	}

	tcu::UVec3				gridSize			= getShaderGridSize(m_imageType, m_imageSize);

	//Prepare the buffer with the initial data for the image
	m_inputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(deviceInterface,
													device,
													allocator,
													makeBufferCreateInfo(imageSizeInBytes,
																		 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
																		 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
																		 (isTexelBuffer ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : static_cast<VkBufferUsageFlagBits>(0u))),
													MemoryRequirement::HostVisible));

	// Fill in buffer with initial data used for image.
	initDataForImage(device, deviceInterface, m_format, m_operation, gridSize, *m_inputBuffer);

	// Create a buffer to store shader output copied from result image
	m_outputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(deviceInterface,
													device,
													allocator,
													makeBufferCreateInfo(outBuffSizeInBytes,
																		 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																		 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
																		 (isTexelBuffer ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : static_cast<VkBufferUsageFlagBits>(0u))),
													MemoryRequirement::HostVisible));

	if (!isTexelBuffer)
	{
		prepareResources(m_useTransfer);
	}

	prepareDescriptors(isTexelBuffer);

	Move<VkDescriptorSet>	descriptorSetFillImage;
	Move<VkShaderModule>	shaderModuleFillImage;
	Move<VkPipelineLayout>	pipelineLayoutFillImage;
	Move<VkPipeline>		pipelineFillImage;

	Move<VkDescriptorSet>	descriptorSetReadImage;
	Move<VkShaderModule>	shaderModuleReadImage;
	Move<VkPipelineLayout>	pipelineLayoutReadImage;
	Move<VkPipeline>		pipelineReadImage;

	if (!m_useTransfer)
	{
		m_descriptorSetLayoutNoTransfer =
			DescriptorSetLayoutBuilder()
			.addSingleBinding((isTexelBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE), VK_SHADER_STAGE_COMPUTE_BIT)
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(deviceInterface, device);

		m_descriptorPoolNoTransfer =
			DescriptorPoolBuilder()
			.addType((isTexelBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE), 2)
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
			.build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

		descriptorSetFillImage = makeDescriptorSet(deviceInterface,
			device,
			*m_descriptorPoolNoTransfer,
			*m_descriptorSetLayoutNoTransfer);

		descriptorSetReadImage = makeDescriptorSet(deviceInterface,
			device,
			*m_descriptorPoolNoTransfer,
			*m_descriptorSetLayoutNoTransfer);

		shaderModuleFillImage	= createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get("fillShader"), 0);
		pipelineLayoutFillImage	= makePipelineLayout(deviceInterface, device, *m_descriptorSetLayoutNoTransfer);
		pipelineFillImage		= makeComputePipeline(deviceInterface, device, *pipelineLayoutFillImage, *shaderModuleFillImage);

		if (m_readType == ShaderReadType::SPARSE)
		{
			shaderModuleReadImage = createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get("readShaderResidency"), 0);
		}
		else
		{
			shaderModuleReadImage = createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get("readShader"), 0);
		}
		pipelineLayoutReadImage = makePipelineLayout(deviceInterface, device, *m_descriptorSetLayoutNoTransfer);
		pipelineReadImage		= makeComputePipeline(deviceInterface, device, *pipelineLayoutFillImage, *shaderModuleReadImage);
	}

	// Create pipeline
	const Unique<VkShaderModule>	shaderModule(createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get(m_name), 0));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(deviceInterface, device, *m_descriptorSetLayout));
	const Unique<VkPipeline>		pipeline(makeComputePipeline(deviceInterface, device, *pipelineLayout, *shaderModule));

	// Create command buffer
	const Unique<VkCommandPool>		cmdPool(createCommandPool(deviceInterface, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(deviceInterface, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(deviceInterface, *cmdBuffer);

	if (!isTexelBuffer)
	{
		if (m_useTransfer)
		{
			const vector<VkBufferImageCopy>	bufferImageCopy(1, makeBufferImageCopy(makeExtent3D(getLayerSize(m_imageType, m_imageSize)), getNumLayers(m_imageType, m_imageSize)));
			copyBufferToImage(deviceInterface,
							  *cmdBuffer,
							  *(*m_inputBuffer),
							  imageSizeInBytes,
							  bufferImageCopy,
							  VK_IMAGE_ASPECT_COLOR_BIT,
							  1,
							  getNumLayers(m_imageType, m_imageSize), m_resultImage->get(), VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
		else
		{
			shaderFillImage(*cmdBuffer, *(*m_inputBuffer), *pipelineFillImage, *pipelineLayoutFillImage, *descriptorSetFillImage, imageSizeInBytes, gridSize);
		}
		commandsBeforeCompute(*cmdBuffer);
	}

	deviceInterface.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	deviceInterface.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);

	deviceInterface.cmdDispatch(*cmdBuffer, NUM_INVOCATIONS_PER_PIXEL * gridSize.x(), gridSize.y(), gridSize.z());

	commandsAfterCompute(*cmdBuffer,
						 *pipelineReadImage,
						 *pipelineLayoutReadImage,
						 *descriptorSetReadImage,
						 outBuffSizeInBytes,
						 m_useTransfer);

	const VkBufferMemoryBarrier	outputBufferPreHostReadBarrier
		= makeBufferMemoryBarrier(((m_useTransfer || isTexelBuffer) ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_WRITE_BIT),
								  VK_ACCESS_HOST_READ_BIT,
								  m_outputBuffer->get(),
								  0ull,
								  outBuffSizeInBytes);

	deviceInterface.cmdPipelineBarrier(*cmdBuffer,
									   ((m_useTransfer || isTexelBuffer) ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),
									   VK_PIPELINE_STAGE_HOST_BIT,
									   DE_FALSE, 0u, DE_NULL,
									   1u, &outputBufferPreHostReadBarrier, 0u, DE_NULL);

	endCommandBuffer(deviceInterface, *cmdBuffer);

	std::vector<VkPipelineStageFlags> waitStages(m_waitSemaphores.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	submitCommandsAndWait(deviceInterface, device, queue, *cmdBuffer, false, 1u,
		static_cast<deUint32>(m_waitSemaphores.size()), de::dataOrNull(m_waitSemaphores), de::dataOrNull(waitStages));

	Allocation& outputBufferAllocation = m_outputBuffer->getAllocation();

	invalidateAlloc(deviceInterface, device, outputBufferAllocation);

	if (verifyResult(outputBufferAllocation, (imageFormat == VK_FORMAT_R64_UINT || imageFormat == VK_FORMAT_R64_SINT)))
		return tcu::TestStatus::pass("Comparison succeeded");
	else
		return tcu::TestStatus::fail("Comparison failed");
}

void BinaryAtomicInstanceBase::shaderFillImage (const VkCommandBuffer	cmdBuffer,
												const VkBuffer&			buffer,
												const VkPipeline		pipeline,
												const VkPipelineLayout	pipelineLayout,
												const VkDescriptorSet	descriptorSet,
												const VkDeviceSize&		range,
												const tcu::UVec3&		gridSize)
{
	const VkDevice					device					= m_context.getDevice();
	const DeviceInterface&			deviceInterface			= m_context.getDeviceInterface();
	const VkDescriptorImageInfo		descResultImageInfo		= makeDescriptorImageInfo(DE_NULL, *m_resultImageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo	descResultBufferInfo	= makeDescriptorBufferInfo(buffer, 0, range);
	const VkImageSubresourceRange	subresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));

	DescriptorSetUpdateBuilder()
		.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descResultImageInfo)
		.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descResultBufferInfo)
		.update(deviceInterface, device);

	const VkImageMemoryBarrier imageBarrierPre = makeImageMemoryBarrier(0,
																		VK_ACCESS_SHADER_WRITE_BIT,
																		VK_IMAGE_LAYOUT_UNDEFINED,
																		VK_IMAGE_LAYOUT_GENERAL,
																		m_resultImage->get(),
																		subresourceRange);

	deviceInterface.cmdPipelineBarrier(	cmdBuffer,
										VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										(VkDependencyFlags)0,
										0, (const VkMemoryBarrier*)DE_NULL,
										0, (const VkBufferMemoryBarrier*)DE_NULL,
										1, &imageBarrierPre);

	deviceInterface.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	deviceInterface.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

	deviceInterface.cmdDispatch(cmdBuffer, gridSize.x(), gridSize.y(), gridSize.z());

	const VkImageMemoryBarrier imageBarrierPost = makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
																		 VK_ACCESS_SHADER_READ_BIT,
																		 VK_IMAGE_LAYOUT_GENERAL,
																		 VK_IMAGE_LAYOUT_GENERAL,
																		 m_resultImage->get(),
																		 subresourceRange);

	deviceInterface.cmdPipelineBarrier(	cmdBuffer,
										VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
										(VkDependencyFlags)0,
										0, (const VkMemoryBarrier*)DE_NULL,
										0, (const VkBufferMemoryBarrier*)DE_NULL,
										1, &imageBarrierPost);
}

void BinaryAtomicInstanceBase::createImageAndView	(VkFormat						imageFormat,
													 const tcu::UVec3&				imageExent,
													 bool							useTransfer,
													 de::MovePtr<Image>&			imagePtr,
													 Move<VkImageView>&				imageViewPtr)
{
	const VkDevice			device			= m_context.getDevice();
	const DeviceInterface&	deviceInterface	= m_context.getDeviceInterface();
	Allocator&				allocator		= m_context.getDefaultAllocator();
	const VkImageUsageFlags	usageFlags		= getUsageFlags(useTransfer);
	VkImageCreateFlags		createFlags		= 0u;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
		createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	const auto numLayers = getNumLayers(m_imageType, m_imageSize);

	VkImageCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		createFlags,											// VkImageCreateFlags		flags;
		mapImageType(m_imageType),								// VkImageType				imageType;
		imageFormat,											// VkFormat					format;
		makeExtent3D(imageExent),								// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		numLayers,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		m_tiling,												// VkImageTiling			tiling;
		usageFlags,												// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};

#ifndef CTS_USES_VULKANSC
	if (m_backingType == ImageBackingType::SPARSE)
	{
		const auto&		vki				= m_context.getInstanceInterface();
		const auto		physicalDevice	= m_context.getPhysicalDevice();
		const auto		sparseQueue		= m_context.getSparseQueue();
		const auto		sparseQueueIdx	= m_context.getSparseQueueFamilyIndex();
		const auto		universalQIdx	= m_context.getUniversalQueueFamilyIndex();
		const deUint32	queueIndices[]	= { universalQIdx, sparseQueueIdx };

		createInfo.flags |= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

		if (sparseQueueIdx != universalQIdx)
		{
			createInfo.sharingMode				= VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount	= static_cast<deUint32>(DE_LENGTH_OF_ARRAY(queueIndices));
			createInfo.pQueueFamilyIndices		= queueIndices;
		}

		const auto sparseImage = new SparseImage(deviceInterface, device, physicalDevice, vki, createInfo, sparseQueue, allocator, m_format);
		m_waitSemaphores.push_back(sparseImage->getSemaphore());
		imagePtr = de::MovePtr<Image>(sparseImage);
	}
	else
#endif // CTS_USES_VULKANSC
		imagePtr = de::MovePtr<Image>(new Image(deviceInterface, device, allocator, createInfo, MemoryRequirement::Any));

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numLayers);

	imageViewPtr = makeImageView(deviceInterface, device, imagePtr->get(), mapImageViewType(m_imageType), imageFormat, subresourceRange);
}

void BinaryAtomicInstanceBase::createImageResources (const VkFormat&	imageFormat,
													 const bool			useTransfer)
{
	//Create the image that is going to store results of atomic operations
	createImageAndView(imageFormat, getLayerSize(m_imageType, m_imageSize), useTransfer, m_resultImage, m_resultImageView);
}

class BinaryAtomicEndResultInstance : public BinaryAtomicInstanceBase
{
public:

						BinaryAtomicEndResultInstance  (Context&					context,
														const string&				name,
														const ImageType				imageType,
														const tcu::UVec3&			imageSize,
														const TextureFormat&		format,
														const VkImageTiling			tiling,
														const AtomicOperation		operation,
														const bool					useTransfer,
														const ShaderReadType		shaderReadType,
														const ImageBackingType		backingType)
							: BinaryAtomicInstanceBase(context, name, imageType, imageSize, format, tiling, operation, useTransfer, shaderReadType, backingType) {}

	virtual deUint32	getOutputBufferSize			   (void) const;

	virtual void		prepareResources			   (const bool					useTransfer) { DE_UNREF(useTransfer); }
	virtual void		prepareDescriptors			   (const bool					isTexelBuffer);

	virtual void		commandsBeforeCompute		   (const VkCommandBuffer) const {}
	virtual void		commandsAfterCompute		   (const VkCommandBuffer		cmdBuffer,
														const VkPipeline			pipeline,
														const VkPipelineLayout		pipelineLayout,
														const VkDescriptorSet		descriptorSet,
														const VkDeviceSize&			range,
														const bool					useTransfer);

	virtual bool		verifyResult				   (Allocation&					outputBufferAllocation,
														const bool					is64Bit) const;

protected:

	template <typename T>
	bool				isValueCorrect				   (const T						resultValue,
														deInt32						x,
														deInt32						y,
														deInt32						z,
														const UVec3&				gridSize,
														const IVec3					extendedGridSize) const;
};

deUint32 BinaryAtomicEndResultInstance::getOutputBufferSize (void) const
{
	return tcu::getPixelSize(m_format) * getNumPixels(m_imageType, m_imageSize);
}

void BinaryAtomicEndResultInstance::prepareDescriptors (const bool	isTexelBuffer)
{
	const VkDescriptorType	descriptorType	= isTexelBuffer ?
											VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
											VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	const VkDevice			device			= m_context.getDevice();
	const DeviceInterface&	deviceInterface = m_context.getDeviceInterface();

	m_descriptorSetLayout =
		DescriptorSetLayoutBuilder()
		.addSingleBinding(descriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(deviceInterface, device);

	m_descriptorPool =
		DescriptorPoolBuilder()
		.addType(descriptorType)
		.build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descriptorSet = makeDescriptorSet(deviceInterface, device, *m_descriptorPool, *m_descriptorSetLayout);

	if (isTexelBuffer)
	{
		m_descResultBufferView = makeBufferView(deviceInterface, device, *(*m_inputBuffer), mapTextureFormat(m_format), 0, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &(m_descResultBufferView.get()))
			.update(deviceInterface, device);
	}
	else
	{
		const VkDescriptorImageInfo	descResultImageInfo = makeDescriptorImageInfo(DE_NULL, *m_resultImageView, VK_IMAGE_LAYOUT_GENERAL);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &descResultImageInfo)
			.update(deviceInterface, device);
	}
}

void BinaryAtomicEndResultInstance::commandsAfterCompute (const VkCommandBuffer		cmdBuffer,
														  const VkPipeline			pipeline,
														  const VkPipelineLayout	pipelineLayout,
														  const VkDescriptorSet		descriptorSet,
														  const VkDeviceSize&		range,
														  const bool				useTransfer)
{
	const DeviceInterface&			deviceInterface		= m_context.getDeviceInterface();
	const VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));
	const UVec3						layerSize			= getLayerSize(m_imageType, m_imageSize);

	if (m_imageType == IMAGE_TYPE_BUFFER)
	{
		m_outputBuffer = m_inputBuffer;
	}
	else if (useTransfer)
	{
		const VkImageMemoryBarrier	resultImagePostDispatchBarrier =
			makeImageMemoryBarrier(	VK_ACCESS_SHADER_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT,
									VK_IMAGE_LAYOUT_GENERAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									m_resultImage->get(),
									subresourceRange);

		deviceInterface.cmdPipelineBarrier(	cmdBuffer,
											VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
											VK_PIPELINE_STAGE_TRANSFER_BIT,
											DE_FALSE, 0u, DE_NULL, 0u, DE_NULL,
											1u, &resultImagePostDispatchBarrier);

		const VkBufferImageCopy		bufferImageCopyParams = makeBufferImageCopy(makeExtent3D(layerSize), getNumLayers(m_imageType, m_imageSize));

		deviceInterface.cmdCopyImageToBuffer(cmdBuffer, m_resultImage->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_outputBuffer->get(), 1u, &bufferImageCopyParams);
	}
	else
	{
		const VkDevice					device					= m_context.getDevice();
		const VkDescriptorImageInfo		descResultImageInfo		= makeDescriptorImageInfo(DE_NULL, *m_resultImageView, VK_IMAGE_LAYOUT_GENERAL);
		const VkDescriptorBufferInfo	descResultBufferInfo	= makeDescriptorBufferInfo(m_outputBuffer->get(), 0, range);

		DescriptorSetUpdateBuilder()
			.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descResultImageInfo)
			.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descResultBufferInfo)
			.update(deviceInterface, device);

		const VkImageMemoryBarrier	resultImagePostDispatchBarrier =
			makeImageMemoryBarrier(	VK_ACCESS_SHADER_WRITE_BIT,
									VK_ACCESS_SHADER_READ_BIT,
									VK_IMAGE_LAYOUT_GENERAL,
									VK_IMAGE_LAYOUT_GENERAL,
									m_resultImage->get(),
									subresourceRange);

		deviceInterface.cmdPipelineBarrier(	cmdBuffer,
											VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
											VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
											DE_FALSE, 0u, DE_NULL, 0u, DE_NULL,
											1u, &resultImagePostDispatchBarrier);

		deviceInterface.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		deviceInterface.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

		switch (m_imageType)
		{
			case IMAGE_TYPE_1D_ARRAY:
				deviceInterface.cmdDispatch(cmdBuffer, layerSize.x(), subresourceRange.layerCount, layerSize.z());
				break;
			case IMAGE_TYPE_2D_ARRAY:
			case IMAGE_TYPE_CUBE:
			case IMAGE_TYPE_CUBE_ARRAY:
				deviceInterface.cmdDispatch(cmdBuffer, layerSize.x(), layerSize.y(), subresourceRange.layerCount);
				break;
			default:
				deviceInterface.cmdDispatch(cmdBuffer, layerSize.x(), layerSize.y(), layerSize.z());
				break;
		}
	}
}

bool BinaryAtomicEndResultInstance::verifyResult (Allocation&	outputBufferAllocation,
												  const bool	is64Bit) const
{
	const UVec3	gridSize			= getShaderGridSize(m_imageType, m_imageSize);
	const IVec3 extendedGridSize	= IVec3(NUM_INVOCATIONS_PER_PIXEL*gridSize.x(), gridSize.y(), gridSize.z());

	tcu::ConstPixelBufferAccess resultBuffer(m_format, gridSize.x(), gridSize.y(), gridSize.z(), outputBufferAllocation.getHostPtr());

	for (deInt32 z = 0; z < resultBuffer.getDepth();  z++)
	for (deInt32 y = 0; y < resultBuffer.getHeight(); y++)
	for (deInt32 x = 0; x < resultBuffer.getWidth();  x++)
	{
		const void* resultValue = resultBuffer.getPixelPtr(x, y, z);
		deInt32 floatToIntValue = 0;
		bool isFloatValue = false;
		if (isFloatFormat(mapTextureFormat(m_format)))
		{
			isFloatValue = true;
			floatToIntValue = static_cast<deInt32>(*((float*)resultValue));
		}

		if (isOrderIndependentAtomicOperation(m_operation))
		{
			if (isUintFormat(mapTextureFormat(m_format)))
			{
				if(is64Bit)
				{
					if (!isValueCorrect<deUint64>(*((deUint64*)resultValue), x, y, z, gridSize, extendedGridSize))
						return false;
				}
				else
				{
					if (!isValueCorrect<deUint32>(*((deUint32*)resultValue), x, y, z, gridSize, extendedGridSize))
						return false;
				}
			}
			else if (isIntFormat(mapTextureFormat(m_format)))
			{
				if (is64Bit)
				{
					if (!isValueCorrect<deInt64>(*((deInt64*)resultValue), x, y, z, gridSize, extendedGridSize))
						return false;
				}
				else
				{
					if (!isValueCorrect<deInt32>(*((deInt32*)resultValue), x, y, z, gridSize, extendedGridSize))
						return false;
				}
			}
			else
			{
				// 32-bit floating point
				if (!isValueCorrect<deInt32>(floatToIntValue, x, y, z, gridSize, extendedGridSize))
					return false;
			}
		}
		else if (m_operation == ATOMIC_OPERATION_EXCHANGE)
		{
			// Check if the end result equals one of the atomic args.
			bool matchFound = false;

			for (deInt32 i = 0; i < static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL) && !matchFound; i++)
			{
				const IVec3 gid(x + i*gridSize.x(), y, z);
				matchFound = is64Bit ?
					(*((deInt64*)resultValue) == getAtomicFuncArgument<deInt64>(m_operation, gid, extendedGridSize)) :
					isFloatValue ?
					floatToIntValue == getAtomicFuncArgument<deInt32>(m_operation, gid, extendedGridSize) :
					(*((deInt32*)resultValue) == getAtomicFuncArgument<deInt32>(m_operation, gid, extendedGridSize));

			}

			if (!matchFound)
				return false;
		}
		else if (m_operation == ATOMIC_OPERATION_COMPARE_EXCHANGE)
		{
			// Check if the end result equals one of the atomic args.
			bool matchFound = false;

			for (deInt32 i = 0; i < static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL) && !matchFound; i++)
			{
				const IVec3 gid(x + i*gridSize.x(), y, z);
				matchFound = is64Bit ?
					(*((deInt64*)resultValue) == getAtomicFuncArgument<deInt64>(m_operation, gid, extendedGridSize)) :
					isFloatValue ?
					floatToIntValue == getAtomicFuncArgument<deInt32>(m_operation, gid, extendedGridSize) :
					(*((deInt32*)resultValue) == getAtomicFuncArgument<deInt32>(m_operation, gid, extendedGridSize));
			}

			if (!matchFound)
				return false;
		}
		else
			DE_ASSERT(false);
	}
	return true;
}

template <typename T>
bool BinaryAtomicEndResultInstance::isValueCorrect(const T resultValue, deInt32 x, deInt32 y, deInt32 z, const UVec3& gridSize, const IVec3 extendedGridSize) const
{
	T reference = getOperationInitialValue<T>(m_operation);
	for (deInt32 i = 0; i < static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL); i++)
	{
		const IVec3 gid(x + i*gridSize.x(), y, z);
		T			arg = getAtomicFuncArgument<T>(m_operation, gid, extendedGridSize);
		reference = computeBinaryAtomicOperationResult(m_operation, reference, arg);
	}
	return (resultValue == reference);
}

TestInstance* BinaryAtomicEndResultCase::createInstance (Context& context) const
{
	return new BinaryAtomicEndResultInstance(context, m_name, m_imageType, m_imageSize, m_format, m_tiling, m_operation, m_useTransfer, m_readType, m_backingType);
}

class BinaryAtomicIntermValuesInstance : public BinaryAtomicInstanceBase
{
public:

						BinaryAtomicIntermValuesInstance   (Context&				context,
															const string&			name,
															const ImageType			imageType,
															const tcu::UVec3&		imageSize,
															const TextureFormat&	format,
															const VkImageTiling		tiling,
															const AtomicOperation	operation,
															const bool				useTransfer,
															const ShaderReadType	shaderReadType,
															const ImageBackingType	backingType)
							: BinaryAtomicInstanceBase(context, name, imageType, imageSize, format, tiling, operation, useTransfer, shaderReadType, backingType) {}

	virtual deUint32	getOutputBufferSize				   (void) const;

	virtual void		prepareResources				   (const bool				useTransfer);
	virtual void		prepareDescriptors				   (const bool				isTexelBuffer);

	virtual void		commandsBeforeCompute			   (const VkCommandBuffer	cmdBuffer) const;
	virtual void		commandsAfterCompute			   (const VkCommandBuffer	cmdBuffer,
															const VkPipeline		pipeline,
															const VkPipelineLayout	pipelineLayout,
															const VkDescriptorSet	descriptorSet,
															const VkDeviceSize&		range,
															const bool				useTransfer);

	virtual bool		verifyResult					   (Allocation&				outputBufferAllocation,
															const bool				is64Bit) const;

protected:

	template <typename T>
	bool				areValuesCorrect				   (tcu::ConstPixelBufferAccess& resultBuffer,
															const bool isFloatingPoint,
															deInt32 x,
															deInt32 y,
															deInt32 z,
															const UVec3& gridSize,
															const IVec3 extendedGridSize) const;

	template <typename T>
	bool				verifyRecursive					   (const deInt32			index,
															const T					valueSoFar,
															bool					argsUsed[NUM_INVOCATIONS_PER_PIXEL],
															const T					atomicArgs[NUM_INVOCATIONS_PER_PIXEL],
															const T					resultValues[NUM_INVOCATIONS_PER_PIXEL]) const;
	de::MovePtr<Image>	m_intermResultsImage;
	Move<VkImageView>	m_intermResultsImageView;
};

deUint32 BinaryAtomicIntermValuesInstance::getOutputBufferSize (void) const
{
	return NUM_INVOCATIONS_PER_PIXEL * tcu::getPixelSize(m_format) * getNumPixels(m_imageType, m_imageSize);
}

void BinaryAtomicIntermValuesInstance::prepareResources (const bool useTransfer)
{
	const UVec3 layerSize			= getLayerSize(m_imageType, m_imageSize);
	const bool  isCubeBasedImage	= (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY);
	const UVec3 extendedLayerSize	= isCubeBasedImage	? UVec3(NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), NUM_INVOCATIONS_PER_PIXEL * layerSize.y(), layerSize.z())
														: UVec3(NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), layerSize.y(), layerSize.z());

	createImageAndView(mapTextureFormat(m_format), extendedLayerSize, useTransfer, m_intermResultsImage, m_intermResultsImageView);
}

void BinaryAtomicIntermValuesInstance::prepareDescriptors (const bool	isTexelBuffer)
{
	const VkDescriptorType	descriptorType	= isTexelBuffer ?
											VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
											VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	const VkDevice			device			= m_context.getDevice();
	const DeviceInterface&	deviceInterface = m_context.getDeviceInterface();

	m_descriptorSetLayout =
		DescriptorSetLayoutBuilder()
		.addSingleBinding(descriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(descriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(deviceInterface, device);

	m_descriptorPool =
		DescriptorPoolBuilder()
		.addType(descriptorType, 2u)
		.build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descriptorSet = makeDescriptorSet(deviceInterface, device, *m_descriptorPool, *m_descriptorSetLayout);

	if (isTexelBuffer)
	{
		m_descResultBufferView			= makeBufferView(deviceInterface, device, *(*m_inputBuffer), mapTextureFormat(m_format), 0, VK_WHOLE_SIZE);
		m_descIntermResultsBufferView	= makeBufferView(deviceInterface, device, *(*m_outputBuffer), mapTextureFormat(m_format), 0, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &(m_descResultBufferView.get()))
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &(m_descIntermResultsBufferView.get()))
			.update(deviceInterface, device);
	}
	else
	{
		const VkDescriptorImageInfo	descResultImageInfo			= makeDescriptorImageInfo(DE_NULL, *m_resultImageView, VK_IMAGE_LAYOUT_GENERAL);
		const VkDescriptorImageInfo	descIntermResultsImageInfo	= makeDescriptorImageInfo(DE_NULL, *m_intermResultsImageView, VK_IMAGE_LAYOUT_GENERAL);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &descResultImageInfo)
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &descIntermResultsImageInfo)
			.update(deviceInterface, device);
	}
}

void BinaryAtomicIntermValuesInstance::commandsBeforeCompute (const VkCommandBuffer cmdBuffer) const
{
	const DeviceInterface&			deviceInterface		= m_context.getDeviceInterface();
	const VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));

	const VkImageMemoryBarrier	imagePreDispatchBarrier =
		makeImageMemoryBarrier(	0u,
								VK_ACCESS_SHADER_WRITE_BIT,
								VK_IMAGE_LAYOUT_UNDEFINED,
								VK_IMAGE_LAYOUT_GENERAL,
								m_intermResultsImage->get(),
								subresourceRange);

	deviceInterface.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DE_FALSE, 0u, DE_NULL, 0u, DE_NULL, 1u, &imagePreDispatchBarrier);
}

void BinaryAtomicIntermValuesInstance::commandsAfterCompute (const VkCommandBuffer		cmdBuffer,
															 const VkPipeline			pipeline,
															 const VkPipelineLayout		pipelineLayout,
															 const VkDescriptorSet		descriptorSet,
															 const VkDeviceSize&		range,
															 const bool					useTransfer)
{
	// nothing is needed for texel image buffer
	if (m_imageType == IMAGE_TYPE_BUFFER)
		return;

	const DeviceInterface&			deviceInterface		= m_context.getDeviceInterface();
	const VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));
	const UVec3						layerSize			= getLayerSize(m_imageType, m_imageSize);

	if (useTransfer)
	{
		const VkImageMemoryBarrier	imagePostDispatchBarrier =
			makeImageMemoryBarrier(	VK_ACCESS_SHADER_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT,
									VK_IMAGE_LAYOUT_GENERAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									m_intermResultsImage->get(),
									subresourceRange);

		deviceInterface.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, 0u, DE_NULL, 0u, DE_NULL, 1u, &imagePostDispatchBarrier);

		const UVec3					extendedLayerSize		= UVec3(NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), layerSize.y(), layerSize.z());
		const VkBufferImageCopy		bufferImageCopyParams	= makeBufferImageCopy(makeExtent3D(extendedLayerSize), getNumLayers(m_imageType, m_imageSize));

		deviceInterface.cmdCopyImageToBuffer(cmdBuffer, m_intermResultsImage->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_outputBuffer->get(), 1u, &bufferImageCopyParams);
	}
	else
	{
		const VkDevice					device					= m_context.getDevice();
		const VkDescriptorImageInfo		descResultImageInfo		= makeDescriptorImageInfo(DE_NULL, *m_intermResultsImageView, VK_IMAGE_LAYOUT_GENERAL);
		const VkDescriptorBufferInfo	descResultBufferInfo	= makeDescriptorBufferInfo(m_outputBuffer->get(), 0, range);

		DescriptorSetUpdateBuilder()
			.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descResultImageInfo)
			.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descResultBufferInfo)
			.update(deviceInterface, device);

		const VkImageMemoryBarrier	resultImagePostDispatchBarrier =
		makeImageMemoryBarrier(	VK_ACCESS_SHADER_WRITE_BIT,
								VK_ACCESS_SHADER_READ_BIT,
								VK_IMAGE_LAYOUT_GENERAL,
								VK_IMAGE_LAYOUT_GENERAL,
								m_intermResultsImage->get(),
								subresourceRange);

		deviceInterface.cmdPipelineBarrier(	cmdBuffer,
									VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
									VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
									DE_FALSE, 0u, DE_NULL, 0u, DE_NULL,
									1u, &resultImagePostDispatchBarrier);

		deviceInterface.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		deviceInterface.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

		switch (m_imageType)
		{
			case IMAGE_TYPE_1D_ARRAY:
				deviceInterface.cmdDispatch(cmdBuffer, NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), subresourceRange.layerCount, layerSize.z());
				break;
			case IMAGE_TYPE_2D_ARRAY:
			case IMAGE_TYPE_CUBE:
			case IMAGE_TYPE_CUBE_ARRAY:
				deviceInterface.cmdDispatch(cmdBuffer, NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), layerSize.y(), subresourceRange.layerCount);
				break;
			default:
				deviceInterface.cmdDispatch(cmdBuffer, NUM_INVOCATIONS_PER_PIXEL * layerSize.x(), layerSize.y(), layerSize.z());
				break;
		}
	}
}

bool BinaryAtomicIntermValuesInstance::verifyResult (Allocation&	outputBufferAllocation,
													 const bool		is64Bit) const
{
	const UVec3	gridSize		 = getShaderGridSize(m_imageType, m_imageSize);
	const IVec3 extendedGridSize = IVec3(NUM_INVOCATIONS_PER_PIXEL*gridSize.x(), gridSize.y(), gridSize.z());

	tcu::ConstPixelBufferAccess resultBuffer(m_format, extendedGridSize.x(), extendedGridSize.y(), extendedGridSize.z(), outputBufferAllocation.getHostPtr());

	for (deInt32 z = 0; z < resultBuffer.getDepth(); z++)
	for (deInt32 y = 0; y < resultBuffer.getHeight(); y++)
	for (deUint32 x = 0; x < gridSize.x(); x++)
	{
		if (isUintFormat(mapTextureFormat(m_format)))
		{
			if (is64Bit)
			{
				if (!areValuesCorrect<deUint64>(resultBuffer, false, x, y, z, gridSize, extendedGridSize))
					return false;
			}
			else
			{
				if (!areValuesCorrect<deUint32>(resultBuffer, false, x, y, z, gridSize, extendedGridSize))
					return false;
			}
		}
		else if (isIntFormat(mapTextureFormat(m_format)))
		{
			if (is64Bit)
			{
				if (!areValuesCorrect<deInt64>(resultBuffer, false, x, y, z, gridSize, extendedGridSize))
					return false;
			}
			else
			{
				if (!areValuesCorrect<deInt32>(resultBuffer, false, x, y, z, gridSize, extendedGridSize))
					return false;
			}
		}
		else
		{
			// 32-bit floating point
			if (!areValuesCorrect<deInt32>(resultBuffer, true, x, y, z, gridSize, extendedGridSize))
				return false;
		}
	}

	return true;
}

template <typename T>
bool BinaryAtomicIntermValuesInstance::areValuesCorrect(tcu::ConstPixelBufferAccess& resultBuffer, const bool isFloatingPoint, deInt32 x, deInt32 y, deInt32 z, const UVec3& gridSize, const IVec3 extendedGridSize) const
{
	T		resultValues[NUM_INVOCATIONS_PER_PIXEL];
	T		atomicArgs[NUM_INVOCATIONS_PER_PIXEL];
	bool	argsUsed[NUM_INVOCATIONS_PER_PIXEL];

	for (deInt32 i = 0; i < static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL); i++)
	{
		IVec3 gid(x + i*gridSize.x(), y, z);
		T data = *((T*)resultBuffer.getPixelPtr(gid.x(), gid.y(), gid.z()));
		if (isFloatingPoint)
		{
			float fData;
			deMemcpy(&fData, &data, sizeof(fData));
			data = static_cast<T>(fData);
		}
		resultValues[i] = data;
		atomicArgs[i]	= getAtomicFuncArgument<T>(m_operation, gid, extendedGridSize);
		argsUsed[i]		= false;
	}

	// Verify that the return values form a valid sequence.
	return verifyRecursive(0, getOperationInitialValue<T>(m_operation), argsUsed, atomicArgs, resultValues);
}

template <typename T>
bool BinaryAtomicIntermValuesInstance::verifyRecursive (const deInt32	index,
														const T			valueSoFar,
														bool			argsUsed[NUM_INVOCATIONS_PER_PIXEL],
														const T			atomicArgs[NUM_INVOCATIONS_PER_PIXEL],
														const T			resultValues[NUM_INVOCATIONS_PER_PIXEL]) const
{
	if (index >= static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL))
		return true;

	for (deInt32 i = 0; i < static_cast<deInt32>(NUM_INVOCATIONS_PER_PIXEL); i++)
	{
		if (!argsUsed[i] && resultValues[i] == valueSoFar)
		{
			argsUsed[i] = true;

			if (verifyRecursive(index + 1, computeBinaryAtomicOperationResult(m_operation, valueSoFar, atomicArgs[i]), argsUsed, atomicArgs, resultValues))
			{
				return true;
			}

			argsUsed[i] = false;
		}
	}

	return false;
}

TestInstance* BinaryAtomicIntermValuesCase::createInstance (Context& context) const
{
	return new BinaryAtomicIntermValuesInstance(context, m_name, m_imageType, m_imageSize, m_format, m_tiling, m_operation, m_useTransfer, m_readType, m_backingType);
}

} // anonymous ns

tcu::TestCaseGroup* createImageAtomicOperationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> imageAtomicOperationsTests(new tcu::TestCaseGroup(testCtx, "atomic_operations", "Atomic image operations cases"));

	struct ImageParams
	{
		ImageParams(const ImageType imageType, const tcu::UVec3& imageSize)
			: m_imageType	(imageType)
			, m_imageSize	(imageSize)
		{
		}
		const ImageType		m_imageType;
		const tcu::UVec3	m_imageSize;
	};

	const ImageParams imageParamsArray[] =
	{
		ImageParams(IMAGE_TYPE_1D,			tcu::UVec3(64u, 1u, 1u)),
		ImageParams(IMAGE_TYPE_1D_ARRAY,	tcu::UVec3(64u, 1u, 8u)),
		ImageParams(IMAGE_TYPE_2D,			tcu::UVec3(64u, 64u, 1u)),
		ImageParams(IMAGE_TYPE_2D_ARRAY,	tcu::UVec3(64u, 64u, 8u)),
		ImageParams(IMAGE_TYPE_3D,			tcu::UVec3(48u, 48u, 8u)),
		ImageParams(IMAGE_TYPE_CUBE,		tcu::UVec3(64u, 64u, 1u)),
		ImageParams(IMAGE_TYPE_CUBE_ARRAY,	tcu::UVec3(64u, 64u, 2u)),
		ImageParams(IMAGE_TYPE_BUFFER,		tcu::UVec3(64u, 1u, 1u))
	};

	const tcu::TextureFormat formats[] =
	{
		tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::SIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT),
		tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT64),
		tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::SIGNED_INT64)
	};

    static const VkImageTiling s_tilings[] = {
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TILING_LINEAR,
    };

	const struct
	{
		ShaderReadType		type;
		const char*			name;
	} readTypes[] =
	{
		{	ShaderReadType::NORMAL,	"normal_read"	},
#ifndef CTS_USES_VULKANSC
		{	ShaderReadType::SPARSE,	"sparse_read"	},
#endif // CTS_USES_VULKANSC
	};

	const struct
	{
		ImageBackingType	type;
		const char*			name;
	} backingTypes[] =
	{
		{	ImageBackingType::NORMAL,	"normal_img"	},
#ifndef CTS_USES_VULKANSC
		{	ImageBackingType::SPARSE,	"sparse_img"	},
#endif // CTS_USES_VULKANSC
	};

	for (deUint32 operationI = 0; operationI < ATOMIC_OPERATION_LAST; operationI++)
	{
		const AtomicOperation operation = (AtomicOperation)operationI;

		de::MovePtr<tcu::TestCaseGroup> operationGroup(new tcu::TestCaseGroup(testCtx, getAtomicOperationCaseName(operation).c_str(), ""));

		for (deUint32 imageTypeNdx = 0; imageTypeNdx < DE_LENGTH_OF_ARRAY(imageParamsArray); imageTypeNdx++)
		{
			const ImageType	 imageType = imageParamsArray[imageTypeNdx].m_imageType;
			const tcu::UVec3 imageSize = imageParamsArray[imageTypeNdx].m_imageSize;

			de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

			for (int useTransferIdx = 0; useTransferIdx < 2; ++useTransferIdx)
			{
				const bool				useTransfer	= (useTransferIdx > 0);
				const string			groupName	= (!useTransfer ? "no" : "") + string("transfer");

				de::MovePtr<tcu::TestCaseGroup> transferGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));

				for (int readTypeIdx = 0; readTypeIdx < DE_LENGTH_OF_ARRAY(readTypes); ++readTypeIdx)
				{
					const auto& readType = readTypes[readTypeIdx];

					de::MovePtr<tcu::TestCaseGroup> readTypeGroup(new tcu::TestCaseGroup(testCtx, readType.name, ""));

					for (int backingTypeIdx = 0; backingTypeIdx < DE_LENGTH_OF_ARRAY(backingTypes); ++backingTypeIdx)
					{
						const auto& backingType = backingTypes[backingTypeIdx];

						de::MovePtr<tcu::TestCaseGroup> backingTypeGroup(new tcu::TestCaseGroup(testCtx, backingType.name, ""));

						for (deUint32 formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
						{
							for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(s_tilings); tilingNdx++)
							{
								const TextureFormat&	format		= formats[formatNdx];
								const std::string		formatName	= getShaderImageFormatQualifier(format);
								const char* suffix = (s_tilings[tilingNdx] == VK_IMAGE_TILING_OPTIMAL) ? "" : "_linear";

								// Need SPIRV programs in vktImageAtomicSpirvShaders.cpp
								if (imageType == IMAGE_TYPE_BUFFER && (format.type != tcu::TextureFormat::FLOAT))
								{
									continue;
								}

								// Only 2D and 3D images may support sparse residency.
								// VK_IMAGE_TILING_LINEAR does not support sparse residency
								const auto vkImageType = mapImageType(imageType);
								if (backingType.type == ImageBackingType::SPARSE && ((vkImageType != VK_IMAGE_TYPE_2D && vkImageType != VK_IMAGE_TYPE_3D) || (s_tilings[tilingNdx] == VK_IMAGE_TILING_LINEAR)))
									continue;

								// Only some operations are supported on floating-point
								if (format.type == tcu::TextureFormat::FLOAT)
								{
									if (operation != ATOMIC_OPERATION_ADD &&
#ifndef CTS_USES_VULKANSC
										operation != ATOMIC_OPERATION_MIN &&
										operation != ATOMIC_OPERATION_MAX &&
#endif // CTS_USES_VULKANSC
										operation != ATOMIC_OPERATION_EXCHANGE)
									{
										continue;
									}
								}

								if (readType.type == ShaderReadType::SPARSE)
								{
									// When using transfer, shader reads will not be used, so avoid creating two identical cases.
									if (useTransfer)
										continue;

									// Sparse reads are not supported for all types of images.
									if (imageType == IMAGE_TYPE_1D || imageType == IMAGE_TYPE_1D_ARRAY || imageType == IMAGE_TYPE_BUFFER)
										continue;
								}

								//!< Atomic case checks the end result of the operations, and not the intermediate return values
								const string caseEndResult = formatName + "_end_result" + suffix;
								backingTypeGroup->addChild(new BinaryAtomicEndResultCase(testCtx, caseEndResult, "", imageType, imageSize, format, s_tilings[tilingNdx], operation, useTransfer, readType.type, backingType.type, glu::GLSL_VERSION_450));

								//!< Atomic case checks the return values of the atomic function and not the end result.
								const string caseIntermValues = formatName + "_intermediate_values" + suffix;
								backingTypeGroup->addChild(new BinaryAtomicIntermValuesCase(testCtx, caseIntermValues, "", imageType, imageSize, format, s_tilings[tilingNdx], operation, useTransfer, readType.type, backingType.type, glu::GLSL_VERSION_450));
							}
						}

						readTypeGroup->addChild(backingTypeGroup.release());
					}

					transferGroup->addChild(readTypeGroup.release());
				}

				imageTypeGroup->addChild(transferGroup.release());
			}

			operationGroup->addChild(imageTypeGroup.release());
		}

		imageAtomicOperationsTests->addChild(operationGroup.release());
	}

	return imageAtomicOperationsTests.release();
}

} // image
} // vkt
