/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC.
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
 * \brief Shared memory model layout tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryModelSharedLayout.hpp"
#include "vktMemoryModelSharedLayoutCase.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkMemUtil.hpp"

namespace vkt
{
namespace MemoryModel
{
using std::string;
using std::vector;

namespace
{

enum FeatureBits
{
	FEATURE_VECTORS				= (1 << 0),
	FEATURE_MATRICES			= (1 << 1),
	FEATURE_ARRAYS				= (1 << 2),
	FEATURE_STRUCTS				= (1 << 3),
	FEATURE_UNUSED_VARS			= (1 << 4),
	FEATURE_UNUSED_MEMBERS		= (1 << 5),
	FEATURE_ARRAYS_OF_ARRAYS	= (1 << 6),
	FEATURE_16BIT_TYPES			= (1 << 7),
	FEATURE_8BIT_TYPES			= (1 << 8),
};

/*--------------------------------------------------------------------*//*!
 * \brief Generates names for shared memory structs and their members.
 * \param first The first character of the alphabet.
 * \param last The last character of the alphabet.
 * \param ndx The index of the name in the alphabet.
 *
 * If the index lies within the range [1, (last-first)+1], returns
 * the character represented by the ASCII code 'first + ndx - 1'
 * as a string.
 *
 * E.g. if "first" is 'a', "last" 'z' and "ndx" is 1, returns a. If "ndx"
 * is 2, returns b and so forth.
 *
 * If "ndx" is greater than the range, the function keeps dividing it by
 * the alphabet length until the index is within the range. In each iteration,
 * the name is prefixed with the ASCII character represented by the modulo
 * of the index.
 *
 * E.g. if "first" is 'a', "last" 'z' and "ndx" is 28, returns ab. If "ndx"
 * is 702, returns aaa and so forth.
 *//*--------------------------------------------------------------------*/
string genName (char first, char last, int ndx)
{
	string	str;
	int		alphabetLen = static_cast<int>(last) - static_cast<int>(first) + 1;

	while (ndx > 0)
	{
		const int asciiCode = static_cast<int>(first) + ((ndx - 1) % alphabetLen);
		str.insert(str.begin(), static_cast<char>(asciiCode));
		ndx = ((ndx - 1) / alphabetLen);
	}

	return str;
}

void createRandomCaseGroup (tcu::TestCaseGroup* parentGroup, tcu::TestContext &testCtx, const char *groupName,
							const char *description, const deUint32 features, const int numCases, deUint32 baseSeed)
{
	tcu::TestCaseGroup *group = new tcu::TestCaseGroup(testCtx, groupName, description);
	parentGroup->addChild(group);

	baseSeed += static_cast<deUint32>(testCtx.getCommandLine().getBaseSeed());

	for (int i = 0; i < numCases; i++)
		group->addChild(new RandomSharedLayoutCase(testCtx, de::toString(i).c_str(), "", features, static_cast<deUint32>(i + baseSeed)));
}
} // anonymous

RandomSharedLayoutCase::RandomSharedLayoutCase (tcu::TestContext &testCtx, const char *name, const char *description,
												deUint32 features, deUint32 seed)
	: SharedLayoutCase(testCtx, name, description)
	, m_features(features)
	, m_maxArrayLength((features & FEATURE_ARRAYS) ? 3 : 0)
	, m_seed(seed)
{
	de::Random rnd(m_seed);

	m_interface.enable16BitTypes(features & FEATURE_16BIT_TYPES);
	m_interface.enable8BitTypes(features & FEATURE_8BIT_TYPES);

	for (int i = 0; i < rnd.getInt(1, m_maxSharedObjects); i++)
		generateSharedMemoryObject(rnd);

	init();
}

/*--------------------------------------------------------------------*//*!
 * \brief Creates definitions for shared memory structs.
 * \param rnd Random value generator used for deciding the type of the variable.
 *
 * Creates definitions for shared memory structs. Each struct's name starts with
 * an upper-case S and its instance name with a lower-case s followed by its index
 * number.
 *//*--------------------------------------------------------------------*/
void RandomSharedLayoutCase::generateSharedMemoryObject (de::Random &rnd)
{
	const string	name			= "S" + de::toString(m_interface.getNumSharedObjects() + 1);
	const string	instanceName	= "s" + de::toString(m_interface.getNumSharedObjects() + 1);
	SharedStruct	&object			= m_interface.allocSharedObject(name, instanceName);
	const int		numVars			= rnd.getInt(2, m_maxSharedObjectMembers);

	for (int i = 0; i < numVars; i++)
		generateSharedMemoryVar(rnd, object);
}

void RandomSharedLayoutCase::generateSharedMemoryVar (de::Random &rnd, SharedStruct &object)
{
	SharedStructVar var;
	var.name = genName('a', 'z', object.getNumMembers() + 1);

	if ((m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0 || (m_features & FEATURE_STRUCTS) != 0)
		var.type = generateType(rnd, 3, true);
	else
		var.type = generateType(rnd, 1, true);

	var.topLevelArraySize = 1;
	if (var.type.isArrayType())
		var.topLevelArraySize = var.type.getArraySize() == glu::VarType::UNSIZED_ARRAY ? 0 : var.type.getArraySize();

	object.addMember(var);
}

glu::VarType RandomSharedLayoutCase::generateType (de::Random &rnd, int typeDepth, bool arrayOk)
{
	const float structWeight	= 0.7f;
	const float arrayWeight		= 0.8f;

	if (typeDepth > 0 && rnd.getFloat() < structWeight && (m_features & FEATURE_STRUCTS))
	{
		vector<glu::VarType>			memberTypes;
		const int						numMembers	= rnd.getInt(1, m_maxStructMembers);

		// Generate members first so nested struct declarations are in correct order.
		for (int i = 0; i < numMembers; i++)
			memberTypes.push_back(generateType(rnd, typeDepth - 1, true));

		const string					name		= "s" + genName('A', 'Z', m_interface.getNumStructs() + 1);
		de::SharedPtr<glu::StructType>	structType	= m_interface.allocStruct(name);

		DE_ASSERT(numMembers <= 'Z' - 'A');
		for (int i = 0; i < numMembers; i++)
			structType.get()->addMember((string("m") + static_cast<char>(('A' + i))).c_str(), memberTypes[i]);

		return glu::VarType(structType.get());
	}
	else if (typeDepth > 0 && m_maxArrayLength > 0 && arrayOk && rnd.getFloat() < arrayWeight)
	{
		const int			arrayLength		= rnd.getInt(1, m_maxArrayLength);
		const bool			childArrayOk	= (m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0;
		const glu::VarType	elementType		= generateType(rnd, typeDepth - 1, childArrayOk);

		return glu::VarType(elementType, arrayLength);
	}
	else
	{
		const float weight8Bit		= (m_features & FEATURE_8BIT_TYPES) ? 0.7f : 0.0f;
		const float weight16Bit		= (m_features & FEATURE_16BIT_TYPES) ? 0.7f : 0.0f;
		const float weightMatrices	= (m_features & FEATURE_MATRICES) ? 0.3f : 0.0f;

		vector<glu::DataType> typeCandidates;
		if (rnd.getFloat() < weight16Bit)
		{
			typeCandidates.push_back(glu::TYPE_UINT16);
			typeCandidates.push_back(glu::TYPE_INT16);
			typeCandidates.push_back(glu::TYPE_FLOAT16);

			if (m_features & FEATURE_VECTORS)
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
		}
		else if (rnd.getFloat() < weight8Bit)
		{
			typeCandidates.push_back(glu::TYPE_UINT8);
			typeCandidates.push_back(glu::TYPE_INT8);

			if (m_features & FEATURE_VECTORS)
			{
				typeCandidates.push_back(glu::TYPE_INT8_VEC2);
				typeCandidates.push_back(glu::TYPE_INT8_VEC3);
				typeCandidates.push_back(glu::TYPE_INT8_VEC4);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC2);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC3);
				typeCandidates.push_back(glu::TYPE_UINT8_VEC4);
			}
		}
		else
		{
			typeCandidates.push_back(glu::TYPE_FLOAT);
			typeCandidates.push_back(glu::TYPE_INT);
			typeCandidates.push_back(glu::TYPE_UINT);
			typeCandidates.push_back(glu::TYPE_BOOL);

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
			}
		}

		if (rnd.getFloat() < weightMatrices)
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
			const glu::Precision precisionCandidates[]	= { glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP};
			precision									= rnd.choose<glu::Precision>(&precisionCandidates[0],
													&precisionCandidates[DE_LENGTH_OF_ARRAY(precisionCandidates)]);
		}
		else
			precision = glu::PRECISION_LAST;

