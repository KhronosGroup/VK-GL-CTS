/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief SSBO Nested Unsized Arrays Tests Header File.
 *//*--------------------------------------------------------------------*/

#include "vktSSBOLayoutNestedUnsizedArraysTests.hpp"
#include "vktTestCase.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

#include <numeric>
#include <random>
#include <iostream>

namespace vkt
{
namespace ssbo
{
namespace
{
using namespace vk;
namespace sg
{
struct SG;
struct INode;
typedef std::shared_ptr<INode> INodePtr;
template <class>
struct Node;
enum class Kind : int
{
    None,
    Simple,
    Vector,
    Matrix,
    Array,
    Struct
};

struct INode
{
    friend struct Node<INodePtr>;
    const std::string typeName;
    INodePtr next;
    virtual INodePtr getChildren(bool raise = true) const
    {
        if (raise)
        {
            DE_ASSERT(0);
        }
        return nullptr;
    }
    virtual INodePtr getLastChild(bool raise = true) const
    {
        return getChildren(raise);
    }
    virtual INodePtr getElementType() const
    {
        return nullptr;
    }
    virtual Kind getKind() const
    {
        return Kind::None;
    }
    virtual INodePtr clone() const
    {
        return nullptr;
    }
    virtual void loop(float & /*seedValue*/)
    {
    }
    virtual std::string genFieldName(uint32_t index, bool /*appendRank*/ = false, uint32_t /*rank*/ = 0) const
    {
        return typeName + '_' + std::to_string(index);
    }
    virtual std::size_t getBaseAlignment() const
    {
        DE_ASSERT(false);
        return 0u;
    }
    virtual std::size_t getLogicalSize() const
    {
        return 0;
    }
    virtual void serializeOrDeserialize(const void * /*src*/, void * /*dst*/, size_t & /*offset*/, bool /*serialize*/,
                                        int & /*nesting*/)
    {
    }
    virtual uint32_t getVisitCount() const
    {
        DE_ASSERT(false);
        return 0;
    }
    virtual void genLoops(std::ostream & /*str*/, const std::string & /*currentPath*/, const std::string & /* rhsCode*/,
                          uint32_t /* fieldIndex*/, uint32_t /* indent*/, uint32_t & /* iteratorSeed*/
    ) const
    {
    }
    virtual ~INode() = default;

protected:
    INode(const std::string &typeName_, INodePtr) : typeName(typeName_)
    {
    }
};

static void putIndent(std::ostream &str, uint32_t indent)
{
    str << std::string((indent * 4u), ' ');
}

template <std::size_t K, class T>
struct vec;
template <std::size_t Cols, std::size_t Rows, class T, bool ColumnMajor>
struct mat;
template <class X, std::size_t Elems, bool IsRuntime>
struct Array;

template <class>
const char *type_to_string = "";
template <>
[[maybe_unused]] inline constexpr const char *type_to_string<int32_t> = "int";
template <>
[[maybe_unused]] inline constexpr const char *type_to_string<uint32_t> = "uint";
template <>
[[maybe_unused]] inline constexpr const char *type_to_string<float> = "float";
template <class X, std::size_t Elems, bool IsRuntime>
inline constexpr const char *type_to_string<Array<X, Elems, IsRuntime>> = type_to_string<X>;

template <class X>
struct Node : public INode
{
    X value{};
    Node(const std::string & /*typeName*/, INodePtr) : INode(type_to_string<X>, nullptr)
    {
    }
    virtual Kind getKind() const override
    {
        return Kind::Simple;
    }
    virtual INodePtr clone() const override
    {
        auto c   = std::make_shared<Node>(type_to_string<X>, nullptr);
        c->value = value;
        return c;
    }
    virtual void loop(float &seed) override
    {
        value = X(seed);
        seed += 1.0f;
    }
    virtual std::size_t getBaseAlignment() const override
    {
        return sizeof(X);
    }
    virtual std::size_t getLogicalSize() const override
    {
        return sizeof(X);
    }

    virtual void serializeOrDeserialize(const void *src, void *dst, size_t &offset, bool serialize,
                                        int & /*nesting*/) override
    {
        if (serialize)
        {
            X *p = reinterpret_cast<X *>(reinterpret_cast<std::byte *>(dst) + offset);
            *p   = value;
        }
        else
        {
            const X *p = reinterpret_cast<const X *>(reinterpret_cast<const std::byte *>(src) + offset);
            value      = *p;
        }
    }
    virtual uint32_t getVisitCount() const override
    {
        return 1u;
    };

