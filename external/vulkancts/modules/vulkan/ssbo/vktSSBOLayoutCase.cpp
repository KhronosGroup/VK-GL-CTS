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
 * \brief SSBO layout case.
 *//*--------------------------------------------------------------------*/

#include "tcuFloat.hpp"
#include "deInt32.h"
#include "deMath.h"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluVarType.hpp"
#include "gluVarTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktSSBOLayoutCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "util/vktTypeComparisonUtil.hpp"

namespace vkt
{
namespace ssbo
{

using glu::StructMember;
using glu::StructType;
using glu::VarType;
using std::string;
using std::vector;
using tcu::TestLog;

struct LayoutFlagsFmt
{
    uint32_t flags;
    LayoutFlagsFmt(uint32_t flags_) : flags(flags_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const LayoutFlagsFmt &fmt)
{
    static const struct
    {
        uint32_t bit;
        const char *token;
    } bitDesc[] = {{LAYOUT_STD140, "std140"},
                   {LAYOUT_STD430, "std430"},
                   {LAYOUT_SCALAR, "scalar"},
                   {LAYOUT_ROW_MAJOR, "row_major"},
                   {LAYOUT_COLUMN_MAJOR, "column_major"}};

    uint32_t remBits = fmt.flags;
    for (int descNdx = 0; descNdx < DE_LENGTH_OF_ARRAY(bitDesc); descNdx++)
    {
        if (remBits & bitDesc[descNdx].bit)
        {
            if (remBits != fmt.flags)
                str << ", ";
            str << bitDesc[descNdx].token;
            remBits &= ~bitDesc[descNdx].bit;
        }
    }
    DE_ASSERT(remBits == 0);
    return str;
}

// BufferVar implementation.

BufferVar::BufferVar(const char *name, const VarType &type, uint32_t flags)
    : m_name(name)
    , m_type(type)
    , m_flags(flags)
    , m_offset(~0u)
{
}

// BufferBlock implementation.

BufferBlock::BufferBlock(const char *blockName) : m_blockName(blockName), m_arraySize(-1), m_flags(0)
{
    setArraySize(0);
}

void BufferBlock::setArraySize(int arraySize)
{
    DE_ASSERT(arraySize >= 0);
    m_lastUnsizedArraySizes.resize(arraySize == 0 ? 1 : arraySize, 0);
    m_arraySize = arraySize;
}

std::ostream &operator<<(std::ostream &stream, const BlockLayoutEntry &entry)
{
    stream << entry.name << " { name = " << entry.name << ", size = " << entry.size << ", activeVarIndices = [";

    for (vector<int>::const_iterator i = entry.activeVarIndices.begin(); i != entry.activeVarIndices.end(); i++)
    {
        if (i != entry.activeVarIndices.begin())
            stream << ", ";
        stream << *i;
    }

    stream << "] }";
    return stream;
}

static bool isUnsizedArray(const BufferVarLayoutEntry &entry)
{
    DE_ASSERT(entry.arraySize != 0 || entry.topLevelArraySize != 0);
    return entry.arraySize == 0 || entry.topLevelArraySize == 0;
}

std::ostream &operator<<(std::ostream &stream, const BufferVarLayoutEntry &entry)
{
    stream << entry.name << " { type = " << glu::getDataTypeName(entry.type) << ", blockNdx = " << entry.blockNdx
           << ", offset = " << entry.offset << ", arraySize = " << entry.arraySize
           << ", arrayStride = " << entry.arrayStride << ", matrixStride = " << entry.matrixStride
           << ", topLevelArraySize = " << entry.topLevelArraySize
           << ", topLevelArrayStride = " << entry.topLevelArrayStride
           << ", isRowMajor = " << (entry.isRowMajor ? "true" : "false") << " }";
    return stream;
}

// \todo [2012-01-24 pyry] Speed up lookups using hash.

int BufferLayout::getVariableIndex(const string &name) const
{
    for (int ndx = 0; ndx < (int)bufferVars.size(); ndx++)
    {
        if (bufferVars[ndx].name == name)
            return ndx;
    }
    return -1;
}

int BufferLayout::getBlockIndex(const string &name) const
{
    for (int ndx = 0; ndx < (int)blocks.size(); ndx++)
    {
        if (blocks[ndx].name == name)
            return ndx;
    }
    return -1;
}

// ShaderInterface implementation.

ShaderInterface::ShaderInterface(void)
{
}

ShaderInterface::~ShaderInterface(void)
{
    for (std::vector<StructType *>::iterator i = m_structs.begin(); i != m_structs.end(); i++)
        delete *i;

    for (std::vector<BufferBlock *>::iterator i = m_bufferBlocks.begin(); i != m_bufferBlocks.end(); i++)
        delete *i;
}

StructType &ShaderInterface::allocStruct(const char *name)
{
    m_structs.reserve(m_structs.size() + 1);
    m_structs.push_back(new StructType(name));
    return *m_structs.back();
}

struct StructNameEquals
{
    std::string name;

    StructNameEquals(const char *name_) : name(name_)
    {
    }

    bool operator()(const StructType *type) const
    {
        return type->getTypeName() && name == type->getTypeName();
    }
};

const StructType *ShaderInterface::findStruct(const char *name) const
{
    std::vector<StructType *>::const_iterator pos =
        std::find_if(m_structs.begin(), m_structs.end(), StructNameEquals(name));
    return pos != m_structs.end() ? *pos : nullptr;
}

void ShaderInterface::getNamedStructs(std::vector<const StructType *> &structs) const
{
    for (std::vector<StructType *>::const_iterator i = m_structs.begin(); i != m_structs.end(); i++)
    {
        if ((*i)->getTypeName() != nullptr)
            structs.push_back(*i);
    }
}

BufferBlock &ShaderInterface::allocBlock(const char *name)
{
    m_bufferBlocks.reserve(m_bufferBlocks.size() + 1);
    m_bufferBlocks.push_back(new BufferBlock(name));
    return *m_bufferBlocks.back();
}

namespace // Utilities
{
// Layout computation.

int getDataTypeByteSize(glu::DataType type)
{
    if (deInRange32(type, glu::TYPE_UINT8, glu::TYPE_UINT8_VEC4) ||
        deInRange32(type, glu::TYPE_INT8, glu::TYPE_INT8_VEC4))
    {
        return glu::getDataTypeScalarSize(type) * (int)sizeof(uint8_t);
    }
    else if (deInRange32(type, glu::TYPE_UINT16, glu::TYPE_UINT16_VEC4) ||
             deInRange32(type, glu::TYPE_INT16, glu::TYPE_INT16_VEC4) ||
             deInRange32(type, glu::TYPE_FLOAT16, glu::TYPE_FLOAT16_VEC4))
    {
        return glu::getDataTypeScalarSize(type) * (int)sizeof(uint16_t);
    }
    else
    {
        return glu::getDataTypeScalarSize(type) * (int)sizeof(uint32_t);
    }
}

int getDataTypeByteAlignment(glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_FLOAT:
    case glu::TYPE_INT:
    case glu::TYPE_UINT:
    case glu::TYPE_BOOL:
        return 1 * (int)sizeof(uint32_t);

    case glu::TYPE_FLOAT_VEC2:
    case glu::TYPE_INT_VEC2:
    case glu::TYPE_UINT_VEC2:
    case glu::TYPE_BOOL_VEC2:
        return 2 * (int)sizeof(uint32_t);

    case glu::TYPE_FLOAT_VEC3:
    case glu::TYPE_INT_VEC3:
    case glu::TYPE_UINT_VEC3:
    case glu::TYPE_BOOL_VEC3: // Fall-through to vec4

    case glu::TYPE_FLOAT_VEC4:
    case glu::TYPE_INT_VEC4:
    case glu::TYPE_UINT_VEC4:
    case glu::TYPE_BOOL_VEC4:
        return 4 * (int)sizeof(uint32_t);

    case glu::TYPE_UINT8:
    case glu::TYPE_INT8:
        return 1 * (int)sizeof(uint8_t);

    case glu::TYPE_UINT8_VEC2:
    case glu::TYPE_INT8_VEC2:
        return 2 * (int)sizeof(uint8_t);

    case glu::TYPE_UINT8_VEC3:
    case glu::TYPE_INT8_VEC3: // Fall-through to vec4

    case glu::TYPE_UINT8_VEC4:
    case glu::TYPE_INT8_VEC4:
        return 4 * (int)sizeof(uint8_t);

    case glu::TYPE_UINT16:
    case glu::TYPE_INT16:
    case glu::TYPE_FLOAT16:
        return 1 * (int)sizeof(uint16_t);

    case glu::TYPE_UINT16_VEC2:
    case glu::TYPE_INT16_VEC2:
    case glu::TYPE_FLOAT16_VEC2:
        return 2 * (int)sizeof(uint16_t);

    case glu::TYPE_UINT16_VEC3:
    case glu::TYPE_INT16_VEC3:
    case glu::TYPE_FLOAT16_VEC3: // Fall-through to vec4

    case glu::TYPE_UINT16_VEC4:
    case glu::TYPE_INT16_VEC4:
    case glu::TYPE_FLOAT16_VEC4:
        return 4 * (int)sizeof(uint16_t);

    default:
        DE_ASSERT(false);
        return 0;
    }
}

int computeStd140BaseAlignment(const VarType &type, uint32_t layoutFlags)
{
    const int vec4Alignment = (int)sizeof(uint32_t) * 4;

    if (type.isBasicType())
    {
        glu::DataType basicType = type.getBasicType();

        if (glu::isDataTypeMatrix(basicType))
        {
            const bool isRowMajor = !!(layoutFlags & LAYOUT_ROW_MAJOR);
            const int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);
            const int vecAlign = deAlign32(getDataTypeByteAlignment(glu::getDataTypeFloatVec(vecSize)), vec4Alignment);

            return vecAlign;
        }
        else
            return getDataTypeByteAlignment(basicType);
    }
    else if (type.isArrayType())
    {
        int elemAlignment = computeStd140BaseAlignment(type.getElementType(), layoutFlags);

        // Round up to alignment of vec4
        return deAlign32(elemAlignment, vec4Alignment);
    }
    else
    {
        DE_ASSERT(type.isStructType());

        int maxBaseAlignment = 0;

        for (StructType::ConstIterator memberIter = type.getStructPtr()->begin();
             memberIter != type.getStructPtr()->end(); memberIter++)
            maxBaseAlignment =
                de::max(maxBaseAlignment, computeStd140BaseAlignment(memberIter->getType(), layoutFlags));

        return deAlign32(maxBaseAlignment, vec4Alignment);
    }
}

int computeStd430BaseAlignment(const VarType &type, uint32_t layoutFlags)
{
    // Otherwise identical to std140 except that alignment of structures and arrays
    // are not rounded up to alignment of vec4.

    if (type.isBasicType())
    {
        glu::DataType basicType = type.getBasicType();

        if (glu::isDataTypeMatrix(basicType))
        {
            const bool isRowMajor = !!(layoutFlags & LAYOUT_ROW_MAJOR);
            const int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);
            const int vecAlign = getDataTypeByteAlignment(glu::getDataTypeFloatVec(vecSize));
            return vecAlign;
        }
        else
            return getDataTypeByteAlignment(basicType);
    }
    else if (type.isArrayType())
    {
        return computeStd430BaseAlignment(type.getElementType(), layoutFlags);
    }
    else
    {
        DE_ASSERT(type.isStructType());

        int maxBaseAlignment = 0;

        for (StructType::ConstIterator memberIter = type.getStructPtr()->begin();
             memberIter != type.getStructPtr()->end(); memberIter++)
            maxBaseAlignment =
                de::max(maxBaseAlignment, computeStd430BaseAlignment(memberIter->getType(), layoutFlags));

        return maxBaseAlignment;
    }
}

int computeRelaxedBlockBaseAlignment(const VarType &type, uint32_t layoutFlags)
{
    if (type.isBasicType())
    {
        glu::DataType basicType = type.getBasicType();

        if (glu::isDataTypeVector(basicType))
            return getDataTypeByteAlignment(glu::getDataTypeScalarType(basicType));

        if (glu::isDataTypeMatrix(basicType))
        {
            const bool isRowMajor = !!(layoutFlags & LAYOUT_ROW_MAJOR);
            const int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);
            const int vecAlign = getDataTypeByteAlignment(glu::getDataTypeFloatVec(vecSize));
            return vecAlign;
        }
        else
            return getDataTypeByteAlignment(basicType);
    }
    else if (type.isArrayType())
        return computeStd430BaseAlignment(type.getElementType(), layoutFlags);
    else
    {
        DE_ASSERT(type.isStructType());

        int maxBaseAlignment = 0;
        for (StructType::ConstIterator memberIter = type.getStructPtr()->begin();
             memberIter != type.getStructPtr()->end(); memberIter++)
            maxBaseAlignment =
                de::max(maxBaseAlignment, computeRelaxedBlockBaseAlignment(memberIter->getType(), layoutFlags));

        return maxBaseAlignment;
    }
}

