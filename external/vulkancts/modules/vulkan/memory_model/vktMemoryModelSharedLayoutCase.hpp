#ifndef _VKTMEMORYMODELSHAREDLAYOUTCASE_HPP
#define _VKTMEMORYMODELSHAREDLAYOUTCASE_HPP
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
 * \brief Shared memory layout tests.
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "tcuDefs.hpp"
#include "gluShaderUtil.hpp"
#include "gluVarType.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"

#include <vector>

namespace vkt
{
namespace MemoryModel
{
typedef de::SharedPtr<glu::StructType> NamedStructSP;

struct SharedStructVarEntry
{
	SharedStructVarEntry(glu::DataType type_, int arraySize_)
		: type(type_), arraySize(arraySize_) {}

	glu::DataType	type;
	int				arraySize;
};

struct SharedStructVar
{
	std::string							name;
	glu::VarType						type;
	int									arraySize;
	int									topLevelArraySize;
	std::vector<SharedStructVarEntry>	entries;

	// Contains all the values assigned to the variable.
	std::vector<std::string>			entryValues;
};

class SharedStruct
{
public:
	typedef std::vector<SharedStructVar>::iterator				iterator;
	typedef std::vector<SharedStructVar>::const_iterator		const_iterator;

								SharedStruct			(const std::string name, const std::string instanceName)
									: m_name(name), m_instanceName(instanceName) {}

	const std::string			getName					(void) const			{ return m_name; }
	const std::string			getInstanceName			(void) const			{ return m_instanceName; }

	void						addMember				(SharedStructVar var)	{ m_members.push_back(var); }
	int							getNumMembers			(void)					{ return static_cast<int>(m_members.size()); }

	inline iterator				begin					(void)					{ return m_members.begin(); }
	inline const_iterator		begin					(void) const			{ return m_members.begin(); }
	inline iterator				end						(void)					{ return m_members.end(); }
	inline const_iterator		end						(void) const			{ return m_members.end(); }

private:
	// Shared struct name
	std::string						m_name;

	// Shared struct instance name
	std::string						m_instanceName;

	// Contains the members of this struct.
	std::vector<SharedStructVar>	m_members;
};

class ShaderInterface
{
public:
										ShaderInterface		(void) {}
										~ShaderInterface	(void) {}

	SharedStruct&						allocSharedObject	(const std::string& name, const std::string& instanceName);
	NamedStructSP						allocStruct			(const std::string& name);

	std::vector<NamedStructSP>&			getStructs			(void)			{ return m_structs; }
	int									getNumStructs		(void)			{ return static_cast<int>(m_structs.size()); }

	int									getNumSharedObjects	(void) const	{ return static_cast<int>(m_sharedMemoryObjects.size()); }
	std::vector<SharedStruct>&			getSharedObjects	(void)			{ return m_sharedMemoryObjects; }
	const std::vector<SharedStruct>&	getSharedObjects	(void) const	{ return m_sharedMemoryObjects; }

	void								enable8BitTypes		(bool enabled)	{ m_8BitTypesEnabled = enabled; }
	void								enable16BitTypes	(bool enabled)	{ m_16BitTypesEnabled = enabled; }
	bool								is8BitTypesEnabled	(void) const	{ return m_8BitTypesEnabled; }
	bool								is16BitTypesEnabled	(void) const	{ return m_16BitTypesEnabled; }
private:
										ShaderInterface		(const ShaderInterface&);
	ShaderInterface&					operator=			(const ShaderInterface&);

	std::vector<NamedStructSP>			m_structs;
	std::vector<SharedStruct>			m_sharedMemoryObjects;
	bool								m_8BitTypesEnabled;
	bool								m_16BitTypesEnabled;
};

class SharedLayoutCaseInstance : public TestInstance
{
public:
			SharedLayoutCaseInstance(Context& context)
				: TestInstance(context) {}
	virtual ~SharedLayoutCaseInstance(void) {}
	virtual tcu::TestStatus iterate(void);
};

class SharedLayoutCase : public vkt::TestCase
{
public:
							SharedLayoutCase	(tcu::TestContext& testCtx, const char* name, const char* description)
								: TestCase(testCtx, name, description) {}
	virtual					~SharedLayoutCase	(void) {}
	virtual	void			delayedInit			(void);
	virtual	void			initPrograms		(vk::SourceCollections& programCollection) const;
	virtual	TestInstance*	createInstance		(Context& context) const;
	virtual void			checkSupport		(Context& context) const;

protected:
	ShaderInterface			m_interface;
	std::string				m_computeShaderSrc;

private:
							SharedLayoutCase	(const SharedLayoutCase&);
	SharedLayoutCase&		operator=			(const SharedLayoutCase&);
};

class RandomSharedLayoutCase : public SharedLayoutCase
{
public:
	RandomSharedLayoutCase		(tcu::TestContext& testCtx, const char* name, const char* description,
								deUint32 features, deUint32 seed);

private:
	void			generateSharedMemoryObject	(de::Random& rnd);
	void			generateSharedMemoryVar		(de::Random& rnd, SharedStruct& object);
	glu::VarType	generateType				(de::Random& rnd, int typeDepth, bool arrayOk);

	deUint32		m_features;
	int				m_maxArrayLength;
	deUint32		m_seed;

	const int		m_maxSharedObjects			= 3;
	const int		m_maxSharedObjectMembers	= 4;
	const int		m_maxStructMembers			= 3;
};

} // MemoryModel
} // vkt

#endif // _VKTMEMORYMODELSHAREDLAYOUTCASE_HPP
