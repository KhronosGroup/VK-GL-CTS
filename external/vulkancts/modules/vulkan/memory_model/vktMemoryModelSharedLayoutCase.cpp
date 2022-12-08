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
 * \brief Shared memory layout test case.
 *//*--------------------------------------------------------------------*/

#include <vkDefs.hpp>
#include "deRandom.hpp"
#include "gluContextInfo.hpp"
#include "gluVarTypeUtil.hpp"
#include "tcuTestLog.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "vktMemoryModelSharedLayoutCase.hpp"
#include "util/vktTypeComparisonUtil.hpp"

namespace vkt
{
namespace MemoryModel
{

using tcu::TestLog;
using std::string;
using std::vector;
using glu::VarType;
using glu::StructMember;

namespace
{
	void computeReferenceLayout (const VarType& type, vector<SharedStructVarEntry>& entries)
	{
		if (type.isBasicType())
			entries.push_back(SharedStructVarEntry(type.getBasicType(), 1));
		else if (type.isArrayType())
		{
			const VarType &elemType = type.getElementType();

			// Array of scalars, vectors or matrices.
			if (elemType.isBasicType())
				entries.push_back(SharedStructVarEntry(elemType.getBasicType(), type.getArraySize()));
			else
			{
				DE_ASSERT(elemType.isStructType() || elemType.isArrayType());
				for (int i = 0; i < type.getArraySize(); i++)
					computeReferenceLayout(type.getElementType(), entries);
			}
		}
		else
		{
			DE_ASSERT(type.isStructType());
			for (const auto& member : *type.getStructPtr())
				computeReferenceLayout(member.getType(), entries);
		}
	}

	void computeReferenceLayout (SharedStructVar& var)
	{
		// Top-level arrays need special care.
		if (var.type.isArrayType())
			computeReferenceLayout(var.type.getElementType(), var.entries);
		else
			computeReferenceLayout(var.type, var.entries);
	}

	void generateValue (const SharedStructVarEntry& entry, de::Random& rnd, vector<string>& values)
	{
		const glu::DataType scalarType	= glu::getDataTypeScalarType(entry.type);
		const int scalarSize			= glu::getDataTypeScalarSize(entry.type);
		const int arraySize				= entry.arraySize;
		const bool isMatrix				= glu::isDataTypeMatrix(entry.type);
		const int numVecs				= isMatrix ? glu::getDataTypeMatrixNumColumns(entry.type) : 1;
		const int vecSize				= scalarSize / numVecs;

		DE_ASSERT(scalarSize % numVecs == 0);
		DE_ASSERT(arraySize >= 0);

		string generatedValue;
		for (int elemNdx = 0; elemNdx < arraySize; elemNdx++)
		{
			for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
			{
				for (int compNdx = 0; compNdx < vecSize; compNdx++)
				{
					switch (scalarType)
					{
						case glu::TYPE_INT:
						case glu::TYPE_INT8:
						case glu::TYPE_INT16:
							// Fall through. This fits into all the types above.
							generatedValue = de::toString(rnd.getInt(-9, 9));
							break;
						case glu::TYPE_UINT:
						case glu::TYPE_UINT8:
						case glu::TYPE_UINT16:
							// Fall through. This fits into all the types above.
							generatedValue = de::toString(rnd.getInt(0, 9)).append("u");
							break;
						case glu::TYPE_FLOAT:
						case glu::TYPE_FLOAT16:
							// Fall through. This fits into all the types above.
							generatedValue = de::floatToString(static_cast<float>(rnd.getInt(-9, 9)), 1);
							break;
						case glu::TYPE_BOOL:
							generatedValue = rnd.getBool() ? "true" : "false";
							break;
						default:
							DE_ASSERT(false);
					}

					values.push_back(generatedValue);
				}
			}
		}
	}

