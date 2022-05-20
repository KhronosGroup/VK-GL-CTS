/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief Mesh Shader In Out Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderInOutTestsEXT.hpp"
#include "vktTestCase.hpp"
#include "vktMeshShaderUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <limits>
#include <vector>
#include <sstream>
#include <memory>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

// Tests checking varied interfaces between task, mesh and frag.

// Output images will use this format.
VkFormat getOutputFormat ()
{
	return VK_FORMAT_R8G8B8A8_UNORM;
}

// Threshold that's reasonable for the previous format.
float getCompareThreshold ()
{
	return 0.005f; // 1/256 < 0.005 < 2/256
}

enum class Owner
{
	VERTEX = 0,
	PRIMITIVE,
};

enum class DataType
{
	INTEGER = 0,
	FLOAT,
};

// Note: 8-bit variables not available for Input/Output.
enum class BitWidth
{
	B64 = 64,
	B32 = 32,
	B16 = 16,
};

enum class DataDim
{
	SCALAR = 1,
	VEC2   = 2,
	VEC3   = 3,
	VEC4   = 4,
};

enum class Interpolation
{
	NORMAL = 0,
	FLAT,
};

enum class Direction
{
	IN = 0,
	OUT,
};

// Interface variable.
struct IfaceVar
{
	static constexpr uint32_t kNumVertices		= 4u;
	static constexpr uint32_t kNumPrimitives	= 2u;
	static constexpr uint32_t kVarsPerType		= 2u;

	IfaceVar (Owner owner_, DataType dataType_, BitWidth bitWidth_, DataDim dataDim_, Interpolation interpolation_, uint32_t index_)
		: owner			(owner_)
		, dataType		(dataType_)
		, bitWidth		(bitWidth_)
		, dataDim		(dataDim_)
		, interpolation	(interpolation_)
		, index			(index_)
		{
			DE_ASSERT(!(dataType == DataType::INTEGER && interpolation == Interpolation::NORMAL));
			DE_ASSERT(!(owner == Owner::PRIMITIVE && interpolation == Interpolation::NORMAL));
			DE_ASSERT(!(dataType == DataType::FLOAT && bitWidth == BitWidth::B64 && interpolation == Interpolation::NORMAL));
			DE_ASSERT(index < kVarsPerType);
		}

	// This constructor needs to be defined for the code to compile, but it should never be actually called.
	// To make sure it's not used, the index is defined to be very large, which should trigger the assertion in getName() below.
	IfaceVar ()
		: owner			(Owner::VERTEX)
		, dataType		(DataType::FLOAT)
		, bitWidth		(BitWidth::B32)
		, dataDim		(DataDim::VEC4)
		, interpolation	(Interpolation::NORMAL)
		, index			(std::numeric_limits<uint32_t>::max())
		{
		}

	Owner			owner;
	DataType		dataType;
	BitWidth		bitWidth;
	DataDim			dataDim;
	Interpolation	interpolation;
	uint32_t		index; // In case there are several variables matching this type.

	// The variable name will be unique and depend on its type.
	std::string getName () const
	{
		DE_ASSERT(index < kVarsPerType);

		std::ostringstream name;
		name
			<< ((owner == Owner::VERTEX) ? "vert" : "prim") << "_"
			<< ((dataType == DataType::INTEGER) ? "i" : "f") << static_cast<int>(bitWidth)
			<< "d" << static_cast<int>(dataDim) << "_"
			<< ((interpolation == Interpolation::NORMAL) ? "inter" : "flat") << "_"
			<< index
			;
		return name.str();
	}

	// Get location size according to the type.
	uint32_t getLocationSize () const
	{
		return ((bitWidth == BitWidth::B64 && dataDim >= DataDim::VEC3) ? 2u : 1u);
	}

	// Get the variable type in GLSL.
	std::string getGLSLType () const
	{
		const auto widthStr		= std::to_string(static_cast<int>(bitWidth));
		const auto dimStr		= std::to_string(static_cast<int>(dataDim));
		const auto shortTypeStr	= ((dataType == DataType::INTEGER) ? "i" : "f");
		const auto typeStr		= ((dataType == DataType::INTEGER) ? "int" : "float");

		if (dataDim == DataDim::SCALAR)
			return typeStr + widthStr + "_t";				// e.g. int32_t or float16_t
		return shortTypeStr + widthStr + "vec" + dimStr;	// e.g. i16vec2 or f64vec4.
	}

	// Get a simple declaration of type and name. This can be reused for several things.
	std::string getTypeAndName () const
	{
		return getGLSLType() + " " + getName();
	}

	std::string getTypeAndNameDecl (bool arrayDecl = false) const
	{
		std::ostringstream decl;
		decl << "    " << getTypeAndName();
		if (arrayDecl)
			decl << "[" << ((owner == Owner::PRIMITIVE) ? IfaceVar::kNumPrimitives : IfaceVar::kNumVertices) << "]";
		decl << ";\n";
		return decl.str();
	}

	// Variable declaration statement given its location and direction.
	std::string getLocationDecl (size_t location, Direction direction) const
	{
		std::ostringstream decl;
		decl
			<< "layout (location=" << location << ") "
			<< ((direction == Direction::IN) ? "in" : "out") << " "
			<< ((owner == Owner::PRIMITIVE) ? "perprimitiveEXT " : "")
			<< ((interpolation == Interpolation::FLAT) ? "flat " : "")
			<< getTypeAndName()
			<< ((direction == Direction::OUT) ? "[]" : "") << ";\n"
			;
		return decl.str();
	}

	// Get the name of the source data for this variable. Tests will use a storage buffer for the per-vertex data and a uniform
	// buffer for the per-primitive data. The names in those will match.
	std::string getDataSourceName () const
	{
		// per-primitive data or per-vertex data buffers.
		return ((owner == Owner::PRIMITIVE) ? "ppd" : "pvd") + ("." + getName());
	}

	// Get the boolean check variable name (see below).
	std::string getCheckName () const
	{
		return "good_" + getName();
	}

	// Get the check statement that would be used in the fragment shader.
	std::string getCheckStatement () const
	{
		std::ostringstream	check;
		const auto			sourceName	= getDataSourceName();
		const auto			glslType	= getGLSLType();
		const auto			name		= getName();

		check << "    bool " << getCheckName() << " = ";
		if (owner == Owner::VERTEX)
		{
			// There will be 4 values in the buffers.
			std::ostringstream maxElem;
			std::ostringstream minElem;

			maxElem << glslType << "(max(max(max(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName  << "[2]), " << sourceName << "[3]))";
			minElem << glslType << "(min(min(min(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName  << "[2]), " << sourceName << "[3]))";

			if (dataDim == DataDim::SCALAR)
			{
				check << "(" << name << " <= " << maxElem.str() << ") && (" << name << " >= " << minElem.str() << ")";
			}
			else
			{
				check << "all(lessThanEqual(" << name << ", " << maxElem.str() << ")) && "
				      << "all(greaterThanEqual(" << name << ", " << minElem.str() << "))";
			}
		}
		else if (owner == Owner::PRIMITIVE)
		{
			// There will be 2 values in the buffers.
			check << "((gl_PrimitiveID == 0 || gl_PrimitiveID == 1) && ("
			      << "(gl_PrimitiveID == 0 && " << name << " == " << sourceName << "[0]) || "
				  << "(gl_PrimitiveID == 1 && " << name << " == " << sourceName << "[1])))";
		}
		check << ";\n";

		return check.str();
	}

	// Get an assignment statement for an out variable.
	std::string getAssignmentStatement (size_t arrayIndex, const std::string& leftPrefix, const std::string& rightPrefix) const
	{
		const auto			name	= getName();
		const auto			typeStr	= getGLSLType();
		std::ostringstream	stmt;

		stmt << "    " << leftPrefix << (leftPrefix.empty() ? "" : ".") << name << "[" << arrayIndex << "] = " << typeStr << "(" << rightPrefix << (rightPrefix.empty() ? "" : ".") << name << "[" << arrayIndex << "]);\n";
		return stmt.str();
	}

	// Get the corresponding array size based on the owner (vertex or primitive)
	uint32_t getArraySize () const
	{
		return ((owner == Owner::PRIMITIVE) ? IfaceVar::kNumPrimitives : IfaceVar::kNumVertices);
	}

};

