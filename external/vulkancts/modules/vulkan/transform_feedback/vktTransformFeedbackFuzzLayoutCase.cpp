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

#include "vkPrograms.hpp"

#include "gluVarType.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "deSharedPtr.hpp"
#include "deFloat16.h"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <iomanip>

namespace vkt
{
namespace TransformFeedback
{

using namespace vk;

typedef std::map<int, int> BufferGeneralMapping;

typedef std::pair<int, int>				UsedRange;
typedef std::vector<UsedRange>			UsedRangeList;
typedef std::map<int, UsedRangeList>	BufferUsedRangesMap;

// VarType implementation.

VarType::VarType (void)
	: m_type	(TYPE_LAST)
	, m_flags	(0)
{
}

VarType::VarType (const VarType& other)
	: m_type	(TYPE_LAST)
	, m_flags	(0)
{
	*this = other;
}

VarType::VarType (glu::DataType basicType, deUint32 flags)
	: m_type	(TYPE_BASIC)
	, m_flags	(flags)
{
	m_data.basicType = basicType;
}

VarType::VarType (const VarType& elementType, int arraySize)
	: m_type	(TYPE_ARRAY)
	, m_flags	(0)
{
	m_data.array.size			= arraySize;
	m_data.array.elementType	= new VarType(elementType);
}

VarType::VarType (const StructType* structPtr, deUint32 flags)
	: m_type	(TYPE_STRUCT)
	, m_flags	(flags)
{
	m_data.structPtr = structPtr;
}

VarType::~VarType (void)
{
	if (m_type == TYPE_ARRAY)
		delete m_data.array.elementType;
}

VarType& VarType::operator= (const VarType& other)
{
	if (this == &other)
		return *this; // Self-assignment.

	VarType *oldElementType = m_type == TYPE_ARRAY ? m_data.array.elementType : DE_NULL;

	m_type	= other.m_type;
	m_flags	= other.m_flags;
	m_data	= Data();

	if (m_type == TYPE_ARRAY)
	{
		m_data.array.elementType	= new VarType(*other.m_data.array.elementType);
		m_data.array.size			= other.m_data.array.size;
	}
	else
		m_data = other.m_data;

	delete oldElementType;

	return *this;
}

// StructType implementation.
void StructType::addMember (const std::string& name, const VarType& type, deUint32 flags)
{
	m_members.push_back(StructMember(name, type, flags));
}

// InterfaceBlockMember implementation.
InterfaceBlockMember::InterfaceBlockMember (const std::string& name, const VarType& type, deUint32 flags)
	: m_name	(name)
	, m_type	(type)
	, m_flags	(flags)
{
}

// InterfaceBlock implementation.
InterfaceBlock::InterfaceBlock (const std::string& blockName)
	: m_blockName	(blockName)
	, m_xfbBuffer	(0)
	, m_arraySize	(0)
	, m_flags		(0)
{
}

std::ostream& operator<< (std::ostream& stream, const BlockLayoutEntry& entry)
{
	stream << entry.name << " { name = " << entry.name
		   << ", buffer = " << entry.xfbBuffer
		   << ", offset = " << entry.xfbOffset
		   << ", size = " << entry.xfbSize
		   << ", blockDeclarationNdx = " << entry.blockDeclarationNdx
		   << ", instanceNdx = " << entry.instanceNdx
		   << ", activeInterfaceIndices = [";

	for (std::vector<int>::const_iterator i = entry.activeInterfaceIndices.begin(); i != entry.activeInterfaceIndices.end(); i++)
	{
		if (i != entry.activeInterfaceIndices.begin())
			stream << ", ";
		stream << *i;
	}

	stream << "] }";
	return stream;
}

std::ostream& operator<< (std::ostream& stream, const InterfaceLayoutEntry& entry)
{
	stream << entry.name << " { type = " << glu::getDataTypeName(entry.type)
		   << ", arraySize = " << entry.arraySize
		   << ", blockNdx = " << entry.blockLayoutNdx
		   << ", offset = " << entry.offset
		   << ", arrayStride = " << entry.arrayStride
		   << ", matrixStride = " << entry.matrixStride
		   << " }";

	return stream;
}

std::ostream& operator<< (std::ostream& str, const InterfaceLayout& layout)
{
	const int	numBlocks	= (int)layout.blocks.size();

	str << "Blocks:" << std::endl;
	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
		str << layout.blocks[blockNdx] << std::endl;
	str << std::endl;

	str << "Interfaces:" << std::endl;
	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
	{
		int		numEntries	= (int)layout.blocks[blockNdx].activeInterfaceIndices.size();

		for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
		{
			const InterfaceLayoutEntry&	entry	= layout.interfaces[layout.blocks[blockNdx].activeInterfaceIndices[entryNdx]];

			str << blockNdx << ":" << entryNdx << " " << entry << std::endl;
		}
	}
	str << std::endl;

	return str;
}

int InterfaceLayout::getInterfaceLayoutIndex (int blockNdx, const std::string& name) const
{
	for (int ndx = 0; ndx < (int)interfaces.size(); ndx++)
	{
		if (blocks[interfaces[ndx].blockLayoutNdx].blockDeclarationNdx == blockNdx && interfaces[ndx].name == name)
			return ndx;
	}

	return -1;
}

int InterfaceLayout::getBlockLayoutIndex (int blockNdx, int instanceNdx) const
{
	for (int ndx = 0; ndx < (int)blocks.size(); ndx++)
	{
		if (blocks[ndx].blockDeclarationNdx == blockNdx && blocks[ndx].instanceNdx == instanceNdx)
			return ndx;
	}

	return -1;
}

// ShaderInterface implementation.

ShaderInterface::ShaderInterface (void)
{
}

ShaderInterface::~ShaderInterface (void)
{
}

StructType& ShaderInterface::allocStruct (const std::string& name)
{
	m_structs.push_back(StructTypeSP(new StructType(name)));
	return *m_structs.back();
}

struct StructNameEquals
{
	std::string name;

	StructNameEquals (const std::string& name_) : name(name_) {}

	bool operator() (const StructTypeSP type) const
	{
		return type->hasTypeName() && name == type->getTypeName();
	}
};

void ShaderInterface::getNamedStructs (std::vector<const StructType*>& structs) const
{
	for (std::vector<StructTypeSP>::const_iterator i = m_structs.begin(); i != m_structs.end(); i++)
	{
		if ((*i)->hasTypeName())
			structs.push_back((*i).get());
	}
}

InterfaceBlock& ShaderInterface::allocBlock (const std::string& name)
{
	m_interfaceBlocks.push_back(InterfaceBlockSP(new InterfaceBlock(name)));

	return *m_interfaceBlocks.back();
}

namespace // Utilities
{

struct PrecisionFlagsFmt
{
	deUint32 flags;
	PrecisionFlagsFmt (deUint32 flags_) : flags(flags_) {}
};

void dumpBytes (std::ostream& str, const std::string& msg, const void* dataBytes, size_t size, const void* dataMask = DE_NULL)
{
	const deUint8*		data	= (const deUint8*)dataBytes;
	const deUint8*		mask	= (const deUint8*)dataMask;
	std::ios::fmtflags	flags;

	str << msg;

	flags = str.flags ( std::ios::hex | std::ios::uppercase );
	{
		for (size_t i = 0; i < size; i++)
		{
			if (i%16 == 0) str << std::endl << std::setfill('0') << std::setw(8) << i << ":";
			else if (i%8 == 0) str << "  ";
			else if (i%4 == 0) str << " ";

			str << " " << std::setfill('0') << std::setw(2);

			if (mask == DE_NULL || mask[i] != 0)
				str << (deUint32)data[i];
			else
				str << "__";
		}
		str << std::endl << std::endl;
	}
	str.flags ( flags );
}

std::ostream& operator<< (std::ostream& str, const PrecisionFlagsFmt& fmt)
{
	// Precision.
	DE_ASSERT(dePop32(fmt.flags & (PRECISION_LOW|PRECISION_MEDIUM|PRECISION_HIGH)) <= 1);
	str << (fmt.flags & PRECISION_LOW		? "lowp"	:
			fmt.flags & PRECISION_MEDIUM	? "mediump"	:
			fmt.flags & PRECISION_HIGH		? "highp"	: "");
	return str;
}

struct LayoutFlagsFmt
{
	deUint32 flags;
	deUint32 buffer;
	deUint32 stride;
	deUint32 offset;