    virtual void genLoops(std::ostream &str, const std::string &currentPath, const std::string &rhsCode,
                          uint32_t, // fieldIndex
                          uint32_t indent, uint32_t &iteratorSeed) const override
    {
        iteratorSeed = iteratorSeed + 1u;
        putIndent(str, indent);
        str << currentPath << " = " << typeName << '(' << rhsCode << ");\n";
    }
};

template <>
struct Node<INodePtr> : public INode
{
    INodePtr children;
    Node(const std::string &, INodePtr ch) : INode(ch->typeName, ch), children(cloneChildren(ch))
    {
    }
    virtual INodePtr getChildren(bool) const override
    {
        return children;
    }
    static INodePtr createEmpty(const std::string &typeName)
    {
        INodePtr children(new INode(typeName, nullptr));
        return std::make_shared<Node>(typeName, children);
    }
    INodePtr cloneChildren(INodePtr ch) const
    {
        INodePtr list(new INode(ch->typeName, nullptr));
        INodePtr *nextField = &list->next;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            *nextField = p->clone();
            nextField  = &nextField->get()->next;
        }
        return list;
    }
    virtual INodePtr getLastChild(bool) const override
    {
        INodePtr ch = children;
        while (ch->next)
            ch = ch->next;
        return ch;
    }
    virtual Kind getKind() const override
    {
        return Kind::Struct;
    }
    virtual INodePtr clone() const override
    {
        return std::make_shared<Node>(typeName, children);
    }
    virtual void loop(float &seed) override
    {
        for (INodePtr p = children->next; p; p = p->next)
            p->loop(seed);
    }
    virtual std::size_t getBaseAlignment() const override
    {
        size_t maxAb = 0u;
        for (INodePtr p = children->next; p; p = p->next)
        {
            maxAb = std::max(maxAb, p->getBaseAlignment());
        }
        return maxAb;
    }
    static std::size_t updateOffsetWithPadding(const INode *field, std::size_t baseAlignment, std::size_t &offset)
    {
        const std::size_t Ab            = field ? field->getBaseAlignment() : baseAlignment;
        const std::size_t alignedOffset = ((offset + Ab - 1u) / Ab) * Ab;
        const std::size_t padding       = alignedOffset - offset;
        offset += padding;
        return padding;
    }
    virtual std::size_t getLogicalSize() const override
    {
        const std::size_t structAb = getBaseAlignment();
        std::size_t offset         = 0;

        for (INodePtr p = children->next; p; p = p->next)
        {
            const std::size_t fieldAb   = p->getBaseAlignment();
            const std::size_t fieldSize = p->getLogicalSize();

            updateOffsetWithPadding(nullptr, fieldAb, offset);
            offset += fieldSize;
        }

        updateOffsetWithPadding(nullptr, structAb, offset);
        return offset;
    }

    virtual void serializeOrDeserialize(const void *src, void *dst, size_t &offset, bool serialize,
                                        int &nesting) override
    {
        nesting = nesting + 1;

        const std::size_t thisStructAb = getBaseAlignment();
        // Add padding before serialize/deserialize this structura
        updateOffsetWithPadding(nullptr, thisStructAb, offset);

        uint32_t childIndex = 0u;

        for (INodePtr p = children->next; p; p = p->next, ++childIndex)
        {
            // Add padding before enter serialize/deserialize consecutive child field
            updateOffsetWithPadding(nullptr, p->getBaseAlignment(), offset);
            p->serializeOrDeserialize(src, dst, offset, serialize, nesting);
            if (p->getKind() != Kind::Struct)
            {
                offset = offset + p->getLogicalSize();
            }
        }
        // Add padding after serialize/deserialize this structura
        updateOffsetWithPadding(nullptr, thisStructAb, offset);

        nesting = nesting - 1;
    }

