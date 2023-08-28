/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief SSBO layout tests.
 *//*--------------------------------------------------------------------*/

#include "vktSSBOLayoutTests.hpp"
#include "vktSSBOLayoutCase.hpp"
#include "vktSSBOCornerCase.hpp"

#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include <array>
#include <utility>

namespace vkt
{
namespace ssbo
{
namespace
{

using std::string;
using std::vector;
using std::array;
using std::pair;
using glu::VarType;
using glu::StructType;
using namespace vk;

enum FeatureBits
{
	FEATURE_VECTORS				= (1<<0),
	FEATURE_MATRICES			= (1<<1),
	FEATURE_ARRAYS				= (1<<2),
	FEATURE_STRUCTS				= (1<<3),
	FEATURE_NESTED_STRUCTS		= (1<<4),
	FEATURE_INSTANCE_ARRAYS		= (1<<5),
	FEATURE_UNUSED_VARS			= (1<<6),
	FEATURE_UNUSED_MEMBERS		= (1<<7),
	FEATURE_STD140_LAYOUT		= (1<<8),
	FEATURE_STD430_LAYOUT		= (1<<9),
	FEATURE_MATRIX_LAYOUT		= (1<<10),	//!< Matrix layout flags.
	FEATURE_UNSIZED_ARRAYS		= (1<<11),
	FEATURE_ARRAYS_OF_ARRAYS	= (1<<12),
	FEATURE_RELAXED_LAYOUT		= (1<<13),
	FEATURE_16BIT_STORAGE		= (1<<14),
	FEATURE_8BIT_STORAGE		= (1<<15),
	FEATURE_SCALAR_LAYOUT		= (1<<16),
	FEATURE_DESCRIPTOR_INDEXING	= (1<<17),
};

class RandomSSBOLayoutCase : public SSBOLayoutCase
{
public:

							RandomSSBOLayoutCase		(tcu::TestContext& testCtx, const char* name, const char* description, BufferMode bufferMode, deUint32 features, deUint32 seed, bool usePhysStorageBuffer);

private:
	void					generateBlock				(de::Random& rnd, deUint32 layoutFlags);
	void					generateBufferVar			(de::Random& rnd, BufferBlock& block, bool isLastMember);
	glu::VarType			generateType				(de::Random& rnd, int structDepth, int arrayDepth, bool arrayOk, bool unusedArrayOk);

	deUint32				m_features;
	int						m_maxBlocks;
	int						m_maxInstances;
	int						m_maxArrayLength;
	int						m_maxArrayDepth;
	int						m_maxStructDepth;
	int						m_maxBlockMembers;
	int						m_maxStructMembers;
	deUint32				m_seed;