	LayoutFlagsFmt	(const deUint32	flags_,
					 const deUint32	buffer_,
					 const deUint32	stride_,
					 const deUint32	offset_)
		: flags		(flags_)
		, buffer	(buffer_)
		, stride	(stride_)
		, offset	(offset_)
	{
	}
};

std::ostream& operator<< (std::ostream& str, const LayoutFlagsFmt& fmt)
{
	static const struct
	{
		deUint32	bit;
		const char*	token;
	} bitDesc[] =
	{
		{ LAYOUT_XFBBUFFER,	"xfb_buffer"	},
		{ LAYOUT_XFBOFFSET,	"xfb_offset"	},
		{ LAYOUT_XFBSTRIDE,	"xfb_stride"	},
	};

	deUint32 remBits = fmt.flags;
	for (int descNdx = 0; descNdx < DE_LENGTH_OF_ARRAY(bitDesc); descNdx++)
	{
		if (remBits & bitDesc[descNdx].bit)
		{
			str << bitDesc[descNdx].token;

			if (bitDesc[descNdx].bit == LAYOUT_XFBBUFFER) str << " = " << fmt.buffer;
			if (bitDesc[descNdx].bit == LAYOUT_XFBOFFSET) str << " = " << fmt.offset;
			if (bitDesc[descNdx].bit == LAYOUT_XFBSTRIDE) str << " = " << fmt.stride;

			remBits &= ~bitDesc[descNdx].bit;

			if (remBits != 0)
				str << ", ";
		}
	}
	DE_ASSERT(remBits == 0);
	return str;
}

std::ostream& operator<< (std::ostream& str, const DeviceSizeVector& vec)
{
	str << " [";

	for (size_t vecNdx = 0; vecNdx < vec.size(); vecNdx++)
		str << (deUint64)vec[vecNdx] << (vecNdx + 1 < vec.size() ? ", " : "]");

	return str;
}

// Layout computation.

int getDataTypeByteSize (glu::DataType type)
{
	if (getDataTypeScalarType(type) == glu::TYPE_DOUBLE)
	{
		return glu::getDataTypeScalarSize(type)*(int)sizeof(deUint64);
	}
	else
	{
		return glu::getDataTypeScalarSize(type)*(int)sizeof(deUint32);
	}
}

int getDataTypeArrayStride (glu::DataType type)
{
	DE_ASSERT(!glu::isDataTypeMatrix(type));

	return getDataTypeByteSize(type);
}

int getDataTypeArrayStrideForLocation (glu::DataType type)
{
	DE_ASSERT(!glu::isDataTypeMatrix(type));

	const int baseStride	= getDataTypeByteSize(type);
	const int vec4Alignment	= (int)sizeof(deUint32) * 4;

	return deAlign32(baseStride, vec4Alignment);
}

int computeInterfaceBlockMemberAlignment (const VarType& type)
{
	if (type.isBasicType())
	{
		glu::DataType basicType = type.getBasicType();

		if (glu::isDataTypeMatrix(basicType) || isDataTypeVector(basicType))
			basicType = glu::getDataTypeScalarType(basicType);

		switch (basicType)
		{
			case glu::TYPE_FLOAT:
			case glu::TYPE_INT:
			case glu::TYPE_UINT:	return sizeof(deUint32);
			case glu::TYPE_DOUBLE:	return sizeof(deUint64);
			default:				TCU_THROW(InternalError, "Invalid type");
		}
	}
	else if (type.isArrayType())
	{
		return computeInterfaceBlockMemberAlignment(type.getElementType());
	}
	else if (type.isStructType())
	{
		int maxAlignment = 0;

		for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end(); memberIter++)
			maxAlignment = de::max(maxAlignment, computeInterfaceBlockMemberAlignment(memberIter->getType()));

		return maxAlignment;
	}
	else
		TCU_THROW(InternalError, "Invalid type");
}

void createMask (void* maskBasePtr, const InterfaceLayoutEntry& entry, const void* basePtr0, const void* basePtr)
{
	const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);
	const int			scalarSize	= glu::getDataTypeScalarSize(entry.type);
	const bool			isMatrix	= glu::isDataTypeMatrix(entry.type);
	const int			numVecs		= isMatrix ? glu::getDataTypeMatrixNumColumns(entry.type) : 1;
	const int			vecSize		= scalarSize / numVecs;
	const bool			isArray		= entry.arraySize > 1;
	const size_t		compSize	= getDataTypeByteSize(scalarType);

	DE_ASSERT(scalarSize%numVecs == 0);

	for (int elemNdx = 0; elemNdx < entry.arraySize; elemNdx++)
	{
		deUint8* elemPtr = (deUint8*)basePtr + entry.offset + (isArray ? elemNdx*entry.arrayStride : 0);

		for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
		{
			deUint8* vecPtr = elemPtr + (isMatrix ? vecNdx*entry.matrixStride : 0);

			for (int compNdx = 0; compNdx < vecSize; compNdx++)
			{
				const deUint8*	compPtr		= vecPtr + compSize*compNdx;
				const size_t	offset		= compPtr - (deUint8*)basePtr0;
				deUint8*		maskPtr		= (deUint8*)maskBasePtr + offset;

				switch (scalarType)
				{
					case glu::TYPE_DOUBLE:
					case glu::TYPE_FLOAT:
					case glu::TYPE_INT:
					case glu::TYPE_UINT:
					{
						for (size_t ndx = 0; ndx < compSize; ++ndx)
							++maskPtr[ndx];

						break;
					}
					default:
						DE_ASSERT(false);
				}
			}
		}
	}
}

std::vector<deUint8> createMask (const InterfaceLayout& layout, const std::map<int, void*>& blockPointers, const void* basePtr0, const size_t baseSize)
{
	std::vector<deUint8>	mask		(baseSize, 0);
	const int				numBlocks	((int)layout.blocks.size());

	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
	{
		void*	basePtr		= blockPointers.find(blockNdx)->second;
		int		numEntries	= (int)layout.blocks[blockNdx].activeInterfaceIndices.size();

		for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
		{
			const InterfaceLayoutEntry&	entry	= layout.interfaces[layout.blocks[blockNdx].activeInterfaceIndices[entryNdx]];

			if (entry.validate)
				createMask (&mask[0], entry, basePtr0, basePtr);
		}
	}

	return mask;
}

int computeInterfaceBlockAlignment(const InterfaceBlock& interfaceBlock)
{
	int baseAlignment = 0;

	for (InterfaceBlock::ConstIterator memberIter = interfaceBlock.begin(); memberIter != interfaceBlock.end(); memberIter++)
	{
		const InterfaceBlockMember& member = *memberIter;

		baseAlignment = std::max(baseAlignment, computeInterfaceBlockMemberAlignment(member.getType()));
	}

	return baseAlignment;
}

static inline bool isOverlaped(const int a1, const int b1, const int a2, const int b2)
{
	DE_ASSERT(b1 > 0 && b2 > 0);

	const int b1s = b1 - 1;
	const int b2s = b2 - 1;

	return	deInRange32(a1,  a2, b2s) ||
			deInRange32(b1s, a2, b2s) ||
			deInRange32(a2,  a1, b1s) ||
			deInRange32(b2s, a1, b1s);
}

void computeXfbLayout (InterfaceLayout& layout, int& curOffset, int& curLocation, int curBlockNdx, const std::string& curPrefix, const VarType& type, deUint32 layoutFlags)
{
	const int	locationAlignSize	= 16;
	const bool	validate			= 0 == (layoutFlags & (FIELD_MISSING|FIELD_UNASSIGNED));
	int			baseAlignment		= computeInterfaceBlockMemberAlignment(type);

	DE_ASSERT(baseAlignment == sizeof(deUint32) || baseAlignment == sizeof(deUint64));

	curOffset = deAlign32(curOffset, baseAlignment);

	if (type.isBasicType())
	{
		const glu::DataType		basicType				= type.getBasicType();
		int						fieldSize				= 0;
		int						fieldSizeForLocation	= 0;
		InterfaceLayoutEntry	entry;

		entry.name				= curPrefix;
		entry.type				= basicType;
		entry.arraySize			= 1;
		entry.arrayStride		= 0;
		entry.matrixStride		= 0;
		entry.blockLayoutNdx	= curBlockNdx;
		entry.locationNdx		= 0;
		entry.validate			= validate;

		if (glu::isDataTypeMatrix(basicType))
		{
			// Array of vectors
			const int				vecSize				= glu::getDataTypeMatrixNumRows(basicType);
			const int				numVecs				= glu::getDataTypeMatrixNumColumns(basicType);
			const glu::DataType		elemType			= glu::getDataTypeScalarType(basicType);
			const int				stride				= getDataTypeArrayStride(glu::getDataTypeVector(elemType, vecSize));
			const int				strideForLocation	= getDataTypeArrayStrideForLocation(glu::getDataTypeVector(elemType, vecSize));

			entry.matrixStride		= stride;

			fieldSize				= numVecs * stride;
			fieldSizeForLocation	= numVecs * strideForLocation;
		}
		else
		{
			// Scalar or vector.
			fieldSize				= getDataTypeByteSize(basicType);
			fieldSizeForLocation	= deAlign32(fieldSize, locationAlignSize);
		}

		entry.offset		= curOffset;
		entry.locationNdx	= curLocation;

		curOffset += fieldSize;
		curLocation += deDivRoundUp32(fieldSizeForLocation, locationAlignSize);

		layout.interfaces.push_back(entry);
	}
	else if (type.isArrayType())
	{
		const VarType&	elemType	= type.getElementType();

		if (elemType.isBasicType() && !glu::isDataTypeMatrix(elemType.getBasicType()))
		{
			// Array of scalars or vectors.
			const glu::DataType		elemBasicType			= elemType.getBasicType();
			const int				stride					= getDataTypeArrayStride(elemBasicType);
			const int				fieldSize				= stride * type.getArraySize();
			const int				strideForLocation		= getDataTypeArrayStrideForLocation(elemBasicType);
			const int				fieldSizeForLocation	= strideForLocation * type.getArraySize();
			InterfaceLayoutEntry	entry;

			entry.name				= curPrefix + "[0]"; // Array interfaces are always postfixed with [0]
			entry.type				= elemBasicType;
			entry.blockLayoutNdx	= curBlockNdx;
			entry.offset			= curOffset;
			entry.arraySize			= type.getArraySize();
			entry.arrayStride		= stride;
			entry.matrixStride		= 0;
			entry.locationNdx		= curLocation;
			entry.validate			= validate;

			curOffset += fieldSize;
			curLocation += deDivRoundUp32(fieldSizeForLocation, locationAlignSize);

			layout.interfaces.push_back(entry);
		}
		else if (elemType.isBasicType() && glu::isDataTypeMatrix(elemType.getBasicType()))
		{
			// Array of matrices.
			const glu::DataType		elemBasicType			= elemType.getBasicType();
			const glu::DataType		scalarType				= glu::getDataTypeScalarType(elemBasicType);
			const int				vecSize					= glu::getDataTypeMatrixNumRows(elemBasicType);
			const int				numVecs					= glu::getDataTypeMatrixNumColumns(elemBasicType);
			const int				stride					= getDataTypeArrayStride(glu::getDataTypeVector(scalarType, vecSize));
			const int				fieldSize				= numVecs * type.getArraySize() * stride;
			const int				strideForLocation		= getDataTypeArrayStrideForLocation(glu::getDataTypeVector(scalarType, vecSize));
			const int				fieldSizeForLocation	= numVecs * type.getArraySize() * strideForLocation;
			InterfaceLayoutEntry	entry;

			entry.name				= curPrefix + "[0]"; // Array interfaces are always postfixed with [0]
			entry.type				= elemBasicType;
			entry.blockLayoutNdx	= curBlockNdx;
			entry.offset			= curOffset;
			entry.arraySize			= type.getArraySize();
			entry.arrayStride		= stride*numVecs;
			entry.matrixStride		= stride;
			entry.locationNdx		= curLocation;
			entry.validate			= validate;

			curOffset += fieldSize;
			curLocation += deDivRoundUp32(fieldSizeForLocation, locationAlignSize);

			layout.interfaces.push_back(entry);
		}
		else
		{
			DE_ASSERT(elemType.isStructType() || elemType.isArrayType());

			for (int elemNdx = 0; elemNdx < type.getArraySize(); elemNdx++)
				computeXfbLayout(layout, curOffset, curLocation, curBlockNdx, curPrefix + "[" + de::toString(elemNdx) + "]", type.getElementType(), layoutFlags);
		}
	}
	else
	{
		DE_ASSERT(type.isStructType());

		for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end(); memberIter++)
			computeXfbLayout(layout, curOffset, curLocation, curBlockNdx, curPrefix + "." + memberIter->getName(), memberIter->getType(), (memberIter->getFlags() | layoutFlags) & FIELD_OPTIONS);

		curOffset = deAlign32(curOffset, baseAlignment);
	}
}

