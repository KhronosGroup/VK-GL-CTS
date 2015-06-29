/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Program binary registry.
 *//*--------------------------------------------------------------------*/

#include "vkBinaryRegistry.hpp"
#include "tcuResource.hpp"
#include "deFilePath.hpp"

#include <fstream>

namespace vk
{

using std::string;
using std::vector;

static string getProgramFileName (const ProgramIdentifier& id)
{
	// \todo [2015-06-26 pyry] Sanitize progName
	return id.testCasePath + "." + id.programName + ".spirv";
}

// BinaryRegistryWriter

BinaryRegistryWriter::BinaryRegistryWriter (const std::string& dstPath)
	: m_dstPath(dstPath)
{
}

BinaryRegistryWriter::~BinaryRegistryWriter (void)
{
}

void BinaryRegistryWriter::storeProgram (const ProgramIdentifier& id, const ProgramBinary& binary)
{
	const string	fullPath	= de::FilePath::join(m_dstPath, getProgramFileName(id)).getPath();
	std::ofstream	out			(fullPath.c_str(), std::ios_base::binary);

	if (!out.is_open() || !out.good())
		throw tcu::Exception("Failed to open " + fullPath);

	out.write((const char*)binary.getBinary(), binary.getSize());
	out.close();
}

// BinaryRegistryReader

BinaryRegistryReader::BinaryRegistryReader (const tcu::Archive& archive, const std::string& srcPath)
	: m_archive	(archive)
	, m_srcPath	(srcPath)
{
}

BinaryRegistryReader::~BinaryRegistryReader (void)
{
}

ProgramBinary* BinaryRegistryReader::loadProgram (const ProgramIdentifier& id) const
{
	const string	fullPath	= de::FilePath::join(m_srcPath, getProgramFileName(id)).getPath();

	try
	{
		de::UniquePtr<tcu::Resource>	progRes		(m_archive.getResource(fullPath.c_str()));
		const int						progSize	= progRes->getSize();
		vector<deUint8>					bytes		(progSize);

		TCU_CHECK_INTERNAL(!bytes.empty());

		progRes->read(&bytes[0], progSize);

		return new ProgramBinary(vk::PROGRAM_FORMAT_SPIRV, bytes.size(), &bytes[0]);
	}
	catch (const tcu::ResourceError&)
	{
		throw ProgramNotFoundException(id);
	}
}


} // vk
