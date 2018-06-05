#ifndef _TCUOSXMETALVIEW_HPP
#define _TCUOSXMETALVIEW_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2018 The Android Open Source Project
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
 * \brief VK_MVK_macos_surface compatible view
 *//*--------------------------------------------------------------------*/

namespace tcu
{
namespace osx
{

class MetalView
{
public:
				MetalView			(int width, int height);
				~MetalView			(void);

	void		setSize				(int width, int height);

	void*		getView				(void) const { return m_view;	}

private:
				MetalView			(const MetalView&);
	MetalView	operator=			(const MetalView&);

	void*		m_view;
};

} // osx
} // tcu

#endif // _TCUOSXMETALVIEW_HPP