	int						m_blockNdx;
	int						m_bufferVarNdx;
	int						m_structNdx;
};

RandomSSBOLayoutCase::RandomSSBOLayoutCase (tcu::TestContext& testCtx, const char* name, const char* description, BufferMode bufferMode, deUint32 features, deUint32 seed, bool usePhysStorageBuffer)
	: SSBOLayoutCase		(testCtx, name, description, bufferMode, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, usePhysStorageBuffer)
	, m_features			(features)
	, m_maxBlocks			((features & FEATURE_DESCRIPTOR_INDEXING)	? 1 : 4)
	, m_maxInstances		((features & FEATURE_INSTANCE_ARRAYS)		? 3 : 0)
	, m_maxArrayLength		((features & FEATURE_ARRAYS)				? 8 : 1)
	, m_maxArrayDepth		((features& FEATURE_ARRAYS_OF_ARRAYS)		? 2 : 0)
	, m_maxStructDepth		((features & FEATURE_STRUCTS)				? 2 : 0)
	, m_maxBlockMembers		(5)
	, m_maxStructMembers	(4)
	, m_seed				(seed)
	, m_blockNdx			(1)
	, m_bufferVarNdx		(1)
	, m_structNdx			(1)
{
	de::Random rnd(m_seed);

	const int numBlocks = rnd.getInt(1, m_maxBlocks);

	for (int ndx = 0; ndx < numBlocks; ndx++)
		generateBlock(rnd, 0);

	init();
}

void RandomSSBOLayoutCase::generateBlock (de::Random& rnd, deUint32 layoutFlags)
{
	DE_ASSERT(m_blockNdx <= 'z' - 'a');

	const float		instanceArrayWeight	= 0.3f;
	BufferBlock&	block				= m_interface.allocBlock((string("Block") + (char)('A' + m_blockNdx)).c_str());
	int				numInstances		= (m_maxInstances > 0 && rnd.getFloat() < instanceArrayWeight) ? rnd.getInt(0, m_maxInstances) : 0;
	int				numVars				= rnd.getInt(1, m_maxBlockMembers);

	if (m_features & FEATURE_DESCRIPTOR_INDEXING)
		numInstances = rnd.getInt(2, 4);

	if (numInstances > 0)
		block.setArraySize(numInstances);

	if (m_usePhysStorageBuffer || numInstances > 0 || rnd.getBool())
		block.setInstanceName((string("block") + (char)('A' + m_blockNdx)).c_str());

	// Layout flag candidates.
	vector<deUint32> layoutFlagCandidates;

	if (m_features & FEATURE_STD430_LAYOUT)
		layoutFlagCandidates.push_back(LAYOUT_STD430);

	if (m_features & FEATURE_STD140_LAYOUT)
		layoutFlagCandidates.push_back(LAYOUT_STD140);

	if (m_features & FEATURE_RELAXED_LAYOUT)
		layoutFlagCandidates.push_back(LAYOUT_RELAXED);

	if (m_features & FEATURE_SCALAR_LAYOUT)
		layoutFlagCandidates.push_back(LAYOUT_SCALAR);

	if (m_features & FEATURE_16BIT_STORAGE)
		layoutFlags |= LAYOUT_16BIT_STORAGE;

	if (m_features & FEATURE_8BIT_STORAGE)
		layoutFlags |= LAYOUT_8BIT_STORAGE;

	if (m_features & FEATURE_DESCRIPTOR_INDEXING)
		layoutFlags |= LAYOUT_DESCRIPTOR_INDEXING;

	DE_ASSERT(!layoutFlagCandidates.empty());

	layoutFlags |= rnd.choose<deUint32>(layoutFlagCandidates.begin(), layoutFlagCandidates.end());

	if (m_features & FEATURE_MATRIX_LAYOUT)
	{
		static const deUint32 matrixCandidates[] = { 0, LAYOUT_ROW_MAJOR, LAYOUT_COLUMN_MAJOR };
		layoutFlags |= rnd.choose<deUint32>(&matrixCandidates[0], &matrixCandidates[DE_LENGTH_OF_ARRAY(matrixCandidates)]);
	}

	block.setFlags(layoutFlags);

	for (int ndx = 0; ndx < numVars; ndx++)
		generateBufferVar(rnd, block, (ndx+1 == numVars));

	if (numVars > 0)
	{
		const BufferVar&	lastVar			= *(block.end()-1);
		const glu::VarType&	lastType		= lastVar.getType();
		const bool			isUnsizedArr	= lastType.isArrayType() && (lastType.getArraySize() == glu::VarType::UNSIZED_ARRAY);

		if (isUnsizedArr)
		{
			for (int instanceNdx = 0; instanceNdx < (numInstances ? numInstances : 1); instanceNdx++)
			{
				const int arrSize = rnd.getInt(1, m_maxArrayLength);
				block.setLastUnsizedArraySize(instanceNdx, arrSize);
			}
		}
	}

	m_blockNdx += 1;
}

static std::string genName (char first, char last, int ndx)
{
	std::string	str			= "";
	int			alphabetLen	= last - first + 1;

	while (ndx > alphabetLen)
	{
		str.insert(str.begin(), (char)(first + ((ndx-1)%alphabetLen)));
		ndx = ((ndx-1) / alphabetLen);
	}

	str.insert(str.begin(), (char)(first + (ndx%(alphabetLen+1)) - 1));

	return str;
}

void RandomSSBOLayoutCase::generateBufferVar (de::Random& rnd, BufferBlock& block, bool isLastMember)
{
	const float			readWeight			= 0.7f;
	const float			writeWeight			= 0.7f;
	const float			accessWeight		= 0.85f;
	const bool			unusedOk			= (m_features & FEATURE_UNUSED_VARS) != 0;
	const std::string	name				= genName('a', 'z', m_bufferVarNdx);
	const glu::VarType	type				= generateType(rnd, 0, 0, true, isLastMember && (m_features & FEATURE_UNSIZED_ARRAYS));
	const bool			access				= !unusedOk || (rnd.getFloat() < accessWeight);
	const bool			read				= access ? (rnd.getFloat() < readWeight) : false;
	const bool			write				= access ? (!read || (rnd.getFloat() < writeWeight)) : false;
	const deUint32		flags				= (read ? ACCESS_READ : 0) | (write ? ACCESS_WRITE : 0);

	block.addMember(BufferVar(name.c_str(), type, flags));

	m_bufferVarNdx += 1;
}

glu::VarType RandomSSBOLayoutCase::generateType (de::Random& rnd, int structDepth, int arrayDepth, bool arrayOk, bool unsizedArrayOk)
{
	const float structWeight		= 0.1f;
	const float arrayWeight			= 0.1f;
	const float	unsizedArrayWeight	= 0.8f;

	DE_ASSERT(arrayOk || !unsizedArrayOk);

	if (unsizedArrayOk && (rnd.getFloat() < unsizedArrayWeight))
	{
		const bool			childArrayOk	= ((m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0) &&
											  (arrayDepth < m_maxArrayDepth);
		const glu::VarType	elementType		= generateType(rnd, structDepth, arrayDepth + 1, childArrayOk, false);
		return glu::VarType(elementType, glu::VarType::UNSIZED_ARRAY);
	}
	else if (structDepth < m_maxStructDepth && rnd.getFloat() < structWeight)
	{
		vector<glu::VarType>	memberTypes;
		int						numMembers = rnd.getInt(1, m_maxStructMembers);

		// Generate members first so nested struct declarations are in correct order.
		for (int ndx = 0; ndx < numMembers; ndx++)
			memberTypes.push_back(generateType(rnd, structDepth + 1, arrayDepth, (arrayDepth < m_maxArrayDepth), false));

		glu::StructType& structType = m_interface.allocStruct((string("s") + genName('A', 'Z', m_structNdx)).c_str());
		m_structNdx += 1;

		DE_ASSERT(numMembers <= 'Z' - 'A');
		for (int ndx = 0; ndx < numMembers; ndx++)
		{
			structType.addMember((string("m") + (char)('A' + ndx)).c_str(), memberTypes[ndx]);
		}

		return glu::VarType(&structType);
	}
	else if (m_maxArrayLength > 0 && arrayOk && rnd.getFloat() < arrayWeight)
	{
		const int			arrayLength		= rnd.getInt(1, m_maxArrayLength);
		const bool			childArrayOk	= ((m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0) &&
											  (arrayDepth < m_maxArrayDepth);
		const glu::VarType	elementType		= generateType(rnd, structDepth, arrayDepth + 1, childArrayOk, false);

		return glu::VarType(elementType, arrayLength);
	}
	else
	{
		vector<glu::DataType> typeCandidates;

		typeCandidates.push_back(glu::TYPE_FLOAT);
		typeCandidates.push_back(glu::TYPE_INT);
		typeCandidates.push_back(glu::TYPE_UINT);
		typeCandidates.push_back(glu::TYPE_BOOL);

		if (m_features & FEATURE_16BIT_STORAGE)
		{
			typeCandidates.push_back(glu::TYPE_UINT16);
			typeCandidates.push_back(glu::TYPE_INT16);
			typeCandidates.push_back(glu::TYPE_FLOAT16);
		}

		if (m_features & FEATURE_8BIT_STORAGE)
		{
			typeCandidates.push_back(glu::TYPE_UINT8);
			typeCandidates.push_back(glu::TYPE_INT8);
		}

		if (m_features & FEATURE_VECTORS)
		{
			typeCandidates.push_back(glu::TYPE_FLOAT_VEC2);
			typeCandidates.push_back(glu::TYPE_FLOAT_VEC3);
			typeCandidates.push_back(glu::TYPE_FLOAT_VEC4);
			typeCandidates.push_back(glu::TYPE_INT_VEC2);
			typeCandidates.push_back(glu::TYPE_INT_VEC3);
			typeCandidates.push_back(glu::TYPE_INT_VEC4);
			typeCandidates.push_back(glu::TYPE_UINT_VEC2);
			typeCandidates.push_back(glu::TYPE_UINT_VEC3);
			typeCandidates.push_back(glu::TYPE_UINT_VEC4);
			typeCandidates.push_back(glu::TYPE_BOOL_VEC2);
			typeCandidates.push_back(glu::TYPE_BOOL_VEC3);
			typeCandidates.push_back(glu::TYPE_BOOL_VEC4);
			if (m_features & FEATURE_16BIT_STORAGE)
			{
				typeCandidates.push_back(glu::TYPE_FLOAT16_VEC2);
				typeCandidates.push_back(glu::TYPE_FLOAT16_VEC3);
				typeCandidates.push_back(glu::TYPE_FLOAT16_VEC4);
				typeCandidates.push_back(glu::TYPE_INT16_VEC2);
				typeCandidates.push_back(glu::TYPE_INT16_VEC3);
				typeCandidates.push_back(glu::TYPE_INT16_VEC4);
				typeCandidates.push_back(glu::TYPE_UINT16_VEC2);
				typeCandidates.push_back(glu::TYPE_UINT16_VEC3);
				typeCandidates.push_back(glu::TYPE_UINT16_VEC4);
			}
			if (m_features & FEATURE_8BIT_STORAGE)
			{
				typeCandidates.push_back(glu::TYPE_INT8_VEC2);
				typeCandidates.push_back(glu::TYPE_INT8_VEC3);
				typeCandidates.push_back(glu::TYPE_INT8_VEC4);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC2);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC3);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC4);
			}
		}

		if (m_features & FEATURE_MATRICES)
		{
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT2);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT2X3);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X2);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT3);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X4);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X2);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X3);
			typeCandidates.push_back(glu::TYPE_FLOAT_MAT4);
		}

		glu::DataType	type		= rnd.choose<glu::DataType>(typeCandidates.begin(), typeCandidates.end());
		glu::Precision	precision;

		if (glu::dataTypeSupportsPrecisionModifier(type))
		{
			// Precision.
			static const glu::Precision precisionCandidates[] = { glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP };
			precision = rnd.choose<glu::Precision>(&precisionCandidates[0], &precisionCandidates[DE_LENGTH_OF_ARRAY(precisionCandidates)]);
		}
		else
			precision = glu::PRECISION_LAST;

		return glu::VarType(type, precision);
	}
}