	string getStructMemberName (const SharedStructVar& var, const glu::TypeComponentVector& accessPath)
	{
		std::ostringstream name;

		name << "." << var.name;

		for (auto pathComp = accessPath.begin(); pathComp != accessPath.end(); pathComp++)
		{
			if (pathComp->type == glu::VarTypeComponent::STRUCT_MEMBER)
			{
				const VarType			curType		= glu::getVarType(var.type, accessPath.begin(), pathComp);
				const glu::StructType	*structPtr	= curType.getStructPtr();

				name << "." << structPtr->getMember(pathComp->index).getName();
			}
			else if (pathComp->type == glu::VarTypeComponent::ARRAY_ELEMENT)
				name << "[" << pathComp->index << "]";
			else
				DE_ASSERT(false);
		}

		return name.str();
	}
} // anonymous

NamedStructSP ShaderInterface::allocStruct (const string& name)
{
	m_structs.emplace_back(new glu::StructType(name.c_str()));
	return m_structs.back();
}

SharedStruct& ShaderInterface::allocSharedObject (const string& name, const string& instanceName)
{
	m_sharedMemoryObjects.emplace_back(name, instanceName);
	return m_sharedMemoryObjects.back();
}

void generateCompareFuncs (std::ostream &str, const ShaderInterface &interface)
{
	std::set<glu::DataType> types;
	std::set<glu::DataType> compareFuncs;

	// Collect unique basic types.
	for (const auto& sharedObj : interface.getSharedObjects())
		for (const auto& var : sharedObj)
			vkt::typecomputil::collectUniqueBasicTypes(types, var.type);

	// Set of compare functions required.
	for (const auto& type : types)
		vkt::typecomputil::getCompareDependencies(compareFuncs, type);

	for (int type = 0; type < glu::TYPE_LAST; ++type)
		if (compareFuncs.find(glu::DataType(type)) != compareFuncs.end())
			str << vkt::typecomputil::getCompareFuncForType(glu::DataType(type));
}

void generateSharedMemoryWrites (std::ostream &src, const SharedStruct &object,
								const SharedStructVar &var, const glu::SubTypeAccess &accessPath,
								vector<string>::const_iterator &valueIter, bool compare)
{
	const VarType curType = accessPath.getType();

	if (curType.isArrayType())
	{
		const int arraySize = curType.getArraySize();
		for (int i = 0; i < arraySize; i++)
			generateSharedMemoryWrites(src, object, var, accessPath.element(i), valueIter, compare);
	}
	else if (curType.isStructType())
	{
		const int numMembers = curType.getStructPtr()->getNumMembers();
		for (int i = 0; i < numMembers; i++)
			generateSharedMemoryWrites(src, object, var, accessPath.member(i), valueIter, compare);
	}
	else
	{
		DE_ASSERT(curType.isBasicType());

		const glu::DataType basicType				= curType.getBasicType();
		const string		typeName				= glu::getDataTypeName(basicType);
		const string		sharedObjectVarName		= object.getInstanceName();
		const string		structMember			= getStructMemberName(var, accessPath.getPath());
		const glu::DataType promoteType				= vkt::typecomputil::getPromoteType(basicType);

		int numElements = glu::getDataTypeScalarSize(basicType);
		if (glu::isDataTypeMatrix(basicType))
			numElements = glu::getDataTypeMatrixNumColumns(basicType) * glu::getDataTypeMatrixNumRows(basicType);

		if (compare)
		{
			src << "\t" << "allOk" << " = " << "allOk" << " && compare_" << typeName << "(";
			// Comparison functions use 32-bit values. Convert 8/16-bit scalar and vector types if necessary.
			// E.g. uint8_t becomes int.
			if (basicType != promoteType || numElements > 1)
				src << glu::getDataTypeName(promoteType) << "(";
		}
		else
		{
			src << "\t" << sharedObjectVarName << structMember << " = " << "";
			// If multiple literals or a 8/16-bit literal is assigned, the variable must be
			// initialized with the constructor.
			if (basicType != promoteType || numElements > 1)
				src << glu::getDataTypeName(basicType) << "(";
		}

		for (int i = 0; i < numElements; i++)
			src << (i != 0 ? ", " : "") << *valueIter++;

		if (basicType != promoteType)
			src << ")";
		else if (numElements > 1)
			src << ")";

		// Write the variable in the shared memory as the next argument for the comparison function.
		// Initialize it as a new 32-bit variable in the case it's a 8-bit or a 16-bit variable.
		if (compare)
		{
			if (basicType != promoteType)
				src << ", " << glu::getDataTypeName(promoteType) << "(" << sharedObjectVarName
					<< structMember
					<< "))";
			else
				src << ", " << sharedObjectVarName << structMember << ")";
		}

		src << ";\n";
	}
}

string generateComputeShader (ShaderInterface &interface)
{
	std::ostringstream src;

	src << "#version 450\n";

	if (interface.is16BitTypesEnabled())
		src << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n";
	if (interface.is8BitTypesEnabled())
		src << "#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable\n";

	src << "layout(local_size_x = 1) in;\n";
	src << "\n";

	src << "layout(std140, binding = 0) buffer block { highp uint passed; };\n";

	// Output definitions for the struct fields of the shared memory objects.
	std::vector<NamedStructSP>& namedStructs = interface.getStructs();

	for (const auto& s: namedStructs)
		src << glu::declare(s.get()) << ";\n";

	// Output definitions for the shared memory structs.
	for (auto& sharedObj : interface.getSharedObjects())
	{
		src << "struct " << sharedObj.getName() << " {\n";

		for (auto& var : sharedObj)
			src << "\t" << glu::declare(var.type, var.name, 1) << ";\n";

		src << "};\n";
	}

	// Comparison utilities.
	src << "\n";
	generateCompareFuncs(src, interface);

	src << "\n";
	for (auto& sharedObj : interface.getSharedObjects())
		src << "shared " << sharedObj.getName() << " " << sharedObj.getInstanceName() << ";\n";

	src << "\n";
	src << "void main (void) {\n";

	for (auto& sharedObj : interface.getSharedObjects())
	{
		for (const auto& var : sharedObj)
		{
			vector<string>::const_iterator valueIter = var.entryValues.begin();
			generateSharedMemoryWrites(src, sharedObj, var, glu::SubTypeAccess(var.type), valueIter, false);
		}
	}

	src << "\n";
	src << "\tbarrier();\n";
	src << "\tmemoryBarrier();\n";
	src << "\tbool allOk = true;\n";

	for (auto& sharedObj : interface.getSharedObjects())
	{
		for (const auto& var : sharedObj)
		{
			vector<string>::const_iterator valueIter = var.entryValues.begin();
			generateSharedMemoryWrites(src, sharedObj, var, glu::SubTypeAccess(var.type), valueIter, true);
		}
	}

	src << "\tif (allOk)\n"
		<< "\t\tpassed++;\n"
		<< "\n";

	src << "}\n";

	return src.str();
}

void SharedLayoutCase::checkSupport(Context& context) const
{
	if ((m_interface.is16BitTypesEnabled() || m_interface.is8BitTypesEnabled())
		&& !context.isDeviceFunctionalitySupported("VK_KHR_shader_float16_int8"))
		TCU_THROW(NotSupportedError, "VK_KHR_shader_float16_int8 extension for 16-/8-bit types not supported");

	const vk::VkPhysicalDeviceVulkan12Features features = context.getDeviceVulkan12Features();
	if (m_interface.is16BitTypesEnabled() && !features.shaderFloat16)
		TCU_THROW(NotSupportedError, "16-bit types not supported");
	if (m_interface.is8BitTypesEnabled() && !features.shaderInt8)
		TCU_THROW(NotSupportedError, "8-bit types not supported");
}

tcu::TestStatus SharedLayoutCaseInstance::iterate (void)
{
	const vk::DeviceInterface					&vk							= m_context.getDeviceInterface();
	const vk::VkDevice							device						= m_context.getDevice();
	const vk::VkQueue							queue						= m_context.getUniversalQueue();
	const deUint32								queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	vk::Allocator&								allocator					= m_context.getDefaultAllocator();
	const deUint32								bufferSize					= 4;

	// Create descriptor set
	const vk::VkBufferCreateInfo				params						=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,									// pNext
		0u,											// flags
		bufferSize,									// size
		vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,											// queueFamilyCount
		&queueFamilyIndex							// pQueueFamilyIndices
	};