    virtual uint32_t getVisitCount() const override
    {
        uint32_t visitCount = 0u;
        for (INodePtr p = children->next; p; p = p->next)
            visitCount += p->getVisitCount();
        return visitCount;
    }
    virtual void genLoops(std::ostream &str, const std::string &currentPath, const std::string &rhsCode,
                          uint32_t fieldIndex, uint32_t indent, uint32_t &iteratorSeed) const override
    {
        fieldIndex = 0u;
        for (INodePtr p = children->next; p; p = p->next)
        {
            p->genLoops(str, (currentPath + '.' + p->genFieldName(fieldIndex)), rhsCode, fieldIndex, indent,
                        iteratorSeed);
            fieldIndex = fieldIndex + 1u;
        }
    }
};

template <std::size_t Elems, bool IsRuntime>
struct Node<Array<INodePtr, Elems, IsRuntime>>
    : public INode, public std::enable_shared_from_this<Node<Array<INodePtr, Elems, IsRuntime>>>
{
    static_assert(Elems != 0, "Array size must be a positive integer");
    std::vector<INodePtr> value;
    const std::size_t dynamicElems;
    const bool dynamicIsRuntime;
    const std::string privateName;
    Node(const std::string & /*typeName*/, INodePtr structure)
        : INode(structure->typeName, nullptr)
        , value(Elems)
        , dynamicElems(Elems)
        , dynamicIsRuntime(IsRuntime)
        , privateName()
    {
        for (uint32_t i = 0u; i < Elems; ++i)
            value[i] = structure->clone();
    }
    Node(INodePtr elemType, std::size_t elemCount, bool isRuntimeArray = false)
        : INode(elemType->typeName, nullptr)
        , value(elemCount)
        , dynamicElems(elemCount)
        , dynamicIsRuntime(isRuntimeArray)
        , privateName()
    {
        for (uint32_t i = 0u; i < elemCount; ++i)
            value[i] = elemType->clone();
    }
    std::string checkElems(const std::vector<INodePtr> &elems)
    {
        return elems[0]->typeName;
    }
    Node(const std::vector<INodePtr> &elems, bool isRuntimeArray = false, const std::string &arrayName = std::string())
        : INode(checkElems(elems), nullptr)
        , value(elems.size())
        , dynamicElems(elems.size())
        , dynamicIsRuntime(isRuntimeArray)
        , privateName(arrayName)
    {
        for (uint32_t i = 0u; i < dynamicElems; ++i)
            value[i] = elems[i]->clone();
    }
    virtual Kind getKind() const override
    {
        return Kind::Array;
    }
    virtual INodePtr clone() const override
    {
        auto c = std::make_shared<Node<Array<INodePtr, 1, false>>>(value[0], dynamicElems, dynamicIsRuntime);
        for (uint32_t i = 0u; i < dynamicElems; ++i)
            c->value[i] = value[i]->clone();
        return c;
    }
    virtual INodePtr getElementType() const override
    {
        return value[0];
    }
    virtual std::string genFieldName(uint32_t index, bool appendRank, uint32_t /*rank*/ = 0) const override
    {
        const std::string fieldName = privateName.empty() ? INode::genFieldName(index) : privateName;
        if (appendRank)
        {
            return fieldName + '[' +
                   (dynamicIsRuntime ? ("/* " + std::to_string(dynamicElems) + " */") : std::to_string(dynamicElems)) +
                   ']';
        }
        return fieldName;
    }
    virtual void loop(float &seed) override
    {
        for (std::size_t i = 0; i < dynamicElems; ++i)
        {
            value[i]->loop(seed);
        }
    }
    virtual std::size_t getBaseAlignment() const override
    {
        return value[0]->getBaseAlignment();
    }
    virtual std::size_t getLogicalSize() const override
    {
        return dynamicElems * getStride();
    }
    std::size_t getStride() const
    {
        std::size_t offset = value[0]->getLogicalSize();
        Node<INodePtr>::updateOffsetWithPadding(nullptr, getBaseAlignment(), offset);
        return offset;
    }
    virtual void serializeOrDeserialize(const void *src, void *dst, size_t &offset, bool serialize,
                                        int &nesting) override
    {
        const std::size_t stride = getStride();

        std::size_t rwOffset = offset;
        for (std::size_t i = 0; i < dynamicElems; ++i)
        {
            std::size_t elementOffset = 0u;

            if (serialize)
            {
                std::byte *elementAddr = reinterpret_cast<std::byte *>(dst) + rwOffset;
                value[i]->serializeOrDeserialize(nullptr, elementAddr, elementOffset, true, nesting);
            }
            else
            {
                const std::byte *elementAddr = reinterpret_cast<const std::byte *>(src) + rwOffset;
                value[i]->serializeOrDeserialize(elementAddr, nullptr, elementOffset, false, nesting);
            }
            rwOffset = rwOffset + stride;
        }
    }
    virtual uint32_t getVisitCount() const override
    {
        return uint32_t(value[0]->getVisitCount() * dynamicElems);
    }
    virtual void genLoops(std::ostream &str, const std::string &currentPath, const std::string &rhsCode,
                          uint32_t fieldIndex, uint32_t indent, uint32_t &iteratorSeed) const override
    {
        const std::string it = genFieldName(fieldIndex, false) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << it << " = 0; " << it << " < " << dynamicElems << "; ++" << it << ") {\n";
        value[0]->genLoops(str, (currentPath + '[' + it + ']'), rhsCode, 0, (indent + 1), iteratorSeed);
        putIndent(str, indent);
        str << "}\n";
    }
};

template <std::size_t Cols, std::size_t Rows, class T = float, bool ColumnMajor = true>
struct mat
{
};
template <std::size_t Cols, size_t Rows, class T, bool ColumnMajor>
struct Node<mat<Cols, Rows, T, ColumnMajor>> : public INode
{
    static_assert(Cols == 2 || Cols == 3 || Cols == 4 || Rows == 2 || Rows == 3 || Rows == 4, "???");
    T value[Cols * Rows]{};

    virtual Kind getKind() const override
    {
        return Kind::Matrix;
    }

    virtual INodePtr clone() const override
    {
        auto c = std::make_shared<Node>(genTypeName(), nullptr);
        for (uint32_t R = 0u; R < Rows; ++R)
            for (uint32_t C = 0u; C < Cols; ++C)
                c->value[R * Cols + C] = value[R * Cols + C];
        return c;
    }