class BlockBasicTypeCase : public SSBOLayoutCase
{
public:
	BlockBasicTypeCase (tcu::TestContext& testCtx, const char* name, const char* description, const VarType& type, deUint32 layoutFlags, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer, bool readonly)
		: SSBOLayoutCase(testCtx, name, description, BUFFERMODE_PER_BLOCK, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
	{
		VarType tempType = type;
		while (tempType.isArrayType())
		{
			tempType = tempType.getElementType();
		}
		if (getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_UINT16 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_INT16 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_FLOAT16)
		{
			layoutFlags |= LAYOUT_16BIT_STORAGE;
		}
		if (getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_UINT8 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_INT8)
		{
			layoutFlags |= LAYOUT_8BIT_STORAGE;
		}

		BufferBlock& block = m_interface.allocBlock("Block");
		// For scalar layout tests with non-scalar types, add a scalar padding variable
		// before "var", to make var only be scalar aligned.
		if ((layoutFlags & LAYOUT_SCALAR) && !(type.isBasicType() && isDataTypeScalar(type.getBasicType()))) {
			block.addMember(BufferVar("padding", VarType(getDataTypeScalarType(tempType.getBasicType()), glu::PRECISION_LAST), ACCESS_READ|(readonly ? 0 : ACCESS_WRITE)));
		}
		block.addMember(BufferVar("var", type, ACCESS_READ|(readonly ? 0 : ACCESS_WRITE)));

		block.setFlags(layoutFlags);

		if (m_usePhysStorageBuffer || numInstances > 0)
		{
			block.setArraySize(numInstances);
			block.setInstanceName("block");
		}

		init();
	}
};

class BlockBasicUnsizedArrayCase : public SSBOLayoutCase
{
public:
	BlockBasicUnsizedArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, const VarType& elementType, int arraySize, deUint32 layoutFlags, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer, bool readonly)
		: SSBOLayoutCase(testCtx, name, description, BUFFERMODE_PER_BLOCK, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
	{
		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("var", VarType(elementType, VarType::UNSIZED_ARRAY), ACCESS_READ|(readonly ? 0 : ACCESS_WRITE)));

		VarType tempType = elementType;
		while (tempType.isArrayType())
		{
			tempType = tempType.getElementType();
		}
		if (getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_UINT16 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_INT16 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_FLOAT16)
		{
			layoutFlags |= LAYOUT_16BIT_STORAGE;
		}
		if (getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_UINT8 ||
			getDataTypeScalarType(tempType.getBasicType()) == glu::TYPE_INT8)
		{
			layoutFlags |= LAYOUT_8BIT_STORAGE;
		}

		block.setFlags(layoutFlags);

		block.setLastUnsizedArraySize(0, arraySize);

		if (m_usePhysStorageBuffer)
		{
			block.setInstanceName("block");
		}

		init();
	}
};

static void createRandomCaseGroup (tcu::TestCaseGroup* parentGroup, tcu::TestContext& testCtx, const char* groupName, const char* description, SSBOLayoutCase::BufferMode bufferMode, deUint32 features, int numCases, deUint32 baseSeed, bool usePhysStorageBuffer)
{
	tcu::TestCaseGroup* group = new tcu::TestCaseGroup(testCtx, groupName, description);
	parentGroup->addChild(group);

	baseSeed += (deUint32)testCtx.getCommandLine().getBaseSeed();

	for (int ndx = 0; ndx < numCases; ndx++)
		group->addChild(new RandomSSBOLayoutCase(testCtx, de::toString(ndx).c_str(), "", bufferMode, features, (deUint32)ndx+baseSeed, usePhysStorageBuffer));
}

class BlockSingleStructCase : public SSBOLayoutCase
{
public:
	BlockSingleStructCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer, bool readonly)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, glu::PRECISION_HIGHP)); // \todo [pyry] First member is unused.
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("s", VarType(&typeS), ACCESS_READ|(readonly ? 0 : ACCESS_WRITE)));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockSingleStructArrayCase : public SSBOLayoutCase
{
public:
	BlockSingleStructArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, glu::PRECISION_HIGHP)); // \todo [pyry] UNUSED
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("u", VarType(glu::TYPE_UINT, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("s", VarType(VarType(&typeS), 3), ACCESS_READ|ACCESS_WRITE));
		block.addMember(BufferVar("v", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_MEDIUMP), ACCESS_WRITE));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockSingleNestedStructCase : public SSBOLayoutCase
{
public:
	BlockSingleNestedStructCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, glu::PRECISION_HIGHP));
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)); // \todo [pyry] UNUSED

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP));
		typeT.addMember("b", VarType(&typeS));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("s", VarType(&typeS), ACCESS_READ));
		block.addMember(BufferVar("v", VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("t", VarType(&typeT), ACCESS_READ|ACCESS_WRITE));
		block.addMember(BufferVar("u", VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP), ACCESS_WRITE));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockSingleNestedStructArrayCase : public SSBOLayoutCase
{
public:
	BlockSingleNestedStructArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, glu::PRECISION_HIGHP));
		typeS.addMember("b", VarType(VarType(glu::TYPE_INT_VEC2, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)); // \todo [pyry] UNUSED

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP));
		typeT.addMember("b", VarType(VarType(&typeS), 3));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("s", VarType(&typeS), ACCESS_WRITE));
		block.addMember(BufferVar("v", VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("t", VarType(VarType(&typeT), 2), ACCESS_READ));
		block.addMember(BufferVar("u", VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP), ACCESS_READ|ACCESS_WRITE));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockUnsizedStructArrayCase : public SSBOLayoutCase
{
public:
	BlockUnsizedStructArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_UINT_VEC2, glu::PRECISION_HIGHP)); // \todo [pyry] UNUSED
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT2X4, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC3, glu::PRECISION_HIGHP));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("u", VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("v", VarType(glu::TYPE_UINT, glu::PRECISION_MEDIUMP), ACCESS_WRITE));
		block.addMember(BufferVar("s", VarType(VarType(&typeS), VarType::UNSIZED_ARRAY), ACCESS_READ|ACCESS_WRITE));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		{
			de::Random rnd(246);
			for (int ndx = 0; ndx < (m_numInstances ? m_numInstances : 1); ndx++)
			{
				const int lastArrayLen = rnd.getInt(1, 5);
				block.setLastUnsizedArraySize(ndx, lastArrayLen);
			}
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class Block2LevelUnsizedStructArrayCase : public SSBOLayoutCase
{
public:
	Block2LevelUnsizedStructArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, glu::PRECISION_HIGHP));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("u", VarType(glu::TYPE_UINT, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("v", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_MEDIUMP), ACCESS_WRITE));
		block.addMember(BufferVar("s", VarType(VarType(VarType(&typeS), 2), VarType::UNSIZED_ARRAY), ACCESS_READ|ACCESS_WRITE));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		{
			de::Random rnd(2344);
			for (int ndx = 0; ndx < (m_numInstances ? m_numInstances : 1); ndx++)
			{
				const int lastArrayLen = rnd.getInt(1, 5);
				block.setLastUnsizedArraySize(ndx, lastArrayLen);
			}
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockUnsizedNestedStructArrayCase : public SSBOLayoutCase
{
public:
	BlockUnsizedNestedStructArrayCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 layoutFlags, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_layoutFlags		(layoutFlags)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_UINT_VEC3, glu::PRECISION_HIGHP));
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)); // \todo [pyry] UNUSED

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT4X3, glu::PRECISION_MEDIUMP));
		typeT.addMember("b", VarType(VarType(&typeS), 3));
		typeT.addMember("c", VarType(glu::TYPE_INT, glu::PRECISION_HIGHP));

		BufferBlock& block = m_interface.allocBlock("Block");
		block.addMember(BufferVar("s", VarType(&typeS), ACCESS_WRITE));
		block.addMember(BufferVar("v", VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_LOWP), 0 /* no access */));
		block.addMember(BufferVar("u", VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP), ACCESS_READ|ACCESS_WRITE));
		block.addMember(BufferVar("t", VarType(VarType(&typeT), VarType::UNSIZED_ARRAY), ACCESS_READ));
		block.setFlags(m_layoutFlags);

		if (m_usePhysStorageBuffer || m_numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(m_numInstances);
		}

		{
			de::Random rnd(7921);
			for (int ndx = 0; ndx < (m_numInstances ? m_numInstances : 1); ndx++)
			{
				const int lastArrayLen = rnd.getInt(1, 5);
				block.setLastUnsizedArraySize(ndx, lastArrayLen);
			}
		}

		init();
	}

