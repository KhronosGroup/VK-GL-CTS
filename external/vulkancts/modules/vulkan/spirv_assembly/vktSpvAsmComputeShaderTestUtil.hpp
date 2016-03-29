#ifndef _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
#define _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
 * \brief Compute Shader Based Test Case Utility Structs/Functions
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deSharedPtr.hpp"
#include "tcuVector.hpp"
#include "vkMemUtil.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace SpirVAssembly
{

typedef de::MovePtr<vk::Allocation>			AllocationMp;
typedef de::SharedPtr<vk::Allocation>		AllocationSp;

/*--------------------------------------------------------------------*//*!
 * \brief Abstract class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
class BufferInterface
{
public:
	virtual				~BufferInterface	(void)				{}

	virtual size_t		getNumBytes			(void) const = 0;
	virtual const void*	data				(void) const = 0;
};

typedef de::SharedPtr<BufferInterface>		BufferSp;

/*--------------------------------------------------------------------*//*!
 * \brief Concrete class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
template<typename E>
class Buffer : public BufferInterface
{
public:
						Buffer				(const std::vector<E>& elements)
							: m_elements(elements)
						{}

	size_t				getNumBytes			(void) const		{ return m_elements.size() * sizeof(E); }
	const void*			data				(void) const		{ return &m_elements.front(); }

private:
	std::vector<E>		m_elements;
};

DE_STATIC_ASSERT(sizeof(tcu::Vec4) == 4 * sizeof(float));

typedef Buffer<float>		Float32Buffer;
typedef Buffer<deInt32>		Int32Buffer;
typedef Buffer<tcu::Vec4>	Vec4Buffer;


/*--------------------------------------------------------------------*//*!
 * \brief Specification for a compute shader.
 *
 * This struct bundles SPIR-V assembly code, input and expected output
 * together.
 *//*--------------------------------------------------------------------*/
struct ComputeShaderSpec
{
	std::string				assembly;
	std::string				entryPoint;
	std::vector<BufferSp>	inputs;
	std::vector<BufferSp>	outputs;
	tcu::IVec3				numWorkGroups;
	std::vector<deUint32>	specConstants;
	// If null, a default verification will be performed by comparing the memory pointed to by outputAllocations
	// and the contents of expectedOutputs. Otherwise the function pointed to by verifyIO will be called.
	// If true is returned, then the test case is assumed to have passed, if false is returned, then the test
	// case is assumed to have failed.
	bool					(*verifyIO)(const std::vector<BufferSp>& inputs, const std::vector<AllocationSp>& outputAllocations, const std::vector<BufferSp>& expectedOutputs);

							ComputeShaderSpec()
								: entryPoint	("main")
								, verifyIO		(DE_NULL)
							{}

};

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