using IfaceVarVec		= std::vector<IfaceVar>;
using IfaceVarVecPtr	= std::unique_ptr<IfaceVarVec>;

struct InterfaceVariableParams
{
	InterfaceVariableParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_,
							 bool useInt64_, bool useFloat64_, bool useInt16_, bool useFloat16_, IfaceVarVecPtr vars_)
		: taskCount		(taskCount_)
		, meshCount		(meshCount_)
		, width			(width_)
		, height		(height_)
		, useInt64		(useInt64_)
		, useFloat64	(useFloat64_)
		, useInt16		(useInt16_)
		, useFloat16	(useFloat16_)
		, ifaceVars		(std::move(vars_))
	{}

	bool needsTaskShader () const
	{
		return static_cast<bool>(taskCount);
	}

	tcu::UVec3 drawCount () const
	{
		if (needsTaskShader())
			return taskCount.get();
		return meshCount;
	}

	tcu::Maybe<tcu::UVec3>	taskCount;
	tcu::UVec3				meshCount;

	uint32_t				width;
	uint32_t				height;

	// These need to match the list of interface variables.
	bool			useInt64;
	bool			useFloat64;
	bool			useInt16;
	bool			useFloat16;

	IfaceVarVecPtr	ifaceVars;
};

using ParamsPtr = std::unique_ptr<InterfaceVariableParams>;

class InterfaceVariablesCase : public vkt::TestCase
{
public:
					InterfaceVariablesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(std::move(params))
						{}
	virtual			~InterfaceVariablesCase		(void) {}

	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;
	void			initPrograms				(vk::SourceCollections& programCollection) const override;

	// Note data types in the input buffers are always plain floats or ints. They will be converted to the appropriate type when
	// copying them in or out of output variables. Note we have two variables per type, as per IfaceVar::kVarsPerType.

	struct PerVertexData
	{
		// Interpolated floats.

		tcu::Vec4	vert_f64d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f64d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f64d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f64d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f64d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f64d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f64d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f64d1_inter_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f32d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f32d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f32d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f32d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f32d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f32d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f32d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f32d1_inter_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f16d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f16d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f16d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f16d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f16d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f16d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f16d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f16d1_inter_1[IfaceVar::kNumVertices];

		// Flat floats.

		tcu::Vec4	vert_f64d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f64d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f64d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f64d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f64d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f64d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f64d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f64d1_flat_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f32d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f32d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f32d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f32d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f32d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f32d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f32d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f32d1_flat_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f16d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f16d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f16d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f16d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f16d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f16d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f16d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f16d1_flat_1[IfaceVar::kNumVertices];

		// Flat ints.

		tcu::IVec4	vert_i64d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i64d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i64d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i64d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i64d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i64d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i64d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i64d1_flat_1[IfaceVar::kNumVertices];

		tcu::IVec4	vert_i32d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i32d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i32d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i32d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i32d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i32d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i32d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i32d1_flat_1[IfaceVar::kNumVertices];

		tcu::IVec4	vert_i16d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i16d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i16d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i16d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i16d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i16d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i16d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i16d1_flat_1[IfaceVar::kNumVertices];

	};

	struct PerPrimitiveData
	{
		// Flat floats.

		tcu::Vec4	prim_f64d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f64d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f64d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f64d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f64d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f64d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f64d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f64d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec4	prim_f32d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f32d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f32d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f32d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f32d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f32d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f32d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f32d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec4	prim_f16d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f16d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f16d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f16d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f16d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f16d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f16d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f16d1_flat_1[IfaceVar::kNumPrimitives];

		// Flat ints.

		tcu::IVec4	prim_i64d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i64d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i64d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i64d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i64d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i64d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i64d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i64d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec4	prim_i32d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i32d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i32d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i32d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i32d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i32d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i32d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i32d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec4	prim_i16d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i16d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i16d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i16d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i16d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i16d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i16d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i16d1_flat_1[IfaceVar::kNumPrimitives];

	};

	static constexpr uint32_t kGlslangBuiltInCount	= 4u;
	static constexpr uint32_t kMaxLocations			= 16u;

protected:
	ParamsPtr m_params;
};