void computeXfbLayout (InterfaceLayout& layout, ShaderInterface& shaderInterface, BufferGeneralMapping& perBufferXfbOffsets, deUint32& locationsUsed)
{
	const int				numInterfaceBlocks	= shaderInterface.getNumInterfaceBlocks();
	int						curLocation			= 0;
	BufferGeneralMapping	bufferAlignments;
	BufferGeneralMapping	buffersList;
	BufferGeneralMapping	bufferStrideGroup;
	BufferUsedRangesMap		bufferUsedRanges;

	for (int blockNdx = 0; blockNdx < numInterfaceBlocks; blockNdx++)
	{
		const InterfaceBlock&	interfaceBlock	= shaderInterface.getInterfaceBlock(blockNdx);
		const int				xfbBuffer		= interfaceBlock.getXfbBuffer();

		buffersList[xfbBuffer] = 1;
		bufferStrideGroup[xfbBuffer] = xfbBuffer;
	}

	for (BufferGeneralMapping::const_iterator xfbBuffersIter = buffersList.begin(); xfbBuffersIter != buffersList.end(); xfbBuffersIter++)
	{
		const int	xfbBufferAnalyzed	= xfbBuffersIter->first;

		for (int blockNdx = 0; blockNdx < numInterfaceBlocks; blockNdx++)
		{
			InterfaceBlock&	interfaceBlock	= shaderInterface.getInterfaceBlockForModify(blockNdx);

			if (interfaceBlock.getXfbBuffer() == xfbBufferAnalyzed)
			{
				const bool			hasInstanceName		= interfaceBlock.hasInstanceName();
				const std::string	blockPrefix			= hasInstanceName ? (interfaceBlock.getBlockName() + ".") : "";
				const int			numInstances		= interfaceBlock.isArray() ? interfaceBlock.getArraySize() : 1;
				int					activeBlockNdx		= (int)layout.blocks.size();
				int					startInterfaceNdx	= (int)layout.interfaces.size();
				int					startLocationNdx	= (int)curLocation;
				int					interfaceAlignement	= computeInterfaceBlockAlignment(interfaceBlock);
				int					curOffset			= 0;
				int					blockSize			= 0;

				do
				{
					const int		xfbFirstInstanceBuffer			= interfaceBlock.getXfbBuffer();
					int&			xfbFirstInstanceBufferOffset	= perBufferXfbOffsets[xfbFirstInstanceBuffer];
					const int		savedLayoutInterfacesNdx		= (int)layout.interfaces.size();
					const int		savedCurOffset					= curOffset;
					const int		savedCurLocation				= curLocation;
					UsedRangeList&	usedRanges						= bufferUsedRanges[xfbFirstInstanceBuffer];
					bool			fitIntoBuffer					= true;

					// GLSL 4.60
					// Further, if applied to an aggregate containing a double, the offset must also be a multiple of 8,
					// and the space taken in the buffer will be a multiple of 8.
					xfbFirstInstanceBufferOffset	= deAlign32(xfbFirstInstanceBufferOffset, interfaceAlignement);

					for (InterfaceBlock::ConstIterator memberIter = interfaceBlock.begin(); memberIter != interfaceBlock.end(); memberIter++)
					{
						const InterfaceBlockMember& member	= *memberIter;

						computeXfbLayout(layout, curOffset, curLocation, activeBlockNdx, blockPrefix + member.getName(), member.getType(), member.getFlags() & FIELD_OPTIONS);
					}

					// GLSL 4.60
					// Further, if applied to an aggregate containing a double, the offset must also be a multiple of 8,
					// and the space taken in the buffer will be a multiple of 8.
					blockSize	= deAlign32(curOffset, interfaceAlignement);

					// Overlapping check
					for (UsedRangeList::const_iterator	usedRangeIt = usedRanges.begin();
														usedRangeIt != usedRanges.end();
														++usedRangeIt)
					{
						const int&	usedRangeStart	= usedRangeIt->first;
						const int&	usedRangeEnd	= usedRangeIt->second;
						const int	genRangeStart	= xfbFirstInstanceBufferOffset;
						const int	genRangeEnd		= xfbFirstInstanceBufferOffset + blockSize;

						// Validate if block has overlapping
						if (isOverlaped(genRangeStart, genRangeEnd, usedRangeStart, usedRangeEnd))
						{
							// Restart from obstacle interface end
							fitIntoBuffer					= false;

							DE_ASSERT(xfbFirstInstanceBufferOffset > usedRangeEnd);

							// Bump up interface start to the end of used range
							xfbFirstInstanceBufferOffset	= usedRangeEnd;

							// Undo allocation
							curOffset						= savedCurOffset;
							curLocation						= savedCurLocation;

							layout.interfaces.resize(savedLayoutInterfacesNdx);
						}
					}

					if (fitIntoBuffer)
						break;
				} while (true);

				const int	xfbFirstInstanceBuffer			= interfaceBlock.getXfbBuffer();
				const int	xfbFirstInstanceBufferOffset	= perBufferXfbOffsets[xfbFirstInstanceBuffer];
				const int	endInterfaceNdx					= (int)layout.interfaces.size();
				const int	blockSizeInLocations			= curLocation - startLocationNdx;

				curLocation -= blockSizeInLocations;

				if (numInstances > 1)
					interfaceBlock.setFlag(LAYOUT_XFBSTRIDE);

				// Create block layout entries for each instance.
				for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
				{
					// Allocate entry for instance.
					layout.blocks.push_back(BlockLayoutEntry());

					BlockLayoutEntry&	blockEntry		= layout.blocks.back();
					const int			xfbBuffer		= xfbFirstInstanceBuffer + instanceNdx;
					int&				xfbBufferOffset	= perBufferXfbOffsets[xfbBuffer];

					DE_ASSERT(xfbBufferOffset <= xfbFirstInstanceBufferOffset);

					xfbBufferOffset					= xfbFirstInstanceBufferOffset;

					blockEntry.name					= interfaceBlock.getBlockName();
					blockEntry.xfbBuffer			= xfbBuffer;
					blockEntry.xfbOffset			= xfbBufferOffset;
					blockEntry.xfbSize				= blockSize;
					blockEntry.blockDeclarationNdx	= blockNdx;
					blockEntry.instanceNdx			= instanceNdx;
					blockEntry.locationNdx			= curLocation;
					blockEntry.locationSize			= blockSizeInLocations;

					xfbBufferOffset	+= blockSize;
					curLocation		+= blockSizeInLocations;

					// Compute active interface set for block.
					for (int interfaceNdx = startInterfaceNdx; interfaceNdx < endInterfaceNdx; interfaceNdx++)
						blockEntry.activeInterfaceIndices.push_back(interfaceNdx);

					if (interfaceBlock.isArray())
						blockEntry.name += "[" + de::toString(instanceNdx) + "]";

					bufferUsedRanges[xfbBuffer].push_back(UsedRange(blockEntry.xfbOffset, blockEntry.xfbOffset + blockEntry.xfbSize));

					// Store maximum per-buffer alignment
					bufferAlignments[xfbBuffer] = std::max(interfaceAlignement, bufferAlignments[xfbBuffer]);

					// Buffers bound through instanced arrays must have same stride (and alignment)
					bufferStrideGroup[xfbBuffer] = bufferStrideGroup[xfbFirstInstanceBuffer];
				}
			}
		}
	}

	// All XFB buffers within group must have same stride
	{
		BufferGeneralMapping groupStride;

		for (BufferGeneralMapping::const_iterator xfbBuffersIter = perBufferXfbOffsets.begin(); xfbBuffersIter != perBufferXfbOffsets.end(); xfbBuffersIter++)
		{
			const int	xfbBuffer	= xfbBuffersIter->first;
			const int	xfbStride	= perBufferXfbOffsets[xfbBuffer];
			const int	group		= bufferStrideGroup[xfbBuffer];

			groupStride[group] = std::max(groupStride[group], xfbStride);
		}

		for (BufferGeneralMapping::const_iterator xfbBuffersIter = perBufferXfbOffsets.begin(); xfbBuffersIter != perBufferXfbOffsets.end(); xfbBuffersIter++)
		{
			const int	xfbBuffer	= xfbBuffersIter->first;
			const int	group		= bufferStrideGroup[xfbBuffer];

			perBufferXfbOffsets[xfbBuffer] = groupStride[group];
		}
	}

	// All XFB buffers within group must have same stride alignment
	{
		BufferGeneralMapping groupAlignment;

		for (BufferGeneralMapping::const_iterator xfbBuffersIter = perBufferXfbOffsets.begin(); xfbBuffersIter != perBufferXfbOffsets.end(); xfbBuffersIter++)
		{
			const int	xfbBuffer	= xfbBuffersIter->first;
			const int	group		= bufferStrideGroup[xfbBuffer];
			const int	xfbAlign	= bufferAlignments[xfbBuffer];

			groupAlignment[group] = std::max(groupAlignment[group], xfbAlign);
		}

		for (BufferGeneralMapping::const_iterator xfbBuffersIter = perBufferXfbOffsets.begin(); xfbBuffersIter != perBufferXfbOffsets.end(); xfbBuffersIter++)
		{
			const int	xfbBuffer	= xfbBuffersIter->first;
			const int	group		= bufferStrideGroup[xfbBuffer];

			bufferAlignments[xfbBuffer] = groupAlignment[group];
		}
	}

	// GLSL 4.60
	// If the buffer is capturing any outputs with double-precision components, the stride must be a multiple of 8, ...
	for (BufferGeneralMapping::const_iterator xfbBuffersIter = perBufferXfbOffsets.begin(); xfbBuffersIter != perBufferXfbOffsets.end(); xfbBuffersIter++)
	{
		const int	xfbBuffer	= xfbBuffersIter->first;
		const int	xfbAlign	= bufferAlignments[xfbBuffer];
		int&		xfbOffset	= perBufferXfbOffsets[xfbBuffer];

		xfbOffset	= deAlign32(xfbOffset, xfbAlign);
	}

	// Keep stride in interface blocks
	for (int blockNdx = 0; blockNdx < (int)layout.blocks.size(); blockNdx++)
		layout.blocks[blockNdx].xfbStride	= perBufferXfbOffsets[layout.blocks[blockNdx].xfbBuffer];

	locationsUsed = static_cast<deUint32>(curLocation);
}