	vk::Move<vk::VkBuffer>						buffer						(vk::createBuffer(vk, device, &params));
	vk::VkMemoryRequirements					requirements				= getBufferMemoryRequirements(vk, device, *buffer);
	de::MovePtr<vk::Allocation>					bufferAlloc					(allocator.allocate(requirements, vk::MemoryRequirement::HostVisible));
	VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

	deMemset(bufferAlloc->getHostPtr(), 0, bufferSize);
	flushMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), requirements.size);

	vk::DescriptorSetLayoutBuilder				setLayoutBuilder;
	vk::DescriptorPoolBuilder					poolBuilder;

	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, deUint32(1));

	const vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout			(setLayoutBuilder.build(vk, device));
	const vk::Unique<vk::VkDescriptorPool>		descriptorPool				(poolBuilder.build(vk, device,
																			vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::VkDescriptorSetAllocateInfo		allocInfo					=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		*descriptorPool,										// VkDescriptorPool					descriptorPool;
		1u,														// deUint32							descriptorSetCount;
		&descriptorSetLayout.get(),								// const VkDescriptorSetLayout		*pSetLayouts;
	};

	const vk::Unique<vk::VkDescriptorSet>		descriptorSet				(allocateDescriptorSet(vk, device, &allocInfo));
	const vk::VkDescriptorBufferInfo			descriptorInfo				= makeDescriptorBufferInfo(*buffer, 0ull, bufferSize);

	vk::DescriptorSetUpdateBuilder				setUpdateBuilder;
	std::vector<vk::VkDescriptorBufferInfo>		descriptors;

	setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
								vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);

	setUpdateBuilder.update(vk, device);

	const vk::VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(vk::VkPipelineLayoutCreateFlags) 0,				// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							descriptorSetCount;
		&*descriptorSetLayout,								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
	};
	vk::Move<vk::VkPipelineLayout>				pipelineLayout				(createPipelineLayout(vk, device, &pipelineLayoutParams));

	vk::Move<vk::VkShaderModule>				shaderModule				(createShaderModule(vk, device, m_context.getBinaryCollection().get("compute"), 0));
	const vk::VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		(vk::VkPipelineShaderStageCreateFlags) 0,					// VkPipelineShaderStageCreateFlags	flags;
		vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStage					stage;
		*shaderModule,												// VkShaderModule					module;
		"main",														// const char*						pName;
		DE_NULL,													// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const vk::VkComputePipelineCreateInfo		pipelineCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		0,														// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,								// VkPipelineShaderStageCreateInfo	stage;
		*pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,												// VkPipeline						basePipelineHandle;
		0,														// deInt32							basePipelineIndex;
	};

	vk::Move<vk::VkPipeline>					pipeline					(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));
	vk::Move<vk::VkCommandPool>					cmdPool						(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	vk::Move<vk::VkCommandBuffer>				cmdBuffer					(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout,
							0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Read back passed data
	bool										counterOk;
	const int									refCount					= 1;
	int											resCount					= 0;

	invalidateAlloc(vk, device, *bufferAlloc);

	resCount = *(static_cast<const int *>(bufferAlloc->getHostPtr()));

	counterOk = (refCount == resCount);
	if (!counterOk)
		m_context.getTestContext().getLog() << TestLog::Message << "Error: passed = " << resCount
											<< ", expected " << refCount << TestLog::EndMessage;

	// Validate result
	if (counterOk)
		return tcu::TestStatus::pass("Counter value OK");

	return tcu::TestStatus::fail("Counter value incorrect");
}

void SharedLayoutCase::initPrograms (vk::SourceCollections &programCollection) const
{
	DE_ASSERT(!m_computeShaderSrc.empty());
	programCollection.glslSources.add("compute") << glu::ComputeSource(m_computeShaderSrc);
}

TestInstance* SharedLayoutCase::createInstance (Context &context) const
{
	return new SharedLayoutCaseInstance(context);
}

void SharedLayoutCase::delayedInit (void)
{

	for (auto& sharedObj : m_interface.getSharedObjects())
		for (auto &var : sharedObj)
			computeReferenceLayout(var);

	deUint32	seed	= deStringHash(getName()) ^ 0xad2f7214;
	de::Random	rnd		(seed);

	for (auto& sharedObj : m_interface.getSharedObjects())
		for (auto &var : sharedObj)
			for (int i = 0; i < var.topLevelArraySize; i++)
				for (auto &entry : var.entries)
					generateValue(entry, rnd, var.entryValues);

	m_computeShaderSrc = generateComputeShader(m_interface);
}

} // MemoryModel
} // vkt
