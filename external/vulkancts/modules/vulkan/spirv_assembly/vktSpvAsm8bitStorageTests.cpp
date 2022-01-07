/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for the VK_KHR_8bit_storage
 *//*--------------------------------------------------------------------*/


#include "vktSpvAsm8bitStorageTests.hpp"

#include "tcuFloat.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deMath.h"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include <limits>
#include <map>
#include <string>
#include <sstream>
#include <utility>

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::IVec4;
using tcu::RGBA;
using tcu::TestLog;
using tcu::TestStatus;
using tcu::Vec4;
using de::UniquePtr;
using tcu::StringTemplate;
using tcu::Vec4;

namespace
{
static const deUint32	arrayStrideInBytesUniform	= 16u; // from the spec

enum ShaderTemplate
{
	SHADERTEMPLATE_STRIDE8BIT_STD140 = 0,
	SHADERTEMPLATE_STRIDE32BIT_STD140,
	SHADERTEMPLATE_STRIDEMIX_STD140,
	SHADERTEMPLATE_STRIDE8BIT_STD430,
	SHADERTEMPLATE_STRIDE32BIT_STD430,
	SHADERTEMPLATE_STRIDEMIX_STD430
};

struct StructTestData
{
	const int structArraySize; //Size of Struct Array
	const int nestedArraySize; //Max size of any nested arrays
};

struct Capability
{
	const char*				name;
	const char*				cap;
	const char*				decor;
	vk::VkDescriptorType	dtype;
};

enum
{
	STORAGE_BUFFER_TEST = 0,
	UNIFORM_AND_STORAGEBUFFER_TEST
};

static const Capability	CAPABILITIES[]	=
{
	{"storage_buffer",				"StorageBuffer8BitAccess",				"StorageBuffer",	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
	{"uniform",						"UniformAndStorageBuffer8BitAccess",	"Block",			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
};

static const StructTestData structData = {7, 11};

int getStructSize(const ShaderTemplate shaderTemplate)
{
	switch (shaderTemplate)
	{
	case SHADERTEMPLATE_STRIDE8BIT_STD140:
		return 1184 * structData.structArraySize;	//size of struct in 8b with offsets
	case SHADERTEMPLATE_STRIDE32BIT_STD140:
		return 304 * structData.structArraySize;	//size of struct in 32b with offsets
	case SHADERTEMPLATE_STRIDEMIX_STD140:
		return 4480 * structData.structArraySize;	//size of struct in 8b with offsets
	case SHADERTEMPLATE_STRIDE8BIT_STD430:
		return 224 * structData.structArraySize;	//size of struct in 8b with offset
	case SHADERTEMPLATE_STRIDE32BIT_STD430:
		return 184 * structData.structArraySize;	//size of struct in 32b with offset
	case SHADERTEMPLATE_STRIDEMIX_STD430:
		return 976 * structData.structArraySize;	//size of struct in 8b with offset
	default:
		DE_ASSERT(0);
	}
	return 0;
}

VulkanFeatures	get8BitStorageFeatures	(const char* cap)
{
	VulkanFeatures features;
	if (string(cap) == "storage_buffer")
		features.ext8BitStorage = EXT8BITSTORAGEFEATURES_STORAGE_BUFFER;
	else if (string(cap) == "uniform")
		features.ext8BitStorage = EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER;
	else if  (string(cap) == "push_constant")
		features.ext8BitStorage = EXT8BITSTORAGEFEATURES_PUSH_CONSTANT;
	else
		DE_ASSERT(false && "not supported");

	return features;
}

bool computeCheckBuffers (const std::vector<Resource>&	originalInts,
						  const vector<AllocationSp>&	outputAllocs,
						  const std::vector<Resource>&	/*expectedOutputs*/,
						  tcu::TestLog&					/*log*/)
{
	std::vector<deUint8> result;
	originalInts.front().getBytes(result);
	return deMemCmp(&result[0], outputAllocs.front()->getHostPtr(), result.size()) == 0;
}

void addInfo(vector<bool>& info, int& ndx, const int count, const bool isData)
{
	for (int index = 0; index < count; ++index)
		info[ndx++] = isData;
}

vector<deInt8> data8bit (const ShaderTemplate std, de::Random& rnd, const bool isData = true)
{
	const int size = getStructSize(std);
	if (!isData)
		return vector<deInt8>(size, 0);
	return getInt8s(rnd, size);
}

vector<deInt32> data32bit (const ShaderTemplate std, de::Random& rnd, const bool isData = true)
{
	const int size = getStructSize(std);
	if (!isData)
		return vector<deInt32>(size, 0);
	return getInt32s(rnd, size);
}

vector<bool> info8bitStd140 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDE8BIT_STD140));

	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;						//i8
		infoData[ndx++] = false;					//offset

		infoData[ndx++] = true;						//v2i8
		infoData[ndx++] = true;						//v2i8

		addInfo(infoData, ndx, 3, true);			//v3i8
		infoData[ndx++] = false;					//offset

		addInfo(infoData, ndx, 4, true);			//v4i8
		addInfo(infoData, ndx, 4, false);			//offset

		//i8[3];
		for (int i = 0; i < 3; ++i)
		{
			infoData[ndx++] = true;					//i8[i];
			addInfo(infoData, ndx, 15, false);		//offset
		}

		//struct {i8, v2i8[3]} [11]
		for (int i = 0; i < 11; ++i)
		{
			//struct.i8
			infoData[ndx++] = true;					//i8
			addInfo(infoData, ndx, 15, false);		//offset
			//struct.v2i8[3]
			for (int j = 0; j < 3; ++j)
			{
				infoData[ndx++] = true;				//v2i8
				infoData[ndx++] = true;				//v2i8
				addInfo(infoData, ndx, 14, false);	//offset
			}
		}

		//v2i8[11];
		for (int i = 0; i < 11; ++i)
		{
			infoData[ndx++] = true;					//v2i8
			infoData[ndx++] = true;					//v2i8
			addInfo(infoData, ndx, 14, false);		//offset
		}

		//i8
		infoData[ndx++] = true;						//i8
		addInfo(infoData, ndx, 15, false);			//offset

		//v3i8[11]
		for (int i = 0; i < 11; ++i)
		{
			addInfo(infoData, ndx, 3, true);		//v3i8
			addInfo(infoData, ndx, 13, false);		//offset
		}

		//v4i8[3]
		for (int i = 0; i < 3; ++i)
		{
			addInfo(infoData, ndx, 4, true);		//v4i8
			addInfo(infoData, ndx, 12, false);		//offset
		}
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));

	return infoData;
}

vector<bool> info8bitStd430 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDE8BIT_STD430));

	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;					//i8
		infoData[ndx++] = false;				//offset

		infoData[ndx++] = true;					//v2i8
		infoData[ndx++] = true;					//v2i8

		addInfo(infoData, ndx, 3, true);		//v3i8
		infoData[ndx++] = false;				//offset

		addInfo(infoData, ndx, 4, true);		//v4i8
		addInfo(infoData, ndx, 4, false);		//offset

		//i8[3];
		for (int i = 0; i < 3; ++i)
		{
			infoData[ndx++] = true;				//i8;
		}
		addInfo(infoData, ndx, 13, false);		//offset

		//struct {i8, v2i8[3]} [11]
		for (int i = 0; i < 11; ++i)
		{
			//struct.i8
			infoData[ndx++] = true;				//i8
			infoData[ndx++] = false;			//offset
			//struct.v2i8[3]
			for (int j = 0; j < 3; ++j)
			{
				infoData[ndx++] = true;			//v2i8
				infoData[ndx++] = true;			//v2i8
			}
		}
		addInfo(infoData, ndx, 8, false);		//offset

		//vec2[11];
		for (int i = 0; i < 11; ++i)
		{
			infoData[ndx++] = true;				//v2i8
			infoData[ndx++] = true;				//v2i8
		}

		//i8
		infoData[ndx++] = true;					//i8
		addInfo(infoData, ndx, 9, false);		//offset

		//v3i8[11]
		for (int i = 0; i < 11; ++i)
		{
			addInfo(infoData, ndx, 3, true);	//v3i8
			infoData[ndx++] = false;			//offset
		}
		addInfo(infoData, ndx, 4, false);		//offset

		//v4i8[3]
		for (int i = 0; i < 3; ++i)
		{
			addInfo(infoData, ndx, 4, true);	//v4i8
		}
		addInfo(infoData, ndx, 4, false);		//offset
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));
	return infoData;
}

vector<bool> info32bitStd140 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDE32BIT_STD140));

	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;					//i32
		infoData[ndx++] = false;				//offset

		infoData[ndx++] = true;					//v2i32
		infoData[ndx++] = true;					//v2i32

		addInfo(infoData, ndx, 3, true);		//v3i32
		infoData[ndx++] = false;				//offset

		addInfo(infoData, ndx, 4, true);		//v4i32

		//i32[3];
		for (int i = 0; i < 3; ++i)
		{
			infoData[ndx++] = true;				//i32;
			addInfo(infoData, ndx, 3, false);	//offset
		}

		//struct {i32, v2i32[3]} [11]
		for (int i = 0; i < 11; ++i)
		{
			//struct.f32
			infoData[ndx++] = true;				//i32
			addInfo(infoData, ndx, 3, false);	//offset
			//struct.f32.v2f16[3]
			for (int j = 0; j < 3; ++j)
			{
				infoData[ndx++] = true;			//v2i32
				infoData[ndx++] = true;			//v2i32
				infoData[ndx++] = false;		//offset
				infoData[ndx++] = false;		//offset
			}
		}

		//v2f32[11];
		for (int i = 0; i < 11; ++i)
		{
			infoData[ndx++] = true;				//v2i32
			infoData[ndx++] = true;				//v2i32
			infoData[ndx++] = false;			//offset
			infoData[ndx++] = false;			//offset
		}

		//i32
		infoData[ndx++] = true;					//i32
		addInfo(infoData, ndx, 3, false);		//offset

		//v3i32[11]
		for (int i = 0; i < 11; ++i)
		{
			addInfo(infoData, ndx, 3, true);	//v3i32
			infoData[ndx++] = false;			//offset
		}

		//v4i32[3]
		for (int i = 0; i < 3; ++i)
		{
			addInfo(infoData, ndx, 4, true);	//v4i32
		}
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));
	return infoData;
}

vector<bool> info32bitStd430 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDE32BIT_STD430));

	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;					//i32
		infoData[ndx++] = false;				//offset

		addInfo(infoData, ndx, 2, true);		//v2i32

		addInfo(infoData, ndx, 3, true);		//v3i32
		infoData[ndx++] = false;				//offset

		addInfo(infoData, ndx, 4, true);		//v4i32

		addInfo(infoData, ndx, 3, true);		//i32[3];
		infoData[ndx++] = false;				//offset

		//struct {i32, v2i32[3]} [11]
		for (int i = 0; i < 11; ++i)
		{
			//struct.i32
			infoData[ndx++] = true;				//i32
			infoData[ndx++] = false;			//offset
			addInfo(infoData, ndx, 6, true);	//v2i32[3]
		}

		addInfo(infoData, ndx, 22, true);		//v2i32[11];

		//i32
		infoData[ndx++] = true;					//i32
		infoData[ndx++] = false;				//offset

		//v3i32[11]
		for (int i = 0; i < 11; ++i)
		{
			addInfo(infoData, ndx, 3, true);	//v3i32
			infoData[ndx++] = false;			//offset
		}

		addInfo(infoData, ndx, 12, true);	//v4i32[3]
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));
	return infoData;
}

vector<bool> infoMixStd140 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDEMIX_STD140));
	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;					//8b
		addInfo(infoData, ndx, 3, false);		//offset

		addInfo(infoData, ndx, 4, true);		//32b

		addInfo(infoData, ndx, 2, true);		//v2b8
		addInfo(infoData, ndx, 6, false);		//offset

		addInfo(infoData, ndx, 8, true);		//v2b32

		addInfo(infoData, ndx, 3, true);		//v3b8
		addInfo(infoData, ndx, 5, false);		//offset

		addInfo(infoData, ndx, 12, true);		//v3b32
		addInfo(infoData, ndx, 4,  false);		//offset

		addInfo(infoData, ndx, 4, true);		//v4b8
		addInfo(infoData, ndx, 12, false);		//offset

		addInfo(infoData, ndx, 16, true);		//v4b32

		//strut {b8, b32, v2b8[11], b32[11]}
		for (int i = 0; i < structData.nestedArraySize; ++i)
		{
			infoData[ndx++] = true;				//8b
			addInfo(infoData, ndx, 3, false);	//offset

			addInfo(infoData, ndx, 4, true);	//32b
			addInfo(infoData, ndx, 8, false);	//offset

			for (int j = 0; j < structData.nestedArraySize; ++j)
			{
				addInfo(infoData, ndx, 2, true);	//v2b8[11]
				addInfo(infoData, ndx, 14, false);	//offset
			}

			for (int j = 0; j < structData.nestedArraySize; ++j)
			{
				addInfo(infoData, ndx, 4, true);	//b32[11]
				addInfo(infoData, ndx, 12, false);	//offset
			}
		}

		for (int i = 0; i < structData.nestedArraySize; ++i)
		{
			infoData[ndx++] = true;				//8b[11]
			addInfo(infoData, ndx, 15, false);	//offset
		}

		for (int i = 0; i < structData.nestedArraySize; ++i)
		{
			addInfo(infoData, ndx, 4, true);	//b32bIn[11]
			addInfo(infoData, ndx, 12, false);	//offset
		}
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));
	return infoData;
}

vector<bool> infoMixStd430 (void)
{
	int				ndx			= 0u;
	vector<bool>	infoData	(getStructSize(SHADERTEMPLATE_STRIDEMIX_STD430));
	for(int elementNdx = 0; elementNdx < structData.structArraySize; ++elementNdx)
	{
		infoData[ndx++] = true;					//8b
		addInfo(infoData, ndx, 3, false);		//offset

		addInfo(infoData, ndx, 4, true);		//32b

		addInfo(infoData, ndx, 2, true);		//v2b8
		addInfo(infoData, ndx, 6, false);		//offset

		addInfo(infoData, ndx, 8, true);		//v2b32

		addInfo(infoData, ndx, 3, true);		//v3b8
		addInfo(infoData, ndx, 5, false);		//offset

		addInfo(infoData, ndx, 12, true);		//v3b32
		addInfo(infoData, ndx, 4,  false);		//offset

		addInfo(infoData, ndx, 4, true);		//v4b8
		addInfo(infoData, ndx, 12, false);		//offset

		addInfo(infoData, ndx, 16, true);		//v4b32

		//strut {b8, b32, v2b8[11], b32[11]}
		for (int i = 0; i < structData.nestedArraySize; ++i)
		{
			infoData[ndx++] = true;				//8b
			addInfo(infoData, ndx, 3, false);	//offset

			addInfo(infoData, ndx, 4, true);	//32b

			addInfo(infoData, ndx, 22, true);	//v2b8[11]
			addInfo(infoData, ndx, 2, false);	//offset

			addInfo(infoData, ndx, 44, true);	//b32[11]
		}

		addInfo(infoData, ndx, 11, true);		//8b[11]
		infoData[ndx++] = false;				//offset

		addInfo(infoData, ndx, 44, true);		//32b[11]
		addInfo(infoData, ndx, 4, false);		//offset
	}

	//Please check the data and offset
	DE_ASSERT(ndx == static_cast<int>(infoData.size()));
	return infoData;
}

template<typename originType, typename resultType, ShaderTemplate funcOrigin, ShaderTemplate funcResult>
bool compareStruct(const resultType* returned, const originType* original)
{
	vector<bool>		resultInfo;
	vector<bool>		originInfo;
	vector<resultType >	resultToCompare;
	vector<originType >	originToCompare;

	switch(funcOrigin)
	{
	case SHADERTEMPLATE_STRIDE8BIT_STD140:
		originInfo = info8bitStd140();
		break;
	case SHADERTEMPLATE_STRIDE8BIT_STD430:
		originInfo = info8bitStd430();
		break;
	case SHADERTEMPLATE_STRIDE32BIT_STD140:
		originInfo = info32bitStd140();
		break;
	case SHADERTEMPLATE_STRIDE32BIT_STD430:
		originInfo = info32bitStd430();
		break;
	case SHADERTEMPLATE_STRIDEMIX_STD140:
		originInfo = infoMixStd140();
		break;
	case SHADERTEMPLATE_STRIDEMIX_STD430:
		originInfo = infoMixStd430();
		break;
	default:
		DE_ASSERT(0);
	}

	switch(funcResult)
	{
	case SHADERTEMPLATE_STRIDE8BIT_STD140:
		resultInfo = info8bitStd140();
		break;
	case SHADERTEMPLATE_STRIDE8BIT_STD430:
		resultInfo = info8bitStd430();
		break;
	case SHADERTEMPLATE_STRIDE32BIT_STD140:
		resultInfo = info32bitStd140();
		break;
	case SHADERTEMPLATE_STRIDE32BIT_STD430:
		resultInfo = info32bitStd430();
		break;
	case SHADERTEMPLATE_STRIDEMIX_STD140:
		resultInfo = infoMixStd140();
		break;
	case SHADERTEMPLATE_STRIDEMIX_STD430:
		resultInfo = infoMixStd430();
		break;
	default:
		DE_ASSERT(0);
	}

	for (int ndx = 0; ndx < static_cast<int>(resultInfo.size()); ++ndx)
	{
		if (resultInfo[ndx])
			resultToCompare.push_back(returned[ndx]);
	}

	for (int ndx = 0; ndx < static_cast<int>(originInfo.size()); ++ndx)
	{
		if (originInfo[ndx])
			originToCompare.push_back(original[ndx]);
	}

	//Different offset but that same amount of data
	DE_ASSERT(originToCompare.size() == resultToCompare.size());

	for (int ndx = 0; ndx < static_cast<int>(originToCompare.size()); ++ndx)
	{
		if (static_cast<deInt8>(originToCompare[ndx]) != static_cast<deInt8>(resultToCompare[ndx]))
			return false;
	}
	return true;
}

template<typename originType, typename resultType, ShaderTemplate funcOrigin, ShaderTemplate funcResult>
bool checkStruct (const std::vector<Resource>&	originalFloats,
				  const vector<AllocationSp>&	outputAllocs,
				  const std::vector<Resource>&	/* expectedOutputs */,
				  tcu::TestLog&					/* log */)
{
	for (deUint32 outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		vector<deUint8>	originalBytes;
		originalFloats[outputNdx].getBytes(originalBytes);

		const resultType*	returned	= static_cast<const resultType*>(outputAllocs[outputNdx]->getHostPtr());
		const originType*	original	= reinterpret_cast<const originType*>(&originalBytes.front());

		if(!compareStruct<originType, resultType, funcOrigin, funcResult>(returned, original))
			return false;
	}
	return true;
}

template<typename originType, typename resultType, deUint32 compositCount>
bool checkUniformsArray (const std::vector<Resource>&	originalFloats,
						 const vector<AllocationSp>&	outputAllocs,
						 const std::vector<Resource>&	/* expectedOutputs */,
						 tcu::TestLog&					/* log */)
{
	const deUint32	originTypeSize = static_cast<deUint32>(sizeof(originType));

	DE_ASSERT((originTypeSize * compositCount) <= arrayStrideInBytesUniform); // one element of array can't be bigger then 16B

	for (deUint32 outputNdx = 0; outputNdx < static_cast<deUint32>(outputAllocs.size()); ++outputNdx)
	{
		vector<deUint8>	originalBytes;
		originalFloats[outputNdx].getBytes(originalBytes);
		const int elemntsNumber = (static_cast<int>(originalBytes.size()) / arrayStrideInBytesUniform) / compositCount;

		const resultType*	returned = static_cast<const resultType*>(outputAllocs[outputNdx]->getHostPtr());
		const originType*	original = reinterpret_cast<const originType*>(&originalBytes.front());

		for (int ndx = 0; ndx < elemntsNumber; ++ndx)
		{
			for (deUint32 ndxData = 0u; ndxData < compositCount; ++ndxData)
			{
				if (static_cast<deInt8>(*original) != static_cast<deInt8>(*returned))
					return false;
				original++;
				returned++;
			}
			original += arrayStrideInBytesUniform / originTypeSize - compositCount;
		}
	}
	return true;
}

template<typename originType, typename resultType, int compositCount, int ndxConts>
bool checkUniformsArrayConstNdx (const std::vector<Resource>&	originalFloats,
								 const vector<AllocationSp>&	outputAllocs,
								 const std::vector<Resource>&	/* expectedOutputs */,
								 tcu::TestLog&					/* log */)
{
	const deUint32	originTypeSize = static_cast<deUint32>(sizeof(originType));

	DE_ASSERT((originTypeSize * compositCount) <= arrayStrideInBytesUniform); // one element of array can't be bigger then 16B

	for (deUint32 outputNdx = 0; outputNdx < static_cast<deUint32>(outputAllocs.size()); ++outputNdx)
	{
		vector<deUint8>	originalBytes;
		originalFloats[outputNdx].getBytes(originalBytes);
		const int elemntsNumber = (static_cast<int>(originalBytes.size()) / arrayStrideInBytesUniform) / compositCount;

		const resultType*	returned = static_cast<const resultType*>(outputAllocs[outputNdx]->getHostPtr());
		const originType*	original = reinterpret_cast<const originType*>(&originalBytes.front());

		deUint32 idx = (arrayStrideInBytesUniform / originTypeSize) * ndxConts;

		for (int ndx = 0; ndx < elemntsNumber; ++ndx)
		{
			for (int ndxData = 0; ndxData < compositCount; ++ndxData)
			{
				if (static_cast<deInt8>(original[idx + ndxData]) != static_cast<deInt8>(*returned))
					return false;
				returned++;
			}
		}
	}
	return true;
}