class InterfaceVariablesInstance : public vkt::TestInstance
{
public:
						InterfaceVariablesInstance	(Context& context, const InterfaceVariableParams* params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~InterfaceVariablesInstance	(void) {}

	void				generateReferenceLevel		();
	tcu::TestStatus		iterate						(void) override;
	bool				verifyResult				(const tcu::ConstPixelBufferAccess& referenceAccess) const;

protected:
	const InterfaceVariableParams*		m_params;
	std::unique_ptr<tcu::TextureLevel>	m_referenceLevel;
};

TestInstance* InterfaceVariablesCase::createInstance (Context& context) const
{
	return new InterfaceVariablesInstance(context, m_params.get());
}

void InterfaceVariablesCase::checkSupport (Context& context) const
{
	const auto params = dynamic_cast<InterfaceVariableParams*>(m_params.get());
	DE_ASSERT(params);

	checkTaskMeshShaderSupportEXT(context, m_params->needsTaskShader(), true);

	if (params->useFloat64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);

	if (params->useInt64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

	if (params->useInt16)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

	if (params->useFloat16)
	{
		const auto& features = context.getShaderFloat16Int8Features();
		if (!features.shaderFloat16)
			TCU_THROW(NotSupportedError, "shaderFloat16 feature not supported");
	}

	if (params->useInt16 || params->useFloat16)
	{
		const auto& features = context.get16BitStorageFeatures();
		if (!features.storageInputOutput16)
			TCU_THROW(NotSupportedError, "storageInputOutput16 feature not supported");
	}

	// glslang will use several built-ins in the generated mesh code, which count against the location and component limits.
	{
		const auto	neededComponents	= (kGlslangBuiltInCount + kMaxLocations) * 4u;
		const auto&	properties			= context.getMeshShaderPropertiesEXT();

		// Making this a TCU_FAIL since the minimum maxMeshOutputComponents is 128, which should allow us to use 32 locations and we
		// use only 16 plus a few built-ins.
		if (neededComponents > properties.maxMeshOutputComponents)
			TCU_FAIL("maxMeshOutputComponents too low to run this test");
	}
}

void InterfaceVariablesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	// Bindings needs to match the PerVertexData and PerPrimitiveData structures.
	std::ostringstream bindings;
	bindings
		<< "layout(set=0, binding=0, std430) readonly buffer PerVertexBlock {\n"
		<< "    vec4   vert_f64d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i64d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i64d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i64d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i64d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i64d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i64d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i64d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i64d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i32d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i32d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i32d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i32d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i32d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i32d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i32d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i32d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i16d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i16d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i16d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i16d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i16d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i16d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i16d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i16d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< " } pvd;\n"
		<< "\n"
		<< "layout(set=0, binding=1, std430) readonly buffer PerPrimitiveBlock {\n"
		<< "    vec4   prim_f64d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f64d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f64d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f64d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f64d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f64d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f64d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f64d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f32d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f32d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f32d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f32d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f32d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f32d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f32d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f32d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f16d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f16d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f16d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f16d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f16d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f16d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f16d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f16d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i64d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i64d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i64d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i64d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i64d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i64d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i64d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i64d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i32d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i32d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i32d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i32d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i32d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i32d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i32d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i32d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i16d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i16d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i16d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i16d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i16d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i16d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i16d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i16d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< " } ppd;\n"
		<< "\n"
		;
	const auto bindingsDecl = bindings.str();

	const auto	params	= dynamic_cast<InterfaceVariableParams*>(m_params.get());
	DE_ASSERT(params);
	const auto&	varVec	= *(params->ifaceVars);

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "\n"
		<< bindingsDecl
		;

	// Declare interface variables as Input in the fragment shader.
	{
		uint32_t usedLocations = 0u;
		for (const auto& var : varVec)
		{
			frag << var.getLocationDecl(usedLocations, Direction::IN);
			usedLocations += var.getLocationSize();
		}
	}

	frag
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		;

	// Emit checks for each variable value in the fragment shader.
	std::ostringstream allConditions;

	for (size_t i = 0; i < varVec.size(); ++i)
	{
		frag << varVec[i].getCheckStatement();
		allConditions << ((i == 0) ? "" : " && ") << varVec[i].getCheckName();
	}

	// Emit final check.
	frag
		<< "    if (" << allConditions.str() << ") {\n"
		<< "        outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    } else {\n"
		<< "        outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;

	std::ostringstream pvdDataDeclStream;
	pvdDataDeclStream
		<< "    vec4 positions[4];\n"
		<< "    float pointSizes[4];\n"
		<< "    float clipDistances[4];\n"
		<< "    vec4 custom1[4];\n"
		<< "    float custom2[4];\n"
		<< "    int custom3[4];\n"
		;
	const auto pvdDataDecl = pvdDataDeclStream.str();

	std::ostringstream ppdDataDeclStream;
	ppdDataDeclStream
		<< "    int primitiveIds[2];\n"
		<< "    int viewportIndices[2];\n"
		<< "    uvec4 custom4[2];\n"
		<< "    float custom5[2];\n"
		;
	const auto ppdDataDecl = ppdDataDeclStream.str();

	std::ostringstream taskDataStream;
	taskDataStream << "struct TaskData {\n";
	for (size_t i = 0; i < varVec.size(); ++i)
		taskDataStream << varVec[i].getTypeAndNameDecl(/*arrayDecl*/true);
	taskDataStream << "};\n\n";
	taskDataStream << "taskPayloadSharedEXT TaskData td;\n";

	const auto taskShader		= m_params->needsTaskShader();
	const auto taskDataDecl		= taskDataStream.str();
	const auto meshPvdPrefix	= (taskShader ? "td" : "pvd");
	const auto meshPpdPrefix	= (taskShader ? "td" : "ppd");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "\n"
		<< "layout (local_size_x=1) in;\n"
		<< "layout (max_primitives=" << IfaceVar::kNumPrimitives << ", max_vertices=" << IfaceVar::kNumVertices << ") out;\n"
		<< "layout (triangles) out;\n"
		<< "\n"
		;

	// Declare interface variables as Output variables.
	{
		uint32_t usedLocations = 0u;
		for (const auto& var : varVec)
		{
			mesh << var.getLocationDecl(usedLocations, Direction::OUT);
			usedLocations += var.getLocationSize();
		}
	}

	mesh
		<< "out gl_MeshPerVertexEXT {\n"
		<< "   vec4  gl_Position;\n"
		<< "} gl_MeshVerticesEXT[];\n"
		<< "out perprimitiveEXT gl_MeshPerPrimitiveEXT {\n"
		<< "  int gl_PrimitiveID;\n"
		<< "} gl_MeshPrimitivesEXT[];\n"
		<< "\n"
		<< (taskShader ? taskDataDecl : bindingsDecl)
		<< "vec4 positions[" << IfaceVar::kNumVertices << "] = vec4[](\n"
		<< "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
		<< "    vec4( 1.0, -1.0, 0.0, 1.0),\n"
		<< "    vec4(-1.0,  1.0, 0.0, 1.0),\n"
		<< "    vec4( 1.0,  1.0, 0.0, 1.0)\n"
		<< ");\n"
		<< "\n"
		<< "uvec3 indices[" << IfaceVar::kNumPrimitives << "] = uvec3[](\n"
		<< "    uvec3(0, 1, 2),\n"
		<< "    uvec3(2, 3, 1)\n"
		<< ");\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(" << IfaceVar::kNumVertices << ", " << IfaceVar::kNumPrimitives << ");\n"
		<< "\n"
		;

	// Emit positions, indices and primitive IDs.
	for (uint32_t i = 0; i < IfaceVar::kNumVertices; ++i)
		mesh << "    gl_MeshVerticesEXT[" << i << "].gl_Position = positions[" << i << "];\n";
	mesh << "\n";

	for (uint32_t i = 0; i < IfaceVar::kNumPrimitives; ++i)
		mesh << "    gl_PrimitiveTriangleIndicesEXT[" << i << "] = indices[" << i << "];\n";
	mesh << "\n";

	for (uint32_t i = 0; i < IfaceVar::kNumPrimitives; ++i)
		mesh << "    gl_MeshPrimitivesEXT[" << i << "].gl_PrimitiveID = " << i << ";\n";
	mesh << "\n";

	// Copy data to output variables, either from the task data or the bindings.
	for (size_t i = 0; i < varVec.size(); ++i)
	{
		const auto arraySize	= varVec[i].getArraySize();
		const auto prefix		= ((varVec[i].owner == Owner::VERTEX) ? meshPvdPrefix : meshPpdPrefix);
		for (uint32_t arrayIndex = 0u; arrayIndex < arraySize; ++arrayIndex)
			mesh << varVec[i].getAssignmentStatement(arrayIndex, "", prefix);
	}

	mesh
		<< "\n"
		<< "}\n"
		;

	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	// Task shader if needed.
	if (taskShader)
	{
		const auto& meshCount		= m_params->meshCount;
		const auto	taskPvdPrefix	= "pvd";
		const auto	taskPpdPrefix	= "ppd";

		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
			<< "\n"
			<< taskDataDecl
			<< bindingsDecl
			<< "void main ()\n"
			<< "{\n"
			;

		// Copy data from bindings to the task data structure.
		for (size_t i = 0; i < varVec.size(); ++i)
		{
			const auto arraySize	= varVec[i].getArraySize();
			const auto prefix		= ((varVec[i].owner == Owner::VERTEX) ? taskPvdPrefix : taskPpdPrefix);

			for (uint32_t arrayIndex = 0u; arrayIndex < arraySize; ++arrayIndex)
				task << varVec[i].getAssignmentStatement(arrayIndex, "td", prefix);
		}

		task
			<< "\n"
			<< "    EmitMeshTasksEXT(" << meshCount.x() << ", " << meshCount.y() << ", " << meshCount.z() << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}
}

void InterfaceVariablesInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();
	const auto blueColor	= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	tcu::clear(access, blueColor);
}

bool InterfaceVariablesInstance::verifyResult (const tcu::ConstPixelBufferAccess& resultAccess) const
{
	const auto&	referenceLevel	= *m_referenceLevel.get();
	const auto	referenceAccess	= referenceLevel.getAccess();

	const auto refWidth		= referenceAccess.getWidth();
	const auto refHeight	= referenceAccess.getHeight();
	const auto refDepth		= referenceAccess.getDepth();

	const auto resWidth		= resultAccess.getWidth();
	const auto resHeight	= resultAccess.getHeight();
	const auto resDepth		= resultAccess.getDepth();

	DE_ASSERT(resWidth == refWidth || resHeight == refHeight || resDepth == refDepth);

	// For release builds.
	DE_UNREF(refWidth);
	DE_UNREF(refHeight);
	DE_UNREF(refDepth);
	DE_UNREF(resWidth);
	DE_UNREF(resHeight);
	DE_UNREF(resDepth);

	const auto outputFormat		= getOutputFormat();
	const auto expectedFormat	= mapVkFormat(outputFormat);
	const auto resFormat		= resultAccess.getFormat();
	const auto refFormat		= referenceAccess.getFormat();

	DE_ASSERT(resFormat == expectedFormat && refFormat == expectedFormat);

	// For release builds
	DE_UNREF(expectedFormat);
	DE_UNREF(resFormat);
	DE_UNREF(refFormat);

	auto&			log				= m_context.getTestContext().getLog();
	const auto		threshold		= getCompareThreshold();
	const tcu::Vec4	thresholdVec	(threshold, threshold, threshold, threshold);

	return tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec, tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus InterfaceVariablesInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		imageFormat	= getOutputFormat();
	const auto		tcuFormat	= mapVkFormat(imageFormat);
	const auto		imageExtent	= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto		imageUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		hasTask		= binaries.contains("task");
	const auto		bufStages	= (VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | (hasTask ? VK_SHADER_STAGE_TASK_BIT_EXT : 0));

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	// Create color image and view.
	ImageWithMemory	colorImage	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorView	= makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// Bindings data.
	// The initialization statements below were generated automatically with a Python script.
	// Note: it works with stdin/stdout.
#if 0
import re
import sys

# Lines look like: tcu::Vec4 vert_f64d4_inter_0[IfaceVar::kNumVertices];
lineRE = re.compile(r'^\s*(\S+)\s+(\w+)\[(\S+)\];.*$')
vecRE = re.compile(r'^.*Vec(\d)$')
floatSuffixes = (
    (0.25, 0.50, 0.875, 0.0),
    (0.25, 0.75, 0.875, 0.0),
    (0.50, 0.50, 0.875, 0.0),
    (0.50, 0.75, 0.875, 0.0),
)
lineCounter = 0

for line in sys.stdin:
    match = lineRE.search(line)
    if not match:
        continue

    varType = match.group(1)
    varName = match.group(2)
    varSize = match.group(3)

    arraySize = (4 if varSize == 'IfaceVar::kNumVertices' else 2)
    vecMatch = vecRE.match(varType)
    numComponents = (1 if not vecMatch else vecMatch.group(1))
    isFlat = '_flat_' in varName

    lineCounter += 1
    varBaseVal = 1000 + 10 * lineCounter
    valueTemplate = ('%s' if numComponents == 1 else '%s(%%s)' % (varType,))

    for index in range(arraySize):
        valueStr = ''
        for comp in range(numComponents):
            compValue = varBaseVal + comp + 1
            if not isFlat:
                compValue += floatSuffixes[index][comp]
            valueStr += ('' if comp == 0 else ', ') + str(compValue)
        value = valueTemplate % (valueStr,)
        statement = '%s[%s] = %s;' % (varName, index, value)
        print('%s' % (statement,))
#endif
	InterfaceVariablesCase::PerVertexData perVertexData;
	{
		perVertexData.vert_f64d4_inter_0[0] = tcu::Vec4(1011.25, 1012.5, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[1] = tcu::Vec4(1011.25, 1012.75, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[2] = tcu::Vec4(1011.5, 1012.5, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[3] = tcu::Vec4(1011.5, 1012.75, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_1[0] = tcu::Vec4(1021.25, 1022.5, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[1] = tcu::Vec4(1021.25, 1022.75, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[2] = tcu::Vec4(1021.5, 1022.5, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[3] = tcu::Vec4(1021.5, 1022.75, 1023.875, 1024.0);
		perVertexData.vert_f64d3_inter_0[0] = tcu::Vec3(1031.25, 1032.5, 1033.875);
		perVertexData.vert_f64d3_inter_0[1] = tcu::Vec3(1031.25, 1032.75, 1033.875);
		perVertexData.vert_f64d3_inter_0[2] = tcu::Vec3(1031.5, 1032.5, 1033.875);
		perVertexData.vert_f64d3_inter_0[3] = tcu::Vec3(1031.5, 1032.75, 1033.875);
		perVertexData.vert_f64d3_inter_1[0] = tcu::Vec3(1041.25, 1042.5, 1043.875);
		perVertexData.vert_f64d3_inter_1[1] = tcu::Vec3(1041.25, 1042.75, 1043.875);
		perVertexData.vert_f64d3_inter_1[2] = tcu::Vec3(1041.5, 1042.5, 1043.875);
		perVertexData.vert_f64d3_inter_1[3] = tcu::Vec3(1041.5, 1042.75, 1043.875);
		perVertexData.vert_f64d2_inter_0[0] = tcu::Vec2(1051.25, 1052.5);
		perVertexData.vert_f64d2_inter_0[1] = tcu::Vec2(1051.25, 1052.75);
		perVertexData.vert_f64d2_inter_0[2] = tcu::Vec2(1051.5, 1052.5);
		perVertexData.vert_f64d2_inter_0[3] = tcu::Vec2(1051.5, 1052.75);
		perVertexData.vert_f64d2_inter_1[0] = tcu::Vec2(1061.25, 1062.5);
		perVertexData.vert_f64d2_inter_1[1] = tcu::Vec2(1061.25, 1062.75);
		perVertexData.vert_f64d2_inter_1[2] = tcu::Vec2(1061.5, 1062.5);
		perVertexData.vert_f64d2_inter_1[3] = tcu::Vec2(1061.5, 1062.75);
		perVertexData.vert_f64d1_inter_0[0] = 1071.25;
		perVertexData.vert_f64d1_inter_0[1] = 1071.25;
		perVertexData.vert_f64d1_inter_0[2] = 1071.5;
		perVertexData.vert_f64d1_inter_0[3] = 1071.5;
		perVertexData.vert_f64d1_inter_1[0] = 1081.25;
		perVertexData.vert_f64d1_inter_1[1] = 1081.25;
		perVertexData.vert_f64d1_inter_1[2] = 1081.5;
		perVertexData.vert_f64d1_inter_1[3] = 1081.5;
		perVertexData.vert_f32d4_inter_0[0] = tcu::Vec4(1091.25, 1092.5, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[1] = tcu::Vec4(1091.25, 1092.75, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[2] = tcu::Vec4(1091.5, 1092.5, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[3] = tcu::Vec4(1091.5, 1092.75, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_1[0] = tcu::Vec4(1101.25, 1102.5, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[1] = tcu::Vec4(1101.25, 1102.75, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[2] = tcu::Vec4(1101.5, 1102.5, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[3] = tcu::Vec4(1101.5, 1102.75, 1103.875, 1104.0);
		perVertexData.vert_f32d3_inter_0[0] = tcu::Vec3(1111.25, 1112.5, 1113.875);
		perVertexData.vert_f32d3_inter_0[1] = tcu::Vec3(1111.25, 1112.75, 1113.875);
		perVertexData.vert_f32d3_inter_0[2] = tcu::Vec3(1111.5, 1112.5, 1113.875);
		perVertexData.vert_f32d3_inter_0[3] = tcu::Vec3(1111.5, 1112.75, 1113.875);
		perVertexData.vert_f32d3_inter_1[0] = tcu::Vec3(1121.25, 1122.5, 1123.875);
		perVertexData.vert_f32d3_inter_1[1] = tcu::Vec3(1121.25, 1122.75, 1123.875);
		perVertexData.vert_f32d3_inter_1[2] = tcu::Vec3(1121.5, 1122.5, 1123.875);
		perVertexData.vert_f32d3_inter_1[3] = tcu::Vec3(1121.5, 1122.75, 1123.875);
		perVertexData.vert_f32d2_inter_0[0] = tcu::Vec2(1131.25, 1132.5);
		perVertexData.vert_f32d2_inter_0[1] = tcu::Vec2(1131.25, 1132.75);
		perVertexData.vert_f32d2_inter_0[2] = tcu::Vec2(1131.5, 1132.5);
		perVertexData.vert_f32d2_inter_0[3] = tcu::Vec2(1131.5, 1132.75);
		perVertexData.vert_f32d2_inter_1[0] = tcu::Vec2(1141.25, 1142.5);
		perVertexData.vert_f32d2_inter_1[1] = tcu::Vec2(1141.25, 1142.75);
		perVertexData.vert_f32d2_inter_1[2] = tcu::Vec2(1141.5, 1142.5);
		perVertexData.vert_f32d2_inter_1[3] = tcu::Vec2(1141.5, 1142.75);
		perVertexData.vert_f32d1_inter_0[0] = 1151.25;
		perVertexData.vert_f32d1_inter_0[1] = 1151.25;
		perVertexData.vert_f32d1_inter_0[2] = 1151.5;
		perVertexData.vert_f32d1_inter_0[3] = 1151.5;
		perVertexData.vert_f32d1_inter_1[0] = 1161.25;
		perVertexData.vert_f32d1_inter_1[1] = 1161.25;
		perVertexData.vert_f32d1_inter_1[2] = 1161.5;
		perVertexData.vert_f32d1_inter_1[3] = 1161.5;
		perVertexData.vert_f16d4_inter_0[0] = tcu::Vec4(1171.25, 1172.5, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[1] = tcu::Vec4(1171.25, 1172.75, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[2] = tcu::Vec4(1171.5, 1172.5, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[3] = tcu::Vec4(1171.5, 1172.75, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_1[0] = tcu::Vec4(1181.25, 1182.5, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[1] = tcu::Vec4(1181.25, 1182.75, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[2] = tcu::Vec4(1181.5, 1182.5, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[3] = tcu::Vec4(1181.5, 1182.75, 1183.875, 1184.0);
		perVertexData.vert_f16d3_inter_0[0] = tcu::Vec3(1191.25, 1192.5, 1193.875);
		perVertexData.vert_f16d3_inter_0[1] = tcu::Vec3(1191.25, 1192.75, 1193.875);
		perVertexData.vert_f16d3_inter_0[2] = tcu::Vec3(1191.5, 1192.5, 1193.875);
		perVertexData.vert_f16d3_inter_0[3] = tcu::Vec3(1191.5, 1192.75, 1193.875);
		perVertexData.vert_f16d3_inter_1[0] = tcu::Vec3(1201.25, 1202.5, 1203.875);
		perVertexData.vert_f16d3_inter_1[1] = tcu::Vec3(1201.25, 1202.75, 1203.875);
		perVertexData.vert_f16d3_inter_1[2] = tcu::Vec3(1201.5, 1202.5, 1203.875);
		perVertexData.vert_f16d3_inter_1[3] = tcu::Vec3(1201.5, 1202.75, 1203.875);
		perVertexData.vert_f16d2_inter_0[0] = tcu::Vec2(1211.25, 1212.5);
		perVertexData.vert_f16d2_inter_0[1] = tcu::Vec2(1211.25, 1212.75);
		perVertexData.vert_f16d2_inter_0[2] = tcu::Vec2(1211.5, 1212.5);
		perVertexData.vert_f16d2_inter_0[3] = tcu::Vec2(1211.5, 1212.75);
		perVertexData.vert_f16d2_inter_1[0] = tcu::Vec2(1221.25, 1222.5);
		perVertexData.vert_f16d2_inter_1[1] = tcu::Vec2(1221.25, 1222.75);
		perVertexData.vert_f16d2_inter_1[2] = tcu::Vec2(1221.5, 1222.5);
		perVertexData.vert_f16d2_inter_1[3] = tcu::Vec2(1221.5, 1222.75);
		perVertexData.vert_f16d1_inter_0[0] = 1231.25;
		perVertexData.vert_f16d1_inter_0[1] = 1231.25;
		perVertexData.vert_f16d1_inter_0[2] = 1231.5;
		perVertexData.vert_f16d1_inter_0[3] = 1231.5;
		perVertexData.vert_f16d1_inter_1[0] = 1241.25;
		perVertexData.vert_f16d1_inter_1[1] = 1241.25;
		perVertexData.vert_f16d1_inter_1[2] = 1241.5;
		perVertexData.vert_f16d1_inter_1[3] = 1241.5;
		perVertexData.vert_f64d4_flat_0[0] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[1] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[2] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[3] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_1[0] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[1] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[2] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[3] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d3_flat_0[0] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[1] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[2] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[3] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_1[0] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[1] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[2] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[3] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d2_flat_0[0] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[1] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[2] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[3] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_1[0] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[1] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[2] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[3] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d1_flat_0[0] = 1311;
		perVertexData.vert_f64d1_flat_0[1] = 1311;
		perVertexData.vert_f64d1_flat_0[2] = 1311;
		perVertexData.vert_f64d1_flat_0[3] = 1311;
		perVertexData.vert_f64d1_flat_1[0] = 1321;
		perVertexData.vert_f64d1_flat_1[1] = 1321;
		perVertexData.vert_f64d1_flat_1[2] = 1321;
		perVertexData.vert_f64d1_flat_1[3] = 1321;
		perVertexData.vert_f32d4_flat_0[0] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[1] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[2] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[3] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_1[0] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[1] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[2] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[3] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d3_flat_0[0] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[1] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[2] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[3] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_1[0] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[1] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[2] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[3] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d2_flat_0[0] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[1] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[2] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[3] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_1[0] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[1] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[2] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[3] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d1_flat_0[0] = 1391;
		perVertexData.vert_f32d1_flat_0[1] = 1391;
		perVertexData.vert_f32d1_flat_0[2] = 1391;
		perVertexData.vert_f32d1_flat_0[3] = 1391;
		perVertexData.vert_f32d1_flat_1[0] = 1401;
		perVertexData.vert_f32d1_flat_1[1] = 1401;
		perVertexData.vert_f32d1_flat_1[2] = 1401;
		perVertexData.vert_f32d1_flat_1[3] = 1401;
		perVertexData.vert_f16d4_flat_0[0] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[1] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[2] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[3] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_1[0] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[1] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[2] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[3] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d3_flat_0[0] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[1] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[2] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[3] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_1[0] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[1] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[2] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[3] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d2_flat_0[0] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[1] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[2] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[3] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_1[0] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[1] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[2] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[3] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d1_flat_0[0] = 1471;
		perVertexData.vert_f16d1_flat_0[1] = 1471;
		perVertexData.vert_f16d1_flat_0[2] = 1471;
		perVertexData.vert_f16d1_flat_0[3] = 1471;
		perVertexData.vert_f16d1_flat_1[0] = 1481;
		perVertexData.vert_f16d1_flat_1[1] = 1481;
		perVertexData.vert_f16d1_flat_1[2] = 1481;
		perVertexData.vert_f16d1_flat_1[3] = 1481;
		perVertexData.vert_i64d4_flat_0[0] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[1] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[2] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[3] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_1[0] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[1] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[2] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[3] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d3_flat_0[0] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[1] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[2] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[3] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_1[0] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[1] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[2] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[3] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d2_flat_0[0] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[1] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[2] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[3] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_1[0] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[1] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[2] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[3] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d1_flat_0[0] = 1551;
		perVertexData.vert_i64d1_flat_0[1] = 1551;
		perVertexData.vert_i64d1_flat_0[2] = 1551;
		perVertexData.vert_i64d1_flat_0[3] = 1551;
		perVertexData.vert_i64d1_flat_1[0] = 1561;
		perVertexData.vert_i64d1_flat_1[1] = 1561;
		perVertexData.vert_i64d1_flat_1[2] = 1561;
		perVertexData.vert_i64d1_flat_1[3] = 1561;
		perVertexData.vert_i32d4_flat_0[0] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[1] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[2] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[3] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_1[0] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[1] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[2] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[3] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d3_flat_0[0] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[1] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[2] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[3] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_1[0] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[1] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[2] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[3] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d2_flat_0[0] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[1] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[2] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[3] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_1[0] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[1] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[2] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[3] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d1_flat_0[0] = 1631;
		perVertexData.vert_i32d1_flat_0[1] = 1631;
		perVertexData.vert_i32d1_flat_0[2] = 1631;
		perVertexData.vert_i32d1_flat_0[3] = 1631;
		perVertexData.vert_i32d1_flat_1[0] = 1641;
		perVertexData.vert_i32d1_flat_1[1] = 1641;
		perVertexData.vert_i32d1_flat_1[2] = 1641;
		perVertexData.vert_i32d1_flat_1[3] = 1641;
		perVertexData.vert_i16d4_flat_0[0] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[1] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[2] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[3] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_1[0] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[1] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[2] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[3] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d3_flat_0[0] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[1] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[2] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[3] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_1[0] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[1] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[2] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[3] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d2_flat_0[0] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[1] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[2] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[3] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_1[0] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[1] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[2] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[3] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d1_flat_0[0] = 1711;
		perVertexData.vert_i16d1_flat_0[1] = 1711;
		perVertexData.vert_i16d1_flat_0[2] = 1711;
		perVertexData.vert_i16d1_flat_0[3] = 1711;
		perVertexData.vert_i16d1_flat_1[0] = 1721;
		perVertexData.vert_i16d1_flat_1[1] = 1721;
		perVertexData.vert_i16d1_flat_1[2] = 1721;
		perVertexData.vert_i16d1_flat_1[3] = 1721;
	}

	InterfaceVariablesCase::PerPrimitiveData perPrimitiveData;
	{
		perPrimitiveData.prim_f64d4_flat_0[0] = tcu::Vec4(1011, 1012, 1013, 1014);
		perPrimitiveData.prim_f64d4_flat_0[1] = tcu::Vec4(1011, 1012, 1013, 1014);
		perPrimitiveData.prim_f64d4_flat_1[0] = tcu::Vec4(1021, 1022, 1023, 1024);
		perPrimitiveData.prim_f64d4_flat_1[1] = tcu::Vec4(1021, 1022, 1023, 1024);
		perPrimitiveData.prim_f64d3_flat_0[0] = tcu::Vec3(1031, 1032, 1033);
		perPrimitiveData.prim_f64d3_flat_0[1] = tcu::Vec3(1031, 1032, 1033);
		perPrimitiveData.prim_f64d3_flat_1[0] = tcu::Vec3(1041, 1042, 1043);
		perPrimitiveData.prim_f64d3_flat_1[1] = tcu::Vec3(1041, 1042, 1043);
		perPrimitiveData.prim_f64d2_flat_0[0] = tcu::Vec2(1051, 1052);
		perPrimitiveData.prim_f64d2_flat_0[1] = tcu::Vec2(1051, 1052);
		perPrimitiveData.prim_f64d2_flat_1[0] = tcu::Vec2(1061, 1062);
		perPrimitiveData.prim_f64d2_flat_1[1] = tcu::Vec2(1061, 1062);
		perPrimitiveData.prim_f64d1_flat_0[0] = 1071;
		perPrimitiveData.prim_f64d1_flat_0[1] = 1071;
		perPrimitiveData.prim_f64d1_flat_1[0] = 1081;
		perPrimitiveData.prim_f64d1_flat_1[1] = 1081;
		perPrimitiveData.prim_f32d4_flat_0[0] = tcu::Vec4(1091, 1092, 1093, 1094);
		perPrimitiveData.prim_f32d4_flat_0[1] = tcu::Vec4(1091, 1092, 1093, 1094);
		perPrimitiveData.prim_f32d4_flat_1[0] = tcu::Vec4(1101, 1102, 1103, 1104);
		perPrimitiveData.prim_f32d4_flat_1[1] = tcu::Vec4(1101, 1102, 1103, 1104);
		perPrimitiveData.prim_f32d3_flat_0[0] = tcu::Vec3(1111, 1112, 1113);
		perPrimitiveData.prim_f32d3_flat_0[1] = tcu::Vec3(1111, 1112, 1113);
		perPrimitiveData.prim_f32d3_flat_1[0] = tcu::Vec3(1121, 1122, 1123);
		perPrimitiveData.prim_f32d3_flat_1[1] = tcu::Vec3(1121, 1122, 1123);
		perPrimitiveData.prim_f32d2_flat_0[0] = tcu::Vec2(1131, 1132);
		perPrimitiveData.prim_f32d2_flat_0[1] = tcu::Vec2(1131, 1132);
		perPrimitiveData.prim_f32d2_flat_1[0] = tcu::Vec2(1141, 1142);
		perPrimitiveData.prim_f32d2_flat_1[1] = tcu::Vec2(1141, 1142);
		perPrimitiveData.prim_f32d1_flat_0[0] = 1151;
		perPrimitiveData.prim_f32d1_flat_0[1] = 1151;
		perPrimitiveData.prim_f32d1_flat_1[0] = 1161;
		perPrimitiveData.prim_f32d1_flat_1[1] = 1161;
		perPrimitiveData.prim_f16d4_flat_0[0] = tcu::Vec4(1171, 1172, 1173, 1174);
		perPrimitiveData.prim_f16d4_flat_0[1] = tcu::Vec4(1171, 1172, 1173, 1174);
		perPrimitiveData.prim_f16d4_flat_1[0] = tcu::Vec4(1181, 1182, 1183, 1184);
		perPrimitiveData.prim_f16d4_flat_1[1] = tcu::Vec4(1181, 1182, 1183, 1184);
		perPrimitiveData.prim_f16d3_flat_0[0] = tcu::Vec3(1191, 1192, 1193);
		perPrimitiveData.prim_f16d3_flat_0[1] = tcu::Vec3(1191, 1192, 1193);
		perPrimitiveData.prim_f16d3_flat_1[0] = tcu::Vec3(1201, 1202, 1203);
		perPrimitiveData.prim_f16d3_flat_1[1] = tcu::Vec3(1201, 1202, 1203);
		perPrimitiveData.prim_f16d2_flat_0[0] = tcu::Vec2(1211, 1212);
		perPrimitiveData.prim_f16d2_flat_0[1] = tcu::Vec2(1211, 1212);
		perPrimitiveData.prim_f16d2_flat_1[0] = tcu::Vec2(1221, 1222);
		perPrimitiveData.prim_f16d2_flat_1[1] = tcu::Vec2(1221, 1222);
		perPrimitiveData.prim_f16d1_flat_0[0] = 1231;
		perPrimitiveData.prim_f16d1_flat_0[1] = 1231;
		perPrimitiveData.prim_f16d1_flat_1[0] = 1241;
		perPrimitiveData.prim_f16d1_flat_1[1] = 1241;
		perPrimitiveData.prim_i64d4_flat_0[0] = tcu::IVec4(1251, 1252, 1253, 1254);
		perPrimitiveData.prim_i64d4_flat_0[1] = tcu::IVec4(1251, 1252, 1253, 1254);
		perPrimitiveData.prim_i64d4_flat_1[0] = tcu::IVec4(1261, 1262, 1263, 1264);
		perPrimitiveData.prim_i64d4_flat_1[1] = tcu::IVec4(1261, 1262, 1263, 1264);
		perPrimitiveData.prim_i64d3_flat_0[0] = tcu::IVec3(1271, 1272, 1273);
		perPrimitiveData.prim_i64d3_flat_0[1] = tcu::IVec3(1271, 1272, 1273);
		perPrimitiveData.prim_i64d3_flat_1[0] = tcu::IVec3(1281, 1282, 1283);
		perPrimitiveData.prim_i64d3_flat_1[1] = tcu::IVec3(1281, 1282, 1283);
		perPrimitiveData.prim_i64d2_flat_0[0] = tcu::IVec2(1291, 1292);
		perPrimitiveData.prim_i64d2_flat_0[1] = tcu::IVec2(1291, 1292);
		perPrimitiveData.prim_i64d2_flat_1[0] = tcu::IVec2(1301, 1302);
		perPrimitiveData.prim_i64d2_flat_1[1] = tcu::IVec2(1301, 1302);
		perPrimitiveData.prim_i64d1_flat_0[0] = 1311;
		perPrimitiveData.prim_i64d1_flat_0[1] = 1311;
		perPrimitiveData.prim_i64d1_flat_1[0] = 1321;
		perPrimitiveData.prim_i64d1_flat_1[1] = 1321;
		perPrimitiveData.prim_i32d4_flat_0[0] = tcu::IVec4(1331, 1332, 1333, 1334);
		perPrimitiveData.prim_i32d4_flat_0[1] = tcu::IVec4(1331, 1332, 1333, 1334);
		perPrimitiveData.prim_i32d4_flat_1[0] = tcu::IVec4(1341, 1342, 1343, 1344);
		perPrimitiveData.prim_i32d4_flat_1[1] = tcu::IVec4(1341, 1342, 1343, 1344);
		perPrimitiveData.prim_i32d3_flat_0[0] = tcu::IVec3(1351, 1352, 1353);
		perPrimitiveData.prim_i32d3_flat_0[1] = tcu::IVec3(1351, 1352, 1353);
		perPrimitiveData.prim_i32d3_flat_1[0] = tcu::IVec3(1361, 1362, 1363);
		perPrimitiveData.prim_i32d3_flat_1[1] = tcu::IVec3(1361, 1362, 1363);
		perPrimitiveData.prim_i32d2_flat_0[0] = tcu::IVec2(1371, 1372);
		perPrimitiveData.prim_i32d2_flat_0[1] = tcu::IVec2(1371, 1372);
		perPrimitiveData.prim_i32d2_flat_1[0] = tcu::IVec2(1381, 1382);
		perPrimitiveData.prim_i32d2_flat_1[1] = tcu::IVec2(1381, 1382);
		perPrimitiveData.prim_i32d1_flat_0[0] = 1391;
		perPrimitiveData.prim_i32d1_flat_0[1] = 1391;
		perPrimitiveData.prim_i32d1_flat_1[0] = 1401;
		perPrimitiveData.prim_i32d1_flat_1[1] = 1401;
		perPrimitiveData.prim_i16d4_flat_0[0] = tcu::IVec4(1411, 1412, 1413, 1414);
		perPrimitiveData.prim_i16d4_flat_0[1] = tcu::IVec4(1411, 1412, 1413, 1414);
		perPrimitiveData.prim_i16d4_flat_1[0] = tcu::IVec4(1421, 1422, 1423, 1424);
		perPrimitiveData.prim_i16d4_flat_1[1] = tcu::IVec4(1421, 1422, 1423, 1424);
		perPrimitiveData.prim_i16d3_flat_0[0] = tcu::IVec3(1431, 1432, 1433);
		perPrimitiveData.prim_i16d3_flat_0[1] = tcu::IVec3(1431, 1432, 1433);
		perPrimitiveData.prim_i16d3_flat_1[0] = tcu::IVec3(1441, 1442, 1443);
		perPrimitiveData.prim_i16d3_flat_1[1] = tcu::IVec3(1441, 1442, 1443);
		perPrimitiveData.prim_i16d2_flat_0[0] = tcu::IVec2(1451, 1452);
		perPrimitiveData.prim_i16d2_flat_0[1] = tcu::IVec2(1451, 1452);
		perPrimitiveData.prim_i16d2_flat_1[0] = tcu::IVec2(1461, 1462);
		perPrimitiveData.prim_i16d2_flat_1[1] = tcu::IVec2(1461, 1462);
		perPrimitiveData.prim_i16d1_flat_0[0] = 1471;
		perPrimitiveData.prim_i16d1_flat_0[1] = 1471;
		perPrimitiveData.prim_i16d1_flat_1[0] = 1481;
		perPrimitiveData.prim_i16d1_flat_1[1] = 1481;
	}

	// Create and fill buffers with this data.
	const auto			pvdSize		= static_cast<VkDeviceSize>(sizeof(perVertexData));
	const auto			pvdInfo		= makeBufferCreateInfo(pvdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	pvdData		(vkd, device, alloc, pvdInfo, MemoryRequirement::HostVisible);
	auto&				pvdAlloc	= pvdData.getAllocation();
	void*				pvdPtr		= pvdAlloc.getHostPtr();

	const auto			ppdSize		= static_cast<VkDeviceSize>(sizeof(perPrimitiveData));
	const auto			ppdInfo		= makeBufferCreateInfo(ppdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	ppdData		(vkd, device, alloc, ppdInfo, MemoryRequirement::HostVisible);
	auto&				ppdAlloc	= ppdData.getAllocation();
	void*				ppdPtr		= ppdAlloc.getHostPtr();

	deMemcpy(pvdPtr, &perVertexData, sizeof(perVertexData));
	deMemcpy(ppdPtr, &perPrimitiveData, sizeof(perPrimitiveData));

	flushAlloc(vkd, device, pvdAlloc);
	flushAlloc(vkd, device, ppdAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	// Create and update descriptor set.
	DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);
	const auto descriptorPool	= descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto pvdBufferInfo = makeDescriptorBufferInfo(pvdData.get(), 0ull, pvdSize);
	const auto ppdBufferInfo = makeDescriptorBufferInfo(ppdData.get(), 0ull, ppdSize);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pvdBufferInfo);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ppdBufferInfo);
	updateBuilder.update(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Shader modules.
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const auto						topHalf		= makeViewport(imageExtent.width, imageExtent.height / 2u);
	const std::vector<VkViewport>	viewports	{ makeViewport(imageExtent), topHalf };
	const std::vector<VkRect2D>		scissors	(2u, makeRect2D(imageExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto colorAccess		= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	const auto transferRead		= VK_ACCESS_TRANSFER_READ_BIT;
	const auto transferWrite	= VK_ACCESS_TRANSFER_WRITE_BIT;
	const auto hostRead			= VK_ACCESS_HOST_READ_BIT;

	const auto preCopyBarrier	= makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(transferWrite, hostRead);
	const auto copyRegion		= makeBufferImageCopy(imageExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iExtent				(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuFormat, iExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createMeshShaderInOutTestsEXT (tcu::TestContext& testCtx)
{
	GroupPtr inOutTests (new tcu::TestCaseGroup(testCtx, "in_out", "Mesh Shader Tests checking Input/Output interfaces"));

	const struct
	{
		bool i64; bool f64; bool i16; bool f16;
		const char* name;
	} requiredFeatures[] =
	{
		// Restrict the number of combinations to avoid creating too many tests.
		//	i64		f64		i16		f16		name
		{	false,	false,	false,	false,	"32_bits_only"		},
		{	true,	false,	false,	false,	"with_i64"			},
		{	false,	true,	false,	false,	"with_f64"			},
		{	true,	true,	false,	false,	"all_but_16_bits"	},
		{	false,	false,	true,	false,	"with_i16"			},
		{	false,	false,	false,	true,	"with_f16"			},
		{	true,	true,	true,	true,	"all_types"			},
	};

	Owner			ownerCases[]			= { Owner::VERTEX, Owner::PRIMITIVE };
	DataType		dataTypeCases[]			= { DataType::FLOAT, DataType::INTEGER };
	BitWidth		bitWidthCases[]			= { BitWidth::B64, BitWidth::B32, BitWidth::B16 };
	DataDim			dataDimCases[]			= { DataDim::SCALAR, DataDim::VEC2, DataDim::VEC3, DataDim::VEC4 };
	Interpolation	interpolationCases[]	= { Interpolation::NORMAL, Interpolation::FLAT };
	de::Random		rnd(1636723398u);

	for (const auto& reqs : requiredFeatures)
	{
		GroupPtr reqsGroup (new tcu::TestCaseGroup(testCtx, reqs.name, ""));

		// Generate the variable list according to the group requirements.
		IfaceVarVecPtr varsPtr(new IfaceVarVec);

		for (const auto& ownerCase : ownerCases)
		for (const auto& dataTypeCase : dataTypeCases)
		for (const auto& bitWidthCase : bitWidthCases)
		for (const auto& dataDimCase : dataDimCases)
		for (const auto& interpolationCase : interpolationCases)
		{
			if (dataTypeCase == DataType::FLOAT)
			{
				if (bitWidthCase == BitWidth::B64 && !reqs.f64)
					continue;
				if (bitWidthCase == BitWidth::B16 && !reqs.f16)
					continue;
			}
			else if (dataTypeCase == DataType::INTEGER)
			{
				if (bitWidthCase == BitWidth::B64 && !reqs.i64)
					continue;
				if (bitWidthCase == BitWidth::B16 && !reqs.i16)
					continue;
			}

			if (dataTypeCase == DataType::INTEGER && interpolationCase == Interpolation::NORMAL)
				continue;

			if (ownerCase == Owner::PRIMITIVE && interpolationCase == Interpolation::NORMAL)
				continue;

			if (dataTypeCase == DataType::FLOAT && bitWidthCase == BitWidth::B64 && interpolationCase == Interpolation::NORMAL)
				continue;

			for (uint32_t idx = 0u; idx < IfaceVar::kVarsPerType; ++idx)
				varsPtr->push_back(IfaceVar(ownerCase, dataTypeCase, bitWidthCase, dataDimCase, interpolationCase, idx));
		}

		// Generating all permutations of the variables above would mean millions of tests, so we just generate some pseudorandom permutations.
		constexpr uint32_t kPermutations = 40u;
		for (uint32_t combIdx = 0; combIdx < kPermutations; ++combIdx)
		{
			const auto caseName = "permutation_" + std::to_string(combIdx);
			GroupPtr rndGroup(new tcu::TestCaseGroup(testCtx, caseName.c_str(), ""));

			// Duplicate and shuffle vector.
			IfaceVarVecPtr permutVec (new IfaceVarVec(*varsPtr));
			rnd.shuffle(begin(*permutVec), end(*permutVec));

			// Cut the vector short to the usable number of locations.
			{
				uint32_t	usedLocations	= 0u;
				size_t		vectorEnd		= 0u;
				auto&		varVec			= *permutVec;

				for (size_t i = 0; i < varVec.size(); ++i)
				{
					vectorEnd = i;
					const auto varSize = varVec[i].getLocationSize();
					if (usedLocations + varSize > InterfaceVariablesCase::kMaxLocations)
						break;
					usedLocations += varSize;
				}

				varVec.resize(vectorEnd);
			}

			for (int i = 0; i < 2; ++i)
			{
				const bool useTaskShader	= (i > 0);
				const auto name				= (useTaskShader ? "task_mesh" : "mesh_only");

				// Duplicate vector for this particular case so both variants have the same shuffle.
				IfaceVarVecPtr paramsVec(new IfaceVarVec(*permutVec));

				ParamsPtr paramsPtr (new InterfaceVariableParams(
					/*taskCount*/	(useTaskShader ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
					/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
					/*width*/		8u,
					/*height*/		8u,
					/*useInt64*/	reqs.i64,
					/*useFloat64*/	reqs.f64,
					/*useInt16*/	reqs.i16,
					/*useFloat16*/	reqs.f16,
					/*vars*/		std::move(paramsVec)));

				rndGroup->addChild(new InterfaceVariablesCase(testCtx, name, "", std::move(paramsPtr)));
			}

			reqsGroup->addChild(rndGroup.release());
		}

		inOutTests->addChild(reqsGroup.release());
	}

	return inOutTests.release();
}

} // MeshShader
} // vkt
