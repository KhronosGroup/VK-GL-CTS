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
 * \brief EGL extension resolving.
 *//*--------------------------------------------------------------------*/

#include "egluExtensions.hpp"

#include "egluUtil.hpp"
#include "tcuDefs.hpp"


namespace eglu
{

deFunctionPtr getProcAddressChecked (const char* procName)
{
	const deFunctionPtr func = eglGetProcAddress(procName);

	TCU_CHECK_AND_THROW(NotSupportedError, func, procName);

	return func;
}

ImageFunctions getImageFunctions (EGLDisplay dpy)
{
	ImageFunctions ret;

	if (getVersion(dpy) >= Version(1, 5))
	{
		ret.createImage = getFunction<PFNEGLCREATEIMAGEKHRPROC>("eglCreateImage");
		ret.destroyImage = getFunction<PFNEGLDESTROYIMAGEKHRPROC>("eglDestroyImage");
	}
	else if (hasExtension(dpy, "EGL_KHR_image_base"))
	{
		ret.createImage = getFunction<PFNEGLCREATEIMAGEKHRPROC>("eglCreateImageKHR");
		ret.destroyImage = getFunction<PFNEGLDESTROYIMAGEKHRPROC>("eglDestroyImageKHR");
	}
	else
		TCU_THROW(NotSupportedError, "EGLImages are not supported");

	return ret;
}

}