    std::string genTypeName() const
    {
        std::ostringstream os;
        os << (std::is_unsigned_v<T> ? "u" : "") << (std::is_integral_v<T> ? "i" : "") << "mat";
        if constexpr (Cols == Rows)
            os << Cols;
        else
            os << Cols << 'x' << Rows;
        return os.str();
    }

    Node(const std::string &, INodePtr) : INode(genTypeName(), nullptr)
    {
    }

    virtual INodePtr getElementType() const override
    {
        return std::make_shared<Node<T>>(std::string(), nullptr);
    }

    virtual void loop(float &seed) override
    {
        for (std::size_t C = 0u; C < Cols; ++C)
            for (std::size_t R = 0u; R < Rows; ++R)
            {
                value[C * Rows + R] = T(seed);
                seed += 1.f;
            }
    }

    std::size_t getColumnStride() const
    {
        constexpr std::size_t vecLength  = ColumnMajor ? Rows : Cols;
        constexpr std::size_t normLength = (vecLength == 3 ? 4 : vecLength);
        return normLength * sizeof(T);
    }

    std::size_t getMatrixSize() const
    {
        constexpr std::size_t columnCount = ColumnMajor ? Cols : Rows;
        return columnCount * getColumnStride();
    }

    virtual std::size_t getBaseAlignment() const override
    {
        return getColumnStride();
    }

    virtual std::size_t getLogicalSize() const override
    {
        return getMatrixSize();
    }

    virtual void serializeOrDeserialize(const void *src, void *dst, size_t &xoffset, bool serialize,
                                        int & /*nesting*/) override
    {
        const std::size_t stride = getColumnStride();

        for (std::size_t i = 0; i < Cols; ++i)
        {
            std::size_t columnStart = xoffset + (i * stride);
            for (std::size_t j = 0; j < Rows; ++j)
            {

                if (serialize)
                {
                    T *p = reinterpret_cast<T *>(reinterpret_cast<std::byte *>(dst) + columnStart + (j * sizeof(T)));
                    *p   = value[i * Rows + j];
                }
                else
                {
                    const T *p = reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(src) + columnStart +
                                                             (j * sizeof(T)));
                    value[i * Rows + j] = *p;
                }
            }
        }
    }

    virtual uint32_t getVisitCount() const override
    {
        return uint32_t(Cols * Rows);
    }

    virtual void genLoops(std::ostream &str, const std::string &currentPath, const std::string &rhsCode,
                          uint32_t fieldIndex, uint32_t indent, uint32_t &iteratorSeed) const override
    {
        const std::string ic = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        const std::string ir = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << ic << " = 0; " << ic << " < " << Cols << "; ++" << ic << ") {\n";
        putIndent(str, indent + 1u);
        str << "for (uint " << ir << " = 0; " << ir << " < " << Rows << "; ++" << ir << ") {\n";
        putIndent(str, indent + 2u);
        const std::string path = currentPath + '[' + ic + "][" + ir + ']';
        str << path << " = " << getElementType()->typeName << '(' << rhsCode << ");\n";
        putIndent(str, indent + 1u);
        str << "}\n";
        putIndent(str, indent);
        str << "}\n";
    }
};

template <std::size_t K, class T = float>
struct vec
{
};
template <std::size_t K, class T>
struct Node<vec<K, T>> : public INode
{
    static_assert(false == std::is_same_v<T, INodePtr>, "???");
    T value[K]{};
    using ValueType = T[K];

    virtual Kind getKind() const override
    {
        return Kind::Vector;
    }

    virtual INodePtr clone() const override
    {
        auto c = std::make_shared<Node>(genTypeName(), nullptr);
        for (uint32_t i = 0u; i < K; ++i)
            c->value[i] = value[i];
        return c;
    }

    std::string genTypeName() const
    {
        std::ostringstream os;
        os << (std::is_unsigned_v<T> ? "u" : "") << (std::is_integral_v<T> ? "i" : "") << "vec" << K;
        return os.str();
    }

    Node(const std::string &, INodePtr) : INode(genTypeName(), nullptr)
    {
    }

    virtual INodePtr getElementType() const override
    {
        return std::make_shared<Node<T>>(std::string(), nullptr);
    }

    virtual void loop(float &seed) override
    {
        for (std::size_t X = 0u; X < K; ++X)
        {
            value[X] = T(seed);
            seed += 1.f;
        }
    }

    virtual std::size_t getBaseAlignment() const override
    {
        const std::size_t elemSize = getElementType()->getBaseAlignment();
        return (K == 3u) ? (4u * elemSize) : (K * elemSize);
    }

    virtual std::size_t getLogicalSize() const override
    {
        const std::size_t elemSize = getElementType()->getLogicalSize();
        return K * elemSize;
    }

