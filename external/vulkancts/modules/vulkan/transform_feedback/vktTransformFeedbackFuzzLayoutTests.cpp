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
 * \brief Vulkan Transform Feedback Fuzz Layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackFuzzLayoutCase.hpp"
#include "vktTransformFeedbackRandomLayoutCase.hpp"

#include "tcuCommandLine.hpp"
#include "deStringUtil.hpp"

namespace vkt
{
namespace TransformFeedback
{

namespace
{

class BlockBasicTypeCase : public InterfaceBlockCase
{
public:
	BlockBasicTypeCase (tcu::TestContext&	testCtx,
						const std::string&	name,
						const std::string&	description,
						const VarType&		type,
						deUint32			layoutFlags,
						int					numInstances,
						MatrixLoadFlags		matrixLoadFlag,
						TestStageFlags		testStageFlags)
		: InterfaceBlockCase(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("var", type, 0));

		VarType tempType = type;
		while (tempType.isArrayType())
		{
			tempType = tempType.getElementType();
		}

		block.setFlags(layoutFlags);

		if (numInstances > 0)
		{
			block.setArraySize(numInstances);
			block.setInstanceName("block");
		}
	}
};

void createBlockBasicTypeCases (tcu::TestCaseGroup& group, tcu::TestContext& testCtx, const std::string& name, const VarType& type, deUint32 layoutFlags, int numInstances = 0)
{
	de::MovePtr<tcu::TestCaseGroup>	typeGroup(new tcu::TestCaseGroup(group.getTestContext(), name.c_str(), ""));

	typeGroup->addChild(new BlockBasicTypeCase(testCtx, "vertex",	"", type, layoutFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
	typeGroup->addChild(new BlockBasicTypeCase(testCtx, "geometry",	"", type, layoutFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));

	group.addChild(typeGroup.release());
}

class BlockSingleStructCase : public InterfaceBlockCase
{
public:
	BlockSingleStructCase (tcu::TestContext&	testCtx,
						   const std::string&	name,
						   const std::string&	description,
						   deUint32				layoutFlags,
						   int					numInstances,
						   MatrixLoadFlags		matrixLoadFlag,
						   TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH), FIELD_UNASSIGNED); // First member is unused.
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_VEC3, PRECISION_HIGH), 2));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM));

		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("s", VarType(&typeS), 0));
		block.setFlags(layoutFlags);

		if (numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(numInstances);
		}
	}
};

class BlockSingleStructArrayCase : public InterfaceBlockCase
{
public:
	BlockSingleStructArrayCase (tcu::TestContext&	testCtx,
								const std::string&	name,
								const std::string&	description,
								deUint32			layoutFlags,
								int					numInstances,
								MatrixLoadFlags		matrixLoadFlag,
								TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH), FIELD_UNASSIGNED);
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM), 2));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT, PRECISION_HIGH));

		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("u", VarType(glu::TYPE_UINT, PRECISION_LOW)));
		block.addInterfaceMember(InterfaceBlockMember("s", VarType(VarType(&typeS), 2)));
		block.addInterfaceMember(InterfaceBlockMember("v", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_MEDIUM)));
		block.setFlags(layoutFlags);

		if (numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(numInstances);
		}
	}
};

class BlockSingleNestedStructCase : public InterfaceBlockCase
{
public:
	BlockSingleNestedStructCase (tcu::TestContext&	testCtx,
								 const std::string&	name,
								 const std::string&	description,
								 deUint32			layoutFlags,
								 int				numInstances,
								 MatrixLoadFlags	matrixLoadFlag,
								 TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH));
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM), 2));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT, PRECISION_HIGH), FIELD_UNASSIGNED);

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_FLOAT_VEC3, PRECISION_MEDIUM));
		typeT.addMember("b", VarType(&typeS));

		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("s", VarType(&typeS), 0));
		block.addInterfaceMember(InterfaceBlockMember("v", VarType(glu::TYPE_UINT, PRECISION_LOW), FIELD_UNASSIGNED));
		block.addInterfaceMember(InterfaceBlockMember("t", VarType(&typeT), 0));
		block.addInterfaceMember(InterfaceBlockMember("u", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_HIGH), 0));
		block.setFlags(layoutFlags);

		if (numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(numInstances);
		}
	}
};

