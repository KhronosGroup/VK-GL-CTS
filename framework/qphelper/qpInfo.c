/*-------------------------------------------------------------------------
 * drawElements Quality Program Helper Library
 * -------------------------------------------
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
 * \brief Version and platform info.
 *//*--------------------------------------------------------------------*/

#include "qpInfo.h"

#include <time.h>

DE_BEGIN_EXTERN_C

#define DEQP_RELEASE_NAME	"2014.x"
#define DEQP_RELEASE_ID		0xcafebabe
#define DEQP_LICENSE_BEGIN	0
#define DEQP_LICENSE_END	0xffffffffu

const char* qpGetTargetName (void)
{
#if defined(DEQP_TARGET_NAME)
	return DEQP_TARGET_NAME;
#else
#	error DEQP_TARGET_NAME is not defined!
#endif
}

const char* qpGetReleaseName (void)
{
	return DEQP_RELEASE_NAME;
}

deUint32 qpGetReleaseId (void)
{
	return DEQP_RELEASE_ID;
}

deBool qpIsLicenseValid (void)
{
	deUint64	begin	= (deUint64)DEQP_LICENSE_BEGIN;
	deUint64	end		= (deUint64)DEQP_LICENSE_END;
	deUint64	curTime	= (deUint64)time(DE_NULL);

	return begin <= curTime && curTime <= end;
}

DE_END_EXTERN_C