    virtual void serializeOrDeserialize(const void *src, void *dst, size_t &offset, bool serialize,
                                        int & /*nesting*/) override
    {
        std::size_t elemOffset = offset;

        for (std::size_t i = 0u; i < K; ++i)
        {
            if (serialize)
            {
                T *p = reinterpret_cast<T *>(reinterpret_cast<std::byte *>(dst) + elemOffset);
                *p   = value[i];
            }
            else
            {
                const T *p = reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(src) + elemOffset);
                value[i]   = *p;
            }
            elemOffset = elemOffset + sizeof(T);
        }
    }

    virtual uint32_t getVisitCount() const override
    {
        return uint32_t(K);
    }

    virtual void genLoops(std::ostream &str, const std::string &currentPath, const std::string &rhsCode,
                          uint32_t fieldIndex, uint32_t indent, uint32_t &iteratorSeed) const override
    {
        const std::string it = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << it << " = 0; " << it << " < " << K << "; ++" << it << ") {\n";
        putIndent(str, indent + 1u);
        str << currentPath << '[' << it << ']' << " = " << getElementType()->typeName << '(' << rhsCode << ");\n";
        putIndent(str, indent);
        str << "}\n";
    }
};

struct SG
{
    static INodePtr generateStruct(const std::string &typeName, const std::vector<INodePtr> fields)
    {
        INodePtr root   = Node<INodePtr>::createEmpty(typeName);
        INodePtr *pNext = &root->getChildren()->next;
        for (auto field : fields)
        {
            *pNext = field->clone();
            pNext  = &pNext->get()->next;
        }
        return root;
    }

    // example: makeField(int()), makeField(vec<2,int>()),
    template <class FieldType>
    static INodePtr makeField(const FieldType &)
    {
        return std::make_shared<Node<FieldType>>(std::string(), nullptr);
    }

    static INodePtr makeArrayField(INodePtr elemType, std::size_t elemCount, bool isRuntime = false)
    {
        return std::make_shared<Node<sg::Array<INodePtr, 1, false>>>(elemType, elemCount, isRuntime);
    }

    static INodePtr makeArrayField(const std::vector<INodePtr> &elems, bool isRuntime = false,
                                   const std::string &arrayName = std::string())
    {
        return std::make_shared<Node<sg::Array<INodePtr, 1, false>>>(elems, isRuntime, arrayName);
    }

    static bool structureAppendField(INodePtr rootStruct, INodePtr field)
    {
        DE_ASSERT(rootStruct && rootStruct->getKind() == Kind::Struct);
        INodePtr lastField = rootStruct->getLastChild(false);
        if (lastField)
        {
            lastField->next = field->clone();
            return true;
        }
        return false;
    }

    static void printStruct(INodePtr structNode, std::ostream &str, bool declaration = true)
    {
        DE_ASSERT(structNode && structNode->getKind() == Kind::Struct);
        uint32_t fieldNum = 0u;
        INodePtr ch       = structNode->getChildren();

        std::string::size_type longest = 0u;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            longest = std::max(longest, p->typeName.length());
        }

        if (declaration)
        {
            str << "struct " << structNode->typeName << ' ';
        }
        str << '{' << std::endl;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            str << "    " << p->typeName << std::string((longest - p->typeName.length() + 1), ' ')
                << p->genFieldName(fieldNum++, true) << ";\n";
        }
        str << '}';
        if (declaration)
        {
            str << ";\n";
        }
    }

    static std::vector<INodePtr> getStructList(INodePtr rootStruct)
    {
        DE_ASSERT(rootStruct && rootStruct->getKind() == Kind::Struct);
        std::vector<INodePtr> list{rootStruct};
        _getStructList(rootStruct, list);
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            for (auto j = std::next(i); j != list.end();)
            {
                if (i->get()->typeName == j->get()->typeName)
                {
                    j = list.erase(j);
                }
                else
                {
                    ++j;
                }
            }
        }
        return list;
    }

    static void generateLoops(INodePtr rootStruct, const std::string &rootStructName, std::ostream &str,
                              uint32_t indent, const std::string &rhsCode)
    {
        DE_ASSERT(rootStruct && rootStruct->getKind() == Kind::Struct);
        uint32_t iteratorSeed = 0;
        rootStruct->genLoops(str, rootStructName, rhsCode, 0, indent, iteratorSeed);
    }

    static void deserializeStruct(const void *src, INodePtr structNode, int nesting = -1)
    {
        DE_ASSERT(structNode && structNode->getKind() == Kind::Struct);
        size_t offset = 0;
        structNode->serializeOrDeserialize(src, nullptr, offset, false, nesting);
    }

    static void serializeStruct(INodePtr structNode, void *dst, int nesting = -1)
    {
        DE_ASSERT(structNode && structNode->getKind() == Kind::Struct);
        size_t offset = 0;
        structNode->serializeOrDeserialize(nullptr, dst, offset, true, nesting);
    }