class BlockSingleNestedStructArrayCase : public InterfaceBlockCase
{
public:
	BlockSingleNestedStructArrayCase (tcu::TestContext&		testCtx,
									  const std::string&	name,
									  const std::string&	description,
									  deUint32				layoutFlags,
									  int					numInstances,
									  MatrixLoadFlags		matrixLoadFlag,
									  TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(VarType(glu::TYPE_FLOAT, PRECISION_HIGH), 2));

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM));
		typeT.addMember("b", VarType(VarType(&typeS), 2));

		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("s", VarType(&typeS), 0));
		block.addInterfaceMember(InterfaceBlockMember("v", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_LOW), FIELD_UNASSIGNED));
		block.addInterfaceMember(InterfaceBlockMember("t", VarType(VarType(&typeT), 2), 0));
		block.addInterfaceMember(InterfaceBlockMember("u", VarType(glu::TYPE_UINT, PRECISION_HIGH), 0));
		block.setFlags(layoutFlags);

		if (numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(numInstances);
		}
	}
};

class BlockMultiBasicTypesCase : public InterfaceBlockCase
{
public:
	BlockMultiBasicTypesCase (tcu::TestContext&		testCtx,
							  const std::string&	name,
							  const std::string&	description,
							  deUint32				flagsA,
							  deUint32				flagsB,
							  int					numInstances,
							  MatrixLoadFlags		matrixLoadFlag,
							  TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		InterfaceBlock& blockA = m_interface.allocBlock("BlockA");
		blockA.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_FLOAT, PRECISION_HIGH)));
		blockA.addInterfaceMember(InterfaceBlockMember("b", VarType(glu::TYPE_UINT_VEC3, PRECISION_LOW), FIELD_UNASSIGNED));
		blockA.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM)));
		blockA.setInstanceName("blockA");
		blockA.setFlags(flagsA);

		InterfaceBlock& blockB = m_interface.allocBlock("BlockB");
		blockB.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM)));
		blockB.addInterfaceMember(InterfaceBlockMember("b", VarType(glu::TYPE_INT_VEC2, PRECISION_LOW)));
		blockB.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH), FIELD_UNASSIGNED));
		blockB.addInterfaceMember(InterfaceBlockMember("d", VarType(glu::TYPE_INT, 0)));
		blockB.setInstanceName("blockB");
		blockB.setFlags(flagsB);

		if (numInstances > 0)
		{
			blockA.setArraySize(numInstances);
			blockB.setArraySize(numInstances);
		}
	}
};

class BlockMultiNestedStructCase : public InterfaceBlockCase
{
public:
	BlockMultiNestedStructCase (tcu::TestContext&	testCtx,
								const std::string&	name,
								const std::string&	description,
								deUint32			flagsA,
								deUint32			flagsB,
								int					numInstances,
								MatrixLoadFlags		matrixLoadFlag,
								TestStageFlags		testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_LOW));
		typeS.addMember("b", VarType(VarType(glu::TYPE_INT_VEC2, PRECISION_MEDIUM), 2));

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_UINT, PRECISION_MEDIUM), FIELD_UNASSIGNED);
		typeT.addMember("b", VarType(&typeS));
		typeT.addMember("c", VarType(glu::TYPE_UINT_VEC3, 0));

		InterfaceBlock& blockA = m_interface.allocBlock("BlockA");
		blockA.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_FLOAT, PRECISION_HIGH)));
		blockA.addInterfaceMember(InterfaceBlockMember("b", VarType(&typeS)));
		blockA.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_UINT, PRECISION_LOW), FIELD_UNASSIGNED));
		blockA.setInstanceName("blockA");
		blockA.setFlags(flagsA);

		InterfaceBlock& blockB = m_interface.allocBlock("BlockB");
		blockB.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM)));
		blockB.addInterfaceMember(InterfaceBlockMember("b", VarType(&typeT)));
		blockB.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_INT, 0)));
		blockB.setInstanceName("blockB");
		blockB.setFlags(flagsB);

		if (numInstances > 0)
		{
			blockA.setArraySize(numInstances);
			blockB.setArraySize(numInstances);
		}
	}
};