private:
	deUint32	m_layoutFlags;
	int			m_numInstances;
};

class BlockMultiBasicTypesCase : public SSBOLayoutCase
{
public:
	BlockMultiBasicTypesCase	(tcu::TestContext& testCtx, const char* name, const char* description, deUint32 flagsA, deUint32 flagsB, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase		(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_flagsA				(flagsA)
		, m_flagsB				(flagsB)
		, m_numInstances		(numInstances)
	{
		BufferBlock& blockA = m_interface.allocBlock("BlockA");
		blockA.addMember(BufferVar("a", VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP), ACCESS_READ|ACCESS_WRITE));
		blockA.addMember(BufferVar("b", VarType(glu::TYPE_UINT_VEC3, glu::PRECISION_LOWP), 0 /* no access */));
		blockA.addMember(BufferVar("c", VarType(glu::TYPE_FLOAT_MAT2, glu::PRECISION_MEDIUMP), ACCESS_READ));
		blockA.setInstanceName("blockA");
		blockA.setFlags(m_flagsA);

		BufferBlock& blockB = m_interface.allocBlock("BlockB");
		blockB.addMember(BufferVar("a", VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_MEDIUMP), ACCESS_WRITE));
		blockB.addMember(BufferVar("b", VarType(glu::TYPE_INT_VEC2, glu::PRECISION_LOWP), ACCESS_READ));
		blockB.addMember(BufferVar("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP), 0 /* no access */));
		blockB.addMember(BufferVar("d", VarType(glu::TYPE_BOOL, glu::PRECISION_LAST), ACCESS_READ|ACCESS_WRITE));
		blockB.setInstanceName("blockB");
		blockB.setFlags(m_flagsB);

		if (m_numInstances > 0)
		{
			blockA.setArraySize(m_numInstances);
			blockB.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_flagsA;
	deUint32	m_flagsB;
	int			m_numInstances;
};

class BlockMultiNestedStructCase : public SSBOLayoutCase
{
public:
	BlockMultiNestedStructCase (tcu::TestContext& testCtx, const char* name, const char* description, deUint32 flagsA, deUint32 flagsB, BufferMode bufferMode, int numInstances, MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag, bool usePhysStorageBuffer)
		: SSBOLayoutCase	(testCtx, name, description, bufferMode, matrixLoadFlag, matrixStoreFlag, usePhysStorageBuffer)
		, m_flagsA			(flagsA)
		, m_flagsB			(flagsB)
		, m_numInstances	(numInstances)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, glu::PRECISION_LOWP));
		typeS.addMember("b", VarType(VarType(glu::TYPE_INT_VEC2, glu::PRECISION_MEDIUMP), 4));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP));

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_UINT, glu::PRECISION_MEDIUMP)); // \todo [pyry] UNUSED
		typeT.addMember("b", VarType(&typeS));
		typeT.addMember("c", VarType(glu::TYPE_BOOL_VEC4, glu::PRECISION_LAST));

		BufferBlock& blockA = m_interface.allocBlock("BlockA");
		blockA.addMember(BufferVar("a", VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP), ACCESS_READ|ACCESS_WRITE));
		blockA.addMember(BufferVar("b", VarType(&typeS), ACCESS_WRITE));
		blockA.addMember(BufferVar("c", VarType(glu::TYPE_UINT_VEC3, glu::PRECISION_LOWP), 0 /* no access */));
		blockA.setInstanceName("blockA");
		blockA.setFlags(m_flagsA);

		BufferBlock& blockB = m_interface.allocBlock("BlockB");
		blockB.addMember(BufferVar("a", VarType(glu::TYPE_FLOAT_MAT2, glu::PRECISION_MEDIUMP), ACCESS_WRITE));
		blockB.addMember(BufferVar("b", VarType(&typeT), ACCESS_READ|ACCESS_WRITE));
		blockB.addMember(BufferVar("c", VarType(glu::TYPE_BOOL_VEC4, glu::PRECISION_LAST), 0 /* no access */));
		blockB.addMember(BufferVar("d", VarType(glu::TYPE_BOOL, glu::PRECISION_LAST), ACCESS_READ|ACCESS_WRITE));
		blockB.setInstanceName("blockB");
		blockB.setFlags(m_flagsB);

		if (m_numInstances > 0)
		{
			blockA.setArraySize(m_numInstances);
			blockB.setArraySize(m_numInstances);
		}

		init();
	}

private:
	deUint32	m_flagsA;
	deUint32	m_flagsB;
	int			m_numInstances;
};

// unsized_array_length

struct UnsizedArrayCaseParams
{
	int					elementSize;
	vk::VkDeviceSize	bufferSize;
	bool				useMinBufferOffset;
	vk::VkDeviceSize	bufferBindLength;
	const char*			name;
};

