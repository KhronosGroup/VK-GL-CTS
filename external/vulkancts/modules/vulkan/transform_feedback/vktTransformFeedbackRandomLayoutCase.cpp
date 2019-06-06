/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Vulkan Transform Feedback Random Layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackRandomLayoutCase.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace TransformFeedback
{

namespace
{

static std::string genName (char first, char last, int ndx)
{
	std::string	str			= "";
	int			alphabetLen	= last - first + 1;

	while (ndx > alphabetLen)
	{
		str.insert(str.begin(), (char)(first + ((ndx - 1) % alphabetLen)));
		ndx = (ndx - 1) / alphabetLen;
	}

	str.insert(str.begin(), (char)(first + (ndx % (alphabetLen + 1)) - 1));

	return str;
}

} // anonymous

RandomInterfaceBlockCase::RandomInterfaceBlockCase (tcu::TestContext&		testCtx,
													const std::string&		name,
													const std::string&		description,
													const TestStageFlags	testStageFlags,
													deUint32				features,
													deUint32				seed)
	: InterfaceBlockCase		(testCtx, name, description, LOAD_FULL_MATRIX, testStageFlags, (features & FEATURE_OUT_OF_ORDER_OFFSETS) != 0u)
	, m_features				(features)
	, m_explicitXfbOffsets		((features & (FEATURE_OUT_OF_ORDER_OFFSETS | FEATURE_MISSING_BLOCK_MEMBERS)) != 0u)
	, m_maxBlocks				(3)
	, m_maxInstances			((features & FEATURE_INSTANCE_ARRAYS)	? 3 : 0)
	, m_maxArrayLength			((features & FEATURE_ARRAYS)			? 4 : 0)
	, m_maxStructDepth			((features & FEATURE_STRUCTS)			? 2 : 0)
	, m_maxBlockMembers			(3)
	, m_maxStructMembers		(3)
	, m_seed					(seed)
	, m_blockNdx				(1)
	, m_interfaceNdx			(1)
	, m_structNdx				(1)
	, m_primitiveTypeCandidates	(fillTypeCandidates())
{
	de::Random rnd(m_seed);

	int				numBlocks	= rnd.getInt(1, m_maxBlocks);
	InterfaceFlags	stage		= static_cast<InterfaceFlags>(LAYOUT_XFBBUFFER | LAYOUT_XFBOFFSET);

	for (int ndx = 0; ndx < numBlocks; ndx++)
		generateBlock(rnd, stage);

	// m_primitiveTypeCandidates is required during generation only
	m_primitiveTypeCandidates.clear();

	init();
}

std::vector<glu::DataType> RandomInterfaceBlockCase::fillTypeCandidates()
{
	std::vector<glu::DataType> typeCandidates;

	typeCandidates.reserve(32);

	typeCandidates.push_back(glu::TYPE_FLOAT);
	typeCandidates.push_back(glu::TYPE_INT);
	typeCandidates.push_back(glu::TYPE_UINT);

	if (m_features & FEATURE_DOUBLES)
		typeCandidates.push_back(glu::TYPE_DOUBLE);

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

		if (m_features & FEATURE_DOUBLES)
		{
			typeCandidates.push_back(glu::TYPE_DOUBLE_VEC2);
			typeCandidates.push_back(glu::TYPE_DOUBLE_VEC3);
			typeCandidates.push_back(glu::TYPE_DOUBLE_VEC4);
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

		if (m_features & FEATURE_DOUBLES)
		{
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT2);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT2X3);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT3X2);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT3);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT3X4);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT4X2);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT4X3);
			typeCandidates.push_back(glu::TYPE_DOUBLE_MAT4);
		}
	}

	return typeCandidates;
}

void RandomInterfaceBlockCase::generateBlock (de::Random& rnd, deUint32 layoutFlags)
{
	DE_ASSERT(m_blockNdx <= 'z' - 'a');

	const float		instanceArrayWeight		= 0.3f;
	InterfaceBlock&	block					= m_interface.allocBlock(std::string("Block") + (char)('A' + m_blockNdx));
	int				numInstances			= (m_maxInstances > 0 && rnd.getFloat() < instanceArrayWeight) ? rnd.getInt(0, m_maxInstances) : 0;
	int				numBlockMembers			= rnd.getInt(1, m_maxBlockMembers);
	int				numUnassignedOrMissing	= 0;

	if (numInstances > 0)
		block.setArraySize(numInstances);

	if (numInstances > 0 || rnd.getBool())
		block.setInstanceName(std::string("block") + (char)('A' + m_blockNdx));

	block.setFlags(layoutFlags);

	for (int ndx = 0; ndx < numBlockMembers; ndx++)
		generateBlockMember(rnd, block, numBlockMembers, numUnassignedOrMissing);

	m_blockNdx += 1;
}

