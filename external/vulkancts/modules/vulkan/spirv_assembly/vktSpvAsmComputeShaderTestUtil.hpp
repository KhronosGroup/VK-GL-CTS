#ifndef _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
#define _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