int computeScalarBlockAlignment(const VarType &type, uint32_t layoutFlags)
{
    if (type.isBasicType())
    {
        return getDataTypeByteAlignment(glu::getDataTypeScalarType(type.getBasicType()));
    }
    else if (type.isArrayType())
        return computeScalarBlockAlignment(type.getElementType(), layoutFlags);
    else
    {
        DE_ASSERT(type.isStructType());

        int maxBaseAlignment = 0;
        for (StructType::ConstIterator memberIter = type.getStructPtr()->begin();
             memberIter != type.getStructPtr()->end(); memberIter++)
            maxBaseAlignment =
                de::max(maxBaseAlignment, computeScalarBlockAlignment(memberIter->getType(), layoutFlags));

        return maxBaseAlignment;
    }
}

inline uint32_t mergeLayoutFlags(uint32_t prevFlags, uint32_t newFlags)
{
    const uint32_t packingMask = LAYOUT_STD430 | LAYOUT_STD140 | LAYOUT_RELAXED | LAYOUT_SCALAR;
    const uint32_t matrixMask  = LAYOUT_ROW_MAJOR | LAYOUT_COLUMN_MAJOR;

    uint32_t mergedFlags = 0;

    mergedFlags |= ((newFlags & packingMask) ? newFlags : prevFlags) & packingMask;
    mergedFlags |= ((newFlags & matrixMask) ? newFlags : prevFlags) & matrixMask;

    return mergedFlags;
}

//! Appends all child elements to layout, returns value that should be appended to offset.
int computeReferenceLayout(BufferLayout &layout, int curBlockNdx, int baseOffset, const std::string &curPrefix,
                           const VarType &type, uint32_t layoutFlags)
{
    // Reference layout uses std430 rules by default. std140 rules are
    // choosen only for blocks that have std140 layout.
    const int baseAlignment     = (layoutFlags & LAYOUT_SCALAR) != 0 ? computeScalarBlockAlignment(type, layoutFlags) :
                                  (layoutFlags & LAYOUT_STD140) != 0 ? computeStd140BaseAlignment(type, layoutFlags) :
                                  (layoutFlags & LAYOUT_RELAXED) != 0 ?
                                                                       computeRelaxedBlockBaseAlignment(type, layoutFlags) :
                                                                       computeStd430BaseAlignment(type, layoutFlags);
    int curOffset               = deAlign32(baseOffset, baseAlignment);
    const int topLevelArraySize = 1; // Default values
    const int topLevelArrayStride = 0;

    if (type.isBasicType())
    {
        const glu::DataType basicType = type.getBasicType();
        BufferVarLayoutEntry entry;

        entry.name                = curPrefix;
        entry.type                = basicType;
        entry.arraySize           = 1;
        entry.arrayStride         = 0;
        entry.matrixStride        = 0;
        entry.topLevelArraySize   = topLevelArraySize;
        entry.topLevelArrayStride = topLevelArrayStride;
        entry.blockNdx            = curBlockNdx;

        if (glu::isDataTypeMatrix(basicType))
        {
            // Array of vectors as specified in rules 5 & 7.
            const bool isRowMajor = !!(layoutFlags & LAYOUT_ROW_MAJOR);
            const int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);
            const glu::DataType vecType = glu::getDataTypeFloatVec(vecSize);
            const int numVecs =
                isRowMajor ? glu::getDataTypeMatrixNumRows(basicType) : glu::getDataTypeMatrixNumColumns(basicType);
            const int vecStride = (layoutFlags & LAYOUT_SCALAR) ? getDataTypeByteSize(vecType) : baseAlignment;

            entry.offset       = curOffset;
            entry.matrixStride = vecStride;
            entry.isRowMajor   = isRowMajor;

            curOffset += numVecs * entry.matrixStride;
        }
        else
        {
            if (!(layoutFlags & LAYOUT_SCALAR) && (layoutFlags & LAYOUT_RELAXED) && glu::isDataTypeVector(basicType) &&
                (getDataTypeByteSize(basicType) <= 16 ?
                     curOffset / 16 != (curOffset + getDataTypeByteSize(basicType) - 1) / 16 :
                     curOffset % 16 != 0))
                curOffset = deIntRoundToPow2(curOffset, 16);

            // Scalar or vector.
            entry.offset = curOffset;

            curOffset += getDataTypeByteSize(basicType);
        }

        layout.bufferVars.push_back(entry);
    }
    else if (type.isArrayType())
    {
        const VarType &elemType = type.getElementType();

        if (elemType.isBasicType() && !glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of scalars or vectors.
            const glu::DataType elemBasicType = elemType.getBasicType();
            const int stride = (layoutFlags & LAYOUT_SCALAR) ? getDataTypeByteSize(elemBasicType) : baseAlignment;
            BufferVarLayoutEntry entry;

            entry.name                = curPrefix + "[0]"; // Array variables are always postfixed with [0]
            entry.type                = elemBasicType;
            entry.blockNdx            = curBlockNdx;
            entry.offset              = curOffset;
            entry.arraySize           = type.getArraySize();
            entry.arrayStride         = stride;
            entry.matrixStride        = 0;
            entry.topLevelArraySize   = topLevelArraySize;
            entry.topLevelArrayStride = topLevelArrayStride;

            curOffset += stride * type.getArraySize();

            layout.bufferVars.push_back(entry);
        }
        else if (elemType.isBasicType() && glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of matrices.
            const glu::DataType elemBasicType = elemType.getBasicType();
            const bool isRowMajor             = !!(layoutFlags & LAYOUT_ROW_MAJOR);
            const int vecSize                 = isRowMajor ? glu::getDataTypeMatrixNumColumns(elemBasicType) :
                                                             glu::getDataTypeMatrixNumRows(elemBasicType);
            const glu::DataType vecType       = glu::getDataTypeFloatVec(vecSize);
            const int numVecs                 = isRowMajor ? glu::getDataTypeMatrixNumRows(elemBasicType) :
                                                             glu::getDataTypeMatrixNumColumns(elemBasicType);
            const int vecStride = (layoutFlags & LAYOUT_SCALAR) ? getDataTypeByteSize(vecType) : baseAlignment;
            BufferVarLayoutEntry entry;

            entry.name                = curPrefix + "[0]"; // Array variables are always postfixed with [0]
            entry.type                = elemBasicType;
            entry.blockNdx            = curBlockNdx;
            entry.offset              = curOffset;
            entry.arraySize           = type.getArraySize();
            entry.arrayStride         = vecStride * numVecs;
            entry.matrixStride        = vecStride;
            entry.isRowMajor          = isRowMajor;
            entry.topLevelArraySize   = topLevelArraySize;
            entry.topLevelArrayStride = topLevelArrayStride;

            curOffset += entry.arrayStride * type.getArraySize();

            layout.bufferVars.push_back(entry);
        }
        else
        {
            DE_ASSERT(elemType.isStructType() || elemType.isArrayType());

            for (int elemNdx = 0; elemNdx < type.getArraySize(); elemNdx++)
                curOffset += computeReferenceLayout(layout, curBlockNdx, curOffset,
                                                    curPrefix + "[" + de::toString(elemNdx) + "]",
                                                    type.getElementType(), layoutFlags);
        }
    }
    else
    {
        DE_ASSERT(type.isStructType());

        for (StructType::ConstIterator memberIter = type.getStructPtr()->begin();
             memberIter != type.getStructPtr()->end(); memberIter++)
            curOffset += computeReferenceLayout(layout, curBlockNdx, curOffset, curPrefix + "." + memberIter->getName(),
                                                memberIter->getType(), layoutFlags);

        if (!(layoutFlags & LAYOUT_SCALAR))
            curOffset = deAlign32(curOffset, baseAlignment);
    }

    return curOffset - baseOffset;
}

//! Appends all child elements to layout, returns offset increment.
int computeReferenceLayout(BufferLayout &layout, int curBlockNdx, const std::string &blockPrefix, int baseOffset,
                           const BufferVar &bufVar, uint32_t blockLayoutFlags)
{
    const VarType &varType       = bufVar.getType();
    const uint32_t combinedFlags = mergeLayoutFlags(blockLayoutFlags, bufVar.getFlags());

    if (varType.isArrayType())
    {
        // Top-level arrays need special care.
        const int topLevelArraySize = varType.getArraySize() == VarType::UNSIZED_ARRAY ? 0 : varType.getArraySize();
        const string prefix         = blockPrefix + bufVar.getName() + "[0]";
        const bool isStd140         = (blockLayoutFlags & LAYOUT_STD140) != 0;
        const int vec4Align         = (int)sizeof(uint32_t) * 4;
        const int baseAlignment =
            (blockLayoutFlags & LAYOUT_SCALAR) != 0  ? computeScalarBlockAlignment(varType, combinedFlags) :
            isStd140                                 ? computeStd140BaseAlignment(varType, combinedFlags) :
            (blockLayoutFlags & LAYOUT_RELAXED) != 0 ? computeRelaxedBlockBaseAlignment(varType, combinedFlags) :
                                                       computeStd430BaseAlignment(varType, combinedFlags);
        int curOffset           = deAlign32(baseOffset, baseAlignment);
        const VarType &elemType = varType.getElementType();

        if (elemType.isBasicType() && !glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of scalars or vectors.
            const glu::DataType elemBasicType = elemType.getBasicType();
            const int elemBaseAlign           = getDataTypeByteAlignment(elemBasicType);
            const int stride = (blockLayoutFlags & LAYOUT_SCALAR) ? getDataTypeByteSize(elemBasicType) :
                               isStd140                           ? deAlign32(elemBaseAlign, vec4Align) :
                                                                    elemBaseAlign;

            BufferVarLayoutEntry entry;

            entry.name                = prefix;
            entry.topLevelArraySize   = 1;
            entry.topLevelArrayStride = 0;
            entry.type                = elemBasicType;
            entry.blockNdx            = curBlockNdx;
            entry.offset              = curOffset;
            entry.arraySize           = topLevelArraySize;
            entry.arrayStride         = stride;
            entry.matrixStride        = 0;

            layout.bufferVars.push_back(entry);

            curOffset += stride * topLevelArraySize;
        }
        else if (elemType.isBasicType() && glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of matrices.
            const glu::DataType elemBasicType = elemType.getBasicType();
            const bool isRowMajor             = !!(combinedFlags & LAYOUT_ROW_MAJOR);
            const int vecSize                 = isRowMajor ? glu::getDataTypeMatrixNumColumns(elemBasicType) :
                                                             glu::getDataTypeMatrixNumRows(elemBasicType);
            const int numVecs                 = isRowMajor ? glu::getDataTypeMatrixNumRows(elemBasicType) :
                                                             glu::getDataTypeMatrixNumColumns(elemBasicType);
            const glu::DataType vecType       = glu::getDataTypeFloatVec(vecSize);
            const int vecBaseAlign            = getDataTypeByteAlignment(vecType);
            const int stride                  = (blockLayoutFlags & LAYOUT_SCALAR) ? getDataTypeByteSize(vecType) :
                                                isStd140 ? deAlign32(vecBaseAlign, vec4Align) :
                                                           vecBaseAlign;

            BufferVarLayoutEntry entry;

            entry.name                = prefix;
            entry.topLevelArraySize   = 1;
            entry.topLevelArrayStride = 0;
            entry.type                = elemBasicType;
            entry.blockNdx            = curBlockNdx;
            entry.offset              = curOffset;
            entry.arraySize           = topLevelArraySize;
            entry.arrayStride         = stride * numVecs;
            entry.matrixStride        = stride;
            entry.isRowMajor          = isRowMajor;

            layout.bufferVars.push_back(entry);

            curOffset += entry.arrayStride * topLevelArraySize;
        }
        else
        {
            DE_ASSERT(elemType.isStructType() || elemType.isArrayType());

            // Struct base alignment is not added multiple times as curOffset supplied to computeReferenceLayout
            // was already aligned correctly. Thus computeReferenceLayout should not add any extra padding
            // before struct. Padding after struct will be added as it should.
            //
            // Stride could be computed prior to creating child elements, but it would essentially require running
            // the layout computation twice. Instead we fix stride to child elements afterwards.

            const int firstChildNdx = (int)layout.bufferVars.size();

            const int size   = computeReferenceLayout(layout, curBlockNdx, deAlign32(curOffset, baseAlignment), prefix,
                                                      varType.getElementType(), combinedFlags);
            const int stride = deAlign32(size, baseAlignment);

            for (int childNdx = firstChildNdx; childNdx < (int)layout.bufferVars.size(); childNdx++)
            {
                layout.bufferVars[childNdx].topLevelArraySize   = topLevelArraySize;
                layout.bufferVars[childNdx].topLevelArrayStride = stride;
            }

            if (topLevelArraySize != 0)
                curOffset += stride * (topLevelArraySize - 1) + size;
        }

        return curOffset - baseOffset;
    }
    else
        return computeReferenceLayout(layout, curBlockNdx, baseOffset, blockPrefix + bufVar.getName(), varType,
                                      combinedFlags);
}