void RandomInterfaceBlockCase::generateBlockMember (de::Random& rnd, InterfaceBlock& block, const int numBlockMembers, int& numUnassignedOrMissing)
{
	const float		unassignedBlockMembersWeight	= 0.15f;
	const float		missingBlockMembersWeight		= 0.15f;
	const bool		unassignedAllowed				= (m_features & FEATURE_UNASSIGNED_BLOCK_MEMBERS) != 0;
	const bool		missingAllowed					= (m_features & FEATURE_MISSING_BLOCK_MEMBERS) != 0;
	deUint32		flags							= 0;
	std::string		name							= genName('a', 'z', m_interfaceNdx);
	VarType			type							= generateType(rnd, 0, true);

	if (numUnassignedOrMissing < numBlockMembers - 1)
	{
		if (missingAllowed && rnd.getFloat() < missingBlockMembersWeight)
		{
			flags |= FIELD_MISSING;
			numUnassignedOrMissing++;
		}
		else if (unassignedAllowed && rnd.getFloat() < unassignedBlockMembersWeight)
		{
			flags |= FIELD_UNASSIGNED;
			numUnassignedOrMissing++;
		}
	}

	block.addInterfaceMember(InterfaceBlockMember(name, type, flags));

	m_interfaceNdx += 1;
}

VarType RandomInterfaceBlockCase::generateType (de::Random& rnd, int typeDepth, bool arrayOk)
{
	const float structWeight	= 0.1f;
	const float arrayWeight		= 0.1f;

	if (typeDepth < m_maxStructDepth && rnd.getFloat() < structWeight)
	{
		const float				unassignedFieldWeight	= 0.15f;
		const bool				unassignedOk			= (m_features & FEATURE_UNASSIGNED_FIELDS) != 0;
		const int				numMembers				= rnd.getInt(1, m_maxStructMembers);
		std::vector<VarType>	memberTypes;

		// Generate members first so nested struct declarations are in correct order.
		for (int ndx = 0; ndx < numMembers; ndx++)
			memberTypes.push_back(generateType(rnd, typeDepth+1, true));

		StructType& structType = m_interface.allocStruct(std::string("s") + genName('A', 'Z', m_structNdx));
		m_structNdx += 1;

		DE_ASSERT(numMembers <= 'Z' - 'A');
		for (int ndx = 0; ndx < numMembers; ndx++)
		{
			deUint32 flags = 0;

			if (unassignedOk && rnd.getFloat() < unassignedFieldWeight)
			{
				flags |= FIELD_UNASSIGNED;
			}

			structType.addMember(std::string("m") + (char)('A' + ndx), memberTypes[ndx], flags);
		}

		return VarType(&structType, m_explicitXfbOffsets ? static_cast<deUint32>(LAYOUT_XFBOFFSET) : 0u);
	}
	else if (m_maxArrayLength > 0 && arrayOk && rnd.getFloat() < arrayWeight)
	{
		const bool	arraysOfArraysOk	= (m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0;
		const int	arrayLength			= rnd.getInt(1, m_maxArrayLength);
		VarType		elementType			= generateType(rnd, typeDepth, arraysOfArraysOk);

		return VarType(elementType, arrayLength);
	}
	else
	{
		glu::DataType	type	= rnd.choose<glu::DataType>(m_primitiveTypeCandidates.begin(), m_primitiveTypeCandidates.end());
		deUint32		flags	= (m_explicitXfbOffsets ? static_cast<deUint32>(LAYOUT_XFBOFFSET) : 0u);

		if (glu::dataTypeSupportsPrecisionModifier(type))
		{
			// Precision.
			static const deUint32 precisionCandidates[] = { PRECISION_LOW, PRECISION_MEDIUM, PRECISION_HIGH };
			flags |= rnd.choose<deUint32>(&precisionCandidates[0], &precisionCandidates[DE_LENGTH_OF_ARRAY(precisionCandidates)]);
		}

		return VarType(type, flags);
	}
}

} // TransformFeedback
} // vkt