class BlockVariousBuffersCase : public InterfaceBlockCase
{
public:
	BlockVariousBuffersCase (tcu::TestContext&		testCtx,
							 const std::string&		name,
							 const std::string&		description,
							 deUint32				flags,
							 deUint32				xfbBufferA,
							 deUint32				xfbBufferB,
							 deUint32				xfbBufferC,
							 int					numInstances,
							 MatrixLoadFlags		matrixLoadFlag,
							 TestStageFlags			testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(VarType(glu::TYPE_FLOAT, PRECISION_LOW), 3));
		typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_VEC2, PRECISION_MEDIUM), 2));
		typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH));

		StructType& typeT = m_interface.allocStruct("T");
		typeT.addMember("a", VarType(glu::TYPE_UINT, PRECISION_MEDIUM), FIELD_UNASSIGNED);
		typeT.addMember("b", VarType(glu::TYPE_INT_VEC3, 0));

		InterfaceBlock& blockA = m_interface.allocBlock("BlockA");
		blockA.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_INT, PRECISION_HIGH)));
		blockA.addInterfaceMember(InterfaceBlockMember("b", VarType(&typeS)));
		blockA.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_UINT_VEC3, PRECISION_LOW), FIELD_UNASSIGNED));
		blockA.setInstanceName("blockA");
		blockA.setFlags(flags);
		blockA.setXfbBuffer(xfbBufferA);

		InterfaceBlock& blockB = m_interface.allocBlock("BlockB");
		blockB.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM)));
		blockB.addInterfaceMember(InterfaceBlockMember("b", VarType(&typeT)));
		blockB.addInterfaceMember(InterfaceBlockMember("c", VarType(glu::TYPE_INT_VEC4, 0), FIELD_UNASSIGNED));
		blockB.addInterfaceMember(InterfaceBlockMember("d", VarType(glu::TYPE_INT, 0)));
		blockB.setInstanceName("blockB");
		blockB.setFlags(flags);
		blockB.setXfbBuffer(xfbBufferB);

		InterfaceBlock& blockC = m_interface.allocBlock("BlockC");
		blockC.addInterfaceMember(InterfaceBlockMember("a", VarType(glu::TYPE_UINT, PRECISION_HIGH)));
		blockC.addInterfaceMember(InterfaceBlockMember("b", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_HIGH)));
		blockC.setInstanceName("blockC");
		blockC.setFlags(flags);
		blockC.setXfbBuffer(xfbBufferC);

		if (numInstances > 0)
		{
			blockA.setArraySize(numInstances);
			blockB.setArraySize(numInstances);
		}
	}
};

class Block2LevelStructArrayCase : public InterfaceBlockCase
{
public:
	Block2LevelStructArrayCase (tcu::TestContext&		testCtx,
								const std::string&		name,
								const std::string&		description,
								deUint32				flags,
								int						numInstances,
								MatrixLoadFlags			matrixLoadFlag,
								TestStageFlags			testStageFlags)
		: InterfaceBlockCase	(testCtx, name, description, matrixLoadFlag, testStageFlags)
	{
		StructType& typeS = m_interface.allocStruct("S");
		typeS.addMember("a", VarType(glu::TYPE_UINT_VEC3, PRECISION_HIGH), FIELD_UNASSIGNED);
		typeS.addMember("b", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM));

		InterfaceBlock& block = m_interface.allocBlock("Block");
		block.addInterfaceMember(InterfaceBlockMember("u", VarType(glu::TYPE_INT, PRECISION_MEDIUM)));
		block.addInterfaceMember(InterfaceBlockMember("s", VarType(VarType(VarType(&typeS), 2), 2)));
		block.addInterfaceMember(InterfaceBlockMember("v", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_MEDIUM)));
		block.setFlags(flags);

		if (numInstances > 0)
		{
			block.setInstanceName("block");
			block.setArraySize(numInstances);
		}
	}
};