void computeReferenceLayout(BufferLayout &layout, ShaderInterface &interface)
{
    int numBlocks = interface.getNumBlocks();

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        BufferBlock &block      = interface.getBlock(blockNdx);
        bool hasInstanceName    = block.getInstanceName() != nullptr;
        std::string blockPrefix = hasInstanceName ? (std::string(block.getBlockName()) + ".") : std::string("");
        int curOffset           = 0;
        int activeBlockNdx      = (int)layout.blocks.size();
        int firstVarNdx         = (int)layout.bufferVars.size();

        size_t oldSize = layout.bufferVars.size();
        for (BufferBlock::iterator varIter = block.begin(); varIter != block.end(); varIter++)
        {
            BufferVar &bufVar = *varIter;
            curOffset +=
                computeReferenceLayout(layout, activeBlockNdx, blockPrefix, curOffset, bufVar, block.getFlags());
            if (block.getFlags() & LAYOUT_RELAXED)
            {
                DE_ASSERT(!(layout.bufferVars.size() <= oldSize));
                bufVar.setOffset(layout.bufferVars[oldSize].offset);
            }
            oldSize = layout.bufferVars.size();
        }

        int varIndicesEnd = (int)layout.bufferVars.size();
        int blockSize     = curOffset;
        int numInstances  = block.isArray() ? block.getArraySize() : 1;

        // Create block layout entries for each instance.
        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            // Allocate entry for instance.
            layout.blocks.push_back(BlockLayoutEntry());
            BlockLayoutEntry &blockEntry = layout.blocks.back();

            blockEntry.name = block.getBlockName();
            blockEntry.size = blockSize;

            // Compute active variable set for block.
            for (int varNdx = firstVarNdx; varNdx < varIndicesEnd; varNdx++)
                blockEntry.activeVarIndices.push_back(varNdx);

            if (block.isArray())
                blockEntry.name += "[" + de::toString(instanceNdx) + "]";
        }
    }
}

// Value generator.

void generateValue(const BufferVarLayoutEntry &entry, int unsizedArraySize, void *basePtr, de::Random &rnd)
{
    const glu::DataType scalarType = glu::getDataTypeScalarType(entry.type);
    const int scalarSize           = glu::getDataTypeScalarSize(entry.type);
    const int arraySize            = entry.arraySize == 0 ? unsizedArraySize : entry.arraySize;
    const int arrayStride          = entry.arrayStride;
    const int topLevelSize         = entry.topLevelArraySize == 0 ? unsizedArraySize : entry.topLevelArraySize;
    const int topLevelStride       = entry.topLevelArrayStride;
    const bool isMatrix            = glu::isDataTypeMatrix(entry.type);
    const int numVecs              = isMatrix ? (entry.isRowMajor ? glu::getDataTypeMatrixNumRows(entry.type) :
                                                                    glu::getDataTypeMatrixNumColumns(entry.type)) :
                                                1;
    const int vecSize              = scalarSize / numVecs;
    const size_t compSize          = getDataTypeByteSize(scalarType);

    DE_ASSERT(scalarSize % numVecs == 0);
    DE_ASSERT(topLevelSize >= 0);
    DE_ASSERT(arraySize >= 0);

    for (int topElemNdx = 0; topElemNdx < topLevelSize; topElemNdx++)
    {
        uint8_t *const topElemPtr = (uint8_t *)basePtr + entry.offset + topElemNdx * topLevelStride;

        for (int elemNdx = 0; elemNdx < arraySize; elemNdx++)
        {
            uint8_t *const elemPtr = topElemPtr + elemNdx * arrayStride;

            for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
            {
                uint8_t *const vecPtr = elemPtr + (isMatrix ? vecNdx * entry.matrixStride : 0);

                for (int compNdx = 0; compNdx < vecSize; compNdx++)
                {
                    uint8_t *const compPtr = vecPtr + compSize * compNdx;

                    switch (scalarType)
                    {
                    case glu::TYPE_FLOAT:
                        *((float *)compPtr) = (float)rnd.getInt(-9, 9);
                        break;
                    case glu::TYPE_INT:
                        *((int *)compPtr) = rnd.getInt(-9, 9);
                        break;
                    case glu::TYPE_UINT:
                        *((uint32_t *)compPtr) = (uint32_t)rnd.getInt(0, 9);
                        break;
                    case glu::TYPE_INT8:
                        *((int8_t *)compPtr) = (int8_t)rnd.getInt(-9, 9);
                        break;
                    case glu::TYPE_UINT8:
                        *((uint8_t *)compPtr) = (uint8_t)rnd.getInt(0, 9);
                        break;
                    case glu::TYPE_INT16:
                        *((int16_t *)compPtr) = (int16_t)rnd.getInt(-9, 9);
                        break;
                    case glu::TYPE_UINT16:
                        *((uint16_t *)compPtr) = (uint16_t)rnd.getInt(0, 9);
                        break;
                    case glu::TYPE_FLOAT16:
                        *((tcu::float16_t *)compPtr) = tcu::Float16((float)rnd.getInt(-9, 9)).bits();
                        break;
                    // \note Random bit pattern is used for true values. Spec states that all non-zero values are
                    //       interpreted as true but some implementations fail this.
                    case glu::TYPE_BOOL:
                        *((uint32_t *)compPtr) = rnd.getBool() ? rnd.getUint32() | 1u : 0u;
                        break;
                    default:
                        DE_ASSERT(false);
                    }
                }
            }
        }
    }
}

void generateValues(const BufferLayout &layout, const vector<BlockDataPtr> &blockPointers, uint32_t seed)
{
    de::Random rnd(seed);
    const int numBlocks = (int)layout.blocks.size();

    DE_ASSERT(numBlocks == (int)blockPointers.size());

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        const BlockLayoutEntry &blockLayout = layout.blocks[blockNdx];
        const BlockDataPtr &blockPtr        = blockPointers[blockNdx];
        const int numEntries                = (int)layout.blocks[blockNdx].activeVarIndices.size();

        for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
        {
            const int varNdx                     = blockLayout.activeVarIndices[entryNdx];
            const BufferVarLayoutEntry &varEntry = layout.bufferVars[varNdx];

            generateValue(varEntry, blockPtr.lastUnsizedArraySize, blockPtr.ptr, rnd);
        }
    }
}

// Shader generator.

void collectUniqueBasicTypes(std::set<glu::DataType> &basicTypes, const BufferBlock &bufferBlock)
{
    for (BufferBlock::const_iterator iter = bufferBlock.begin(); iter != bufferBlock.end(); ++iter)
        vkt::typecomputil::collectUniqueBasicTypes(basicTypes, iter->getType());
}

void collectUniqueBasicTypes(std::set<glu::DataType> &basicTypes, const ShaderInterface &interface)
{
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
        collectUniqueBasicTypes(basicTypes, interface.getBlock(ndx));
}

void generateCompareFuncs(std::ostream &str, const ShaderInterface &interface)
{
    std::set<glu::DataType> types;
    std::set<glu::DataType> compareFuncs;

    // Collect unique basic types
    collectUniqueBasicTypes(types, interface);

    // Set of compare functions required
    for (std::set<glu::DataType>::const_iterator iter = types.begin(); iter != types.end(); ++iter)
    {
        vkt::typecomputil::getCompareDependencies(compareFuncs, *iter);
    }

    for (int type = 0; type < glu::TYPE_LAST; ++type)
    {
        if (compareFuncs.find(glu::DataType(type)) != compareFuncs.end())
            str << vkt::typecomputil::getCompareFuncForType(glu::DataType(type));
    }
}

bool usesRelaxedLayout(const ShaderInterface &interface)
{
    //If any of blocks has LAYOUT_RELAXED flag
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
    {
        if (interface.getBlock(ndx).getFlags() & LAYOUT_RELAXED)
            return true;
    }
    return false;
}

bool uses16BitStorage(const ShaderInterface &interface)
{
    // If any of blocks has LAYOUT_16BIT_STORAGE flag
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
    {
        if (interface.getBlock(ndx).getFlags() & LAYOUT_16BIT_STORAGE)
            return true;
    }
    return false;
}

bool uses8BitStorage(const ShaderInterface &interface)
{
    // If any of blocks has LAYOUT_8BIT_STORAGE flag
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
    {
        if (interface.getBlock(ndx).getFlags() & LAYOUT_8BIT_STORAGE)
            return true;
    }
    return false;
}

bool usesScalarLayout(const ShaderInterface &interface)
{
    // If any of blocks has LAYOUT_SCALAR flag
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
    {
        if (interface.getBlock(ndx).getFlags() & LAYOUT_SCALAR)
            return true;
    }
    return false;
}

bool usesDescriptorIndexing(const ShaderInterface &interface)
{
    // If any of blocks has DESCRIPTOR_INDEXING flag
    for (int ndx = 0; ndx < interface.getNumBlocks(); ++ndx)
    {
        if (interface.getBlock(ndx).getFlags() & LAYOUT_DESCRIPTOR_INDEXING)
            return true;
    }
    return false;
}