		return glu::VarType(type, precision);
	}
}

tcu::TestCaseGroup* createSharedMemoryLayoutTests (tcu::TestContext &testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> sharedMemoryLayoutGroup(new tcu::TestCaseGroup(testCtx, "shared", "Shared memory layout tests"));
	tcu::TestCaseGroup *parentGroup = sharedMemoryLayoutGroup.get();
	{
		const deUint32 allBasicTypes	= FEATURE_VECTORS | FEATURE_MATRICES;
		const deUint32 unused			= FEATURE_UNUSED_MEMBERS | FEATURE_UNUSED_VARS;

		for (int i = 0; i < 3; ++i)
		{
			if (i == 1)
			{
				parentGroup = new tcu::TestCaseGroup(testCtx, "16bit", "16bit");
				sharedMemoryLayoutGroup->addChild(parentGroup);
			}
			else if (i == 2)
			{
				parentGroup = new tcu::TestCaseGroup(testCtx, "8bit", "8bit");
				sharedMemoryLayoutGroup->addChild(parentGroup);
			}
			const deUint32 use16BitTypes	= i == 1 ? FEATURE_16BIT_TYPES : 0;
			const deUint32 use8BitTypes		= i == 2 ? FEATURE_8BIT_TYPES : 0;

			createRandomCaseGroup(parentGroup, testCtx, "scalar_types", "Scalar types only",
								use8BitTypes | use16BitTypes | unused, 10, 0);
			createRandomCaseGroup(parentGroup, testCtx, "vector_types", "Scalar and vector types only",
								use8BitTypes | use16BitTypes | unused | FEATURE_VECTORS, 10, 25);
			createRandomCaseGroup(parentGroup, testCtx, "basic_types", "All basic types",
								use8BitTypes | use16BitTypes | unused | allBasicTypes, 10, 50);
			createRandomCaseGroup(parentGroup, testCtx, "basic_arrays", "Arrays",
								use8BitTypes | use16BitTypes | unused | allBasicTypes | FEATURE_ARRAYS, 10, 50);
			createRandomCaseGroup(parentGroup, testCtx, "arrays_of_arrays", "Arrays of arrays",
								use8BitTypes | use16BitTypes | unused | allBasicTypes | FEATURE_ARRAYS |
								FEATURE_ARRAYS_OF_ARRAYS, 10, 950);
			createRandomCaseGroup(parentGroup, testCtx, "nested_structs", "Nested structs",
								use8BitTypes | use16BitTypes | unused | allBasicTypes | FEATURE_STRUCTS, 10, 100);
			createRandomCaseGroup(parentGroup, testCtx, "nested_structs_arrays", "Nested structs, arrays",
								use8BitTypes | use16BitTypes | unused | allBasicTypes | FEATURE_STRUCTS |
								FEATURE_ARRAYS | FEATURE_ARRAYS_OF_ARRAYS, 10, 150);
		}
	}

	return sharedMemoryLayoutGroup.release();
}

} // MemoryModel
} // vkt