string getStructShaderComponet (const ShaderTemplate component)
{
	switch(component)
	{
	case SHADERTEMPLATE_STRIDE8BIT_STD140:
		return string(
		//struct {i8, v2i8[3]} [11]
		"OpDecorate %v2i8arr3 ArrayStride 16\n"
		"OpMemberDecorate %struct8 0 Offset 0\n"
		"OpMemberDecorate %struct8 1 Offset 16\n"
		"OpDecorate %struct8arr11 ArrayStride 64\n"
		"\n"
		"OpDecorate %i8arr3       ArrayStride 16\n"
		"OpDecorate %v2i8arr11    ArrayStride 16\n"
		"OpDecorate %v3i8arr11    ArrayStride 16\n"
		"OpDecorate %v4i8arr3     ArrayStride 16\n"
		"OpDecorate %i8StructArr7 ArrayStride 1184\n"
		"\n"
		"OpMemberDecorate %i8Struct 0 Offset 0\n"		//i8
		"OpMemberDecorate %i8Struct 1 Offset 2\n"		//v2i8
		"OpMemberDecorate %i8Struct 2 Offset 4\n"		//v3i8
		"OpMemberDecorate %i8Struct 3 Offset 8\n"		//v4i8
		"OpMemberDecorate %i8Struct 4 Offset 16\n"		//i8[3]
		"OpMemberDecorate %i8Struct 5 Offset 64\n"		//struct {i8, v2i8[3]} [11]
		"OpMemberDecorate %i8Struct 6 Offset 768\n"		//v2i8[11]
		"OpMemberDecorate %i8Struct 7 Offset 944\n"		//i8
		"OpMemberDecorate %i8Struct 8 Offset 960\n"		//v3i8[11]
		"OpMemberDecorate %i8Struct 9 Offset 1136\n");	//v4i8[3]
	case SHADERTEMPLATE_STRIDE8BIT_STD430:
		return string(
		//struct {i8, v2i8[3]} [11]
		"OpDecorate %v2i8arr3     ArrayStride 2\n"
		"OpMemberDecorate %struct8 0 Offset 0\n"
		"OpMemberDecorate %struct8 1 Offset 2\n"
		"OpDecorate %struct8arr11 ArrayStride 8\n"
		"\n"
		"OpDecorate %i8arr3    ArrayStride 1\n"
		"OpDecorate %v2i8arr11 ArrayStride 2\n"
		"OpDecorate %v3i8arr11 ArrayStride 4\n"
		"OpDecorate %v4i8arr3  ArrayStride 4\n"
		"OpDecorate %i8StructArr7 ArrayStride 224\n"
		"\n"
		"OpMemberDecorate %i8Struct 0 Offset 0\n"		//i8
		"OpMemberDecorate %i8Struct 1 Offset 2\n"		//v2i8
		"OpMemberDecorate %i8Struct 2 Offset 4\n"		//v3i8
		"OpMemberDecorate %i8Struct 3 Offset 8\n"		//v4i8
		"OpMemberDecorate %i8Struct 4 Offset 16\n"		//i8[3]
		"OpMemberDecorate %i8Struct 5 Offset 32\n"		//struct {i8, v2i8[3]} [11]
		"OpMemberDecorate %i8Struct 6 Offset 128\n"		//v2i8[11]
		"OpMemberDecorate %i8Struct 7 Offset 150\n"		//i8
		"OpMemberDecorate %i8Struct 8 Offset 160\n"		//v3i8[11]
		"OpMemberDecorate %i8Struct 9 Offset 208\n");	//v4i8[3]
	case SHADERTEMPLATE_STRIDE32BIT_STD140:
		return string (
		//struct {i32, v2i32[3]} [11]
		"OpDecorate %v2i32arr3 ArrayStride 16\n"
		"OpMemberDecorate %struct32 0 Offset 0\n"
		"OpMemberDecorate %struct32 1 Offset 16\n"
		"OpDecorate %struct32arr11 ArrayStride 64\n"
		"\n"
		"OpDecorate %i32arr3   ArrayStride 16\n"
		"OpDecorate %v2i32arr11 ArrayStride 16\n"
		"OpDecorate %v3i32arr11 ArrayStride 16\n"
		"OpDecorate %v4i32arr3 ArrayStride 16\n"
		"OpDecorate %i32StructArr7 ArrayStride 1216\n"
		"\n"
		"OpMemberDecorate %i32Struct 0 Offset 0\n"		//i32
		"OpMemberDecorate %i32Struct 1 Offset 8\n"		//v2i32
		"OpMemberDecorate %i32Struct 2 Offset 16\n"		//v3i32
		"OpMemberDecorate %i32Struct 3 Offset 32\n"		//v4i32
		"OpMemberDecorate %i32Struct 4 Offset 48\n"		//i32[3]
		"OpMemberDecorate %i32Struct 5 Offset 96\n"		//struct {i32, v2i32[3]} [11]
		"OpMemberDecorate %i32Struct 6 Offset 800\n"	//v2i32[11]
		"OpMemberDecorate %i32Struct 7 Offset 976\n"	//i32
		"OpMemberDecorate %i32Struct 8 Offset 992\n"	//v3i32[11]
		"OpMemberDecorate %i32Struct 9 Offset 1168\n");	//v4i32[3]
	case SHADERTEMPLATE_STRIDE32BIT_STD430:
		return string(
		//struct {i32, v2i32[3]} [11]
		"OpDecorate %v2i32arr3 ArrayStride 8\n"
		"OpMemberDecorate %struct32 0 Offset 0\n"
		"OpMemberDecorate %struct32 1 Offset 8\n"
		"OpDecorate %struct32arr11 ArrayStride 32\n"
		"\n"
		"OpDecorate %i32arr3    ArrayStride 4\n"
		"OpDecorate %v2i32arr11 ArrayStride 8\n"
		"OpDecorate %v3i32arr11 ArrayStride 16\n"
		"OpDecorate %v4i32arr3  ArrayStride 16\n"
		"OpDecorate %i32StructArr7 ArrayStride 736\n"
		"\n"
		"OpMemberDecorate %i32Struct 0 Offset 0\n"		//i32
		"OpMemberDecorate %i32Struct 1 Offset 8\n"		//v2i32
		"OpMemberDecorate %i32Struct 2 Offset 16\n"		//v3i32
		"OpMemberDecorate %i32Struct 3 Offset 32\n"		//v4i32
		"OpMemberDecorate %i32Struct 4 Offset 48\n"		//i32[3]
		"OpMemberDecorate %i32Struct 5 Offset 64\n"		//struct {i32, v2i32[3]}[11]
		"OpMemberDecorate %i32Struct 6 Offset 416\n"	//v2i32[11]
		"OpMemberDecorate %i32Struct 7 Offset 504\n"	//i32
		"OpMemberDecorate %i32Struct 8 Offset 512\n"	//v3i32[11]
		"OpMemberDecorate %i32Struct 9 Offset 688\n");	//v4i32[3]
	case SHADERTEMPLATE_STRIDEMIX_STD140:
		return string(
		"\n"//strutNestedIn {b8, b32, v2b8[11], b32[11]}
		"OpDecorate %v2b8NestedArr11${InOut} ArrayStride 16\n"	//v2b8[11]
		"OpDecorate %b32NestedArr11${InOut} ArrayStride 16\n"	//b32[11]
		"OpMemberDecorate %sNested${InOut} 0 Offset 0\n"		//b8
		"OpMemberDecorate %sNested${InOut} 1 Offset 4\n"		//b32
		"OpMemberDecorate %sNested${InOut} 2 Offset 16\n"		//v2b8[11]
		"OpMemberDecorate %sNested${InOut} 3 Offset 192\n"		//b32[11]
		"OpDecorate %sNestedArr11${InOut} ArrayStride 368\n"	//strutNestedIn[11]
		"\n"//strutIn {b8, b32, v2b8, v2b32, v3b8, v3b32, v4b8, v4b32, strutNestedIn[11], b8In[11], b32bIn[11]}
		"OpDecorate %sb8Arr11${InOut} ArrayStride 16\n"			//b8In[11]
		"OpDecorate %sb32Arr11${InOut} ArrayStride 16\n"		//b32bIn[11]
		"OpMemberDecorate %struct${InOut} 0 Offset 0\n"			//b8
		"OpMemberDecorate %struct${InOut} 1 Offset 4\n"			//b32
		"OpMemberDecorate %struct${InOut} 2 Offset 8\n"			//v2b8
		"OpMemberDecorate %struct${InOut} 3 Offset 16\n"		//v2b32
		"OpMemberDecorate %struct${InOut} 4 Offset 24\n"		//v3b8
		"OpMemberDecorate %struct${InOut} 5 Offset 32\n"		//v3b32
		"OpMemberDecorate %struct${InOut} 6 Offset 48\n"		//v4b8
		"OpMemberDecorate %struct${InOut} 7 Offset 64\n"		//v4b32
		"OpMemberDecorate %struct${InOut} 8 Offset 80\n"		//strutNestedIn[11]
		"OpMemberDecorate %struct${InOut} 9 Offset 4128\n"		//b8In[11]
		"OpMemberDecorate %struct${InOut} 10 Offset 4304\n"		//b32bIn[11]
		"OpDecorate %structArr7${InOut} ArrayStride 4480\n");	//strutIn[7]
	case SHADERTEMPLATE_STRIDEMIX_STD430:
		return string(
		"\n"//strutNestedOut {b8, b32, v2b8[11], b32[11]}
		"OpDecorate %v2b8NestedArr11${InOut} ArrayStride 2\n"	//v2b8[11]
		"OpDecorate %b32NestedArr11${InOut}  ArrayStride 4\n"	//b32[11]
		"OpMemberDecorate %sNested${InOut} 0 Offset 0\n"		//b8
		"OpMemberDecorate %sNested${InOut} 1 Offset 4\n"		//b32
		"OpMemberDecorate %sNested${InOut} 2 Offset 8\n"		//v2b8[11]
		"OpMemberDecorate %sNested${InOut} 3 Offset 32\n"		//b32[11]
		"OpDecorate %sNestedArr11${InOut} ArrayStride 76\n"		//strutNestedOut[11]
		"\n"//strutOut {b8, b32, v2b8, v2b32, v3b8, v3b32, v4b8, v4b32, strutNestedOut[11], b8Out[11], b32bOut[11]}
		"OpDecorate %sb8Arr11${InOut} ArrayStride 1\n"			//b8Out[11]
		"OpDecorate %sb32Arr11${InOut} ArrayStride 4\n"			//b32bOut[11]
		"OpMemberDecorate %struct${InOut} 0 Offset 0\n"			//b8
		"OpMemberDecorate %struct${InOut} 1 Offset 4\n"			//b32
		"OpMemberDecorate %struct${InOut} 2 Offset 8\n"			//v2b8
		"OpMemberDecorate %struct${InOut} 3 Offset 16\n"		//v2b32
		"OpMemberDecorate %struct${InOut} 4 Offset 24\n"		//v3b8
		"OpMemberDecorate %struct${InOut} 5 Offset 32\n"		//v3b32
		"OpMemberDecorate %struct${InOut} 6 Offset 48\n"		//v4b8
		"OpMemberDecorate %struct${InOut} 7 Offset 64\n"		//v4b32
		"OpMemberDecorate %struct${InOut} 8 Offset 80\n"		//strutNestedOut[11]
		"OpMemberDecorate %struct${InOut} 9 Offset 916\n"		//b8Out[11]
		"OpMemberDecorate %struct${InOut} 10 Offset 928\n"		//b32bOut[11]
		"OpDecorate %structArr7${InOut} ArrayStride 976\n");	//strutOut[7]
	default:
		DE_ASSERT(0);
		return string("");
	}
}
/*Return string contains spirv loop begin.
 the spec should contains "exeCount" - with name of const i32, it is number of executions
 the spec should contains "loopName" - suffix for all local names
 %Val${loopName} - index which can be used inside loop
 "%ndxArr${loopName}   = OpVariable %fp_i32  Function\n" - has to be defined outside
 The function should be always use with endLoop function*/
std::string beginLoop(const std::map<std::string, std::string>& spec)
{
	const tcu::StringTemplate	loopBegin	(
	"OpStore %ndxArr${loopName} %zero\n"
	"OpBranch %Loop${loopName}\n"
	"%Loop${loopName} = OpLabel\n"
	"OpLoopMerge %MergeLabel1${loopName} %MergeLabel2${loopName} None\n"
	"OpBranch %Label1${loopName}\n"
	"%Label1${loopName} = OpLabel\n"
	"%Val${loopName} = OpLoad %i32 %ndxArr${loopName}\n"
	"%LessThan${loopName} = OpSLessThan %bool %Val${loopName} %${exeCount}\n"
	"OpBranchConditional %LessThan${loopName} %ifLabel${loopName} %MergeLabel1${loopName}\n"
	"%ifLabel${loopName} = OpLabel\n");
	return loopBegin.specialize(spec);
}
/*Return string contains spirv loop end.
 the spec should contains "loopName" - suffix for all local names, suffix should be the same in beginLoop
The function should be always use with beginLoop function*/
std::string endLoop(const std::map<std::string, std::string>& spec)
{
	const tcu::StringTemplate	loopEnd	(
	"OpBranch %MergeLabel2${loopName}\n"
	"%MergeLabel2${loopName} = OpLabel\n"
	"%plusOne${loopName} = OpIAdd %i32 %Val${loopName} %c_i32_1\n"
	"OpStore %ndxArr${loopName} %plusOne${loopName}\n"
	"OpBranch %Loop${loopName}\n"
	"%MergeLabel1${loopName} = OpLabel\n");
	return loopEnd.specialize(spec);
}

void addCompute8bitStorage32To8Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 128;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %SSBO32 Block\n"
		"OpDecorate %SSBO8 Block\n"
		"OpMemberDecorate %SSBO32 0 Offset 0\n"
		"OpMemberDecorate %SSBO8 0 Offset 0\n"
		"OpDecorate %ssbo32 DescriptorSet 0\n"
		"OpDecorate %ssbo8 DescriptorSet 0\n"
		"OpDecorate %ssbo32 Binding 0\n"
		"OpDecorate %ssbo8 Binding 1\n"

		"${matrix_decor:opt}\n"

		"${rounding:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%f32       = OpTypeFloat 32\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%fvec3     = OpTypeVector %f32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"
		"%i32ptr    = OpTypePointer StorageBuffer %i32\n"
		"%f32ptr    = OpTypePointer StorageBuffer %f32\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"
		"%c_i32_128 = OpConstant %i32 128\n"

		"%i32arr    = OpTypeArray %i32 %c_i32_128\n"
		"%f32arr    = OpTypeArray %f32 %c_i32_128\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%SSBO32    = OpTypeStruct %${matrix_prefix:opt}${base32}arr\n"
		"%SSBO8    = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%up_SSBO32 = OpTypePointer ${storage} %SSBO32\n"
		"%up_SSBO8 = OpTypePointer ${storage} %SSBO8\n"
		"%ssbo32    = OpVariable %up_SSBO32 ${storage}\n"
		"%ssbo8    = OpVariable %up_SSBO8 ${storage}\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base32}ptr %ssbo32 %zero %x ${index0:opt}\n"
		"%val32     = OpLoad %${base32} %inloc\n"
		"%val8     = ${convert} %${base8} %val32\n"
		"%outloc    = OpAccessChain %${base8}ptr %ssbo8 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val8\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{  // Integers
		const char		sintTypes[]	=
			"%i8       = OpTypeInt 8 1\n"
			"%i8ptr    = OpTypePointer StorageBuffer %i8\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_128\n"
			"%v2i8     = OpTypeVector %i8 2\n"
			"%v4i8     = OpTypeVector %i8 4\n"
			"%v2i32    = OpTypeVector %i32 2\n"
			"%v4i32    = OpTypeVector %i32 4\n"
			"%v2i8ptr  = OpTypePointer StorageBuffer %v2i8\n"
			"%v2i32ptr = OpTypePointer StorageBuffer %v2i32\n"
			"%v2i8arr  = OpTypeArray %v2i8 %c_i32_64\n"
			"%v2i32arr = OpTypeArray %v2i32 %c_i32_64\n";

		const char		uintTypes[]	=
			"%u8       = OpTypeInt 8 0\n"
			"%u8ptr    = OpTypePointer StorageBuffer %u8\n"
			"%u32ptr   = OpTypePointer StorageBuffer %u32\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_128\n"
			"%u32arr   = OpTypeArray %u32 %c_i32_128\n"
			"%v2u8     = OpTypeVector %u8 2\n"
			"%v2u32    = OpTypeVector %u32 2\n"
			"%v4u32    = OpTypeVector %u32 4\n"
			"%v2u8ptr  = OpTypePointer StorageBuffer %v2u8\n"
			"%v2u32ptr = OpTypePointer StorageBuffer %v2u32\n"
			"%v2u8arr  = OpTypeArray %v2u8 %c_i32_64\n"
			"%v2u32arr = OpTypeArray %v2u32 %c_i32_64\n";

		struct CompositeType
		{
			const char*	name;
			const char* types;
			const char*	base32;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			unsigned	count;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	sintTypes,	"i32",		"i8",	"OpSConvert",	"OpDecorate %i32arr ArrayStride 4\nOpDecorate %i8arr ArrayStride 1\n",				numElements},
			{"scalar_uint",	uintTypes,	"u32",		"u8",	"OpUConvert",	"OpDecorate %u32arr ArrayStride 4\nOpDecorate %u8arr ArrayStride 1\n",				numElements},
			{"vector_sint",	sintTypes,	"v2i32",	"v2i8",	"OpSConvert",	"OpDecorate %v2i32arr ArrayStride 8\nOpDecorate %v2i8arr ArrayStride 2\n",			numElements / 2},
			{"vector_uint",	uintTypes,	"v2u32",	"v2u8",	"OpUConvert",	"OpDecorate %v2u32arr ArrayStride 8\nOpDecorate %v2u8arr ArrayStride 2\n",			numElements / 2},
		};

		vector<deInt32>	inputs			= getInt32s(rnd, numElements);
		vector<deInt8> outputs			(inputs.size());

		for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
			outputs[numNdx] = (static_cast<deInt8>(0xff & inputs[numNdx]));

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			string					testName	= string(CAPABILITIES[STORAGE_BUFFER_TEST].name) + "_" + cTypes[tyIdx].name;

			specs["capability"]		= CAPABILITIES[STORAGE_BUFFER_TEST].cap;
			specs["storage"]		= CAPABILITIES[STORAGE_BUFFER_TEST].decor;
			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base32"]			= cTypes[tyIdx].base32;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(cTypes[tyIdx].count, 1, 1);

			spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(inputs)), CAPABILITIES[STORAGE_BUFFER_TEST].dtype));
			spec.outputs.push_back(Resource(BufferSp(new Int8Buffer(outputs))));
			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[STORAGE_BUFFER_TEST].name);

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
		}
	}
}

void addCompute8bitUniform8To32Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx				= group->getTestContext();
	de::Random						rnd					(deStringHash(group->getName()));
	const int						numElements			= 128;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %SSBO32 Block\n"
		"OpDecorate %SSBO8 Block\n"
		"OpMemberDecorate %SSBO32 0 Offset 0\n"
		"OpMemberDecorate %SSBO8 0 Offset 0\n"
		"OpDecorate %SSBO8 ${storage}\n"
		"OpDecorate %ssbo32 DescriptorSet 0\n"
		"OpDecorate %ssbo8 DescriptorSet 0\n"
		"OpDecorate %ssbo32 Binding 1\n"
		"OpDecorate %ssbo8 Binding 0\n"

		"${matrix_decor:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"
		"%i32ptr    = OpTypePointer StorageBuffer %i32\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_2   = OpConstant %i32 2\n"
		"%c_i32_3   = OpConstant %i32 3\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"
		"%c_i32_128 = OpConstant %i32 128\n"

		"%i32arr    = OpTypeArray %i32 %c_i32_128\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%SSBO32    = OpTypeStruct %${matrix_prefix:opt}${base32}arr\n"
		"%SSBO8    = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
		"%up_SSBO8 = OpTypePointer Uniform %SSBO8\n"
		"%ssbo32    = OpVariable %up_SSBO32 StorageBuffer\n"
		"%ssbo8    = OpVariable %up_SSBO8 Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base8}ptr %ssbo8 %zero %x ${index0:opt}\n"
		"%val8     = OpLoad %${base8} %inloc\n"
		"%val32     = ${convert} %${base32} %val8\n"
		"%outloc    = OpAccessChain %${base32}ptr %ssbo32 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val32\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");


	{  // Integers
		const char		sintTypes[]		=
			"%i8       = OpTypeInt 8 1\n"
			"%i8ptr    = OpTypePointer Uniform %i8\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_128\n"
			"%v4i8     = OpTypeVector %i8 4\n"
			"%v4i32     = OpTypeVector %i32 4\n"
			"%v4i8ptr  = OpTypePointer Uniform %v4i8\n"
			"%v4i32ptr  = OpTypePointer StorageBuffer %v4i32\n"
			"%v4i8arr  = OpTypeArray %v4i8 %c_i32_32\n"
			"%v4i32arr  = OpTypeArray %v4i32 %c_i32_32\n";

		const char		uintTypes[]		=
			"%u8       = OpTypeInt 8 0\n"
			"%u8ptr    = OpTypePointer Uniform %u8\n"
			"%u32ptr    = OpTypePointer StorageBuffer %u32\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_128\n"
			"%u32arr    = OpTypeArray %u32 %c_i32_128\n"
			"%v4u8     = OpTypeVector %u8 4\n"
			"%v4u32     = OpTypeVector %u32 4\n"
			"%v4u8ptr  = OpTypePointer Uniform %v4u8\n"
			"%v4u32ptr  = OpTypePointer StorageBuffer %v4u32\n"
			"%v4u8arr  = OpTypeArray %v4u8 %c_i32_32\n"
			"%v4u32arr  = OpTypeArray %v4u32 %c_i32_32\n";

		struct CompositeType
		{
			const char*	name;
			const char* types;
			const char*	base32;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			const int	componentsCount;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	sintTypes,	"i32",		"i8",	"OpSConvert",	"OpDecorate %i32arr ArrayStride 4\nOpDecorate %i8arr ArrayStride 16\n",			1},
			{"scalar_uint",	uintTypes,	"u32",		"u8",	"OpUConvert",	"OpDecorate %u32arr ArrayStride 4\nOpDecorate %u8arr ArrayStride 16\n",			1},
			{"vector_sint",	sintTypes,	"v4i32",	"v4i8",	"OpSConvert",	"OpDecorate %v4i32arr ArrayStride 16\nOpDecorate %v4i8arr ArrayStride 16\n",	4},
			{"vector_uint",	uintTypes,	"v4u32",	"v4u8",	"OpUConvert",	"OpDecorate %v4u32arr ArrayStride 16\nOpDecorate %v4u8arr ArrayStride 16\n",	4},
		};

		vector<deInt32> outputs(numElements);

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			string					testName	= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + cTypes[tyIdx].name;

			vector<deInt8>	inputs = getInt8s(rnd, (arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt8))) * (numElements / cTypes[tyIdx].componentsCount));

			specs["capability"]		= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
			specs["storage"]		= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].decor;
			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base32"]			= cTypes[tyIdx].base32;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(numElements / cTypes[tyIdx].componentsCount, 1, 1);

			spec.inputs.push_back(Resource(BufferSp(new Int8Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
			spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(outputs))));

			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);

			if (cTypes[tyIdx].componentsCount == 4)
				spec.verifyIO = checkUniformsArray<deInt8, deInt32, 4>;
			else
				spec.verifyIO = checkUniformsArray<deInt8, deInt32, 1>;

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
		}
	}
}

void addCompute8bitStoragePushConstant8To32Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 64;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability StoragePushConstant8\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %PC8 Block\n"
		"OpDecorate %SSBO32 Block\n"
		"OpMemberDecorate %PC8 0 Offset 0\n"
		"OpMemberDecorate %SSBO32 0 Offset 0\n"
		"OpDecorate %ssbo32 DescriptorSet 0\n"
		"OpDecorate %ssbo32 Binding 0\n"

		"${matrix_decor:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"
		"%i32ptr    = OpTypePointer StorageBuffer %i32\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_8   = OpConstant %i32 8\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"

		"%i32arr    = OpTypeArray %i32 %c_i32_64\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%PC8      = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%pp_PC8   = OpTypePointer PushConstant %PC8\n"
		"%pc8      = OpVariable %pp_PC8 PushConstant\n"
		"%SSBO32    = OpTypeStruct %${matrix_prefix:opt}${base32}arr\n"
		"%up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
		"%ssbo32    = OpVariable %up_SSBO32 StorageBuffer\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base8}ptr %pc8 %zero %x ${index0:opt}\n"
		"%val8     = OpLoad %${base8} %inloc\n"
		"%val32     = ${convert} %${base32} %val8\n"
		"%outloc    = OpAccessChain %${base32}ptr %ssbo32 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val32\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{  // integers
		const char		sintTypes[]		=
			"%i8       = OpTypeInt 8 1\n"
			"%i8ptr    = OpTypePointer PushConstant %i8\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_64\n"
			"%v2i8     = OpTypeVector %i8 2\n"
			"%v2i32     = OpTypeVector %i32 2\n"
			"%v2i8ptr  = OpTypePointer PushConstant %v2i8\n"
			"%v2i32ptr  = OpTypePointer StorageBuffer %v2i32\n"
			"%v2i8arr  = OpTypeArray %v2i8 %c_i32_32\n"
			"%v2i32arr  = OpTypeArray %v2i32 %c_i32_32\n";

		const char		uintTypes[]		=
			"%u8       = OpTypeInt 8 0\n"
			"%u8ptr    = OpTypePointer PushConstant %u8\n"
			"%u32ptr    = OpTypePointer StorageBuffer %u32\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_64\n"
			"%u32arr    = OpTypeArray %u32 %c_i32_64\n"
			"%v2u8     = OpTypeVector %u8 2\n"
			"%v2u32     = OpTypeVector %u32 2\n"
			"%v2u8ptr  = OpTypePointer PushConstant %v2u8\n"
			"%v2u32ptr  = OpTypePointer StorageBuffer %v2u32\n"
			"%v2u8arr  = OpTypeArray %v2u8 %c_i32_32\n"
			"%v2u32arr  = OpTypeArray %v2u32 %c_i32_32\n";

		struct CompositeType
		{
			const char*	name;
			bool		isSigned;
			const char* types;
			const char*	base32;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			unsigned	count;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	true,	sintTypes,	"i32",		"i8",	"OpSConvert",	"OpDecorate %i32arr ArrayStride 4\nOpDecorate %i8arr ArrayStride 1\n",		numElements},
			{"scalar_uint",	false,	uintTypes,	"u32",		"u8",	"OpUConvert",	"OpDecorate %u32arr ArrayStride 4\nOpDecorate %u8arr ArrayStride 1\n",		numElements},
			{"vector_sint",	true,	sintTypes,	"v2i32",	"v2i8",	"OpSConvert",	"OpDecorate %v2i32arr ArrayStride 8\nOpDecorate %v2i8arr ArrayStride 2\n",	numElements / 2},
			{"vector_uint",	false,	uintTypes,	"v2u32",	"v2u8",	"OpUConvert",	"OpDecorate %v2u32arr ArrayStride 8\nOpDecorate %v2u8arr ArrayStride 2\n",	numElements / 2},
		};

		vector<deInt8>	inputs			= getInt8s(rnd, numElements);
		vector<deInt32> sOutputs;
		vector<deInt32> uOutputs;
		const deUint8	signBitMask		= 0x80;
		const deUint32	signExtendMask	= 0xffff0000;

		sOutputs.reserve(inputs.size());
		uOutputs.reserve(inputs.size());

		for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
		{
			uOutputs.push_back(static_cast<deUint8>(inputs[numNdx]));
			if (inputs[numNdx] & signBitMask)
				sOutputs.push_back(static_cast<deInt32>(inputs[numNdx] | signExtendMask));
			else
				sOutputs.push_back(static_cast<deInt32>(inputs[numNdx]));
		}

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			const char*				testName	= cTypes[tyIdx].name;

			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base32"]			= cTypes[tyIdx].base32;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(cTypes[tyIdx].count, 1, 1);
			spec.pushConstants		= BufferSp(new Int8Buffer(inputs));

			if (cTypes[tyIdx].isSigned)
				spec.outputs.push_back(BufferSp(new Int32Buffer(sOutputs)));
			else
				spec.outputs.push_back(BufferSp(new Int32Buffer(uOutputs)));
			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures.ext8BitStorage = EXT8BITSTORAGEFEATURES_PUSH_CONSTANT;

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName, testName, spec));
		}
	}
}

void addCompute8bitStorage16To8Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 128;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpCapability StorageUniform16\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_16bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %SSBO16 Block\n"
		"OpDecorate %SSBO8 Block\n"
		"OpMemberDecorate %SSBO16 0 Offset 0\n"
		"OpMemberDecorate %SSBO8 0 Offset 0\n"
		"OpDecorate %ssbo16 DescriptorSet 0\n"
		"OpDecorate %ssbo8 DescriptorSet 0\n"
		"OpDecorate %ssbo16 Binding 0\n"
		"OpDecorate %ssbo8 Binding 1\n"

		"${matrix_decor:opt}\n"

		"${rounding:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%i32       = OpTypeInt 32 1\n"
		"%u32       = OpTypeInt 32 0\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"
		"%c_i32_128 = OpConstant %i32 128\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%SSBO16    = OpTypeStruct %${matrix_prefix:opt}${base16}arr\n"
		"%SSBO8     = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%up_SSBO16 = OpTypePointer ${storage} %SSBO16\n"
		"%up_SSBO8  = OpTypePointer ${storage} %SSBO8\n"
		"%ssbo16    = OpVariable %up_SSBO16 ${storage}\n"
		"%ssbo8     = OpVariable %up_SSBO8 ${storage}\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base16}ptr %ssbo16 %zero %x ${index0:opt}\n"
		"%val16     = OpLoad %${base16} %inloc\n"
		"%val8      = ${convert} %${base8} %val16\n"
		"%outloc    = OpAccessChain %${base8}ptr %ssbo8 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val8\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{  // Integers
		const char		sintTypes[]	=
			"%i8       = OpTypeInt 8 1\n"
			"%i16      = OpTypeInt 16 1\n"
			"%i8ptr    = OpTypePointer StorageBuffer %i8\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_128\n"
			"%i16arr   = OpTypeArray %i16 %c_i32_128\n"
			"%v2i8     = OpTypeVector %i8 2\n"
			"%v2i16    = OpTypeVector %i16 2\n"
			"%v2i8ptr  = OpTypePointer StorageBuffer %v2i8\n"
			"%v2i16ptr = OpTypePointer StorageBuffer %v2i16\n"
			"%v2i8arr  = OpTypeArray %v2i8 %c_i32_64\n"
			"%v2i16arr = OpTypeArray %v2i16 %c_i32_64\n"
			"%i16ptr   = OpTypePointer StorageBuffer %i16\n";

		const char		uintTypes[]	=
			"%u8       = OpTypeInt 8 0\n"
			"%u16      = OpTypeInt 16 0\n"
			"%u8ptr    = OpTypePointer StorageBuffer %u8\n"
			"%u16ptr   = OpTypePointer StorageBuffer %u16\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_128\n"
			"%u16arr   = OpTypeArray %u16 %c_i32_128\n"
			"%v2u8     = OpTypeVector %u8 2\n"
			"%v2u16    = OpTypeVector %u16 2\n"
			"%v2u8ptr  = OpTypePointer StorageBuffer %v2u8\n"
			"%v2u16ptr = OpTypePointer StorageBuffer %v2u16\n"
			"%v2u8arr  = OpTypeArray %v2u8 %c_i32_64\n"
			"%v2u16arr = OpTypeArray %v2u16 %c_i32_64\n";

		struct CompositeType
		{
			const char*	name;
			const char* types;
			const char*	base16;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			unsigned	count;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	sintTypes,	"i16",		"i8",		"OpSConvert",	"OpDecorate %i16arr ArrayStride 2\nOpDecorate %i8arr ArrayStride 1\n",		numElements},
			{"scalar_uint",	uintTypes,	"u16",		"u8",		"OpUConvert",	"OpDecorate %u16arr ArrayStride 2\nOpDecorate %u8arr ArrayStride 1\n",		numElements},
			{"vector_sint",	sintTypes,	"v2i16",	"v2i8",		"OpSConvert",	"OpDecorate %v2i16arr ArrayStride 4\nOpDecorate %v2i8arr ArrayStride 2\n",	numElements / 2},
			{"vector_uint",	uintTypes,	"v2u16",	"v2u8",		"OpUConvert",	"OpDecorate %v2u16arr ArrayStride 4\nOpDecorate %v2u8arr ArrayStride 2\n",	numElements / 2},
		};

		vector<deInt16>	inputs			= getInt16s(rnd, numElements);
		vector<deInt8> outputs;

		outputs.reserve(inputs.size());
		for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
			outputs.push_back(static_cast<deInt8>(0xff & inputs[numNdx]));

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			string					testName	= string(CAPABILITIES[STORAGE_BUFFER_TEST].name) + "_" + cTypes[tyIdx].name;

			specs["capability"]		= CAPABILITIES[STORAGE_BUFFER_TEST].cap;
			specs["storage"]		= CAPABILITIES[STORAGE_BUFFER_TEST].decor;
			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base16"]			= cTypes[tyIdx].base16;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(cTypes[tyIdx].count, 1, 1);

			spec.inputs.push_back(Resource(BufferSp(new Int16Buffer(inputs)), CAPABILITIES[STORAGE_BUFFER_TEST].dtype));
			spec.outputs.push_back(Resource(BufferSp(new Int8Buffer(outputs))));
			spec.extensions.push_back("VK_KHR_16bit_storage");
			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[STORAGE_BUFFER_TEST].name);
			spec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM;

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
		}
	}
}