void createRandomCaseGroup (tcu::TestCaseGroup* parentGroup, tcu::TestContext& testCtx, const char* groupName, const char* description, deUint32 numCases, TestStageFlags testStageFlags, deUint32 features)
{
	const deUint32			baseSeed	= deStringHash(groupName) + static_cast<deUint32>(testCtx.getCommandLine().getBaseSeed());
	tcu::TestCaseGroup*		group		= new tcu::TestCaseGroup(testCtx, groupName, description);

	parentGroup->addChild(group);

	for (deUint32 ndx = 0; ndx < numCases; ndx++)
		group->addChild(new RandomInterfaceBlockCase(testCtx, de::toString(ndx), "", testStageFlags, features, ndx + baseSeed));
}

// InterfaceBlockTests

class InterfaceBlockTests : public tcu::TestCaseGroup
{
public:
							InterfaceBlockTests		(tcu::TestContext& testCtx);
							~InterfaceBlockTests	(void);

	void					init					(void);

private:
							InterfaceBlockTests		(const InterfaceBlockTests& other);
	InterfaceBlockTests&	operator=				(const InterfaceBlockTests& other);
};

InterfaceBlockTests::InterfaceBlockTests (tcu::TestContext& testCtx)
	: TestCaseGroup(testCtx, "fuzz", "Transform feedback fuzz tests")
{
}

InterfaceBlockTests::~InterfaceBlockTests (void)
{
}