struct Indent
{
    int level;
    Indent(int level_) : level(level_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const Indent &indent)
{
    for (int i = 0; i < indent.level; i++)
        str << "\t";
    return str;
}

void generateDeclaration(std::ostream &src, const BufferVar &bufferVar, int indentLevel)
{
    // \todo [pyry] Qualifiers
    if ((bufferVar.getFlags() & LAYOUT_MASK) != 0)
        src << "layout(" << LayoutFlagsFmt(bufferVar.getFlags() & LAYOUT_MASK) << ") ";
    else if (bufferVar.getOffset() != ~0u)
        src << "layout(offset = " << bufferVar.getOffset() << ") ";

    src << glu::declare(bufferVar.getType(), bufferVar.getName(), indentLevel);
}

void generateDeclaration(std::ostream &src, const BufferBlock &block, int bindingPoint, bool usePhysStorageBuffer)
{
    src << "layout(";
    if ((block.getFlags() & LAYOUT_MASK) != 0)
        src << LayoutFlagsFmt(block.getFlags() & LAYOUT_MASK) << ", ";

    if (usePhysStorageBuffer)
        src << "buffer_reference";
    else
        src << "binding = " << bindingPoint;

    src << ") ";

    bool readonly = true;
    for (BufferBlock::const_iterator varIter = block.begin(); varIter != block.end(); varIter++)
    {
        const BufferVar &bufVar = *varIter;
        if (bufVar.getFlags() & ACCESS_WRITE)
        {
            readonly = false;
            break;
        }
    }
    if (readonly)
        src << "readonly ";

    src << "buffer " << block.getBlockName();
    src << "\n{\n";

    for (BufferBlock::const_iterator varIter = block.begin(); varIter != block.end(); varIter++)
    {
        src << Indent(1);

        generateDeclaration(src, *varIter, 1 /* indent level */);
        src << ";\n";
    }

    src << "}";

    if (!usePhysStorageBuffer)
    {
        if (block.getInstanceName() != nullptr)
        {
            src << " " << block.getInstanceName();
            if (block.getFlags() & LAYOUT_DESCRIPTOR_INDEXING)
                src << "[]";
            else if (block.isArray())
                src << "[" << block.getArraySize() << "]";
        }
        else
            DE_ASSERT(!block.isArray());
    }

    src << ";\n";
}

void generateImmMatrixSrc(std::ostream &src, glu::DataType basicType, int matrixStride, bool isRowMajor, bool singleCol,
                          int colNumber, const void *valuePtr)
{
    DE_ASSERT(glu::isDataTypeMatrix(basicType));

    const int compSize = sizeof(uint32_t);
    const int numRows  = glu::getDataTypeMatrixNumRows(basicType);
    const int numCols  = glu::getDataTypeMatrixNumColumns(basicType);

    src << glu::getDataTypeName(singleCol ? glu::getDataTypeMatrixColumnType(basicType) : basicType) << "(";

    // Constructed in column-wise order.
    bool firstElem = true;
    for (int colNdx = 0; colNdx < numCols; colNdx++)
    {
        if (singleCol && colNdx != colNumber)
            continue;

        for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
        {
            const uint8_t *compPtr =
                (const uint8_t *)valuePtr +
                (isRowMajor ? rowNdx * matrixStride + colNdx * compSize : colNdx * matrixStride + rowNdx * compSize);

            if (!firstElem)
                src << ", ";

            src << de::floatToString(*((const float *)compPtr), 1);
            firstElem = false;
        }
    }

    src << ")";
}

void generateImmMatrixSrc(std::ostream &src, glu::DataType basicType, int matrixStride, bool isRowMajor,
                          const void *valuePtr, const char *resultVar, const char *typeName, const string shaderName)
{
    const int compSize = sizeof(uint32_t);
    const int numRows  = glu::getDataTypeMatrixNumRows(basicType);
    const int numCols  = glu::getDataTypeMatrixNumColumns(basicType);

    typeName = "float";
    for (int colNdex = 0; colNdex < numCols; colNdex++)
    {
        for (int rowNdex = 0; rowNdex < numRows; rowNdex++)
        {
            src << "\t" << resultVar << " = compare_" << typeName << "(" << shaderName << "[" << colNdex << "]["
                << rowNdex << "], ";
            const uint8_t *compPtr =
                (const uint8_t *)valuePtr + (isRowMajor ? rowNdex * matrixStride + colNdex * compSize :
                                                          colNdex * matrixStride + rowNdex * compSize);

            src << de::floatToString(*((const float *)compPtr), 1);
            src << ") && " << resultVar << ";\n";
        }
    }

    typeName = "vec";
    for (int colNdex = 0; colNdex < numCols; colNdex++)
    {
        src << "\t" << resultVar << " = compare_" << typeName << numRows << "(" << shaderName << "[" << colNdex << "], "
            << typeName << numRows << "(";
        for (int rowNdex = 0; rowNdex < numRows; rowNdex++)
        {
            const uint8_t *compPtr =
                (const uint8_t *)valuePtr + (isRowMajor ? (rowNdex * matrixStride + colNdex * compSize) :
                                                          (colNdex * matrixStride + rowNdex * compSize));
            src << de::floatToString(*((const float *)compPtr), 1);

            if (rowNdex < numRows - 1)
                src << ", ";
        }
        src << ")) && " << resultVar << ";\n";
    }
}

void generateImmScalarVectorSrc(std::ostream &src, glu::DataType basicType, const void *valuePtr)
{
    DE_ASSERT(glu::isDataTypeFloatOrVec(basicType) || glu::isDataTypeIntOrIVec(basicType) ||
              glu::isDataTypeUintOrUVec(basicType) || glu::isDataTypeBoolOrBVec(basicType) ||
              glu::isDataTypeExplicitPrecision(basicType));

    const glu::DataType scalarType = glu::getDataTypeScalarType(basicType);
    const int scalarSize           = glu::getDataTypeScalarSize(basicType);
    const size_t compSize          = getDataTypeByteSize(scalarType);

    if (scalarSize > 1)
        src << glu::getDataTypeName(vkt::typecomputil::getPromoteType(basicType)) << "(";

    for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
    {
        const uint8_t *compPtr = (const uint8_t *)valuePtr + scalarNdx * compSize;

        if (scalarNdx > 0)
            src << ", ";

        switch (scalarType)
        {
        case glu::TYPE_FLOAT16:
            src << de::floatToString(tcu::Float16(*((const tcu::float16_t *)compPtr)).asFloat(), 1);
            break;
        case glu::TYPE_FLOAT:
            src << de::floatToString(*((const float *)compPtr), 1);
            break;
        case glu::TYPE_INT8:
            src << (uint32_t) * ((const int8_t *)compPtr);
            break;
        case glu::TYPE_INT16:
            src << *((const int16_t *)compPtr);
            break;
        case glu::TYPE_INT:
            src << *((const int *)compPtr);
            break;
        case glu::TYPE_UINT8:
            src << (uint32_t) * ((const uint8_t *)compPtr) << "u";
            break;
        case glu::TYPE_UINT16:
            src << *((const uint16_t *)compPtr) << "u";
            break;
        case glu::TYPE_UINT:
            src << *((const uint32_t *)compPtr) << "u";
            break;
        case glu::TYPE_BOOL:
            src << (*((const uint32_t *)compPtr) != 0u ? "true" : "false");
            break;
        default:
            DE_ASSERT(false);
        }
    }

    if (scalarSize > 1)
        src << ")";
}

string getAPIName(const BufferBlock &block, const BufferVar &var, const glu::TypeComponentVector &accessPath)
{
    std::ostringstream name;

    if (block.getInstanceName())
        name << block.getBlockName() << ".";

    name << var.getName();

    for (glu::TypeComponentVector::const_iterator pathComp = accessPath.begin(); pathComp != accessPath.end();
         pathComp++)
    {
        if (pathComp->type == glu::VarTypeComponent::STRUCT_MEMBER)
        {
            const VarType curType       = glu::getVarType(var.getType(), accessPath.begin(), pathComp);
            const StructType *structPtr = curType.getStructPtr();

            name << "." << structPtr->getMember(pathComp->index).getName();
        }
        else if (pathComp->type == glu::VarTypeComponent::ARRAY_ELEMENT)
        {
            if (pathComp == accessPath.begin() || (pathComp + 1) == accessPath.end())
                name << "[0]"; // Top- / bottom-level array
            else
                name << "[" << pathComp->index << "]";
        }
        else
            DE_ASSERT(false);
    }

    return name.str();
}

string getShaderName(const BufferBlock &block, int instanceNdx, const BufferVar &var,
                     const glu::TypeComponentVector &accessPath)
{
    std::ostringstream name;

    if (block.getInstanceName())
    {
        name << block.getInstanceName();

        if (block.getFlags() & LAYOUT_DESCRIPTOR_INDEXING)
            name << "[nonuniformEXT(" << instanceNdx << ")]";
        else if (block.isArray())
            name << "[" << instanceNdx << "]";

        name << ".";
    }
    else
        DE_ASSERT(instanceNdx == 0);

    name << var.getName();

    for (glu::TypeComponentVector::const_iterator pathComp = accessPath.begin(); pathComp != accessPath.end();
         pathComp++)
    {
        if (pathComp->type == glu::VarTypeComponent::STRUCT_MEMBER)
        {
            const VarType curType       = glu::getVarType(var.getType(), accessPath.begin(), pathComp);
            const StructType *structPtr = curType.getStructPtr();

            name << "." << structPtr->getMember(pathComp->index).getName();
        }
        else if (pathComp->type == glu::VarTypeComponent::ARRAY_ELEMENT)
            name << "[" << pathComp->index << "]";
        else
            DE_ASSERT(false);
    }

    return name.str();
}

int computeOffset(const BufferVarLayoutEntry &varLayout, const glu::TypeComponentVector &accessPath)
{
    const int topLevelNdx = (accessPath.size() > 1 && accessPath.front().type == glu::VarTypeComponent::ARRAY_ELEMENT) ?
                                accessPath.front().index :
                                0;
    const int bottomLevelNdx = (!accessPath.empty() && accessPath.back().type == glu::VarTypeComponent::ARRAY_ELEMENT) ?
                                   accessPath.back().index :
                                   0;

    return varLayout.offset + varLayout.topLevelArrayStride * topLevelNdx + varLayout.arrayStride * bottomLevelNdx;
}

void generateCompareSrc(std::ostream &src, const char *resultVar, const BufferLayout &bufferLayout,
                        const BufferBlock &block, int instanceNdx, const BlockDataPtr &blockPtr,
                        const BufferVar &bufVar, const glu::SubTypeAccess &accessPath, MatrixLoadFlags matrixLoadFlag,
                        int &compareLimit)
{
    const VarType curType = accessPath.getType();

    // if limit for number of performed compare operations was reached then skip remaining compares
    if (compareLimit < 1)
        return;

    if (curType.isArrayType())
    {
        const int arraySize = curType.getArraySize() == VarType::UNSIZED_ARRAY ?
                                  block.getLastUnsizedArraySize(instanceNdx) :
                                  curType.getArraySize();

        for (int elemNdx = 0; elemNdx < arraySize; elemNdx++)
            generateCompareSrc(src, resultVar, bufferLayout, block, instanceNdx, blockPtr, bufVar,
                               accessPath.element(elemNdx), LOAD_FULL_MATRIX, compareLimit);
    }
    else if (curType.isStructType())
    {
        const int numMembers = curType.getStructPtr()->getNumMembers();

        for (int memberNdx = 0; memberNdx < numMembers; memberNdx++)
            generateCompareSrc(src, resultVar, bufferLayout, block, instanceNdx, blockPtr, bufVar,
                               accessPath.member(memberNdx), LOAD_FULL_MATRIX, compareLimit);
    }
    else
    {
        DE_ASSERT(curType.isBasicType());
        compareLimit--;

        const string apiName = getAPIName(block, bufVar, accessPath.getPath());
        const int varNdx     = bufferLayout.getVariableIndex(apiName);

        DE_ASSERT(varNdx >= 0);
        {
            const BufferVarLayoutEntry &varLayout = bufferLayout.bufferVars[varNdx];
            const string shaderName               = getShaderName(block, instanceNdx, bufVar, accessPath.getPath());
            const glu::DataType basicType         = curType.getBasicType();
            const bool isMatrix                   = glu::isDataTypeMatrix(basicType);
            const char *typeName                  = glu::getDataTypeName(basicType);
            const void *valuePtr = (const uint8_t *)blockPtr.ptr + computeOffset(varLayout, accessPath.getPath());

            if (isMatrix)
            {
                if (matrixLoadFlag == LOAD_MATRIX_COMPONENTS)
                    generateImmMatrixSrc(src, basicType, varLayout.matrixStride, varLayout.isRowMajor, valuePtr,
                                         resultVar, typeName, shaderName);
                else
                {
                    src << "\t" << resultVar << " = compare_" << typeName << "(" << shaderName << ", ";
                    generateImmMatrixSrc(src, basicType, varLayout.matrixStride, varLayout.isRowMajor, false, -1,
                                         valuePtr);
                    src << ") && " << resultVar << ";\n";
                }
            }
            else
            {
                const char *castName      = "";
                glu::DataType promoteType = vkt::typecomputil::getPromoteType(basicType);
                if (basicType != promoteType)
                    castName = glu::getDataTypeName(promoteType);

                src << "\t" << resultVar << " = compare_" << typeName << "(" << castName << "(" << shaderName << "), ";
                generateImmScalarVectorSrc(src, basicType, valuePtr);
                src << ") && " << resultVar << ";\n";
            }
        }
    }
}

void generateCompareSrc(std::ostream &src, const char *resultVar, const ShaderInterface &interface,
                        const BufferLayout &layout, const vector<BlockDataPtr> &blockPointers,
                        MatrixLoadFlags matrixLoadFlag)
{
    // limit number of performed compare operations; some generated tests execute
    // large number of compare operations that result in slow compile times which
    // in turn result in test skip on slower platforms
    int compareLimit = 130;

    for (int declNdx = 0; declNdx < interface.getNumBlocks(); declNdx++)
    {
        const BufferBlock &block = interface.getBlock(declNdx);
        const bool isArray       = block.isArray();
        const int numInstances   = isArray ? block.getArraySize() : 1;

        DE_ASSERT(!isArray || block.getInstanceName());

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            const string instanceName =
                block.getBlockName() + (isArray ? "[" + de::toString(instanceNdx) + "]" : string(""));
            const int blockNdx           = layout.getBlockIndex(instanceName);
            const BlockDataPtr &blockPtr = blockPointers[blockNdx];

            for (BufferBlock::const_iterator varIter = block.begin(); varIter != block.end(); varIter++)
            {
                const BufferVar &bufVar = *varIter;

                if ((bufVar.getFlags() & ACCESS_READ) == 0)
                    continue; // Don't read from that variable.

                generateCompareSrc(src, resultVar, layout, block, instanceNdx, blockPtr, bufVar,
                                   glu::SubTypeAccess(bufVar.getType()), matrixLoadFlag, compareLimit);
            }
        }
    }
}

// \todo [2013-10-14 pyry] Almost identical to generateCompareSrc - unify?