void addCompute8bitUniform8To16Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 128;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpCapability StorageUniform16\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_16bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %SSBO16 Block\n"
		"OpDecorate %SSBO8 Block\n"
		"OpMemberDecorate %SSBO16 0 Offset 0\n"
		"OpMemberDecorate %SSBO8 0 Offset 0\n"
		"OpDecorate %SSBO8 ${storage}\n"
		"OpDecorate %ssbo16 DescriptorSet 0\n"
		"OpDecorate %ssbo8 DescriptorSet 0\n"
		"OpDecorate %ssbo16 Binding 1\n"
		"OpDecorate %ssbo8 Binding 0\n"

		"${matrix_decor:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"

		"%i32       = OpTypeInt 32 1\n"
		"%u32       = OpTypeInt 32 0\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_2   = OpConstant %i32 2\n"
		"%c_i32_3   = OpConstant %i32 3\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"
		"%c_i32_128 = OpConstant %i32 128\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%SSBO16    = OpTypeStruct %${matrix_prefix:opt}${base16}arr\n"
		"%SSBO8    = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%up_SSBO16 = OpTypePointer StorageBuffer %SSBO16\n"
		"%up_SSBO8 = OpTypePointer Uniform %SSBO8\n"
		"%ssbo16    = OpVariable %up_SSBO16 StorageBuffer\n"
		"%ssbo8    = OpVariable %up_SSBO8 Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base8}ptr %ssbo8 %zero %x ${index0:opt}\n"
		"%val8     = OpLoad %${base8} %inloc\n"
		"%val16     = ${convert} %${base16} %val8\n"
		"%outloc    = OpAccessChain %${base16}ptr %ssbo16 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val16\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");


	{  // Integers
		const char		sintTypes[]		=
			"%i8       = OpTypeInt 8 1\n"
			"%i16      = OpTypeInt 16 1\n"
			"%i8ptr    = OpTypePointer Uniform %i8\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_128\n"
			"%i16arr    = OpTypeArray %i16 %c_i32_128\n"
			"%i16ptr   = OpTypePointer StorageBuffer %i16\n"
			"%v4i8     = OpTypeVector %i8 4\n"
			"%v4i16    = OpTypeVector %i16 4\n"
			"%v4i8ptr  = OpTypePointer Uniform %v4i8\n"
			"%v4i16ptr = OpTypePointer StorageBuffer %v4i16\n"
			"%v4i8arr  = OpTypeArray %v4i8 %c_i32_32\n"
			"%v4i16arr = OpTypeArray %v4i16 %c_i32_32\n";

		const char		uintTypes[]		=
			"%u8       = OpTypeInt 8 0\n"
			"%u16      = OpTypeInt 16 0\n"
			"%u8ptr    = OpTypePointer Uniform %u8\n"
			"%u16ptr   = OpTypePointer StorageBuffer %u16\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_128\n"
			"%u16arr   = OpTypeArray %u16 %c_i32_128\n"
			"%v4u8     = OpTypeVector %u8 4\n"
			"%v4u16    = OpTypeVector %u16 4\n"
			"%v4u8ptr  = OpTypePointer Uniform %v4u8\n"
			"%v4u16ptr = OpTypePointer StorageBuffer %v4u16\n"
			"%v4u8arr  = OpTypeArray %v4u8 %c_i32_32\n"
			"%v4u16arr = OpTypeArray %v4u16 %c_i32_32\n";

		struct CompositeType
		{
			const char*	name;
			const char* types;
			const char*	base16;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			const int	componentsCount;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	sintTypes,	"i16",		"i8",	"OpSConvert",	"OpDecorate %i16arr ArrayStride 2\nOpDecorate %i8arr ArrayStride 16\n",			1},
			{"scalar_uint",	uintTypes,	"u16",		"u8",	"OpUConvert",	"OpDecorate %u16arr ArrayStride 2\nOpDecorate %u8arr ArrayStride 16\n",			1},
			{"vector_sint",	sintTypes,	"v4i16",	"v4i8",	"OpSConvert",	"OpDecorate %v4i16arr ArrayStride 8\nOpDecorate %v4i8arr ArrayStride 16\n",	4},
			{"vector_uint",	uintTypes,	"v4u16",	"v4u8",	"OpUConvert",	"OpDecorate %v4u16arr ArrayStride 8\nOpDecorate %v4u8arr ArrayStride 16\n",	4},
		};

		vector<deInt16> outputs(numElements);

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			string					testName	= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + cTypes[tyIdx].name;

			vector<deInt8>			inputs		= getInt8s(rnd, (arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt8))) * (numElements / cTypes[tyIdx].componentsCount));

			specs["capability"]		= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
			specs["storage"]		= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].decor;
			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base16"]			= cTypes[tyIdx].base16;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(numElements / cTypes[tyIdx].componentsCount, 1, 1);

			spec.inputs.push_back(Resource(BufferSp(new Int8Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
			spec.outputs.push_back(Resource(BufferSp(new Int16Buffer(outputs))));
			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_16bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);
			spec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM;

			if (cTypes[tyIdx].componentsCount == 4)
				spec.verifyIO = checkUniformsArray<deInt8, deInt16, 4>;
			else
				spec.verifyIO = checkUniformsArray<deInt8, deInt16, 1>;

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
		}
	}
}

void addCompute8bitStoragePushConstant8To16Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 64;

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability StorageUniform16\n"
		"OpCapability StoragePushConstant8\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_16bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${stride}"

		"OpDecorate %PC8 Block\n"
		"OpDecorate %SSBO16 Block\n"
		"OpMemberDecorate %PC8 0 Offset 0\n"
		"OpMemberDecorate %SSBO16 0 Offset 0\n"
		"OpDecorate %ssbo16 DescriptorSet 0\n"
		"OpDecorate %ssbo16 Binding 0\n"

		"${matrix_decor:opt}\n"

		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%i32       = OpTypeInt 32 1\n"
		"%u32       = OpTypeInt 32 0\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"

		"%zero      = OpConstant %i32 0\n"
		"%c_i32_1   = OpConstant %i32 1\n"
		"%c_i32_8   = OpConstant %i32 8\n"
		"%c_i32_16  = OpConstant %i32 16\n"
		"%c_i32_32  = OpConstant %i32 32\n"
		"%c_i32_64  = OpConstant %i32 64\n"

		"${types}\n"
		"${matrix_types:opt}\n"

		"%PC8       = OpTypeStruct %${matrix_prefix:opt}${base8}arr\n"
		"%pp_PC8    = OpTypePointer PushConstant %PC8\n"
		"%pc8       = OpVariable %pp_PC8 PushConstant\n"
		"%SSBO16    = OpTypeStruct %${matrix_prefix:opt}${base16}arr\n"
		"%up_SSBO16 = OpTypePointer StorageBuffer %SSBO16\n"
		"%ssbo16    = OpVariable %up_SSBO16 StorageBuffer\n"

		"%id        = OpVariable %uvec3ptr Input\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %${base8}ptr %pc8 %zero %x ${index0:opt}\n"
		"%val8      = OpLoad %${base8} %inloc\n"
		"%val16     = ${convert} %${base16} %val8\n"
		"%outloc    = OpAccessChain %${base16}ptr %ssbo16 %zero %x ${index0:opt}\n"
		"             OpStore %outloc %val16\n"
		"${matrix_store:opt}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{  // integers
		const char		sintTypes[]		=
			"%i8       = OpTypeInt 8 1\n"
			"%i16      = OpTypeInt 16 1\n"
			"%i8ptr    = OpTypePointer PushConstant %i8\n"
			"%i16ptr   = OpTypePointer StorageBuffer %i16\n"
			"%i8arr    = OpTypeArray %i8 %c_i32_64\n"
			"%i16arr   = OpTypeArray %i16 %c_i32_64\n"
			"%v2i8     = OpTypeVector %i8 2\n"
			"%v2i16    = OpTypeVector %i16 2\n"
			"%v2i8ptr  = OpTypePointer PushConstant %v2i8\n"
			"%v2i16ptr = OpTypePointer StorageBuffer %v2i16\n"
			"%v2i8arr  = OpTypeArray %v2i8 %c_i32_32\n"
			"%v2i16arr = OpTypeArray %v2i16 %c_i32_32\n";

		const char		uintTypes[]		=
			"%u8       = OpTypeInt 8 0\n"
			"%u16      = OpTypeInt 16 0\n"
			"%u8ptr    = OpTypePointer PushConstant %u8\n"
			"%u16ptr   = OpTypePointer StorageBuffer %u16\n"
			"%u8arr    = OpTypeArray %u8 %c_i32_64\n"
			"%u16arr   = OpTypeArray %u16 %c_i32_64\n"
			"%v2u8     = OpTypeVector %u8 2\n"
			"%v2u16    = OpTypeVector %u16 2\n"
			"%v2u8ptr  = OpTypePointer PushConstant %v2u8\n"
			"%v2u16ptr = OpTypePointer StorageBuffer %v2u16\n"
			"%v2u8arr  = OpTypeArray %v2u8 %c_i32_32\n"
			"%v2u16arr = OpTypeArray %v2u16 %c_i32_32\n";

		struct CompositeType
		{
			const char*	name;
			bool		isSigned;
			const char* types;
			const char*	base16;
			const char*	base8;
			const char* opcode;
			const char*	stride;
			unsigned	count;
		};

		const CompositeType	cTypes[]	=
		{
			{"scalar_sint",	true,	sintTypes,	"i16",		"i8",		"OpSConvert",	"OpDecorate %i16arr ArrayStride 2\nOpDecorate %i8arr ArrayStride 1\n",		numElements},
			{"scalar_uint",	false,	uintTypes,	"u16",		"u8",		"OpUConvert",	"OpDecorate %u16arr ArrayStride 2\nOpDecorate %u8arr ArrayStride 1\n",		numElements},
			{"vector_sint",	true,	sintTypes,	"v2i16",	"v2i8",		"OpSConvert",	"OpDecorate %v2i16arr ArrayStride 4\nOpDecorate %v2i8arr ArrayStride 2\n",	numElements / 2},
			{"vector_uint",	false,	uintTypes,	"v2u16",	"v2u8",		"OpUConvert",	"OpDecorate %v2u16arr ArrayStride 4\nOpDecorate %v2u8arr ArrayStride 2\n",	numElements / 2},
		};

		vector<deInt8>	inputs			= getInt8s(rnd, numElements);
		vector<deInt16> sOutputs;
		vector<deInt16> uOutputs;
		const deUint8	signBitMask		= 0x80;
		const deUint16	signExtendMask	= 0xff00;

		sOutputs.reserve(inputs.size());
		uOutputs.reserve(inputs.size());

		for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
		{
			uOutputs.push_back(static_cast<deUint8>(inputs[numNdx]));
			if (inputs[numNdx] & signBitMask)
				sOutputs.push_back(static_cast<deInt16>(inputs[numNdx] | signExtendMask));
			else
				sOutputs.push_back(static_cast<deInt16>(inputs[numNdx]));
		}

		for (deUint32 tyIdx = 0; tyIdx < DE_LENGTH_OF_ARRAY(cTypes); ++tyIdx)
		{
			ComputeShaderSpec		spec;
			map<string, string>		specs;
			const char*				testName	= cTypes[tyIdx].name;

			specs["stride"]			= cTypes[tyIdx].stride;
			specs["base16"]			= cTypes[tyIdx].base16;
			specs["base8"]			= cTypes[tyIdx].base8;
			specs["types"]			= cTypes[tyIdx].types;
			specs["convert"]		= cTypes[tyIdx].opcode;

			spec.assembly			= shaderTemplate.specialize(specs);
			spec.numWorkGroups		= IVec3(cTypes[tyIdx].count, 1, 1);
			spec.pushConstants		= BufferSp(new Int8Buffer(inputs));

			if (cTypes[tyIdx].isSigned)
				spec.outputs.push_back(BufferSp(new Int16Buffer(sOutputs)));
			else
				spec.outputs.push_back(BufferSp(new Int16Buffer(uOutputs)));
			spec.extensions.push_back("VK_KHR_8bit_storage");
			spec.extensions.push_back("VK_KHR_16bit_storage");
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.requestedVulkanFeatures.ext8BitStorage = EXT8BITSTORAGEFEATURES_PUSH_CONSTANT;
			spec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM;

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName, testName, spec));
		}
	}
}

void addCompute8bitStorageBuffer8To8Group (tcu::TestCaseGroup* group)
{
	tcu::TestContext&		testCtx				= group->getTestContext();
	de::Random				rnd					(deStringHash(group->getName()));
	const int				numElements			= 128;
	const vector<deInt8>	int8Data			= getInt8s(rnd, numElements);
	const vector<deInt8>	int8UnusedData		(numElements, 0);
	ComputeShaderSpec		spec;
	std::ostringstream		shaderTemplate;
		shaderTemplate<<"OpCapability Shader\n"
			<< "OpCapability StorageBuffer8BitAccess \n"
			<< "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
			<< "OpExtension \"SPV_KHR_8bit_storage\"\n"
			<< "OpMemoryModel Logical GLSL450\n"
			<< "OpEntryPoint GLCompute %main \"main\" %id\n"
			<< "OpExecutionMode %main LocalSize 1 1 1\n"
			<< "OpDecorate %id BuiltIn GlobalInvocationId\n"
			<< "OpDecorate %i8arr ArrayStride 1\n"
			<< "OpDecorate %SSBO_IN Block\n"
			<< "OpDecorate %SSBO_OUT Block\n"
			<< "OpMemberDecorate %SSBO_IN 0 Coherent\n"
			<< "OpMemberDecorate %SSBO_OUT 0 Coherent\n"
			<< "OpMemberDecorate %SSBO_IN 0 Offset 0\n"
			<< "OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
			<< "OpDecorate %ssboIN DescriptorSet 0\n"
			<< "OpDecorate %ssboOUT DescriptorSet 0\n"
			<< "OpDecorate %ssboIN Binding 0\n"
			<< "OpDecorate %ssboOUT Binding 1\n"
			<< "\n"
			<< "%bool      = OpTypeBool\n"
			<< "%void      = OpTypeVoid\n"
			<< "%voidf     = OpTypeFunction %void\n"
			<< "%u32       = OpTypeInt 32 0\n"
			<< "%i32       = OpTypeInt 32 1\n"
			<< "%uvec3     = OpTypeVector %u32 3\n"
			<< "%uvec3ptr  = OpTypePointer Input %uvec3\n"
			<< "%i8        = OpTypeInt 8 1\n"
			<< "%i8ptr     = OpTypePointer StorageBuffer %i8\n"
			<< "\n"
			<< "%zero      = OpConstant %i32 0\n"
			<< "%c_size    = OpConstant %i32 " << numElements << "\n"
			<< "\n"
			<< "%i8arr     = OpTypeArray %i8 %c_size\n"
			<< "%SSBO_IN   = OpTypeStruct %i8arr\n"
			<< "%SSBO_OUT  = OpTypeStruct %i8arr\n"
			<< "%up_SSBOIN = OpTypePointer StorageBuffer %SSBO_IN\n"
			<< "%up_SSBOOUT = OpTypePointer StorageBuffer %SSBO_OUT\n"
			<< "%ssboIN    = OpVariable %up_SSBOIN StorageBuffer\n"
			<< "%ssboOUT   = OpVariable %up_SSBOOUT StorageBuffer\n"
			<< "\n"
			<< "%id        = OpVariable %uvec3ptr Input\n"
			<< "%main      = OpFunction %void None %voidf\n"
			<< "%label     = OpLabel\n"
			<< "%idval     = OpLoad %uvec3 %id\n"
			<< "%x         = OpCompositeExtract %u32 %idval 0\n"
			<< "%y         = OpCompositeExtract %u32 %idval 1\n"
			<< "\n"
			<< "%inlocx     = OpAccessChain %i8ptr %ssboIN %zero %x \n"
			<< "%valx       = OpLoad %i8 %inlocx\n"
			<< "%outlocx    = OpAccessChain %i8ptr %ssboOUT %zero %x \n"
			<< "             OpStore %outlocx %valx\n"

			<< "%inlocy    = OpAccessChain %i8ptr %ssboIN %zero %y \n"
			<< "%valy      = OpLoad %i8 %inlocy\n"
			<< "%outlocy   = OpAccessChain %i8ptr %ssboOUT %zero %y \n"
			<< "             OpStore %outlocy %valy\n"
			<< "\n"
			<< "             OpReturn\n"
			<< "             OpFunctionEnd\n";

	spec.assembly			= shaderTemplate.str();
	spec.numWorkGroups		= IVec3(numElements, numElements, 1);
	spec.verifyIO			= computeCheckBuffers;
	spec.coherentMemory		= true;
	spec.inputs.push_back(BufferSp(new Int8Buffer(int8Data)));
	spec.outputs.push_back(BufferSp(new Int8Buffer(int8UnusedData)));
	spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
	spec.extensions.push_back("VK_KHR_8bit_storage");
	spec.requestedVulkanFeatures.ext8BitStorage = EXT8BITSTORAGEFEATURES_STORAGE_BUFFER;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "stress_test", "Granularity stress test", spec));
}

void addCompute8bitStorageUniform8StructTo32StructGroup (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));
	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"\n"
		"${stridei8}"
		"\n"
		"${stridei32}"
		"\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %SSBO_IN Block\n"
		"OpDecorate %SSBO_OUT Block\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"OpDecorate %ssboOUT Binding 1\n"
		"\n"
		"%bool     = OpTypeBool\n"
		"%void     = OpTypeVoid\n"
		"%voidf    = OpTypeFunction %void\n"
		"%u32      = OpTypeInt 32 0\n"
		"%uvec3    = OpTypeVector %u32 3\n"
		"%uvec3ptr = OpTypePointer Input %uvec3\n"
		"\n"
		"%i32      = OpTypeInt 32 1\n"
		"%v2i32    = OpTypeVector %i32 2\n"
		"%v3i32    = OpTypeVector %i32 3\n"
		"%v4i32    = OpTypeVector %i32 4\n"
		"\n"
		"%i8       = OpTypeInt 8 1\n"
		"%v2i8     = OpTypeVector %i8 2\n"
		"%v3i8     = OpTypeVector %i8 3\n"
		"%v4i8     = OpTypeVector %i8 4\n"
		"%i8ptr    = OpTypePointer ${8Storage} %i8\n"
		"%v2i8ptr  = OpTypePointer ${8Storage} %v2i8\n"
		"%v3i8ptr  = OpTypePointer ${8Storage} %v3i8\n"
		"%v4i8ptr  = OpTypePointer ${8Storage} %v4i8\n"
		"\n"
		"%i32ptr   = OpTypePointer ${32Storage} %i32\n"
		"%v2i32ptr = OpTypePointer ${32Storage} %v2i32\n"
		"%v3i32ptr = OpTypePointer ${32Storage} %v3i32\n"
		"%v4i32ptr = OpTypePointer ${32Storage} %v4i32\n"
		"\n"
		"%zero = OpConstant %i32 0\n"
		"%c_i32_1 = OpConstant %i32 1\n"
		"%c_i32_2 = OpConstant %i32 2\n"
		"%c_i32_3 = OpConstant %i32 3\n"
		"%c_i32_4 = OpConstant %i32 4\n"
		"%c_i32_5 = OpConstant %i32 5\n"
		"%c_i32_6 = OpConstant %i32 6\n"
		"%c_i32_7 = OpConstant %i32 7\n"
		"%c_i32_8 = OpConstant %i32 8\n"
		"%c_i32_9 = OpConstant %i32 9\n"
		"\n"
		"%c_u32_1 = OpConstant %u32 1\n"
		"%c_u32_3 = OpConstant %u32 3\n"
		"%c_u32_7 = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"
		"%i8arr3       = OpTypeArray %i8 %c_u32_3\n"
		"%v2i8arr3     = OpTypeArray %v2i8 %c_u32_3\n"
		"%v2i8arr11    = OpTypeArray %v2i8 %c_u32_11\n"
		"%v3i8arr11    = OpTypeArray %v3i8 %c_u32_11\n"
		"%v4i8arr3     = OpTypeArray %v4i8 %c_u32_3\n"
		"%struct8      = OpTypeStruct %i8 %v2i8arr3\n"
		"%struct8arr11 = OpTypeArray %struct8 %c_u32_11\n"
		"%i8Struct = OpTypeStruct %i8 %v2i8 %v3i8 %v4i8 %i8arr3 %struct8arr11 %v2i8arr11 %i8 %v3i8arr11 %v4i8arr3\n"
		"\n"
		"%i32arr3       = OpTypeArray %i32 %c_u32_3\n"
		"%v2i32arr3     = OpTypeArray %v2i32 %c_u32_3\n"
		"%v2i32arr11    = OpTypeArray %v2i32 %c_u32_11\n"
		"%v3i32arr11    = OpTypeArray %v3i32 %c_u32_11\n"
		"%v4i32arr3     = OpTypeArray %v4i32 %c_u32_3\n"
		"%struct32      = OpTypeStruct %i32 %v2i32arr3\n"
		"%struct32arr11 = OpTypeArray %struct32 %c_u32_11\n"
		"%i32Struct = OpTypeStruct %i32 %v2i32 %v3i32 %v4i32 %i32arr3 %struct32arr11 %v2i32arr11 %i32 %v3i32arr11 %v4i32arr3\n"
		"\n"
		"%i8StructArr7  = OpTypeArray %i8Struct %c_u32_7\n"
		"%i32StructArr7 = OpTypeArray %i32Struct %c_u32_7\n"
		"%SSBO_IN       = OpTypeStruct %i8StructArr7\n"
		"%SSBO_OUT      = OpTypeStruct %i32StructArr7\n"
		"%up_SSBOIN     = OpTypePointer Uniform %SSBO_IN\n"
		"%up_SSBOOUT    = OpTypePointer StorageBuffer %SSBO_OUT\n"
		"%ssboIN        = OpVariable %up_SSBOIN Uniform\n"
		"%ssboOUT       = OpVariable %up_SSBOOUT StorageBuffer\n"
		"\n"
		"%id        = OpVariable %uvec3ptr Input\n"
		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%y         = OpCompositeExtract %u32 %idval 1\n"
		"\n"
		"%i8src  = OpAccessChain %i8ptr %ssboIN %zero %x %zero\n"
		"%val_i8 = OpLoad %i8 %i8src\n"
		"%val_i32 = OpSConvert %i32 %val_i8\n"
		"%i32dst  = OpAccessChain %i32ptr %ssboOUT %zero %x %zero\n"
		"OpStore %i32dst %val_i32\n"
		"\n"
		"%v2i8src  = OpAccessChain %v2i8ptr %ssboIN %zero %x %c_i32_1\n"
		"%val_v2i8 = OpLoad %v2i8 %v2i8src\n"
		"%val_v2i32 = OpSConvert %v2i32 %val_v2i8\n"
		"%v2i32dst  = OpAccessChain %v2i32ptr %ssboOUT %zero %x %c_i32_1\n"
		"OpStore %v2i32dst %val_v2i32\n"
		"\n"
		"%v3i8src  = OpAccessChain %v3i8ptr %ssboIN %zero %x %c_i32_2\n"
		"%val_v3i8 = OpLoad %v3i8 %v3i8src\n"
		"%val_v3i32 = OpSConvert %v3i32 %val_v3i8\n"
		"%v3i32dst  = OpAccessChain %v3i32ptr %ssboOUT %zero %x %c_i32_2\n"
		"OpStore %v3i32dst %val_v3i32\n"
		"\n"
		"%v4i8src  = OpAccessChain %v4i8ptr %ssboIN %zero %x %c_i32_3\n"
		"%val_v4i8 = OpLoad %v4i8 %v4i8src\n"
		"%val_v4i32 = OpSConvert %v4i32 %val_v4i8\n"
		"%v4i32dst  = OpAccessChain %v4i32ptr %ssboOUT %zero %x %c_i32_3\n"
		"OpStore %v4i32dst %val_v4i32\n"
		"\n"
		//struct {i8, v2i8[3]}
		"%Si8src  = OpAccessChain %i8ptr %ssboIN %zero %x %c_i32_5 %y %zero\n"
		"%Sval_i8 = OpLoad %i8 %Si8src\n"
		"%Sval_i32 = OpSConvert %i32 %Sval_i8\n"
		"%Si32dst2  = OpAccessChain %i32ptr %ssboOUT %zero %x %c_i32_5 %y %zero\n"
		"OpStore %Si32dst2 %Sval_i32\n"
		"\n"
		"%Sv2i8src0   = OpAccessChain %v2i8ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %zero\n"
		"%Sv2i8_0     = OpLoad %v2i8 %Sv2i8src0\n"
		"%Sv2i32_0     = OpSConvert %v2i32 %Sv2i8_0\n"
		"%Sv2i32dst_0  = OpAccessChain %v2i32ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %zero\n"
		"OpStore %Sv2i32dst_0 %Sv2i32_0\n"
		"\n"
		"%Sv2i8src1  = OpAccessChain %v2i8ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %c_i32_1\n"
		"%Sv2i8_1 = OpLoad %v2i8 %Sv2i8src1\n"
		"%Sv2i32_1 = OpSConvert %v2i32 %Sv2i8_1\n"
		"%Sv2i32dst_1  = OpAccessChain %v2i32ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %c_i32_1\n"
		"OpStore %Sv2i32dst_1 %Sv2i32_1\n"
		"\n"
		"%Sv2i8src2  = OpAccessChain %v2i8ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %c_i32_2\n"
		"%Sv2i8_2 = OpLoad %v2i8 %Sv2i8src2\n"
		"%Sv2i32_2 = OpSConvert %v2i32 %Sv2i8_2\n"
		"%Sv2i32dst_2  = OpAccessChain %v2i32ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %c_i32_2\n"
		"OpStore %Sv2i32dst_2 %Sv2i32_2\n"
		"\n"
		"%v2i8src2  = OpAccessChain %v2i8ptr %ssboIN %zero %x %c_i32_6 %y\n"
		"%val2_v2i8 = OpLoad %v2i8 %v2i8src2\n"
		"%val2_v2i32 = OpSConvert %v2i32 %val2_v2i8\n"
		"%v2i32dst2  = OpAccessChain %v2i32ptr %ssboOUT %zero %x %c_i32_6 %y\n"
		"OpStore %v2i32dst2 %val2_v2i32\n"
		"\n"
		"%i8src2  = OpAccessChain %i8ptr %ssboIN %zero %x %c_i32_7\n"
		"%val2_i8 = OpLoad %i8 %i8src2\n"
		"%val2_i32 = OpSConvert %i32 %val2_i8\n"
		"%i32dst2  = OpAccessChain %i32ptr %ssboOUT %zero %x %c_i32_7\n"
		"OpStore %i32dst2 %val2_i32\n"
		"\n"
		"%v3i8src2  = OpAccessChain %v3i8ptr %ssboIN %zero %x %c_i32_8 %y\n"
		"%val2_v3i8 = OpLoad %v3i8 %v3i8src2\n"
		"%val2_v3i32 = OpSConvert %v3i32 %val2_v3i8\n"
		"%v3i32dst2  = OpAccessChain %v3i32ptr %ssboOUT %zero %x %c_i32_8 %y\n"
		"OpStore %v3i32dst2 %val2_v3i32\n"
		"\n"
		//Array with 3 elements
		"%LessThan3 = OpSLessThan %bool %y %c_i32_3\n"
		"OpSelectionMerge %BlockIf None\n"
		"OpBranchConditional %LessThan3 %LabelIf %BlockIf\n"
		"%LabelIf = OpLabel\n"
		"  %i8src3  = OpAccessChain %i8ptr %ssboIN %zero %x %c_i32_4 %y\n"
		"  %val3_i8 = OpLoad %i8 %i8src3\n"
		"  %val3_i32 = OpSConvert %i32 %val3_i8\n"
		"  %i32dst3  = OpAccessChain %i32ptr %ssboOUT %zero %x %c_i32_4 %y\n"
		"  OpStore %i32dst3 %val3_i32\n"
		"\n"
		"  %v4i8src2  = OpAccessChain %v4i8ptr %ssboIN %zero %x %c_i32_9 %y\n"
		"  %val2_v4i8 = OpLoad %v4i8 %v4i8src2\n"
		"  %val2_v4i32 = OpSConvert %v4i32 %val2_v4i8\n"
		"  %v4i32dst2  = OpAccessChain %v4i32ptr %ssboOUT %zero %x %c_i32_9 %y\n"
		"  OpStore %v4i32dst2 %val2_v4i32\n"
		"OpBranch %BlockIf\n"
		"%BlockIf = OpLabel\n"

		"   OpReturn\n"
		"   OpFunctionEnd\n");

	{  // int
		vector<deInt32>			int32Data	= data32bit(SHADERTEMPLATE_STRIDE32BIT_STD430, rnd, false);

		vector<deInt8>			in8DData	= data8bit(SHADERTEMPLATE_STRIDE8BIT_STD140, rnd);
		ComputeShaderSpec		spec;
		map<string, string>		specs;
		string					testName	= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);

		specs["capability"]		= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
		specs["stridei8"]		= getStructShaderComponet(SHADERTEMPLATE_STRIDE8BIT_STD140);
		specs["stridei32"]		= getStructShaderComponet(SHADERTEMPLATE_STRIDE32BIT_STD430);
		specs["32Storage"]		= "StorageBuffer";
		specs["8Storage"]		= "Uniform";

		spec.assembly			= shaderTemplate.specialize(specs);
		spec.numWorkGroups		= IVec3(structData.structArraySize, structData.nestedArraySize, 1);
		spec.verifyIO			= checkStruct<deInt8, deInt32, SHADERTEMPLATE_STRIDE8BIT_STD140, SHADERTEMPLATE_STRIDE32BIT_STD430>;
		spec.inputs.push_back(Resource(BufferSp(new Int8Buffer(in8DData)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(int32Data))));
		spec.extensions.push_back("VK_KHR_8bit_storage");
		spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
	}
}

