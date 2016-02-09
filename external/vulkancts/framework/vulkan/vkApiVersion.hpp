#ifndef _VKAPIVERSION_HPP
#define _VKAPIVERSION_HPP
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
 * \brief Vulkan api version.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include <ostream>

namespace vk
{

struct ApiVersion
{
	deUint32	majorNum;
	deUint32	minorNum;
	deUint32	patchNum;

	ApiVersion (deUint32	majorNum_,
				deUint32	minorNum_,
				deUint32	patchNum_)
		: majorNum	(majorNum_)
		, minorNum	(minorNum_)
		, patchNum	(patchNum_)
	{
	}
};

ApiVersion		unpackVersion		(deUint32 version);
deUint32		pack				(const ApiVersion& version);

inline std::ostream& operator<< (std::ostream& s, const ApiVersion& version)
{
	return s << version.majorNum << "." << version.minorNum << "." << version.patchNum;
}

} // vk

#endif // _VKAPIVERSION_HPP