void generateWriteSrc(std::ostream &src, const BufferLayout &bufferLayout, const BufferBlock &block, int instanceNdx,
                      const BlockDataPtr &blockPtr, const BufferVar &bufVar, const glu::SubTypeAccess &accessPath,
                      MatrixStoreFlags matrixStoreFlag)
{
    const VarType curType = accessPath.getType();

    if (curType.isArrayType())
    {
        const int arraySize = curType.getArraySize() == VarType::UNSIZED_ARRAY ?
                                  block.getLastUnsizedArraySize(instanceNdx) :
                                  curType.getArraySize();

        for (int elemNdx = 0; elemNdx < arraySize; elemNdx++)
            generateWriteSrc(src, bufferLayout, block, instanceNdx, blockPtr, bufVar, accessPath.element(elemNdx),
                             matrixStoreFlag);
    }
    else if (curType.isStructType())
    {
        const int numMembers = curType.getStructPtr()->getNumMembers();

        for (int memberNdx = 0; memberNdx < numMembers; memberNdx++)
            generateWriteSrc(src, bufferLayout, block, instanceNdx, blockPtr, bufVar, accessPath.member(memberNdx),
                             matrixStoreFlag);
    }
    else
    {
        DE_ASSERT(curType.isBasicType());

        const string apiName = getAPIName(block, bufVar, accessPath.getPath());
        const int varNdx     = bufferLayout.getVariableIndex(apiName);

        DE_ASSERT(varNdx >= 0);
        {
            const BufferVarLayoutEntry &varLayout = bufferLayout.bufferVars[varNdx];
            const string shaderName               = getShaderName(block, instanceNdx, bufVar, accessPath.getPath());
            const glu::DataType basicType         = curType.getBasicType();
            const bool isMatrix                   = glu::isDataTypeMatrix(basicType);
            const void *valuePtr = (const uint8_t *)blockPtr.ptr + computeOffset(varLayout, accessPath.getPath());

            const char *castName      = "";
            glu::DataType promoteType = vkt::typecomputil::getPromoteType(basicType);
            if (basicType != promoteType)
                castName = glu::getDataTypeName((!isMatrix || matrixStoreFlag == STORE_FULL_MATRIX) ?
                                                    basicType :
                                                    glu::getDataTypeMatrixColumnType(basicType));

            if (isMatrix)
            {
                switch (matrixStoreFlag)
                {
                case STORE_FULL_MATRIX:
                {
                    src << "\t" << shaderName << " = " << castName << "(";
                    generateImmMatrixSrc(src, basicType, varLayout.matrixStride, varLayout.isRowMajor, false, -1,
                                         valuePtr);
                    src << ");\n";
                    break;
                }
                case STORE_MATRIX_COLUMNS:
                {
                    int numCols = glu::getDataTypeMatrixNumColumns(basicType);
                    for (int colIdx = 0; colIdx < numCols; ++colIdx)
                    {
                        src << "\t" << shaderName << "[" << colIdx << "]"
                            << " = " << castName << "(";
                        generateImmMatrixSrc(src, basicType, varLayout.matrixStride, varLayout.isRowMajor, true, colIdx,
                                             valuePtr);
                        src << ");\n";
                    }
                    break;
                }
                default:
                    DE_ASSERT(false);
                    break;
                }
            }
            else
            {
                src << "\t" << shaderName << " = " << castName << "(";
                generateImmScalarVectorSrc(src, basicType, valuePtr);
                src << ");\n";
            }
        }
    }
}

void generateWriteSrc(std::ostream &src, const ShaderInterface &interface, const BufferLayout &layout,
                      const vector<BlockDataPtr> &blockPointers, MatrixStoreFlags matrixStoreFlag)
{
    for (int declNdx = 0; declNdx < interface.getNumBlocks(); declNdx++)
    {
        const BufferBlock &block = interface.getBlock(declNdx);
        const bool isArray       = block.isArray();
        const int numInstances   = isArray ? block.getArraySize() : 1;

        DE_ASSERT(!isArray || block.getInstanceName());

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            const string instanceName =
                block.getBlockName() + (isArray ? "[" + de::toString(instanceNdx) + "]" : string(""));
            const int blockNdx           = layout.getBlockIndex(instanceName);
            const BlockDataPtr &blockPtr = blockPointers[blockNdx];

            for (BufferBlock::const_iterator varIter = block.begin(); varIter != block.end(); varIter++)
            {
                const BufferVar &bufVar = *varIter;

                if ((bufVar.getFlags() & ACCESS_WRITE) == 0)
                    continue; // Don't write to that variable.

                generateWriteSrc(src, layout, block, instanceNdx, blockPtr, bufVar,
                                 glu::SubTypeAccess(bufVar.getType()), matrixStoreFlag);
            }
        }
    }
}

string generateComputeShader(const ShaderInterface &interface, const BufferLayout &layout,
                             const vector<BlockDataPtr> &comparePtrs, const vector<BlockDataPtr> &writePtrs,
                             MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag,
                             bool usePhysStorageBuffer)
{
    std::ostringstream src;

    if (uses16BitStorage(interface) || uses8BitStorage(interface) || usesRelaxedLayout(interface) ||
        usesScalarLayout(interface) || usesDescriptorIndexing(interface))
    {
        src << "#version 450\n";
    }
    else
        src << "#version 310 es\n";

    src << "#extension GL_EXT_shader_16bit_storage : enable\n";
    src << "#extension GL_EXT_shader_8bit_storage : enable\n";
    src << "#extension GL_EXT_scalar_block_layout : enable\n";
    src << "#extension GL_EXT_buffer_reference : enable\n";
    src << "#extension GL_EXT_nonuniform_qualifier : enable\n";
    src << "layout(local_size_x = 1) in;\n";
    src << "\n";

    // Atomic counter for counting passed invocations.
    src << "layout(std140, binding = 0) buffer AcBlock { highp uint ac_numPassed; };\n\n";

    std::vector<const StructType *> namedStructs;
    interface.getNamedStructs(namedStructs);
    for (std::vector<const StructType *>::const_iterator structIter = namedStructs.begin();
         structIter != namedStructs.end(); structIter++)
        src << glu::declare(*structIter) << ";\n";

    {
        for (int blockNdx = 0; blockNdx < interface.getNumBlocks(); blockNdx++)
        {
            const BufferBlock &block = interface.getBlock(blockNdx);
            generateDeclaration(src, block, 1 + blockNdx, usePhysStorageBuffer);
        }

        if (usePhysStorageBuffer)
        {
            src << "layout (push_constant, std430) uniform PC {\n";
            for (int blockNdx = 0; blockNdx < interface.getNumBlocks(); blockNdx++)
            {
                const BufferBlock &block = interface.getBlock(blockNdx);
                if (block.getInstanceName() != nullptr)
                {
                    src << "    " << block.getBlockName() << " " << block.getInstanceName();
                    if (block.isArray())
                        src << "[" << block.getArraySize() << "]";
                    src << ";\n";
                }
            }
            src << "};\n";
        }
    }

    // Comparison utilities.
    src << "\n";
    generateCompareFuncs(src, interface);

    src << "\n"
           "void main (void)\n"
           "{\n"
           "    bool allOk = true;\n";

    // Value compare.
    generateCompareSrc(src, "allOk", interface, layout, comparePtrs, matrixLoadFlag);

    src << "    if (allOk)\n"
        << "        ac_numPassed++;\n"
        << "\n";

    // Value write.
    generateWriteSrc(src, interface, layout, writePtrs, matrixStoreFlag);

    src << "}\n";

    return src.str();
}

void copyBufferVarData(const BufferVarLayoutEntry &dstEntry, const BlockDataPtr &dstBlockPtr,
                       const BufferVarLayoutEntry &srcEntry, const BlockDataPtr &srcBlockPtr)
{
    DE_ASSERT(dstEntry.arraySize <= srcEntry.arraySize);
    DE_ASSERT(dstEntry.topLevelArraySize <= srcEntry.topLevelArraySize);
    DE_ASSERT(dstBlockPtr.lastUnsizedArraySize <= srcBlockPtr.lastUnsizedArraySize);
    DE_ASSERT(dstEntry.type == srcEntry.type);

    uint8_t *const dstBasePtr       = (uint8_t *)dstBlockPtr.ptr + dstEntry.offset;
    const uint8_t *const srcBasePtr = (const uint8_t *)srcBlockPtr.ptr + srcEntry.offset;
    const int scalarSize            = glu::getDataTypeScalarSize(dstEntry.type);
    const bool isMatrix             = glu::isDataTypeMatrix(dstEntry.type);
    glu::DataType scalarType        = glu::getDataTypeScalarType(dstEntry.type);
    const size_t compSize           = getDataTypeByteSize(scalarType);
    const int dstArraySize          = dstEntry.arraySize == 0 ? dstBlockPtr.lastUnsizedArraySize : dstEntry.arraySize;
    const int dstArrayStride        = dstEntry.arrayStride;
    const int dstTopLevelSize =
        dstEntry.topLevelArraySize == 0 ? dstBlockPtr.lastUnsizedArraySize : dstEntry.topLevelArraySize;
    const int dstTopLevelStride = dstEntry.topLevelArrayStride;
    const int srcArraySize      = srcEntry.arraySize == 0 ? srcBlockPtr.lastUnsizedArraySize : srcEntry.arraySize;
    const int srcArrayStride    = srcEntry.arrayStride;
    const int srcTopLevelSize =
        srcEntry.topLevelArraySize == 0 ? srcBlockPtr.lastUnsizedArraySize : srcEntry.topLevelArraySize;
    const int srcTopLevelStride = srcEntry.topLevelArrayStride;

    DE_ASSERT(dstArraySize <= srcArraySize && dstTopLevelSize <= srcTopLevelSize);
    DE_UNREF(srcArraySize && srcTopLevelSize);

    for (int topElemNdx = 0; topElemNdx < dstTopLevelSize; topElemNdx++)
    {
        uint8_t *const dstTopPtr       = dstBasePtr + topElemNdx * dstTopLevelStride;
        const uint8_t *const srcTopPtr = srcBasePtr + topElemNdx * srcTopLevelStride;

        for (int elementNdx = 0; elementNdx < dstArraySize; elementNdx++)
        {
            uint8_t *const dstElemPtr       = dstTopPtr + elementNdx * dstArrayStride;
            const uint8_t *const srcElemPtr = srcTopPtr + elementNdx * srcArrayStride;

            if (isMatrix)
            {
                const int numRows = glu::getDataTypeMatrixNumRows(dstEntry.type);
                const int numCols = glu::getDataTypeMatrixNumColumns(dstEntry.type);

                for (int colNdx = 0; colNdx < numCols; colNdx++)
                {
                    for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
                    {
                        uint8_t *dstCompPtr =
                            dstElemPtr + (dstEntry.isRowMajor ? rowNdx * dstEntry.matrixStride + colNdx * compSize :
                                                                colNdx * dstEntry.matrixStride + rowNdx * compSize);
                        const uint8_t *srcCompPtr =
                            srcElemPtr + (srcEntry.isRowMajor ? rowNdx * srcEntry.matrixStride + colNdx * compSize :
                                                                colNdx * srcEntry.matrixStride + rowNdx * compSize);

                        DE_ASSERT((intptr_t)(srcCompPtr + compSize) - (intptr_t)srcBlockPtr.ptr <=
                                  (intptr_t)srcBlockPtr.size);
                        DE_ASSERT((intptr_t)(dstCompPtr + compSize) - (intptr_t)dstBlockPtr.ptr <=
                                  (intptr_t)dstBlockPtr.size);
                        deMemcpy(dstCompPtr, srcCompPtr, compSize);
                    }
                }
            }
            else
            {
                DE_ASSERT((intptr_t)(srcElemPtr + scalarSize * compSize) - (intptr_t)srcBlockPtr.ptr <=
                          (intptr_t)srcBlockPtr.size);
                DE_ASSERT((intptr_t)(dstElemPtr + scalarSize * compSize) - (intptr_t)dstBlockPtr.ptr <=
                          (intptr_t)dstBlockPtr.size);
                deMemcpy(dstElemPtr, srcElemPtr, scalarSize * compSize);
            }
        }
    }
}

void copyData(const BufferLayout &dstLayout, const vector<BlockDataPtr> &dstBlockPointers,
              const BufferLayout &srcLayout, const vector<BlockDataPtr> &srcBlockPointers)
{
    // \note Src layout is used as reference in case of activeVarIndices happens to be incorrect in dstLayout blocks.
    int numBlocks = (int)srcLayout.blocks.size();

    for (int srcBlockNdx = 0; srcBlockNdx < numBlocks; srcBlockNdx++)
    {
        const BlockLayoutEntry &srcBlock = srcLayout.blocks[srcBlockNdx];
        const BlockDataPtr &srcBlockPtr  = srcBlockPointers[srcBlockNdx];
        int dstBlockNdx                  = dstLayout.getBlockIndex(srcBlock.name.c_str());

        if (dstBlockNdx >= 0)
        {
            DE_ASSERT(de::inBounds(dstBlockNdx, 0, (int)dstBlockPointers.size()));

            const BlockDataPtr &dstBlockPtr = dstBlockPointers[dstBlockNdx];

            for (vector<int>::const_iterator srcVarNdxIter = srcBlock.activeVarIndices.begin();
                 srcVarNdxIter != srcBlock.activeVarIndices.end(); srcVarNdxIter++)
            {
                const BufferVarLayoutEntry &srcEntry = srcLayout.bufferVars[*srcVarNdxIter];
                int dstVarNdx                        = dstLayout.getVariableIndex(srcEntry.name.c_str());

                if (dstVarNdx >= 0)
                    copyBufferVarData(dstLayout.bufferVars[dstVarNdx], dstBlockPtr, srcEntry, srcBlockPtr);
            }
        }
    }
}