void addCompute8bitStorageUniform32StructTo8StructGroup (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx			= group->getTestContext();
	de::Random						rnd				(deStringHash(group->getName()));

	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability ${capability}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"\n"
		"${stridei8}"
		"\n"
		"${stridei32}"
		"\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %SSBO_IN Block\n"
		"OpDecorate %SSBO_OUT Block\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"OpDecorate %ssboOUT Binding 1\n"
		"\n"
		"%bool     = OpTypeBool\n"
		"%void     = OpTypeVoid\n"
		"%voidf    = OpTypeFunction %void\n"
		"%u32      = OpTypeInt 32 0\n"
		"%uvec3    = OpTypeVector %u32 3\n"
		"%uvec3ptr = OpTypePointer Input %uvec3\n"
		"\n"
		"%i32      = OpTypeInt 32 1\n"
		"%v2i32    = OpTypeVector %i32 2\n"
		"%v3i32    = OpTypeVector %i32 3\n"
		"%v4i32    = OpTypeVector %i32 4\n"
		"\n"
		"%i8       = OpTypeInt 8 1\n"
		"%v2i8     = OpTypeVector %i8 2\n"
		"%v3i8     = OpTypeVector %i8 3\n"
		"%v4i8     = OpTypeVector %i8 4\n"
		"%i8ptr    = OpTypePointer ${8Storage} %i8\n"
		"%v2i8ptr  = OpTypePointer ${8Storage} %v2i8\n"
		"%v3i8ptr  = OpTypePointer ${8Storage} %v3i8\n"
		"%v4i8ptr  = OpTypePointer ${8Storage} %v4i8\n"
		"\n"
		"%i32ptr   = OpTypePointer ${32Storage} %i32\n"
		"%v2i32ptr = OpTypePointer ${32Storage} %v2i32\n"
		"%v3i32ptr = OpTypePointer ${32Storage} %v3i32\n"
		"%v4i32ptr = OpTypePointer ${32Storage} %v4i32\n"
		"\n"
		"%zero = OpConstant %i32 0\n"
		"%c_i32_1 = OpConstant %i32 1\n"
		"%c_i32_2 = OpConstant %i32 2\n"
		"%c_i32_3 = OpConstant %i32 3\n"
		"%c_i32_4 = OpConstant %i32 4\n"
		"%c_i32_5 = OpConstant %i32 5\n"
		"%c_i32_6 = OpConstant %i32 6\n"
		"%c_i32_7 = OpConstant %i32 7\n"
		"%c_i32_8 = OpConstant %i32 8\n"
		"%c_i32_9 = OpConstant %i32 9\n"
		"\n"
		"%c_u32_1 = OpConstant %u32 1\n"
		"%c_u32_3 = OpConstant %u32 3\n"
		"%c_u32_7 = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"
		"%i8arr3      = OpTypeArray %i8 %c_u32_3\n"
		"%v2i8arr3     = OpTypeArray %v2i8 %c_u32_3\n"
		"%v2i8arr11    = OpTypeArray %v2i8 %c_u32_11\n"
		"%v3i8arr11    = OpTypeArray %v3i8 %c_u32_11\n"
		"%v4i8arr3     = OpTypeArray %v4i8 %c_u32_3\n"
		"%struct8       = OpTypeStruct %i8 %v2i8arr3\n"
		"%struct8arr11 = OpTypeArray %struct8 %c_u32_11\n"
		"%i8Struct = OpTypeStruct %i8 %v2i8 %v3i8 %v4i8 %i8arr3 %struct8arr11 %v2i8arr11 %i8 %v3i8arr11 %v4i8arr3\n"
		"\n"
		"%i32arr3       = OpTypeArray %i32 %c_u32_3\n"
		"%v2i32arr3     = OpTypeArray %v2i32 %c_u32_3\n"
		"%v2i32arr11    = OpTypeArray %v2i32 %c_u32_11\n"
		"%v3i32arr11    = OpTypeArray %v3i32 %c_u32_11\n"
		"%v4i32arr3     = OpTypeArray %v4i32 %c_u32_3\n"
		"%struct32      = OpTypeStruct %i32 %v2i32arr3\n"
		"%struct32arr11 = OpTypeArray %struct32 %c_u32_11\n"
		"%i32Struct = OpTypeStruct %i32 %v2i32 %v3i32 %v4i32 %i32arr3 %struct32arr11 %v2i32arr11 %i32 %v3i32arr11 %v4i32arr3\n"
		"\n"
		"%i8StructArr7  = OpTypeArray %i8Struct %c_u32_7\n"
		"%i32StructArr7 = OpTypeArray %i32Struct %c_u32_7\n"
		"%SSBO_IN       = OpTypeStruct %i32StructArr7\n"
		"%SSBO_OUT      = OpTypeStruct %i8StructArr7\n"
		"%up_SSBOIN     = OpTypePointer Uniform %SSBO_IN\n"
		"%up_SSBOOUT    = OpTypePointer ${storage} %SSBO_OUT\n"
		"%ssboIN        = OpVariable %up_SSBOIN Uniform\n"
		"%ssboOUT       = OpVariable %up_SSBOOUT ${storage}\n"
		"\n"
		"%id        = OpVariable %uvec3ptr Input\n"
		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%y         = OpCompositeExtract %u32 %idval 1\n"
		"\n"
		"%i32src  = OpAccessChain %i32ptr %ssboIN %zero %x %zero\n"
		"%val_i32 = OpLoad %i32 %i32src\n"
		"%val_i8 = OpSConvert %i8 %val_i32\n"
		"%i8dst  = OpAccessChain %i8ptr %ssboOUT %zero %x %zero\n"
		"OpStore %i8dst %val_i8\n"
		"\n"
		"%v2i32src  = OpAccessChain %v2i32ptr %ssboIN %zero %x %c_i32_1\n"
		"%val_v2i32 = OpLoad %v2i32 %v2i32src\n"
		"%val_v2i8 = OpSConvert %v2i8 %val_v2i32\n"
		"%v2i8dst  = OpAccessChain %v2i8ptr %ssboOUT %zero %x %c_i32_1\n"
		"OpStore %v2i8dst %val_v2i8\n"
		"\n"
		"%v3i32src  = OpAccessChain %v3i32ptr %ssboIN %zero %x %c_i32_2\n"
		"%val_v3i32 = OpLoad %v3i32 %v3i32src\n"
		"%val_v3i8 = OpSConvert %v3i8 %val_v3i32\n"
		"%v3i8dst  = OpAccessChain %v3i8ptr %ssboOUT %zero %x %c_i32_2\n"
		"OpStore %v3i8dst %val_v3i8\n"
		"\n"
		"%v4i32src  = OpAccessChain %v4i32ptr %ssboIN %zero %x %c_i32_3\n"
		"%val_v4i32 = OpLoad %v4i32 %v4i32src\n"
		"%val_v4i8 = OpSConvert %v4i8 %val_v4i32\n"
		"%v4i8dst  = OpAccessChain %v4i8ptr %ssboOUT %zero %x %c_i32_3\n"
		"OpStore %v4i8dst %val_v4i8\n"
		"\n"

		//struct {i8, v2i8[3]}
		"%Si32src  = OpAccessChain %i32ptr %ssboIN %zero %x %c_i32_5 %y %zero\n"
		"%Sval_i32 = OpLoad %i32 %Si32src\n"
		"%Sval_i8 = OpSConvert %i8 %Sval_i32\n"
		"%Si8dst2  = OpAccessChain %i8ptr %ssboOUT %zero %x %c_i32_5 %y %zero\n"
		"OpStore %Si8dst2 %Sval_i8\n"
		"\n"
		"%Sv2i32src0   = OpAccessChain %v2i32ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %zero\n"
		"%Sv2i32_0     = OpLoad %v2i32 %Sv2i32src0\n"
		"%Sv2i8_0     = OpSConvert %v2i8 %Sv2i32_0\n"
		"%Sv2i8dst_0  = OpAccessChain %v2i8ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %zero\n"
		"OpStore %Sv2i8dst_0 %Sv2i8_0\n"
		"\n"
		"%Sv2i32src1  = OpAccessChain %v2i32ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %c_i32_1\n"
		"%Sv2i32_1 = OpLoad %v2i32 %Sv2i32src1\n"
		"%Sv2i8_1 = OpSConvert %v2i8 %Sv2i32_1\n"
		"%Sv2i8dst_1  = OpAccessChain %v2i8ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %c_i32_1\n"
		"OpStore %Sv2i8dst_1 %Sv2i8_1\n"
		"\n"
		"%Sv2i32src2  = OpAccessChain %v2i32ptr %ssboIN %zero %x %c_i32_5 %y %c_i32_1 %c_i32_2\n"
		"%Sv2i32_2 = OpLoad %v2i32 %Sv2i32src2\n"
		"%Sv2i8_2 = OpSConvert %v2i8 %Sv2i32_2\n"
		"%Sv2i8dst_2  = OpAccessChain %v2i8ptr %ssboOUT %zero %x %c_i32_5 %y %c_i32_1 %c_i32_2\n"
		"OpStore %Sv2i8dst_2 %Sv2i8_2\n"
		"\n"

		"%v2i32src2  = OpAccessChain %v2i32ptr %ssboIN %zero %x %c_i32_6 %y\n"
		"%val2_v2i32 = OpLoad %v2i32 %v2i32src2\n"
		"%val2_v2i8 = OpSConvert %v2i8 %val2_v2i32\n"
		"%v2i8dst2  = OpAccessChain %v2i8ptr %ssboOUT %zero %x %c_i32_6 %y\n"
		"OpStore %v2i8dst2 %val2_v2i8\n"
		"\n"
		"%i32src2  = OpAccessChain %i32ptr %ssboIN %zero %x %c_i32_7\n"
		"%val2_i32 = OpLoad %i32 %i32src2\n"
		"%val2_i8 = OpSConvert %i8 %val2_i32\n"
		"%i8dst2  = OpAccessChain %i8ptr %ssboOUT %zero %x %c_i32_7\n"
		"OpStore %i8dst2 %val2_i8\n"
		"\n"
		"%v3i32src2  = OpAccessChain %v3i32ptr %ssboIN %zero %x %c_i32_8 %y\n"
		"%val2_v3i32 = OpLoad %v3i32 %v3i32src2\n"
		"%val2_v3i8 = OpSConvert %v3i8 %val2_v3i32\n"
		"%v3i8dst2  = OpAccessChain %v3i8ptr %ssboOUT %zero %x %c_i32_8 %y\n"
		"OpStore %v3i8dst2 %val2_v3i8\n"
		"\n"

		//Array with 3 elements
		"%LessThan3 = OpSLessThan %bool %y %c_i32_3\n"
		"OpSelectionMerge %BlockIf None\n"
		"OpBranchConditional %LessThan3 %LabelIf %BlockIf\n"
		"  %LabelIf = OpLabel\n"
		"  %i32src3  = OpAccessChain %i32ptr %ssboIN %zero %x %c_i32_4 %y\n"
		"  %val3_i32 = OpLoad %i32 %i32src3\n"
		"  %val3_i8 = OpSConvert %i8 %val3_i32\n"
		"  %i8dst3  = OpAccessChain %i8ptr %ssboOUT %zero %x %c_i32_4 %y\n"
		"  OpStore %i8dst3 %val3_i8\n"
		"\n"
		"  %v4i32src2  = OpAccessChain %v4i32ptr %ssboIN %zero %x %c_i32_9 %y\n"
		"  %val2_v4i32 = OpLoad %v4i32 %v4i32src2\n"
		"  %val2_v4i8 = OpSConvert %v4i8 %val2_v4i32\n"
		"  %v4i8dst2  = OpAccessChain %v4i8ptr %ssboOUT %zero %x %c_i32_9 %y\n"
		"  OpStore %v4i8dst2 %val2_v4i8\n"
		"OpBranch %BlockIf\n"
		"%BlockIf = OpLabel\n"

		"   OpReturn\n"
		"   OpFunctionEnd\n");

	{  // Int
		vector<deInt8>		int8Data		= data8bit(SHADERTEMPLATE_STRIDE8BIT_STD430, rnd, false);

		ComputeShaderSpec		spec;
		map<string, string>		specs;
		string					testName	= string(CAPABILITIES[STORAGE_BUFFER_TEST].name);
		vector<deInt32>			int32DData	= data32bit(SHADERTEMPLATE_STRIDE32BIT_STD140, rnd);

		specs["capability"]		= CAPABILITIES[STORAGE_BUFFER_TEST].cap;
		specs["storage"]		= CAPABILITIES[STORAGE_BUFFER_TEST].decor;
		specs["stridei8"]		= getStructShaderComponet(SHADERTEMPLATE_STRIDE8BIT_STD430);
		specs["stridei32"]		= getStructShaderComponet(SHADERTEMPLATE_STRIDE32BIT_STD140);
		specs["8Storage"]		= "StorageBuffer";
		specs["32Storage"]		= "Uniform";

		spec.assembly			= shaderTemplate.specialize(specs);
		spec.numWorkGroups		= IVec3(structData.structArraySize, structData.nestedArraySize, 1);
		spec.verifyIO			= checkStruct<deInt32, deInt8, SHADERTEMPLATE_STRIDE32BIT_STD140, SHADERTEMPLATE_STRIDE8BIT_STD430>;

		spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(int32DData)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		spec.outputs.push_back(Resource(BufferSp(new Int8Buffer(int8Data))));
		spec.extensions.push_back("VK_KHR_8bit_storage");
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.requestedVulkanFeatures = get8BitStorageFeatures(CAPABILITIES[STORAGE_BUFFER_TEST].name);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
	}
}

void addCompute8bitStorage8bitStructMixedTypesGroup (tcu::TestCaseGroup* group)
{
	tcu::TestContext&		testCtx			= group->getTestContext();
	de::Random				rnd				(deStringHash(group->getName()));
	vector<deInt8>			outData			= data8bit(SHADERTEMPLATE_STRIDEMIX_STD430, rnd, false);

	const StringTemplate	shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability StorageBuffer8BitAccess\n"
		"${capability}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"${OutOffsets}"
		"${InOffsets}"
		"\n"//SSBO IN
		"OpDecorate %SSBO_IN Block\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"\n"//SSBO OUT
		"OpDecorate %SSBO_OUT Block\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboOUT Binding 1\n"
		"\n"//Types
		"%void  = OpTypeVoid\n"
		"%bool  = OpTypeBool\n"
		"%i8    = OpTypeInt 8 1\n"
		"%v2i8  = OpTypeVector %i8 2\n"
		"%v3i8  = OpTypeVector %i8 3\n"
		"%v4i8  = OpTypeVector %i8 4\n"
		"%i32   = OpTypeInt 32 1\n"
		"%v2i32 = OpTypeVector %i32 2\n"
		"%v3i32 = OpTypeVector %i32 3\n"
		"%v4i32 = OpTypeVector %i32 4\n"
		"%u32   = OpTypeInt 32 0\n"
		"%uvec3 = OpTypeVector %u32 3\n"
		"%f32   = OpTypeFloat 32\n"
		"%v4f32 = OpTypeVector %f32  4\n"
		"%voidf = OpTypeFunction %void\n"
		"\n"//Consta value
		"%zero     = OpConstant %i32 0\n"
		"%c_i32_1  = OpConstant %i32 1\n"
		"%c_i32_2  = OpConstant %i32 2\n"
		"%c_i32_3  = OpConstant %i32 3\n"
		"%c_i32_4  = OpConstant %i32 4\n"
		"%c_i32_5  = OpConstant %i32 5\n"
		"%c_i32_6  = OpConstant %i32 6\n"
		"%c_i32_7  = OpConstant %i32 7\n"
		"%c_i32_8  = OpConstant %i32 8\n"
		"%c_i32_9  = OpConstant %i32 9\n"
		"%c_i32_10 = OpConstant %i32 10\n"
		"%c_i32_11 = OpConstant %i32 11\n"
		"%c_u32_1  = OpConstant %u32 1\n"
		"%c_u32_7  = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"//Arrays & Structs
		"%v2b8NestedArr11In  = OpTypeArray %v2i8 %c_u32_11\n"
		"%b32NestedArr11In   = OpTypeArray %i32 %c_u32_11\n"
		"%sb8Arr11In         = OpTypeArray %i8 %c_u32_11\n"
		"%sb32Arr11In        = OpTypeArray %i32 %c_u32_11\n"
		"%sNestedIn          = OpTypeStruct %i8 %i32 %v2b8NestedArr11In %b32NestedArr11In\n"
		"%sNestedArr11In     = OpTypeArray %sNestedIn %c_u32_11\n"
		"%structIn           = OpTypeStruct %i8 %i32 %v2i8 %v2i32 %v3i8 %v3i32 %v4i8 %v4i32 %sNestedArr11In %sb8Arr11In %sb32Arr11In\n"
		"%structArr7In       = OpTypeArray %structIn %c_u32_7\n"
		"%v2b8NestedArr11Out = OpTypeArray %v2i8 %c_u32_11\n"
		"%b32NestedArr11Out  = OpTypeArray %i32 %c_u32_11\n"
		"%sb8Arr11Out        = OpTypeArray %i8 %c_u32_11\n"
		"%sb32Arr11Out       = OpTypeArray %i32 %c_u32_11\n"
		"%sNestedOut         = OpTypeStruct %i8 %i32 %v2b8NestedArr11Out %b32NestedArr11Out\n"
		"%sNestedArr11Out    = OpTypeArray %sNestedOut %c_u32_11\n"
		"%structOut          = OpTypeStruct %i8 %i32 %v2i8 %v2i32 %v3i8 %v3i32 %v4i8 %v4i32 %sNestedArr11Out %sb8Arr11Out %sb32Arr11Out\n"
		"%structArr7Out      = OpTypeArray %structOut %c_u32_7\n"
		"\n"//Pointers
		"${uniformPtr}"
		"%i8outPtr    = OpTypePointer StorageBuffer %i8\n"
		"%v2i8outPtr  = OpTypePointer StorageBuffer %v2i8\n"
		"%v3i8outPtr  = OpTypePointer StorageBuffer %v3i8\n"
		"%v4i8outPtr  = OpTypePointer StorageBuffer %v4i8\n"
		"%i32outPtr   = OpTypePointer StorageBuffer %i32\n"
		"%v2i32outPtr = OpTypePointer StorageBuffer %v2i32\n"
		"%v3i32outPtr = OpTypePointer StorageBuffer %v3i32\n"
		"%v4i32outPtr = OpTypePointer StorageBuffer %v4i32\n"
		"%fp_i32      = OpTypePointer Function %i32\n"
		"%uvec3ptr = OpTypePointer Input %uvec3\n"
		"\n"//SSBO IN
		"%SSBO_IN    = OpTypeStruct %structArr7In\n"
		"%up_SSBOIN  = OpTypePointer ${inStorage} %SSBO_IN\n"
		"%ssboIN     = OpVariable %up_SSBOIN ${inStorage}\n"
		"\n"//SSBO OUT
		"%SSBO_OUT   = OpTypeStruct %structArr7Out\n"
		"%up_SSBOOUT = OpTypePointer StorageBuffer %SSBO_OUT\n"
		"%ssboOUT    = OpVariable %up_SSBOOUT StorageBuffer\n"
		"\n"//MAIN
		"%id                = OpVariable %uvec3ptr Input\n"
		"%main              = OpFunction %void None %voidf\n"
		"%label             = OpLabel\n"
		"%ndxArrz           = OpVariable %fp_i32  Function\n"
		"%idval             = OpLoad %uvec3 %id\n"
		"%x                 = OpCompositeExtract %u32 %idval 0\n"
		"%y                 = OpCompositeExtract %u32 %idval 1\n"
		"\n"//strutOut.b8 = strutIn.b8
		"%inP1  = OpAccessChain %i8${inPtr} %ssboIN %zero %x %zero\n"
		"%inV1  = OpLoad %i8 %inP1\n"
		"%outP1 = OpAccessChain %i8outPtr %ssboOUT %zero %x %zero\n"
		"OpStore %outP1 %inV1\n"
		"\n"//strutOut.b32 = strutIn.b32
		"%inP2  = OpAccessChain %i32${inPtr} %ssboIN %zero %x %c_i32_1\n"
		"%inV2  = OpLoad %i32 %inP2\n"
		"%outP2 = OpAccessChain %i32outPtr %ssboOUT %zero %x %c_i32_1\n"
		"OpStore %outP2 %inV2\n"
		"\n"//strutOut.v2b8 = strutIn.v2b8
		"%inP3  = OpAccessChain %v2i8${inPtr} %ssboIN %zero %x %c_i32_2\n"
		"%inV3  = OpLoad %v2i8 %inP3\n"
		"%outP3 = OpAccessChain %v2i8outPtr %ssboOUT %zero %x %c_i32_2\n"
		"OpStore %outP3 %inV3\n"
		"\n"//strutOut.v2b32 = strutIn.v2b32
		"%inP4  = OpAccessChain %v2i32${inPtr} %ssboIN %zero %x %c_i32_3\n"
		"%inV4  = OpLoad %v2i32 %inP4\n"
		"%outP4 = OpAccessChain %v2i32outPtr %ssboOUT %zero %x %c_i32_3\n"
		"OpStore %outP4 %inV4\n"
		"\n"//strutOut.v3b8 = strutIn.v3b8
		"%inP5  = OpAccessChain %v3i8${inPtr} %ssboIN %zero %x %c_i32_4\n"
		"%inV5  = OpLoad %v3i8 %inP5\n"
		"%outP5 = OpAccessChain %v3i8outPtr %ssboOUT %zero %x %c_i32_4\n"
		"OpStore %outP5 %inV5\n"
		"\n"//strutOut.v3b32 = strutIn.v3b32
		"%inP6  = OpAccessChain %v3i32${inPtr} %ssboIN %zero %x %c_i32_5\n"
		"%inV6  = OpLoad %v3i32 %inP6\n"
		"%outP6 = OpAccessChain %v3i32outPtr %ssboOUT %zero %x %c_i32_5\n"
		"OpStore %outP6 %inV6\n"
		"\n"//strutOut.v4b8 = strutIn.v4b8
		"%inP7  = OpAccessChain %v4i8${inPtr} %ssboIN %zero %x %c_i32_6\n"
		"%inV7  = OpLoad %v4i8 %inP7\n"
		"%outP7 = OpAccessChain %v4i8outPtr %ssboOUT %zero %x %c_i32_6\n"
		"OpStore %outP7 %inV7\n"
		"\n"//strutOut.v4b32 = strutIn.v4b32
		"%inP8  = OpAccessChain %v4i32${inPtr} %ssboIN %zero %x %c_i32_7\n"
		"%inV8  = OpLoad %v4i32 %inP8\n"
		"%outP8 = OpAccessChain %v4i32outPtr %ssboOUT %zero %x %c_i32_7\n"
		"OpStore %outP8 %inV8\n"
		"\n"//strutOut.b8[y] = strutIn.b8[y]
		"%inP9  = OpAccessChain %i8${inPtr} %ssboIN %zero %x %c_i32_9 %y\n"
		"%inV9  = OpLoad %i8 %inP9\n"
		"%outP9 = OpAccessChain %i8outPtr %ssboOUT %zero %x %c_i32_9 %y\n"
		"OpStore %outP9 %inV9\n"
		"\n"//strutOut.b32[y] = strutIn.b32[y]
		"%inP10  = OpAccessChain %i32${inPtr} %ssboIN %zero %x %c_i32_10 %y\n"
		"%inV10  = OpLoad %i32 %inP10\n"
		"%outP10 = OpAccessChain %i32outPtr %ssboOUT %zero %x %c_i32_10 %y\n"
		"OpStore %outP10 %inV10\n"
		"\n"//strutOut.strutNestedOut[y].b8 = strutIn.strutNestedIn[y].b8
		"%inP11 = OpAccessChain %i8${inPtr} %ssboIN %zero %x %c_i32_8 %y %zero\n"
		"%inV11 = OpLoad %i8 %inP11\n"
		"%outP11 = OpAccessChain %i8outPtr %ssboOUT %zero %x %c_i32_8 %y %zero\n"
		"OpStore %outP11 %inV11\n"
		"\n"//strutOut.strutNestedOut[y].b32 = strutIn.strutNestedIn[y].b32
		"%inP12 = OpAccessChain %i32${inPtr} %ssboIN %zero %x %c_i32_8 %y %c_i32_1\n"
		"%inV12 = OpLoad %i32 %inP12\n"
		"%outP12 = OpAccessChain %i32outPtr %ssboOUT %zero %x %c_i32_8 %y %c_i32_1\n"
		"OpStore %outP12 %inV12\n"
		"\n"
		"${zBeginLoop}"
		"\n"//strutOut.strutNestedOut[y].v2b8[valNdx] = strutIn.strutNestedIn[y].v2b8[valNdx]
		"%inP13  = OpAccessChain %v2i8${inPtr} %ssboIN %zero %x %c_i32_8 %y %c_i32_2 %Valz\n"
		"%inV13  = OpLoad %v2i8 %inP13\n"
		"%outP13 = OpAccessChain %v2i8outPtr %ssboOUT %zero %x %c_i32_8 %y %c_i32_2 %Valz\n"
		"OpStore %outP13 %inV13\n"
		"\n"//strutOut.strutNestedOut[y].b32[valNdx] = strutIn.strutNestedIn[y].b32[valNdx]
		"%inP14  = OpAccessChain %i32${inPtr} %ssboIN %zero %x %c_i32_8 %y %c_i32_3 %Valz\n"
		"%inV14  = OpLoad %i32 %inP14\n"
		"%outP14 = OpAccessChain %i32outPtr %ssboOUT %zero %x %c_i32_8 %y %c_i32_3 %Valz\n"
		"OpStore %outP14 %inV14\n"
		"\n${zEndLoop}\n"
		"OpBranch %exitLabel\n"
		"%exitLabel = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n");

	for (deUint32 capIdx = 0; capIdx < DE_LENGTH_OF_ARRAY(CAPABILITIES); ++capIdx)
	{  // int
		const bool				isUniform	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER == CAPABILITIES[capIdx].dtype;
		vector<deInt8>			inData		= isUniform ? data8bit(SHADERTEMPLATE_STRIDEMIX_STD140, rnd) : data8bit(SHADERTEMPLATE_STRIDEMIX_STD430, rnd);
		ComputeShaderSpec		spec;
		map<string, string>		specsOffset;
		map<string, string>		specsLoop;
		map<string, string>		specs;
		string					testName	= string(CAPABILITIES[capIdx].name);

		specsLoop["exeCount"]	= "c_i32_11";
		specsLoop["loopName"]	= "z";
		specs["zBeginLoop"]		= beginLoop(specsLoop);
		specs["zEndLoop"]		= endLoop(specsLoop);
		specs["inStorage"]		= isUniform ? "Uniform" : "StorageBuffer";
		specs["capability"]		= "";
		specs["uniformPtr"]		= isUniform ?
								"%i8inPtr     = OpTypePointer Uniform %i8\n"
								"%v2i8inPtr   = OpTypePointer Uniform %v2i8\n"
								"%v3i8inPtr   = OpTypePointer Uniform %v3i8\n"
								"%v4i8inPtr   = OpTypePointer Uniform %v4i8\n"
								"%i32inPtr    = OpTypePointer Uniform %i32\n"
								"%v2i32inPtr  = OpTypePointer Uniform %v2i32\n"
								"%v3i32inPtr  = OpTypePointer Uniform %v3i32\n"
								"%v4i32inPtr  = OpTypePointer Uniform %v4i32\n" :
								"";
		specs["inPtr"]			= isUniform ? "inPtr" : "outPtr";
		specsOffset["InOut"]	= "In";
		specs["InOffsets"]		= StringTemplate(isUniform ? getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD140) : getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD430)).specialize(specsOffset);
		specsOffset["InOut"]	= "Out";
		specs["OutOffsets"]		= StringTemplate(getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD430)).specialize(specsOffset);
		if(isUniform)
		{
			specs["capability"]				= "OpCapability " + string(CAPABILITIES[capIdx].cap);
		}

		spec.assembly					= shaderTemplate.specialize(specs);
		spec.numWorkGroups				= IVec3(structData.structArraySize, structData.nestedArraySize, 1);
		spec.verifyIO					= isUniform ? checkStruct<deInt8, deInt8, SHADERTEMPLATE_STRIDEMIX_STD140, SHADERTEMPLATE_STRIDEMIX_STD430> : checkStruct<deInt8, deInt8, SHADERTEMPLATE_STRIDEMIX_STD430, SHADERTEMPLATE_STRIDEMIX_STD430>;
		spec.inputs.push_back			(Resource(BufferSp(new Int8Buffer(inData)), CAPABILITIES[capIdx].dtype));
		spec.outputs.push_back			(Resource(BufferSp(new Int8Buffer(outData))));
		spec.extensions.push_back		("VK_KHR_8bit_storage");
		spec.extensions.push_back		("VK_KHR_storage_buffer_storage_class");
		spec.requestedVulkanFeatures	= get8BitStorageFeatures(CAPABILITIES[capIdx].name);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testName.c_str(), spec));
	}
}

