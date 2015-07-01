#ifndef _VKBINARYREGISTRY_HPP
#define _VKBINARYREGISTRY_HPP
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
