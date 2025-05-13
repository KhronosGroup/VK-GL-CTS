/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Program binary registry.
 *//*--------------------------------------------------------------------*/

#include "vkBinaryRegistry.hpp"
#include "tcuResource.hpp"
#include "tcuFormatUtil.hpp"
#include "deFilePath.hpp"
#include "deStringUtil.hpp"
#include "deDirectoryIterator.hpp"
#include "deString.h"
#include "deInt32.h"
#include "deFile.h"
#include "deMemory.h"

#include <sstream>
#include <fstream>
#include <stdexcept>
#include <limits>

namespace vk
{
namespace BinaryRegistryDetail
{

using std::string;
using std::vector;

namespace
{

string getProgramFileName(uint32_t index)
{
    return de::toString(tcu::toHex(index)) + ".spv";
}

string getProgramPath(const std::string &dirName, uint32_t index)
{
    return de::FilePath::join(dirName, getProgramFileName(index)).getPath();
}

bool isHexChr(char c)
{
    return de::inRange(c, '0', '9') || de::inRange(c, 'a', 'f') || de::inRange(c, 'A', 'F');
}

bool isProgramFileName(const std::string &name)
{
    // 0x + 00000000 + .spv
    if (name.length() != (2 + 8 + 4))
        return false;

    if (name[0] != '0' || name[1] != 'x' || name[10] != '.' || name[11] != 's' || name[12] != 'p' || name[13] != 'v')
        return false;

    for (size_t ndx = 2; ndx < 10; ++ndx)
    {
        if (!isHexChr(name[ndx]))
            return false;
    }

    return true;
}

uint32_t getProgramIndexFromName(const std::string &name)
{
    DE_ASSERT(isProgramFileName(name));

    uint32_t index = ~0u;
    std::stringstream str;

    str << std::hex << name.substr(2, 10);
    str >> index;

    DE_ASSERT(getProgramFileName(index) == name);

    return index;
}

string getIndexPath(const std::string &dirName)
{
    return de::FilePath::join(dirName, "index.bin").getPath();
}

void writeBinary(const ProgramBinary &binary, const std::string &dstPath)
{
    const de::FilePath filePath(dstPath);

    if (!de::FilePath(filePath.getDirName()).exists())
        de::createDirectoryAndParents(filePath.getDirName().c_str());

    {
        std::ofstream out(dstPath.c_str(), std::ios_base::binary);

        if (!out.is_open() || !out.good())
            throw tcu::Exception("Failed to open " + dstPath);

        out.write((const char *)binary.getBinary(), binary.getSize());
        out.close();
    }
}

void writeBinary(const std::string &dstDir, uint32_t index, const ProgramBinary &binary)
{
    writeBinary(binary, getProgramPath(dstDir, index));
}

ProgramBinary *readBinary(const std::string &srcPath)
{
    std::ifstream in(srcPath.c_str(), std::ios::binary | std::ios::ate);
    const size_t size = (size_t)in.tellg();

    if (!in.is_open() || !in.good())
        throw tcu::Exception("Failed to open " + srcPath);

    if (size == 0)
        throw tcu::Exception("Malformed binary, size = 0");

    in.seekg(0, std::ios::beg);

    {
        std::vector<uint8_t> bytes(size);

        in.read((char *)&bytes[0], size);
        DE_ASSERT(bytes[0] != 0);

        return new ProgramBinary(vk::PROGRAM_FORMAT_SPIRV, bytes.size(), &bytes[0]);
    }
}

uint32_t binaryHash(const ProgramBinary *binary)
{
    return deMemoryHash(binary->getBinary(), binary->getSize());
}

bool binaryEqual(const ProgramBinary *a, const ProgramBinary *b)
{
    return a->getSize() == b->getSize() && memcmp(a->getBinary(), b->getBinary(), a->getSize()) == 0;
}

std::vector<uint32_t> getSearchPath(const ProgramIdentifier &id)
{
    const std::string combinedStr = id.testCasePath + '#' + id.programName;
    const size_t strLen           = combinedStr.size();
    const size_t numWords         = strLen / 4 + 1; // Must always end up with at least one 0 byte
    vector<uint32_t> words(numWords, 0u);

    deMemcpy(&words[0], combinedStr.c_str(), strLen);

    return words;
}

const uint32_t *findBinaryIndex(BinaryIndexAccess *index, const ProgramIdentifier &id)
{
    const vector<uint32_t> words = getSearchPath(id);
    size_t nodeNdx               = 0;
    size_t wordNdx               = 0;

    for (;;)
    {
        const BinaryIndexNode &curNode = (*index)[nodeNdx];

        if (curNode.word == words[wordNdx])
        {
            if (wordNdx + 1 < words.size())
            {
                TCU_CHECK_INTERNAL((size_t)curNode.index < index->size());

                nodeNdx = curNode.index;
                wordNdx += 1;
            }
            else if (wordNdx + 1 == words.size())
                return &curNode.index;
            else
                return nullptr;
        }
        else if (curNode.word != 0)
        {
            nodeNdx += 1;

            // Index should always be null-terminated
            TCU_CHECK_INTERNAL(nodeNdx < index->size());
        }
        else
            return nullptr;
    }

    return nullptr;
}

//! Sparse index node used for final binary index construction
struct SparseIndexNode
{
    uint32_t word;
    uint32_t index;
    std::vector<SparseIndexNode *> children;