// Value generator.

void generateValue (const InterfaceLayoutEntry& entry, void* basePtr, de::Random& rnd)
{
	const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);
	const int			scalarSize	= glu::getDataTypeScalarSize(entry.type);
	const bool			isMatrix	= glu::isDataTypeMatrix(entry.type);
	const int			numVecs		= isMatrix ? glu::getDataTypeMatrixNumColumns(entry.type) : 1;
	const int			vecSize		= scalarSize / numVecs;
	const bool			isArray		= entry.arraySize > 1;
	const size_t		compSize	= getDataTypeByteSize(scalarType);

	DE_ASSERT(scalarSize%numVecs == 0);

	for (int elemNdx = 0; elemNdx < entry.arraySize; elemNdx++)
	{
		deUint8* elemPtr = (deUint8*)basePtr + entry.offset + (isArray ? elemNdx*entry.arrayStride : 0);

		for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
		{
			deUint8* vecPtr = elemPtr + (isMatrix ? vecNdx*entry.matrixStride : 0);

			for (int compNdx = 0; compNdx < vecSize; compNdx++)
			{
				deUint8*	compPtr = vecPtr + compSize*compNdx;
				const int	sign	= rnd.getBool() ? +1 : -1;
				const int	value	= rnd.getInt(1, 127);

				switch (scalarType)
				{
					case glu::TYPE_DOUBLE:	*((double*)compPtr)		= (double)  (sign * value);	break;
					case glu::TYPE_FLOAT:	*((float*)compPtr)		= (float)   (sign * value);	break;
					case glu::TYPE_INT:		*((deInt32*)compPtr)	= (deInt32) (sign * value);	break;
					case glu::TYPE_UINT:	*((deUint32*)compPtr)	= (deUint32)(       value);	break;
					default:
						DE_ASSERT(false);
				}
			}
		}
	}
}

void generateValues (const InterfaceLayout& layout, const std::map<int, void*>& blockPointers, deUint32 seed)
{
	de::Random	rnd			(seed);
	int			numBlocks	= (int)layout.blocks.size();

	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
	{
		void*	basePtr		= blockPointers.find(blockNdx)->second;
		int		numEntries	= (int)layout.blocks[blockNdx].activeInterfaceIndices.size();

		for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
		{
			const InterfaceLayoutEntry& entry = layout.interfaces[layout.blocks[blockNdx].activeInterfaceIndices[entryNdx]];

			if (entry.validate)
				generateValue(entry, basePtr, rnd);
		}
	}
}

// Shader generator.

struct Indent
{
	int level;
	Indent (int level_) : level(level_) {}
};

std::ostream& operator<< (std::ostream& str, const Indent& indent)
{
	for (int i = 0; i < indent.level; i++)
		str << "\t";
	return str;
}

void	generateDeclaration			(std::ostringstream& src, const VarType& type, const std::string& name, int indentLevel, deUint32 unusedHints, deUint32 flagsMask, deUint32 buffer, deUint32 stride, deUint32 offset);
void	generateDeclaration			(std::ostringstream& src, const InterfaceBlockMember& member, int indentLevel, deUint32 buffer, deUint32 stride, deUint32 offset);
void	generateDeclaration			(std::ostringstream& src, const StructType& structType, int indentLevel);

void	generateLocalDeclaration	(std::ostringstream& src, const StructType& structType, int indentLevel);
void	generateFullDeclaration		(std::ostringstream& src, const StructType& structType, int indentLevel);

void generateDeclaration (std::ostringstream& src, const StructType& structType, int indentLevel)
{
	DE_ASSERT(structType.hasTypeName());
	generateFullDeclaration(src, structType, indentLevel);
	src << ";\n";
}

void generateFullDeclaration (std::ostringstream& src, const StructType& structType, int indentLevel)
{
	src << "struct";
	if (structType.hasTypeName())
		src << " " << structType.getTypeName();
	src << "\n" << Indent(indentLevel) << "{\n";

	for (StructType::ConstIterator memberIter = structType.begin(); memberIter != structType.end(); memberIter++)
	{
		src << Indent(indentLevel + 1);
		generateDeclaration(src, memberIter->getType(), memberIter->getName(), indentLevel + 1, memberIter->getFlags() & FIELD_OPTIONS, ~LAYOUT_MASK, 0u, 0u, 0u);
	}

	src << Indent(indentLevel) << "}";
}

void generateLocalDeclaration (std::ostringstream& src, const StructType& structType, int /* indentLevel */)
{
	src << structType.getTypeName();
}

void generateLayoutAndPrecisionDeclaration (std::ostringstream& src, deUint32 flags, deUint32 buffer, deUint32 stride, deUint32 offset)
{
	if ((flags & LAYOUT_MASK) != 0)
		src << "layout(" << LayoutFlagsFmt(flags & LAYOUT_MASK, buffer, stride, offset) << ") ";

	if ((flags & PRECISION_MASK) != 0)
		src << PrecisionFlagsFmt(flags & PRECISION_MASK) << " ";
}

void generateDeclaration (std::ostringstream& src, const VarType& type, const std::string& name, int indentLevel, deUint32 fieldHints, deUint32 flagsMask, deUint32 buffer, deUint32 stride, deUint32 offset)
{
	if (fieldHints & FIELD_MISSING)
		src << "// ";

	generateLayoutAndPrecisionDeclaration(src, type.getFlags() & flagsMask, buffer, stride, offset);

	if (type.isBasicType())
		src << glu::getDataTypeName(type.getBasicType()) << " " << name;
	else if (type.isArrayType())
	{
		std::vector<int>	arraySizes;
		const VarType*		curType		= &type;
		while (curType->isArrayType())
		{
			arraySizes.push_back(curType->getArraySize());
			curType = &curType->getElementType();
		}

		generateLayoutAndPrecisionDeclaration(src, curType->getFlags() & flagsMask, buffer, stride, offset);

		if (curType->isBasicType())
			src << glu::getDataTypeName(curType->getBasicType());
		else
		{
			DE_ASSERT(curType->isStructType());
			generateLocalDeclaration(src, curType->getStruct(), indentLevel+1);
		}

		src << " " << name;

		for (std::vector<int>::const_iterator sizeIter = arraySizes.begin(); sizeIter != arraySizes.end(); sizeIter++)
			src << "[" << *sizeIter << "]";
	}
	else
	{
		generateLocalDeclaration(src, type.getStruct(), indentLevel+1);
		src << " " << name;
	}

	src << ";";

	// Print out unused hints.
	if (fieldHints & FIELD_MISSING)
		src << " // missing field";
	else if (fieldHints & FIELD_UNASSIGNED)
		src << " // unassigned";

	src << "\n";
}