void InterfaceBlockTests::init (void)
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
		glu::TYPE_FLOAT_MAT2,
		glu::TYPE_FLOAT_MAT3,
		glu::TYPE_FLOAT_MAT4,
		glu::TYPE_FLOAT_MAT2X3,
		glu::TYPE_FLOAT_MAT2X4,
		glu::TYPE_FLOAT_MAT3X2,
		glu::TYPE_FLOAT_MAT3X4,
		glu::TYPE_FLOAT_MAT4X2,
		glu::TYPE_FLOAT_MAT4X3,
		glu::TYPE_DOUBLE,
		glu::TYPE_DOUBLE_VEC2,
		glu::TYPE_DOUBLE_VEC3,
		glu::TYPE_DOUBLE_VEC4,
		glu::TYPE_DOUBLE_MAT2,
		glu::TYPE_DOUBLE_MAT2X3,
		glu::TYPE_DOUBLE_MAT2X4,
		glu::TYPE_DOUBLE_MAT3X2,
		glu::TYPE_DOUBLE_MAT3,
		glu::TYPE_DOUBLE_MAT3X4,
		glu::TYPE_DOUBLE_MAT4X2,
		glu::TYPE_DOUBLE_MAT4X3,
		glu::TYPE_DOUBLE_MAT4,
	};

	static const struct
	{
		const std::string	name;
		deUint32			flags;
	} precisionFlags[] =
	{
		// TODO remove PRECISION_LOW because both PRECISION_LOW and PRECISION_MEDIUM means relaxed precision?
		{ "lowp",		PRECISION_LOW		},
		{ "mediump",	PRECISION_MEDIUM	},
		{ "highp",		PRECISION_HIGH		}
	};
	const deUint32	defaultFlags	= LAYOUT_XFBBUFFER | LAYOUT_XFBOFFSET;

	// .2_level_array
	{
		tcu::TestCaseGroup* nestedArrayGroup = new tcu::TestCaseGroup(m_testCtx, "2_level_array", "2-level basic array variable in single buffer");
		addChild(nestedArrayGroup);

		for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
		{
			const glu::DataType	type		= basicTypes[basicTypeNdx];
			const char* const	typeName	= glu::getDataTypeName(type);
			const int			childSize	= 2;
			const int			parentSize	= 2;
			const VarType		childType	(VarType(type, !dataTypeSupportsPrecisionModifier(type) ? 0 : PRECISION_HIGH), childSize);
			const VarType		parentType	(childType, parentSize);

			createBlockBasicTypeCases(*nestedArrayGroup, m_testCtx, typeName, parentType, defaultFlags);
		}
	}

	// .3_level_array
	{
		tcu::TestCaseGroup* nestedArrayGroup = new tcu::TestCaseGroup(m_testCtx, "3_level_array", "3-level basic array variable in single buffer");
		addChild(nestedArrayGroup);

		for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
		{
			const glu::DataType	type		= basicTypes[basicTypeNdx];
			const char* const	typeName	= glu::getDataTypeName(type);
			const int			childSize0	= 2;
			const int			childSize1	= 2;
			const int			parentSize	= 2;
			const VarType		childType0	(VarType(type, !dataTypeSupportsPrecisionModifier(type) ? 0 : PRECISION_HIGH), childSize0);
			const VarType		childType1	(childType0, childSize1);
			const VarType		parentType	(childType1, parentSize);

			createBlockBasicTypeCases(*nestedArrayGroup, m_testCtx, typeName, parentType, defaultFlags);
		}
	}

	// .2_level_struct_array
	{
		tcu::TestCaseGroup* structArrayArrayGroup = new tcu::TestCaseGroup(m_testCtx, "2_level_struct_array", "Struct array in one interface block");
		addChild(structArrayArrayGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			structArrayArrayGroup->addChild(new Block2LevelStructArrayCase(m_testCtx, (baseName + "_vertex"),	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			structArrayArrayGroup->addChild(new Block2LevelStructArrayCase(m_testCtx, (baseName + "_geometry"),	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .single_basic_type
	{
		tcu::TestCaseGroup* singleBasicTypeGroup = new tcu::TestCaseGroup(m_testCtx, "single_basic_type", "Single basic variable in single buffer");
		addChild(singleBasicTypeGroup);

		for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
		{
			glu::DataType		type = basicTypes[basicTypeNdx];
			const char* const	typeName = glu::getDataTypeName(type);

			if (!dataTypeSupportsPrecisionModifier(type))
				createBlockBasicTypeCases(*singleBasicTypeGroup, m_testCtx, typeName, VarType(type, 0), defaultFlags);
		}

		for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisionFlags); precNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	precGroup(new tcu::TestCaseGroup(m_testCtx, precisionFlags[precNdx].name.c_str(), ""));

			for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
			{
				glu::DataType		type		= basicTypes[basicTypeNdx];
				const char* const	typeName	= glu::getDataTypeName(type);

				if (dataTypeSupportsPrecisionModifier(type))
					createBlockBasicTypeCases(*precGroup, m_testCtx, typeName, VarType(type, precisionFlags[precNdx].flags), defaultFlags);
			}
			singleBasicTypeGroup->addChild(precGroup.release());
		}
	}

	// .single_basic_array
	{
		tcu::TestCaseGroup* singleBasicArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_basic_array", "Single basic array variable in single buffer");
		addChild(singleBasicArrayGroup);

		for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
		{
			glu::DataType		type		= basicTypes[basicTypeNdx];
			const char* const	typeName	= glu::getDataTypeName(type);
			const int			arraySize	= 3;

			createBlockBasicTypeCases(*singleBasicArrayGroup, m_testCtx, typeName,
										VarType(VarType(type, !dataTypeSupportsPrecisionModifier(type) ? 0 : PRECISION_HIGH), arraySize),
										defaultFlags);
		}
	}

	// .single_struct
	{
		tcu::TestCaseGroup* singleStructGroup = new tcu::TestCaseGroup(m_testCtx, "single_struct", "Single struct in interface block");
		addChild(singleStructGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 3 : 0;

			singleStructGroup->addChild(new BlockSingleStructCase(m_testCtx, baseName + "_vertex",		"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			singleStructGroup->addChild(new BlockSingleStructCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .single_struct_array
	{
		tcu::TestCaseGroup* singleStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_struct_array", "Struct array in one interface block");
		addChild(singleStructArrayGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			singleStructArrayGroup->addChild(new BlockSingleStructArrayCase(m_testCtx, baseName + "_vertex",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			singleStructArrayGroup->addChild(new BlockSingleStructArrayCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .single_nested_struct
	{
		tcu::TestCaseGroup* singleNestedStructGroup = new tcu::TestCaseGroup(m_testCtx, "single_nested_struct", "Nested struct in one interface block");
		addChild(singleNestedStructGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			singleNestedStructGroup->addChild(new BlockSingleNestedStructCase(m_testCtx, baseName + "_vertex",		"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			singleNestedStructGroup->addChild(new BlockSingleNestedStructCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .single_nested_struct_array
	{
		tcu::TestCaseGroup* singleNestedStructArrayGroup = new tcu::TestCaseGroup(m_testCtx, "single_nested_struct_array", "Nested struct array in one interface block");
		addChild(singleNestedStructArrayGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			singleNestedStructArrayGroup->addChild(new BlockSingleNestedStructArrayCase(m_testCtx, baseName + "_vertex",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			singleNestedStructArrayGroup->addChild(new BlockSingleNestedStructArrayCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .instance_array_basic_type
	{
		tcu::TestCaseGroup* instanceArrayBasicTypeGroup = new tcu::TestCaseGroup(m_testCtx, "instance_array_basic_type", "Single basic variable in instance array");
		addChild(instanceArrayBasicTypeGroup);

		for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
		{
			glu::DataType		type			= basicTypes[basicTypeNdx];
			const char* const	typeName		= glu::getDataTypeName(type);
			const int			numInstances	= 3;

			createBlockBasicTypeCases(*instanceArrayBasicTypeGroup, m_testCtx, typeName,
										VarType(type, !dataTypeSupportsPrecisionModifier(type) ? 0 : PRECISION_HIGH),
										defaultFlags, numInstances);
		}
	}

	// .multi_basic_types
	{
		tcu::TestCaseGroup* multiBasicTypesGroup = new tcu::TestCaseGroup(m_testCtx, "multi_basic_types", "Multiple buffers with basic types");
		addChild(multiBasicTypesGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			multiBasicTypesGroup->addChild(new BlockMultiBasicTypesCase(m_testCtx, baseName + "_vertex",	"", defaultFlags,	defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			multiBasicTypesGroup->addChild(new BlockMultiBasicTypesCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .multi_nested_struct
	{
		tcu::TestCaseGroup* multiNestedStructGroup = new tcu::TestCaseGroup(m_testCtx, "multi_nested_struct", "Multiple buffers with nested structs");
		addChild(multiNestedStructGroup);

		for (int isArray = 0; isArray < 2; isArray++)
		{
			const std::string	baseName		= isArray ? "instance_array" : "std";
			const int			numInstances	= isArray ? 2 : 0;

			multiNestedStructGroup->addChild(new BlockMultiNestedStructCase(m_testCtx, baseName + "_vertex",	"", defaultFlags,	defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
			multiNestedStructGroup->addChild(new BlockMultiNestedStructCase(m_testCtx, baseName + "_geometry",	"", defaultFlags,	defaultFlags,	numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
		}
	}

	// .various_buffers
	{
		static const struct
		{
			const std::string	name;
			deUint32			bufferA;
			deUint32			bufferB;
			deUint32			bufferC;
		} xfbBufferNumbers[] =
		{
			{ "000",	0,	0,	0	},
			{ "010",	0,	1,	0	},
			{ "100",	1,	0,	0	},
			{ "110",	1,	1,	0	},
		};

		tcu::TestCaseGroup* multiNestedStructGroup = new tcu::TestCaseGroup(m_testCtx, "various_buffers", "Output data into several transform feedback buffers");
		addChild(multiNestedStructGroup);

		for (int xfbBufferNdx = 0; xfbBufferNdx < DE_LENGTH_OF_ARRAY(xfbBufferNumbers); xfbBufferNdx++)
		{
			const deUint32 bufferA = xfbBufferNumbers[xfbBufferNdx].bufferA;
			const deUint32 bufferB = xfbBufferNumbers[xfbBufferNdx].bufferB;
			const deUint32 bufferC = xfbBufferNumbers[xfbBufferNdx].bufferC;

			for (int isArray = 0; isArray < 2; isArray++)
			{
				std::string	baseName		= "buffers" + xfbBufferNumbers[xfbBufferNdx].name;
				const int	numInstances	= isArray ? 2 : 0;

				if (isArray)
					baseName += "_instance_array";

				multiNestedStructGroup->addChild(new BlockVariousBuffersCase(m_testCtx, baseName + "_vertex",	"", defaultFlags, bufferA, bufferB, bufferC, numInstances, LOAD_FULL_MATRIX, TEST_STAGE_VERTEX));
				multiNestedStructGroup->addChild(new BlockVariousBuffersCase(m_testCtx, baseName + "_geometry",	"", defaultFlags, bufferA, bufferB, bufferC, numInstances, LOAD_FULL_MATRIX, TEST_STAGE_GEOMETRY));
			}
		}
	}

	// .random
	{
		static const struct
		{
			std::string		name;
			TestStageFlags	testStageFlags;
		}
		stages[] =
		{
			{ "vertex",		TEST_STAGE_VERTEX	},
			{ "geometry",	TEST_STAGE_GEOMETRY	},
		};

		for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
		{
			const std::string		groupName		= "random_" + stages[stageNdx].name;
			const TestStageFlags	stage			= stages[stageNdx].testStageFlags;
			const deUint32			allBasicTypes	= FEATURE_VECTORS|FEATURE_MATRICES|FEATURE_DOUBLES;
			const deUint32			unused			= FEATURE_UNASSIGNED_FIELDS|FEATURE_UNASSIGNED_BLOCK_MEMBERS;
			const deUint32			disabled		= FEATURE_INSTANCE_ARRAYS|FEATURE_MISSING_BLOCK_MEMBERS|FEATURE_OUT_OF_ORDER_OFFSETS; // OOO & missing offsets handled in a dedicated case group
			const deUint32			allFeatures		= ~disabled;
			const deUint32			numCases		= 50;

			tcu::TestCaseGroup* group = new tcu::TestCaseGroup(m_testCtx, groupName.c_str(), "Random Interface Block cases");
			addChild(group);

			createRandomCaseGroup(group, m_testCtx, "scalar_types",								"Scalar types only, per-block buffers",					numCases, stage, unused								);
			createRandomCaseGroup(group, m_testCtx, "vector_types",								"Scalar and vector types only, per-block buffers",		numCases, stage, unused|FEATURE_VECTORS				);
			createRandomCaseGroup(group, m_testCtx, "basic_types",								"All basic types, per-block buffers",					numCases, stage, unused|allBasicTypes				);
			createRandomCaseGroup(group, m_testCtx, "basic_arrays",								"Arrays, per-block buffers",							numCases, stage, unused|allBasicTypes|FEATURE_ARRAYS);

			createRandomCaseGroup(group, m_testCtx, "basic_instance_arrays",					"Basic instance arrays, per-block buffers",				numCases, stage, unused|allBasicTypes|FEATURE_INSTANCE_ARRAYS								);
			createRandomCaseGroup(group, m_testCtx, "nested_structs",							"Nested structs, per-block buffers",					numCases, stage, unused|allBasicTypes|FEATURE_STRUCTS										);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_arrays",					"Nested structs, arrays, per-block buffers",			numCases, stage, unused|allBasicTypes|FEATURE_STRUCTS|FEATURE_ARRAYS						);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_instance_arrays",			"Nested structs, instance arrays, per-block buffers",	numCases, stage, unused|allBasicTypes|FEATURE_STRUCTS|FEATURE_INSTANCE_ARRAYS				);
			createRandomCaseGroup(group, m_testCtx, "nested_structs_arrays_instance_arrays",	"Nested structs, instance arrays, per-block buffers",	numCases, stage, unused|allBasicTypes|FEATURE_STRUCTS|FEATURE_ARRAYS|FEATURE_INSTANCE_ARRAYS);

			createRandomCaseGroup(group, m_testCtx, "all_instance_array",						"All random features, shared buffer",					numCases*2, stage, allFeatures|FEATURE_INSTANCE_ARRAYS);
			createRandomCaseGroup(group, m_testCtx, "all_unordered_and_instance_array",			"All random features, out of order member offsets",		numCases*2, stage, allFeatures|FEATURE_OUT_OF_ORDER_OFFSETS|FEATURE_INSTANCE_ARRAYS);
			createRandomCaseGroup(group, m_testCtx, "all_missing",								"All random features, missing interface members",		numCases*2, stage, allFeatures|FEATURE_MISSING_BLOCK_MEMBERS);
			createRandomCaseGroup(group, m_testCtx, "all_unordered_and_missing",				"All random features, unordered and missing members",	numCases*2, stage, allFeatures|FEATURE_OUT_OF_ORDER_OFFSETS|FEATURE_MISSING_BLOCK_MEMBERS);
		}
	}
}

} // anonymous

tcu::TestCaseGroup*	createTransformFeedbackFuzzLayoutTests (tcu::TestContext& testCtx)
{
	return new InterfaceBlockTests(testCtx);
}

} // TransformFeedback
} // vkt