    SparseIndexNode(uint32_t word_, uint32_t index_) : word(word_), index(index_)
    {
    }

    SparseIndexNode(void) : word(0), index(0)
    {
    }

    ~SparseIndexNode(void)
    {
        for (size_t ndx = 0; ndx < children.size(); ndx++)
            delete children[ndx];
    }
};

#if defined(DE_DEBUG)
bool isNullByteTerminated(uint32_t word)
{
    uint8_t bytes[4];
    deMemcpy(bytes, &word, sizeof(word));
    return bytes[3] == 0;
}
#endif

void addToSparseIndex(SparseIndexNode *group, const uint32_t *words, size_t numWords, uint32_t index)
{
    const uint32_t curWord = words[0];
    SparseIndexNode *child = nullptr;

    for (size_t childNdx = 0; childNdx < group->children.size(); childNdx++)
    {
        if (group->children[childNdx]->word == curWord)
        {
            child = group->children[childNdx];
            break;
        }
    }

    DE_ASSERT(numWords > 1 || !child);

    if (!child)
    {
        group->children.reserve(group->children.size() + 1);
        group->children.push_back(new SparseIndexNode(curWord, numWords == 1 ? index : 0));

        child = group->children.back();
    }

    if (numWords > 1)
        addToSparseIndex(child, words + 1, numWords - 1, index);
    else
        DE_ASSERT(isNullByteTerminated(curWord));
}

// Prepares sparse index for finalization. Ensures that child with word = 0 is moved
// to the end, or one is added if there is no such child already.
void normalizeSparseIndex(SparseIndexNode *group)
{
    int zeroChildPos = -1;

    for (size_t childNdx = 0; childNdx < group->children.size(); childNdx++)
    {
        normalizeSparseIndex(group->children[childNdx]);

        if (group->children[childNdx]->word == 0)
        {
            DE_ASSERT(zeroChildPos < 0);
            zeroChildPos = (int)childNdx;
        }
    }

    if (zeroChildPos >= 0)
    {
        // Move child with word = 0 to last
        while (zeroChildPos != (int)group->children.size() - 1)
        {
            std::swap(group->children[zeroChildPos], group->children[zeroChildPos + 1]);
            zeroChildPos += 1;
        }
    }
    else if (!group->children.empty())
    {
        group->children.reserve(group->children.size() + 1);
        group->children.push_back(new SparseIndexNode(0, 0));
    }
}

uint32_t getIndexSize(const SparseIndexNode *group)
{
    size_t numNodes = group->children.size();

    for (size_t childNdx = 0; childNdx < group->children.size(); childNdx++)
        numNodes += getIndexSize(group->children[childNdx]);

    DE_ASSERT(numNodes <= std::numeric_limits<uint32_t>::max());

    return (uint32_t)numNodes;
}

uint32_t addAndCountNodes(BinaryIndexNode *index, uint32_t baseOffset, const SparseIndexNode *group)
{
    const uint32_t numLocalNodes = (uint32_t)group->children.size();
    uint32_t curOffset           = numLocalNodes;

    // Must be normalized prior to construction of final index
    DE_ASSERT(group->children.empty() || group->children.back()->word == 0);

    for (size_t childNdx = 0; childNdx < numLocalNodes; childNdx++)
    {
        const SparseIndexNode *child = group->children[childNdx];
        const uint32_t subtreeSize   = addAndCountNodes(index + curOffset, baseOffset + curOffset, child);

        index[childNdx].word = child->word;

        if (subtreeSize == 0)
            index[childNdx].index = child->index;
        else
        {
            DE_ASSERT(child->index == 0);
            index[childNdx].index = baseOffset + curOffset;
        }

        curOffset += subtreeSize;
    }

    return curOffset;
}

void buildFinalIndex(std::vector<BinaryIndexNode> *dst, const SparseIndexNode *root)
{
    const uint32_t indexSize = getIndexSize(root);

    if (indexSize > 0)
    {
        dst->resize(indexSize);
        addAndCountNodes(&(*dst)[0], 0, root);
    }
    else
    {
        // Generate empty index
        dst->resize(1);
        (*dst)[0].word  = 0u;
        (*dst)[0].index = 0u;
    }
}

void buildBinaryIndex(std::vector<BinaryIndexNode> *dst, size_t numEntries, const ProgramIdentifierIndex *entries)
{
    de::UniquePtr<SparseIndexNode> sparseIndex(new SparseIndexNode());

    for (size_t ndx = 0; ndx < numEntries; ndx++)
    {
        const std::vector<uint32_t> searchPath = getSearchPath(entries[ndx].id);
        addToSparseIndex(sparseIndex.get(), &searchPath[0], searchPath.size(), entries[ndx].index);
    }

    normalizeSparseIndex(sparseIndex.get());
    buildFinalIndex(dst, sparseIndex.get());
}

} // namespace

// BinaryIndexHash

DE_IMPLEMENT_POOL_HASH(BinaryIndexHashImpl, const ProgramBinary *, uint32_t, binaryHash, binaryEqual);

BinaryIndexHash::BinaryIndexHash(void) : m_hash(BinaryIndexHashImpl_create(m_memPool.getRawPool()))
{
    if (!m_hash)
        throw std::bad_alloc();
}

BinaryIndexHash::~BinaryIndexHash(void)
{
}

uint32_t *BinaryIndexHash::find(const ProgramBinary *binary) const
{
    return BinaryIndexHashImpl_find(m_hash, binary);
}

void BinaryIndexHash::insert(const ProgramBinary *binary, uint32_t index)
{
    if (!BinaryIndexHashImpl_insert(m_hash, binary, index))
        throw std::bad_alloc();
}

// BinaryRegistryWriter

BinaryRegistryWriter::BinaryRegistryWriter(const std::string &dstPath) : m_dstPath(dstPath)
{
    if (de::FilePath(dstPath).exists())
        initFromPath(dstPath);
}

BinaryRegistryWriter::~BinaryRegistryWriter(void)
{
    for (BinaryVector::const_iterator binaryIter = m_binaries.begin(); binaryIter != m_binaries.end(); ++binaryIter)
        delete binaryIter->binary;
}

void BinaryRegistryWriter::initFromPath(const std::string &srcPath)
{
    DE_ASSERT(m_binaries.empty());

    for (de::DirectoryIterator iter(srcPath); iter.hasItem(); iter.next())
    {
        const de::FilePath path    = iter.getItem();
        const std::string baseName = path.getBaseName();

        if (isProgramFileName(baseName))
        {
            const uint32_t index = getProgramIndexFromName(baseName);
            const de::UniquePtr<ProgramBinary> binary(readBinary(path.getPath()));

            addBinary(index, *binary);
            // \note referenceCount is left to 0 and will only be incremented
            //         if binary is reused (added via addProgram()).
        }
    }
}

void BinaryRegistryWriter::addProgram(const ProgramIdentifier &id, const ProgramBinary &binary)
{
    const uint32_t *const indexPtr = findBinary(binary);
    uint32_t index                 = indexPtr ? *indexPtr : ~0u;

    if (!indexPtr)
    {
        index = getNextSlot();
        addBinary(index, binary);
    }

    m_binaries[index].referenceCount += 1;
    m_binaryIndices.push_back(ProgramIdentifierIndex(id, index));
}

uint32_t *BinaryRegistryWriter::findBinary(const ProgramBinary &binary) const
{
    return m_binaryHash.find(&binary);
}

uint32_t BinaryRegistryWriter::getNextSlot(void) const
{
    const uint32_t index = (uint32_t)m_binaries.size();

    if ((size_t)index != m_binaries.size())
        throw std::bad_alloc(); // Overflow

    return index;
}

void BinaryRegistryWriter::addBinary(uint32_t index, const ProgramBinary &binary)
{
    DE_ASSERT(binary.getFormat() == vk::PROGRAM_FORMAT_SPIRV);
    DE_ASSERT(findBinary(binary) == nullptr);

    ProgramBinary *const binaryClone = new ProgramBinary(binary);

    try
    {
        if (m_binaries.size() < (size_t)index + 1)
            m_binaries.resize(index + 1);

        DE_ASSERT(!m_binaries[index].binary);
        DE_ASSERT(m_binaries[index].referenceCount == 0);

        m_binaries[index].binary = binaryClone;
        // \note referenceCount is not incremented here
    }
    catch (...)
    {
        delete binaryClone;
        throw;
    }

    m_binaryHash.insert(binaryClone, index);
}

void BinaryRegistryWriter::write(void) const
{
    writeToPath(m_dstPath);
}

void BinaryRegistryWriter::writeToPath(const std::string &dstPath) const
{
    if (!de::FilePath(dstPath).exists())
        de::createDirectoryAndParents(dstPath.c_str());

    DE_ASSERT(m_binaries.size() <= 0xffffffffu);
    for (size_t binaryNdx = 0; binaryNdx < m_binaries.size(); ++binaryNdx)
    {
        const BinarySlot &slot = m_binaries[binaryNdx];

        if (slot.referenceCount > 0)
        {
            DE_ASSERT(slot.binary);
            writeBinary(dstPath, (uint32_t)binaryNdx, *slot.binary);
        }
        else
        {
            // Delete stale binary if such exists
            const std::string progPath = getProgramPath(dstPath, (uint32_t)binaryNdx);

            if (de::FilePath(progPath).exists())
                deDeleteFile(progPath.c_str());
        }
    }

    // Write index
    {
        const de::FilePath indexPath = getIndexPath(dstPath);
        std::vector<BinaryIndexNode> index;

        buildBinaryIndex(&index, m_binaryIndices.size(), !m_binaryIndices.empty() ? &m_binaryIndices[0] : nullptr);

        // Even in empty index there is always terminating node for the root group
        DE_ASSERT(!index.empty());

        if (!de::FilePath(indexPath.getDirName()).exists())
            de::createDirectoryAndParents(indexPath.getDirName().c_str());

        {
            std::ofstream indexOut(indexPath.getPath(), std::ios_base::binary);

            if (!indexOut.is_open() || !indexOut.good())
                throw tcu::InternalError(string("Failed to open program binary index file ") + indexPath.getPath());

            indexOut.write((const char *)&index[0], index.size() * sizeof(BinaryIndexNode));
        }
    }
}

// BinaryRegistryReader

BinaryRegistryReader::BinaryRegistryReader(const tcu::Archive &archive, const std::string &srcPath)
    : m_archive(archive)
    , m_srcPath(srcPath)
{
}

BinaryRegistryReader::~BinaryRegistryReader(void)
{
}

ProgramBinary *BinaryRegistryReader::loadProgram(const ProgramIdentifier &id) const
{
    if (!m_binaryIndex)
    {
        try
        {
            m_binaryIndex = BinaryIndexPtr(new BinaryIndexAccess(
                de::MovePtr<tcu::Resource>(m_archive.getResource(getIndexPath(m_srcPath).c_str()))));
        }
        catch (const tcu::ResourceError &e)
        {
            throw ProgramNotFoundException(id, string("Failed to open binary index (") + e.what() + ")");
        }
    }

    {
        const uint32_t *indexPos = findBinaryIndex(m_binaryIndex.get(), id);

        if (indexPos)
        {
            const string fullPath = getProgramPath(m_srcPath, *indexPos);

            try
            {
                de::UniquePtr<tcu::Resource> progRes(m_archive.getResource(fullPath.c_str()));
                const int progSize = progRes->getSize();
                vector<uint8_t> bytes(progSize);

                TCU_CHECK_INTERNAL(!bytes.empty());

                progRes->read(&bytes[0], progSize);

                return new ProgramBinary(vk::PROGRAM_FORMAT_SPIRV, bytes.size(), &bytes[0]);
            }
            catch (const tcu::ResourceError &e)
            {
                throw ProgramNotFoundException(id, e.what());
            }
        }
        else
            throw ProgramNotFoundException(id, "Program not found in index");
    }
}

} // namespace BinaryRegistryDetail
} // namespace vk