void generateDeclaration (std::ostringstream& src, const InterfaceBlockMember& member, int indentLevel, deUint32 buffer, deUint32 stride, deUint32 offset)
{
	if ((member.getFlags() & LAYOUT_MASK) != 0)
		src << "layout(" << LayoutFlagsFmt(member.getFlags() & LAYOUT_MASK, buffer, stride, offset) << ") ";

	generateDeclaration(src, member.getType(), member.getName(), indentLevel, member.getFlags() & FIELD_OPTIONS, ~0u, buffer, stride, offset);
}

deUint32 getBlockMemberOffset (int blockNdx, const InterfaceBlock& block, const InterfaceBlockMember& member, const InterfaceLayout& layout)
{
	std::ostringstream	name;
	const VarType*		curType = &member.getType();

	if (block.getInstanceName().length() != 0)
		name << block.getBlockName() << ".";	// \note InterfaceLayoutEntry uses block name rather than instance name

	name << member.getName();

	while (!curType->isBasicType())
	{
		if (curType->isArrayType())
		{
			name << "[0]";
			curType = &curType->getElementType();
		}

		if (curType->isStructType())
		{
			const StructType::ConstIterator firstMember = curType->getStruct().begin();

			name << "." << firstMember->getName();
			curType = &firstMember->getType();
		}
	}

	const int interfaceLayoutNdx = layout.getInterfaceLayoutIndex(blockNdx, name.str());
	DE_ASSERT(interfaceLayoutNdx >= 0);

	return layout.interfaces[interfaceLayoutNdx].offset;
}

template<typename T>
void semiShuffle (std::vector<T>& v)
{
	const std::vector<T>	src	= v;
	int						i	= -1;
	int						n	= static_cast<int>(src.size());

	v.clear();

	while (n)
	{
		i += n;
		v.push_back(src[i]);
		n = (n > 0 ? 1 - n : -1 - n);
	}
}

template<typename T>
//! \note Stores pointers to original elements
class Traverser
{
public:
	template<typename Iter>
	Traverser (const Iter beg, const Iter end, const bool shuffled)
	{
		for (Iter it = beg; it != end; ++it)
			m_elements.push_back(&(*it));

		if (shuffled)
			semiShuffle(m_elements);

		m_next = m_elements.begin();
	}

	T* next (void)
	{
		if (m_next != m_elements.end())
			return *m_next++;
		else
			return DE_NULL;
	}

private:
	typename std::vector<T*>					m_elements;
	typename std::vector<T*>::const_iterator	m_next;
};

void generateDeclaration (std::ostringstream& src, int blockNdx, const InterfaceBlock& block, const InterfaceLayout& layout, bool shuffleUniformMembers)
{
	const int indentOne		= 1;
	const int ndx			= layout.getBlockLayoutIndex(blockNdx, 0);
	const int locationNdx	= layout.blocks[ndx].locationNdx;
	const int xfbOffset		= layout.blocks[ndx].xfbOffset;
	const int xfbBuffer		= layout.blocks[ndx].xfbBuffer;
	const int xfbStride		= layout.blocks[ndx].xfbStride;

	src << "layout(";
	src << "location = " << locationNdx;
	if ((block.getFlags() & LAYOUT_MASK) != 0)
		src << ", " << LayoutFlagsFmt(block.getFlags() & LAYOUT_MASK, xfbBuffer, xfbStride, xfbOffset);
	src << ") out " << block.getBlockName();

	src << " //"
		<< " sizeInBytes=" << layout.blocks[ndx].xfbSize
		<< " sizeInLocations=" << layout.blocks[ndx].locationSize;

	src << "\n{\n";

	Traverser<const InterfaceBlockMember> interfaces(block.begin(), block.end(), shuffleUniformMembers);

	while (const InterfaceBlockMember* pUniform = interfaces.next())
	{
		src << Indent(indentOne);
		generateDeclaration(src, *pUniform, indentOne, xfbBuffer, xfbStride, xfbOffset + getBlockMemberOffset(blockNdx, block, *pUniform, layout));
	}

	src << "}";

	if (block.hasInstanceName())
	{
		src << " " << block.getInstanceName();
		if (block.isArray())
			src << "[" << block.getArraySize() << "]";
	}
	else
		DE_ASSERT(!block.isArray());

	src << ";\n";
}

int generateValueSrc (std::ostringstream& src, const InterfaceLayoutEntry& entry, const void* basePtr, int elementNdx)
{
	const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);
	const int			scalarSize	= glu::getDataTypeScalarSize(entry.type);
	const bool			isArray		= entry.arraySize > 1;
	const deUint8*		elemPtr		= (const deUint8*)basePtr + entry.offset + (isArray ? elementNdx * entry.arrayStride : 0);
	const size_t		compSize	= getDataTypeByteSize(scalarType);

	if (scalarSize > 1)
		src << glu::getDataTypeName(entry.type) << "(";

	if (glu::isDataTypeMatrix(entry.type))
	{
		const int	numRows	= glu::getDataTypeMatrixNumRows(entry.type);
		const int	numCols	= glu::getDataTypeMatrixNumColumns(entry.type);

		DE_ASSERT(scalarType == glu::TYPE_FLOAT || scalarType == glu::TYPE_DOUBLE);

		// Constructed in column-wise order.
		for (int colNdx = 0; colNdx < numCols; colNdx++)
		{
			for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
			{
				const deUint8*	compPtr	= elemPtr + (colNdx * entry.matrixStride + rowNdx * compSize);
				const float		compVal	= (scalarType == glu::TYPE_FLOAT) ? *((const float*)compPtr)
										: (scalarType == glu::TYPE_DOUBLE) ? (float)*((const double*)compPtr)
										: 0.0f;

				if (colNdx > 0 || rowNdx > 0)
					src << ", ";

				src << de::floatToString(compVal, 1);
			}
		}
	}
	else
	{
		for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
		{
			const deUint8* compPtr = elemPtr + scalarNdx * compSize;

			if (scalarNdx > 0)
				src << ", ";

			switch (scalarType)
			{
				case glu::TYPE_DOUBLE:	src << de::floatToString((float)(*((const double*)compPtr)), 1);	break;
				case glu::TYPE_FLOAT:	src << de::floatToString(*((const float*)compPtr), 1) << "f";		break;
				case glu::TYPE_INT:		src << *((const int*)compPtr);										break;
				case glu::TYPE_UINT:	src << *((const deUint32*)compPtr) << "u";							break;
				default:				DE_ASSERT(false && "Invalid type");									break;
			}
		}
	}

	if (scalarSize > 1)
		src << ")";

	return static_cast<int>(elemPtr - static_cast<const deUint8*>(basePtr));
}

void writeMatrixTypeSrc (int							columnCount,
						 int							rowCount,
						 std::string					type,
						 std::ostringstream&			src,
						 const std::string&				srcName,
						 const void*					basePtr,
						 const InterfaceLayoutEntry&	entry,
						 bool							vector)
{
	if (vector)	// generateTestSrcMatrixPerVec
	{
		for (int colNdx = 0; colNdx < columnCount; colNdx++)
		{
			src << "\t" << srcName << "[" << colNdx << "] = ";

			if (glu::isDataTypeMatrix(entry.type))
			{
				const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);
				const int			scalarSize	= glu::getDataTypeScalarSize(entry.type);
				const deUint8*		compPtr		= (const deUint8*)basePtr + entry.offset;

				if (scalarSize > 1)
					src << type << "(";

				for (int rowNdx = 0; rowNdx < rowCount; rowNdx++)
				{
					const float		compVal	= (scalarType == glu::TYPE_FLOAT) ? *((const float*)compPtr)
											: (scalarType == glu::TYPE_DOUBLE) ? (float)*((const double*)compPtr)
											: 0.0f;

					src << de::floatToString(compVal, 1);

					if (rowNdx < rowCount-1)
						src << ", ";
				}

				src << ");\n";
			}
			else
			{
				generateValueSrc(src, entry, basePtr, 0);
				src << "[" << colNdx << "];\n";
			}
		}
	}
	else		// generateTestSrcMatrixPerElement
	{
		const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);

		for (int colNdx = 0; colNdx < columnCount; colNdx++)
		{
			for (int rowNdx = 0; rowNdx < rowCount; rowNdx++)
			{
				src << "\t" << srcName << "[" << colNdx << "][" << rowNdx << "] = ";
				if (glu::isDataTypeMatrix(entry.type))
				{
					const deUint8*	elemPtr		= (const deUint8*)basePtr + entry.offset;
					const size_t	compSize	= getDataTypeByteSize(scalarType);
					const deUint8*	compPtr		= elemPtr + (colNdx * entry.matrixStride + rowNdx * compSize);
					const float		compVal		= (scalarType == glu::TYPE_FLOAT) ? *((const float*)compPtr)
												: (scalarType == glu::TYPE_DOUBLE) ? (float)*((const double*)compPtr)
												: 0.0f;

					src << de::floatToString(compVal, 1) << ";\n";
				}
				else
				{
					generateValueSrc(src, entry, basePtr, 0);
					src << "[" << colNdx << "][" << rowNdx << "];\n";
				}
			}
		}
	}
}

