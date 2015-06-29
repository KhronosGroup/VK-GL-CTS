#ifndef _VKBINARYREGISTRY_HPP
#define _VKBINARYREGISTRY_HPP
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

#include "vkDefs.hpp"
#include "vkPrograms.hpp"

namespace tcu
{
class Archive;
}

namespace vk
{

struct ProgramIdentifier
{
	std::string		testCasePath;
	std::string		programName;

	ProgramIdentifier (const std::string& testCasePath_, const std::string& programName_)
		: testCasePath	(testCasePath_)
		, programName	(programName_)
	{
	}
};

class ProgramNotFoundException : public tcu::ResourceError
{
public:
	ProgramNotFoundException (const ProgramIdentifier& id)
		: tcu::ResourceError("Program " + id.testCasePath + " / '" + id.programName + "' not found")
	{
	}
};

class BinaryRegistryReader
{
public:
						BinaryRegistryReader	(const tcu::Archive& archive, const std::string& srcPath);
						~BinaryRegistryReader	(void);

	ProgramBinary*		loadProgram				(const ProgramIdentifier& id) const;

private:
	const tcu::Archive&	m_archive;
	const std::string&	m_srcPath;
};

class BinaryRegistryWriter
{
public:
						BinaryRegistryWriter	(const std::string& dstPath);
						~BinaryRegistryWriter	(void);

	void				storeProgram			(const ProgramIdentifier& id, const ProgramBinary& binary);

private:
	const std::string	m_dstPath;
};

} // vk

#endif // _VKBINARYREGISTRY_HPP