private:
    static void _getStructList(INodePtr rootStruct, std::vector<INodePtr> &list)
    {
        INodePtr ch = rootStruct->getChildren(false);
        for (INodePtr p = ch->next; p; p = p->next)
        {
            if (INodePtr e = p->getElementType(); e && e->getChildren(false))
            {
                list.insert(list.begin(), e);
                _getStructList(e, list);
            }
            if (p->getChildren(false))
            {
                list.insert(list.begin(), p);
                _getStructList(p, list);
            }
        }
    }
};

} //namespace sg
using namespace sg;

class NestedUnsizedArraysTestInstance : public TestInstance
{
    INodePtr m_structure;
    const float m_seed;
    const uint32_t m_outerArrayLen;
    virtual tcu::TestStatus iterate() override;
    bool verify(const BufferWithMemory &buffer, const std::size_t stride, std::string &msg) const;

public:
    NestedUnsizedArraysTestInstance(Context &context, INodePtr structure, float seed, uint32_t outerArrayLen)
        : TestInstance(context)
        , m_structure(structure)
        , m_seed(seed)
        , m_outerArrayLen(outerArrayLen)
    {
    }
};

class NestedUnsizedArraysTestCase : public TestCase
{
    INodePtr m_structure;
    uint32_t m_outerArrayLen = 0u;
    int m_seed               = 17;

    virtual void delayedInit(void) override;
    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    INodePtr generateStructure(const float fseed, uint32_t &outerArrayLen, uint32_t &nestedArrayLen) const;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new NestedUnsizedArraysTestInstance(context, m_structure, float(m_seed), m_outerArrayLen);
    }

public:
    using TestCase::TestCase;
    static inline uint32_t guardZoneCount = 2u;
};

template <class T, class N, class P = T (*)[1], class R = decltype(std::begin(*std::declval<P>()))>
static auto makeStdBeginEnd(void *p, N &&n) -> std::pair<R, R>
{
    auto tmp   = std::begin(*P(p));
    auto begin = tmp;
    std::advance(tmp, std::forward<N>(n));
    return {begin, tmp};
}

template <class T, class N, class P = const T (*)[1], class R = decltype(std::cbegin(*std::declval<const P>()))>
auto makeStdBeginEnd(void const *p, N &&n) -> std::pair<R, R>
{
    auto tmp   = std::cbegin(*P(p));
    auto begin = tmp;
    std::advance(tmp, std::forward<N>(n));
    return {begin, tmp};
}

tcu::TestStatus NestedUnsizedArraysTestInstance::iterate()
{
    const DeviceInterface &di       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const std::size_t structSize = m_structure->getLogicalSize();
    const std::size_t descriptorArrayStride =
        deAlignSize(structSize, std::size_t(m_context.getDeviceProperties().limits.minStorageBufferOffsetAlignment));
    const uint32_t guardZoneCount     = NestedUnsizedArraysTestCase::guardZoneCount;
    const uint32_t descriptorArrayLen = guardZoneCount + m_outerArrayLen + guardZoneCount;
    const std::size_t bufferSize      = descriptorArrayStride * descriptorArrayLen;
    const VkBufferCreateInfo bci      = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory buffer(di, device, allocator, bci, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent));

    Move<VkDescriptorPool> dsPool = DescriptorPoolBuilder()
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorArrayLen)
                                        .build(di, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSetLayout> dsLayout =
        DescriptorSetLayoutBuilder()
            .addArrayBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorArrayLen, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(di, device);

    Move<VkDescriptorSet> ds = makeDescriptorSet(di, device, *dsPool, *dsLayout);

    VkDeviceSize descriptorArrayOffset = 0u;
    std::vector<VkDescriptorBufferInfo> updateInfos(descriptorArrayLen);
    for (uint32_t i = 0u; i < descriptorArrayLen; ++i)
    {
        updateInfos[i]        = makeDescriptorBufferInfo(*buffer, descriptorArrayOffset, descriptorArrayStride);
        descriptorArrayOffset = descriptorArrayOffset + descriptorArrayStride;
    }
    DescriptorSetUpdateBuilder()
        .writeArray(*ds, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    descriptorArrayLen, updateInfos.data())
        .update(di, device);

    struct PushConstant
    {
        float seed;
        int visits;
    } const pc{m_seed, int(m_structure->getVisitCount())};
    const VkPushConstantRange pcRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0u, uint32_t(sizeof(PushConstant))};
    Move<VkPipelineLayout> plLayout   = makePipelineLayout(di, device, *dsLayout, &pcRange);
    Move<VkShaderModule> module =
        createShaderModule(di, device, m_context.getBinaryCollection().get("comp_" + std::to_string(m_outerArrayLen)));
    Move<VkPipeline> pipeline = makeComputePipeline(di, device, *plLayout, *module);

    Move<VkCommandPool> cmdPool =
        createCommandPool(di, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmd = allocateCommandBuffer(di, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    auto range = makeStdBeginEnd<int>(buffer.getAllocation().getHostPtr(), (bufferSize / 4u));
    std::fill(range.first, range.second, 1);

    beginCommandBuffer(di, *cmd);
    {
        di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *plLayout, 0u, 1u, &ds.get(), 0u, nullptr);
        di.cmdPushConstants(*cmd, *plLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, pcRange.size, &pc);
        di.cmdDispatch(*cmd, 1u, 1u, 1u);
    }
    endCommandBuffer(di, *cmd);
    submitCommandsAndWait(di, device, queue, *cmd);

    std::string msg;
    const bool verdict = verify(buffer, descriptorArrayStride, msg);

    return verdict ? tcu::TestStatus::pass(msg) : tcu::TestStatus::fail(msg);
}