void generateTestSrcMatrixPerVec (std::ostringstream&			src,
								  glu::DataType					elementType,
								  const std::string&			srcName,
								  const void*					basePtr,
								  const InterfaceLayoutEntry&	entry)
{
	switch (elementType)
	{
		case glu::TYPE_FLOAT_MAT2:		writeMatrixTypeSrc(2, 2, "vec2", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT2X3:	writeMatrixTypeSrc(2, 3, "vec3", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT2X4:	writeMatrixTypeSrc(2, 4, "vec4", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT3X4:	writeMatrixTypeSrc(3, 4, "vec4", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT4:		writeMatrixTypeSrc(4, 4, "vec4", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT4X2:	writeMatrixTypeSrc(4, 2, "vec2", src, srcName, basePtr, entry, true);	break;
		case glu::TYPE_FLOAT_MAT4X3:	writeMatrixTypeSrc(4, 3, "vec3", src, srcName, basePtr, entry, true);	break;
		default:						DE_ASSERT(false && "Invalid type");										break;
	}
}

void generateTestSrcMatrixPerElement (std::ostringstream&			src,
									  glu::DataType					elementType,
									  const std::string&			srcName,
									  const void*					basePtr,
									  const InterfaceLayoutEntry&	entry)
{
	std::string type = "float";
	switch (elementType)
	{
		case glu::TYPE_FLOAT_MAT2:		writeMatrixTypeSrc(2, 2, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT2X3:	writeMatrixTypeSrc(2, 3, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT2X4:	writeMatrixTypeSrc(2, 4, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT3X4:	writeMatrixTypeSrc(3, 4, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT4:		writeMatrixTypeSrc(4, 4, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT4X2:	writeMatrixTypeSrc(4, 2, type, src, srcName, basePtr, entry, false);	break;
		case glu::TYPE_FLOAT_MAT4X3:	writeMatrixTypeSrc(4, 3, type, src, srcName, basePtr, entry, false);	break;
		default:						DE_ASSERT(false && "Invalid type");										break;
	}
}

void generateSingleAssignment (std::ostringstream&			src,
							   glu::DataType				elementType,
							   const std::string&			srcName,
							   const void*					basePtr,
							   const InterfaceLayoutEntry&	entry,
							   MatrixLoadFlags				matrixLoadFlag)
{
	if (matrixLoadFlag == LOAD_FULL_MATRIX)
	{
		src << "\t" << srcName << " = ";
		generateValueSrc(src, entry, basePtr, 0);
		src << ";\n";
	}
	else
	{
		if (glu::isDataTypeMatrix(elementType))
		{
			generateTestSrcMatrixPerVec		(src, elementType, srcName, basePtr, entry);
			generateTestSrcMatrixPerElement	(src, elementType, srcName, basePtr, entry);
		}
	}
}

void generateAssignment (std::ostringstream&	src,
						 const InterfaceLayout&	layout,
						 const VarType&			type,
						 const std::string&		srcName,
						 const std::string&		apiName,
						 int					blockNdx,
						 const void*			basePtr,
						 MatrixLoadFlags		matrixLoadFlag)
{
	if (type.isBasicType() || (type.isArrayType() && type.getElementType().isBasicType()))
	{
		// Basic type or array of basic types.
		bool						isArray				= type.isArrayType();
		glu::DataType				elementType			= isArray ? type.getElementType().getBasicType() : type.getBasicType();
		std::string					fullApiName			= std::string(apiName) + (isArray ? "[0]" : ""); // Arrays are always postfixed with [0]
		int							interfaceLayoutNdx	= layout.getInterfaceLayoutIndex(blockNdx, fullApiName);
		const InterfaceLayoutEntry&	entry				= layout.interfaces[interfaceLayoutNdx];

		if (isArray)
		{
			for (int elemNdx = 0; elemNdx < type.getArraySize(); elemNdx++)
			{
				src << "\t" << srcName << "[" << elemNdx << "] = ";
				generateValueSrc(src, entry, basePtr, elemNdx);
				src << ";\n";
			}
		}
		else
		{
			generateSingleAssignment(src, elementType, srcName, basePtr, entry, matrixLoadFlag);
		}
	}
	else if (type.isArrayType())
	{
		const VarType& elementType = type.getElementType();

		for (int elementNdx = 0; elementNdx < type.getArraySize(); elementNdx++)
		{
			const std::string op				= std::string("[") + de::toString(elementNdx) + "]";
			const std::string elementSrcName	= std::string(srcName) + op;
			const std::string elementApiName	= std::string(apiName) + op;

			generateAssignment(src, layout, elementType, elementSrcName, elementApiName, blockNdx, basePtr, LOAD_FULL_MATRIX);
		}
	}
	else
	{
		DE_ASSERT(type.isStructType());

		for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end(); memberIter++)
		{
			const StructMember&	member			= *memberIter;
			const std::string	op				= std::string(".") + member.getName();
			const std::string	memberSrcName	= std::string(srcName) + op;
			const std::string	memberApiName	= std::string(apiName) + op;

			if (0 == (member.getFlags() & (FIELD_UNASSIGNED | FIELD_MISSING)))
				generateAssignment(src, layout, memberIter->getType(), memberSrcName, memberApiName, blockNdx, basePtr, LOAD_FULL_MATRIX);
		}
	}
}

void generateAssignment (std::ostringstream&			src,
						 const InterfaceLayout&			layout,
						 const ShaderInterface&			shaderInterface,
						 const std::map<int, void*>&	blockPointers,
						 MatrixLoadFlags				matrixLoadFlag)
{
	for (int blockNdx = 0; blockNdx < shaderInterface.getNumInterfaceBlocks(); blockNdx++)
	{
		const InterfaceBlock& block = shaderInterface.getInterfaceBlock(blockNdx);

		bool			hasInstanceName	= block.hasInstanceName();
		bool			isArray			= block.isArray();
		int				numInstances	= isArray ? block.getArraySize() : 1;
		std::string		apiPrefix		= hasInstanceName ? block.getBlockName() + "." : std::string("");

		DE_ASSERT(!isArray || hasInstanceName);

		for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
		{
			std::string		instancePostfix		= isArray ? std::string("[") + de::toString(instanceNdx) + "]" : std::string("");
			std::string		blockInstanceName	= block.getBlockName() + instancePostfix;
			std::string		srcPrefix			= hasInstanceName ? block.getInstanceName() + instancePostfix + "." : std::string("");
			int				blockLayoutNdx		= layout.getBlockLayoutIndex(blockNdx, instanceNdx);
			void*			basePtr				= blockPointers.find(blockLayoutNdx)->second;

			for (InterfaceBlock::ConstIterator interfaceMemberIter = block.begin(); interfaceMemberIter != block.end(); interfaceMemberIter++)
			{
				const InterfaceBlockMember& interfaceMember = *interfaceMemberIter;

				if ((interfaceMember.getFlags() & (FIELD_MISSING | FIELD_UNASSIGNED)) == 0)
				{
					std::string srcName = srcPrefix + interfaceMember.getName();
					std::string apiName = apiPrefix + interfaceMember.getName();

					generateAssignment(src, layout, interfaceMember.getType(), srcName, apiName, blockNdx, basePtr, matrixLoadFlag);
				}
			}
		}
	}
}

std::string generatePassthroughShader ()
{
	std::ostringstream	src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";

	src << "\n"
		   "void main (void)\n"
		   "{\n"
		   "}\n";

	return src.str();
}

std::string generateTestShader (const ShaderInterface& shaderInterface, const InterfaceLayout& layout, const std::map<int, void*>& blockPointers, MatrixLoadFlags matrixLoadFlag, TestStageFlags testStageFlags, bool shuffleUniformMembers)
{
	std::ostringstream				src;
	std::vector<const StructType*>	namedStructs;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n";

	if (testStageFlags == TEST_STAGE_GEOMETRY)
	{
		src << "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n\n";
	}

	shaderInterface.getNamedStructs(namedStructs);
	for (std::vector<const StructType*>::const_iterator structIter = namedStructs.begin(); structIter != namedStructs.end(); structIter++)
		generateDeclaration(src, **structIter, 0);

	for (int blockNdx = 0; blockNdx < shaderInterface.getNumInterfaceBlocks(); blockNdx++)
	{
		const InterfaceBlock& block = shaderInterface.getInterfaceBlock(blockNdx);

		generateDeclaration(src, blockNdx, block, layout, shuffleUniformMembers);
	}

	src << "\n"
		   "void main (void)\n"
		   "{\n";

	generateAssignment(src, layout, shaderInterface, blockPointers, matrixLoadFlag);

	if (testStageFlags == TEST_STAGE_GEOMETRY)
	{
		src << "\n"
			<< "\tEmitVertex();\n"
			<< "\tEndPrimitive();\n";
	}

	src << "}\n";

	return src.str();
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			geometryModule,
									   const VkExtent2D				renderSize)
{
	const std::vector<VkViewport>				viewports						(1, makeViewport(renderSize));
	const std::vector<VkRect2D>					scissors						(1, makeRect2D(renderSize));
	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,													// const void*									pNext
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags		flags
		0u,															// deUint32										vertexBindingDescriptionCount
		DE_NULL,													// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		0u,															// deUint32										vertexAttributeDescriptionCount
		DE_NULL,													// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	return makeGraphicsPipeline(vk,									// const DeviceInterface&						vk
								device,								// const VkDevice								device
								pipelineLayout,						// const VkPipelineLayout						pipelineLayout
								vertexModule,						// const VkShaderModule							vertexShaderModule
								DE_NULL,							// const VkShaderModule							tessellationControlModule
								DE_NULL,							// const VkShaderModule							tessellationEvalModule
								geometryModule,						// const VkShaderModule							geometryShaderModule
								DE_NULL,							// const VkShaderModule							m_maxGeometryBlocksShaderModule
								renderPass,							// const VkRenderPass							renderPass
								viewports,							// const std::vector<VkViewport>&				viewports
								scissors,							// const std::vector<VkRect2D>&					scissors
								VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology					topology
								0u,									// const deUint32								subpass
								0u,									// const deUint32								patchControlPoints
								&vertexInputStateCreateInfo);		// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
}

// InterfaceBlockCaseInstance

class InterfaceBlockCaseInstance : public vkt::TestInstance
{
public:
									InterfaceBlockCaseInstance	(Context&							context,
																 const InterfaceLayout&				layout,
																 const std::map<int, void*>&		blockPointers,
																 const std::vector<deUint8>&		data,
																 const std::vector<VkDeviceSize>&	tfBufBindingOffsets,
																 const std::vector<VkDeviceSize>&	tfBufBindingSizes,
																 const deUint32						locationsRequired,
																 const TestStageFlags				testStageFlags);

	virtual							~InterfaceBlockCaseInstance	(void);
	virtual tcu::TestStatus			iterate						(void);

private:
	Move<VkShaderModule>			getGeometryShaderModule		(const DeviceInterface&	vk,
																 const VkDevice			device);

	bool							usesFloat64					(void);
	std::string						validateValue				(const InterfaceLayoutEntry& entry, const void* basePtr0, const void* basePtr, const void* receivedBasePtr);
	std::string						validateValues				(const void* recievedDataPtr);

	typedef de::SharedPtr<vk::Unique<vk::VkBuffer> >	VkBufferSp;
	typedef de::SharedPtr<vk::Allocation>				AllocationSp;

	const InterfaceLayout&			m_layout;
	const std::vector<deUint8>&		m_data;
	const DeviceSizeVector&			m_tfBufBindingOffsets;
	const DeviceSizeVector&			m_tfBufBindingSizes;
	const std::map<int, void*>&		m_blockPointers;
	const deUint32					m_locationsRequired;
	const TestStageFlags			m_testStageFlags;
	const VkExtent2D				m_imageExtent2D;
};

InterfaceBlockCaseInstance::InterfaceBlockCaseInstance (Context&							ctx,
														const InterfaceLayout&				layout,
														const std::map<int, void*>&			blockPointers,
														const std::vector<deUint8>&			data,
														const std::vector<VkDeviceSize>&	tfBufBindingOffsets,
														const std::vector<VkDeviceSize>&	tfBufBindingSizes,
														const deUint32						locationsRequired,
														const TestStageFlags				testStageFlags)
	: vkt::TestInstance		(ctx)
	, m_layout				(layout)
	, m_data				(data)
	, m_tfBufBindingOffsets	(tfBufBindingOffsets)
	, m_tfBufBindingSizes	(tfBufBindingSizes)
	, m_blockPointers		(blockPointers)
	, m_locationsRequired	(locationsRequired)
	, m_testStageFlags		(testStageFlags)
	, m_imageExtent2D		(makeExtent2D(256u, 256u))
{
	const deUint32											componentsPerLocation		= 4u;
	const deUint32											componentsRequired			= m_locationsRequired * componentsPerLocation;
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const VkPhysicalDeviceLimits							limits						= getPhysicalDeviceProperties(vki, physDevice).limits;
	VkPhysicalDeviceTransformFeedbackPropertiesEXT			transformFeedbackProperties;
	VkPhysicalDeviceProperties2								deviceProperties2;

	if (transformFeedbackFeatures.transformFeedback == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedback feature is not supported");

	deMemset(&deviceProperties2, 0, sizeof(deviceProperties2));
	deMemset(&transformFeedbackProperties, 0x00, sizeof(transformFeedbackProperties));

	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &transformFeedbackProperties;

	transformFeedbackProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
	transformFeedbackProperties.pNext = DE_NULL;

	vki.getPhysicalDeviceProperties2(physDevice, &deviceProperties2);

	if (transformFeedbackProperties.maxTransformFeedbackBuffers < tfBufBindingSizes.size())
		TCU_THROW(NotSupportedError, "maxTransformFeedbackBuffers=" + de::toString(transformFeedbackProperties.maxTransformFeedbackBuffers) + " is less than required (" + de::toString(tfBufBindingSizes.size()) + ")");

	if (transformFeedbackProperties.maxTransformFeedbackBufferDataSize < m_data.size())
		TCU_THROW(NotSupportedError, "maxTransformFeedbackBufferDataSize=" + de::toString(transformFeedbackProperties.maxTransformFeedbackBufferDataSize) + " is less than required (" + de::toString(m_data.size()) + ")");

	if (m_testStageFlags == TEST_STAGE_VERTEX)
	{
		if (limits.maxVertexOutputComponents < componentsRequired)
			TCU_THROW(NotSupportedError, "maxVertexOutputComponents=" + de::toString(limits.maxVertexOutputComponents) + " is less than required (" + de::toString(componentsRequired) + ")");
	}

	if (m_testStageFlags == TEST_STAGE_GEOMETRY)
	{
		if (limits.maxGeometryOutputComponents < componentsRequired)
			TCU_THROW(NotSupportedError, "maxGeometryOutputComponents=" + de::toString(limits.maxGeometryOutputComponents) + " is less than required (" + de::toString(componentsRequired) + ")");
	}

	if (usesFloat64())
		m_context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);
}

InterfaceBlockCaseInstance::~InterfaceBlockCaseInstance (void)
{
}

bool InterfaceBlockCaseInstance::usesFloat64 (void)
{
	for (size_t layoutNdx = 0; layoutNdx< m_layout.interfaces.size(); ++layoutNdx)
		if (isDataTypeDoubleType(m_layout.interfaces[layoutNdx].type))
			return true;

	return false;
}

Move<VkShaderModule> InterfaceBlockCaseInstance::getGeometryShaderModule (const DeviceInterface&	vk,
																		  const VkDevice			device)
{
	if (m_testStageFlags == TEST_STAGE_GEOMETRY)
		return createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0u);

	return Move<VkShaderModule>();
}

tcu::TestStatus InterfaceBlockCaseInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const Move<VkShaderModule>		vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Move<VkShaderModule>		geomModule			(getGeometryShaderModule(vk, device));
	const Move<VkRenderPass>		renderPass			(makeRenderPass			(vk, device, VK_FORMAT_UNDEFINED));
	const Move<VkFramebuffer>		framebuffer			(makeFramebuffer		(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Move<VkPipelineLayout>	pipelineLayout		(makePipelineLayout		(vk, device));
	const Move<VkPipeline>			pipeline			(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertModule, *geomModule, m_imageExtent2D));
	const Move<VkCommandPool>		cmdPool				(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo		tfBufCreateInfo		= makeBufferCreateInfo(m_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>			tfBuf				= createBuffer(vk, device, &tfBufCreateInfo);
	const de::MovePtr<Allocation>	tfBufAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const deUint32					tfBufBindingCount	= static_cast<deUint32>(m_tfBufBindingOffsets.size());
	const std::vector<VkBuffer>		tfBufBindings		(tfBufBindingCount, *tfBuf);

	DE_ASSERT(tfBufBindings.size() == tfBufBindingCount);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	deMemset(tfBufAllocation->getHostPtr(), 0, m_data.size());
	flushMappedMemoryRange(vk, device, tfBufAllocation->getMemory(), tfBufAllocation->getOffset(), VK_WHOLE_SIZE);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, tfBufBindingCount, &tfBufBindings[0], &m_tfBufBindingOffsets[0], &m_tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		const VkMemoryBarrier tfMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,               // VkStructureType      sType;
			DE_NULL,                                        // const void*          pNext;
			VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,     // VkAccessFlags        outputMask;
			VK_ACCESS_HOST_READ_BIT                         // VkAccessFlags        inputMask;
		};
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, tfBufAllocation->getMemory(), tfBufAllocation->getOffset(), VK_WHOLE_SIZE);

	std::string result = validateValues(tfBufAllocation->getHostPtr());

	if (!result.empty())
		return tcu::TestStatus::fail(result);

	return tcu::TestStatus::pass("Pass");
}

std::string InterfaceBlockCaseInstance::validateValue (const InterfaceLayoutEntry& entry, const void* basePtr0, const void* basePtr, const void* receivedBasePtr)
{
	const glu::DataType	scalarType	= glu::getDataTypeScalarType(entry.type);
	const int			scalarSize	= glu::getDataTypeScalarSize(entry.type);
	const bool			isMatrix	= glu::isDataTypeMatrix(entry.type);
	const int			numVecs		= isMatrix ? glu::getDataTypeMatrixNumColumns(entry.type) : 1;
	const int			vecSize		= scalarSize / numVecs;
	const bool			isArray		= entry.arraySize > 1;
	const size_t		compSize	= getDataTypeByteSize(scalarType);
	std::string			result;

	DE_ASSERT(scalarSize%numVecs == 0);

	for (int elemNdx = 0; elemNdx < entry.arraySize; elemNdx++)
	{
		deUint8* elemPtr = (deUint8*)basePtr + entry.offset + (isArray ? elemNdx*entry.arrayStride : 0);

		for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
		{
			deUint8* vecPtr = elemPtr + (isMatrix ? vecNdx*entry.matrixStride : 0);

			for (int compNdx = 0; compNdx < vecSize; compNdx++)
			{
				const deUint8*	compPtr		= vecPtr + compSize*compNdx;
				const size_t	offset		= compPtr - (deUint8*)basePtr0;
				const deUint8*	receivedPtr	= (deUint8*)receivedBasePtr + offset;

				switch (scalarType)
				{
					case glu::TYPE_DOUBLE:
					{
						const double expected	= *((double*)compPtr);
						const double received	= *((double*)receivedPtr);

						if (deAbs(received - expected) > 0.05)
							result = "Mismatch at offset " + de::toString(offset) + " expected " + de::toString(expected) + " received " + de::toString(received);

						break;
					}
					case glu::TYPE_FLOAT:
					{
						const float expected	= *((float*)compPtr);
						const float received	= *((float*)receivedPtr);

						if (deAbs(received - expected) > 0.05)
							result = "Mismatch at offset " + de::toString(offset) + " expected " + de::toString(expected) + " received " + de::toString(received);

						break;
					}
					case glu::TYPE_INT:
					{
						const deInt32 expected	= *((deInt32*)compPtr);
						const deInt32 received	= *((deInt32*)receivedPtr);

						if (received != expected)
							result = "Mismatch at offset " + de::toString(offset) + " expected " + de::toString(expected) + " received " + de::toString(received);

						break;
					}
					case glu::TYPE_UINT:
					{
						const deUint32 expected	= *((deUint32*)compPtr);
						const deUint32 received	= *((deUint32*)receivedPtr);

						if (received != expected)
							result = "Mismatch at offset " + de::toString(offset) + " expected " + de::toString(expected) + " received " + de::toString(received);

						break;
					}
					default:
						DE_ASSERT(false);
				}

				if (!result.empty())
				{
					result += " (elemNdx=" + de::toString(elemNdx) + " vecNdx=" + de::toString(vecNdx) + " compNdx=" + de::toString(compNdx) + ")";

					return result;
				}
			}
		}
	}

	return result;
}

std::string InterfaceBlockCaseInstance::validateValues (const void* recievedDataPtr)
{
	const int	numBlocks	= (int)m_layout.blocks.size();

	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
	{
		void*	basePtr		= m_blockPointers.find(blockNdx)->second;
		int		numEntries	= (int)m_layout.blocks[blockNdx].activeInterfaceIndices.size();

		for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
		{
			const InterfaceLayoutEntry&	entry	= m_layout.interfaces[m_layout.blocks[blockNdx].activeInterfaceIndices[entryNdx]];
			const std::string			result	= entry.validate ? validateValue(entry, &m_data[0], basePtr, recievedDataPtr) : "";

			if (!result.empty())
			{
				tcu::TestLog&			log		= m_context.getTestContext().getLog();
				std::vector<deUint8>	mask	= createMask(m_layout, m_blockPointers, &m_data[0], m_data.size());
				std::ostringstream		str;

				str << "Error at entry '" << entry.name << "' block '" << m_layout.blocks[blockNdx].name << "'" << std::endl;
				str << result << std::endl;

				str << m_layout;

				str << "Xfb buffer offsets: " << m_tfBufBindingOffsets << std::endl;
				str << "Xfb buffer sizes: " << m_tfBufBindingSizes << std::endl << std::endl;

				dumpBytes(str, "Expected:", &m_data[0], m_data.size(), &mask[0]);
				dumpBytes(str, "Retrieved:", recievedDataPtr, m_data.size(), &mask[0]);

				dumpBytes(str, "Expected (unfiltered):", &m_data[0], m_data.size());
				dumpBytes(str, "Retrieved (unfiltered):", recievedDataPtr, m_data.size());

				log << tcu::TestLog::Message << str.str() << tcu::TestLog::EndMessage;

				return result;
			}
		}
	}

	return std::string();
}

} // anonymous (utilities)

// InterfaceBlockCase.

InterfaceBlockCase::InterfaceBlockCase (tcu::TestContext&	testCtx,
										const std::string&	name,
										const std::string&	description,
										MatrixLoadFlags		matrixLoadFlag,
										TestStageFlags		testStageFlags,
										bool				shuffleInterfaceMembers)
	: TestCase					(testCtx, name, description)
	, m_matrixLoadFlag			(matrixLoadFlag)
	, m_testStageFlags			(testStageFlags)
	, m_shuffleInterfaceMembers	(shuffleInterfaceMembers)
	, m_locationsRequired		(0)
{
}

InterfaceBlockCase::~InterfaceBlockCase (void)
{
}

void InterfaceBlockCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_vertShaderSource.empty());

	programCollection.glslSources.add("vert") << glu::VertexSource(m_vertShaderSource);

	if (!m_geomShaderSource.empty())
		programCollection.glslSources.add("geom") << glu::GeometrySource(m_geomShaderSource);
}

TestInstance* InterfaceBlockCase::createInstance (Context& context) const
{
	return new InterfaceBlockCaseInstance(context, m_interfaceLayout, m_blockPointers, m_data, m_tfBufBindingOffsets, m_tfBufBindingSizes, m_locationsRequired, m_testStageFlags);
}

void InterfaceBlockCase::delayedInit (void)
{
	BufferGeneralMapping	xfbBufferSize;
	std::string				notSupportedComment;

	// Compute reference layout.
	computeXfbLayout(m_interfaceLayout, m_interface, xfbBufferSize, m_locationsRequired);

	// Assign storage for reference values.
	// m_data contains all xfb buffers starting with all interfaces of first xfb_buffer, then all interfaces of next xfb_buffer
	{
		BufferGeneralMapping	xfbBufferOffsets;
		int						totalSize			= 0;
		int						maxXfb				= 0;

		for (BufferGeneralMapping::const_iterator xfbBuffersIter = xfbBufferSize.begin(); xfbBuffersIter != xfbBufferSize.end(); xfbBuffersIter++)
		{
			xfbBufferOffsets[xfbBuffersIter->first] = totalSize;
			totalSize += xfbBuffersIter->second;
			maxXfb = std::max(maxXfb, xfbBuffersIter->first);
		}
		m_data.resize(totalSize);

		DE_ASSERT(de::inBounds(maxXfb, 0, 256)); // Not correlated with spec: just make sure vectors won't be huge

		m_tfBufBindingSizes.resize(maxXfb + 1);
		for (BufferGeneralMapping::const_iterator xfbBuffersIter = xfbBufferSize.begin(); xfbBuffersIter != xfbBufferSize.end(); xfbBuffersIter++)
			m_tfBufBindingSizes[xfbBuffersIter->first] = xfbBuffersIter->second;

		m_tfBufBindingOffsets.resize(maxXfb + 1);
		for (BufferGeneralMapping::const_iterator xfbBuffersIter = xfbBufferOffsets.begin(); xfbBuffersIter != xfbBufferOffsets.end(); xfbBuffersIter++)
			m_tfBufBindingOffsets[xfbBuffersIter->first] = xfbBuffersIter->second;

		// Pointers for each block.
		for (int blockNdx = 0; blockNdx < (int)m_interfaceLayout.blocks.size(); blockNdx++)
		{
			const int dataXfbBufferStartOffset	= xfbBufferOffsets[m_interfaceLayout.blocks[blockNdx].xfbBuffer];
			const int offset					= dataXfbBufferStartOffset + m_interfaceLayout.blocks[blockNdx].xfbOffset;

			m_blockPointers[blockNdx] = &m_data[0] + offset;
		}
	}

	// Generate values.
	generateValues(m_interfaceLayout, m_blockPointers, 1 /* seed */);

	// Overlap validation
	{
		std::vector<deUint8>	mask	= createMask(m_interfaceLayout, m_blockPointers, &m_data[0], m_data.size());

		for (size_t maskNdx = 0; maskNdx < mask.size(); ++maskNdx)
			DE_ASSERT(mask[maskNdx] <= 1);
	}

	if (m_testStageFlags == TEST_STAGE_VERTEX)
	{
		m_vertShaderSource = generateTestShader(m_interface, m_interfaceLayout, m_blockPointers, m_matrixLoadFlag, m_testStageFlags, m_shuffleInterfaceMembers);
		m_geomShaderSource = "";
	}
	else if (m_testStageFlags == TEST_STAGE_GEOMETRY)
	{
		m_vertShaderSource = generatePassthroughShader();
		m_geomShaderSource = generateTestShader(m_interface, m_interfaceLayout, m_blockPointers, m_matrixLoadFlag, m_testStageFlags, m_shuffleInterfaceMembers);
	}
	else
	{
		DE_ASSERT(false && "Unknown test stage specified");
	}
}

} // TransformFeedback
} // vkt
