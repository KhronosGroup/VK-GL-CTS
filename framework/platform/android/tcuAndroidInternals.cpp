/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Access to Android internals that are not a part of the NDK.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidInternals.hpp"

namespace tcu
{
namespace Android
{
namespace internal
{

using std::string;
using de::DynamicLibrary;

template<typename Func>
void setFuncPtr (Func*& funcPtr, DynamicLibrary& lib, const string& symname)
{
	funcPtr = reinterpret_cast<Func*>(lib.getFunction(symname.c_str()));
	if (!funcPtr)
		TCU_THROW(NotSupportedError, ("Unable to look up symbol from shared object: " + symname).c_str());
}

LibUI::LibUI (void)
	: m_library	("libui.so")
{
	GraphicBufferFunctions& gb = m_functions.graphicBuffer;

	setFuncPtr(gb.constructor,		m_library,	"_ZN7android13GraphicBufferC1Ejjij");
	setFuncPtr(gb.destructor,		m_library,	"_ZN7android13GraphicBufferD1Ev");
	setFuncPtr(gb.getNativeBuffer,	m_library,	"_ZNK7android13GraphicBuffer15getNativeBufferEv");
	setFuncPtr(gb.lock,				m_library,	"_ZN7android13GraphicBuffer4lockEjPPv");
	setFuncPtr(gb.unlock,			m_library,	"_ZN7android13GraphicBuffer6unlockEv");
}

#define GRAPHICBUFFER_SIZE 1024 // Hopefully enough

GraphicBuffer::GraphicBuffer (const LibUI& lib, deUint32 width, deUint32 height, PixelFormat format, deUint32 usage)
	: m_functions	(lib.getFunctions().graphicBuffer)
	, m_memory		(GRAPHICBUFFER_SIZE) // vector<char> (new char[]) is max-aligned
	, m_impl		(m_functions.constructor(&m_memory.front(), width, height, format, usage))
{
}

GraphicBuffer::~GraphicBuffer (void)
{
	m_functions.destructor(m_impl);
}

status_t GraphicBuffer::lock (deUint32 usage, void** vaddr)
{
	return m_functions.lock(m_impl, usage, vaddr);
}

status_t GraphicBuffer::unlock (void)
{
	return m_functions.unlock(m_impl);
}

ANativeWindowBuffer* GraphicBuffer::getNativeBuffer (void) const
{
	return m_functions.getNativeBuffer(m_impl);
}

} // internal
} // Android
} // tcu