bool NestedUnsizedArraysTestInstance::verify(const BufferWithMemory &buffer, const std::size_t stride,
                                             std::string &msg) const
{
    const uint32_t guardZoneCount = NestedUnsizedArraysTestCase::guardZoneCount;
    DE_ASSERT((guardZoneCount + m_outerArrayLen + guardZoneCount) * stride <= buffer.getBufferSize());

    const std::byte *resultData = reinterpret_cast<const std::byte *>(buffer.getAllocation().getHostPtr());
    std::vector<std::byte> expectedData(uint32_t(buffer.getBufferSize()));

    const auto expectedRange = makeStdBeginEnd<int>(expectedData.data(), buffer.getBufferSize() / 4u);
    std::fill(expectedRange.first, expectedRange.second, 1);

    float seed         = m_seed;
    std::size_t offset = guardZoneCount * stride; // skip leading pad
    std::vector<INodePtr> result(m_outerArrayLen), expected(m_outerArrayLen);
    for (uint32_t i = 0u; i < m_outerArrayLen; ++i)
    {
        expected[i] = m_structure->clone();
        expected[i]->loop(seed);
        SG::serializeStruct(expected[i], (expectedData.data() + offset));

        result[i] = m_structure->clone();
        SG::deserializeStruct((resultData + offset), result[i]);

        offset = offset + stride;
    }

    DE_ASSERT((guardZoneCount + m_outerArrayLen) * stride == offset);

    const std::size_t intCount     = stride / 4u;
    const size_t dwordCount        = intCount * (m_outerArrayLen + 2u);
    const uint32_t *resultDWords   = reinterpret_cast<const uint32_t *>(resultData);
    const uint32_t *expectedDWords = reinterpret_cast<uint32_t *>(expectedData.data());

    for (uint32_t i = 0u; i < dwordCount; ++i)
    {
        if (expectedDWords[i] != resultDWords[i])
        {
            std::ostringstream os;
            tcu::TestLog &log = m_context.getTestContext().getLog();
            os << "Mismatch at dword " << i << std::hex;
            os << ", expected 0x" << expectedDWords[i];
            os << " got 0x" << resultDWords[i];
            log << tcu::TestLog::Message << os.str() << tcu::TestLog::EndMessage;
            msg = os.str();
            return false;
        }
    }

    return true;
}

void NestedUnsizedArraysTestCase::delayedInit()
{
    auto cm = getContextManager();
    if (cm)
    {
        if (const int tmp = cm->getCommandLine().getBaseSeed(); tmp)
        {
            m_seed = tmp;
        }
    }
    uint32_t nestedArrayLen = 0u;
    m_structure             = generateStructure(float(m_seed), m_outerArrayLen, nestedArrayLen);
}