void copyNonWrittenData(const BufferLayout &layout, const BufferBlock &block, int instanceNdx,
                        const BlockDataPtr &srcBlockPtr, const BlockDataPtr &dstBlockPtr, const BufferVar &bufVar,
                        const glu::SubTypeAccess &accessPath)
{
    const VarType curType = accessPath.getType();

    if (curType.isArrayType())
    {
        const int arraySize = curType.getArraySize() == VarType::UNSIZED_ARRAY ?
                                  block.getLastUnsizedArraySize(instanceNdx) :
                                  curType.getArraySize();

        for (int elemNdx = 0; elemNdx < arraySize; elemNdx++)
            copyNonWrittenData(layout, block, instanceNdx, srcBlockPtr, dstBlockPtr, bufVar,
                               accessPath.element(elemNdx));
    }
    else if (curType.isStructType())
    {
        const int numMembers = curType.getStructPtr()->getNumMembers();

        for (int memberNdx = 0; memberNdx < numMembers; memberNdx++)
            copyNonWrittenData(layout, block, instanceNdx, srcBlockPtr, dstBlockPtr, bufVar,
                               accessPath.member(memberNdx));
    }
    else
    {
        DE_ASSERT(curType.isBasicType());

        const string apiName = getAPIName(block, bufVar, accessPath.getPath());
        const int varNdx     = layout.getVariableIndex(apiName);

        DE_ASSERT(varNdx >= 0);
        {
            const BufferVarLayoutEntry &varLayout = layout.bufferVars[varNdx];
            copyBufferVarData(varLayout, dstBlockPtr, varLayout, srcBlockPtr);
        }
    }
}

void copyNonWrittenData(const ShaderInterface &interface, const BufferLayout &layout,
                        const vector<BlockDataPtr> &srcPtrs, const vector<BlockDataPtr> &dstPtrs)
{
    for (int declNdx = 0; declNdx < interface.getNumBlocks(); declNdx++)
    {
        const BufferBlock &block = interface.getBlock(declNdx);
        const bool isArray       = block.isArray();
        const int numInstances   = isArray ? block.getArraySize() : 1;

        DE_ASSERT(!isArray || block.getInstanceName());

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            const string instanceName =
                block.getBlockName() + (isArray ? "[" + de::toString(instanceNdx) + "]" : string(""));
            const int blockNdx              = layout.getBlockIndex(instanceName);
            const BlockDataPtr &srcBlockPtr = srcPtrs[blockNdx];
            const BlockDataPtr &dstBlockPtr = dstPtrs[blockNdx];

            for (BufferBlock::const_iterator varIter = block.begin(); varIter != block.end(); varIter++)
            {
                const BufferVar &bufVar = *varIter;

                if (bufVar.getFlags() & ACCESS_WRITE)
                    continue;

                copyNonWrittenData(layout, block, instanceNdx, srcBlockPtr, dstBlockPtr, bufVar,
                                   glu::SubTypeAccess(bufVar.getType()));
            }
        }
    }
}

bool compareComponents(glu::DataType scalarType, const void *ref, const void *res, int numComps)
{
    if (scalarType == glu::TYPE_FLOAT)
    {
        const float threshold = 0.05f; // Same as used in shaders - should be fine for values being used.

        for (int ndx = 0; ndx < numComps; ndx++)
        {
            const float refVal = *((const float *)ref + ndx);
            const float resVal = *((const float *)res + ndx);

            if (deFloatAbs(resVal - refVal) >= threshold)
                return false;
        }
    }
    else if (scalarType == glu::TYPE_BOOL)
    {
        for (int ndx = 0; ndx < numComps; ndx++)
        {
            const uint32_t refVal = *((const uint32_t *)ref + ndx);
            const uint32_t resVal = *((const uint32_t *)res + ndx);

            if ((refVal != 0) != (resVal != 0))
                return false;
        }
    }
    else if (scalarType == glu::TYPE_INT8 || scalarType == glu::TYPE_UINT8)
    {
        return deMemCmp(ref, res, numComps * sizeof(uint8_t)) == 0;
    }
    else if (scalarType == glu::TYPE_INT16 || scalarType == glu::TYPE_UINT16 || scalarType == glu::TYPE_FLOAT16)
    {
        return deMemCmp(ref, res, numComps * sizeof(uint16_t)) == 0;
    }
    else
    {
        DE_ASSERT(scalarType == glu::TYPE_INT || scalarType == glu::TYPE_UINT);

        return deMemCmp(ref, res, numComps * sizeof(uint32_t)) == 0;
    }

    return true;
}

bool compareBufferVarData(tcu::TestLog &log, const BufferVarLayoutEntry &refEntry, const BlockDataPtr &refBlockPtr,
                          const BufferVarLayoutEntry &resEntry, const BlockDataPtr &resBlockPtr)
{
    DE_ASSERT(resEntry.arraySize <= refEntry.arraySize);
    DE_ASSERT(resEntry.topLevelArraySize <= refEntry.topLevelArraySize);
    DE_ASSERT(resBlockPtr.lastUnsizedArraySize <= refBlockPtr.lastUnsizedArraySize);
    DE_ASSERT(resEntry.type == refEntry.type);

    uint8_t *const resBasePtr       = (uint8_t *)resBlockPtr.ptr + resEntry.offset;
    const uint8_t *const refBasePtr = (const uint8_t *)refBlockPtr.ptr + refEntry.offset;
    const glu::DataType scalarType  = glu::getDataTypeScalarType(refEntry.type);
    const int scalarSize            = glu::getDataTypeScalarSize(resEntry.type);
    const bool isMatrix             = glu::isDataTypeMatrix(resEntry.type);
    const size_t compSize           = getDataTypeByteSize(scalarType);
    const int maxPrints             = 3;
    int numFailed                   = 0;

    const int resArraySize   = resEntry.arraySize == 0 ? resBlockPtr.lastUnsizedArraySize : resEntry.arraySize;
    const int resArrayStride = resEntry.arrayStride;
    const int resTopLevelSize =
        resEntry.topLevelArraySize == 0 ? resBlockPtr.lastUnsizedArraySize : resEntry.topLevelArraySize;
    const int resTopLevelStride = resEntry.topLevelArrayStride;
    const int refArraySize      = refEntry.arraySize == 0 ? refBlockPtr.lastUnsizedArraySize : refEntry.arraySize;
    const int refArrayStride    = refEntry.arrayStride;
    const int refTopLevelSize =
        refEntry.topLevelArraySize == 0 ? refBlockPtr.lastUnsizedArraySize : refEntry.topLevelArraySize;
    const int refTopLevelStride = refEntry.topLevelArrayStride;

    DE_ASSERT(resArraySize <= refArraySize && resTopLevelSize <= refTopLevelSize);
    DE_UNREF(refArraySize && refTopLevelSize);

    for (int topElemNdx = 0; topElemNdx < resTopLevelSize; topElemNdx++)
    {
        uint8_t *const resTopPtr       = resBasePtr + topElemNdx * resTopLevelStride;
        const uint8_t *const refTopPtr = refBasePtr + topElemNdx * refTopLevelStride;

        for (int elementNdx = 0; elementNdx < resArraySize; elementNdx++)
        {
            uint8_t *const resElemPtr       = resTopPtr + elementNdx * resArrayStride;
            const uint8_t *const refElemPtr = refTopPtr + elementNdx * refArrayStride;

            if (isMatrix)
            {
                const int numRows = glu::getDataTypeMatrixNumRows(resEntry.type);
                const int numCols = glu::getDataTypeMatrixNumColumns(resEntry.type);
                bool isOk         = true;

                for (int colNdx = 0; colNdx < numCols; colNdx++)
                {
                    for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
                    {
                        uint8_t *resCompPtr =
                            resElemPtr + (resEntry.isRowMajor ? rowNdx * resEntry.matrixStride + colNdx * compSize :
                                                                colNdx * resEntry.matrixStride + rowNdx * compSize);
                        const uint8_t *refCompPtr =
                            refElemPtr + (refEntry.isRowMajor ? rowNdx * refEntry.matrixStride + colNdx * compSize :
                                                                colNdx * refEntry.matrixStride + rowNdx * compSize);

                        DE_ASSERT((intptr_t)(refCompPtr + compSize) - (intptr_t)refBlockPtr.ptr <=
                                  (intptr_t)refBlockPtr.size);
                        DE_ASSERT((intptr_t)(resCompPtr + compSize) - (intptr_t)resBlockPtr.ptr <=
                                  (intptr_t)resBlockPtr.size);

                        isOk = isOk && compareComponents(scalarType, resCompPtr, refCompPtr, 1);
                    }
                }

                if (!isOk)
                {
                    numFailed += 1;
                    if (numFailed < maxPrints)
                    {
                        std::ostringstream expected, got;
                        generateImmMatrixSrc(expected, refEntry.type, refEntry.matrixStride, refEntry.isRowMajor, false,
                                             -1, refElemPtr);
                        generateImmMatrixSrc(got, resEntry.type, resEntry.matrixStride, resEntry.isRowMajor, false, -1,
                                             resElemPtr);
                        log << TestLog::Message << "ERROR: mismatch in " << refEntry.name << ", top-level ndx "
                            << topElemNdx << ", bottom-level ndx " << elementNdx << ":\n"
                            << "  expected " << expected.str() << "\n"
                            << "  got " << got.str() << TestLog::EndMessage;
                    }
                }
            }
            else
            {
                DE_ASSERT((intptr_t)(refElemPtr + scalarSize * compSize) - (intptr_t)refBlockPtr.ptr <=
                          (intptr_t)refBlockPtr.size);
                DE_ASSERT((intptr_t)(resElemPtr + scalarSize * compSize) - (intptr_t)resBlockPtr.ptr <=
                          (intptr_t)resBlockPtr.size);

                const bool isOk = compareComponents(scalarType, resElemPtr, refElemPtr, scalarSize);

                if (!isOk)
                {
                    numFailed += 1;
                    if (numFailed < maxPrints)
                    {
                        std::ostringstream expected, got;
                        generateImmScalarVectorSrc(expected, refEntry.type, refElemPtr);
                        generateImmScalarVectorSrc(got, resEntry.type, resElemPtr);
                        log << TestLog::Message << "ERROR: mismatch in " << refEntry.name << ", top-level ndx "
                            << topElemNdx << ", bottom-level ndx " << elementNdx << ":\n"
                            << "  expected " << expected.str() << "\n"
                            << "  got " << got.str() << TestLog::EndMessage;
                    }
                }
            }
        }
    }

    if (numFailed >= maxPrints)
        log << TestLog::Message << "... (" << numFailed << " failures for " << refEntry.name << " in total)"
            << TestLog::EndMessage;

    return numFailed == 0;
}

bool compareData(tcu::TestLog &log, const BufferLayout &refLayout, const vector<BlockDataPtr> &refBlockPointers,
                 const BufferLayout &resLayout, const vector<BlockDataPtr> &resBlockPointers)
{
    const int numBlocks = (int)refLayout.blocks.size();
    bool allOk          = true;

    for (int refBlockNdx = 0; refBlockNdx < numBlocks; refBlockNdx++)
    {
        const BlockLayoutEntry &refBlock = refLayout.blocks[refBlockNdx];
        const BlockDataPtr &refBlockPtr  = refBlockPointers[refBlockNdx];
        int resBlockNdx                  = resLayout.getBlockIndex(refBlock.name.c_str());

        if (resBlockNdx >= 0)
        {
            DE_ASSERT(de::inBounds(resBlockNdx, 0, (int)resBlockPointers.size()));

            const BlockDataPtr &resBlockPtr = resBlockPointers[resBlockNdx];

            for (vector<int>::const_iterator refVarNdxIter = refBlock.activeVarIndices.begin();
                 refVarNdxIter != refBlock.activeVarIndices.end(); refVarNdxIter++)
            {
                const BufferVarLayoutEntry &refEntry = refLayout.bufferVars[*refVarNdxIter];
                int resVarNdx                        = resLayout.getVariableIndex(refEntry.name.c_str());

                if (resVarNdx >= 0)
                {
                    const BufferVarLayoutEntry &resEntry = resLayout.bufferVars[resVarNdx];
                    allOk = compareBufferVarData(log, refEntry, refBlockPtr, resEntry, resBlockPtr) && allOk;
                }
            }
        }
    }

    return allOk;
}

string getBlockAPIName(const BufferBlock &block, int instanceNdx)
{
    DE_ASSERT(block.isArray() || instanceNdx == 0);
    return block.getBlockName() + (block.isArray() ? ("[" + de::toString(instanceNdx) + "]") : string());
}

// \note Some implementations don't report block members in the order they are declared.
//         For checking whether size has to be adjusted by some top-level array actual size,
//         we only need to know a) whether there is a unsized top-level array, and b)
//         what is stride of that array.

static bool hasUnsizedArray(const BufferLayout &layout, const BlockLayoutEntry &entry)
{
    for (vector<int>::const_iterator varNdx = entry.activeVarIndices.begin(); varNdx != entry.activeVarIndices.end();
         ++varNdx)
    {
        if (isUnsizedArray(layout.bufferVars[*varNdx]))
            return true;
    }

    return false;
}

