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

#include <vector>

namespace tcu
{
namespace Android
{

using std::string;
using std::vector;

void checkJNIException (JNIEnv* env)
{
	if (env->ExceptionCheck())
	{
		env->ExceptionDescribe();
		env->ExceptionClear();
		throw std::runtime_error("Got JNI exception");
	}
}

string getJNIStringValue (JNIEnv* env, jstring jniStr)
{
	const char*		ptr		= env->GetStringUTFChars(jniStr, DE_NULL);
	const string	str		= string(ptr);

	env->ReleaseStringUTFChars(jniStr, ptr);

	return str;
}

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
		return getJNIStringValue(env, extraStr);
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

template<typename Type>
const char* getJNITypeStr (void);

template<>
const char* getJNITypeStr<int> (void)
{
	return "I";
}

template<>
const char* getJNITypeStr<string> (void)
{
	return "Ljava/lang/String;";
}

template<>
const char* getJNITypeStr<vector<string> > (void)
{
	return "[Ljava/lang/String;";
}

template<typename FieldType>
FieldType getStaticFieldValue (JNIEnv* env, jclass cls, jfieldID fieldId);

template<>
int getStaticFieldValue<int> (JNIEnv* env, jclass cls, jfieldID fieldId)
{
	DE_ASSERT(cls && fieldId);
	return env->GetStaticIntField(cls, fieldId);
}

template<>
string getStaticFieldValue<string> (JNIEnv* env, jclass cls, jfieldID fieldId)
{
	const jstring	jniStr	= (jstring)env->GetStaticObjectField(cls, fieldId);

	if (jniStr)
		return getJNIStringValue(env, jniStr);
	else
		return string();
}

template<>
vector<string> getStaticFieldValue<vector<string> > (JNIEnv* env, jclass cls, jfieldID fieldId)
{
	const jobjectArray	array		= (jobjectArray)env->GetStaticObjectField(cls, fieldId);
	vector<string>		result;

	checkJNIException(env);

	if (array)
	{
		const int	numElements		= env->GetArrayLength(array);

		for (int ndx = 0; ndx < numElements; ndx++)
		{
			const jstring	jniStr	= (jstring)env->GetObjectArrayElement(array, ndx);

			checkJNIException(env);

			if (jniStr)
				result.push_back(getJNIStringValue(env, jniStr));
		}
	}

	return result;
}

template<typename FieldType>
FieldType getStaticField (JNIEnv* env, const char* className, const char* fieldName)
{
	const jclass	cls			= env->FindClass(className);
	const jfieldID	fieldId		= cls ? env->GetStaticFieldID(cls, fieldName, getJNITypeStr<FieldType>()) : (jfieldID)0;

	checkJNIException(env);

	if (cls && fieldId)
		return getStaticFieldValue<FieldType>(env, cls, fieldId);
	else
		return FieldType();
}

class ScopedJNIEnv
{
public:

					ScopedJNIEnv	(JavaVM* vm);
					~ScopedJNIEnv	(void);

	JavaVM*			getVM			(void) const { return m_vm;		}
	JNIEnv*			getEnv			(void) const { return m_env;	}

private:
	JavaVM* const	m_vm;
	JNIEnv*			m_env;
	bool			m_detach;
};

ScopedJNIEnv::ScopedJNIEnv (JavaVM* vm)
	: m_vm		(vm)
	, m_env		(DE_NULL)
	, m_detach	(false)
{
	const int	getEnvRes	= m_vm->GetEnv((void**)&m_env, JNI_VERSION_1_6);

	if (getEnvRes == JNI_EDETACHED)
	{
		if (m_vm->AttachCurrentThread(&m_env, DE_NULL) != JNI_OK)
			throw std::runtime_error("JNI AttachCurrentThread() failed");

		m_detach = true;
	}
	else if (getEnvRes != JNI_OK)
		throw std::runtime_error("JNI GetEnv() failed");

	DE_ASSERT(m_env);
}

ScopedJNIEnv::~ScopedJNIEnv (void)
{
	if (m_detach)
		m_vm->DetachCurrentThread();
}

void describePlatform (ANativeActivity* activity, std::ostream& dst)
{
	const ScopedJNIEnv	env				(activity->vm);
	const char* const	buildClass		= "android/os/Build";
	const char* const	versionClass	= "android/os/Build$VERSION";

	static const struct
	{
		const char*		classPath;
		const char*		className;
		const char*		fieldName;
	} s_stringFields[] =
	{
		{ buildClass,	"Build",			"BOARD"			},
		{ buildClass,	"Build",			"BRAND"			},
		{ buildClass,	"Build",			"DEVICE"		},
		{ buildClass,	"Build",			"DISPLAY"		},
		{ buildClass,	"Build",			"FINGERPRINT"	},
		{ buildClass,	"Build",			"HARDWARE"		},
		{ buildClass,	"Build",			"MANUFACTURER"	},
		{ buildClass,	"Build",			"MODEL"			},
		{ buildClass,	"Build",			"PRODUCT"		},
		{ buildClass,	"Build",			"TAGS"			},
		{ buildClass,	"Build",			"TYPE"			},
		{ versionClass,	"Build.VERSION",	"RELEASE"		},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_stringFields); ndx++)
		dst << s_stringFields[ndx].className << "." << s_stringFields[ndx].fieldName
			<< ": " << getStaticField<string>(env.getEnv(), s_stringFields[ndx].classPath, s_stringFields[ndx].fieldName)
			<< "\n";

	dst << "Build.VERSION.SDK_INT: " << getStaticField<int>(env.getEnv(), versionClass, "SDK_INT") << "\n";

	{
		const vector<string>	supportedAbis	= getStaticField<vector<string> >(env.getEnv(), buildClass, "SUPPORTED_ABIS");

		dst << "Build.SUPPORTED_ABIS: ";

		for (size_t ndx = 0; ndx < supportedAbis.size(); ndx++)
			dst << (ndx != 0 ? ", " : "") << supportedAbis[ndx];

		dst << "\n";
	}
}

} // Android
} // tcu
