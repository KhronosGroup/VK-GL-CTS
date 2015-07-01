/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