void createUnsizedArrayLengthProgs (SourceCollections& dst, UnsizedArrayCaseParams)
{
	dst.glslSources.add("comp") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(set=0, binding=0, std430) readonly buffer x {\n"
		"   int xs[];\n"
		"};\n"
		"layout(set=0, binding=1, std430) writeonly buffer y {\n"
		"   int observed_size;\n"
		"};\n"
		"layout(local_size_x=1) in;\n"
		"void main (void) {\n"
		"   observed_size = xs.length();\n"
		"}\n");
}

tcu::TestStatus ssboUnsizedArrayLengthTest (Context& context, UnsizedArrayCaseParams params)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	Allocator&								allocator				= context.getDefaultAllocator();

	DescriptorSetLayoutBuilder				builder;
	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);	// input buffer
	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);	// result buffer

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(builder.build(vk, device));
	const Unique<VkDescriptorPool>			descriptorPool			(vk::DescriptorPoolBuilder()
																	.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
																	.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkPipelineLayoutCreateFlags)0,
		1,															// setLayoutCount,
		&descriptorSetLayout.get(),									// pSetLayouts
		0,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, device, &pipelineLayoutCreateInfo));

	const Unique<VkShaderModule>			computeModule			(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), (VkShaderModuleCreateFlags)0u));

	const VkPipelineShaderStageCreateInfo	shaderCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*computeModule,												// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const Unique<VkPipeline>				pipeline				(createComputePipeline(vk, device, (VkPipelineCache)0u, &pipelineCreateInfo));

	// Input buffer
	const VkBufferCreateInfo inputBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0,															// flags
		(VkDeviceSize) params.bufferSize,							// size
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,							// usage TODO: also test _DYNAMIC case.
		VK_SHARING_MODE_EXCLUSIVE,
		0u,															// queueFamilyCount
		DE_NULL,													// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					inputBuffer				(createBuffer(vk, device, &inputBufferCreateInfo));
	const VkMemoryRequirements				inputBufferRequirements	= getBufferMemoryRequirements(vk, device, *inputBuffer);
	const de::MovePtr<Allocation>			inputBufferMemory		= allocator.allocate(inputBufferRequirements, MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(device, *inputBuffer, inputBufferMemory->getMemory(), inputBufferMemory->getOffset()));
	// Note: don't care about the contents of the input buffer -- we only determine a size.

	// Output buffer
	const VkBufferCreateInfo outputBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0,
		(VkDeviceSize) 4,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL,
	};
	const Unique<VkBuffer>					outputBuffer			(createBuffer(vk, device, &outputBufferCreateInfo));
	const VkMemoryRequirements				outputBufferRequirements= getBufferMemoryRequirements(vk, device, *outputBuffer);
	const de::MovePtr<Allocation>			outputBufferMemory		= allocator.allocate(outputBufferRequirements, MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(device, *outputBuffer, outputBufferMemory->getMemory(), outputBufferMemory->getOffset()));

	// Initialize output buffer contents
	const VkMappedMemoryRange	range			=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	// sType
		DE_NULL,								// pNext
		outputBufferMemory->getMemory(),		// memory
		0,										// offset
		VK_WHOLE_SIZE,							// size
	};
	int *							outputBufferPtr			= (int *)outputBufferMemory->getHostPtr();
	*outputBufferPtr = -1;
	VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &range));

	// Build descriptor set
	vk::VkDeviceSize bufferBindOffset = 0;
	if (params.useMinBufferOffset)
	{
		const VkPhysicalDeviceLimits	deviceLimits				= getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;
		bufferBindOffset											= deviceLimits.minStorageBufferOffsetAlignment;
	}

	const VkDescriptorBufferInfo			inputBufferDesc			= makeDescriptorBufferInfo(*inputBuffer, bufferBindOffset, params.bufferBindLength);
	const VkDescriptorBufferInfo			outputBufferDesc		= makeDescriptorBufferInfo(*outputBuffer, 0u, VK_WHOLE_SIZE);

	const VkDescriptorSetAllocateInfo		descAllocInfo			=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*descriptorPool,											// pool
		1u,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
	};
	const Unique<VkDescriptorSet>			descSet					(allocateDescriptorSet(vk, device, &descAllocInfo));

	DescriptorSetUpdateBuilder()
		.writeSingle(*descSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferDesc)
		.writeSingle(*descSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDesc)
		.update(vk, device);

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags
		context.getUniversalQueueFamilyIndex(),						// queueFamilyIndex
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			// sType
		DE_NULL,												// pNext
		*cmdPool,												// pool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						// level
		1u,														// bufferCount
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, device, &cmdBufParams));

	// Record commands
	beginCommandBuffer(vk, *cmdBuf);

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, DE_NULL);
	vk.cmdDispatch(*cmdBuf, 1, 1, 1);

	const VkMemoryBarrier					barrier					=
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
		DE_NULL,							// pNext
		VK_ACCESS_SHADER_WRITE_BIT,			// srcAccessMask
		VK_ACCESS_HOST_READ_BIT,			// dstAccessMask
	};
	vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 1, &barrier, 0, DE_NULL, 0, DE_NULL);

	endCommandBuffer(vk, *cmdBuf);

	submitCommandsAndWait(vk, device, queue, cmdBuf.get());

	// Read back output buffer contents
	VK_CHECK(vk.invalidateMappedMemoryRanges(device, 1, &range));

	// Expected number of elements in array at end of storage buffer
	const VkDeviceSize						boundLength				= params.bufferBindLength == VK_WHOLE_SIZE
																	? params.bufferSize - bufferBindOffset
																	: params.bufferBindLength;
	const int								expectedResult			= (int)(boundLength / params.elementSize);
	const int								actualResult			= *outputBufferPtr;

	context.getTestContext().getLog()
	<< tcu::TestLog::Message
	<< "Buffer size " << params.bufferSize
	<< " offset " << bufferBindOffset
	<< " length " << params.bufferBindLength
	<< " element size " << params.elementSize
	<< " expected array size: " << expectedResult
	<< " actual array size: " << actualResult
	<< tcu::TestLog::EndMessage;

	if (expectedResult == actualResult)
		return tcu::TestStatus::pass("Got expected array size");
	else
		return tcu::TestStatus::fail("Mismatch array size");
}

class SSBOLayoutTests : public tcu::TestCaseGroup
{
public:
							SSBOLayoutTests		(tcu::TestContext& testCtx, bool usePhysStorageBuffer, bool readonly);
							~SSBOLayoutTests	(void);

	void					init				(void);

private:
							SSBOLayoutTests		(const SSBOLayoutTests& other);
	SSBOLayoutTests&		operator=			(const SSBOLayoutTests& other);

	bool m_usePhysStorageBuffer;
	bool m_readonly;
};


SSBOLayoutTests::SSBOLayoutTests (tcu::TestContext& testCtx, bool usePhysStorageBuffer, bool readonly)
	: TestCaseGroup(testCtx, "layout", "SSBO Layout Tests")
	, m_usePhysStorageBuffer(usePhysStorageBuffer)
	, m_readonly(readonly)
{
}

SSBOLayoutTests::~SSBOLayoutTests (void)
{
}