static int getUnsizedArrayStride(const BufferLayout &layout, const BlockLayoutEntry &entry)
{
    for (vector<int>::const_iterator varNdx = entry.activeVarIndices.begin(); varNdx != entry.activeVarIndices.end();
         ++varNdx)
    {
        const BufferVarLayoutEntry &varEntry = layout.bufferVars[*varNdx];

        if (varEntry.arraySize == 0)
            return varEntry.arrayStride;
        else if (varEntry.topLevelArraySize == 0)
            return varEntry.topLevelArrayStride;
    }

    return 0;
}

vector<int> computeBufferSizes(const ShaderInterface &interface, const BufferLayout &layout)
{
    vector<int> sizes(layout.blocks.size());

    for (int declNdx = 0; declNdx < interface.getNumBlocks(); declNdx++)
    {
        const BufferBlock &block = interface.getBlock(declNdx);
        const bool isArray       = block.isArray();
        const int numInstances   = isArray ? block.getArraySize() : 1;

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            const string apiName = getBlockAPIName(block, instanceNdx);
            const int blockNdx   = layout.getBlockIndex(apiName);

            if (blockNdx >= 0)
            {
                const BlockLayoutEntry &blockLayout = layout.blocks[blockNdx];
                const int baseSize                  = blockLayout.size;
                const bool isLastUnsized            = hasUnsizedArray(layout, blockLayout);
                const int lastArraySize             = isLastUnsized ? block.getLastUnsizedArraySize(instanceNdx) : 0;
                const int stride                    = isLastUnsized ? getUnsizedArrayStride(layout, blockLayout) : 0;

                sizes[blockNdx] = baseSize + lastArraySize * stride;
            }
        }
    }

    return sizes;
}

BlockDataPtr getBlockDataPtr(const BufferLayout &layout, const BlockLayoutEntry &blockLayout, void *ptr, int bufferSize)
{
    const bool isLastUnsized = hasUnsizedArray(layout, blockLayout);
    const int baseSize       = blockLayout.size;

    if (isLastUnsized)
    {
        const int lastArrayStride = getUnsizedArrayStride(layout, blockLayout);
        const int lastArraySize   = (bufferSize - baseSize) / (lastArrayStride ? lastArrayStride : 1);

        DE_ASSERT(baseSize + lastArraySize * lastArrayStride == bufferSize);

        return BlockDataPtr(ptr, bufferSize, lastArraySize);
    }
    else
        return BlockDataPtr(ptr, bufferSize, 0);
}

struct Buffer
{
    uint32_t buffer;
    int size;

    Buffer(uint32_t buffer_, int size_) : buffer(buffer_), size(size_)
    {
    }
    Buffer(void) : buffer(0), size(0)
    {
    }
};

struct BlockLocation
{
    int index;
    int offset;
    int size;

    BlockLocation(int index_, int offset_, int size_) : index(index_), offset(offset_), size(size_)
    {
    }
    BlockLocation(void) : index(0), offset(0), size(0)
    {
    }
};

void initRefDataStorage(const ShaderInterface &interface, const BufferLayout &layout, RefDataStorage &storage)
{
    DE_ASSERT(storage.data.empty() && storage.pointers.empty());

    const vector<int> bufferSizes = computeBufferSizes(interface, layout);
    int totalSize                 = 0;
    const int vec4Alignment       = (int)sizeof(uint32_t) * 4;

    for (vector<int>::const_iterator sizeIter = bufferSizes.begin(); sizeIter != bufferSizes.end(); ++sizeIter)
    {
        // Include enough space for alignment of individual blocks
        totalSize += deRoundUp32(*sizeIter, vec4Alignment);
    }

    storage.data.resize(totalSize);

    // Pointers for each block.
    {
        uint8_t *basePtr = storage.data.empty() ? nullptr : &storage.data[0];
        int curOffset    = 0;

        DE_ASSERT(bufferSizes.size() == layout.blocks.size());
        DE_ASSERT(totalSize == 0 || basePtr);

        storage.pointers.resize(layout.blocks.size());

        for (int blockNdx = 0; blockNdx < (int)layout.blocks.size(); blockNdx++)
        {
            const BlockLayoutEntry &blockLayout = layout.blocks[blockNdx];
            const int bufferSize                = bufferSizes[blockNdx];

            storage.pointers[blockNdx] = getBlockDataPtr(layout, blockLayout, basePtr + curOffset, bufferSize);

            // Ensure each new block starts fully aligned to avoid unaligned host accesses
            curOffset += deRoundUp32(bufferSize, vec4Alignment);
        }
    }
}

vector<BlockDataPtr> blockLocationsToPtrs(const BufferLayout &layout, const vector<BlockLocation> &blockLocations,
                                          const vector<void *> &bufPtrs)
{
    vector<BlockDataPtr> blockPtrs(blockLocations.size());

    DE_ASSERT(layout.blocks.size() == blockLocations.size());

    for (int blockNdx = 0; blockNdx < (int)layout.blocks.size(); blockNdx++)
    {
        const BlockLayoutEntry &blockLayout = layout.blocks[blockNdx];
        const BlockLocation &location       = blockLocations[blockNdx];

        blockPtrs[blockNdx] =
            getBlockDataPtr(layout, blockLayout, (uint8_t *)bufPtrs[location.index] + location.offset, location.size);
    }

    return blockPtrs;
}

} // namespace

de::MovePtr<vk::Allocation> allocateAndBindMemory(Context &context, vk::VkBuffer buffer, vk::MemoryRequirement memReqs)
{
    const vk::DeviceInterface &vkd         = context.getDeviceInterface();
    const vk::VkMemoryRequirements bufReqs = vk::getBufferMemoryRequirements(vkd, context.getDevice(), buffer);
    de::MovePtr<vk::Allocation> memory     = context.getDefaultAllocator().allocate(bufReqs, memReqs);

    vkd.bindBufferMemory(context.getDevice(), buffer, memory->getMemory(), memory->getOffset());

    return memory;
}

vk::Move<vk::VkBuffer> createBuffer(Context &context, vk::VkDeviceSize bufferSize, vk::VkBufferUsageFlags usageFlags)
{
    const vk::VkDevice vkDevice     = context.getDevice();
    const vk::DeviceInterface &vk   = context.getDeviceInterface();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const vk::VkBufferCreateInfo bufferInfo = {
        vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkBufferCreateFlags flags;
        bufferSize,                               // VkDeviceSize size;
        usageFlags,                               // VkBufferUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                       // uint32_t queueFamilyCount;
        &queueFamilyIndex                         // const uint32_t* pQueueFamilyIndices;
    };

    return vk::createBuffer(vk, vkDevice, &bufferInfo);
}

// SSBOLayoutCaseInstance

class SSBOLayoutCaseInstance : public TestInstance
{
public:
    SSBOLayoutCaseInstance(Context &context, SSBOLayoutCase::BufferMode bufferMode, const ShaderInterface &interface,
                           const BufferLayout &refLayout, const RefDataStorage &initialData,
                           const RefDataStorage &writeData, bool usePhysStorageBuffer);
    virtual ~SSBOLayoutCaseInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    SSBOLayoutCase::BufferMode m_bufferMode;
    const ShaderInterface &m_interface;
    const BufferLayout &m_refLayout;
    const RefDataStorage &m_initialData; // Initial data stored in buffer.
    const RefDataStorage &m_writeData;   // Data written by compute shader.
    const bool m_usePhysStorageBuffer;

    typedef de::SharedPtr<vk::Unique<vk::VkBuffer>> VkBufferSp;
    typedef de::SharedPtr<vk::Allocation> AllocationSp;

    std::vector<VkBufferSp> m_uniformBuffers;
    std::vector<AllocationSp> m_uniformAllocs;
};

SSBOLayoutCaseInstance::SSBOLayoutCaseInstance(Context &context, SSBOLayoutCase::BufferMode bufferMode,
                                               const ShaderInterface &interface, const BufferLayout &refLayout,
                                               const RefDataStorage &initialData, const RefDataStorage &writeData,
                                               bool usePhysStorageBuffer)
    : TestInstance(context)
    , m_bufferMode(bufferMode)
    , m_interface(interface)
    , m_refLayout(refLayout)
    , m_initialData(initialData)
    , m_writeData(writeData)
    , m_usePhysStorageBuffer(usePhysStorageBuffer)
{
}

SSBOLayoutCaseInstance::~SSBOLayoutCaseInstance(void)
{
}

