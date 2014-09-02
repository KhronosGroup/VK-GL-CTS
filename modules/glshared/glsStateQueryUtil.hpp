#ifndef _GLSSTATEQUERYUTIL_HPP
#define _GLSSTATEQUERYUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief State Query test utils.
 *//*--------------------------------------------------------------------*/

#include "deMath.h"
#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"

namespace deqp
{
namespace gls
{
namespace StateQueryUtil
{

/*--------------------------------------------------------------------*//*!
 * \brief Rounds given float to the nearest integer (half up).
 *
 * Returns the nearest integer for a float argument. In the case that there
 * are two nearest integers at the equal distance (aka. the argument is of
 * form x.5), the integer with the higher value is chosen. (x.5 rounds to x+1)
 *//*--------------------------------------------------------------------*/
template <typename T>
T roundGLfloatToNearestIntegerHalfUp (float val)
{
	return (T)(deFloatFloor(val + 0.5f));
}

/*--------------------------------------------------------------------*//*!
 * \brief Rounds given float to the nearest integer (half down).
 *
 * Returns the nearest integer for a float argument. In the case that there
 * are two nearest integers at the equal distance (aka. the argument is of
 * form x.5), the integer with the higher value is chosen. (x.5 rounds to x)
 *//*--------------------------------------------------------------------*/
template <typename T>
T roundGLfloatToNearestIntegerHalfDown (float val)
{
	return (T)(deFloatCeil(val - 0.5f));
}

template <typename T>
class StateQueryMemoryWriteGuard
{
public:
					StateQueryMemoryWriteGuard	(void);

					operator T&					(void);
	T*				operator &					(void);

	bool			isUndefined					(void) const;
	bool			isMemoryContaminated		(void) const;
	bool			verifyValidity				(tcu::TestContext& testCtx) const;

	const T&		get							(void) const { return m_value; }

private:
	enum
	{
		GUARD_VALUE = 0xDEDEADCD
	};
	enum
	{
		WRITE_GUARD_VALUE = 0xDE
	};

	deInt32			m_preguard;
	union
	{
		T			m_value;
		deUint8		m_isWrittenToGuard[sizeof(T)];
	};
	deInt32			m_postguard; // \note guards are not const qualified since the GL implementation might modify them
};

template <typename T>
StateQueryMemoryWriteGuard<T>::StateQueryMemoryWriteGuard (void)
	: m_preguard	((deInt32)(GUARD_VALUE))
	, m_postguard	((deInt32)(GUARD_VALUE))
{
	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(m_isWrittenToGuard); ++i)
		m_isWrittenToGuard[i] = (deUint8)WRITE_GUARD_VALUE;
}

template <typename T>
StateQueryMemoryWriteGuard<T>::operator T& (void)
{
	return m_value;
}

template <typename T>
T* StateQueryMemoryWriteGuard<T>::operator & (void)
{
	return &m_value;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isUndefined () const
{
	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(m_isWrittenToGuard); ++i)
		if (m_isWrittenToGuard[i] != (deUint8)WRITE_GUARD_VALUE)
			return false;
	return true;
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::isMemoryContaminated () const
{
	return (m_preguard != (deInt32)(GUARD_VALUE)) || (m_postguard != (deInt32)(GUARD_VALUE));
}

template <typename T>
bool StateQueryMemoryWriteGuard<T>::verifyValidity (tcu::TestContext& testCtx) const
{
	using tcu::TestLog;

	if (m_preguard != (deInt32)(GUARD_VALUE))
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: Pre-guard value was modified " << TestLog::EndMessage;
		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS ||
			testCtx.getTestResult() == QP_TEST_RESULT_LAST)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did an illegal memory write");

		return false;
	}
	else if (m_postguard != (deInt32)(GUARD_VALUE))
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: Post-guard value was modified " << TestLog::EndMessage;
		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS ||
			testCtx.getTestResult() == QP_TEST_RESULT_LAST)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did an illegal memory write");

		return false;
	}
	else if (isUndefined())
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: Get* did not return a value" << TestLog::EndMessage;
		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS ||
			testCtx.getTestResult() == QP_TEST_RESULT_LAST)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Get* did not return a value");

		return false;
	}

	return true;
}

template<typename T>
std::ostream& operator<< (std::ostream& str, const StateQueryMemoryWriteGuard<T>& guard)
{
	return str << guard.get();
}

} // StateQueryUtil
} // gls
} // deqp

#endif // _GLSSTATEQUERYUTIL_HPP