void SSBOLayoutTests::init (void)
{
	static const glu::DataType basicTypes[] =
	{
		glu::TYPE_FLOAT,
		glu::TYPE_FLOAT_VEC2,
		glu::TYPE_FLOAT_VEC3,
		glu::TYPE_FLOAT_VEC4,
		glu::TYPE_INT,
		glu::TYPE_INT_VEC2,
		glu::TYPE_INT_VEC3,
		glu::TYPE_INT_VEC4,
		glu::TYPE_UINT,
		glu::TYPE_UINT_VEC2,
		glu::TYPE_UINT_VEC3,
		glu::TYPE_UINT_VEC4,
		glu::TYPE_BOOL,
		glu::TYPE_BOOL_VEC2,
		glu::TYPE_BOOL_VEC3,
		glu::TYPE_BOOL_VEC4,
		glu::TYPE_FLOAT_MAT2,
		glu::TYPE_FLOAT_MAT3,
		glu::TYPE_FLOAT_MAT4,
		glu::TYPE_FLOAT_MAT2X3,
		glu::TYPE_FLOAT_MAT2X4,
		glu::TYPE_FLOAT_MAT3X2,
		glu::TYPE_FLOAT_MAT3X4,
		glu::TYPE_FLOAT_MAT4X2,
		glu::TYPE_FLOAT_MAT4X3,
		glu::TYPE_UINT8,
		glu::TYPE_UINT8_VEC2,
		glu::TYPE_UINT8_VEC3,
		glu::TYPE_UINT8_VEC4,
		glu::TYPE_INT8,
		glu::TYPE_INT8_VEC2,
		glu::TYPE_INT8_VEC3,
		glu::TYPE_INT8_VEC4,
		glu::TYPE_UINT16,
		glu::TYPE_UINT16_VEC2,
		glu::TYPE_UINT16_VEC3,
		glu::TYPE_UINT16_VEC4,
		glu::TYPE_INT16,
		glu::TYPE_INT16_VEC2,
		glu::TYPE_INT16_VEC3,
		glu::TYPE_INT16_VEC4,
		glu::TYPE_FLOAT16,
		glu::TYPE_FLOAT16_VEC2,
		glu::TYPE_FLOAT16_VEC3,
		glu::TYPE_FLOAT16_VEC4,
	};

	static const struct
	{
		const char*		name;
		deUint32		flags;
	} layoutFlags[] =
	{
		{ "std140",	LAYOUT_STD140 },
		{ "std430",	LAYOUT_STD430 },
		{ "scalar",	LAYOUT_SCALAR },
	};

	static const struct
	{
		const char*		name;
		deUint32		flags;
	} matrixFlags[] =
	{
		{ "row_major",		LAYOUT_ROW_MAJOR	},
		{ "column_major",	LAYOUT_COLUMN_MAJOR }
	};

	static const struct
	{
		const char*						name;
		SSBOLayoutCase::BufferMode		mode;
	} bufferModes[] =
	{
		{ "per_block_buffer",	SSBOLayoutCase::BUFFERMODE_PER_BLOCK },
		{ "single_buffer",		SSBOLayoutCase::BUFFERMODE_SINGLE	}
	};

	using SuffixLoadFlag	= pair<string, MatrixLoadFlags>;
	using SuffixStoreFlag	= pair<string, MatrixStoreFlags>;

	static const array<SuffixLoadFlag, 2> matrixLoadTypes =
	{{
		SuffixLoadFlag( "",				LOAD_FULL_MATRIX		),
		SuffixLoadFlag( "_comp_access",	LOAD_MATRIX_COMPONENTS	),
	}};

	static const array<SuffixStoreFlag, 2> matrixStoreTypes =
	{{
		SuffixStoreFlag( "",			STORE_FULL_MATRIX		),
		SuffixStoreFlag( "_store_cols",	STORE_MATRIX_COLUMNS	),
	}};

	// ssbo.single_basic_type
	{
		tcu::TestCaseGroup* singleBasicTypeGroup = new tcu::TestCaseGroup(m_testCtx, "single_basic_type", "Single basic variable in single buffer");
		addChild(singleBasicTypeGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			singleBasicTypeGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);

				if (!glu::dataTypeSupportsPrecisionModifier(type))
					layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, typeName, "", VarType(type, glu::PRECISION_LAST), layoutFlags[layoutFlagNdx].flags, 0, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));
				else
				{
					for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
					{
						const glu::Precision	precision	= glu::Precision(precNdx);
						const string			caseName	= string(glu::getPrecisionName(precision)) + "_" + typeName;

						layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, caseName.c_str(), "", VarType(type, precision), layoutFlags[layoutFlagNdx].flags, 0, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));
					}
				}

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
						{
							const glu::Precision	precision	= glu::Precision(precNdx);
							const string			caseName	= string(matrixFlags[matFlagNdx].name) + "_" + string(glu::getPrecisionName(precision)) + "_" + typeName;

							for (const auto& loadType	: matrixLoadTypes)
							for (const auto& storeType	: matrixStoreTypes)
								layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, (caseName + loadType.first + storeType.first).c_str(), "", glu::VarType(type, precision), layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, 0, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
						}
					}
				}
			}
		}
	}

	// ssbo.single_basic_array
	{
		tcu::TestCaseGroup* singleBasicArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_basic_array", "Single basic array variable in single buffer");
		addChild(singleBasicArrayGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			singleBasicArrayGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);
				const int		arraySize	= 3;

				layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, typeName, "",
															 VarType(VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP), arraySize),
															 layoutFlags[layoutFlagNdx].flags, 0, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																		 VarType(VarType(type, glu::PRECISION_HIGHP), arraySize),
																		 layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, 0, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.basic_unsized_array
	{
		tcu::TestCaseGroup* basicUnsizedArray = new tcu::TestCaseGroup(m_testCtx, "basic_unsized_array", "Basic unsized array tests");
		addChild(basicUnsizedArray);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			basicUnsizedArray->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);
				const int		arraySize	= 19;

				layoutGroup->addChild(new BlockBasicUnsizedArrayCase(m_testCtx, typeName, "",
																	 VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP),
																	 arraySize, layoutFlags[layoutFlagNdx].flags, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicUnsizedArrayCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																				 VarType(type, glu::PRECISION_HIGHP), arraySize,
																				 layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.2_level_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* nestedArrayGroup = new tcu::TestCaseGroup(m_testCtx, "2_level_array", "2-level nested array");
		addChild(nestedArrayGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			nestedArrayGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);
				const int		childSize	= 3;
				const int		parentSize	= 4;
				const VarType	childType	(VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP), childSize);
				const VarType	fullType	(childType, parentSize);

				layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, typeName, "", fullType, layoutFlags[layoutFlagNdx].flags, 0, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																		 fullType,
																		 layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, 0, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.3_level_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* nestedArrayGroup = new tcu::TestCaseGroup(m_testCtx, "3_level_array", "3-level nested array");
		addChild(nestedArrayGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			nestedArrayGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);
				const int		childSize0	= 3;
				const int		childSize1	= 2;
				const int		parentSize	= 4;
				const VarType	childType0	(VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP), childSize0);
				const VarType	childType1	(childType0, childSize1);
				const VarType	fullType	(childType1, parentSize);

				layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, typeName, "", fullType, layoutFlags[layoutFlagNdx].flags, 0, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																		 fullType,
																		 layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, 0, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.3_level_unsized_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* nestedArrayGroup = new tcu::TestCaseGroup(m_testCtx, "3_level_unsized_array", "3-level nested array, top-level array unsized");
		addChild(nestedArrayGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			nestedArrayGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type		= basicTypes[basicTypeNdx];
				const char*		typeName	= glu::getDataTypeName(type);
				const int		childSize0	= 2;
				const int		childSize1	= 4;
				const int		parentSize	= 3;
				const VarType	childType0	(VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP), childSize0);
				const VarType	childType1	(childType0, childSize1);

				layoutGroup->addChild(new BlockBasicUnsizedArrayCase(m_testCtx, typeName, "", childType1, parentSize, layoutFlags[layoutFlagNdx].flags, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicUnsizedArrayCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																				 childType1, parentSize,
																				 layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.single_struct
	{
		tcu::TestCaseGroup* singleStructGroup = new tcu::TestCaseGroup(m_testCtx, "single_struct", "Single struct in uniform block");
		addChild(singleStructGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleStructGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					const deUint32	caseFlags	= layoutFlags[layoutFlagNdx].flags;
					string			caseName	= layoutFlags[layoutFlagNdx].name;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						caseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockSingleStructCase(m_testCtx, (caseName + loadType.first + storeType.first).c_str(), "", caseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
				}
			}
		}
	}

	// ssbo.single_struct_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* singleStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_struct_array", "Struct array in one uniform block");
		addChild(singleStructArrayGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleStructArrayGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockSingleStructArrayCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.single_nested_struct
	if (!m_readonly)
	{
		tcu::TestCaseGroup* singleNestedStructGroup = new tcu::TestCaseGroup(m_testCtx, "single_nested_struct", "Nested struct in one uniform block");
		addChild(singleNestedStructGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleNestedStructGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockSingleNestedStructCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.single_nested_struct_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* singleNestedStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_nested_struct_array", "Nested struct array in one uniform block");
		addChild(singleNestedStructArrayGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleNestedStructArrayGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockSingleNestedStructArrayCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.unsized_struct_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* singleStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "unsized_struct_array", "Unsized struct array in one uniform block");
		addChild(singleStructArrayGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleStructArrayGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockUnsizedStructArrayCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.2_level_unsized_struct_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* structArrayGroup = new tcu::TestCaseGroup(m_testCtx, "2_level_unsized_struct_array", "Unsized 2-level struct array in one uniform block");
		addChild(structArrayGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			structArrayGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new Block2LevelUnsizedStructArrayCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.unsized_nested_struct_array
	if (!m_readonly)
	{
		tcu::TestCaseGroup* singleNestedStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "unsized_nested_struct_array", "Unsized, nested struct array in one uniform block");
		addChild(singleNestedStructArrayGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			singleNestedStructArrayGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (bufferModes[modeNdx].mode == SSBOLayoutCase::BUFFERMODE_SINGLE && isArray == 0)
						continue; // Doesn't make sense to add this variant.

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockUnsizedNestedStructArrayCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.instance_array_basic_type
	if (!m_readonly)
	{
		tcu::TestCaseGroup* instanceArrayBasicTypeGroup = new tcu::TestCaseGroup(m_testCtx, "instance_array_basic_type", "Single basic variable in instance array");
		addChild(instanceArrayBasicTypeGroup);

		for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
		{
			tcu::TestCaseGroup* layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
			instanceArrayBasicTypeGroup->addChild(layoutGroup);

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType	type			= basicTypes[basicTypeNdx];
				const char*		typeName		= glu::getDataTypeName(type);
				const int		numInstances	= 3;

				layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, typeName, "",
															 VarType(type, !glu::dataTypeSupportsPrecisionModifier(type) ? glu::PRECISION_LAST : glu::PRECISION_HIGHP),
															 layoutFlags[layoutFlagNdx].flags, numInstances, LOAD_FULL_MATRIX, STORE_FULL_MATRIX, m_usePhysStorageBuffer, m_readonly));

				if (glu::isDataTypeMatrix(type))
				{
					for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
					{
						for (const auto& loadType	: matrixLoadTypes)
						for (const auto& storeType	: matrixStoreTypes)
							layoutGroup->addChild(new BlockBasicTypeCase(m_testCtx, (string(matrixFlags[matFlagNdx].name) + "_" + typeName + loadType.first + storeType.first).c_str(), "",
																		 VarType(type, glu::PRECISION_HIGHP), layoutFlags[layoutFlagNdx].flags|matrixFlags[matFlagNdx].flags,
																		 numInstances, loadType.second, storeType.second, m_usePhysStorageBuffer, m_readonly));
					}
				}
			}
		}
	}

	// ssbo.multi_basic_types
	if (!m_readonly)
	{
		tcu::TestCaseGroup* multiBasicTypesGroup = new tcu::TestCaseGroup(m_testCtx, "multi_basic_types", "Multiple buffers with basic types");
		addChild(multiBasicTypesGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			multiBasicTypesGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockMultiBasicTypesCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}

			for (int isArray = 0; isArray < 2; isArray++)
			{
				std::string	baseName	= "relaxed_block";
				deUint32	baseFlags	= LAYOUT_RELAXED;

				if (isArray)
					baseName += "_instance_array";

				for (const auto& loadType	: matrixLoadTypes)
				for (const auto& storeType	: matrixStoreTypes)
					modeGroup->addChild(new BlockMultiBasicTypesCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
			}
		}
	}

	// ssbo.multi_nested_struct
	if (!m_readonly)
	{
		tcu::TestCaseGroup* multiNestedStructGroup = new tcu::TestCaseGroup(m_testCtx, "multi_nested_struct", "Multiple buffers with nested structs");
		addChild(multiNestedStructGroup);

		for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
		{
			tcu::TestCaseGroup* modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
			multiNestedStructGroup->addChild(modeGroup);

			for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
			{
				for (int isArray = 0; isArray < 2; isArray++)
				{
					std::string	baseName	= layoutFlags[layoutFlagNdx].name;
					deUint32	baseFlags	= layoutFlags[layoutFlagNdx].flags;

					if (isArray)
						baseName += "_instance_array";

					for (const auto& loadType	: matrixLoadTypes)
					for (const auto& storeType	: matrixStoreTypes)
						modeGroup->addChild(new BlockMultiNestedStructCase(m_testCtx, (baseName + loadType.first + storeType.first).c_str(), "", baseFlags, baseFlags, bufferModes[modeNdx].mode, isArray ? 3 : 0, loadType.second, storeType.second, m_usePhysStorageBuffer));
				}
			}
		}
	}

	// ssbo.random
	if (!m_readonly)
	{
		const deUint32	allStdLayouts		= FEATURE_STD140_LAYOUT|FEATURE_STD430_LAYOUT;
		const deUint32	allBasicTypes		= FEATURE_VECTORS|FEATURE_MATRICES;
		const deUint32	unused				= FEATURE_UNUSED_MEMBERS|FEATURE_UNUSED_VARS;
		const deUint32	unsized				= FEATURE_UNSIZED_ARRAYS;
		const deUint32	matFlags			= FEATURE_MATRIX_LAYOUT;
		const deUint32	allButRelaxed		= ~FEATURE_RELAXED_LAYOUT & ~FEATURE_16BIT_STORAGE & ~FEATURE_8BIT_STORAGE & ~FEATURE_SCALAR_LAYOUT & ~FEATURE_DESCRIPTOR_INDEXING;
		const deUint32	allRelaxed			= FEATURE_VECTORS|FEATURE_RELAXED_LAYOUT|FEATURE_INSTANCE_ARRAYS;
		const deUint32	allScalar			= ~FEATURE_RELAXED_LAYOUT & ~allStdLayouts & ~FEATURE_16BIT_STORAGE & ~FEATURE_8BIT_STORAGE & ~FEATURE_DESCRIPTOR_INDEXING;
		const deUint32	descriptorIndexing	= allStdLayouts|FEATURE_RELAXED_LAYOUT|FEATURE_SCALAR_LAYOUT|FEATURE_DESCRIPTOR_INDEXING|allBasicTypes|unused|matFlags;

		tcu::TestCaseGroup* randomGroup = new tcu::TestCaseGroup(m_testCtx, "random", "Random Uniform Block cases");
		addChild(randomGroup);

		for (int i = 0; i < 3; ++i)
		{

			tcu::TestCaseGroup* group = randomGroup;
			if (i == 1)
			{
				group = new tcu::TestCaseGroup(m_testCtx, "16bit", "16bit storage");
				randomGroup->addChild(group);
			}
			else if (i == 2)
			{
				group = new tcu::TestCaseGroup(m_testCtx, "8bit", "8bit storage");
				randomGroup->addChild(group);
			}
			const deUint32 use16BitStorage = i == 1 ? FEATURE_16BIT_STORAGE : 0;
			const deUint32 use8BitStorage  = i == 2 ? FEATURE_8BIT_STORAGE : 0;

			// Basic types.
			createRandomCaseGroup(group, m_testCtx, "scalar_types",		"Scalar types only, per-block buffers",				SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused,																			25, 0, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "vector_types",		"Scalar and vector types only, per-block buffers",	SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|FEATURE_VECTORS,															25, 25, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "basic_types",		"All basic types, per-block buffers",				SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags,													25, 50, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "basic_arrays",		"Arrays, per-block buffers",						SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|FEATURE_ARRAYS,									25, 50, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "unsized_arrays",	"Unsized arrays, per-block buffers",				SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_ARRAYS,							25, 50, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "arrays_of_arrays",	"Arrays of arrays, per-block buffers",				SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_ARRAYS|FEATURE_ARRAYS_OF_ARRAYS,	25, 950, m_usePhysStorageBuffer);

			createRandomCaseGroup(group, m_testCtx, "basic_instance_arrays",					"Basic instance arrays, per-block buffers",				SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_INSTANCE_ARRAYS,															25, 75, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "nested_structs",							"Nested structs, per-block buffers",					SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_STRUCTS,																	25, 100, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_arrays",					"Nested structs, arrays, per-block buffers",			SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_STRUCTS|FEATURE_ARRAYS|FEATURE_ARRAYS_OF_ARRAYS,							25, 150, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_instance_arrays",			"Nested structs, instance arrays, per-block buffers",	SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_STRUCTS|FEATURE_INSTANCE_ARRAYS,											25, 125, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_arrays_instance_arrays",	"Nested structs, instance arrays, per-block buffers",	SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allStdLayouts|unused|allBasicTypes|matFlags|unsized|FEATURE_STRUCTS|FEATURE_ARRAYS|FEATURE_ARRAYS_OF_ARRAYS|FEATURE_INSTANCE_ARRAYS,	25, 175, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "all_per_block_buffers",	"All random features, per-block buffers",	SSBOLayoutCase::BUFFERMODE_PER_BLOCK,	use8BitStorage|use16BitStorage|allButRelaxed,	50, 200, m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "all_shared_buffer",		"All random features, shared buffer",		SSBOLayoutCase::BUFFERMODE_SINGLE,		use8BitStorage|use16BitStorage|allButRelaxed,	50, 250, m_usePhysStorageBuffer);

			createRandomCaseGroup(group, m_testCtx, "relaxed",				"VK_KHR_relaxed_block_layout",		SSBOLayoutCase::BUFFERMODE_SINGLE,	use8BitStorage|use16BitStorage|allRelaxed, 100, deInt32Hash(313), m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "scalar",				"VK_EXT_scalar_block_layout",		SSBOLayoutCase::BUFFERMODE_SINGLE,	use8BitStorage|use16BitStorage|allScalar, 100, deInt32Hash(313), m_usePhysStorageBuffer);
			createRandomCaseGroup(group, m_testCtx, "descriptor_indexing",	"VK_EXT_descriptor_indexing",		SSBOLayoutCase::BUFFERMODE_SINGLE,	use8BitStorage|use16BitStorage|descriptorIndexing, 50, 123, m_usePhysStorageBuffer);
		}
	}
}

void createUnsizedArrayTests (tcu::TestCaseGroup* testGroup)
{
	const UnsizedArrayCaseParams subcases[] =
	{
		{ 4, 256, false, 256,			"float_no_offset_explicit_size" },
		{ 4, 256, false, VK_WHOLE_SIZE,	"float_no_offset_whole_size" },
		{ 4, 512, true, 32,				"float_offset_explicit_size" },
		{ 4, 512, true, VK_WHOLE_SIZE,	"float_offset_whole_size" },
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(subcases); ndx++)
	{
		const UnsizedArrayCaseParams& params = subcases[ndx];
		addFunctionCaseWithPrograms<UnsizedArrayCaseParams>(testGroup, params.name, "", createUnsizedArrayLengthProgs, ssboUnsizedArrayLengthTest, params);
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> ssboTestGroup (new tcu::TestCaseGroup(testCtx, name.c_str(), "Shader Storage Buffer Object Tests"));

	ssboTestGroup->addChild(new SSBOLayoutTests(testCtx, false, false));
	addTestGroup(ssboTestGroup.get(), "unsized_array_length", "SSBO unsized array length tests", createUnsizedArrayTests);

	de::MovePtr<tcu::TestCaseGroup> readonlyGroup(new tcu::TestCaseGroup(testCtx, "readonly", "Readonly Shader Storage Buffer Tests"));
	readonlyGroup->addChild(new SSBOLayoutTests(testCtx, false, true));
	ssboTestGroup->addChild(readonlyGroup.release());

	de::MovePtr<tcu::TestCaseGroup> physGroup(new tcu::TestCaseGroup(testCtx, "phys", "Physical Storage Buffer Pointer Tests"));
	physGroup->addChild(new SSBOLayoutTests(testCtx, true, false));
	ssboTestGroup->addChild(physGroup.release());

	ssboTestGroup->addChild(createSSBOCornerCaseTests(testCtx));

	return ssboTestGroup.release();
}

} // ssbo
} // vkt