void addGraphics8BitStorageUniformInt32To8Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	const deUint32						numDataPoints		= 256u;
	RGBA								defaultColors[4];
	GraphicsResources					resources;
	vector<string>						extensions;
	const StringTemplate				capabilities		("OpCapability ${cap}\n");
	vector<deInt8>						outputs				(numDataPoints);

	extensions.push_back("VK_KHR_8bit_storage");
	fragments["extension"]	=
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"";

	getDefaultColors(defaultColors);

	struct IntegerFacts
	{
		const char*	name;
		const char*	type32;
		const char*	type8;
		const char* opcode;
		const char*	isSigned;
	};

	const IntegerFacts	intFacts[]		=
	{
		{"sint",	"%i32",		"%i8",		"OpSConvert",	"1"},
		{"uint",	"%u32",		"%u8",		"OpUConvert",	"0"},
	};

	const StringTemplate	scalarPreMain(
			"${itype8} = OpTypeInt 8 ${signed}\n"
			"%c_i32_256 = OpConstant %i32 256\n"
			"   %up_i32 = OpTypePointer Uniform ${itype32}\n"
			"   %up_i8 = OpTypePointer StorageBuffer ${itype8}\n"
			"   %ra_i32 = OpTypeArray ${itype32} %c_i32_256\n"
			"   %ra_i8 = OpTypeArray ${itype8} %c_i32_256\n"
			"   %SSBO32 = OpTypeStruct %ra_i32\n"
			"   %SSBO8 = OpTypeStruct %ra_i8\n"
			"%up_SSBO32 = OpTypePointer Uniform %SSBO32\n"
			"%up_SSBO8 = OpTypePointer StorageBuffer %SSBO8\n"
			"   %ssbo32 = OpVariable %up_SSBO32 Uniform\n"
			"   %ssbo8 = OpVariable %up_SSBO8 StorageBuffer\n");

	const StringTemplate	scalarDecoration(
			"OpDecorate %ra_i32 ArrayStride 16\n"
			"OpDecorate %ra_i8 ArrayStride 1\n"
			"OpDecorate %SSBO32 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 0\n"
			"OpDecorate %ssbo8 Binding 1\n");

	const StringTemplate	scalarTestFunc(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_256\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_i32 %ssbo32 %c_i32_0 %30\n"
			"%val32 = OpLoad ${itype32} %src\n"
			"%val8 = ${convert} ${itype8} %val32\n"
			"  %dst = OpAccessChain %up_i8 %ssbo8 %c_i32_0 %30\n"
			"         OpStore %dst %val8\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"

			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	const StringTemplate	vecPreMain(
			"${itype8} = OpTypeInt 8 ${signed}\n"
			" %c_i32_64 = OpConstant %i32 64\n"
			"%v4itype8 = OpTypeVector ${itype8} 4\n"
			" %up_v4i32 = OpTypePointer Uniform ${v4itype32}\n"
			" %up_v4i8 = OpTypePointer StorageBuffer %v4itype8\n"
			" %ra_v4i32 = OpTypeArray ${v4itype32} %c_i32_64\n"
			" %ra_v4i8 = OpTypeArray %v4itype8 %c_i32_64\n"
			"   %SSBO32 = OpTypeStruct %ra_v4i32\n"
			"   %SSBO8 = OpTypeStruct %ra_v4i8\n"
			"%up_SSBO32 = OpTypePointer Uniform %SSBO32\n"
			"%up_SSBO8 = OpTypePointer StorageBuffer %SSBO8\n"
			"   %ssbo32 = OpVariable %up_SSBO32 Uniform\n"
			"   %ssbo8 = OpVariable %up_SSBO8 StorageBuffer\n");

	const StringTemplate	vecDecoration(
			"OpDecorate %ra_v4i32 ArrayStride 16\n"
			"OpDecorate %ra_v4i8 ArrayStride 4\n"
			"OpDecorate %SSBO32 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 0\n"
			"OpDecorate %ssbo8 Binding 1\n");

	const StringTemplate	vecTestFunc(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_64\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_v4i32 %ssbo32 %c_i32_0 %30\n"
			"%val32 = OpLoad ${v4itype32} %src\n"
			"%val8 = ${convert} %v4itype8 %val32\n"
			"  %dst = OpAccessChain %up_v4i8 %ssbo8 %c_i32_0 %30\n"
			"         OpStore %dst %val8\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"

			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	struct Category
	{
		const char*				name;
		const StringTemplate&	preMain;
		const StringTemplate&	decoration;
		const StringTemplate&	testFunction;
		const deUint32			numElements;
	};

	const Category		categories[]	=
	{
		{"scalar",	scalarPreMain,	scalarDecoration,	scalarTestFunc,	1},
		{"vector",	vecPreMain,		vecDecoration,		vecTestFunc,	4},
	};


	for (deUint32 catIdx = 0; catIdx < DE_LENGTH_OF_ARRAY(categories); ++catIdx)
	{
		resources.inputs.clear();
		resources.outputs.clear();
		vector<deInt32>	inputs = getInt32s(rnd, ((arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt32))) * numDataPoints) / categories[catIdx].numElements);

		if ( 0 != (arrayStrideInBytesUniform - static_cast<deUint32>(sizeof(deInt32)) * categories[catIdx].numElements))
			resources.verifyIO = checkUniformsArray<deInt32, deInt8, 1>;
		else
		{
			resources.verifyIO = DE_NULL;
			for (deUint32 numNdx = 0; numNdx < numDataPoints; ++numNdx)
				outputs[numNdx] = static_cast<deInt8>(0xffff & inputs[numNdx]);
		}

		resources.inputs.push_back(Resource(BufferSp(new Int32Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		resources.outputs.push_back(Resource(BufferSp(new Int8Buffer(outputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		for (deUint32 factIdx = 0; factIdx < DE_LENGTH_OF_ARRAY(intFacts); ++factIdx)
		{
			map<string, string>	specs;
			VulkanFeatures		features;
			string				name		= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + categories[catIdx].name + "_" + intFacts[factIdx].name;

			specs["cap"]					= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
			specs["itype32"]				= intFacts[factIdx].type32;
			specs["v4itype32"]				= "%v4" + string(intFacts[factIdx].type32).substr(1);
			specs["itype8"]					= intFacts[factIdx].type8;
			specs["signed"]					= intFacts[factIdx].isSigned;
			specs["convert"]				= intFacts[factIdx].opcode;

			fragments["pre_main"]			= categories[catIdx].preMain.specialize(specs);
			fragments["testfun"]			= categories[catIdx].testFunction.specialize(specs);
			fragments["capability"]			= capabilities.specialize(specs);
			fragments["decoration"]			= categories[catIdx].decoration.specialize(specs);

			features												= get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);
			features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
			features.coreFeatures.fragmentStoresAndAtomics			= true;

			createTestsForAllStages(name, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
	}
}

void addGraphics8BitStorageUniformInt8To32Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	const deUint32						numDataPoints		= 256;
	RGBA								defaultColors[4];
	vector<deInt32>						outputs				(numDataPoints);
	GraphicsResources					resources;
	vector<string>						extensions;
	const StringTemplate				capabilities		("OpCapability ${cap}\n");

	extensions.push_back("VK_KHR_8bit_storage");
	fragments["extension"]	=
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"";

	getDefaultColors(defaultColors);

	struct IntegerFacts
	{
		const char*	name;
		const char*	type32;
		const char*	type8;
		const char* opcode;
		bool		isSigned;
	};

	const IntegerFacts	intFacts[]	=
	{
		{"sint",	"%i32",		"%i8",		"OpSConvert",	true},
		{"uint",	"%u32",		"%u8",		"OpUConvert",	false},
	};

	struct ConstantIndex
	{
		bool		useConstantIndex;
		deUint32	constantIndex;
	};

	ConstantIndex	constantIndices[] =
	{
		{ false,	0 },
		{ true,		4 },
		{ true,		5 },
		{ true,		6 }
	};

	const StringTemplate scalarPreMain		(
			"${itype8} = OpTypeInt 8 ${signed}\n"
			" %c_i32_256 = OpConstant %i32 256\n"
			"%c_i32_ci  = OpConstant %i32 ${constarrayidx}\n"
			"   %up_i32 = OpTypePointer StorageBuffer ${itype32}\n"
			"   %up_i8 = OpTypePointer Uniform ${itype8}\n"
			"   %ra_i32 = OpTypeArray ${itype32} %c_i32_256\n"
			"   %ra_i8 = OpTypeArray ${itype8} %c_i32_256\n"
			"   %SSBO32 = OpTypeStruct %ra_i32\n"
			"   %SSBO8 = OpTypeStruct %ra_i8\n"
			"%up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
			"%up_SSBO8 = OpTypePointer Uniform %SSBO8\n"
			"   %ssbo32 = OpVariable %up_SSBO32 StorageBuffer\n"
			"   %ssbo8 = OpVariable %up_SSBO8 Uniform\n");

	const StringTemplate scalarDecoration		(
			"OpDecorate %ra_i32 ArrayStride 4\n"
			"OpDecorate %ra_i8 ArrayStride 16\n"
			"OpDecorate %SSBO32 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 1\n"
			"OpDecorate %ssbo8 Binding 0\n");

	const StringTemplate scalarTestFunc	(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_256\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_i8 %ssbo8 %c_i32_0 %${arrayindex}\n"
			"%val8 = OpLoad ${itype8} %src\n"
			"%val32 = ${convert} ${itype32} %val8\n"
			"  %dst = OpAccessChain %up_i32 %ssbo32 %c_i32_0 %30\n"
			"         OpStore %dst %val32\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"
			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	const StringTemplate vecPreMain		(
			"${itype8} = OpTypeInt 8 ${signed}\n"
			"%c_i32_128 = OpConstant %i32 128\n"
			"%c_i32_ci  = OpConstant %i32 ${constarrayidx}\n"
			"%v2itype8 = OpTypeVector ${itype8} 2\n"
			" %up_v2i32 = OpTypePointer StorageBuffer ${v2itype32}\n"
			" %up_v2i8 = OpTypePointer Uniform %v2itype8\n"
			" %ra_v2i32 = OpTypeArray ${v2itype32} %c_i32_128\n"
			" %ra_v2i8 = OpTypeArray %v2itype8 %c_i32_128\n"
			"   %SSBO32 = OpTypeStruct %ra_v2i32\n"
			"   %SSBO8 = OpTypeStruct %ra_v2i8\n"
			"%up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
			"%up_SSBO8 = OpTypePointer Uniform %SSBO8\n"
			"   %ssbo32 = OpVariable %up_SSBO32 StorageBuffer\n"
			"   %ssbo8 = OpVariable %up_SSBO8 Uniform\n");

	const StringTemplate vecDecoration		(
			"OpDecorate %ra_v2i32 ArrayStride 8\n"
			"OpDecorate %ra_v2i8 ArrayStride 16\n"
			"OpDecorate %SSBO32 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 1\n"
			"OpDecorate %ssbo8 Binding 0\n");

	const StringTemplate vecTestFunc	(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_128\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_v2i8 %ssbo8 %c_i32_0 %${arrayindex}\n"
			"%val8 = OpLoad %v2itype8 %src\n"
			"%val32 = ${convert} ${v2itype32} %val8\n"
			"  %dst = OpAccessChain %up_v2i32 %ssbo32 %c_i32_0 %30\n"
			"         OpStore %dst %val32\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"
			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	struct Category
	{
		const char*				name;
		const StringTemplate&	preMain;
		const StringTemplate&	decoration;
		const StringTemplate&	testFunction;
		const deUint32			numElements;
	};

	const Category		categories[]	=
	{
		{"scalar", scalarPreMain,	scalarDecoration,	scalarTestFunc,	1},
		{"vector", vecPreMain,		vecDecoration,		vecTestFunc,	2},
	};

	for (deUint32 catIdx = 0; catIdx < DE_LENGTH_OF_ARRAY(categories); ++catIdx)
	{
		resources.inputs.clear();
		vector<deInt8>	inputs = getInt8s(rnd, (arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt8))) * (numDataPoints / categories[catIdx].numElements));
		resources.inputs.push_back(Resource(BufferSp(new Int8Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		for (deUint32 factIdx = 0; factIdx < DE_LENGTH_OF_ARRAY(intFacts); ++factIdx)
			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool				useConstIdx	= constantIndices[constIndexIdx].useConstantIndex;
				deUint32			constIdx	= constantIndices[constIndexIdx].constantIndex;
				map<string, string>	specs;
				VulkanFeatures		features;
				string				name		= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + categories[catIdx].name + "_" + intFacts[factIdx].name;

				specs["cap"]					= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
				specs["itype32"]				= intFacts[factIdx].type32;
				specs["v2itype32"]				= "%v2" + string(intFacts[factIdx].type32).substr(1);
				specs["itype8"]					= intFacts[factIdx].type8;
				if (intFacts[factIdx].isSigned)
					specs["signed"]				= "1";
				else
					specs["signed"]				= "0";
				specs["convert"]				= intFacts[factIdx].opcode;
				specs["constarrayidx"]			= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				fragments["pre_main"]			= categories[catIdx].preMain.specialize(specs);
				fragments["testfun"]			= categories[catIdx].testFunction.specialize(specs);
				fragments["capability"]			= capabilities.specialize(specs);
				fragments["decoration"]			= categories[catIdx].decoration.specialize(specs);

				if (useConstIdx)
					name += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(outputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
				if (useConstIdx)
				{
					switch(constantIndices[constIndexIdx].constantIndex)
					{
					case 0:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 2, 0>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 1, 0>;
						break;
					case 4:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 2, 4>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 1, 4>;
						break;
					case 5:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 2, 5>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 1, 5>;
						break;
					case 6:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 2, 6>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt32, 1, 6>;
						break;
					default:
						DE_FATAL("Impossible");
						break;
					};
				}
				else
				{
					if (categories[catIdx].numElements == 2)
						resources.verifyIO = checkUniformsArray<deInt8, deInt32, 2>;
					else
						resources.verifyIO = checkUniformsArray<deInt8, deInt32, 1>;
				}

				features												= get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);
				features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
				features.coreFeatures.fragmentStoresAndAtomics			= true;

				createTestsForAllStages(name, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
	}
}

void addGraphics8BitStoragePushConstantInt8To32Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	RGBA								defaultColors[4];
	const deUint32						numDataPoints		= 64;
	vector<deInt8>						inputs				= getInt8s(rnd, numDataPoints);
	vector<deInt32>						sOutputs;
	vector<deInt32>						uOutputs;
	PushConstants						pcs;
	GraphicsResources					resources;
	vector<string>						extensions;
	const deUint8						signBitMask			= 0x80;
	const deUint32						signExtendMask		= 0xffff0000;
	VulkanFeatures						requiredFeatures;

	struct ConstantIndex
	{
		bool		useConstantIndex;
		deUint32	constantIndex;
	};

	ConstantIndex	constantIndices[] =
	{
		{ false,	0 },
		{ true,		4 },
		{ true,		5 },
		{ true,		6 }
	};

	sOutputs.reserve(inputs.size());
	uOutputs.reserve(inputs.size());

	for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
	{
		uOutputs.push_back(static_cast<deUint8>(inputs[numNdx]));
		if (inputs[numNdx] & signBitMask)
			sOutputs.push_back(static_cast<deInt32>(inputs[numNdx] | signExtendMask));
		else
			sOutputs.push_back(static_cast<deInt32>(inputs[numNdx]));
	}

	extensions.push_back("VK_KHR_8bit_storage");

	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	requiredFeatures.ext8BitStorage									= EXT8BITSTORAGEFEATURES_PUSH_CONSTANT;

	fragments["capability"]				= "OpCapability StoragePushConstant8\n";
	fragments["extension"]				= "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
										  "OpExtension \"SPV_KHR_8bit_storage\"";

	pcs.setPushConstant(BufferSp(new Int8Buffer(inputs)));

	getDefaultColors(defaultColors);

	const StringTemplate	testFun		(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"    %i = OpVariable %fp_i32 Function\n"
		"         OpStore %i %c_i32_0\n"
		"         OpBranch %loop\n"

		" %loop = OpLabel\n"
		"   %15 = OpLoad %i32 %i\n"
		"   %lt = OpSLessThan %bool %15 %c_i32_${count}\n"
		"         OpLoopMerge %merge %inc None\n"
		"         OpBranchConditional %lt %write %merge\n"

		"%write = OpLabel\n"
		"   %30 = OpLoad %i32 %i\n"
		"  %src = OpAccessChain %pp_${type8} %pc8 %c_i32_0 %${arrayindex}\n"
		"%val8 = OpLoad %${type8} %src\n"
		"%val32 = ${convert} %${type32} %val8\n"
		"  %dst = OpAccessChain %up_${type32} %ssbo32 %c_i32_0 %30\n"
		"         OpStore %dst %val32\n"
		"         OpBranch %inc\n"

		"  %inc = OpLabel\n"
		"   %37 = OpLoad %i32 %i\n"
		"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
		"         OpStore %i %39\n"
		"         OpBranch %loop\n"

		"%merge = OpLabel\n"
		"         OpReturnValue %param\n"

		"OpFunctionEnd\n");

	{  // Scalar cases
		const StringTemplate	preMain		(
			"         %${type8} = OpTypeInt 8 ${signed}\n"
			"    %c_i32_${count} = OpConstant %i32 ${count}\n"					// Should be the same as numDataPoints
			"         %c_i32_ci = OpConstant %i32 ${constarrayidx}\n"
			"%a${count}${type8} = OpTypeArray %${type8} %c_i32_${count}\n"
			"%a${count}${type32} = OpTypeArray %${type32} %c_i32_${count}\n"
			"      %pp_${type8} = OpTypePointer PushConstant %${type8}\n"
			"      %up_${type32} = OpTypePointer StorageBuffer      %${type32}\n"
			"            %SSBO32 = OpTypeStruct %a${count}${type32}\n"
			"         %up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
			"            %ssbo32 = OpVariable %up_SSBO32 StorageBuffer\n"
			"              %PC8 = OpTypeStruct %a${count}${type8}\n"
			"           %pp_PC8 = OpTypePointer PushConstant %PC8\n"
			"              %pc8 = OpVariable %pp_PC8 PushConstant\n");

		const StringTemplate	decoration	(
			"OpDecorate %a${count}${type8} ArrayStride 1\n"
			"OpDecorate %a${count}${type32} ArrayStride 4\n"
			"OpDecorate %SSBO32 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpDecorate %PC8 Block\n"
			"OpMemberDecorate %PC8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 0\n");

		{  // signed int
			map<string, string>		specs;

			specs["type8"]			= "i8";
			specs["type32"]			= "i32";
			specs["signed"]			= "1";
			specs["count"]			= "64";
			specs["convert"]		= "OpSConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "sint_scalar";
				vector<deInt32>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(sOutputs[constIdx]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(useConstIdx ? constIdxData : sOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
		{  // signed int
			map<string, string>		specs;

			specs["type8"]			= "u8";
			specs["type32"]			= "u32";
			specs["signed"]			= "0";
			specs["count"]			= "64";
			specs["convert"]		= "OpUConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "uint_scalar";
				vector<deInt32>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(uOutputs[constIdx]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(useConstIdx ? constIdxData : uOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
	}

	{  // Vector cases
		const StringTemplate	preMain		(
			"    %${base_type8} = OpTypeInt 8 ${signed}\n"
			"         %${type8} = OpTypeVector %${base_type8} 2\n"
			"    %c_i32_${count} = OpConstant %i32 ${count}\n"
			"          %c_i32_ci = OpConstant %i32 ${constarrayidx}\n"
			"%a${count}${type8} = OpTypeArray %${type8} %c_i32_${count}\n"
			"%a${count}${type32} = OpTypeArray %${type32} %c_i32_${count}\n"
			"      %pp_${type8} = OpTypePointer PushConstant %${type8}\n"
			"      %up_${type32} = OpTypePointer StorageBuffer      %${type32}\n"
			"            %SSBO32 = OpTypeStruct %a${count}${type32}\n"
			"         %up_SSBO32 = OpTypePointer StorageBuffer %SSBO32\n"
			"            %ssbo32 = OpVariable %up_SSBO32 StorageBuffer\n"
			"              %PC8 = OpTypeStruct %a${count}${type8}\n"
			"           %pp_PC8 = OpTypePointer PushConstant %PC8\n"
			"              %pc8 = OpVariable %pp_PC8 PushConstant\n");

		const StringTemplate	decoration	(
			"OpDecorate %a${count}${type8} ArrayStride 2\n"
			"OpDecorate %a${count}${type32} ArrayStride 8\n"
			"OpDecorate %SSBO32 Block\n"
			"OpMemberDecorate %SSBO32 0 Offset 0\n"
			"OpDecorate %PC8 Block\n"
			"OpMemberDecorate %PC8 0 Offset 0\n"
			"OpDecorate %ssbo32 DescriptorSet 0\n"
			"OpDecorate %ssbo32 Binding 0\n");

		{  // signed int
			map<string, string>		specs;

			specs["base_type8"]		= "i8";
			specs["type8"]			= "v2i8";
			specs["type32"]			= "v2i32";
			specs["signed"]			= "1";
			specs["count"]			= "32";				// 64 / 2
			specs["convert"]		= "OpSConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "sint_vector";
				vector<deInt32>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(sOutputs[constIdx * 2 + numIdx % 2]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(useConstIdx ? constIdxData : sOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
		{  // signed int
			map<string, string>		specs;

			specs["base_type8"]		= "u8";
			specs["type8"]			= "v2u8";
			specs["type32"]			= "v2u32";
			specs["signed"]			= "0";
			specs["count"]			= "32";
			specs["convert"]		= "OpUConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "uint_vector";
				vector<deInt32>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(uOutputs[constIdx * 2 + numIdx % 2]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(useConstIdx ? constIdxData : uOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
	}
}

void addGraphics8BitStorageUniformInt16To8Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	const deUint32						numDataPoints		= 256;
	RGBA								defaultColors[4];
	GraphicsResources					resources;
	vector<string>						extensions;
	const StringTemplate				capabilities		("OpCapability ${cap}\n");

	extensions.push_back("VK_KHR_8bit_storage");
	extensions.push_back("VK_KHR_16bit_storage");
	fragments["extension"]	=
		"OpCapability StorageUniform16\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_16bit_storage\"\n";

	getDefaultColors(defaultColors);

	struct IntegerFacts
	{
		const char*	name;
		const char*	type16;
		const char*	type8;
		const char* opcode;
		const char*	isSigned;
	};

	const IntegerFacts	intFacts[]		=
	{
		{"sint",	"%i16",		"%i8",		"OpSConvert",	"1"},
		{"uint",	"%u16",		"%u8",		"OpUConvert",	"0"},
	};

	const StringTemplate	scalarPreMain(
			"${itype8}  = OpTypeInt 8 ${signed}\n"
			"${itype16} = OpTypeInt 16 ${signed}\n"
			"%c_i32_256 = OpConstant %i32 256\n"
			"   %up_i16 = OpTypePointer Uniform ${itype16}\n"
			"   %up_i8  = OpTypePointer StorageBuffer ${itype8}\n"
			"   %ra_i16 = OpTypeArray ${itype16} %c_i32_256\n"
			"   %ra_i8  = OpTypeArray ${itype8} %c_i32_256\n"
			"   %SSBO16 = OpTypeStruct %ra_i16\n"
			"   %SSBO8  = OpTypeStruct %ra_i8\n"
			"%up_SSBO16 = OpTypePointer Uniform %SSBO16\n"
			"%up_SSBO8  = OpTypePointer StorageBuffer %SSBO8\n"
			"   %ssbo16 = OpVariable %up_SSBO16 Uniform\n"
			"   %ssbo8  = OpVariable %up_SSBO8 StorageBuffer\n");

	const StringTemplate	scalarDecoration(
			"OpDecorate %ra_i16 ArrayStride 16\n"
			"OpDecorate %ra_i8 ArrayStride 1\n"
			"OpDecorate %SSBO16 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 0\n"
			"OpDecorate %ssbo8 Binding 1\n");

	const StringTemplate	scalarTestFunc(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_256\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_i16 %ssbo16 %c_i32_0 %30\n"
			"%val16 = OpLoad ${itype16} %src\n"
			"%val8 = ${convert} ${itype8} %val16\n"
			"  %dst = OpAccessChain %up_i8 %ssbo8 %c_i32_0 %30\n"
			"         OpStore %dst %val8\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"

			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	const StringTemplate	vecPreMain(
			"${itype8} = OpTypeInt 8 ${signed}\n"
			"${itype16} = OpTypeInt 16 ${signed}\n"
			"${v4itype16} = OpTypeVector ${itype16} 4\n"
			"%c_i32_64 = OpConstant %i32 64\n"
			"%v4itype8 = OpTypeVector ${itype8} 4\n"
			" %up_v4i16 = OpTypePointer Uniform ${v4itype16}\n"
			" %up_v4i8 = OpTypePointer StorageBuffer %v4itype8\n"
			" %ra_v4i16 = OpTypeArray ${v4itype16} %c_i32_64\n"
			" %ra_v4i8 = OpTypeArray %v4itype8 %c_i32_64\n"
			"   %SSBO16 = OpTypeStruct %ra_v4i16\n"
			"   %SSBO8 = OpTypeStruct %ra_v4i8\n"
			"%up_SSBO16 = OpTypePointer Uniform %SSBO16\n"
			"%up_SSBO8 = OpTypePointer StorageBuffer %SSBO8\n"
			"   %ssbo16 = OpVariable %up_SSBO16 Uniform\n"
			"   %ssbo8 = OpVariable %up_SSBO8 StorageBuffer\n");

	const StringTemplate	vecDecoration(
			"OpDecorate %ra_v4i16 ArrayStride 16\n"
			"OpDecorate %ra_v4i8 ArrayStride 4\n"
			"OpDecorate %SSBO16 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 0\n"
			"OpDecorate %ssbo8 Binding 1\n");

	const StringTemplate	vecTestFunc(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_64\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_v4i16 %ssbo16 %c_i32_0 %30\n"
			"%val16 = OpLoad ${v4itype16} %src\n"
			"%val8 = ${convert} %v4itype8 %val16\n"
			"  %dst = OpAccessChain %up_v4i8 %ssbo8 %c_i32_0 %30\n"
			"         OpStore %dst %val8\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"

			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	struct Category
	{
		const char*				name;
		const StringTemplate&	preMain;
		const StringTemplate&	decoration;
		const StringTemplate&	testFunction;
		const deUint32			numElements;
	};

	const Category		categories[]	=
	{
		{"scalar",	scalarPreMain,	scalarDecoration,	scalarTestFunc,	1},
		{"vector",	vecPreMain,		vecDecoration,		vecTestFunc,	4},
	};

	for (deUint32 catIdx = 0; catIdx < DE_LENGTH_OF_ARRAY(categories); ++catIdx)
	{
		resources.inputs.clear();
		resources.outputs.clear();
		vector<deInt16>						inputs	= getInt16s(rnd, ((arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt16))) * numDataPoints) / categories[catIdx].numElements);
		vector<deInt8>						outputs	(numDataPoints/ categories[catIdx].numElements);

		switch (categories[catIdx].numElements)
		{
		case 1:
			resources.verifyIO = checkUniformsArray<deInt16, deInt8, 1>;
			break;
		case 4:
			resources.verifyIO = checkUniformsArray<deInt16, deInt8, 4>;
			break;
		default:
			DE_FATAL("Impossible");
			break;
		}

		resources.inputs.push_back(Resource(BufferSp(new Int16Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		resources.outputs.push_back(Resource(BufferSp(new Int8Buffer(outputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		for (deUint32 factIdx = 0; factIdx < DE_LENGTH_OF_ARRAY(intFacts); ++factIdx)
		{
			map<string, string>	specs;
			VulkanFeatures		features;
			string				name		= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + categories[catIdx].name + "_" + intFacts[factIdx].name;

			specs["cap"]					= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
			specs["itype16"]				= intFacts[factIdx].type16;
			specs["v4itype16"]				= "%v4" + string(intFacts[factIdx].type16).substr(1);
			specs["itype8"]					= intFacts[factIdx].type8;
			specs["signed"]					= intFacts[factIdx].isSigned;
			specs["convert"]				= intFacts[factIdx].opcode;

			fragments["pre_main"]			= categories[catIdx].preMain.specialize(specs);
			fragments["testfun"]			= categories[catIdx].testFunction.specialize(specs);
			fragments["capability"]			= capabilities.specialize(specs);
			fragments["decoration"]			= categories[catIdx].decoration.specialize(specs);

			features												= get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);
			features.ext16BitStorage								= EXT16BITSTORAGEFEATURES_UNIFORM;
			features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
			features.coreFeatures.fragmentStoresAndAtomics			= true;

			createTestsForAllStages(name, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
	}
}

void addGraphics8BitStorageUniformInt8To16Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	const deUint32						numDataPoints		= 256;
	vector<deInt16>						outputs				(numDataPoints);
	RGBA								defaultColors[4];
	GraphicsResources					resources;
	vector<string>						extensions;
	const StringTemplate				capabilities		("OpCapability ${cap}\n");

	extensions.push_back("VK_KHR_8bit_storage");
	extensions.push_back("VK_KHR_16bit_storage");
	fragments["extension"]	=
		"OpCapability StorageUniform16\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_16bit_storage\"\n";

	getDefaultColors(defaultColors);

	struct IntegerFacts
	{
		const char*	name;
		const char*	type16;
		const char*	type8;
		const char* opcode;
		bool		isSigned;
	};

	const IntegerFacts	intFacts[]	=
	{
		{"sint",	"%i16",		"%i8",		"OpSConvert",	true},
		{"uint",	"%u16",		"%u8",		"OpUConvert",	false},
	};

	struct ConstantIndex
	{
		bool		useConstantIndex;
		deUint32	constantIndex;
	};

	ConstantIndex	constantIndices[] =
	{
		{ false,	0 },
		{ true,		4 },
		{ true,		5 },
		{ true,		6 }
	};

	const StringTemplate scalarPreMain		(
			"${itype8}   = OpTypeInt 8 ${signed}\n"
			"${itype16}   = OpTypeInt 16 ${signed}\n"
			" %c_i32_256 = OpConstant %i32 256\n"
			"%c_i32_ci   = OpConstant %i32 ${constarrayidx}\n"
			"   %up_i16  = OpTypePointer StorageBuffer ${itype16}\n"
			"   %up_i8   = OpTypePointer Uniform ${itype8}\n"
			"   %ra_i16  = OpTypeArray ${itype16} %c_i32_256\n"
			"   %ra_i8   = OpTypeArray ${itype8} %c_i32_256\n"
			"   %SSBO16  = OpTypeStruct %ra_i16\n"
			"   %SSBO8   = OpTypeStruct %ra_i8\n"
			"%up_SSBO16  = OpTypePointer StorageBuffer %SSBO16\n"
			"%up_SSBO8   = OpTypePointer Uniform %SSBO8\n"
			"   %ssbo16  = OpVariable %up_SSBO16 StorageBuffer\n"
			"   %ssbo8   = OpVariable %up_SSBO8 Uniform\n");

	const StringTemplate scalarDecoration		(
			"OpDecorate %ra_i16 ArrayStride 2\n"
			"OpDecorate %ra_i8 ArrayStride 16\n"
			"OpDecorate %SSBO16 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 1\n"
			"OpDecorate %ssbo8 Binding 0\n");

	const StringTemplate scalarTestFunc	(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_256\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_i8 %ssbo8 %c_i32_0 %${arrayindex}\n"
			"%val8 = OpLoad ${itype8} %src\n"
			"%val16 = ${convert} ${itype16} %val8\n"
			"  %dst = OpAccessChain %up_i16 %ssbo16 %c_i32_0 %30\n"
			"         OpStore %dst %val16\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"
			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	const StringTemplate vecPreMain		(
			"${itype8}  = OpTypeInt 8 ${signed}\n"
			"${itype16} = OpTypeInt 16 ${signed}\n"
			"${v2itype16} = OpTypeVector ${itype16} 2\n"
			"%c_i32_128 = OpConstant %i32 128\n"
			"%c_i32_ci  = OpConstant %i32 ${constarrayidx}\n"
			"%v2itype8  = OpTypeVector ${itype8} 2\n"
			" %up_v2i16 = OpTypePointer StorageBuffer ${v2itype16}\n"
			" %up_v2i8  = OpTypePointer Uniform %v2itype8\n"
			" %ra_v2i16 = OpTypeArray ${v2itype16} %c_i32_128\n"
			" %ra_v2i8  = OpTypeArray %v2itype8 %c_i32_128\n"
			"   %SSBO16 = OpTypeStruct %ra_v2i16\n"
			"   %SSBO8  = OpTypeStruct %ra_v2i8\n"
			"%up_SSBO16 = OpTypePointer StorageBuffer %SSBO16\n"
			"%up_SSBO8  = OpTypePointer Uniform %SSBO8\n"
			"   %ssbo16 = OpVariable %up_SSBO16 StorageBuffer\n"
			"   %ssbo8  = OpVariable %up_SSBO8 Uniform\n");

	const StringTemplate vecDecoration		(
			"OpDecorate %ra_v2i16 ArrayStride 4\n"
			"OpDecorate %ra_v2i8 ArrayStride 16\n"
			"OpDecorate %SSBO16 Block\n"
			"OpDecorate %SSBO8 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpMemberDecorate %SSBO8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo8 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 1\n"
			"OpDecorate %ssbo8 Binding 0\n");

	const StringTemplate vecTestFunc	(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"%entry = OpLabel\n"
			"    %i = OpVariable %fp_i32 Function\n"
			"         OpStore %i %c_i32_0\n"
			"         OpBranch %loop\n"

			" %loop = OpLabel\n"
			"   %15 = OpLoad %i32 %i\n"
			"   %lt = OpSLessThan %bool %15 %c_i32_128\n"
			"         OpLoopMerge %merge %inc None\n"
			"         OpBranchConditional %lt %write %merge\n"

			"%write = OpLabel\n"
			"   %30 = OpLoad %i32 %i\n"
			"  %src = OpAccessChain %up_v2i8 %ssbo8 %c_i32_0 %${arrayindex}\n"
			"%val8 = OpLoad %v2itype8 %src\n"
			"%val16 = ${convert} ${v2itype16} %val8\n"
			"  %dst = OpAccessChain %up_v2i16 %ssbo16 %c_i32_0 %30\n"
			"         OpStore %dst %val16\n"
			"         OpBranch %inc\n"

			"  %inc = OpLabel\n"
			"   %37 = OpLoad %i32 %i\n"
			"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
			"         OpStore %i %39\n"
			"         OpBranch %loop\n"
			"%merge = OpLabel\n"
			"         OpReturnValue %param\n"

			"OpFunctionEnd\n");

	struct Category
	{
		const char*				name;
		const StringTemplate&	preMain;
		const StringTemplate&	decoration;
		const StringTemplate&	testFunction;
		const deUint32			numElements;
	};

	const Category		categories[]	=
	{
		{"scalar", scalarPreMain,	scalarDecoration,	scalarTestFunc,	1},
		{"vector", vecPreMain,		vecDecoration,		vecTestFunc,	2},
	};

	for (deUint32 catIdx = 0; catIdx < DE_LENGTH_OF_ARRAY(categories); ++catIdx)
	{
		resources.inputs.clear();
		vector<deInt8>	inputs = getInt8s(rnd, (arrayStrideInBytesUniform / static_cast<deUint32>(sizeof(deInt8))) * (numDataPoints / categories[catIdx].numElements));
		resources.inputs.push_back(Resource(BufferSp(new Int8Buffer(inputs)), CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].dtype));
		for (deUint32 factIdx = 0; factIdx < DE_LENGTH_OF_ARRAY(intFacts); ++factIdx)
			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool				useConstIdx	= constantIndices[constIndexIdx].useConstantIndex;
				deUint32			constIdx	= constantIndices[constIndexIdx].constantIndex;
				map<string, string>	specs;
				VulkanFeatures		features;
				string				name		= string(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name) + "_" + categories[catIdx].name + "_" + intFacts[factIdx].name;

				specs["cap"]					= CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].cap;
				specs["itype16"]				= intFacts[factIdx].type16;
				specs["v2itype16"]				= "%v2" + string(intFacts[factIdx].type16).substr(1);
				specs["itype8"]					= intFacts[factIdx].type8;
				if (intFacts[factIdx].isSigned)
					specs["signed"]				= "1";
				else
					specs["signed"]				= "0";
				specs["convert"]				= intFacts[factIdx].opcode;
				specs["constarrayidx"]			= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				fragments["pre_main"]			= categories[catIdx].preMain.specialize(specs);
				fragments["testfun"]			= categories[catIdx].testFunction.specialize(specs);
				fragments["capability"]			= capabilities.specialize(specs);
				fragments["decoration"]			= categories[catIdx].decoration.specialize(specs);

				if (useConstIdx)
					name += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int16Buffer(outputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
				if (useConstIdx)
				{
					switch (constantIndices[constIndexIdx].constantIndex)
					{
					case 0:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 2, 0>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 1, 0>;
						break;
					case 4:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 2, 4>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 1, 4>;
						break;
					case 5:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 2, 5>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 1, 5>;
						break;
					case 6:
						if (categories[catIdx].numElements == 2)
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 2, 6>;
						else
							resources.verifyIO = checkUniformsArrayConstNdx<deInt8, deInt16, 1, 6>;
						break;
					default:
						DE_FATAL("Impossible");
						break;
					};
				}
				else
				{
					if (categories[catIdx].numElements == 2)
						resources.verifyIO = checkUniformsArray<deInt8, deInt16, 2>;
					else
						resources.verifyIO = checkUniformsArray<deInt8, deInt16, 1>;
				}

				features												= get8BitStorageFeatures(CAPABILITIES[UNIFORM_AND_STORAGEBUFFER_TEST].name);
				features.ext16BitStorage								= EXT16BITSTORAGEFEATURES_UNIFORM;
				features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
				features.coreFeatures.fragmentStoresAndAtomics			= true;

				createTestsForAllStages(name, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
	}
}

void addGraphics8BitStoragePushConstantInt8To16Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	RGBA								defaultColors[4];
	const deUint32						numDataPoints		= 64;
	vector<deInt8>						inputs				= getInt8s(rnd, numDataPoints);
	vector<deInt16>						sOutputs;
	vector<deInt16>						uOutputs;
	PushConstants						pcs;
	GraphicsResources					resources;
	vector<string>						extensions;
	const deUint8						signBitMask			= 0x80;
	const deUint16						signExtendMask		= 0xff00;
	VulkanFeatures						requiredFeatures;

	struct ConstantIndex
	{
		bool		useConstantIndex;
		deUint32	constantIndex;
	};

	ConstantIndex	constantIndices[] =
	{
		{ false,	0 },
		{ true,		4 },
		{ true,		5 },
		{ true,		6 }
	};

	sOutputs.reserve(inputs.size());
	uOutputs.reserve(inputs.size());

	for (deUint32 numNdx = 0; numNdx < inputs.size(); ++numNdx)
	{
		uOutputs.push_back(static_cast<deUint8>(inputs[numNdx]));
		if (inputs[numNdx] & signBitMask)
			sOutputs.push_back(static_cast<deInt16>(inputs[numNdx] | signExtendMask));
		else
			sOutputs.push_back(static_cast<deInt16>(inputs[numNdx]));
	}

	extensions.push_back("VK_KHR_8bit_storage");
	extensions.push_back("VK_KHR_16bit_storage");

	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	requiredFeatures.ext8BitStorage									= EXT8BITSTORAGEFEATURES_PUSH_CONSTANT;
	requiredFeatures.ext16BitStorage								= EXT16BITSTORAGEFEATURES_UNIFORM;

	fragments["capability"]				= "OpCapability StoragePushConstant8\n"
										  "OpCapability StorageUniform16\n";
	fragments["extension"]				= "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
										  "OpExtension \"SPV_KHR_8bit_storage\"\n"
										  "OpExtension \"SPV_KHR_16bit_storage\"\n";

	pcs.setPushConstant(BufferSp(new Int8Buffer(inputs)));

	getDefaultColors(defaultColors);

	const StringTemplate	testFun		(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"    %i = OpVariable %fp_i32 Function\n"
		"         OpStore %i %c_i32_0\n"
		"         OpBranch %loop\n"

		" %loop = OpLabel\n"
		"   %15 = OpLoad %i32 %i\n"
		"   %lt = OpSLessThan %bool %15 %c_i32_${count}\n"
		"         OpLoopMerge %merge %inc None\n"
		"         OpBranchConditional %lt %write %merge\n"

		"%write = OpLabel\n"
		"   %30 = OpLoad %i32 %i\n"
		"  %src = OpAccessChain %pp_${type8} %pc8 %c_i32_0 %${arrayindex}\n"
		"%val8 = OpLoad %${type8} %src\n"
		"%val16 = ${convert} %${type16} %val8\n"
		"  %dst = OpAccessChain %up_${type16} %ssbo16 %c_i32_0 %30\n"
		"         OpStore %dst %val16\n"
		"         OpBranch %inc\n"

		"  %inc = OpLabel\n"
		"   %37 = OpLoad %i32 %i\n"
		"   %39 = OpIAdd %i32 %37 %c_i32_1\n"
		"         OpStore %i %39\n"
		"         OpBranch %loop\n"

		"%merge = OpLabel\n"
		"         OpReturnValue %param\n"

		"OpFunctionEnd\n");

	{  // Scalar cases
		const StringTemplate	preMain		(
			"         %${type8} = OpTypeInt 8 ${signed}\n"
			"         %${type16} = OpTypeInt 16 ${signed}\n"
			"    %c_i32_${count} = OpConstant %i32 ${count}\n"					// Should be the same as numDataPoints
			"         %c_i32_ci = OpConstant %i32 ${constarrayidx}\n"
			"%a${count}${type8} = OpTypeArray %${type8} %c_i32_${count}\n"
			"%a${count}${type16} = OpTypeArray %${type16} %c_i32_${count}\n"
			"      %pp_${type8} = OpTypePointer PushConstant %${type8}\n"
			"      %up_${type16} = OpTypePointer StorageBuffer      %${type16}\n"
			"            %SSBO16 = OpTypeStruct %a${count}${type16}\n"
			"         %up_SSBO16 = OpTypePointer StorageBuffer %SSBO16\n"
			"            %ssbo16 = OpVariable %up_SSBO16 StorageBuffer\n"
			"              %PC8 = OpTypeStruct %a${count}${type8}\n"
			"           %pp_PC8 = OpTypePointer PushConstant %PC8\n"
			"              %pc8 = OpVariable %pp_PC8 PushConstant\n");

		const StringTemplate	decoration	(
			"OpDecorate %a${count}${type8} ArrayStride 1\n"
			"OpDecorate %a${count}${type16} ArrayStride 2\n"
			"OpDecorate %SSBO16 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpDecorate %PC8 Block\n"
			"OpMemberDecorate %PC8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 0\n");

		{  // signed int
			map<string, string>		specs;

			specs["type8"]			= "i8";
			specs["type16"]			= "i16";
			specs["signed"]			= "1";
			specs["count"]			= "64";
			specs["convert"]		= "OpSConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "sint_scalar";
				vector<deInt16>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(sOutputs[constIdx]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int16Buffer(useConstIdx ? constIdxData : sOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
		{  // signed int
			map<string, string>		specs;

			specs["type8"]			= "u8";
			specs["type16"]			= "u16";
			specs["signed"]			= "0";
			specs["count"]			= "64";
			specs["convert"]		= "OpUConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "uint_scalar";
				vector<deInt16>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(uOutputs[constIdx]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int16Buffer(useConstIdx ? constIdxData : uOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
	}

	{  // Vector cases
		const StringTemplate	preMain		(
			"    %${base_type8} = OpTypeInt 8 ${signed}\n"
			"         %${type8} = OpTypeVector %${base_type8} 2\n"
			"    %${base_type16} = OpTypeInt 16 ${signed}\n"
			"         %${type16} = OpTypeVector %${base_type16} 2\n"
			"    %c_i32_${count} = OpConstant %i32 ${count}\n"
			"          %c_i32_ci = OpConstant %i32 ${constarrayidx}\n"
			"%a${count}${type8} = OpTypeArray %${type8} %c_i32_${count}\n"
			"%a${count}${type16} = OpTypeArray %${type16} %c_i32_${count}\n"
			"      %pp_${type8} = OpTypePointer PushConstant %${type8}\n"
			"      %up_${type16} = OpTypePointer StorageBuffer      %${type16}\n"
			"            %SSBO16 = OpTypeStruct %a${count}${type16}\n"
			"         %up_SSBO16 = OpTypePointer StorageBuffer %SSBO16\n"
			"            %ssbo16 = OpVariable %up_SSBO16 StorageBuffer\n"
			"              %PC8 = OpTypeStruct %a${count}${type8}\n"
			"           %pp_PC8 = OpTypePointer PushConstant %PC8\n"
			"              %pc8 = OpVariable %pp_PC8 PushConstant\n");

		const StringTemplate	decoration	(
			"OpDecorate %a${count}${type8} ArrayStride 2\n"
			"OpDecorate %a${count}${type16} ArrayStride 4\n"
			"OpDecorate %SSBO16 Block\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpDecorate %PC8 Block\n"
			"OpMemberDecorate %PC8 0 Offset 0\n"
			"OpDecorate %ssbo16 DescriptorSet 0\n"
			"OpDecorate %ssbo16 Binding 0\n");

		{  // signed int
			map<string, string>		specs;

			specs["base_type8"]		= "i8";
			specs["base_type16"]	= "i16";
			specs["type8"]			= "v2i8";
			specs["type16"]			= "v2i16";
			specs["signed"]			= "1";
			specs["count"]			= "32";				// 64 / 2
			specs["convert"]		= "OpSConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "sint_vector";
				vector<deInt16>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(sOutputs[constIdx * 2 + numIdx % 2]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int16Buffer(useConstIdx ? constIdxData : sOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
		{  // signed int
			map<string, string>		specs;

			specs["base_type8"]		= "u8";
			specs["base_type16"]	= "u16";
			specs["type8"]			= "v2u8";
			specs["type16"]			= "v2u16";
			specs["signed"]			= "0";
			specs["count"]			= "32";
			specs["convert"]		= "OpUConvert";

			for (deUint32 constIndexIdx = 0; constIndexIdx < DE_LENGTH_OF_ARRAY(constantIndices); ++constIndexIdx)
			{
				bool			useConstIdx		= constantIndices[constIndexIdx].useConstantIndex;
				deUint32		constIdx		= constantIndices[constIndexIdx].constantIndex;
				string			testName		= "uint_vector";
				vector<deInt16>	constIdxData;

				if (useConstIdx)
				{
					constIdxData.reserve(numDataPoints);

					for (deUint32 numIdx = 0; numIdx < numDataPoints; ++numIdx)
						constIdxData.push_back(uOutputs[constIdx * 2 + numIdx % 2]);
				}

				specs["constarrayidx"]	= de::toString(constIdx);
				if (useConstIdx)
					specs["arrayindex"] = "c_i32_ci";
				else
					specs["arrayindex"] = "30";

				if (useConstIdx)
					testName += string("_const_idx_") + de::toString(constIdx);

				resources.outputs.clear();
				resources.outputs.push_back(Resource(BufferSp(new Int16Buffer(useConstIdx ? constIdxData : uOutputs)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

				fragments["testfun"]	= testFun.specialize(specs);
				fragments["pre_main"]	= preMain.specialize(specs);
				fragments["decoration"]	= decoration.specialize(specs);

				createTestsForAllStages(testName.c_str(), defaultColors, defaultColors, fragments, pcs, resources, extensions, testGroup, requiredFeatures);
			}
		}
	}
}

void addGraphics8BitStorageUniformStruct8To32Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	vector<string>						extensions;
	RGBA								defaultColors[4];
	const StringTemplate				capabilities		("OpCapability ${cap}\n");
	vector<deInt32>						i32Data				= data32bit(SHADERTEMPLATE_STRIDE32BIT_STD430, rnd, false);

	extensions.push_back("VK_KHR_8bit_storage");
	fragments["extension"]	= "OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	getDefaultColors(defaultColors);

	const StringTemplate preMain		(
		"\n"
		"%i8      = OpTypeInt 8 ${signed}\n"
		"%v2i8    = OpTypeVector %i8 2\n"
		"%v3i8    = OpTypeVector %i8 3\n"
		"%v4i8    = OpTypeVector %i8 4\n"
		"%i8ptr   = OpTypePointer ${8Storage} %i8\n"
		"%v2i8ptr = OpTypePointer ${8Storage} %v2i8\n"
		"%v3i8ptr = OpTypePointer ${8Storage} %v3i8\n"
		"%v4i8ptr = OpTypePointer ${8Storage} %v4i8\n"
		"\n"
		"%i32ptr   = OpTypePointer ${32Storage} %${32type}\n"
		"%v2i32ptr = OpTypePointer ${32Storage} %v2${32type}\n"
		"%v3i32ptr = OpTypePointer ${32Storage} %v3${32type}\n"
		"%v4i32ptr = OpTypePointer ${32Storage} %v4${32type}\n"
		"\n"
		"%zero = OpConstant %i32 0\n"
		"%c_i32_5  = OpConstant %i32 5\n"
		"%c_i32_6  = OpConstant %i32 6\n"
		"%c_i32_7  = OpConstant %i32 7\n"
		"%c_i32_8  = OpConstant %i32 8\n"
		"%c_i32_9  = OpConstant %i32 9\n"
		"%c_i32_11 = OpConstant %i32 11\n"
		"\n"
		"%c_u32_7 = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"
		"%i8arr3       = OpTypeArray %i8 %c_u32_3\n"
		"%v2i8arr3     = OpTypeArray %v2i8 %c_u32_3\n"
		"%v2i8arr11    = OpTypeArray %v2i8 %c_u32_11\n"
		"%v3i8arr11    = OpTypeArray %v3i8 %c_u32_11\n"
		"%v4i8arr3     = OpTypeArray %v4i8 %c_u32_3\n"
		"%struct8      = OpTypeStruct %i8 %v2i8arr3\n"
		"%struct8arr11 = OpTypeArray %struct8 %c_u32_11\n"
		"%i8Struct = OpTypeStruct %i8 %v2i8 %v3i8 %v4i8 %i8arr3 %struct8arr11 %v2i8arr11 %i8 %v3i8arr11 %v4i8arr3\n"
		"\n"
		"%i32arr3       = OpTypeArray %${32type} %c_u32_3\n"
		"%v2i32arr3     = OpTypeArray %v2${32type} %c_u32_3\n"
		"%v2i32arr11    = OpTypeArray %v2${32type} %c_u32_11\n"
		"%v3i32arr11    = OpTypeArray %v3${32type} %c_u32_11\n"
		"%v4i32arr3     = OpTypeArray %v4${32type} %c_u32_3\n"
		"%struct32      = OpTypeStruct %${32type} %v2i32arr3\n"
		"%struct32arr11 = OpTypeArray %struct32 %c_u32_11\n"
		"%i32Struct = OpTypeStruct %${32type} %v2${32type} %v3${32type} %v4${32type} %i32arr3 %struct32arr11 %v2i32arr11 %${32type} %v3i32arr11 %v4i32arr3\n"
		"\n"
		"%i8StructArr7  = OpTypeArray %i8Struct %c_u32_7\n"
		"%i32StructArr7 = OpTypeArray %i32Struct %c_u32_7\n"
		"%SSBO_IN       = OpTypeStruct %i8StructArr7\n"
		"%SSBO_OUT      = OpTypeStruct %i32StructArr7\n"
		"%up_SSBOIN     = OpTypePointer ${8Storage} %SSBO_IN\n"
		"%up_SSBOOUT    = OpTypePointer ${32Storage} %SSBO_OUT\n"
		"%ssboIN        = OpVariable %up_SSBOIN ${8Storage}\n"
		"%ssboOUT       = OpVariable %up_SSBOOUT ${32Storage}\n"
		"\n");

	const StringTemplate decoration		(
		"${stridei8}"
		"\n"
		"${stridei32}"
		"\n"
		"OpDecorate %SSBO_IN Block\n"
		"OpDecorate %SSBO_OUT Block\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"OpDecorate %ssboOUT Binding 1\n"
		"\n");

	const StringTemplate testFun		(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%loopNdx   = OpVariable %fp_i32 Function\n"
		"%insideLoopNdx = OpVariable %fp_i32 Function\n"

		"OpStore %loopNdx %zero\n"
		"OpBranch %loop\n"
		"%loop = OpLabel\n"
		"OpLoopMerge %merge %13 None\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%valLoopNdx = OpLoad %i32 %loopNdx\n"
		"%18 = OpSLessThan %bool %valLoopNdx %c_i32_7\n"
		"OpBranchConditional %18 %11 %merge\n"
		"%11 = OpLabel\n"
		"\n"
		"%i8src  = OpAccessChain %i8ptr %ssboIN %zero %valLoopNdx %zero\n"
		"%val_i8 = OpLoad %i8 %i8src\n"
		"%val_i32 = ${convert} %${32type} %val_i8\n"
		"%i32dst  = OpAccessChain %i32ptr %ssboOUT %zero %valLoopNdx %zero\n"
		"OpStore %i32dst %val_i32\n"
		"\n"
		"%v2i8src  = OpAccessChain %v2i8ptr %ssboIN %zero %valLoopNdx %c_i32_1\n"
		"%val_v2i8 = OpLoad %v2i8 %v2i8src\n"
		"%val_v2i32 = ${convert} %v2${32type} %val_v2i8\n"
		"%v2i32dst  = OpAccessChain %v2i32ptr %ssboOUT %zero %valLoopNdx %c_i32_1\n"
		"OpStore %v2i32dst %val_v2i32\n"
		"\n"
		"%v3i8src  = OpAccessChain %v3i8ptr %ssboIN %zero %valLoopNdx %c_i32_2\n"
		"%val_v3i8 = OpLoad %v3i8 %v3i8src\n"
		"%val_v3i32 = ${convert} %v3${32type} %val_v3i8\n"
		"%v3i32dst  = OpAccessChain %v3i32ptr %ssboOUT %zero %valLoopNdx %c_i32_2\n"
		"OpStore %v3i32dst %val_v3i32\n"
		"\n"
		"%v4i8src  = OpAccessChain %v4i8ptr %ssboIN %zero %valLoopNdx %c_i32_3\n"
		"%val_v4i8 = OpLoad %v4i8 %v4i8src\n"
		"%val_v4i32 = ${convert} %v4${32type} %val_v4i8\n"
		"%v4i32dst  = OpAccessChain %v4i32ptr %ssboOUT %zero %valLoopNdx %c_i32_3\n"
		"OpStore %v4i32dst %val_v4i32\n"
		"\n"
		"%i8src2  = OpAccessChain %i8ptr %ssboIN %zero %valLoopNdx %c_i32_7\n"
		"%val2_i8 = OpLoad %i8 %i8src2\n"
		"%val2_i32 = ${convert} %${32type} %val2_i8\n"
		"%i32dst2  = OpAccessChain %i32ptr %ssboOUT %zero %valLoopNdx %c_i32_7\n"
		"OpStore %i32dst2 %val2_i32\n"
		"\n"
		"OpStore %insideLoopNdx %zero\n"
		"OpBranch %loopInside\n"
		"%loopInside = OpLabel\n"
		"OpLoopMerge %92 %93 None\n"
		"OpBranch %94\n"
		"%94 = OpLabel\n"
		"%valInsideLoopNdx = OpLoad %i32 %insideLoopNdx\n"
		"%96 = OpSLessThan %bool %valInsideLoopNdx %c_i32_11\n"
		"OpBranchConditional %96 %91 %92\n"
		"\n"
		"%91 = OpLabel\n"
		"\n"
		"%v2i8src2  = OpAccessChain %v2i8ptr %ssboIN %zero %valLoopNdx %c_i32_6 %valInsideLoopNdx\n"
		"%val2_v2i8 = OpLoad %v2i8 %v2i8src2\n"
		"%val2_v2i32 = ${convert} %v2${32type} %val2_v2i8\n"
		"%v2i32dst2  = OpAccessChain %v2i32ptr %ssboOUT %zero %valLoopNdx %c_i32_6 %valInsideLoopNdx\n"
		"OpStore %v2i32dst2 %val2_v2i32\n"
		"\n"
		"%v3i8src2  = OpAccessChain %v3i8ptr %ssboIN %zero %valLoopNdx %c_i32_8 %valInsideLoopNdx\n"
		"%val2_v3i8 = OpLoad %v3i8 %v3i8src2\n"
		"%val2_v3i32 = ${convert} %v3${32type} %val2_v3i8\n"
		"%v3i32dst2  = OpAccessChain %v3i32ptr %ssboOUT %zero %valLoopNdx %c_i32_8 %valInsideLoopNdx\n"
		"OpStore %v3i32dst2 %val2_v3i32\n"
		"\n"
		//struct {i8, v2i8[3]}
		"%Si8src  = OpAccessChain %i8ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %zero\n"
		"%Sval_i8 = OpLoad %i8 %Si8src\n"
		"%Sval_i32 = ${convert} %${32type} %Sval_i8\n"
		"%Si32dst2  = OpAccessChain %i32ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %zero\n"
		"OpStore %Si32dst2 %Sval_i32\n"
		"\n"
		"%Sv2i8src0   = OpAccessChain %v2i8ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %zero\n"
		"%Sv2i8_0     = OpLoad %v2i8 %Sv2i8src0\n"
		"%Sv2i32_0     = ${convert} %v2${32type} %Sv2i8_0\n"
		"%Sv2i32dst_0  = OpAccessChain %v2i32ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %zero\n"
		"OpStore %Sv2i32dst_0 %Sv2i32_0\n"
		"\n"
		"%Sv2i8src1  = OpAccessChain %v2i8ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_1\n"
		"%Sv2i8_1 = OpLoad %v2i8 %Sv2i8src1\n"
		"%Sv2i32_1 = ${convert} %v2${32type} %Sv2i8_1\n"
		"%Sv2i32dst_1  = OpAccessChain %v2i32ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_1\n"
		"OpStore %Sv2i32dst_1 %Sv2i32_1\n"
		"\n"
		"%Sv2i8src2  = OpAccessChain %v2i8ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_2\n"
		"%Sv2i8_2 = OpLoad %v2i8 %Sv2i8src2\n"
		"%Sv2i32_2 = ${convert} %v2${32type} %Sv2i8_2\n"
		"%Sv2i32dst_2  = OpAccessChain %v2i32ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_2\n"
		"OpStore %Sv2i32dst_2 %Sv2i32_2\n"
		"\n"
		//Array with 3 elements
		"%LessThan3 = OpSLessThan %bool %valInsideLoopNdx %c_i32_3\n"
		"OpSelectionMerge %BlockIf None\n"
		"OpBranchConditional %LessThan3 %LabelIf %BlockIf\n"
		"%LabelIf = OpLabel\n"
		"  %i8src3  = OpAccessChain %i8ptr %ssboIN %zero %valLoopNdx %c_i32_4 %valInsideLoopNdx\n"
		"  %val3_i8 = OpLoad %i8 %i8src3\n"
		"  %val3_i32 = ${convert} %${32type} %val3_i8\n"
		"  %i32dst3  = OpAccessChain %i32ptr %ssboOUT %zero %valLoopNdx %c_i32_4 %valInsideLoopNdx\n"
		"  OpStore %i32dst3 %val3_i32\n"
		"\n"
		"  %v4i8src2  = OpAccessChain %v4i8ptr %ssboIN %zero %valLoopNdx %c_i32_9 %valInsideLoopNdx\n"
		"  %val2_v4i8 = OpLoad %v4i8 %v4i8src2\n"
		"  %val2_v4i32 = ${convert} %v4${32type} %val2_v4i8\n"
		"  %v4i32dst2  = OpAccessChain %v4i32ptr %ssboOUT %zero %valLoopNdx %c_i32_9 %valInsideLoopNdx\n"
		"  OpStore %v4i32dst2 %val2_v4i32\n"
		"OpBranch %BlockIf\n"
		"%BlockIf = OpLabel\n"
		"\n"
		"OpBranch %93\n"
		"%93 = OpLabel\n"
		"%132 = OpLoad %i32 %insideLoopNdx\n"
		"%133 = OpIAdd %i32 %132 %c_i32_1\n"
		"OpStore %insideLoopNdx %133\n"
		"OpBranch %loopInside\n"
		"\n"
		"%92 = OpLabel\n"
		"OpBranch %13\n"
		"%13 = OpLabel\n"
		"%134 = OpLoad %i32 %loopNdx\n"
		"%135 = OpIAdd %i32 %134 %c_i32_1\n"
		"OpStore %loopNdx %135\n"
		"OpBranch %loop\n"

		"%merge = OpLabel\n"
		"         OpReturnValue %param\n"
		"         OpFunctionEnd\n");

	struct IntegerFacts
	{
		const char*	name;
		const char* opcode;
		const char*	signedInt;
		const char*	type32;
	};

	const IntegerFacts	intFacts[]	=
	{
		{"sint",	"OpSConvert",	"1", "i32"},
		{"uint",	"OpUConvert",	"0", "u32"},
	};

	for (deUint32 capIdx = 0; capIdx < DE_LENGTH_OF_ARRAY(CAPABILITIES); ++capIdx)
		for (deUint32 intFactsNdx = 0; intFactsNdx < DE_LENGTH_OF_ARRAY(intFacts); ++intFactsNdx)
		{
			const bool				isUniform	= (VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER == CAPABILITIES[capIdx].dtype);
			vector<deInt8>			i8Data		= isUniform ? data8bit(SHADERTEMPLATE_STRIDE8BIT_STD140, rnd) : data8bit(SHADERTEMPLATE_STRIDE8BIT_STD430, rnd);
			GraphicsResources		resources;
			map<string, string>		specs;
			VulkanFeatures			features;
			const string			testName	= string(CAPABILITIES[capIdx].name) + "_" + intFacts[intFactsNdx].name;

			specs["cap"]						= CAPABILITIES[capIdx].cap;
			specs["stridei8"]					= getStructShaderComponet(isUniform ? SHADERTEMPLATE_STRIDE8BIT_STD140 : SHADERTEMPLATE_STRIDE8BIT_STD430);
			specs["stridei32"]					= getStructShaderComponet(SHADERTEMPLATE_STRIDE32BIT_STD430);
			specs["32Storage"]					= "StorageBuffer";
			specs["8Storage"]					= isUniform ? "Uniform" : "StorageBuffer";
			specs["signed"]						= intFacts[intFactsNdx].signedInt;
			specs["convert"]					= intFacts[intFactsNdx].opcode;
			specs["32type"]						= intFacts[intFactsNdx].type32;

			fragments["capability"]				= capabilities.specialize(specs);
			fragments["decoration"]				= decoration.specialize(specs);
			fragments["pre_main"]				= preMain.specialize(specs);
			fragments["testfun"]				= testFun.specialize(specs);

			resources.inputs.push_back(Resource(BufferSp(new Int8Buffer(i8Data)), CAPABILITIES[capIdx].dtype));
			resources.outputs.push_back(Resource(BufferSp(new Int32Buffer(i32Data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			if (isUniform)
				resources.verifyIO = checkStruct<deInt8, deInt32, SHADERTEMPLATE_STRIDE8BIT_STD140, SHADERTEMPLATE_STRIDE32BIT_STD430>;
			else
				resources.verifyIO = checkStruct<deInt8, deInt32, SHADERTEMPLATE_STRIDE8BIT_STD430, SHADERTEMPLATE_STRIDE32BIT_STD430>;

			features												= get8BitStorageFeatures(CAPABILITIES[capIdx].name);
			features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
			features.coreFeatures.fragmentStoresAndAtomics			= true;

			createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
}

void addGraphics8BitStorageUniformStruct32To8Group (tcu::TestCaseGroup* testGroup)
{
	de::Random							rnd					(deStringHash(testGroup->getName()));
	map<string, string>					fragments;
	vector<string>						extensions;
	RGBA								defaultColors[4];
	const StringTemplate				capabilities		("OpCapability ${cap}\n");
	vector<deInt8>						i8Data				= data8bit(SHADERTEMPLATE_STRIDE8BIT_STD430, rnd, false);

	extensions.push_back("VK_KHR_8bit_storage");
	fragments["extension"]	= "OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	getDefaultColors(defaultColors);

	const StringTemplate preMain		(
		"\n"
		"%i8      = OpTypeInt 8 ${signed}\n"
		"%v2i8    = OpTypeVector %i8 2\n"
		"%v3i8    = OpTypeVector %i8 3\n"
		"%v4i8    = OpTypeVector %i8 4\n"
		"%i8ptr   = OpTypePointer ${8Storage} %i8\n"
		"%v2i8ptr = OpTypePointer ${8Storage} %v2i8\n"
		"%v3i8ptr = OpTypePointer ${8Storage} %v3i8\n"
		"%v4i8ptr = OpTypePointer ${8Storage} %v4i8\n"
		"\n"
		"%i32ptr   = OpTypePointer ${32Storage} %${32type}\n"
		"%v2i32ptr = OpTypePointer ${32Storage} %v2${32type}\n"
		"%v3i32ptr = OpTypePointer ${32Storage} %v3${32type}\n"
		"%v4i32ptr = OpTypePointer ${32Storage} %v4${32type}\n"
		"\n"
		"%zero = OpConstant %i32 0\n"
		"%c_i32_5  = OpConstant %i32 5\n"
		"%c_i32_6  = OpConstant %i32 6\n"
		"%c_i32_7  = OpConstant %i32 7\n"
		"%c_i32_8  = OpConstant %i32 8\n"
		"%c_i32_9  = OpConstant %i32 9\n"
		"%c_i32_11 = OpConstant %i32 11\n"
		"\n"
		"%c_u32_7 = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"
		"%i8arr3       = OpTypeArray %i8 %c_u32_3\n"
		"%v2i8arr3    = OpTypeArray %v2i8 %c_u32_3\n"
		"%v2i8arr11    = OpTypeArray %v2i8 %c_u32_11\n"
		"%v3i8arr11    = OpTypeArray %v3i8 %c_u32_11\n"
		"%v4i8arr3     = OpTypeArray %v4i8 %c_u32_3\n"
		"%struct8      = OpTypeStruct %i8 %v2i8arr3\n"
		"%struct8arr11 = OpTypeArray %struct8 %c_u32_11\n"
		"%i8Struct = OpTypeStruct %i8 %v2i8 %v3i8 %v4i8 %i8arr3 %struct8arr11 %v2i8arr11 %i8 %v3i8arr11 %v4i8arr3\n"
		"\n"
		"%i32arr3       = OpTypeArray %${32type} %c_u32_3\n"
		"%v2i32arr3     = OpTypeArray %v2${32type} %c_u32_3\n"
		"%v2i32arr11    = OpTypeArray %v2${32type} %c_u32_11\n"
		"%v3i32arr11    = OpTypeArray %v3${32type} %c_u32_11\n"
		"%v4i32arr3     = OpTypeArray %v4${32type} %c_u32_3\n"
		"%struct32      = OpTypeStruct %${32type} %v2i32arr3\n"
		"%struct32arr11 = OpTypeArray %struct32 %c_u32_11\n"
		"%i32Struct = OpTypeStruct %${32type} %v2${32type} %v3${32type} %v4${32type} %i32arr3 %struct32arr11 %v2i32arr11 %${32type} %v3i32arr11 %v4i32arr3\n"
		"\n"
		"%i8StructArr7  = OpTypeArray %i8Struct %c_u32_7\n"
		"%i32StructArr7 = OpTypeArray %i32Struct %c_u32_7\n"
		"%SSBO_IN       = OpTypeStruct %i32StructArr7\n"
		"%SSBO_OUT      = OpTypeStruct %i8StructArr7\n"
		"%up_SSBOIN     = OpTypePointer ${32Storage} %SSBO_IN\n"
		"%up_SSBOOUT    = OpTypePointer ${8Storage} %SSBO_OUT\n"
		"%ssboIN        = OpVariable %up_SSBOIN ${32Storage}\n"
		"%ssboOUT       = OpVariable %up_SSBOOUT ${8Storage}\n"
		"\n");

	const StringTemplate decoration		(
		"${stridei8}"
		"\n"
		"${stridei32}"
		"\n"
		"OpDecorate %SSBO_IN Block\n"
		"OpDecorate %SSBO_OUT Block\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"OpDecorate %ssboOUT Binding 1\n"
		"\n");

	const StringTemplate testFun		(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%loopNdx    = OpVariable %fp_i32 Function\n"
		"%insideLoopNdx = OpVariable %fp_i32 Function\n"

		"OpStore %loopNdx %zero\n"
		"OpBranch %loop\n"
		"%loop = OpLabel\n"
		"OpLoopMerge %merge %13 None\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%valLoopNdx = OpLoad %i32 %loopNdx\n"
		"%18 = OpSLessThan %bool %valLoopNdx %c_i32_7\n"
		"OpBranchConditional %18 %11 %merge\n"
		"%11 = OpLabel\n"
		"\n"
		"%i32src  = OpAccessChain %i32ptr %ssboIN %zero %valLoopNdx %zero\n"
		"%val_i32 = OpLoad %${32type} %i32src\n"
		"%val_i8 = ${convert} %i8 %val_i32\n"
		"%i8dst  = OpAccessChain %i8ptr %ssboOUT %zero %valLoopNdx %zero\n"
		"OpStore %i8dst %val_i8\n"
		"\n"
		"%v2i32src  = OpAccessChain %v2i32ptr %ssboIN %zero %valLoopNdx %c_i32_1\n"
		"%val_v2i32 = OpLoad %v2${32type} %v2i32src\n"
		"%val_v2i8 = ${convert} %v2i8 %val_v2i32\n"
		"%v2i8dst  = OpAccessChain %v2i8ptr %ssboOUT %zero %valLoopNdx %c_i32_1\n"
		"OpStore %v2i8dst %val_v2i8\n"
		"\n"
		"%v3i32src  = OpAccessChain %v3i32ptr %ssboIN %zero %valLoopNdx %c_i32_2\n"
		"%val_v3i32 = OpLoad %v3${32type} %v3i32src\n"
		"%val_v3i8 = ${convert} %v3i8 %val_v3i32\n"
		"%v3i8dst  = OpAccessChain %v3i8ptr %ssboOUT %zero %valLoopNdx %c_i32_2\n"
		"OpStore %v3i8dst %val_v3i8\n"
		"\n"
		"%v4i32src  = OpAccessChain %v4i32ptr %ssboIN %zero %valLoopNdx %c_i32_3\n"
		"%val_v4i32 = OpLoad %v4${32type} %v4i32src\n"
		"%val_v4i8 = ${convert} %v4i8 %val_v4i32\n"
		"%v4i8dst  = OpAccessChain %v4i8ptr %ssboOUT %zero %valLoopNdx %c_i32_3\n"
		"OpStore %v4i8dst %val_v4i8\n"
		"\n"
		"%i32src2  = OpAccessChain %i32ptr %ssboIN %zero %valLoopNdx %c_i32_7\n"
		"%val2_i32 = OpLoad %${32type} %i32src2\n"
		"%val2_i8 = ${convert} %i8 %val2_i32\n"
		"%i8dst2  = OpAccessChain %i8ptr %ssboOUT %zero %valLoopNdx %c_i32_7\n"
		"OpStore %i8dst2 %val2_i8\n"
		"\n"
		"OpStore %insideLoopNdx %zero\n"
		"OpBranch %loopInside\n"
		"%loopInside = OpLabel\n"
		"OpLoopMerge %92 %93 None\n"
		"OpBranch %94\n"
		"%94 = OpLabel\n"
		"%valInsideLoopNdx = OpLoad %i32 %insideLoopNdx\n"
		"%96 = OpSLessThan %bool %valInsideLoopNdx %c_i32_11\n"
		"OpBranchConditional %96 %91 %92\n"
		"\n"
		"%91 = OpLabel\n"
		"\n"
		//struct {i8, v2i8[3]}
		"%Si32src  = OpAccessChain %i32ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %zero\n"
		"%Sval_i32 = OpLoad %${32type} %Si32src\n"
		"%Sval_i8  = ${convert} %i8 %Sval_i32\n"
		"%Si8dst2  = OpAccessChain %i8ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %zero\n"
		"OpStore %Si8dst2 %Sval_i8\n"
		"\n"
		"%Sv2i32src0 = OpAccessChain %v2i32ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %zero\n"
		"%Sv2i32_0   = OpLoad %v2${32type} %Sv2i32src0\n"
		"%Sv2i8_0    = ${convert} %v2i8 %Sv2i32_0\n"
		"%Sv2i8dst_0 = OpAccessChain %v2i8ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %zero\n"
		"OpStore %Sv2i8dst_0 %Sv2i8_0\n"
		"\n"
		"%Sv2i32src1 = OpAccessChain %v2i32ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_1\n"
		"%Sv2i32_1   = OpLoad %v2${32type} %Sv2i32src1\n"
		"%Sv2i8_1    = ${convert} %v2i8 %Sv2i32_1\n"
		"%Sv2i8dst_1 = OpAccessChain %v2i8ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_1\n"
		"OpStore %Sv2i8dst_1 %Sv2i8_1\n"
		"\n"
		"%Sv2i32src2 = OpAccessChain %v2i32ptr %ssboIN %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_2\n"
		"%Sv2i32_2   = OpLoad %v2${32type} %Sv2i32src2\n"
		"%Sv2i8_2    = ${convert} %v2i8 %Sv2i32_2\n"
		"%Sv2i8dst_2 = OpAccessChain %v2i8ptr %ssboOUT %zero %valLoopNdx %c_i32_5 %valInsideLoopNdx %c_i32_1 %c_i32_2\n"
		"OpStore %Sv2i8dst_2 %Sv2i8_2\n"
		"\n"

		"%v2i32src2  = OpAccessChain %v2i32ptr %ssboIN %zero %valLoopNdx %c_i32_6 %valInsideLoopNdx\n"
		"%val2_v2i32 = OpLoad %v2${32type} %v2i32src2\n"
		"%val2_v2i8  = ${convert} %v2i8 %val2_v2i32\n"
		"%v2i8dst2   = OpAccessChain %v2i8ptr %ssboOUT %zero %valLoopNdx %c_i32_6 %valInsideLoopNdx\n"
		"OpStore %v2i8dst2 %val2_v2i8\n"
		"\n"
		"%v3i32src2  = OpAccessChain %v3i32ptr %ssboIN %zero %valLoopNdx %c_i32_8 %valInsideLoopNdx\n"
		"%val2_v3i32 = OpLoad %v3${32type} %v3i32src2\n"
		"%val2_v3i8  = ${convert} %v3i8 %val2_v3i32\n"
		"%v3i8dst2   = OpAccessChain %v3i8ptr %ssboOUT %zero %valLoopNdx %c_i32_8 %valInsideLoopNdx\n"
		"OpStore %v3i8dst2 %val2_v3i8\n"
		"\n"

		//Array with 3 elements
		"%LessThan3 = OpSLessThan %bool %valInsideLoopNdx %c_i32_3\n"
		"OpSelectionMerge %BlockIf None\n"
		"OpBranchConditional %LessThan3 %LabelIf %BlockIf\n"
		"  %LabelIf = OpLabel\n"
		"  %i32src3  = OpAccessChain %i32ptr %ssboIN %zero %valLoopNdx %c_i32_4 %valInsideLoopNdx\n"
		"  %val3_i32 = OpLoad %${32type} %i32src3\n"
		"  %val3_i8  = ${convert} %i8 %val3_i32\n"
		"  %i8dst3   = OpAccessChain %i8ptr %ssboOUT %zero %valLoopNdx %c_i32_4 %valInsideLoopNdx\n"
		"  OpStore %i8dst3 %val3_i8\n"
		"\n"
		"  %v4i32src2  = OpAccessChain %v4i32ptr %ssboIN %zero %valLoopNdx %c_i32_9 %valInsideLoopNdx\n"
		"  %val2_v4i32 = OpLoad %v4${32type} %v4i32src2\n"
		"  %val2_v4i8  = ${convert} %v4i8 %val2_v4i32\n"
		"  %v4i8dst2   = OpAccessChain %v4i8ptr %ssboOUT %zero %valLoopNdx %c_i32_9 %valInsideLoopNdx\n"
		"  OpStore %v4i8dst2 %val2_v4i8\n"
		"OpBranch %BlockIf\n"
		"%BlockIf = OpLabel\n"

		"OpBranch %93\n"
		"%93 = OpLabel\n"
		"%132 = OpLoad %i32 %insideLoopNdx\n"
		"%133 = OpIAdd %i32 %132 %c_i32_1\n"
		"OpStore %insideLoopNdx %133\n"
		"OpBranch %loopInside\n"
		"\n"
		"%92 = OpLabel\n"
		"OpBranch %13\n"
		"%13 = OpLabel\n"
		"%134 = OpLoad %i32 %loopNdx\n"
		"%135 = OpIAdd %i32 %134 %c_i32_1\n"
		"OpStore %loopNdx %135\n"
		"OpBranch %loop\n"

		"%merge = OpLabel\n"
		"         OpReturnValue %param\n"
		"         OpFunctionEnd\n");

	struct IntegerFacts
	{
		const char*	name;
		const char* opcode;
		const char*	signedInt;
		const char*	type32;
	};

	const IntegerFacts	intFacts[]	=
	{
		{"sint",	"OpSConvert",	"1", "i32"},
		{"uint",	"OpUConvert",	"0", "u32"},
	};

	for (deUint32 capIdx = 0; capIdx < DE_LENGTH_OF_ARRAY(CAPABILITIES); ++capIdx)
		for (deUint32 intFactsNdx = 0; intFactsNdx < DE_LENGTH_OF_ARRAY(intFacts); ++intFactsNdx)
		{
			const bool			isUniform	= (VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER == CAPABILITIES[capIdx].dtype);
			map<string, string>	specs;
			string				testName	= string(CAPABILITIES[capIdx].name) + "_" + intFacts[intFactsNdx].name;
			vector<deInt32>		i32Data		= isUniform ? data32bit(SHADERTEMPLATE_STRIDE32BIT_STD140, rnd) : data32bit(SHADERTEMPLATE_STRIDE32BIT_STD430, rnd);
			GraphicsResources	resources;
			VulkanFeatures		features;

			specs["cap"]					= CAPABILITIES[STORAGE_BUFFER_TEST].cap;
			specs["stridei8"]				= getStructShaderComponet(SHADERTEMPLATE_STRIDE8BIT_STD430);
			specs["stridei32"]				= getStructShaderComponet(isUniform ? SHADERTEMPLATE_STRIDE32BIT_STD140 : SHADERTEMPLATE_STRIDE32BIT_STD430);
			specs["8Storage"]				= "StorageBuffer";
			specs["32Storage"]				= isUniform ? "Uniform" : "StorageBuffer";
			specs["signed"]					= intFacts[intFactsNdx].signedInt;
			specs["convert"]				= intFacts[intFactsNdx].opcode;
			specs["32type"]					= intFacts[intFactsNdx].type32;

			fragments["capability"]			= capabilities.specialize(specs);
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFun.specialize(specs);

			resources.inputs.push_back(Resource(BufferSp(new Int32Buffer(i32Data)), CAPABILITIES[capIdx].dtype));
			resources.outputs.push_back(Resource(BufferSp(new Int8Buffer(i8Data)), CAPABILITIES[STORAGE_BUFFER_TEST].dtype));
			if (isUniform)
				resources.verifyIO = checkStruct<deInt32, deInt8, SHADERTEMPLATE_STRIDE32BIT_STD140, SHADERTEMPLATE_STRIDE8BIT_STD430>;
			else
				resources.verifyIO = checkStruct<deInt32, deInt8, SHADERTEMPLATE_STRIDE32BIT_STD430, SHADERTEMPLATE_STRIDE8BIT_STD430>;

			features												= get8BitStorageFeatures(CAPABILITIES[STORAGE_BUFFER_TEST].name);
			features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
			features.coreFeatures.fragmentStoresAndAtomics			= true;

			createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, extensions, testGroup, features);
		}
}

void addGraphics8bitStorage8bitStructMixedTypesGroup (tcu::TestCaseGroup* group)
{
	de::Random							rnd					(deStringHash(group->getName()));
	map<string, string>					fragments;
	vector<string>						extensions;
	RGBA								defaultColors[4];
	const StringTemplate				capabilities		("OpCapability StorageBuffer8BitAccess\n"
															"${cap}\n");
	vector<deInt8>						outData				= data8bit(SHADERTEMPLATE_STRIDEMIX_STD430, rnd, false);

	extensions.push_back("VK_KHR_8bit_storage");
	fragments["extension"]	= "OpExtension \"SPV_KHR_8bit_storage\"\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	getDefaultColors(defaultColors);

	const StringTemplate				preMain				(
		"\n"//Types
		"%i8    = OpTypeInt 8 1\n"
		"%v2i8  = OpTypeVector %i8 2\n"
		"%v3i8  = OpTypeVector %i8 3\n"
		"%v4i8  = OpTypeVector %i8 4\n"
		"\n"//Consta value
		"%zero     = OpConstant %i32 0\n"
		"%c_i32_5  = OpConstant %i32 5\n"
		"%c_i32_6  = OpConstant %i32 6\n"
		"%c_i32_7  = OpConstant %i32 7\n"
		"%c_i32_8  = OpConstant %i32 8\n"
		"%c_i32_9  = OpConstant %i32 9\n"
		"%c_i32_10 = OpConstant %i32 10\n"
		"%c_i32_11 = OpConstant %i32 11\n"
		"%c_u32_7  = OpConstant %u32 7\n"
		"%c_u32_11 = OpConstant %u32 11\n"
		"\n"//Arrays & Structs
		"%v2b8NestedArr11In  = OpTypeArray %v2i8 %c_u32_11\n"
		"%b32NestedArr11In   = OpTypeArray %i32 %c_u32_11\n"
		"%sb8Arr11In         = OpTypeArray %i8 %c_u32_11\n"
		"%sb32Arr11In        = OpTypeArray %i32 %c_u32_11\n"
		"%sNestedIn          = OpTypeStruct %i8 %i32 %v2b8NestedArr11In %b32NestedArr11In\n"
		"%sNestedArr11In     = OpTypeArray %sNestedIn %c_u32_11\n"
		"%structIn           = OpTypeStruct %i8 %i32 %v2i8 %v2i32 %v3i8 %v3i32 %v4i8 %v4i32 %sNestedArr11In %sb8Arr11In %sb32Arr11In\n"
		"%structArr7In       = OpTypeArray %structIn %c_u32_7\n"
		"%v2b8NestedArr11Out = OpTypeArray %v2i8 %c_u32_11\n"
		"%b32NestedArr11Out  = OpTypeArray %i32 %c_u32_11\n"
		"%sb8Arr11Out        = OpTypeArray %i8 %c_u32_11\n"
		"%sb32Arr11Out       = OpTypeArray %i32 %c_u32_11\n"
		"%sNestedOut         = OpTypeStruct %i8 %i32 %v2b8NestedArr11Out %b32NestedArr11Out\n"
		"%sNestedArr11Out    = OpTypeArray %sNestedOut %c_u32_11\n"
		"%structOut          = OpTypeStruct %i8 %i32 %v2i8 %v2i32 %v3i8 %v3i32 %v4i8 %v4i32 %sNestedArr11Out %sb8Arr11Out %sb32Arr11Out\n"
		"%structArr7Out      = OpTypeArray %structOut %c_u32_7\n"
		"\n"//Pointers
		"${uniformPtr}"
		"%i8outPtr    = OpTypePointer StorageBuffer %i8\n"
		"%v2i8outPtr  = OpTypePointer StorageBuffer %v2i8\n"
		"%v3i8outPtr  = OpTypePointer StorageBuffer %v3i8\n"
		"%v4i8outPtr  = OpTypePointer StorageBuffer %v4i8\n"
		"%i32outPtr   = OpTypePointer StorageBuffer %i32\n"
		"%v2i32outPtr = OpTypePointer StorageBuffer %v2i32\n"
		"%v3i32outPtr = OpTypePointer StorageBuffer %v3i32\n"
		"%v4i32outPtr = OpTypePointer StorageBuffer %v4i32\n"
		"%uvec3ptr = OpTypePointer Input %v3u32\n"
		"\n"//SSBO IN
		"%SSBO_IN    = OpTypeStruct %structArr7In\n"
		"%up_SSBOIN  = OpTypePointer ${inStorage} %SSBO_IN\n"
		"%ssboIN     = OpVariable %up_SSBOIN ${inStorage}\n"
		"\n"//SSBO OUT
		"%SSBO_OUT   = OpTypeStruct %structArr7Out\n"
		"%up_SSBOOUT = OpTypePointer StorageBuffer %SSBO_OUT\n"
		"%ssboOUT    = OpVariable %up_SSBOOUT StorageBuffer\n");

		const StringTemplate			decoration			(
		"${OutOffsets}"
		"${InOffsets}"
		"\n"//SSBO IN
		"OpDecorate %SSBO_IN Block\n"
		"OpMemberDecorate %SSBO_IN 0 Offset 0\n"
		"OpDecorate %ssboIN DescriptorSet 0\n"
		"OpDecorate %ssboIN Binding 0\n"
		"\n"//SSBO OUT
		"OpDecorate %SSBO_OUT Block\n"
		"OpMemberDecorate %SSBO_OUT 0 Offset 0\n"
		"OpDecorate %ssboOUT DescriptorSet 0\n"
		"OpDecorate %ssboOUT Binding 1\n");

		const StringTemplate			testFun				(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%ndxArrx   = OpVariable %fp_i32  Function\n"
		"%ndxArry   = OpVariable %fp_i32  Function\n"
		"%ndxArrz   = OpVariable %fp_i32  Function\n"
		"${xBeginLoop}"
		"\n"//strutOut.b8 = strutIn.b8
		"%inP1  = OpAccessChain %i8${inPtr} %ssboIN %zero %Valx %zero\n"
		"%inV1  = OpLoad %i8 %inP1\n"
		"%outP1 = OpAccessChain %i8outPtr %ssboOUT %zero %Valx %zero\n"
		"OpStore %outP1 %inV1\n"
		"\n"//strutOut.b32 = strutIn.b32
		"%inP2  = OpAccessChain %i32${inPtr} %ssboIN %zero %Valx %c_i32_1\n"
		"%inV2  = OpLoad %i32 %inP2\n"
		"%outP2 = OpAccessChain %i32outPtr %ssboOUT %zero %Valx %c_i32_1\n"
		"OpStore %outP2 %inV2\n"
		"\n"//strutOut.v2b8 = strutIn.v2b8
		"%inP3  = OpAccessChain %v2i8${inPtr} %ssboIN %zero %Valx %c_i32_2\n"
		"%inV3  = OpLoad %v2i8 %inP3\n"
		"%outP3 = OpAccessChain %v2i8outPtr %ssboOUT %zero %Valx %c_i32_2\n"
		"OpStore %outP3 %inV3\n"
		"\n"//strutOut.v2b32 = strutIn.v2b32
		"%inP4  = OpAccessChain %v2i32${inPtr} %ssboIN %zero %Valx %c_i32_3\n"
		"%inV4  = OpLoad %v2i32 %inP4\n"
		"%outP4 = OpAccessChain %v2i32outPtr %ssboOUT %zero %Valx %c_i32_3\n"
		"OpStore %outP4 %inV4\n"
		"\n"//strutOut.v3b8 = strutIn.v3b8
		"%inP5  = OpAccessChain %v3i8${inPtr} %ssboIN %zero %Valx %c_i32_4\n"
		"%inV5  = OpLoad %v3i8 %inP5\n"
		"%outP5 = OpAccessChain %v3i8outPtr %ssboOUT %zero %Valx %c_i32_4\n"
		"OpStore %outP5 %inV5\n"
		"\n"//strutOut.v3b32 = strutIn.v3b32
		"%inP6  = OpAccessChain %v3i32${inPtr} %ssboIN %zero %Valx %c_i32_5\n"
		"%inV6  = OpLoad %v3i32 %inP6\n"
		"%outP6 = OpAccessChain %v3i32outPtr %ssboOUT %zero %Valx %c_i32_5\n"
		"OpStore %outP6 %inV6\n"
		"\n"//strutOut.v4b8 = strutIn.v4b8
		"%inP7  = OpAccessChain %v4i8${inPtr} %ssboIN %zero %Valx %c_i32_6\n"
		"%inV7  = OpLoad %v4i8 %inP7\n"
		"%outP7 = OpAccessChain %v4i8outPtr %ssboOUT %zero %Valx %c_i32_6\n"
		"OpStore %outP7 %inV7\n"
		"\n"//strutOut.v4b32 = strutIn.v4b32
		"%inP8  = OpAccessChain %v4i32${inPtr} %ssboIN %zero %Valx %c_i32_7\n"
		"%inV8  = OpLoad %v4i32 %inP8\n"
		"%outP8 = OpAccessChain %v4i32outPtr %ssboOUT %zero %Valx %c_i32_7\n"
		"OpStore %outP8 %inV8\n"
		"${yBeginLoop}"
		"\n"//strutOut.b8[y] = strutIn.b8[y]
		"%inP9  = OpAccessChain %i8${inPtr} %ssboIN %zero %Valx %c_i32_9 %Valy\n"
		"%inV9  = OpLoad %i8 %inP9\n"
		"%outP9 = OpAccessChain %i8outPtr %ssboOUT %zero %Valx %c_i32_9 %Valy\n"
		"OpStore %outP9 %inV9\n"
		"\n"//strutOut.b32[y] = strutIn.b32[y]
		"%inP10  = OpAccessChain %i32${inPtr} %ssboIN %zero %Valx %c_i32_10 %Valy\n"
		"%inV10  = OpLoad %i32 %inP10\n"
		"%outP10 = OpAccessChain %i32outPtr %ssboOUT %zero %Valx %c_i32_10 %Valy\n"
		"OpStore %outP10 %inV10\n"
		"\n"//strutOut.strutNestedOut[y].b8 = strutIn.strutNestedIn[y].b8
		"%inP11 = OpAccessChain %i8${inPtr} %ssboIN %zero %Valx %c_i32_8 %Valy %zero\n"
		"%inV11 = OpLoad %i8 %inP11\n"
		"%outP11 = OpAccessChain %i8outPtr %ssboOUT %zero %Valx %c_i32_8 %Valy %zero\n"
		"OpStore %outP11 %inV11\n"
		"\n"//strutOut.strutNestedOut[y].b32 = strutIn.strutNestedIn[y].b32
		"%inP12 = OpAccessChain %i32${inPtr} %ssboIN %zero %Valx %c_i32_8 %Valy %c_i32_1\n"
		"%inV12 = OpLoad %i32 %inP12\n"
		"%outP12 = OpAccessChain %i32outPtr %ssboOUT %zero %Valx %c_i32_8 %Valy %c_i32_1\n"
		"OpStore %outP12 %inV12\n"
		"${zBeginLoop}"
		"\n"//strutOut.strutNestedOut[y].v2b8[valNdx] = strutIn.strutNestedIn[y].v2b8[valNdx]
		"%inP13  = OpAccessChain %v2i8${inPtr} %ssboIN %zero %Valx %c_i32_8 %Valy %c_i32_2 %Valz\n"
		"%inV13  = OpLoad %v2i8 %inP13\n"
		"%outP13 = OpAccessChain %v2i8outPtr %ssboOUT %zero %Valx %c_i32_8 %Valy %c_i32_2 %Valz\n"
		"OpStore %outP13 %inV13\n"
		"\n"//strutOut.strutNestedOut[y].b32[valNdx] = strutIn.strutNestedIn[y].b32[valNdx]
		"%inP14  = OpAccessChain %i32${inPtr} %ssboIN %zero %Valx %c_i32_8 %Valy %c_i32_3 %Valz\n"
		"%inV14  = OpLoad %i32 %inP14\n"
		"%outP14 = OpAccessChain %i32outPtr %ssboOUT %zero %Valx %c_i32_8 %Valy %c_i32_3 %Valz\n"
		"OpStore %outP14 %inV14\n"
		"${zEndLoop}"
		"${yEndLoop}"
		"${xEndLoop}"
		"\n"
		"OpBranch %ExitLabel\n"
		"%ExitLabel = OpLabel\n"
		"OpReturnValue %param\n"
		"OpFunctionEnd\n");

	for (deUint32 capIdx = 0; capIdx < DE_LENGTH_OF_ARRAY(CAPABILITIES); ++capIdx)
	{  // int
		const bool				isUniform	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER == CAPABILITIES[capIdx].dtype;
		vector<deInt8>			inData		= isUniform ? data8bit(SHADERTEMPLATE_STRIDEMIX_STD140, rnd) : data8bit(SHADERTEMPLATE_STRIDEMIX_STD430, rnd);
		GraphicsResources		resources;
		map<string, string>		specsLoop;
		map<string, string>		specsOffset;
		map<string, string>		specs;
		VulkanFeatures			features;
		string					testName	= string(CAPABILITIES[capIdx].name);

		specsLoop["exeCount"]	= "c_i32_7";
		specsLoop["loopName"]	= "x";
		specs["xBeginLoop"]		= beginLoop(specsLoop);
		specs["xEndLoop"]		= endLoop(specsLoop);

		specsLoop["exeCount"]	= "c_i32_11";
		specsLoop["loopName"]	= "y";
		specs["yBeginLoop"]		= beginLoop(specsLoop);
		specs["yEndLoop"]		= endLoop(specsLoop);

		specsLoop["exeCount"]	= "c_i32_11";
		specsLoop["loopName"]	= "z";
		specs["zBeginLoop"]		= beginLoop(specsLoop);
		specs["zEndLoop"]		= endLoop(specsLoop);

		specs["inStorage"]		= isUniform ? "Uniform" : "StorageBuffer";
		specs["cap"]			= isUniform ?"OpCapability " + string( CAPABILITIES[capIdx].cap) : "";
		specs["uniformPtr"]		= isUniform ?
								"%i8inPtr     = OpTypePointer Uniform %i8\n"
								"%v2i8inPtr   = OpTypePointer Uniform %v2i8\n"
								"%v3i8inPtr   = OpTypePointer Uniform %v3i8\n"
								"%v4i8inPtr   = OpTypePointer Uniform %v4i8\n"
								"%i32inPtr    = OpTypePointer Uniform %i32\n"
								"%v2i32inPtr  = OpTypePointer Uniform %v2i32\n"
								"%v3i32inPtr  = OpTypePointer Uniform %v3i32\n"
								"%v4i32inPtr  = OpTypePointer Uniform %v4i32\n" :
								"";
		specs["inPtr"]			= isUniform ? "inPtr" : "outPtr";
		specsOffset["InOut"]	= "In";
		specs["InOffsets"]		= StringTemplate(isUniform ? getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD140) : getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD430)).specialize(specsOffset);
		specsOffset["InOut"]	= "Out";
		specs["OutOffsets"]		= StringTemplate(getStructShaderComponet(SHADERTEMPLATE_STRIDEMIX_STD430)).specialize(specsOffset);

		fragments["capability"]	= capabilities.specialize(specs);
		fragments["decoration"]	= decoration.specialize(specs);
		fragments["pre_main"]	= preMain.specialize(specs);
		fragments["testfun"]	= testFun.specialize(specs);

		resources.verifyIO		= isUniform ? checkStruct<deInt8, deInt8, SHADERTEMPLATE_STRIDEMIX_STD140, SHADERTEMPLATE_STRIDEMIX_STD430> : checkStruct<deInt8, deInt8, SHADERTEMPLATE_STRIDEMIX_STD430, SHADERTEMPLATE_STRIDEMIX_STD430>;
		resources.inputs.push_back(Resource(BufferSp(new Int8Buffer(inData)), CAPABILITIES[capIdx].dtype));
		resources.outputs.push_back(Resource(BufferSp(new Int8Buffer(outData)), CAPABILITIES[STORAGE_BUFFER_TEST].dtype));

		features												= get8BitStorageFeatures(CAPABILITIES[capIdx].name);
		features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
		features.coreFeatures.fragmentStoresAndAtomics			= true;

		createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, extensions, group, features);
	}
}

} // anonymous

tcu::TestCaseGroup* create8BitStorageComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "8bit_storage", "Compute tests for VK_KHR_8bit_storage extension"));

	addTestGroup(group.get(), "storagebuffer_32_to_8",	"32bit ints to 8bit tests under capability StorageBuffer8BitAccess",			addCompute8bitStorage32To8Group);
	addTestGroup(group.get(), "uniform_8_to_32",		"8bit ints to 32bit tests under capability UniformAndStorageBuffer8BitAccess",	addCompute8bitUniform8To32Group);
	addTestGroup(group.get(), "push_constant_8_to_32",	"8bit ints to 32bit tests under capability StoragePushConstant8",				addCompute8bitStoragePushConstant8To32Group);

	addTestGroup(group.get(), "storagebuffer_16_to_8",	"16bit ints to 8bit tests under capability StorageBuffer8BitAccess",			addCompute8bitStorage16To8Group);
	addTestGroup(group.get(), "uniform_8_to_16",		"8bit ints to 16bit tests under capability UniformAndStorageBuffer8BitAccess",	addCompute8bitUniform8To16Group);
	addTestGroup(group.get(), "push_constant_8_to_16",	"8bit ints to 16bit tests under capability StoragePushConstant8",				addCompute8bitStoragePushConstant8To16Group);

	addTestGroup(group.get(), "uniform_8_to_8",			"8bit ints to 8bit tests under capability UniformAndStorageBuffer8BitAccess",	addCompute8bitStorageBuffer8To8Group);

	addTestGroup(group.get(), "uniform_8struct_to_32struct",		"8bit floats struct to 32bit tests under capability UniformAndStorageBuffer8BitAccess",	addCompute8bitStorageUniform8StructTo32StructGroup);
	addTestGroup(group.get(), "storagebuffer_32struct_to_8struct",	"32bit floats struct to 8bit tests under capability StorageBuffer8BitAccess",			addCompute8bitStorageUniform32StructTo8StructGroup);
	addTestGroup(group.get(), "struct_mixed_types",					"mixed type of 8bit and 32bit struct",													addCompute8bitStorage8bitStructMixedTypesGroup);

	return group.release();
}

tcu::TestCaseGroup* create8BitStorageGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "8bit_storage", "Graphics tests for VK_KHR_8bit_storage extension"));

	addTestGroup(group.get(), "storagebuffer_int_32_to_8",	"32-bit int into 8-bit tests under capability StorageBuffer8BitAccess",				addGraphics8BitStorageUniformInt32To8Group);
	addTestGroup(group.get(), "uniform_int_8_to_32",		"8-bit int into 32-bit tests under capability UniformAndStorageBuffer8BitAccess",	addGraphics8BitStorageUniformInt8To32Group);
	addTestGroup(group.get(), "push_constant_int_8_to_32",	"8-bit int into 32-bit tests under capability StoragePushConstant8",				addGraphics8BitStoragePushConstantInt8To32Group);

	addTestGroup(group.get(), "storagebuffer_int_16_to_8",	"16-bit int into 8-bit tests under capability StorageBuffer8BitAccess",				addGraphics8BitStorageUniformInt16To8Group);
	addTestGroup(group.get(), "uniform_int_8_to_16",		"8-bit int into 16-bit tests under capability UniformAndStorageBuffer8BitAccess",	addGraphics8BitStorageUniformInt8To16Group);
	addTestGroup(group.get(), "push_constant_int_8_to_16",	"8-bit int into 16-bit tests under capability StoragePushConstant8",				addGraphics8BitStoragePushConstantInt8To16Group);

	addTestGroup(group.get(), "8struct_to_32struct",		"8bit floats struct to 32bit tests ",												addGraphics8BitStorageUniformStruct8To32Group);
	addTestGroup(group.get(), "32struct_to_8struct",		"32bit floats struct to 8bit tests ",												addGraphics8BitStorageUniformStruct32To8Group);
	addTestGroup(group.get(), "struct_mixed_types",			"mixed type of 8bit and 32bit struc",												addGraphics8bitStorage8bitStructMixedTypesGroup);

	return group.release();
}

} // SpirVAssembly
} // vkt