INodePtr NestedUnsizedArraysTestCase::generateStructure(const float fseed, uint32_t &outerArrayLen,
                                                        uint32_t &nestedArrayLen) const
{
    const uint32_t uiseed = uint32_t(fseed);
    std::srand(uiseed);

    std::vector<INodePtr> types{SG::makeField(float()), SG::makeField(vec<3>()), SG::makeField(mat<2, 3>())};
    const uint32_t typesSize             = uint32_t(types.size());
    const uint32_t arraysSize            = 3u;
    const uint32_t structuresSize        = 2u;
    const uint32_t arrayOfStructuresSize = 3u;

    std::vector<INodePtr> arrays(arraysSize);
    for (uint32_t i = 0u; i < arraysSize; ++i)
    {
        const uint32_t arraySize = uint32_t(std::rand()) % typesSize;
        arrays[i]                = SG::makeArrayField(types[i % typesSize], (arraySize + 1u), false);
    }

    std::vector<INodePtr> structures(structuresSize);
    for (uint32_t i = 0u; i < structuresSize; ++i)
    {
        const uint32_t startT     = typesSize > 1u ? uint32_t(std::rand()) % (typesSize - 1u) : 0u;
        const uint32_t requestedT = (uint32_t(std::rand()) % typesSize) + 1u;
        const uint32_t endT       = std::min(startT + requestedT, typesSize);
        const uint32_t countT     = endT - startT;

        const uint32_t startA     = typesSize > 1u ? uint32_t(std::rand()) % (typesSize - 1u) : 0u;
        const uint32_t requestedA = (uint32_t(std::rand()) % typesSize) + 1u;
        const uint32_t endA       = std::min(startA + requestedA, typesSize);
        const uint32_t countA     = endA - startA;

        std::vector<INodePtr> fields(countT + countA);
        std::copy_n(std::next(types.begin(), startT), countT, fields.begin());
        std::copy_n(std::next(arrays.begin(), startA), countA, std::next(fields.begin(), countT));

        structures[i] = SG::generateStruct("S" + std::to_string(i), fields);
    }

    std::vector<INodePtr> array_of_structures(arrayOfStructuresSize);
    for (uint32_t i = 0u; i < arrayOfStructuresSize; ++i)
    {
        const uint32_t arraySize = uint32_t(std::rand()) % typesSize;
        array_of_structures[i]   = SG::makeArrayField(structures[i % structuresSize], (arraySize + 1u), false);
    }

    uint32_t allOffset = 0u;
    const uint32_t sourceSizes[]{arrayOfStructuresSize, structuresSize, arraysSize, typesSize};
    std::vector<INodePtr> *sources[]{&array_of_structures, &structures, &arrays, &types};
    std::vector<INodePtr> all(std::accumulate(std::begin(sourceSizes), std::end(sourceSizes), 0u));
    for (uint32_t i = 0u; i < DE_LENGTH_OF_ARRAY(sources); ++i)
    {
        std::copy_n(sources[i]->begin(), sourceSizes[i], std::next(all.begin(), allOffset));
        allOffset = allOffset + sourceSizes[i];
    }
    std::mt19937 rng(static_cast<std::mt19937::result_type>(std::rand()));
    std::shuffle(all.begin(), all.end(), rng);

    std::size_t biggestStructSize = 0u;
    uint32_t biggestStructIndex   = 0u;
    for (uint32_t i = 0u; i < structuresSize; ++i)
    {
        const std::size_t structSize = structures[i]->getLogicalSize();
        if (structSize > biggestStructSize)
        {
            biggestStructSize  = structSize;
            biggestStructIndex = i;
        }
    }

    outerArrayLen  = ((uint32_t(std::rand()) % typesSize) + 1u) * 4u;
    nestedArrayLen = ((uint32_t(std::rand()) % typesSize) + 1u) * 3u;
    INodePtr dyna  = SG::generateStruct("Root", all);
    INodePtr open  = SG::makeArrayField(structures[biggestStructIndex], nestedArrayLen, true);
    SG::structureAppendField(dyna, open);

    return dyna;
}

void NestedUnsizedArraysTestCase::checkSupport(Context &context) const
{
    const VkPhysicalDeviceVulkan12Features &f = context.getDeviceVulkan12Features();
    if (VK_FALSE == f.runtimeDescriptorArray)
    {
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported by device");
    }
    if (VK_FALSE == f.shaderStorageBufferArrayNonUniformIndexing)
    {
        TCU_THROW(NotSupportedError, "shaderStorageBufferArrayNonUniformIndexing not supported by device");
    }
}

void NestedUnsizedArraysTestCase::initPrograms(SourceCollections &programCollection) const
{
    const std::vector<INodePtr> list = SG::getStructList(m_structure);

    std::ostringstream code;
    code << "#version 450 core\n";
    code << "#extension GL_EXT_nonuniform_qualifier : require\n";
    code << "layout(local_size_x = " << m_outerArrayLen << ", local_size_y = 1, local_size_z = 1) in;\n";
    code << "layout(push_constant) uniform PC {\n";
    putIndent(code, 1);
    code << "float seed;\n";
    putIndent(code, 1);
    code << "int visits;\n";
    code << "} pc; \n";
    for (uint32_t s = 0; s < list.size() - 1; ++s)
    {
        SG::printStruct(list[s], code, true);
    }
    code << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
    SG::printStruct(list.back(), code, false);
    code << " root";
    code << "[/* " << guardZoneCount << " + " << m_outerArrayLen << " + " << guardZoneCount << " */]";
    code << ";\n";
    code << "void main() {\n";
    putIndent(code, 1);
    code << "float seed = pc.seed + gl_LocalInvocationID.x * pc.visits;\n";
    std::ostringstream rootAccess;
    rootAccess << "root[nonuniformEXT(gl_LocalInvocationID.x + " << guardZoneCount << ")]";
    SG::generateLoops(list.back(), rootAccess.str(), code, 1, "seed++");
    code << "}\n";

    programCollection.glslSources.add("comp_" + std::to_string(m_outerArrayLen)) << glu::ComputeSource(code.str());
}

} // unnamed namespace

void appendNestedUnsizedArraysTests(tcu::TestCaseGroup *testGroup)
{
    testGroup->addChild(new NestedUnsizedArraysTestCase(testGroup->getTestContext(), "nested_unsized_arrays"));
}

} // namespace ssbo
} // namespace vkt
