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
 * \brief Android utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidUtil.hpp"

namespace tcu
{
namespace Android
{

using std::string;

static string getIntentStringExtra (JNIEnv* env, jobject activity, const char* name)
{
	// \todo [2013-05-12 pyry] Clean up references on error.

	jclass	activityCls		= env->GetObjectClass(activity);
	jobject	intent			= env->CallObjectMethod(activity, env->GetMethodID(activityCls, "getIntent", "()Landroid/content/Intent;"));
	TCU_CHECK(intent);

	jstring	extraName		= env->NewStringUTF(name);
	jclass	intentCls		= env->GetObjectClass(intent);
	TCU_CHECK(extraName && intentCls);

	jvalue getExtraArgs[1];
	getExtraArgs[0].l = extraName;
	jstring extraStr = (jstring)env->CallObjectMethodA(intent, env->GetMethodID(intentCls, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;"), getExtraArgs);

	env->DeleteLocalRef(extraName);

	if (extraStr)
	{
		const char* ptr = env->GetStringUTFChars(extraStr, DE_NULL);
		string str = string(ptr);
		env->ReleaseStringUTFChars(extraStr, ptr);
		return str;
	}
	else
		return string();
}

string getIntentStringExtra (ANativeActivity* activity, const char* name)
{
	return getIntentStringExtra(activity->env, activity->clazz, name);
}

static void setRequestedOrientation (JNIEnv* env, jobject activity, ScreenOrientation orientation)
{
	jclass		activityCls			= env->GetObjectClass(activity);
	jmethodID	setOrientationId	= env->GetMethodID(activityCls, "setRequestedOrientation", "(I)V");

	env->CallVoidMethod(activity, setOrientationId, (int)orientation);
}

void setRequestedOrientation (ANativeActivity* activity, ScreenOrientation orientation)
{
	setRequestedOrientation(activity->env, activity->clazz, orientation);
}

ScreenOrientation mapScreenRotation (ScreenRotation rotation)
{
	switch (rotation)
	{
		case SCREENROTATION_UNSPECIFIED:	return SCREEN_ORIENTATION_UNSPECIFIED;
		case SCREENROTATION_0:				return SCREEN_ORIENTATION_PORTRAIT;
		case SCREENROTATION_90:				return SCREEN_ORIENTATION_LANDSCAPE;
		case SCREENROTATION_180:			return SCREEN_ORIENTATION_REVERSE_PORTRAIT;
		case SCREENROTATION_270:			return SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
		default:
			print("Warning: Unsupported rotation");
			return SCREEN_ORIENTATION_PORTRAIT;
	}
}

} // Android
} // tcu