tcu::TestStatus SSBOLayoutCaseInstance::iterate(void)
{
    // todo: add compute stage availability check
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const vk::VkQueue queue         = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    // Create descriptor set
    const uint32_t acBufferSize = 1024;
    vk::Move<vk::VkBuffer> acBuffer(createBuffer(m_context, acBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    de::UniquePtr<vk::Allocation> acBufferAlloc(
        allocateAndBindMemory(m_context, *acBuffer, vk::MemoryRequirement::HostVisible));

    deMemset(acBufferAlloc->getHostPtr(), 0, acBufferSize);
    flushMappedMemoryRange(vk, device, acBufferAlloc->getMemory(), acBufferAlloc->getOffset(), acBufferSize);

    vk::DescriptorSetLayoutBuilder setLayoutBuilder;
    vk::DescriptorPoolBuilder poolBuilder;

    setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

    int numBlocks         = 0;
    const int numBindings = m_interface.getNumBlocks();
    for (int bindingNdx = 0; bindingNdx < numBindings; bindingNdx++)
    {
        const BufferBlock &block = m_interface.getBlock(bindingNdx);
        if (block.isArray())
        {
            setLayoutBuilder.addArrayBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, block.getArraySize(),
                                             vk::VK_SHADER_STAGE_COMPUTE_BIT);
            numBlocks += block.getArraySize();
        }
        else
        {
            setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
            numBlocks += 1;
        }
    }

    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)(1 + numBlocks));

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(setLayoutBuilder.build(vk, device));
    const vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const vk::VkDescriptorSetAllocateInfo allocInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, *descriptorPool, 1u, &descriptorSetLayout.get(),
    };

    const vk::Unique<vk::VkDescriptorSet> descriptorSet(allocateDescriptorSet(vk, device, &allocInfo));
    const vk::VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*acBuffer, 0ull, acBufferSize);

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    std::vector<vk::VkDescriptorBufferInfo> descriptors(numBlocks);

    setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                                 vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);

    vector<BlockDataPtr> mappedBlockPtrs;

    vk::VkFlags usageFlags   = vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bool memoryDeviceAddress = false;
    if (m_usePhysStorageBuffer)
    {
        usageFlags |= vk::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        if (m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
            memoryDeviceAddress = true;
    }

    // Upload base buffers
    const std::vector<int> bufferSizes = computeBufferSizes(m_interface, m_refLayout);
    {
        std::vector<void *> mapPtrs;
        std::vector<BlockLocation> blockLocations(numBlocks);

        DE_ASSERT(bufferSizes.size() == m_refLayout.blocks.size());

        if (m_bufferMode == SSBOLayoutCase::BUFFERMODE_PER_BLOCK)
        {
            mapPtrs.resize(numBlocks);
            for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            {
                const uint32_t bufferSize = bufferSizes[blockNdx];
                DE_ASSERT(bufferSize > 0);

                blockLocations[blockNdx] = BlockLocation(blockNdx, 0, bufferSize);

                vk::Move<vk::VkBuffer> buffer     = createBuffer(m_context, bufferSize, usageFlags);
                de::MovePtr<vk::Allocation> alloc = allocateAndBindMemory(
                    m_context, *buffer,
                    vk::MemoryRequirement::HostVisible |
                        (memoryDeviceAddress ? vk::MemoryRequirement::DeviceAddress : vk::MemoryRequirement::Any));

                descriptors[blockNdx] = makeDescriptorBufferInfo(*buffer, 0ull, bufferSize);

                mapPtrs[blockNdx] = alloc->getHostPtr();

                m_uniformBuffers.push_back(VkBufferSp(new vk::Unique<vk::VkBuffer>(buffer)));
                m_uniformAllocs.push_back(AllocationSp(alloc.release()));
            }
        }
        else
        {
            DE_ASSERT(m_bufferMode == SSBOLayoutCase::BUFFERMODE_SINGLE);

            vk::VkPhysicalDeviceProperties properties;
            m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);
            const int bindingAlignment = (int)properties.limits.minStorageBufferOffsetAlignment;
            int curOffset              = 0;
            for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            {
                const int bufferSize = bufferSizes[blockNdx];
                DE_ASSERT(bufferSize > 0);

                if (bindingAlignment > 0)
                    curOffset = deRoundUp32(curOffset, bindingAlignment);

                blockLocations[blockNdx] = BlockLocation(0, curOffset, bufferSize);
                curOffset += bufferSize;
            }

            const int totalBufferSize         = curOffset;
            vk::Move<vk::VkBuffer> buffer     = createBuffer(m_context, totalBufferSize, usageFlags);
            de::MovePtr<vk::Allocation> alloc = allocateAndBindMemory(
                m_context, *buffer,
                vk::MemoryRequirement::HostVisible |
                    (memoryDeviceAddress ? vk::MemoryRequirement::DeviceAddress : vk::MemoryRequirement::Any));

            mapPtrs.push_back(alloc->getHostPtr());

            for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            {
                const uint32_t bufferSize = bufferSizes[blockNdx];
                const uint32_t offset     = blockLocations[blockNdx].offset;

                descriptors[blockNdx] = makeDescriptorBufferInfo(*buffer, offset, bufferSize);
            }

            m_uniformBuffers.push_back(VkBufferSp(new vk::Unique<vk::VkBuffer>(buffer)));
            m_uniformAllocs.push_back(AllocationSp(alloc.release()));
        }

        // Update remaining bindings
        {
            int blockNdx = 0;
            for (int bindingNdx = 0; bindingNdx < numBindings; ++bindingNdx)
            {
                const BufferBlock &block     = m_interface.getBlock(bindingNdx);
                const int numBlocksInBinding = (block.isArray() ? block.getArraySize() : 1);

                setUpdateBuilder.writeArray(
                    *descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(bindingNdx + 1),
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBlocksInBinding, &descriptors[blockNdx]);

                blockNdx += numBlocksInBinding;
            }
        }

        // Copy the initial data to the storage buffers
        {
            mappedBlockPtrs = blockLocationsToPtrs(m_refLayout, blockLocations, mapPtrs);
            copyData(m_refLayout, mappedBlockPtrs, m_refLayout, m_initialData.pointers);

            for (size_t allocNdx = 0; allocNdx < m_uniformAllocs.size(); allocNdx++)
            {
                vk::Allocation *alloc = m_uniformAllocs[allocNdx].get();
                flushMappedMemoryRange(vk, device, alloc->getMemory(), alloc->getOffset(), VK_WHOLE_SIZE);
            }
        }
    }

    std::vector<vk::VkDeviceAddress> gpuAddrs;
    // Query the buffer device addresses and push them via push constants
    if (m_usePhysStorageBuffer)
    {
        //const bool useKHR = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");

        vk::VkBufferDeviceAddressInfo info = {
            vk::VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
            nullptr,                                          // const void* pNext;
            VK_NULL_HANDLE,                                   // VkBuffer            buffer
        };

        for (uint32_t i = 0; i < descriptors.size(); ++i)
        {
            info.buffer = descriptors[i].buffer;
            vk::VkDeviceAddress addr;
            //if (useKHR)
            addr = vk.getBufferDeviceAddress(device, &info);
            //else
            // addr = vk.getBufferDeviceAddressEXT(device, &info);
            addr += descriptors[i].offset;
            gpuAddrs.push_back(addr);
        }
    }

    setUpdateBuilder.update(vk, device);

    const vk::VkPushConstantRange pushConstRange = {
        vk::VK_SHADER_STAGE_COMPUTE_BIT,                             // VkShaderStageFlags    stageFlags
        0,                                                           // uint32_t                offset
        (uint32_t)(sizeof(vk::VkDeviceAddress) * descriptors.size()) // uint32_t                size
    };

    // must fit in spec min max
    DE_ASSERT(pushConstRange.size <= 128);

    const vk::VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        (vk::VkPipelineLayoutCreateFlags)0,
        1u,                               // uint32_t descriptorSetCount;
        &*descriptorSetLayout,            // const VkDescriptorSetLayout* pSetLayouts;
        m_usePhysStorageBuffer ? 1u : 0u, // uint32_t pushConstantRangeCount;
        &pushConstRange,                  // const VkPushConstantRange* pPushConstantRanges;
    };
    vk::Move<vk::VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, device, &pipelineLayoutParams));

    m_context.getTestContext().touchWatchdogAndDisableIntervalTimeLimit();

    vk::Move<vk::VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("compute"), 0));
    const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                 // const void* pNext;
        (vk::VkPipelineShaderStageCreateFlags)0,
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStage stage;
        *shaderModule,                   // VkShader shader;
        "main",                          //
        nullptr,                         // const VkSpecializationInfo* pSpecializationInfo;
    };
    const vk::VkComputePipelineCreateInfo pipelineCreateInfo = {
        vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                            // const void* pNext;
        0,                                                  // VkPipelineCreateFlags flags;
        pipelineShaderStageParams,                          // VkPipelineShaderStageCreateInfo stage;
        *pipelineLayout,                                    // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                     // VkPipeline basePipelineHandle;
        0,                                                  // int32_t basePipelineIndex;
    };
    vk::Move<vk::VkPipeline> pipeline(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

    m_context.getTestContext().touchWatchdogAndEnableIntervalTimeLimit();

    vk::Move<vk::VkCommandPool> cmdPool(
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    vk::Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    if (gpuAddrs.size())
    {
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            (uint32_t)(sizeof(vk::VkDeviceAddress) * gpuAddrs.size()), &gpuAddrs[0]);
    }
    vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                             &descriptorSet.get(), 0u, nullptr);

    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // Add barriers for shader writes to storage buffers before host access
    std::vector<vk::VkBufferMemoryBarrier> barriers;
    if (m_bufferMode == SSBOLayoutCase::BUFFERMODE_PER_BLOCK)
    {
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const vk::VkBuffer uniformBuffer = m_uniformBuffers[blockNdx].get()->get();

            const vk::VkBufferMemoryBarrier barrier = {vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                       nullptr,
                                                       vk::VK_ACCESS_SHADER_WRITE_BIT,
                                                       vk::VK_ACCESS_HOST_READ_BIT,
                                                       VK_QUEUE_FAMILY_IGNORED,
                                                       VK_QUEUE_FAMILY_IGNORED,
                                                       uniformBuffer,
                                                       0u,
                                                       static_cast<vk::VkDeviceSize>(bufferSizes[blockNdx])};
            barriers.push_back(barrier);
        }
    }
    else
    {
        const vk::VkBuffer uniformBuffer = m_uniformBuffers[0].get()->get();

        vk::VkDeviceSize totalSize = 0;
        for (size_t bufferNdx = 0; bufferNdx < bufferSizes.size(); bufferNdx++)
            totalSize += bufferSizes[bufferNdx];

        const vk::VkBufferMemoryBarrier barrier = {vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                   nullptr,
                                                   vk::VK_ACCESS_SHADER_WRITE_BIT,
                                                   vk::VK_ACCESS_HOST_READ_BIT,
                                                   VK_QUEUE_FAMILY_IGNORED,
                                                   VK_QUEUE_FAMILY_IGNORED,
                                                   uniformBuffer,
                                                   0u,
                                                   totalSize};
        barriers.push_back(barrier);
    }
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                          (vk::VkDependencyFlags)0, 0u, nullptr, static_cast<uint32_t>(barriers.size()), &barriers[0],
                          0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Read back ac_numPassed data
    bool counterOk;
    {
        const int refCount = 1;
        int resCount       = 0;

        invalidateAlloc(vk, device, *acBufferAlloc);

        resCount = *((const int *)acBufferAlloc->getHostPtr());

        counterOk = (refCount == resCount);
        if (!counterOk)
        {
            m_context.getTestContext().getLog() << TestLog::Message << "Error: ac_numPassed = " << resCount
                                                << ", expected " << refCount << TestLog::EndMessage;
        }
    }

    for (size_t allocNdx = 0; allocNdx < m_uniformAllocs.size(); allocNdx++)
    {
        vk::Allocation *alloc = m_uniformAllocs[allocNdx].get();
        invalidateAlloc(vk, device, *alloc);
    }

    // Validate result
    const bool compareOk = compareData(m_context.getTestContext().getLog(), m_refLayout, m_writeData.pointers,
                                       m_refLayout, mappedBlockPtrs);

    if (compareOk && counterOk)
        return tcu::TestStatus::pass("Result comparison and counter values are OK");
    else if (!compareOk && counterOk)
        return tcu::TestStatus::fail("Result comparison failed");
    else if (compareOk && !counterOk)
        return tcu::TestStatus::fail("Counter value incorrect");
    else
        return tcu::TestStatus::fail("Result comparison and counter values are incorrect");
}

// SSBOLayoutCase.

SSBOLayoutCase::SSBOLayoutCase(tcu::TestContext &testCtx, const char *name, BufferMode bufferMode,
                               MatrixLoadFlags matrixLoadFlag, MatrixStoreFlags matrixStoreFlag,
                               bool usePhysStorageBuffer)
    : TestCase(testCtx, name)
    , m_bufferMode(bufferMode)
    , m_matrixLoadFlag(matrixLoadFlag)
    , m_matrixStoreFlag(matrixStoreFlag)
    , m_usePhysStorageBuffer(usePhysStorageBuffer)
{
}

SSBOLayoutCase::~SSBOLayoutCase(void)
{
}

void SSBOLayoutCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_computeShaderSrc.empty());

    // Valid scalar layouts are a superset of valid relaxed layouts.  So check scalar layout first.
    if (usesScalarLayout(m_interface))
    {
        programCollection.glslSources.add("compute")
            << glu::ComputeSource(m_computeShaderSrc)
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0,
                                      vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
    }
    else if (usesRelaxedLayout(m_interface))
    {
        programCollection.glslSources.add("compute")
            << glu::ComputeSource(m_computeShaderSrc)
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0,
                                      vk::ShaderBuildOptions::FLAG_ALLOW_RELAXED_OFFSETS);
    }
    else
        programCollection.glslSources.add("compute") << glu::ComputeSource(m_computeShaderSrc);
}

TestInstance *SSBOLayoutCase::createInstance(Context &context) const
{
    return new SSBOLayoutCaseInstance(context, m_bufferMode, m_interface, m_refLayout, m_initialData, m_writeData,
                                      m_usePhysStorageBuffer);
}

void SSBOLayoutCase::checkSupport(Context &context) const
{
    if (!context.isDeviceFunctionalitySupported("VK_KHR_relaxed_block_layout") && usesRelaxedLayout(m_interface))
        TCU_THROW(NotSupportedError, "VK_KHR_relaxed_block_layout not supported");
    if (!context.get16BitStorageFeatures().storageBuffer16BitAccess && uses16BitStorage(m_interface))
        TCU_THROW(NotSupportedError, "storageBuffer16BitAccess not supported");
    if (!context.get8BitStorageFeatures().storageBuffer8BitAccess && uses8BitStorage(m_interface))
        TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");
    if (!context.getScalarBlockLayoutFeatures().scalarBlockLayout && usesScalarLayout(m_interface))
        TCU_THROW(NotSupportedError, "scalarBlockLayout not supported");
    if (m_usePhysStorageBuffer && !context.isBufferDeviceAddressSupported())
        TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");
    if (usesDescriptorIndexing(m_interface) &&
        (!context.getDescriptorIndexingFeatures().shaderStorageBufferArrayNonUniformIndexing ||
         !context.getDescriptorIndexingFeatures().runtimeDescriptorArray))
        TCU_THROW(NotSupportedError, "Descriptor indexing over storage buffer not supported");

    const vk::VkPhysicalDeviceProperties &properties = context.getDeviceProperties();
    // Shader defines N+1 storage buffers: N to operate and one more to store the number of cases passed.
    uint32_t blockCount = 1u;
    for (int32_t blockIdx = 0u; blockIdx < m_interface.getNumBlocks(); blockIdx++)
    {
        blockCount +=
            m_interface.getBlock(blockIdx).getArraySize() ? m_interface.getBlock(blockIdx).getArraySize() : 1u;
    }

    if (properties.limits.maxPerStageDescriptorStorageBuffers < blockCount)
        TCU_THROW(NotSupportedError,
                  "Descriptor set storage buffers count higher than the maximum supported by the driver");
}

void SSBOLayoutCase::delayedInit(void)
{
    computeReferenceLayout(m_refLayout, m_interface);
    initRefDataStorage(m_interface, m_refLayout, m_initialData);
    initRefDataStorage(m_interface, m_refLayout, m_writeData);
    generateValues(m_refLayout, m_initialData.pointers, deStringHash(getName()) ^ 0xad2f7214);
    generateValues(m_refLayout, m_writeData.pointers, deStringHash(getName()) ^ 0x25ca4e7);
    copyNonWrittenData(m_interface, m_refLayout, m_initialData.pointers, m_writeData.pointers);

    m_computeShaderSrc = generateComputeShader(m_interface, m_refLayout, m_initialData.pointers, m_writeData.pointers,
                                               m_matrixLoadFlag, m_matrixStoreFlag, m_usePhysStorageBuffer);
}

} // namespace ssbo
} // namespace vkt
