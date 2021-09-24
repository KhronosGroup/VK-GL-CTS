/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Precision and range tests for builtins and types.
 *
 *//*--------------------------------------------------------------------*/

#include "vktShaderBuiltinPrecisionTests.hpp"
#include "vktShaderExecutor.hpp"

#include "deMath.h"
#include "deMemory.h"
#include "deFloat16.h"
#include "deDefs.hpp"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deArrayUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuInterval.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuResultCollector.hpp"
#include "tcuMaybe.hpp"

#include "gluContextInfo.hpp"
#include "gluVarType.hpp"
#include "gluRenderContext.hpp"
#include "glwDefs.hpp"

#include <cmath>
#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <utility>
#include <limits>

// Uncomment this to get evaluation trace dumps to std::cerr
// #define GLS_ENABLE_TRACE

// set this to true to dump even passing results
#define GLS_LOG_ALL_RESULTS false

#define FLOAT16_1_0		0x3C00 //1.0 float16bit
#define FLOAT16_180_0	0x59A0 //180.0 float16bit
#define FLOAT16_2_0		0x4000 //2.0 float16bit
#define FLOAT16_3_0		0x4200 //3.0 float16bit
#define FLOAT16_0_5		0x3800 //0.5 float16bit
#define FLOAT16_0_0		0x0000 //0.0 float16bit


using tcu::Vector;
typedef Vector<deFloat16, 1>	Vec1_16Bit;
typedef Vector<deFloat16, 2>	Vec2_16Bit;
typedef Vector<deFloat16, 3>	Vec3_16Bit;
typedef Vector<deFloat16, 4>	Vec4_16Bit;

typedef Vector<double, 1>		Vec1_64Bit;
typedef Vector<double, 2>		Vec2_64Bit;
typedef Vector<double, 3>		Vec3_64Bit;
typedef Vector<double, 4>		Vec4_64Bit;

enum
{
	// Computing reference intervals can take a non-trivial amount of time, especially on
	// platforms where toggling floating-point rounding mode is slow (emulated arm on x86).
	// As a workaround watchdog is kept happy by touching it periodically during reference
	// interval computation.
	TOUCH_WATCHDOG_VALUE_FREQUENCY	= 512
};

namespace vkt
{
namespace shaderexecutor
{

using std::string;
using std::map;
using std::ostream;
using std::ostringstream;
using std::pair;
using std::vector;
using std::set;

using de::MovePtr;
using de::Random;
using de::SharedPtr;
using de::UniquePtr;
using tcu::Interval;
using tcu::FloatFormat;
using tcu::MessageBuilder;
using tcu::TestLog;
using tcu::Vector;
using tcu::Matrix;
using glu::Precision;
using glu::VarType;
using glu::DataType;
using glu::ShaderType;

enum PrecisionTestFeatureBits
{
	PRECISION_TEST_FEATURES_NONE									= 0u,
	PRECISION_TEST_FEATURES_16BIT_BUFFER_ACCESS						= (1u << 1),
	PRECISION_TEST_FEATURES_16BIT_UNIFORM_AND_STORAGE_BUFFER_ACCESS	= (1u << 2),
	PRECISION_TEST_FEATURES_16BIT_PUSH_CONSTANT						= (1u << 3),
	PRECISION_TEST_FEATURES_16BIT_INPUT_OUTPUT						= (1u << 4),
	PRECISION_TEST_FEATURES_16BIT_SHADER_FLOAT						= (1u << 5),
	PRECISION_TEST_FEATURES_64BIT_SHADER_FLOAT						= (1u << 6),
};
typedef deUint32 PrecisionTestFeatures;


void areFeaturesSupported (const Context& context, deUint32 toCheck)
{
	if (toCheck == PRECISION_TEST_FEATURES_NONE) return;

	const vk::VkPhysicalDevice16BitStorageFeatures& extensionFeatures = context.get16BitStorageFeatures();

	if ((toCheck & PRECISION_TEST_FEATURES_16BIT_BUFFER_ACCESS) != 0 && extensionFeatures.storageBuffer16BitAccess == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 16bit storage features not supported");

	if ((toCheck & PRECISION_TEST_FEATURES_16BIT_UNIFORM_AND_STORAGE_BUFFER_ACCESS) != 0 && extensionFeatures.uniformAndStorageBuffer16BitAccess == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 16bit storage features not supported");

	if ((toCheck & PRECISION_TEST_FEATURES_16BIT_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant16 == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 16bit storage features not supported");

	if ((toCheck & PRECISION_TEST_FEATURES_16BIT_INPUT_OUTPUT) != 0 && extensionFeatures.storageInputOutput16 == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 16bit storage features not supported");

	if ((toCheck & PRECISION_TEST_FEATURES_16BIT_SHADER_FLOAT) != 0 && context.getShaderFloat16Int8Features().shaderFloat16 == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 16-bit floats (halfs) are not supported in shader code");

	if ((toCheck & PRECISION_TEST_FEATURES_64BIT_SHADER_FLOAT) != 0 && context.getDeviceFeatures().shaderFloat64 == VK_FALSE)
		TCU_THROW(NotSupportedError, "Requested 64-bit floats are not supported in shader code");
}

/*--------------------------------------------------------------------*//*!
 * \brief Generic singleton creator.
 *
 * instance<T>() returns a reference to a unique default-constructed instance
 * of T. This is mainly used for our GLSL function implementations: each
 * function is implemented by an object, and each of the objects has a
 * distinct class. It would be extremely toilsome to maintain a separate
 * context object that contained individual instances of the function classes,
 * so we have to resort to global singleton instances.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
const T& instance (void)
{
	static const T s_instance = T();
	return s_instance;
}

/*--------------------------------------------------------------------*//*!
 * \brief Dummy placeholder type for unused template parameters.
 *
 * In the precision tests we are dealing with functions of different arities.
 * To minimize code duplication, we only define templates with the maximum
 * number of arguments, currently four. If a function's arity is less than the
 * maximum, Void us used as the type for unused arguments.
 *
 * Although Voids are not used at run-time, they still must be compilable, so
 * they must support all operations that other types do.
 *
 *//*--------------------------------------------------------------------*/
struct Void
{
	typedef	Void		Element;
	enum
	{
		SIZE = 0,
	};

	template <typename T>
	explicit			Void			(const T&)		{}
						Void			(void)			{}
						operator double	(void)	const	{ return TCU_NAN; }

	// These are used to make Voids usable as containers in container-generic code.
	Void&				operator[]		(int)			{ return *this; }
	const Void&			operator[]		(int)	const	{ return *this; }
};

ostream& operator<< (ostream& os, Void) { return os << "()"; }

//! Returns true for all other types except Void
template <typename T>	bool isTypeValid		(void)	{ return true;	}
template <>				bool isTypeValid<Void>	(void)	{ return false;	}

template <typename T>	bool isInteger				(void)	{ return false;	}
template <>				bool isInteger<int>			(void)	{ return true;	}
template <>				bool isInteger<tcu::IVec2>	(void)	{ return true;	}
template <>				bool isInteger<tcu::IVec3>	(void)	{ return true; }
template <>				bool isInteger<tcu::IVec4>	(void)	{ return true; }

//! Utility function for getting the name of a data type.
//! This is used in vector and matrix constructors.
template <typename T>
const char* dataTypeNameOf (void)
{
	return glu::getDataTypeName(glu::dataTypeOf<T>());
}

template <>
const char* dataTypeNameOf<Void> (void)
{
	DE_FATAL("Impossible");
	return DE_NULL;
}

template <typename T>
VarType getVarTypeOf (Precision prec = glu::PRECISION_LAST)
{
	return glu::varTypeOf<T>(prec);
}

//! A hack to get Void support for VarType.
template <>
VarType getVarTypeOf<Void> (Precision)
{
	DE_FATAL("Impossible");
	return VarType();
}

/*--------------------------------------------------------------------*//*!
 * \brief Type traits for generalized interval types.
 *
 * We are trying to compute sets of acceptable values not only for
 * float-valued expressions but also for compound values: vectors and
 * matrices. We approximate a set of vectors as a vector of intervals and
 * likewise for matrices.
 *
 * We now need generalized operations for each type and its interval
 * approximation. These are given in the type Traits<T>.
 *
 * The type Traits<T>::IVal is the approximation of T: it is `Interval` for
 * scalar types, and a vector or matrix of intervals for container types.
 *
 * To allow template inference to take place, there are function wrappers for
 * the actual operations in Traits<T>. Hence we can just use:
 *
 * makeIVal(someFloat)
 *
 * instead of:
 *
 * Traits<float>::doMakeIVal(value)
 *
 *//*--------------------------------------------------------------------*/

template <typename T> struct Traits;

//! Create container from elementwise singleton values.
template <typename T>
typename Traits<T>::IVal makeIVal (const T& value)
{
	return Traits<T>::doMakeIVal(value);
}

//! Elementwise union of intervals.
template <typename T>
typename Traits<T>::IVal unionIVal (const typename Traits<T>::IVal& a,
									const typename Traits<T>::IVal& b)
{
	return Traits<T>::doUnion(a, b);
}

//! Returns true iff every element of `ival` contains the corresponding element of `value`.
template <typename T, typename U = Void>
bool contains (const typename Traits<T>::IVal& ival, const T& value, bool is16Bit = false, const tcu::Maybe<U>& modularDivisor = tcu::nothing<U>())
{
	return Traits<T>::doContains(ival, value, is16Bit, modularDivisor);
}

//! Print out an interval with the precision of `fmt`.
template <typename T>
void printIVal (const FloatFormat& fmt, const typename Traits<T>::IVal& ival, ostream& os)
{
	Traits<T>::doPrintIVal(fmt, ival, os);
}

template <typename T>
string intervalToString (const FloatFormat& fmt, const typename Traits<T>::IVal& ival)
{
	ostringstream oss;
	printIVal<T>(fmt, ival, oss);
	return oss.str();
}

//! Print out a value with the precision of `fmt`.
template <typename T>
void printValue16 (const FloatFormat& fmt, const T& value, ostream& os)
{
	Traits<T>::doPrintValue16(fmt, value, os);
}

template <typename T>
string value16ToString(const FloatFormat& fmt, const T& val)
{
	ostringstream oss;
	printValue16(fmt, val, oss);
	return oss.str();
}

const std::string getComparisonOperation(const int ndx)
{
	const int operationCount = 10;
	DE_ASSERT(de::inBounds(ndx, 0, operationCount));
	const std::string operations[operationCount] =
	{
		"OpFOrdEqual\t\t\t",
		"OpFOrdGreaterThan\t",
		"OpFOrdLessThan\t\t",
		"OpFOrdGreaterThanEqual",
		"OpFOrdLessThanEqual\t",
		"OpFUnordEqual\t\t",
		"OpFUnordGreaterThan\t",
		"OpFUnordLessThan\t",
		"OpFUnordGreaterThanEqual",
		"OpFUnordLessThanEqual"
	};
	return operations[ndx];
}

template <typename T>
string comparisonMessage(const T& val)
{
	DE_UNREF(val);
	return "";
}

template <>
string comparisonMessage(const int& val)
{
	ostringstream oss;

	int flags = val;
	for(int ndx = 0; ndx < 10; ++ndx)
	{
		oss << getComparisonOperation(ndx) << "\t:\t" << ((flags & 1) == 1 ? "TRUE" : "FALSE") << "\n";
		flags = flags >> 1;
	}
	return oss.str();
}

template <>
string comparisonMessage(const tcu::IVec2& val)
{
	ostringstream oss;
	tcu::IVec2 flags = val;
	for (int ndx = 0; ndx < 10; ++ndx)
	{
		oss << getComparisonOperation(ndx) << "\t:\t" << ((flags.x() & 1) == 1 ? "TRUE" : "FALSE") << "\t" << ((flags.y() & 1) == 1 ? "TRUE" : "FALSE") << "\n";
		flags.x() = flags.x() >> 1;
		flags.y() = flags.y() >> 1;
	}
	return oss.str();
}

template <>
string comparisonMessage(const tcu::IVec3& val)
{
	ostringstream oss;
	tcu::IVec3 flags = val;
	for (int ndx = 0; ndx < 10; ++ndx)
	{
		oss << getComparisonOperation(ndx) << "\t:\t" << ((flags.x() & 1) == 1 ? "TRUE" : "FALSE") << "\t"
								<< ((flags.y() & 1) == 1 ? "TRUE" : "FALSE") << "\t"
								<< ((flags.z() & 1) == 1 ? "TRUE" : "FALSE") << "\n";
		flags.x() = flags.x() >> 1;
		flags.y() = flags.y() >> 1;
		flags.z() = flags.z() >> 1;
	}
	return oss.str();
}

template <>
string comparisonMessage(const tcu::IVec4& val)
{
	ostringstream oss;
	tcu::IVec4 flags = val;
	for (int ndx = 0; ndx < 10; ++ndx)
	{
		oss << getComparisonOperation(ndx) << "\t:\t" << ((flags.x() & 1) == 1 ? "TRUE" : "FALSE") << "\t"
			<< ((flags.y() & 1) == 1 ? "TRUE" : "FALSE") << "\t"
			<< ((flags.z() & 1) == 1 ? "TRUE" : "FALSE") << "\t"
			<< ((flags.w() & 1) == 1 ? "TRUE" : "FALSE") << "\n";
		flags.x() = flags.x() >> 1;
		flags.y() = flags.y() >> 1;
		flags.z() = flags.z() >> 1;
		flags.w() = flags.z() >> 1;
	}
	return oss.str();
}
//! Print out a value with the precision of `fmt`.
template <typename T>
void printValue32 (const FloatFormat& fmt, const T& value, ostream& os)
{
	Traits<T>::doPrintValue32(fmt, value, os);
}

template <typename T>
string value32ToString (const FloatFormat& fmt, const T& val)
{
	ostringstream oss;
	printValue32(fmt, val, oss);
	return oss.str();
}

template <typename T>
void printValue64 (const FloatFormat& fmt, const T& value, ostream& os)
{
	Traits<T>::doPrintValue64(fmt, value, os);
}

template <typename T>
string value64ToString (const FloatFormat& fmt, const T& val)
{
	ostringstream oss;
	printValue64(fmt, val, oss);
	return oss.str();
}

//! Approximate `value` elementwise to the float precision defined in `fmt`.
//! The resulting interval might not be a singleton if rounding in both
//! directions is allowed.
template <typename T>
typename Traits<T>::IVal round (const FloatFormat& fmt, const T& value)
{
	return Traits<T>::doRound(fmt, value);
}

template <typename T>
typename Traits<T>::IVal convert (const FloatFormat&				fmt,
								  const typename Traits<T>::IVal&	value)
{
	return Traits<T>::doConvert(fmt, value);
}

// Matching input and output types. We may be in a modulo case and modularDivisor may have an actual value.
template <typename T>
bool intervalContains (const Interval& interval, T value, const tcu::Maybe<T>& modularDivisor)
{
	bool contained = interval.contains(value);

	if (!contained && modularDivisor)
	{
		const T divisor = modularDivisor.get();

		// In a modulo operation, if the calculated answer contains the divisor, allow exactly 0.0 as a replacement. Alternatively,
		// if the calculated answer contains 0.0, allow exactly the divisor as a replacement.
		if (interval.contains(static_cast<double>(divisor)))
			contained |= (value == 0.0);
		if (interval.contains(0.0))
			contained |= (value == divisor);
	}
	return contained;
}

// When the input and output types do not match, we are not in a real modulo operation. Do not take the divisor into account. This
// version is provided for syntactical compatibility only.
template <typename T, typename U>
bool intervalContains (const Interval& interval, T value, const tcu::Maybe<U>& modularDivisor)
{
	DE_UNREF(modularDivisor);		// For release builds.
	DE_ASSERT(!modularDivisor);
	return interval.contains(value);
}

//! Common traits for scalar types.
template <typename T>
struct ScalarTraits
{
	typedef				Interval		IVal;

	static Interval		doMakeIVal		(const T& value)
	{
		// Thankfully all scalar types have a well-defined conversion to `double`,
		// hence Interval can represent their ranges without problems.
		return Interval(double(value));
	}

	static Interval		doUnion			(const Interval& a, const Interval& b)
	{
		return a | b;
	}

	static bool			doContains		(const Interval& a, T value)
	{
		return a.contains(double(value));
	}

	static Interval		doConvert		(const FloatFormat& fmt, const IVal& ival)
	{
		return fmt.convert(ival);
	}

	static Interval		doConvert		(const FloatFormat& fmt, const IVal& ival, bool is16Bit)
	{
		DE_UNREF(is16Bit);
		return fmt.convert(ival);
	}

	static Interval		doRound			(const FloatFormat& fmt, T value)
	{
		return fmt.roundOut(double(value), false);
	}
};

template <>
struct ScalarTraits<deUint16>
{
	typedef				Interval		IVal;

	static Interval		doMakeIVal		(const deUint16& value)
	{
		// Thankfully all scalar types have a well-defined conversion to `double`,
		// hence Interval can represent their ranges without problems.
		return Interval(double(deFloat16To32(value)));
	}

	static Interval		doUnion			(const Interval& a, const Interval& b)
	{
		return a | b;
	}

	static Interval		doConvert		(const FloatFormat& fmt, const IVal& ival)
	{
		return fmt.convert(ival);
	}

	static Interval		doRound			(const FloatFormat& fmt, deUint16 value)
	{
		return fmt.roundOut(double(deFloat16To32(value)), false);
	}
};

template<>
struct Traits<float> : ScalarTraits<float>
{
	static void			doPrintIVal		(const FloatFormat&	fmt,
										 const Interval&	ival,
										 ostream&			os)
	{
		os << fmt.intervalToHex(ival);
	}

	static void			doPrintValue16	(const FloatFormat&	fmt,
										 const float&		value,
										 ostream&			os)
	{
		const deUint32 iRep = reinterpret_cast<const deUint32 & >(value);
		float res0 = deFloat16To32((deFloat16)(iRep & 0xFFFF));
		float res1 = deFloat16To32((deFloat16)(iRep >> 16));
		os << fmt.floatToHex(res0) << " " << fmt.floatToHex(res1);
	}

	static void			doPrintValue32	(const FloatFormat&	fmt,
										 const float&		value,
										 ostream&			os)
	{
		os << fmt.floatToHex(value);
	}

	static void			doPrintValue64	(const FloatFormat&	fmt,
										 const float&		value,
										 ostream&			os)
	{
		os << fmt.floatToHex(value);
	}

	template <typename U>
	static bool			doContains		(const Interval& a, const float& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor)
	{
		if(is16Bit)
		{
			// Note: for deFloat16s packed in 32 bits, the original divisor is provided as a float to the shader in the input
			// buffer, so U is also float here and we call the right interlvalContains() version.
			const deUint32 iRep = reinterpret_cast<const deUint32&>(value);
			float res0 = deFloat16To32((deFloat16)(iRep & 0xFFFF));
			float res1 = deFloat16To32((deFloat16)(iRep >> 16));
			return intervalContains(a, res0, modularDivisor) && (res1 == -1.0);
		}
		return intervalContains(a, value, modularDivisor);
	}
};

template<>
struct Traits<double> : ScalarTraits<double>
{
	static void			doPrintIVal		(const FloatFormat&	fmt,
										 const Interval&	ival,
										 ostream&			os)
	{
		os << fmt.intervalToHex(ival);
	}

	static void			doPrintValue16	(const FloatFormat&	fmt,
										 const double&		value,
										 ostream&			os)
	{
		const deUint64 iRep = reinterpret_cast<const deUint64&>(value);
		double byte0 = deFloat16To64((deFloat16)((iRep      ) & 0xffff));
		double byte1 = deFloat16To64((deFloat16)((iRep >> 16) & 0xffff));
		double byte2 = deFloat16To64((deFloat16)((iRep >> 32) & 0xffff));
		double byte3 = deFloat16To64((deFloat16)((iRep >> 48) & 0xffff));
		os << fmt.floatToHex(byte0) << " " << fmt.floatToHex(byte1) << " " << fmt.floatToHex(byte2) << " " << fmt.floatToHex(byte3);
	}

	static void			doPrintValue32	(const FloatFormat&	fmt,
										 const double&		value,
										 ostream&			os)
	{
		const deUint64 iRep = reinterpret_cast<const deUint64&>(value);
		double res0 = static_cast<double>((float)((iRep      ) & 0xffffffff));
		double res1 = static_cast<double>((float)((iRep >> 32) & 0xffffffff));
		os << fmt.floatToHex(res0) << " " << fmt.floatToHex(res1);
	}

	static void			doPrintValue64	(const FloatFormat&	fmt,
										 const double&		value,
										 ostream&			os)
	{
		os << fmt.floatToHex(value);
	}

	template <class U>
	static bool			doContains		(const Interval& a, const double& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor)
	{
		DE_UNREF(is16Bit);
		DE_ASSERT(!is16Bit);
		return intervalContains(a, value, modularDivisor);
	}
};

template<>
struct Traits<deFloat16> : ScalarTraits<deFloat16>
{
	static void			doPrintIVal		(const FloatFormat&	fmt,
										 const Interval&	ival,
										 ostream&			os)
	{
		os << fmt.intervalToHex(ival);
	}

	static void			doPrintValue16	(const FloatFormat&	fmt,
										 const deFloat16&	value,
										 ostream&			os)
	{
		const float res0 = deFloat16To32(value);
		os << fmt.floatToHex(static_cast<double>(res0));
	}
	static void			doPrintValue32	(const FloatFormat&	fmt,
										 const deFloat16&	value,
										 ostream&			os)
	{
		const float res0 = deFloat16To32(value);
		os << fmt.floatToHex(static_cast<double>(res0));
	}

	static void			doPrintValue64	(const FloatFormat&	fmt,
										 const deFloat16&	value,
										 ostream&			os)
	{
		const double res0 = deFloat16To64(value);
		os << fmt.floatToHex(res0);
	}

	// When the value and divisor are both deFloat16, convert both to float to call the right intervalContains version.
	static bool			doContains		(const Interval& a, const deFloat16& value, bool is16Bit, const tcu::Maybe<deFloat16>& modularDivisor)
	{
		DE_UNREF(is16Bit);
		float res0 = deFloat16To32(value);
		const tcu::Maybe<float> convertedDivisor = (modularDivisor ? tcu::just(deFloat16To32(modularDivisor.get())) : tcu::nothing<float>());
		return intervalContains(a, res0, convertedDivisor);
	}

	// If the types don't match we should not be in a modulo operation, so no conversion should take place.
	template <class U>
	static bool			doContains		(const Interval& a, const deFloat16& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor)
	{
		DE_UNREF(is16Bit);
		float res0 = deFloat16To32(value);
		return intervalContains(a, res0, modularDivisor);
	}
};

template<>
struct Traits<bool> : ScalarTraits<bool>
{
	static void			doPrintValue16	(const FloatFormat&,
										 const float&		value,
										 ostream&			os)
	{
		os << (value != 0.0f ? "true" : "false");
	}

	static void		doPrintValue32	(const			FloatFormat&,
									 const float&	value,
									 ostream&		os)
	{
		os << (value != 0.0f ? "true" : "false");
	}

	static void		doPrintValue64	(const			FloatFormat&,
									 const float&	value,
									 ostream&		os)
	{
		os << (value != 0.0f ? "true" : "false");
	}

	static void			doPrintIVal		(const FloatFormat&,
										 const Interval&	ival,
										 ostream&			os)
	{
		os << "{";
		if (ival.contains(false))
			os << "false";
		if (ival.contains(false) && ival.contains(true))
			os << ", ";
		if (ival.contains(true))
			os << "true";
		os << "}";
	}
};

template<>
struct Traits<int> : ScalarTraits<int>
{
	static void			doPrintValue16	(const FloatFormat&,
										 const int&			value,
										 ostream&			os)
	{
		int res0 = value & 0xFFFF;
		int res1 = value >> 16;
		os << res0 << " " << res1;
	}

	static void		doPrintValue32		(const FloatFormat&,
										 const int&			value,
										 ostream&			os)
	{
		os << value;
	}

	static void		doPrintValue64		(const FloatFormat&,
										 const int&			value,
										 ostream&			os)
	{
		os << value;
	}

	static void			doPrintIVal		(const FloatFormat&,
										 const Interval&	ival,
										 ostream&			os)
	{
		os << "[" << int(ival.lo()) << ", " << int(ival.hi()) << "]";
	}

	template <typename U>
	static bool			doContains		(const Interval& a, const int& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor)
	{
		DE_UNREF(is16Bit);
		return intervalContains(a, value, modularDivisor);
	}
};

//! Common traits for containers, i.e. vectors and matrices.
//! T is the container type itself, I is the same type with interval elements.
template <typename T, typename I>
struct ContainerTraits
{
	typedef typename	T::Element		Element;
	typedef				I				IVal;

	static IVal			doMakeIVal		(const T& value)
	{
		IVal ret;

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
			ret[ndx] = makeIVal(value[ndx]);

		return ret;
	}

	static IVal			doUnion			(const IVal& a, const IVal& b)
	{
		IVal ret;

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
			ret[ndx] = unionIVal<Element>(a[ndx], b[ndx]);

		return ret;
	}

	// When the input and output types match, we may be in a modulo operation. If the divisor is provided, use each of its
	// components to determine if the obtained result is fine.
	static bool			doContains		(const IVal& ival, const T& value, bool is16Bit, const tcu::Maybe<T>& modularDivisor)
	{
		using DivisorElement = typename T::Element;

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			const tcu::Maybe<DivisorElement> divisorElement = (modularDivisor ? tcu::just((*modularDivisor)[ndx]) : tcu::nothing<DivisorElement>());
			if (!contains(ival[ndx], value[ndx], is16Bit, divisorElement))
				return false;
		}

		return true;
	}

	// When the input and output types do not match we should not be in a modulo operation. This version is provided for syntactical
	// compatibility.
	template <typename U>
	static bool			doContains		(const IVal& ival, const T& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor)
	{
		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			if (!contains(ival[ndx], value[ndx], is16Bit, modularDivisor))
				return false;
		}

		return true;
	}

	static void			doPrintIVal		(const FloatFormat& fmt, const IVal ival, ostream& os)
	{
		os << "(";

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			if (ndx > 0)
				os << ", ";

			printIVal<Element>(fmt, ival[ndx], os);
		}

		os << ")";
	}

	static void			doPrintValue16	(const FloatFormat& fmt, const T& value, ostream& os)
	{
		os << dataTypeNameOf<T>() << "(";

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			if (ndx > 0)
				os << ", ";

			printValue16<Element>(fmt, value[ndx], os);
		}

		os << ")";
	}

	static void			doPrintValue32	(const FloatFormat& fmt, const T& value, ostream& os)
	{
		os << dataTypeNameOf<T>() << "(";

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			if (ndx > 0)
				os << ", ";

			printValue32<Element>(fmt, value[ndx], os);
		}

		os << ")";
	}

	static void			doPrintValue64	(const FloatFormat& fmt, const T& value, ostream& os)
	{
		os << dataTypeNameOf<T>() << "(";

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
		{
			if (ndx > 0)
				os << ", ";

			printValue64<Element>(fmt, value[ndx], os);
		}

		os << ")";
	}

	static IVal			doConvert		(const FloatFormat& fmt, const IVal& value)
	{
		IVal ret;

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
			ret[ndx] = convert<Element>(fmt, value[ndx]);

		return ret;
	}

	static IVal			doRound			(const FloatFormat& fmt, T value)
	{
		IVal ret;

		for (int ndx = 0; ndx < T::SIZE; ++ndx)
			ret[ndx] = round(fmt, value[ndx]);

		return ret;
	}
};

template <typename T, int Size>
struct Traits<Vector<T, Size> > :
	ContainerTraits<Vector<T, Size>, Vector<typename Traits<T>::IVal, Size> >
{
};

template <typename T, int Rows, int Cols>
struct Traits<Matrix<T, Rows, Cols> > :
	ContainerTraits<Matrix<T, Rows, Cols>, Matrix<typename Traits<T>::IVal, Rows, Cols> >
{
};

//! Void traits. These are just dummies, but technically valid: a Void is a
//! unit type with a single possible value.
template<>
struct Traits<Void>
{
	typedef		Void			IVal;

	static Void	doMakeIVal		(const Void& value)										{ return value; }
	static Void	doUnion			(const Void&, const Void&)								{ return Void(); }
	static bool	doContains		(const Void&, Void)										{ return true; }
	template <typename U>
	static bool	doContains		(const Void&, const Void& value, bool is16Bit, const tcu::Maybe<U>& modularDivisor) { DE_UNREF(value); DE_UNREF(is16Bit); DE_UNREF(modularDivisor); return true; }
	static Void	doRound			(const FloatFormat&, const Void& value)					{ return value; }
	static Void	doConvert		(const FloatFormat&, const Void& value)					{ return value; }

	static void	doPrintValue16	(const FloatFormat&, const Void&, ostream& os)
	{
		os << "()";
	}

	static void	doPrintValue32	(const FloatFormat&, const Void&, ostream& os)
	{
		os << "()";
	}

	static void	doPrintValue64	(const FloatFormat&, const Void&, ostream& os)
	{
		os << "()";
	}

	static void	doPrintIVal		(const FloatFormat&, const Void&, ostream& os)
	{
		os << "()";
	}
};

//! This is needed for container-generic operations.
//! We want a scalar type T to be its own "one-element vector".
template <typename T, int Size> struct ContainerOf	{ typedef Vector<T, Size>	Container; };

template <typename T>			struct ContainerOf<T, 1>		{ typedef T		Container; };
template <int Size>				struct ContainerOf<Void, Size>	{ typedef Void	Container; };

// This is a kludge that is only needed to get the ExprP::operator[] syntactic sugar to work.
template <typename T>	struct ElementOf		{ typedef	typename T::Element	Element; };
template <>				struct ElementOf<float>	{ typedef	void				Element; };
template <>				struct ElementOf<double>{ typedef	void				Element; };
template <>				struct ElementOf<bool>	{ typedef	void				Element; };
template <>				struct ElementOf<int>	{ typedef	void				Element; };

template <typename T>
string comparisonMessageInterval(const typename Traits<T>::IVal& val)
{
	DE_UNREF(val);
	return "";
}

template <>
string comparisonMessageInterval<int>(const Traits<int>::IVal& val)
{
	return comparisonMessage(static_cast<int>(val.lo()));
}

template <>
string comparisonMessageInterval<float>(const Traits<float>::IVal& val)
{
	return comparisonMessage(static_cast<int>(val.lo()));
}

template <>
string comparisonMessageInterval<tcu::Vector<int, 2> >(const tcu::Vector<tcu::Interval, 2> & val)
{
	tcu::IVec2 result(static_cast<int>(val[0].lo()), static_cast<int>(val[1].lo()));
	return comparisonMessage(result);
}

template <>
string comparisonMessageInterval<tcu::Vector<int, 3> >(const tcu::Vector<tcu::Interval, 3> & val)
{
	tcu::IVec3 result(static_cast<int>(val[0].lo()), static_cast<int>(val[1].lo()), static_cast<int>(val[2].lo()));
	return comparisonMessage(result);
}

template <>
string comparisonMessageInterval<tcu::Vector<int, 4> >(const tcu::Vector<tcu::Interval, 4> & val)
{
	tcu::IVec4 result(static_cast<int>(val[0].lo()), static_cast<int>(val[1].lo()), static_cast<int>(val[2].lo()), static_cast<int>(val[3].lo()));
	return comparisonMessage(result);
}

/*--------------------------------------------------------------------*//*!
 *
 * \name Abstract syntax for expressions and statements.
 *
 * We represent GLSL programs as syntax objects: an Expr<T> represents an
 * expression whose GLSL type corresponds to the C++ type T, and a Statement
 * represents a statement.
 *
 * To ease memory management, we use shared pointers to refer to expressions
 * and statements. ExprP<T> is a shared pointer to an Expr<T>, and StatementP
 * is a shared pointer to a Statement.
 *
 * \{
 *
 *//*--------------------------------------------------------------------*/

class ExprBase;
class ExpandContext;
class Statement;
class StatementP;
class FuncBase;
template <typename T> class ExprP;
template <typename T> class Variable;
template <typename T> class VariableP;
template <typename T> class DefaultSampling;

typedef set<const FuncBase*> FuncSet;

template <typename T>
VariableP<T>	variable			(const string& name);
StatementP		compoundStatement	(const vector<StatementP>& statements);

/*--------------------------------------------------------------------*//*!
 * \brief A variable environment.
 *
 * An Environment object maintains the mapping between variables of the
 * abstract syntax tree and their values.
 *
 * \todo [2014-03-28 lauri] At least run-time type safety.
 *
 *//*--------------------------------------------------------------------*/
class Environment
{
public:
	template<typename T>
	void						bind	(const Variable<T>&					variable,
										 const typename Traits<T>::IVal&	value)
	{
		deUint8* const data = new deUint8[sizeof(value)];

		deMemcpy(data, &value, sizeof(value));
		de::insert(m_map, variable.getName(), SharedPtr<deUint8>(data, de::ArrayDeleter<deUint8>()));
	}

	template<typename T>
	typename Traits<T>::IVal&	lookup	(const Variable<T>& variable) const
	{
		deUint8* const data = de::lookup(m_map, variable.getName()).get();

		return *reinterpret_cast<typename Traits<T>::IVal*>(data);
	}

private:
	map<string, SharedPtr<deUint8> >	m_map;
};

/*--------------------------------------------------------------------*//*!
 * \brief Evaluation context.
 *
 * The evaluation context contains everything that separates one execution of
 * an expression from the next. Currently this means the desired floating
 * point precision and the current variable environment.
 *
 *//*--------------------------------------------------------------------*/
struct EvalContext
{
	EvalContext (const FloatFormat&	format_,
				 Precision			floatPrecision_,
				 Environment&		env_,
				 int				callDepth_)
		: format				(format_)
		, floatPrecision		(floatPrecision_)
		, env					(env_)
		, callDepth				(callDepth_) {}

	FloatFormat		format;
	Precision		floatPrecision;
	Environment&	env;
	int				callDepth;
};

/*--------------------------------------------------------------------*//*!
 * \brief Simple incremental counter.
 *
 * This is used to make sure that different ExpandContexts will not produce
 * overlapping temporary names.
 *
 *//*--------------------------------------------------------------------*/
class Counter
{
public:
			Counter		(int count = 0) : m_count(count) {}
	int		operator()	(void) { return m_count++; }

private:
	int		m_count;
};

class ExpandContext
{
public:
						ExpandContext	(Counter& symCounter) : m_symCounter(symCounter) {}
						ExpandContext	(const ExpandContext& parent)
							: m_symCounter(parent.m_symCounter) {}

	template<typename T>
	VariableP<T>		genSym			(const string& baseName)
	{
		return variable<T>(baseName + de::toString(m_symCounter()));
	}

	void				addStatement	(const StatementP& stmt)
	{
		m_statements.push_back(stmt);
	}

	vector<StatementP>	getStatements	(void) const
	{
		return m_statements;
	}
private:
	Counter&			m_symCounter;
	vector<StatementP>	m_statements;
};

/*--------------------------------------------------------------------*//*!
 * \brief A statement or declaration.
 *
 * Statements have no values. Instead, they are executed for their side
 * effects only: the execute() method should modify at least one variable in
 * the environment.
 *
 * As a bit of a kludge, a Statement object can also represent a declaration:
 * when it is evaluated, it can add a variable binding to the environment
 * instead of modifying a current one.
 *
 *//*--------------------------------------------------------------------*/
class Statement
{
public:
	virtual			~Statement		(void)							{								 }
	//! Execute the statement, modifying the environment of `ctx`
	void			execute			(EvalContext&	ctx)	const	{ this->doExecute(ctx);			 }
	void			print			(ostream&		os)		const	{ this->doPrint(os);			 }
	//! Add the functions used in this statement to `dst`.
	void			getUsedFuncs	(FuncSet& dst)			const	{ this->doGetUsedFuncs(dst);	 }
	void			failed			(EvalContext& ctx)		const	{ this->doFail(ctx);			 }

protected:
	virtual void	doPrint			(ostream& os)			const	= 0;
	virtual void	doExecute		(EvalContext& ctx)		const	= 0;
	virtual void	doGetUsedFuncs	(FuncSet& dst)			const	= 0;
	virtual void	doFail			(EvalContext& ctx)		const	{ DE_UNREF(ctx); }
};

ostream& operator<<(ostream& os, const Statement& stmt)
{
	stmt.print(os);
	return os;
}

/*--------------------------------------------------------------------*//*!
 * \brief Smart pointer for statements (and declarations)
 *
 *//*--------------------------------------------------------------------*/
class StatementP : public SharedPtr<const Statement>
{
public:
	typedef		SharedPtr<const Statement>	Super;

				StatementP			(void) {}
	explicit	StatementP			(const Statement* ptr)	: Super(ptr) {}
				StatementP			(const Super& ptr)		: Super(ptr) {}
};

/*--------------------------------------------------------------------*//*!
 * \brief
 *
 * A statement that modifies a variable or a declaration that binds a variable.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
class VariableStatement : public Statement
{
public:
					VariableStatement	(const VariableP<T>& variable, const ExprP<T>& value,
										 bool isDeclaration)
						: m_variable		(variable)
						, m_value			(value)
						, m_isDeclaration	(isDeclaration) {}

protected:
	void			doPrint				(ostream& os)							const
	{
		if (m_isDeclaration)
			os << glu::declare(getVarTypeOf<T>(), m_variable->getName());
		else
			os << m_variable->getName();

		os << " = ";
		os<< *m_value << ";\n";
	}

	void			doExecute			(EvalContext& ctx)						const
	{
		if (m_isDeclaration)
			ctx.env.bind(*m_variable, m_value->evaluate(ctx));
		else
			ctx.env.lookup(*m_variable) = m_value->evaluate(ctx);
	}

	void			doGetUsedFuncs		(FuncSet& dst)							const
	{
		m_value->getUsedFuncs(dst);
	}

	virtual void	doFail			(EvalContext& ctx)		const
	{
		if (m_isDeclaration)
			ctx.env.bind(*m_variable, m_value->fails(ctx));
		else
			ctx.env.lookup(*m_variable) = m_value->fails(ctx);
	}

	VariableP<T>	m_variable;
	ExprP<T>		m_value;
	bool			m_isDeclaration;
};

template <typename T>
StatementP variableStatement (const VariableP<T>&	variable,
							  const ExprP<T>&		value,
							  bool					isDeclaration)
{
	return StatementP(new VariableStatement<T>(variable, value, isDeclaration));
}

template <typename T>
StatementP variableDeclaration (const VariableP<T>& variable, const ExprP<T>& definiens)
{
	return variableStatement(variable, definiens, true);
}

template <typename T>
StatementP variableAssignment (const VariableP<T>& variable, const ExprP<T>& value)
{
	return variableStatement(variable, value, false);
}

/*--------------------------------------------------------------------*//*!
 * \brief A compound statement, i.e. a block.
 *
 * A compound statement is executed by executing its constituent statements in
 * sequence.
 *
 *//*--------------------------------------------------------------------*/
class CompoundStatement : public Statement
{
public:
						CompoundStatement	(const vector<StatementP>& statements)
							: m_statements	(statements) {}

protected:
	void				doPrint				(ostream&		os)						const
	{
		os << "{\n";

		for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
			os << *m_statements[ndx];

		os << "}\n";
	}

	void				doExecute			(EvalContext&	ctx)					const
	{
		for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
			m_statements[ndx]->execute(ctx);
	}

	void				doGetUsedFuncs		(FuncSet& dst)							const
	{
		for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
			m_statements[ndx]->getUsedFuncs(dst);
	}

	vector<StatementP>	m_statements;
};

StatementP compoundStatement(const vector<StatementP>& statements)
{
	return StatementP(new CompoundStatement(statements));
}

//! Common base class for all expressions regardless of their type.
class ExprBase
{
public:
	virtual				~ExprBase		(void)									{}
	void				printExpr		(ostream& os) const { this->doPrintExpr(os); }

	//! Output the functions that this expression refers to
	void				getUsedFuncs	(FuncSet& dst) const
	{
		this->doGetUsedFuncs(dst);
	}

protected:
	virtual void		doPrintExpr		(ostream&)	const	{}
	virtual void		doGetUsedFuncs	(FuncSet&)	const	{}
};

//! Type-specific operations for an expression representing type T.
template <typename T>
class Expr : public ExprBase
{
public:
	typedef				T				Val;
	typedef typename	Traits<T>::IVal	IVal;

	IVal				evaluate		(const EvalContext&	ctx) const;
	IVal				fails			(const EvalContext&	ctx) const	{ return this->doFails(ctx); }

protected:
	virtual IVal		doEvaluate		(const EvalContext&	ctx) const = 0;
	virtual IVal		doFails			(const EvalContext&	ctx) const {return doEvaluate(ctx);}
};

//! Evaluate an expression with the given context, optionally tracing the calls to stderr.
template <typename T>
typename Traits<T>::IVal Expr<T>::evaluate (const EvalContext& ctx) const
{
#ifdef GLS_ENABLE_TRACE
	static const FloatFormat	highpFmt	(-126, 127, 23, true,
											 tcu::MAYBE,
											 tcu::YES,
											 tcu::MAYBE);
	EvalContext					newCtx		(ctx.format, ctx.floatPrecision,
											 ctx.env, ctx.callDepth + 1);
	const IVal					ret			= this->doEvaluate(newCtx);

	if (isTypeValid<T>())
	{
		std::cerr << string(ctx.callDepth, ' ');
		this->printExpr(std::cerr);
		std::cerr << " -> " << intervalToString<T>(highpFmt, ret) << std::endl;
	}
	return ret;
#else
	return this->doEvaluate(ctx);
#endif
}

template <typename T>
class ExprPBase : public SharedPtr<const Expr<T> >
{
public:
};

ostream& operator<< (ostream& os, const ExprBase& expr)
{
	expr.printExpr(os);
	return os;
}

/*--------------------------------------------------------------------*//*!
 * \brief Shared pointer to an expression of a container type.
 *
 * Container types (i.e. vectors and matrices) support the subscription
 * operator. This class provides a bit of syntactic sugar to allow us to use
 * the C++ subscription operator to create a subscription expression.
 *//*--------------------------------------------------------------------*/
template <typename T>
class ContainerExprPBase : public ExprPBase<T>
{
public:
	ExprP<typename T::Element>	operator[]	(int i) const;
};

template <typename T>
class ExprP : public ExprPBase<T> {};

// We treat Voids as containers since the dummy parameters in generalized
// vector functions are represented as Voids.
template <>
class ExprP<Void> : public ContainerExprPBase<Void> {};

template <typename T, int Size>
class ExprP<Vector<T, Size> > : public ContainerExprPBase<Vector<T, Size> > {};

template <typename T, int Rows, int Cols>
class ExprP<Matrix<T, Rows, Cols> > : public ContainerExprPBase<Matrix<T, Rows, Cols> > {};

template <typename T> ExprP<T> exprP (void)
{
	return ExprP<T>();
}

template <typename T>
ExprP<T> exprP (const SharedPtr<const Expr<T> >& ptr)
{
	ExprP<T> ret;
	static_cast<SharedPtr<const Expr<T> >&>(ret) = ptr;
	return ret;
}

template <typename T>
ExprP<T> exprP (const Expr<T>* ptr)
{
	return exprP(SharedPtr<const Expr<T> >(ptr));
}

/*--------------------------------------------------------------------*//*!
 * \brief A shared pointer to a variable expression.
 *
 * This is just a narrowing of ExprP for the operations that require a variable
 * instead of an arbitrary expression.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
class VariableP : public SharedPtr<const Variable<T> >
{
public:
	typedef		SharedPtr<const Variable<T> >	Super;
	explicit	VariableP	(const Variable<T>* ptr) : Super(ptr) {}
				VariableP	(void) {}
				VariableP	(const Super& ptr) : Super(ptr) {}

	operator	ExprP<T>	(void) const { return exprP(SharedPtr<const Expr<T> >(*this)); }
};

/*--------------------------------------------------------------------*//*!
 * \name Syntactic sugar operators for expressions.
 *
 * @{
 *
 * These operators allow the use of C++ syntax to construct GLSL expressions
 * containing operators: e.g. "a+b" creates an addition expression with
 * operands a and b, and so on.
 *
 *//*--------------------------------------------------------------------*/
ExprP<float>						operator+ (const ExprP<float>&						arg0,
											  const ExprP<float>&						arg1);
ExprP<deFloat16>					operator+ (const ExprP<deFloat16>&					arg0,
											  const ExprP<deFloat16>&					arg1);
ExprP<double>						operator+ (const ExprP<double>&						arg0,
											  const ExprP<double>&						arg1);
template <typename T>
ExprP<T>							operator- (const ExprP<T>& arg0);
template <typename T>
ExprP<T>							operator- (const ExprP<T>&							arg0,
											  const ExprP<T>&							arg1);
template<int Left, int Mid, int Right, typename T>
ExprP<Matrix<T, Left, Right> >		operator* (const ExprP<Matrix<T, Left, Mid> >&		left,
											   const ExprP<Matrix<T, Mid, Right> >&		right);
ExprP<float>						operator* (const ExprP<float>&						arg0,
											  const ExprP<float>&						arg1);
ExprP<deFloat16>					operator* (const ExprP<deFloat16>&					arg0,
											   const ExprP<deFloat16>&					arg1);
ExprP<double>						operator* (const ExprP<double>&						arg0,
											  const ExprP<double>&						arg1);
template <typename T>
ExprP<T>							operator/ (const ExprP<T>&							arg0,
											  const ExprP<T>&							arg1);
template<typename T, int Size>
ExprP<Vector<T, Size> >				operator- (const ExprP<Vector<T, Size> >&			arg0);
template<typename T, int Size>
ExprP<Vector<T, Size> >				operator- (const ExprP<Vector<T, Size> >&			arg0,
											   const ExprP<Vector<T, Size> >&			arg1);
template<int Size, typename T>
ExprP<Vector<T, Size> >				operator* (const ExprP<Vector<T, Size> >&			arg0,
											   const ExprP<T>&							arg1);
template<typename T, int Size>
ExprP<Vector<T, Size> >				operator* (const ExprP<Vector<T, Size> >&			arg0,
											   const ExprP<Vector<T, Size> >&			arg1);
template<int Rows, int Cols, typename T>
ExprP<Vector<T, Rows> >				operator* (const ExprP<Vector<T, Cols> >&			left,
											   const ExprP<Matrix<T, Rows, Cols> >&		right);
template<int Rows, int Cols, typename T>
ExprP<Vector<T, Cols> >				operator* (const ExprP<Matrix<T, Rows, Cols> >&		left,
											   const ExprP<Vector<T, Rows> >&			right);
template<int Rows, int Cols, typename T>
ExprP<Matrix<T, Rows, Cols> >		operator* (const ExprP<Matrix<T, Rows, Cols> >&		left,
											   const ExprP<T>&							right);
template<int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols> >	operator+ (const ExprP<Matrix<float, Rows, Cols> >&	left,
											   const ExprP<Matrix<float, Rows, Cols> >&	right);
template<int Rows, int Cols>
ExprP<Matrix<deFloat16, Rows, Cols> >	operator+ (const ExprP<Matrix<deFloat16, Rows, Cols> >&	left,
												   const ExprP<Matrix<deFloat16, Rows, Cols> >&	right);
template<int Rows, int Cols>
ExprP<Matrix<double, Rows, Cols> >	operator+ (const ExprP<Matrix<double, Rows, Cols> >&	left,
											   const ExprP<Matrix<double, Rows, Cols> >&	right);
template<typename T, int Rows, int Cols>
ExprP<Matrix<T, Rows, Cols> >	operator- (const ExprP<Matrix<T, Rows, Cols> >&	mat);

//! @}

/*--------------------------------------------------------------------*//*!
 * \brief Variable expression.
 *
 * A variable is evaluated by looking up its range of possible values from an
 * environment.
 *//*--------------------------------------------------------------------*/
template <typename T>
class Variable : public Expr<T>
{
public:
	typedef typename Expr<T>::IVal IVal;

					Variable	(const string& name) : m_name (name) {}
	string			getName		(void)							const { return m_name; }

protected:
	void			doPrintExpr	(ostream& os)					const { os << m_name; }
	IVal			doEvaluate	(const EvalContext& ctx)		const
	{
		return ctx.env.lookup<T>(*this);
	}

private:
	string	m_name;
};

template <typename T>
VariableP<T> variable (const string& name)
{
	return VariableP<T>(new Variable<T>(name));
}

template <typename T>
VariableP<T> bindExpression (const string& name, ExpandContext& ctx, const ExprP<T>& expr)
{
	VariableP<T> var = ctx.genSym<T>(name);
	ctx.addStatement(variableDeclaration(var, expr));
	return var;
}

/*--------------------------------------------------------------------*//*!
 * \brief Constant expression.
 *
 * A constant is evaluated by rounding it to a set of possible values allowed
 * by the current floating point precision.
 *//*--------------------------------------------------------------------*/
template <typename T>
class Constant : public Expr<T>
{
public:
	typedef typename Expr<T>::IVal IVal;

			Constant		(const T& value) : m_value(value) {}

protected:
	void	doPrintExpr		(ostream& os) const			{ os << m_value; }
	IVal	doEvaluate		(const EvalContext&) const	{ return makeIVal(m_value); }

private:
	T		m_value;
};

template <typename T>
ExprP<T> constant (const T& value)
{
	return exprP(new Constant<T>(value));
}

//! Return a reference to a singleton void constant.
const ExprP<Void>& voidP (void)
{
	static const ExprP<Void> singleton = constant(Void());

	return singleton;
}

/*--------------------------------------------------------------------*//*!
 * \brief Four-element tuple.
 *
 * This is used for various things where we need one thing for each possible
 * function parameter. Currently the maximum supported number of parameters is
 * four.
 *//*--------------------------------------------------------------------*/
template <typename T0 = Void, typename T1 = Void, typename T2 = Void, typename T3 = Void>
struct Tuple4
{
	explicit Tuple4 (const T0 e0 = T0(),
					 const T1 e1 = T1(),
					 const T2 e2 = T2(),
					 const T3 e3 = T3())
		: a	(e0)
		, b	(e1)
		, c	(e2)
		, d	(e3)
	{
	}

	T0 a;
	T1 b;
	T2 c;
	T3 d;
};

/*--------------------------------------------------------------------*//*!
 * \brief Function signature.
 *
 * This is a purely compile-time structure used to bundle all types in a
 * function signature together. This makes passing the signature around in
 * templates easier, since we only need to take and pass a single Sig instead
 * of a bunch of parameter types and a return type.
 *
 *//*--------------------------------------------------------------------*/
template <typename R,
		  typename P0 = Void, typename P1 = Void,
		  typename P2 = Void, typename P3 = Void>
struct Signature
{
	typedef R							Ret;
	typedef P0							Arg0;
	typedef P1							Arg1;
	typedef P2							Arg2;
	typedef P3							Arg3;
	typedef typename Traits<Ret>::IVal	IRet;
	typedef typename Traits<Arg0>::IVal	IArg0;
	typedef typename Traits<Arg1>::IVal	IArg1;
	typedef typename Traits<Arg2>::IVal	IArg2;
	typedef typename Traits<Arg3>::IVal	IArg3;

	typedef Tuple4<	const Arg0&,	const Arg1&,	const Arg2&,	const Arg3&>	Args;
	typedef Tuple4<	const IArg0&,	const IArg1&,	const IArg2&,	const IArg3&>	IArgs;
	typedef Tuple4<	ExprP<Arg0>,	ExprP<Arg1>,	ExprP<Arg2>,	ExprP<Arg3> >	ArgExprs;
};

typedef vector<const ExprBase*> BaseArgExprs;

/*--------------------------------------------------------------------*//*!
 * \brief Type-independent operations for function objects.
 *
 *//*--------------------------------------------------------------------*/
class FuncBase
{
public:
	virtual				~FuncBase				(void)					{}
	virtual string		getName					(void)					const = 0;
	//! Name of extension that this function requires, or empty.
	virtual string		getRequiredExtension	(void)					const { return ""; }
	virtual Interval	getInputRange			(const bool is16bit)	const {DE_UNREF(is16bit); return Interval(true, -TCU_INFINITY, TCU_INFINITY); }
	virtual void		print					(ostream&,
												 const BaseArgExprs&)	const = 0;
	//! Index of output parameter, or -1 if none of the parameters is output.
	virtual int			getOutParamIndex		(void)					const { return -1; }

	virtual SpirVCaseT	getSpirvCase			(void)					const { return SPIRV_CASETYPE_NONE; }

	void				printDefinition			(ostream& os)			const
	{
		doPrintDefinition(os);
	}

	void				getUsedFuncs			(FuncSet& dst) const
	{
		this->doGetUsedFuncs(dst);
	}

protected:
	virtual void		doPrintDefinition		(ostream& os)			const = 0;
	virtual void		doGetUsedFuncs			(FuncSet& dst)			const = 0;
};

typedef Tuple4<string, string, string, string> ParamNames;

/*--------------------------------------------------------------------*//*!
 * \brief Function objects.
 *
 * Each Func object represents a GLSL function. It can be applied to interval
 * arguments, and it returns the an interval that is a conservative
 * approximation of the image of the GLSL function over the argument
 * intervals. That is, it is given a set of possible arguments and it returns
 * the set of possible values.
 *
 *//*--------------------------------------------------------------------*/
template <typename Sig_>
class Func : public FuncBase
{
public:
	typedef Sig_										Sig;
	typedef typename Sig::Ret							Ret;
	typedef typename Sig::Arg0							Arg0;
	typedef typename Sig::Arg1							Arg1;
	typedef typename Sig::Arg2							Arg2;
	typedef typename Sig::Arg3							Arg3;
	typedef typename Sig::IRet							IRet;
	typedef typename Sig::IArg0							IArg0;
	typedef typename Sig::IArg1							IArg1;
	typedef typename Sig::IArg2							IArg2;
	typedef typename Sig::IArg3							IArg3;
	typedef typename Sig::Args							Args;
	typedef typename Sig::IArgs							IArgs;
	typedef typename Sig::ArgExprs						ArgExprs;

	void				print			(ostream&			os,
										 const BaseArgExprs& args)				const
	{
		this->doPrint(os, args);
	}

	IRet				apply			(const EvalContext&	ctx,
										 const IArg0&		arg0 = IArg0(),
										 const IArg1&		arg1 = IArg1(),
										 const IArg2&		arg2 = IArg2(),
										 const IArg3&		arg3 = IArg3())		const
	{
		return this->applyArgs(ctx, IArgs(arg0, arg1, arg2, arg3));
	}

	IRet				fail			(const EvalContext&	ctx,
										 const IArg0&		arg0 = IArg0(),
										 const IArg1&		arg1 = IArg1(),
										 const IArg2&		arg2 = IArg2(),
										 const IArg3&		arg3 = IArg3())		const
	{
		return this->doFail(ctx, IArgs(arg0, arg1, arg2, arg3));
	}
	IRet				applyArgs		(const EvalContext&	ctx,
										 const IArgs&		args)				const
	{
		return this->doApply(ctx, args);
	}
	ExprP<Ret>			operator()		(const ExprP<Arg0>&		arg0 = voidP(),
										 const ExprP<Arg1>&		arg1 = voidP(),
										 const ExprP<Arg2>&		arg2 = voidP(),
										 const ExprP<Arg3>&		arg3 = voidP())		const;

	const ParamNames&	getParamNames	(void)									const
	{
		return this->doGetParamNames();
	}

protected:
	virtual IRet		doApply			(const EvalContext&,
										 const IArgs&)							const = 0;
	virtual IRet		doFail			(const EvalContext&	ctx,
										 const IArgs&		args)				const
	{
		return this->doApply(ctx, args);
	}
	virtual void		doPrint			(ostream& os, const BaseArgExprs& args)	const
	{
		os << getName() << "(";

		if (isTypeValid<Arg0>())
			os << *args[0];

		if (isTypeValid<Arg1>())
			os << ", " << *args[1];

		if (isTypeValid<Arg2>())
			os << ", " << *args[2];

		if (isTypeValid<Arg3>())
			os << ", " << *args[3];

		os << ")";
	}

	virtual const ParamNames&	doGetParamNames	(void)							const
	{
		static ParamNames	names	("a", "b", "c", "d");
		return names;
	}
};

template <typename Sig>
class Apply : public Expr<typename Sig::Ret>
{
public:
	typedef typename Sig::Ret				Ret;
	typedef typename Sig::Arg0				Arg0;
	typedef typename Sig::Arg1				Arg1;
	typedef typename Sig::Arg2				Arg2;
	typedef typename Sig::Arg3				Arg3;
	typedef typename Expr<Ret>::Val			Val;
	typedef typename Expr<Ret>::IVal		IVal;
	typedef Func<Sig>						ApplyFunc;
	typedef typename ApplyFunc::ArgExprs	ArgExprs;

						Apply	(const ApplyFunc&		func,
								 const ExprP<Arg0>&		arg0 = voidP(),
								 const ExprP<Arg1>&		arg1 = voidP(),
								 const ExprP<Arg2>&		arg2 = voidP(),
								 const ExprP<Arg3>&		arg3 = voidP())
							: m_func	(func),
							  m_args	(arg0, arg1, arg2, arg3) {}

						Apply	(const ApplyFunc&	func,
								 const ArgExprs&	args)
							: m_func	(func),
							  m_args	(args) {}
protected:
	void				doPrintExpr			(ostream& os) const
	{
		BaseArgExprs	args;
		args.push_back(m_args.a.get());
		args.push_back(m_args.b.get());
		args.push_back(m_args.c.get());
		args.push_back(m_args.d.get());
		m_func.print(os, args);
	}

	IVal				doEvaluate		(const EvalContext& ctx) const
	{
		return m_func.apply(ctx,
							m_args.a->evaluate(ctx), m_args.b->evaluate(ctx),
							m_args.c->evaluate(ctx), m_args.d->evaluate(ctx));
	}

	void				doGetUsedFuncs	(FuncSet& dst) const
	{
		m_func.getUsedFuncs(dst);
		m_args.a->getUsedFuncs(dst);
		m_args.b->getUsedFuncs(dst);
		m_args.c->getUsedFuncs(dst);
		m_args.d->getUsedFuncs(dst);
	}

	const ApplyFunc&	m_func;
	ArgExprs			m_args;
};

template<typename T>
class Alternatives : public Func<Signature<T, T, T> >
{
public:
	typedef typename	Alternatives::Sig		Sig;

protected:
	typedef typename	Alternatives::IRet		IRet;
	typedef typename	Alternatives::IArgs		IArgs;

	virtual string		getName				(void) const			{ return "alternatives"; }
	virtual void		doPrintDefinition	(std::ostream&) const	{}
	void				doGetUsedFuncs		(FuncSet&) const		{}

	virtual IRet		doApply				(const EvalContext&, const IArgs& args) const
	{
		return unionIVal<T>(args.a, args.b);
	}

	virtual void		doPrint				(ostream& os, const BaseArgExprs& args)	const
	{
		os << "{" << *args[0] << " | " << *args[1] << "}";
	}
};

template <typename Sig>
ExprP<typename Sig::Ret> createApply (const Func<Sig>&						func,
									  const typename Func<Sig>::ArgExprs&	args)
{
	return exprP(new Apply<Sig>(func, args));
}

template <typename Sig>
ExprP<typename Sig::Ret> createApply (
	const Func<Sig>&			func,
	const ExprP<typename Sig::Arg0>&	arg0 = voidP(),
	const ExprP<typename Sig::Arg1>&	arg1 = voidP(),
	const ExprP<typename Sig::Arg2>&	arg2 = voidP(),
	const ExprP<typename Sig::Arg3>&	arg3 = voidP())
{
	return exprP(new Apply<Sig>(func, arg0, arg1, arg2, arg3));
}

template <typename Sig>
ExprP<typename Sig::Ret> Func<Sig>::operator() (const ExprP<typename Sig::Arg0>& arg0,
												const ExprP<typename Sig::Arg1>& arg1,
												const ExprP<typename Sig::Arg2>& arg2,
												const ExprP<typename Sig::Arg3>& arg3) const
{
	return createApply(*this, arg0, arg1, arg2, arg3);
}

template <typename F>
ExprP<typename F::Ret> app (const ExprP<typename F::Arg0>& arg0 = voidP(),
							const ExprP<typename F::Arg1>& arg1 = voidP(),
							const ExprP<typename F::Arg2>& arg2 = voidP(),
							const ExprP<typename F::Arg3>& arg3 = voidP())
{
	return createApply(instance<F>(), arg0, arg1, arg2, arg3);
}

template <typename F>
typename F::IRet call (const EvalContext&			ctx,
					   const typename F::IArg0&		arg0 = Void(),
					   const typename F::IArg1&		arg1 = Void(),
					   const typename F::IArg2&		arg2 = Void(),
					   const typename F::IArg3&		arg3 = Void())
{
	return instance<F>().apply(ctx, arg0, arg1, arg2, arg3);
}

template <typename T>
ExprP<T> alternatives (const ExprP<T>& arg0,
					   const ExprP<T>& arg1)
{
	return createApply<typename Alternatives<T>::Sig>(instance<Alternatives<T> >(), arg0, arg1);
}

template <typename Sig>
class ApplyVar : public Apply<Sig>
{
public:
	typedef typename Sig::Ret				Ret;
	typedef typename Sig::Arg0				Arg0;
	typedef typename Sig::Arg1				Arg1;
	typedef typename Sig::Arg2				Arg2;
	typedef typename Sig::Arg3				Arg3;
	typedef typename Expr<Ret>::Val			Val;
	typedef typename Expr<Ret>::IVal		IVal;
	typedef Func<Sig>						ApplyFunc;
	typedef typename ApplyFunc::ArgExprs	ArgExprs;

						ApplyVar	(const ApplyFunc&			func,
									 const VariableP<Arg0>&		arg0,
									 const VariableP<Arg1>&		arg1,
									 const VariableP<Arg2>&		arg2,
									 const VariableP<Arg3>&		arg3)
							: Apply<Sig> (func, arg0, arg1, arg2, arg3) {}
protected:
	IVal				doEvaluate		(const EvalContext& ctx) const
	{
		const Variable<Arg0>&	var0 = static_cast<const Variable<Arg0>&>(*this->m_args.a);
		const Variable<Arg1>&	var1 = static_cast<const Variable<Arg1>&>(*this->m_args.b);
		const Variable<Arg2>&	var2 = static_cast<const Variable<Arg2>&>(*this->m_args.c);
		const Variable<Arg3>&	var3 = static_cast<const Variable<Arg3>&>(*this->m_args.d);
		return this->m_func.apply(ctx,
								  ctx.env.lookup(var0), ctx.env.lookup(var1),
								  ctx.env.lookup(var2), ctx.env.lookup(var3));
	}

	IVal				doFails		(const EvalContext& ctx) const
	{
		const Variable<Arg0>&	var0 = static_cast<const Variable<Arg0>&>(*this->m_args.a);
		const Variable<Arg1>&	var1 = static_cast<const Variable<Arg1>&>(*this->m_args.b);
		const Variable<Arg2>&	var2 = static_cast<const Variable<Arg2>&>(*this->m_args.c);
		const Variable<Arg3>&	var3 = static_cast<const Variable<Arg3>&>(*this->m_args.d);
		return this->m_func.fail(ctx,
								  ctx.env.lookup(var0), ctx.env.lookup(var1),
								  ctx.env.lookup(var2), ctx.env.lookup(var3));
	}
};

template <typename Sig>
ExprP<typename Sig::Ret> applyVar (const Func<Sig>&						func,
								   const VariableP<typename Sig::Arg0>&	arg0,
								   const VariableP<typename Sig::Arg1>&	arg1,
								   const VariableP<typename Sig::Arg2>&	arg2,
								   const VariableP<typename Sig::Arg3>&	arg3)
{
	return exprP(new ApplyVar<Sig>(func, arg0, arg1, arg2, arg3));
}

template <typename Sig_>
class DerivedFunc : public Func<Sig_>
{
public:
	typedef typename DerivedFunc::ArgExprs		ArgExprs;
	typedef typename DerivedFunc::IRet			IRet;
	typedef typename DerivedFunc::IArgs			IArgs;
	typedef typename DerivedFunc::Ret			Ret;
	typedef typename DerivedFunc::Arg0			Arg0;
	typedef typename DerivedFunc::Arg1			Arg1;
	typedef typename DerivedFunc::Arg2			Arg2;
	typedef typename DerivedFunc::Arg3			Arg3;
	typedef typename DerivedFunc::IArg0			IArg0;
	typedef typename DerivedFunc::IArg1			IArg1;
	typedef typename DerivedFunc::IArg2			IArg2;
	typedef typename DerivedFunc::IArg3			IArg3;

protected:
	void						doPrintDefinition	(ostream& os) const
	{
		const ParamNames&	paramNames	= this->getParamNames();

		initialize();

		os << dataTypeNameOf<Ret>() << " " << this->getName()
			<< "(";
		if (isTypeValid<Arg0>())
			os << dataTypeNameOf<Arg0>() << " " << paramNames.a;
		if (isTypeValid<Arg1>())
			os << ", " << dataTypeNameOf<Arg1>() << " " << paramNames.b;
		if (isTypeValid<Arg2>())
			os << ", " << dataTypeNameOf<Arg2>() << " " << paramNames.c;
		if (isTypeValid<Arg3>())
			os << ", " << dataTypeNameOf<Arg3>() << " " << paramNames.d;
		os << ")\n{\n";

		for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
			os << *m_body[ndx];
		os << "return " << *m_ret << ";\n";
		os << "}\n";
	}

	IRet						doApply			(const EvalContext&	ctx,
												 const IArgs&		args) const
	{
		Environment	funEnv;
		IArgs&		mutArgs		= const_cast<IArgs&>(args);
		IRet		ret;

		initialize();

		funEnv.bind(*m_var0, args.a);
		funEnv.bind(*m_var1, args.b);
		funEnv.bind(*m_var2, args.c);
		funEnv.bind(*m_var3, args.d);

		{
			EvalContext	funCtx(ctx.format, ctx.floatPrecision, funEnv, ctx.callDepth);

			for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
				m_body[ndx]->execute(funCtx);

			ret = m_ret->evaluate(funCtx);
		}

		// \todo [lauri] Store references instead of values in environment
		const_cast<IArg0&>(mutArgs.a) = funEnv.lookup(*m_var0);
		const_cast<IArg1&>(mutArgs.b) = funEnv.lookup(*m_var1);
		const_cast<IArg2&>(mutArgs.c) = funEnv.lookup(*m_var2);
		const_cast<IArg3&>(mutArgs.d) = funEnv.lookup(*m_var3);

		return ret;
	}

	void						doGetUsedFuncs	(FuncSet& dst) const
	{
		initialize();
		if (dst.insert(this).second)
		{
			for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
				m_body[ndx]->getUsedFuncs(dst);
			m_ret->getUsedFuncs(dst);
		}
	}

	virtual ExprP<Ret>			doExpand		(ExpandContext& ctx, const ArgExprs& args_) const = 0;

	// These are transparently initialized when first needed. They cannot be
	// initialized in the constructor because they depend on the doExpand
	// method of the subclass.

	mutable VariableP<Arg0>		m_var0;
	mutable VariableP<Arg1>		m_var1;
	mutable VariableP<Arg2>		m_var2;
	mutable VariableP<Arg3>		m_var3;
	mutable vector<StatementP>	m_body;
	mutable ExprP<Ret>			m_ret;

private:

	void				initialize		(void)	const
	{
		if (!m_ret)
		{
			const ParamNames&	paramNames	= this->getParamNames();
			Counter				symCounter;
			ExpandContext		ctx			(symCounter);
			ArgExprs			args;

			args.a	= m_var0 = variable<Arg0>(paramNames.a);
			args.b	= m_var1 = variable<Arg1>(paramNames.b);
			args.c	= m_var2 = variable<Arg2>(paramNames.c);
			args.d	= m_var3 = variable<Arg3>(paramNames.d);

			m_ret	= this->doExpand(ctx, args);
			m_body	= ctx.getStatements();
		}
	}
};

template <typename Sig>
class PrimitiveFunc : public Func<Sig>
{
public:
	typedef typename PrimitiveFunc::Ret			Ret;
	typedef typename PrimitiveFunc::ArgExprs	ArgExprs;

protected:
	void	doPrintDefinition	(ostream&) const	{}
	void	doGetUsedFuncs		(FuncSet&) const	{}
};

template <typename T>
class Cond : public PrimitiveFunc<Signature<T, bool, T, T> >
{
public:
	typedef typename Cond::IArgs	IArgs;
	typedef typename Cond::IRet		IRet;

	string	getName	(void) const
	{
		return "_cond";
	}

protected:

	void	doPrint	(ostream& os, const BaseArgExprs& args) const
	{
		os << "(" << *args[0] << " ? " << *args[1] << " : " << *args[2] << ")";
	}

	IRet	doApply	(const EvalContext&, const IArgs& iargs)const
	{
		IRet	ret;

		if (iargs.a.contains(true))
			ret = unionIVal<T>(ret, iargs.b);

		if (iargs.a.contains(false))
			ret = unionIVal<T>(ret, iargs.c);

		return ret;
	}
};

template <typename T>
class CompareOperator : public PrimitiveFunc<Signature<bool, T, T> >
{
public:
	typedef typename CompareOperator::IArgs	IArgs;
	typedef typename CompareOperator::IArg0	IArg0;
	typedef typename CompareOperator::IArg1	IArg1;
	typedef typename CompareOperator::IRet	IRet;

protected:
	void			doPrint	(ostream& os, const BaseArgExprs& args) const
	{
		os << "(" << *args[0] << getSymbol() << *args[1] << ")";
	}

	Interval		doApply	(const EvalContext&, const IArgs& iargs) const
	{
		const IArg0&	arg0 = iargs.a;
		const IArg1&	arg1 = iargs.b;
		IRet	ret;

		if (canSucceed(arg0, arg1))
			ret |= true;
		if (canFail(arg0, arg1))
			ret |= false;

		return ret;
	}

	virtual string	getSymbol	(void) const = 0;
	virtual bool	canSucceed	(const IArg0&, const IArg1&) const = 0;
	virtual bool	canFail		(const IArg0&, const IArg1&) const = 0;
};

template <typename T>
class LessThan : public CompareOperator<T>
{
public:
	string	getName		(void) const									{ return "lessThan"; }

protected:
	string	getSymbol	(void) const									{ return "<";		}

	bool	canSucceed	(const Interval& a, const Interval& b) const
	{
		return (a.lo() < b.hi());
	}

	bool	canFail		(const Interval& a, const Interval& b) const
	{
		return !(a.hi() < b.lo());
	}
};

template <typename T>
ExprP<bool> operator< (const ExprP<T>& a, const ExprP<T>& b)
{
	return app<LessThan<T> >(a, b);
}

template <typename T>
ExprP<T> cond (const ExprP<bool>&	test,
			   const ExprP<T>&		consequent,
			   const ExprP<T>&		alternative)
{
	return app<Cond<T> >(test, consequent, alternative);
}

/*--------------------------------------------------------------------*//*!
 *
 * @}
 *
 *//*--------------------------------------------------------------------*/
//Proper parameters for template T
//	Signature<float, float>		32bit tests
//	Signature<float, deFloat16>	16bit tests
//	Signature<double, double>	64bit tests
template< class T>
class FloatFunc1 : public PrimitiveFunc<T>
{
protected:
		Interval			doApply			(const EvalContext& ctx, const typename Signature<typename T::Ret, typename T::Arg0>::IArgs& iargs) const
	{
		return this->applyMonotone(ctx, iargs.a);
	}

	Interval			applyMonotone	(const EvalContext& ctx, const Interval& iarg0) const
	{
		Interval ret;

		TCU_INTERVAL_APPLY_MONOTONE1(ret, arg0, iarg0, val,
									 TCU_SET_INTERVAL(val, point,
													  point = this->applyPoint(ctx, arg0)));

		ret |= innerExtrema(ctx, iarg0);
		ret &= (this->getCodomain(ctx) | TCU_NAN);

		return ctx.format.convert(ret);
	}

	virtual Interval	innerExtrema	(const EvalContext&, const Interval&) const
	{
		return Interval(); // empty interval, i.e. no extrema
	}

	virtual Interval	applyPoint		(const EvalContext& ctx, double arg0) const
	{
		const double	exact	= this->applyExact(arg0);
		const double	prec	= this->precision(ctx, exact, arg0);

		return exact + Interval(-prec, prec);
	}

	virtual double		applyExact		(double) const
	{
		TCU_THROW(InternalError, "Cannot apply");
	}

	virtual Interval	getCodomain		(const EvalContext&) const
	{
		return Interval::unbounded(true);
	}

	virtual double		precision		(const EvalContext& ctx, double, double) const = 0;
};

/*Proper parameters for template T
	Signature<double, double>	64bit tests
	Signature<float, float>		32bit tests
	Signature<float, deFloat16>	16bit tests*/
template <class T>
class CFloatFunc1 : public FloatFunc1<T>
{
public:
						CFloatFunc1	(const string& name, tcu::DoubleFunc1& func)
							: m_name(name), m_func(func) {}

	string				getName		(void) const		{ return m_name; }

protected:
	double				applyExact	(double x) const	{ return m_func(x); }

	const string		m_name;
	tcu::DoubleFunc1&	m_func;
};

//<Signature<float, deFloat16, deFloat16> >
//<Signature<float, float, float> >
//<Signature<double, double, double> >
template <class T>
class FloatFunc2 : public PrimitiveFunc<T>
{
protected:
	Interval			doApply			(const EvalContext&	ctx, const typename Signature<typename T::Ret, typename T::Arg0, typename T::Arg1>::IArgs& iargs) const
	{
		return this->applyMonotone(ctx, iargs.a, iargs.b);
	}

	Interval			applyMonotone	(const EvalContext&	ctx,
										 const Interval&	xi,
										 const Interval&	yi) const
	{
		Interval reti;

		TCU_INTERVAL_APPLY_MONOTONE2(reti, x, xi, y, yi, ret,
									 TCU_SET_INTERVAL(ret, point,
													  point = this->applyPoint(ctx, x, y)));
		reti |= innerExtrema(ctx, xi, yi);
		reti &= (this->getCodomain(ctx) | TCU_NAN);

		return ctx.format.convert(reti);
	}

	virtual Interval	innerExtrema	(const EvalContext&,
										 const Interval&,
										 const Interval&) const
	{
		return Interval(); // empty interval, i.e. no extrema
	}

	virtual Interval	applyPoint		(const EvalContext&	ctx,
										 double				x,
										 double				y) const
	{
		const double exact	= this->applyExact(x, y);
		const double prec	= this->precision(ctx, exact, x, y);

		return exact + Interval(-prec, prec);
	}

	virtual double		applyExact		(double, double) const
	{
		TCU_THROW(InternalError, "Cannot apply");
	}

	virtual Interval	getCodomain		(const EvalContext&) const
	{
		return Interval::unbounded(true);
	}

	virtual double		precision		(const EvalContext&	ctx,
										 double				ret,
										 double				x,
										 double				y) const = 0;
};

template <class T>
class CFloatFunc2 : public FloatFunc2<T>
{
public:
						CFloatFunc2	(const string&		name,
									 tcu::DoubleFunc2&	func)
							: m_name(name)
							, m_func(func)
	{
	}

	string				getName		(void) const						{ return m_name; }

protected:
	double				applyExact	(double x, double y) const			{ return m_func(x, y); }

	const string		m_name;
	tcu::DoubleFunc2&	m_func;
};

template <class T>
class InfixOperator : public FloatFunc2<T>
{
protected:
	virtual string	getSymbol		(void) const = 0;

	void			doPrint			(ostream& os, const BaseArgExprs& args) const
	{
		os << "(" << *args[0] << " " << getSymbol() << " " << *args[1] << ")";
	}

	Interval		applyPoint		(const EvalContext&	ctx,
									 double				x,
									 double				y) const
	{
		const double exact	= this->applyExact(x, y);

		// Allow either representable number on both sides of the exact value,
		// but require exactly representable values to be preserved.
		return ctx.format.roundOut(exact, !deIsInf(x) && !deIsInf(y));
	}

	double			precision		(const EvalContext&, double, double, double) const
	{
		return 0.0;
	}
};

class InfixOperator16Bit : public FloatFunc2 <Signature<float, deFloat16, deFloat16> >
{
protected:
	virtual string	getSymbol		(void) const = 0;

	void			doPrint			(ostream& os, const BaseArgExprs& args) const
	{
		os << "(" << *args[0] << " " << getSymbol() << " " << *args[1] << ")";
	}

	Interval		applyPoint		(const EvalContext&	ctx,
									 double				x,
									 double				y) const
	{
		const double exact	= this->applyExact(x, y);

		// Allow either representable number on both sides of the exact value,
		// but require exactly representable values to be preserved.
		return ctx.format.roundOut(exact, !deIsInf(x) && !deIsInf(y));
	}

	double			precision		(const EvalContext&, double, double, double) const
	{
		return 0.0;
	}
};

template <class T>
class FloatFunc3 : public PrimitiveFunc<T>
{
protected:
	Interval			doApply			(const EvalContext&	ctx, const typename Signature<typename T::Ret, typename T::Arg0, typename T::Arg1, typename T::Arg2>::IArgs& iargs) const
	{
		return this->applyMonotone(ctx, iargs.a, iargs.b, iargs.c);
	}

	Interval			applyMonotone	(const EvalContext&	ctx,
										 const Interval&	xi,
										 const Interval&	yi,
										 const Interval&	zi) const
	{
		Interval reti;
		TCU_INTERVAL_APPLY_MONOTONE3(reti, x, xi, y, yi, z, zi, ret,
									 TCU_SET_INTERVAL(ret, point,
													  point = this->applyPoint(ctx, x, y, z)));
		return ctx.format.convert(reti);
	}

	virtual Interval	applyPoint		(const EvalContext&	ctx,
										 double				x,
										 double				y,
										 double				z) const
	{
		const double exact	= this->applyExact(x, y, z);
		const double prec	= this->precision(ctx, exact, x, y, z);
		return exact + Interval(-prec, prec);
	}

	virtual double		applyExact		(double, double, double) const
	{
		TCU_THROW(InternalError, "Cannot apply");
	}

	virtual double		precision		(const EvalContext&	ctx,
										 double				result,
										 double				x,
										 double				y,
										 double				z) const = 0;
};

// We define syntactic sugar functions for expression constructors. Since
// these have the same names as ordinary mathematical operations (sin, log
// etc.), it's better to give them a dedicated namespace.
namespace Functions
{

using namespace tcu;

template <class T>
class Comparison : public InfixOperator < T >
{
public:
	string		getName			(void) const	{ return "comparison"; }
	string		getSymbol		(void) const	{ return ""; }

	SpirVCaseT	getSpirvCase	() const		{ return SPIRV_CASETYPE_COMPARE; }

	Interval	doApply			(const EvalContext&						ctx,
								 const typename Comparison<T>::IArgs&	iargs) const
	{
		DE_UNREF(ctx);
		if (iargs.a.hasNaN() || iargs.b.hasNaN())
		{
			return TCU_NAN; // one of the floats is NaN: block analysis
		}

		int operationFlag = 1;
		int result = 0;
		const double a = iargs.a.midpoint();
		const double b = iargs.b.midpoint();

		for (int i = 0; i<2; ++i)
		{
			if (a == b)
				result += operationFlag;
			operationFlag = operationFlag << 1;

			if (a > b)
				result += operationFlag;
			operationFlag = operationFlag << 1;

			if (a < b)
				result += operationFlag;
			operationFlag = operationFlag << 1;

			if (a >= b)
				result += operationFlag;
			operationFlag = operationFlag << 1;

			if (a <= b)
				result += operationFlag;
			operationFlag = operationFlag << 1;
		}
		return result;
	}
};

template <class T>
class Add : public InfixOperator < T >
{
public:
	string		getName		(void) const						{ return "add"; }
	string		getSymbol	(void) const						{ return "+"; }

	Interval	doApply		(const EvalContext&	ctx,
							 const typename Signature<typename T::Ret, typename T::Arg0, typename T::Arg1>::IArgs& iargs) const
	{
		// Fast-path for common case
		if (iargs.a.isOrdinary() && iargs.b.isOrdinary())
		{
			Interval ret;
			TCU_SET_INTERVAL_BOUNDS(ret, sum,
									sum = iargs.a.lo() + iargs.b.lo(),
									sum = iargs.a.hi() + iargs.b.hi());
			return ctx.format.convert(ctx.format.roundOut(ret, true));
		}
		return this->applyMonotone(ctx, iargs.a, iargs.b);
	}

protected:
	double		applyExact	(double x, double y) const			{ return x + y; }
};

template<class T>
class Mul : public InfixOperator<T>
{
public:
	string		getName		(void) const									{ return "mul"; }
	string		getSymbol	(void) const									{ return "*"; }

	Interval	doApply		(const EvalContext&	ctx, const typename Signature<typename T::Ret, typename T::Arg0, typename T::Arg1>::IArgs& iargs) const
	{
		Interval a = iargs.a;
		Interval b = iargs.b;

		// Fast-path for common case
		if (a.isOrdinary() && b.isOrdinary())
		{
			Interval ret;
			if (a.hi() < 0)
			{
				a = -a;
				b = -b;
			}
			if (a.lo() >= 0 && b.lo() >= 0)
			{
				TCU_SET_INTERVAL_BOUNDS(ret, prod,
										prod = a.lo() * b.lo(),
										prod = a.hi() * b.hi());
				return ctx.format.convert(ctx.format.roundOut(ret, true));
			}
			if (a.lo() >= 0 && b.hi() <= 0)
			{
				TCU_SET_INTERVAL_BOUNDS(ret, prod,
										prod = a.hi() * b.lo(),
										prod = a.lo() * b.hi());
				return ctx.format.convert(ctx.format.roundOut(ret, true));
			}
		}
		return this->applyMonotone(ctx, iargs.a, iargs.b);
	}

protected:
	double		applyExact	(double x, double y) const						{ return x * y; }

	Interval	innerExtrema(const EvalContext&, const Interval& xi, const Interval& yi) const
	{
		if (((xi.contains(-TCU_INFINITY) || xi.contains(TCU_INFINITY)) && yi.contains(0.0)) ||
			((yi.contains(-TCU_INFINITY) || yi.contains(TCU_INFINITY)) && xi.contains(0.0)))
			return Interval(TCU_NAN);

		return Interval();
	}
};

template<class T>
class Sub : public InfixOperator <T>
{
public:
	string		getName		(void) const				{ return "sub"; }
	string		getSymbol	(void) const				{ return "-"; }

	Interval	doApply		(const EvalContext&	ctx, const typename Signature<typename T::Ret, typename T::Arg0, typename T::Arg1>::IArgs& iargs) const
	{
		// Fast-path for common case
		if (iargs.a.isOrdinary() && iargs.b.isOrdinary())
		{
			Interval ret;

			TCU_SET_INTERVAL_BOUNDS(ret, diff,
									diff = iargs.a.lo() - iargs.b.hi(),
									diff = iargs.a.hi() - iargs.b.lo());
			return ctx.format.convert(ctx.format.roundOut(ret, true));

		}
		else
		{
			return this->applyMonotone(ctx, iargs.a, iargs.b);
		}
	}

protected:
	double		applyExact	(double x, double y) const	{ return x - y; }
};

template <class T>
class Negate : public FloatFunc1<T>
{
public:
	string	getName		(void) const									{ return "_negate"; }
	void	doPrint		(ostream& os, const BaseArgExprs& args) const	{ os << "-" << *args[0]; }

protected:
	double	precision	(const EvalContext&, double, double) const		{ return 0.0; }
	double	applyExact	(double x) const								{ return -x; }
};

template <class T>
class Div : public InfixOperator<T>
{
public:
	string		getName			(void) const						{ return "div"; }

protected:
	string		getSymbol		(void) const						{ return "/"; }

	Interval	innerExtrema	(const EvalContext&,
								 const Interval&		nom,
								 const Interval&		den) const
	{
		Interval ret;

		if (den.contains(0.0))
		{
			if (nom.contains(0.0))
				ret |= TCU_NAN;

			if (nom.lo() < 0.0 || nom.hi() > 0.0)
				ret |= Interval::unbounded();
		}

		return ret;
	}

	double		applyExact		(double x, double y) const { return x / y; }

	Interval	applyPoint		(const EvalContext&	ctx, double x, double y) const
	{
		Interval ret = FloatFunc2<T>::applyPoint(ctx, x, y);

		if (!deIsInf(x) && !deIsInf(y) && y != 0.0)
		{
			const Interval dst = ctx.format.convert(ret);
			if (dst.contains(-TCU_INFINITY)) ret |= -ctx.format.getMaxValue();
			if (dst.contains(+TCU_INFINITY)) ret |= +ctx.format.getMaxValue();
		}

		return ret;
	}

	double		precision		(const EvalContext& ctx, double ret, double, double den) const
	{
		const FloatFormat&	fmt		= ctx.format;

		// \todo [2014-03-05 lauri] Check that the limits in GLSL 3.10 are actually correct.
		// For now, we assume that division's precision is 2.5 ULP when the value is within
		// [2^MINEXP, 2^MAXEXP-1]

		if (den == 0.0)
			return 0.0; // Result must be exactly inf
		else if (de::inBounds(deAbs(den),
							  deLdExp(1.0, fmt.getMinExp()),
							  deLdExp(1.0, fmt.getMaxExp() - 1)))
			return fmt.ulp(ret, 2.5);
		else
			return TCU_INFINITY; // Can be any number, but must be a number.
	}
};

template <class T>
class InverseSqrt : public FloatFunc1 <T>
{
public:
	string		getName		(void) const							{ return "inversesqrt"; }

protected:
	double		applyExact	(double x) const						{ return 1.0 / deSqrt(x); }

	double		precision	(const EvalContext& ctx, double ret, double x) const
	{
		return x <= 0 ? TCU_NAN : ctx.format.ulp(ret, 2.0);
	}

	Interval	getCodomain	(const EvalContext&) const
	{
		return Interval(0.0, TCU_INFINITY);
	}
};

template <class T>
class ExpFunc : public CFloatFunc1<T>
{
public:
				ExpFunc		(const string& name, DoubleFunc1& func)
					: CFloatFunc1<T> (name, func)
				{}
protected:
	double		precision	(const EvalContext& ctx, double ret, double x) const;
	Interval	getCodomain	(const EvalContext&) const
	{
		return Interval(0.0, TCU_INFINITY);
	}
};

template <>
double ExpFunc <Signature<float, float> >::precision (const EvalContext& ctx, double ret, double x) const
{
	switch (ctx.floatPrecision)
	{
	case glu::PRECISION_HIGHP:
		return ctx.format.ulp(ret, 3.0 + 2.0 * deAbs(x));
	case glu::PRECISION_MEDIUMP:
	case glu::PRECISION_LAST:
		return ctx.format.ulp(ret, 1.0 + 2.0 * deAbs(x));
	default:
		DE_FATAL("Impossible");
	}

	return 0.0;
}

template <>
double ExpFunc <Signature<deFloat16, deFloat16> >::precision(const EvalContext& ctx, double ret, double x) const
{
	return ctx.format.ulp(ret, 1.0 + 2.0 * deAbs(x));
}

template <>
double ExpFunc <Signature<double, double> >::precision(const EvalContext& ctx, double ret, double x) const
{
	return ctx.format.ulp(ret, 1.0 + 2.0 * deAbs(x));
}

template <class T>
class Exp2	: public ExpFunc<T>	{ public: Exp2 (void)	: ExpFunc<T>("exp2", deExp2) {} };
template <class T>
class Exp	: public ExpFunc<T>	{ public: Exp (void)	: ExpFunc<T>("exp", deExp) {} };

template <typename T>
ExprP<T> exp2	(const ExprP<T>& x)	{ return app<Exp2< Signature<T, T> > >(x); }
template <typename T>
ExprP<T> exp	(const ExprP<T>& x)	{ return app<Exp< Signature<T, T> > >(x); }

template <class T>
class LogFunc : public CFloatFunc1<T>
{
public:
				LogFunc		(const string& name, DoubleFunc1& func)
					: CFloatFunc1<T>(name, func) {}

protected:
	double		precision	(const EvalContext& ctx, double ret, double x) const;
};

template <>
double LogFunc<Signature<float, float> >::precision(const EvalContext& ctx, double ret, double x) const
{
	if (x <= 0)
		return TCU_NAN;

	switch (ctx.floatPrecision)
	{
	case glu::PRECISION_HIGHP:
		return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -21) : ctx.format.ulp(ret, 3.0);
	case glu::PRECISION_MEDIUMP:
	case glu::PRECISION_LAST:
		return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -7) : ctx.format.ulp(ret, 3.0);
	default:
		DE_FATAL("Impossible");
	}

	return 0;
}

template <>
double LogFunc<Signature<deFloat16, deFloat16> >::precision(const EvalContext& ctx, double ret, double x) const
{
	if (x <= 0)
		return TCU_NAN;
	return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -7) : ctx.format.ulp(ret, 3.0);
}

// Spec: "The precision of double-precision instructions is at least that of single precision."
// Lets pick float high precision as a reference.
template <>
double LogFunc<Signature<double, double> >::precision(const EvalContext& ctx, double ret, double x) const
{
	if (x <= 0)
		return TCU_NAN;
	return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -21) : ctx.format.ulp(ret, 3.0);
}

template <class T>
class Log2	: public LogFunc<T>		{ public: Log2	(void) : LogFunc<T>("log2", deLog2) {} };
template <class T>
class Log	: public LogFunc<T>		{ public: Log	(void) : LogFunc<T>("log", deLog) {} };

ExprP<float> log2	(const ExprP<float>& x)	{ return app<Log2< Signature<float, float> > >(x); }
ExprP<float> log	(const ExprP<float>& x)	{ return app<Log< Signature<float, float> > >(x); }

ExprP<deFloat16> log2	(const ExprP<deFloat16>& x)	{ return app<Log2< Signature<deFloat16, deFloat16> > >(x); }
ExprP<deFloat16> log	(const ExprP<deFloat16>& x)	{ return app<Log< Signature<deFloat16, deFloat16> > >(x); }

ExprP<double> log2	(const ExprP<double>& x)	{ return app<Log2< Signature<double, double> > >(x); }
ExprP<double> log	(const ExprP<double>& x)	{ return app<Log< Signature<double, double> > >(x); }

#define DEFINE_CONSTRUCTOR1(CLASS, TRET, NAME, T0) \
ExprP<TRET> NAME (const ExprP<T0>& arg0) { return app<CLASS>(arg0); }

#define DEFINE_DERIVED1(CLASS, TRET, NAME, T0, ARG0, EXPANSION)				\
class CLASS : public DerivedFunc<Signature<TRET, T0> > /* NOLINT(CLASS) */	\
{																			\
public:																		\
	string			getName		(void) const		{ return #NAME; }		\
																			\
protected:																	\
	ExprP<TRET>		doExpand		(ExpandContext&,						\
									 const CLASS::ArgExprs& args_) const	\
	{																		\
		const ExprP<T0>& ARG0 = args_.a;									\
		return EXPANSION;													\
	}																		\
};																			\
DEFINE_CONSTRUCTOR1(CLASS, TRET, NAME, T0)

#define DEFINE_DERIVED_DOUBLE1(CLASS, NAME, ARG0, EXPANSION) \
	DEFINE_DERIVED1(CLASS, double, NAME, double, ARG0, EXPANSION)

#define DEFINE_DERIVED_FLOAT1(CLASS, NAME, ARG0, EXPANSION) \
	DEFINE_DERIVED1(CLASS, float, NAME, float, ARG0, EXPANSION)


#define DEFINE_DERIVED1_INPUTRANGE(CLASS, TRET, NAME, T0, ARG0, EXPANSION, INTERVAL)	\
class CLASS : public DerivedFunc<Signature<TRET, T0> > /* NOLINT(CLASS) */				\
{																						\
public:																					\
	string			getName		(void) const		{ return #NAME; }					\
																						\
protected:																				\
	ExprP<TRET>		doExpand		(ExpandContext&,									\
									 const CLASS::ArgExprs& args_) const				\
	{																					\
		const ExprP<T0>& ARG0 = args_.a;												\
		return EXPANSION;																\
	}																					\
	Interval	getInputRange	(const bool /*is16bit*/) const							\
	{																					\
		return INTERVAL;																\
	}																					\
};																						\
DEFINE_CONSTRUCTOR1(CLASS, TRET, NAME, T0)

#define DEFINE_DERIVED_FLOAT1_INPUTRANGE(CLASS, NAME, ARG0, EXPANSION, INTERVAL) \
	DEFINE_DERIVED1_INPUTRANGE(CLASS, float, NAME, float, ARG0, EXPANSION, INTERVAL)

#define DEFINE_DERIVED_DOUBLE1_INPUTRANGE(CLASS, NAME, ARG0, EXPANSION, INTERVAL) \
	DEFINE_DERIVED1_INPUTRANGE(CLASS, double, NAME, double, ARG0, EXPANSION, INTERVAL)

#define DEFINE_DERIVED_FLOAT1_16BIT(CLASS, NAME, ARG0, EXPANSION) \
	DEFINE_DERIVED1(CLASS, deFloat16, NAME, deFloat16, ARG0, EXPANSION)

#define DEFINE_DERIVED_FLOAT1_INPUTRANGE_16BIT(CLASS, NAME, ARG0, EXPANSION, INTERVAL) \
	DEFINE_DERIVED1_INPUTRANGE(CLASS, deFloat16, NAME, deFloat16, ARG0, EXPANSION, INTERVAL)

#define DEFINE_CONSTRUCTOR2(CLASS, TRET, NAME, T0, T1)				\
ExprP<TRET> NAME (const ExprP<T0>& arg0, const ExprP<T1>& arg1)		\
{																	\
	return app<CLASS>(arg0, arg1);									\
}

#define DEFINE_CASED_DERIVED2(CLASS, TRET, NAME, T0, Arg0, T1, Arg1, EXPANSION, SPIRVCASE) \
class CLASS : public DerivedFunc<Signature<TRET, T0, T1> > /* NOLINT(CLASS) */ \
{																		\
public:																	\
	string			getName		(void) const	{ return #NAME; }		\
																		\
	SpirVCaseT		getSpirvCase(void) const	{ return SPIRVCASE; }	\
																		\
protected:																\
	ExprP<TRET>		doExpand	(ExpandContext&, const ArgExprs& args_) const \
	{																	\
		const ExprP<T0>& Arg0 = args_.a;								\
		const ExprP<T1>& Arg1 = args_.b;								\
		return EXPANSION;												\
	}																	\
};																		\
DEFINE_CONSTRUCTOR2(CLASS, TRET, NAME, T0, T1)

#define DEFINE_DERIVED2(CLASS, TRET, NAME, T0, Arg0, T1, Arg1, EXPANSION) \
	DEFINE_CASED_DERIVED2(CLASS, TRET, NAME, T0, Arg0, T1, Arg1, EXPANSION, SPIRV_CASETYPE_NONE)

#define DEFINE_DERIVED_DOUBLE2(CLASS, NAME, Arg0, Arg1, EXPANSION)		\
	DEFINE_DERIVED2(CLASS, double, NAME, double, Arg0, double, Arg1, EXPANSION)

#define DEFINE_DERIVED_FLOAT2(CLASS, NAME, Arg0, Arg1, EXPANSION)		\
	DEFINE_DERIVED2(CLASS, float, NAME, float, Arg0, float, Arg1, EXPANSION)

#define DEFINE_DERIVED_FLOAT2_16BIT(CLASS, NAME, Arg0, Arg1, EXPANSION)		\
	DEFINE_DERIVED2(CLASS, deFloat16, NAME, deFloat16, Arg0, deFloat16, Arg1, EXPANSION)

#define DEFINE_CASED_DERIVED_FLOAT2(CLASS, NAME, Arg0, Arg1, EXPANSION, SPIRVCASE) \
	DEFINE_CASED_DERIVED2(CLASS, float, NAME, float, Arg0, float, Arg1, EXPANSION, SPIRVCASE)

#define DEFINE_CASED_DERIVED_FLOAT2_16BIT(CLASS, NAME, Arg0, Arg1, EXPANSION, SPIRVCASE) \
	DEFINE_CASED_DERIVED2(CLASS, deFloat16, NAME, deFloat16, Arg0, deFloat16, Arg1, EXPANSION, SPIRVCASE)

#define DEFINE_CASED_DERIVED_DOUBLE2(CLASS, NAME, Arg0, Arg1, EXPANSION, SPIRVCASE) \
	DEFINE_CASED_DERIVED2(CLASS, double, NAME, double, Arg0, double, Arg1, EXPANSION, SPIRVCASE)

#define DEFINE_CONSTRUCTOR3(CLASS, TRET, NAME, T0, T1, T2)				\
ExprP<TRET> NAME (const ExprP<T0>& arg0, const ExprP<T1>& arg1, const ExprP<T2>& arg2) \
{																		\
	return app<CLASS>(arg0, arg1, arg2);								\
}

#define DEFINE_DERIVED3(CLASS, TRET, NAME, T0, ARG0, T1, ARG1, T2, ARG2, EXPANSION) \
class CLASS : public DerivedFunc<Signature<TRET, T0, T1, T2> > /* NOLINT(CLASS) */ \
{																				\
public:																			\
	string			getName		(void) const	{ return #NAME; }				\
																				\
protected:																		\
	ExprP<TRET>		doExpand	(ExpandContext&, const ArgExprs& args_) const	\
	{																			\
		const ExprP<T0>& ARG0 = args_.a;										\
		const ExprP<T1>& ARG1 = args_.b;										\
		const ExprP<T2>& ARG2 = args_.c;										\
		return EXPANSION;														\
	}																			\
};																				\
DEFINE_CONSTRUCTOR3(CLASS, TRET, NAME, T0, T1, T2)

#define DEFINE_DERIVED_DOUBLE3(CLASS, NAME, ARG0, ARG1, ARG2, EXPANSION)			\
	DEFINE_DERIVED3(CLASS, double, NAME, double, ARG0, double, ARG1, double, ARG2, EXPANSION)

#define DEFINE_DERIVED_FLOAT3(CLASS, NAME, ARG0, ARG1, ARG2, EXPANSION)			\
	DEFINE_DERIVED3(CLASS, float, NAME, float, ARG0, float, ARG1, float, ARG2, EXPANSION)

#define DEFINE_DERIVED_FLOAT3_16BIT(CLASS, NAME, ARG0, ARG1, ARG2, EXPANSION)			\
	DEFINE_DERIVED3(CLASS, deFloat16, NAME, deFloat16, ARG0, deFloat16, ARG1, deFloat16, ARG2, EXPANSION)

#define DEFINE_CONSTRUCTOR4(CLASS, TRET, NAME, T0, T1, T2, T3)			\
ExprP<TRET> NAME (const ExprP<T0>& arg0, const ExprP<T1>& arg1,			\
				  const ExprP<T2>& arg2, const ExprP<T3>& arg3)			\
{																		\
	return app<CLASS>(arg0, arg1, arg2, arg3);							\
}

typedef	 InverseSqrt< Signature<deFloat16, deFloat16> >	InverseSqrt16Bit;
typedef	 InverseSqrt< Signature<float, float> >			InverseSqrt32Bit;
typedef InverseSqrt< Signature<double, double> >		InverseSqrt64Bit;

DEFINE_DERIVED_FLOAT1(Sqrt32Bit,		sqrt,		x,		constant(1.0f) / app<InverseSqrt32Bit>(x));
DEFINE_DERIVED_FLOAT1_16BIT(Sqrt16Bit,	sqrt,		x,		constant((deFloat16)FLOAT16_1_0) / app<InverseSqrt16Bit>(x));
DEFINE_DERIVED_DOUBLE1(Sqrt64Bit,		sqrt,		x,		constant(1.0) / app<InverseSqrt64Bit>(x));
DEFINE_DERIVED_FLOAT2(Pow,				pow,		x,	y,	exp2<float>(y * log2(x)));
DEFINE_DERIVED_FLOAT2_16BIT(Pow16,		pow,		x,	y,	exp2<deFloat16>(y * log2(x)));
DEFINE_DERIVED_DOUBLE2(Pow64,			pow,		x,	y,	exp2<double>(y * log2(x)));
DEFINE_DERIVED_FLOAT1(Radians,			radians,	d,		(constant(DE_PI) / constant(180.0f)) * d);
DEFINE_DERIVED_FLOAT1_16BIT(Radians16,	radians,	d,		(constant((deFloat16)DE_PI_16BIT) / constant((deFloat16)FLOAT16_180_0)) * d);
DEFINE_DERIVED_DOUBLE1(Radians64,		radians,	d,		(constant((double)(DE_PI)) / constant(180.0)) * d);
DEFINE_DERIVED_FLOAT1(Degrees,			degrees,	r,		(constant(180.0f) / constant(DE_PI)) * r);
DEFINE_DERIVED_FLOAT1_16BIT(Degrees16,	degrees,	r,		(constant((deFloat16)FLOAT16_180_0) / constant((deFloat16)DE_PI_16BIT)) * r);
DEFINE_DERIVED_DOUBLE1(Degrees64,		degrees,	r,		(constant(180.0) / constant((double)(DE_PI))) * r);

/*Proper parameters for template T
	Signature<float, float>		32bit tests
	Signature<float, deFloat16>	16bit tests*/
template<class T>
class TrigFunc : public CFloatFunc1<T>
{
public:
					TrigFunc		(const string&		name,
									 DoubleFunc1&		func,
									 const Interval&	loEx,
									 const Interval&	hiEx)
						: CFloatFunc1<T>	(name, func)
						, m_loExtremum		(loEx)
						, m_hiExtremum		(hiEx) {}

protected:
	Interval		innerExtrema	(const EvalContext&, const Interval& angle) const
	{
		const double		lo		= angle.lo();
		const double		hi		= angle.hi();
		const int			loSlope	= doGetSlope(lo);
		const int			hiSlope	= doGetSlope(hi);

		// Detect the high and low values the function can take between the
		// interval endpoints.
		if (angle.length() >= 2.0 * DE_PI_DOUBLE)
		{
			// The interval is longer than a full cycle, so it must get all possible values.
			return m_hiExtremum | m_loExtremum;
		}
		else if (loSlope == 1 && hiSlope == -1)
		{
			// The slope can change from positive to negative only at the maximum value.
			return m_hiExtremum;
		}
		else if (loSlope == -1 && hiSlope == 1)
		{
			// The slope can change from negative to positive only at the maximum value.
			return m_loExtremum;
		}
		else if (loSlope == hiSlope &&
				 deIntSign(CFloatFunc1<T>::applyExact(hi) - CFloatFunc1<T>::applyExact(lo)) * loSlope == -1)
		{
			// The slope has changed twice between the endpoints, so both extrema are included.
			return m_hiExtremum | m_loExtremum;
		}

		return Interval();
	}

	Interval	getCodomain				(const EvalContext&) const
	{
		// Ensure that result is always within [-1, 1], or NaN (for +-inf)
		return Interval(-1.0, 1.0) | TCU_NAN;
	}

	double		precision				(const EvalContext& ctx, double ret, double arg) const;

	Interval	getInputRange			(const bool is16bit) const;
	virtual int	doGetSlope				(double angle) const = 0;

	Interval		m_loExtremum;
	Interval		m_hiExtremum;
};

//Only -DE_PI_DOUBLE, DE_PI_DOUBLE input range
template<>
Interval TrigFunc<Signature<float, float> >::getInputRange(const bool is16bit) const
{
	DE_UNREF(is16bit);
	return Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE);
}

//Only -DE_PI_DOUBLE, DE_PI_DOUBLE input range
template<>
Interval TrigFunc<Signature<deFloat16, deFloat16> >::getInputRange(const bool is16bit) const
{
	DE_UNREF(is16bit);
	return Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE);
}

//Only -DE_PI_DOUBLE, DE_PI_DOUBLE input range
template<>
Interval TrigFunc<Signature<double, double> >::getInputRange(const bool is16bit) const
{
	DE_UNREF(is16bit);
	return Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE);
}

template<>
double TrigFunc<Signature<float, float> >::precision(const EvalContext& ctx, double ret, double arg) const
{
	DE_UNREF(ret);
	if (ctx.floatPrecision == glu::PRECISION_HIGHP)
	{
		if (-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE)
			return deLdExp(1.0, -11);
		else
		{
			// "larger otherwise", let's pick |x| * 2^-12 , which is slightly over
			// 2^-11 at x == pi.
			return deLdExp(deAbs(arg), -12);
		}
	}
	else
	{
		DE_ASSERT(ctx.floatPrecision == glu::PRECISION_MEDIUMP || ctx.floatPrecision == glu::PRECISION_LAST);

		if (-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE)
			return deLdExp(1.0, -7);
		else
		{
			// |x| * 2^-8, slightly larger than 2^-7 at x == pi
			return deLdExp(deAbs(arg), -8);
		}
	}
}
//
/*
 * Half tests
 * From Spec:
 * Absolute error 2^{-7} inside the range [-pi, pi].
*/
template<>
double TrigFunc<Signature<deFloat16, deFloat16> >::precision(const EvalContext& ctx, double ret, double arg) const
{
	DE_UNREF(ctx);
	DE_UNREF(ret);
	DE_UNREF(arg);
	DE_ASSERT(-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE && ctx.floatPrecision == glu::PRECISION_LAST);
	return deLdExp(1.0, -7);
}

// Spec: "The precision of double-precision instructions is at least that of single precision."
// Lets pick float high precision as a reference.
template<>
double TrigFunc<Signature<double, double> >::precision(const EvalContext& ctx, double ret, double arg) const
{
	DE_UNREF(ctx);
	DE_UNREF(ret);
	if (-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE)
		return deLdExp(1.0, -11);
	else
	{
		// "larger otherwise", let's pick |x| * 2^-12 , which is slightly over
		// 2^-11 at x == pi.
		return deLdExp(deAbs(arg), -12);
	}
}

/*Proper parameters for template T
	Signature<float, float>		32bit tests
	Signature<float, deFloat16>	16bit tests*/
template <class T>
class Sin : public TrigFunc<T>
{
public:
				Sin			(void) : TrigFunc<T>("sin", deSin, -1.0, 1.0) {}

protected:
	int			doGetSlope	(double angle) const { return deIntSign(deCos(angle)); }
};

ExprP<float> sin (const ExprP<float>& x) { return app<Sin<Signature<float, float> > >(x); }
ExprP<deFloat16> sin (const ExprP<deFloat16>& x) { return app<Sin<Signature<deFloat16, deFloat16> > >(x); }
ExprP<double> sin (const ExprP<double>& x) { return app<Sin<Signature<double, double> > >(x); }

template <class T>
class Cos : public TrigFunc<T>
{
public:
				Cos			(void) : TrigFunc<T> ("cos", deCos, -1.0, 1.0) {}

protected:
	int			doGetSlope	(double angle) const { return -deIntSign(deSin(angle)); }
};

ExprP<float> cos (const ExprP<float>& x) { return app<Cos<Signature<float, float> > >(x); }
ExprP<deFloat16> cos (const ExprP<deFloat16>& x) { return app<Cos<Signature<deFloat16, deFloat16> > >(x); }
ExprP<double> cos (const ExprP<double>& x) { return app<Cos<Signature<double, double> > >(x); }

DEFINE_DERIVED_FLOAT1_INPUTRANGE(Tan, tan, x, sin(x) * (constant(1.0f) / cos(x)), Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE));
DEFINE_DERIVED_FLOAT1_INPUTRANGE_16BIT(Tan16Bit, tan, x, sin(x) * (constant((deFloat16)FLOAT16_1_0) / cos(x)), Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE));
DEFINE_DERIVED_DOUBLE1_INPUTRANGE(Tan64Bit, tan, x, sin(x) * (constant(1.0) / cos(x)), Interval(false, -DE_PI_DOUBLE, DE_PI_DOUBLE));

template <class T>
class ATan : public CFloatFunc1<T>
{
public:
			ATan		(void) : CFloatFunc1<T>	("atan", deAtanOver) {}

protected:
	double	precision	(const EvalContext& ctx, double ret, double) const
	{
		if (ctx.floatPrecision == glu::PRECISION_HIGHP)
			return ctx.format.ulp(ret, 4096.0);
		else
			return ctx.format.ulp(ret, 5.0);
	}

	Interval getCodomain(const EvalContext& ctx) const
	{
		return ctx.format.roundOut(Interval(-0.5 * DE_PI_DOUBLE, 0.5 * DE_PI_DOUBLE), true);
	}
};

template <class T>
class ATan2 : public CFloatFunc2<T>
{
public:
				ATan2			(void) : CFloatFunc2<T> ("atan", deAtan2) {}

protected:
	Interval	innerExtrema	(const EvalContext&		ctx,
								 const Interval&		yi,
								 const Interval&		xi) const
	{
		Interval ret;

		if (yi.contains(0.0))
		{
			if (xi.contains(0.0))
				ret |= TCU_NAN;
			if (xi.intersects(Interval(-TCU_INFINITY, 0.0)))
				ret |= ctx.format.roundOut(Interval(-DE_PI_DOUBLE, DE_PI_DOUBLE), true);
		}

		if (!yi.isFinite() || !xi.isFinite())
		{
			// Infinities may not be supported, allow anything, including NaN
			ret |= TCU_NAN;
		}

		return ret;
	}

	double		precision		(const EvalContext& ctx, double ret, double, double) const
	{
		if (ctx.floatPrecision == glu::PRECISION_HIGHP)
			return ctx.format.ulp(ret, 4096.0);
		else
			return ctx.format.ulp(ret, 5.0);
	}

	Interval getCodomain(const EvalContext& ctx) const
	{
		return ctx.format.roundOut(Interval(-DE_PI_DOUBLE, DE_PI_DOUBLE), true);
	}
};

ExprP<float> atan2	(const ExprP<float>& x, const ExprP<float>& y)	{ return app<ATan2<Signature<float, float, float> > >(x, y); }

ExprP<deFloat16> atan2	(const ExprP<deFloat16>& x, const ExprP<deFloat16>& y)	{ return app<ATan2<Signature<deFloat16, deFloat16, deFloat16> > >(x, y); }

ExprP<double> atan2	(const ExprP<double>& x, const ExprP<double>& y)	{ return app<ATan2<Signature<double, double, double> > >(x, y); }


DEFINE_DERIVED_FLOAT1(Sinh, sinh, x, (exp<float>(x) - exp<float>(-x)) / constant(2.0f));
DEFINE_DERIVED_FLOAT1(Cosh, cosh, x, (exp<float>(x) + exp<float>(-x)) / constant(2.0f));
DEFINE_DERIVED_FLOAT1(Tanh, tanh, x, sinh(x) / cosh(x));

DEFINE_DERIVED_FLOAT1_16BIT(Sinh16Bit, sinh, x, (exp(x) - exp(-x)) / constant((deFloat16)FLOAT16_2_0));
DEFINE_DERIVED_FLOAT1_16BIT(Cosh16Bit, cosh, x, (exp(x) + exp(-x)) / constant((deFloat16)FLOAT16_2_0));
DEFINE_DERIVED_FLOAT1_16BIT(Tanh16Bit, tanh, x, sinh(x) / cosh(x));

DEFINE_DERIVED_DOUBLE1(Sinh64Bit, sinh, x, (exp<double>(x) - exp<double>(-x)) / constant(2.0));
DEFINE_DERIVED_DOUBLE1(Cosh64Bit, cosh, x, (exp<double>(x) + exp<double>(-x)) / constant(2.0));
DEFINE_DERIVED_DOUBLE1(Tanh64Bit, tanh, x, sinh(x) / cosh(x));

DEFINE_DERIVED_FLOAT1(ASin, asin, x, atan2(x, sqrt(constant(1.0f) - x * x)));
DEFINE_DERIVED_FLOAT1(ACos, acos, x, atan2(sqrt(constant(1.0f) - x * x), x));
DEFINE_DERIVED_FLOAT1(ASinh, asinh, x, log(x + sqrt(x * x + constant(1.0f))));
DEFINE_DERIVED_FLOAT1(ACosh, acosh, x, log(x + sqrt(alternatives((x + constant(1.0f)) * (x - constant(1.0f)),
																 (x * x - constant(1.0f))))));
DEFINE_DERIVED_FLOAT1(ATanh, atanh, x, constant(0.5f) * log((constant(1.0f) + x) /
															(constant(1.0f) - x)));

DEFINE_DERIVED_FLOAT1_16BIT(ASin16Bit, asin, x, atan2(x, sqrt(constant((deFloat16)FLOAT16_1_0) - x * x)));
DEFINE_DERIVED_FLOAT1_16BIT(ACos16Bit, acos, x, atan2(sqrt(constant((deFloat16)FLOAT16_1_0) - x * x), x));
DEFINE_DERIVED_FLOAT1_16BIT(ASinh16Bit, asinh, x, log(x + sqrt(x * x + constant((deFloat16)FLOAT16_1_0))));
DEFINE_DERIVED_FLOAT1_16BIT(ACosh16Bit, acosh, x, log(x + sqrt(alternatives((x + constant((deFloat16)FLOAT16_1_0)) * (x - constant((deFloat16)FLOAT16_1_0)),
																 (x * x - constant((deFloat16)FLOAT16_1_0))))));
DEFINE_DERIVED_FLOAT1_16BIT(ATanh16Bit, atanh, x, constant((deFloat16)FLOAT16_0_5) * log((constant((deFloat16)FLOAT16_1_0) + x) /
															(constant((deFloat16)FLOAT16_1_0) - x)));

DEFINE_DERIVED_DOUBLE1(ASin64Bit, asin, x, atan2(x, sqrt(constant(1.0) - pow(x, constant(2.0)))));
DEFINE_DERIVED_DOUBLE1(ACos64Bit, acos, x, atan2(sqrt(constant(1.0) - pow(x, constant(2.0))), x));
DEFINE_DERIVED_DOUBLE1(ASinh64Bit, asinh, x, log(x + sqrt(x * x + constant(1.0))));
DEFINE_DERIVED_DOUBLE1(ACosh64Bit, acosh, x, log(x + sqrt(alternatives((x + constant(1.0)) * (x - constant(1.0)),
																 (x * x - constant(1.0))))));
DEFINE_DERIVED_DOUBLE1(ATanh64Bit, atanh, x, constant(0.5) * log((constant(1.0) + x) /
															(constant(1.0) - x)));

template <typename T>
class GetComponent : public PrimitiveFunc<Signature<typename T::Element, T, int> >
{
public:
	typedef		typename GetComponent::IRet	IRet;

	string		getName		(void) const { return "_getComponent"; }

	void		print		(ostream&				os,
							 const BaseArgExprs&	args) const
	{
		os << *args[0] << "[" << *args[1] << "]";
	}

protected:
	IRet		doApply		(const EvalContext&,
							 const typename GetComponent::IArgs& iargs) const
	{
		IRet ret;

		for (int compNdx = 0; compNdx < T::SIZE; ++compNdx)
		{
			if (iargs.b.contains(compNdx))
				ret = unionIVal<typename T::Element>(ret, iargs.a[compNdx]);
		}

		return ret;
	}

};

template <typename T>
ExprP<typename T::Element> getComponent (const ExprP<T>& container, int ndx)
{
	DE_ASSERT(0 <= ndx && ndx < T::SIZE);
	return app<GetComponent<T> >(container, constant(ndx));
}

template <typename T>	string	vecNamePrefix			(void);
template <>				string	vecNamePrefix<float>	(void) { return ""; }
template <>				string	vecNamePrefix<deFloat16>(void) { return ""; }
template <>				string	vecNamePrefix<double>	(void) { return "d"; }
template <>				string	vecNamePrefix<int>		(void) { return "i"; }
template <>				string	vecNamePrefix<bool>		(void) { return "b"; }

template <typename T, int Size>
string vecName (void) { return vecNamePrefix<T>() + "vec" + de::toString(Size); }

template <typename T, int Size> class GenVec;

template <typename T>
class GenVec<T, 1> : public DerivedFunc<Signature<T, T> >
{
public:
	typedef typename GenVec<T, 1>::ArgExprs ArgExprs;

	string		getName		(void) const
	{
		return "_" + vecName<T, 1>();
	}

protected:

	ExprP<T>	doExpand	(ExpandContext&, const ArgExprs& args) const { return args.a; }
};

template <typename T>
class GenVec<T, 2> : public PrimitiveFunc<Signature<Vector<T, 2>, T, T> >
{
public:
	typedef typename GenVec::IRet	IRet;
	typedef typename GenVec::IArgs	IArgs;

	string		getName		(void) const
	{
		return vecName<T, 2>();
	}

protected:
	IRet		doApply		(const EvalContext&, const IArgs& iargs) const
	{
		return IRet(iargs.a, iargs.b);
	}
};

template <typename T>
class GenVec<T, 3> : public PrimitiveFunc<Signature<Vector<T, 3>, T, T, T> >
{
public:
	typedef typename GenVec::IRet	IRet;
	typedef typename GenVec::IArgs	IArgs;

	string	getName		(void) const
	{
		return vecName<T, 3>();
	}

protected:
	IRet	doApply		(const EvalContext&, const IArgs& iargs) const
	{
		return IRet(iargs.a, iargs.b, iargs.c);
	}
};

template <typename T>
class GenVec<T, 4> : public PrimitiveFunc<Signature<Vector<T, 4>, T, T, T, T> >
{
public:
	typedef typename GenVec::IRet	IRet;
	typedef typename GenVec::IArgs	IArgs;

	string		getName		(void) const { return vecName<T, 4>(); }

protected:
	IRet		doApply		(const EvalContext&, const IArgs& iargs) const
	{
		return IRet(iargs.a, iargs.b, iargs.c, iargs.d);
	}
};

template <typename T, int Rows, int Columns>
class GenMat;

template <typename T, int Rows>
class GenMat<T, Rows, 2> : public PrimitiveFunc<
	Signature<Matrix<T, Rows, 2>, Vector<T, Rows>, Vector<T, Rows> > >
{
public:
	typedef typename GenMat::Ret	Ret;
	typedef typename GenMat::IRet	IRet;
	typedef typename GenMat::IArgs	IArgs;

	string		getName		(void) const
	{
		return dataTypeNameOf<Ret>();
	}

protected:

	IRet		doApply		(const EvalContext&, const IArgs& iargs) const
	{
		IRet	ret;
		ret[0] = iargs.a;
		ret[1] = iargs.b;
		return ret;
	}
};

template <typename T, int Rows>
class GenMat<T, Rows, 3> : public PrimitiveFunc<
	Signature<Matrix<T, Rows, 3>, Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows> > >
{
public:
	typedef typename GenMat::Ret	Ret;
	typedef typename GenMat::IRet	IRet;
	typedef typename GenMat::IArgs	IArgs;

	string	getName	(void) const
	{
		return dataTypeNameOf<Ret>();
	}

protected:

	IRet	doApply	(const EvalContext&, const IArgs& iargs) const
	{
		IRet	ret;
		ret[0] = iargs.a;
		ret[1] = iargs.b;
		ret[2] = iargs.c;
		return ret;
	}
};

template <typename T, int Rows>
class GenMat<T, Rows, 4> : public PrimitiveFunc<
	Signature<Matrix<T, Rows, 4>,
			  Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows> > >
{
public:
	typedef typename GenMat::Ret	Ret;
	typedef typename GenMat::IRet	IRet;
	typedef typename GenMat::IArgs	IArgs;

	string	getName	(void) const
	{
		return dataTypeNameOf<Ret>();
	}

protected:
	IRet	doApply	(const EvalContext&, const IArgs& iargs) const
	{
		IRet	ret;
		ret[0] = iargs.a;
		ret[1] = iargs.b;
		ret[2] = iargs.c;
		ret[3] = iargs.d;
		return ret;
	}
};

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 2> > mat2 (const ExprP<Vector<T, Rows> >& arg0,
								 const ExprP<Vector<T, Rows> >& arg1)
{
	return app<GenMat<T, Rows, 2> >(arg0, arg1);
}

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 3> > mat3 (const ExprP<Vector<T, Rows> >& arg0,
								 const ExprP<Vector<T, Rows> >& arg1,
								 const ExprP<Vector<T, Rows> >& arg2)
{
	return app<GenMat<T, Rows, 3> >(arg0, arg1, arg2);
}

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 4> > mat4 (const ExprP<Vector<T, Rows> >& arg0,
								 const ExprP<Vector<T, Rows> >& arg1,
								 const ExprP<Vector<T, Rows> >& arg2,
								 const ExprP<Vector<T, Rows> >& arg3)
{
	return app<GenMat<T, Rows, 4> >(arg0, arg1, arg2, arg3);
}

template <typename T, int Rows, int Cols>
class MatNeg : public PrimitiveFunc<Signature<Matrix<T, Rows, Cols>,
											  Matrix<T, Rows, Cols> > >
{
public:
	typedef typename MatNeg::IRet		IRet;
	typedef typename MatNeg::IArgs		IArgs;

	string	getName	(void) const
	{
		return "_matNeg";
	}

protected:
	void	doPrint	(ostream& os, const BaseArgExprs& args) const
	{
		os << "-(" << *args[0] << ")";
	}

	IRet	doApply	(const EvalContext&, const IArgs& iargs)			const
	{
		IRet	ret;

		for (int col = 0; col < Cols; ++col)
		{
			for (int row = 0; row < Rows; ++row)
				ret[col][row] = -iargs.a[col][row];
		}

		return ret;
	}
};

template <typename T, typename Sig>
class CompWiseFunc : public PrimitiveFunc<Sig>
{
public:
	typedef Func<Signature<T, T, T> >	ScalarFunc;

	string				getName			(void)									const
	{
		return doGetScalarFunc().getName();
	}
protected:
	void				doPrint			(ostream&				os,
										 const BaseArgExprs&	args)			const
	{
		doGetScalarFunc().print(os, args);
	}

	virtual
	const ScalarFunc&	doGetScalarFunc	(void)									const = 0;
};

template <typename T, int Rows, int Cols>
class CompMatFuncBase : public CompWiseFunc<T, Signature<Matrix<T, Rows, Cols>,
														 Matrix<T, Rows, Cols>,
														 Matrix<T, Rows, Cols> > >
{
public:
	typedef typename CompMatFuncBase::IRet		IRet;
	typedef typename CompMatFuncBase::IArgs		IArgs;

protected:

	IRet	doApply	(const EvalContext& ctx, const IArgs& iargs) const
	{
		IRet			ret;

		for (int col = 0; col < Cols; ++col)
		{
			for (int row = 0; row < Rows; ++row)
				ret[col][row] = this->doGetScalarFunc().apply(ctx,
															  iargs.a[col][row],
															  iargs.b[col][row]);
		}

		return ret;
	}
};

template <typename F, typename T, int Rows, int Cols>
class CompMatFunc : public CompMatFuncBase<T, Rows, Cols>
{
protected:
	const typename CompMatFunc::ScalarFunc&	doGetScalarFunc	(void) const
	{
		return instance<F>();
	}
};

template <class T>
class ScalarMatrixCompMult : public Mul< Signature<T, T, T> >
{
public:

	string	getName	(void) const
	{
		return "matrixCompMult";
	}

	void	doPrint	(ostream& os, const BaseArgExprs& args) const
	{
		Func<Signature<T, T, T> >::doPrint(os, args);
	}
};

template <int Rows, int Cols, class T>
class MatrixCompMult : public CompMatFunc<ScalarMatrixCompMult<T>, T, Rows, Cols>
{
};

template <int Rows, int Cols>
class ScalarMatFuncBase : public CompWiseFunc<float, Signature<Matrix<float, Rows, Cols>,
															   Matrix<float, Rows, Cols>,
															   float> >
{
public:
	typedef typename ScalarMatFuncBase::IRet	IRet;
	typedef typename ScalarMatFuncBase::IArgs	IArgs;

protected:

	IRet	doApply	(const EvalContext& ctx, const IArgs& iargs) const
	{
		IRet	ret;

		for (int col = 0; col < Cols; ++col)
		{
			for (int row = 0; row < Rows; ++row)
				ret[col][row] = this->doGetScalarFunc().apply(ctx, iargs.a[col][row], iargs.b);
		}

		return ret;
	}
};

template <typename F, int Rows, int Cols>
class ScalarMatFunc : public ScalarMatFuncBase<Rows, Cols>
{
protected:
	const typename ScalarMatFunc::ScalarFunc&	doGetScalarFunc	(void)	const
	{
		return instance<F>();
	}
};

template<typename T, int Size> struct GenXType;

template<typename T>
struct GenXType<T, 1>
{
	static ExprP<T>	genXType	(const ExprP<T>& x) { return x; }
};

template<typename T>
struct GenXType<T, 2>
{
	static ExprP<Vector<T, 2> >	genXType	(const ExprP<T>& x)
	{
		return app<GenVec<T, 2> >(x, x);
	}
};

template<typename T>
struct GenXType<T, 3>
{
	static ExprP<Vector<T, 3> >	genXType	(const ExprP<T>& x)
	{
		return app<GenVec<T, 3> >(x, x, x);
	}
};

template<typename T>
struct GenXType<T, 4>
{
	static ExprP<Vector<T, 4> >	genXType	(const ExprP<T>& x)
	{
		return app<GenVec<T, 4> >(x, x, x, x);
	}
};

//! Returns an expression of vector of size `Size` (or scalar if Size == 1),
//! with each element initialized with the expression `x`.
template<typename T, int Size>
ExprP<typename ContainerOf<T, Size>::Container> genXType (const ExprP<T>& x)
{
	return GenXType<T, Size>::genXType(x);
}

typedef GenVec<float, 2> FloatVec2;
DEFINE_CONSTRUCTOR2(FloatVec2, Vec2, vec2, float, float)

typedef GenVec<deFloat16, 2> FloatVec2_16bit;
DEFINE_CONSTRUCTOR2(FloatVec2_16bit, Vec2_16Bit, vec2, deFloat16, deFloat16)

typedef GenVec<double, 2> DoubleVec2;
DEFINE_CONSTRUCTOR2(DoubleVec2, Vec2_64Bit, vec2, double, double)

typedef GenVec<float, 3> FloatVec3;
DEFINE_CONSTRUCTOR3(FloatVec3, Vec3, vec3, float, float, float)

typedef GenVec<deFloat16, 3> FloatVec3_16bit;
DEFINE_CONSTRUCTOR3(FloatVec3_16bit, Vec3_16Bit, vec3, deFloat16, deFloat16, deFloat16)

typedef GenVec<double, 3> DoubleVec3;
DEFINE_CONSTRUCTOR3(DoubleVec3, Vec3_64Bit, vec3, double, double, double)

typedef GenVec<float, 4> FloatVec4;
DEFINE_CONSTRUCTOR4(FloatVec4, Vec4, vec4, float, float, float, float)

typedef GenVec<deFloat16, 4> FloatVec4_16bit;
DEFINE_CONSTRUCTOR4(FloatVec4_16bit, Vec4_16Bit, vec4, deFloat16, deFloat16, deFloat16, deFloat16)

typedef GenVec<double, 4> DoubleVec4;
DEFINE_CONSTRUCTOR4(DoubleVec4, Vec4_64Bit, vec4, double, double, double, double)

template <class T>
const ExprP<T> getConstZero(void);
template <class T>
const ExprP<T> getConstOne(void);
template <class T>
const ExprP<T> getConstTwo(void);

template <>
const ExprP<float> getConstZero<float>(void)
{
	return constant(0.0f);
}

template <>
const ExprP<deFloat16> getConstZero<deFloat16>(void)
{
	return constant((deFloat16)FLOAT16_0_0);
}

template <>
const ExprP<double> getConstZero<double>(void)
{
	return constant(0.0);
}

template <>
const ExprP<float> getConstOne<float>(void)
{
	return constant(1.0f);
}

template <>
const ExprP<deFloat16> getConstOne<deFloat16>(void)
{
	return constant((deFloat16)FLOAT16_1_0);
}

template <>
const ExprP<double> getConstOne<double>(void)
{
	return constant(1.0);
}

template <>
const ExprP<float> getConstTwo<float>(void)
{
	return constant(2.0f);
}

template <>
const ExprP<deFloat16> getConstTwo<deFloat16>(void)
{
	return constant((deFloat16)FLOAT16_2_0);
}

template <>
const ExprP<double> getConstTwo<double>(void)
{
	return constant(2.0);
}

template <int Size, class T>
class Dot : public DerivedFunc<Signature<T, Vector<T, Size>, Vector<T, Size> > >
{
public:
	typedef typename Dot::ArgExprs ArgExprs;

	string			getName		(void) const
	{
		return "dot";
	}

protected:
	ExprP<T>	doExpand	(ExpandContext&, const ArgExprs& args) const
	{
		ExprP<T> op[Size];
		// Precompute all products.
		for (int ndx = 0; ndx < Size; ++ndx)
			op[ndx] = args.a[ndx] * args.b[ndx];

		int idx[Size];
		//Prepare an array of indices.
		for (int ndx = 0; ndx < Size; ++ndx)
			idx[ndx] = ndx;

		ExprP<T> res = op[0];
		// Compute the first dot alternative: SUM(a[i]*b[i]), i = 0 .. Size-1
		for (int ndx = 1; ndx < Size; ++ndx)
			res = res + op[ndx];

		// Generate all permutations of indices and
		// using a permutation compute a dot alternative.
		// Generates all possible variants fo summation of products in the dot product expansion expression.
		do {
			ExprP<T> alt = getConstZero<T>();
			for (int ndx = 0; ndx < Size; ++ndx)
				alt = alt + op[idx[ndx]];
			res = alternatives(res, alt);
		} while (std::next_permutation(idx, idx + Size));

		return res;
	}
};

template <class T>
class Dot<1, T> : public DerivedFunc<Signature<T, T, T> >
{
public:
	typedef typename DerivedFunc<Signature<T, T, T> >::ArgExprs	TArgExprs;

	string			getName		(void) const
	{
		return "dot";
	}

	ExprP<T>	doExpand	(ExpandContext&, const TArgExprs& args) const
	{
		return args.a * args.b;
	}
};

template <int Size>
ExprP<deFloat16> dot (const ExprP<Vector<deFloat16, Size> >& x, const ExprP<Vector<deFloat16, Size> >& y)
{
	return app<Dot<Size, deFloat16> >(x, y);
}

ExprP<deFloat16> dot (const ExprP<deFloat16>& x, const ExprP<deFloat16>& y)
{
	return app<Dot<1, deFloat16> >(x, y);
}

template <int Size>
ExprP<float> dot (const ExprP<Vector<float, Size> >& x, const ExprP<Vector<float, Size> >& y)
{
	return app<Dot<Size, float> >(x, y);
}

ExprP<float> dot (const ExprP<float>& x, const ExprP<float>& y)
{
	return app<Dot<1, float> >(x, y);
}

template <int Size>
ExprP<double> dot (const ExprP<Vector<double, Size> >& x, const ExprP<Vector<double, Size> >& y)
{
	return app<Dot<Size, double> >(x, y);
}

ExprP<double> dot (const ExprP<double>& x, const ExprP<double>& y)
{
	return app<Dot<1, double> >(x, y);
}

template <int Size, class T>
class Length : public DerivedFunc<
	Signature<T, typename ContainerOf<T, Size>::Container> >
{
public:
	typedef typename Length::ArgExprs ArgExprs;

	string			getName		(void) const
	{
		return "length";
	}

protected:
	ExprP<T>		doExpand	(ExpandContext&, const ArgExprs& args) const
	{
		return sqrt(dot(args.a, args.a));
	}
};


template <class T, class TRet>
ExprP<TRet> length (const ExprP<T>& x)
{
	return app<Length<1, T> >(x);
}

template <int Size, class T, class TRet>
ExprP<TRet> length (const ExprP<typename ContainerOf<T, Size>::Container>& x)
{
	return app<Length<Size, T> >(x);
}

template <int Size, class T>
class Distance : public DerivedFunc<
	Signature<T,
			  typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container> >
{
public:
	typedef typename	Distance::Ret		Ret;
	typedef typename	Distance::ArgExprs	ArgExprs;

	string		getName		(void) const
	{
		return "distance";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext&, const ArgExprs& args) const
	{
		return length<Size, T, Ret>(args.a - args.b);
	}
};

// cross

class Cross : public DerivedFunc<Signature<Vec3, Vec3, Vec3> >
{
public:
	string			getName		(void) const
	{
		return "cross";
	}

protected:
	ExprP<Vec3>		doExpand	(ExpandContext&, const ArgExprs& x) const
	{
		return vec3(x.a[1] * x.b[2] - x.b[1] * x.a[2],
					x.a[2] * x.b[0] - x.b[2] * x.a[0],
					x.a[0] * x.b[1] - x.b[0] * x.a[1]);
	}
};

class Cross16Bit : public DerivedFunc<Signature<Vec3_16Bit, Vec3_16Bit, Vec3_16Bit> >
{
public:
	string			getName		(void) const
	{
		return "cross";
	}

protected:
	ExprP<Vec3_16Bit>		doExpand	(ExpandContext&, const ArgExprs& x) const
	{
		return vec3(x.a[1] * x.b[2] - x.b[1] * x.a[2],
					x.a[2] * x.b[0] - x.b[2] * x.a[0],
					x.a[0] * x.b[1] - x.b[0] * x.a[1]);
	}
};

class Cross64Bit : public DerivedFunc<Signature<Vec3_64Bit, Vec3_64Bit, Vec3_64Bit> >
{
public:
	string			getName		(void) const
	{
		return "cross";
	}

protected:
	ExprP<Vec3_64Bit>		doExpand	(ExpandContext&, const ArgExprs& x) const
	{
		return vec3(x.a[1] * x.b[2] - x.b[1] * x.a[2],
					x.a[2] * x.b[0] - x.b[2] * x.a[0],
					x.a[0] * x.b[1] - x.b[0] * x.a[1]);
	}
};

DEFINE_CONSTRUCTOR2(Cross, Vec3, cross, Vec3, Vec3)
DEFINE_CONSTRUCTOR2(Cross16Bit, Vec3_16Bit, cross, Vec3_16Bit, Vec3_16Bit)
DEFINE_CONSTRUCTOR2(Cross64Bit, Vec3_64Bit, cross, Vec3_64Bit, Vec3_64Bit)

template<int Size, class T>
class Normalize : public DerivedFunc<
	Signature<typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container> >
{
public:
	typedef typename	Normalize::Ret		Ret;
	typedef typename	Normalize::ArgExprs	ArgExprs;

	string		getName		(void) const
	{
		return "normalize";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext&, const ArgExprs& args) const
	{
		return args.a / length<Size, T, T>(args.a);
	}
};

template <int Size, class T>
class FaceForward : public DerivedFunc<
	Signature<typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container> >
{
public:
	typedef typename	FaceForward::Ret		Ret;
	typedef typename	FaceForward::ArgExprs	ArgExprs;

	string		getName		(void) const
	{
		return "faceforward";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext&, const ArgExprs& args) const
	{
		return cond(dot(args.c, args.b) < getConstZero<T>(), args.a, -args.a);
	}
};

template <int Size, class T>
class Reflect : public DerivedFunc<
	Signature<typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container> >
{
public:
	typedef typename	Reflect::Ret		Ret;
	typedef typename	Reflect::Arg0		Arg0;
	typedef typename	Reflect::Arg1		Arg1;
	typedef typename	Reflect::ArgExprs	ArgExprs;

	string		getName		(void) const
	{
		return "reflect";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext& ctx, const ArgExprs& args) const
	{
		const ExprP<Arg0>&	i		= args.a;
		const ExprP<Arg1>&	n		= args.b;
		const ExprP<T>	dotNI	= bindExpression("dotNI", ctx, dot(n, i));

		return i - alternatives((n * dotNI) * getConstTwo<T>(),
								   alternatives( n * (dotNI * getConstTwo<T>()),
												alternatives(n * dot(i * getConstTwo<T>(), n),
															 n * dot(i, n * getConstTwo<T>())
												)
									)
								);
	}
};

template <int Size, class T>
class Refract : public DerivedFunc<
	Signature<typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container,
			  typename ContainerOf<T, Size>::Container,
			  T> >
{
public:
	typedef typename	Refract::Ret		Ret;
	typedef typename	Refract::Arg0		Arg0;
	typedef typename	Refract::Arg1		Arg1;
	typedef typename	Refract::Arg2		Arg2;
	typedef typename	Refract::ArgExprs	ArgExprs;

	string		getName		(void) const
	{
		return "refract";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext&	ctx, const ArgExprs& args) const
	{
		const ExprP<Arg0>&	i		= args.a;
		const ExprP<Arg1>&	n		= args.b;
		const ExprP<Arg2>&	eta		= args.c;
		const ExprP<T>	dotNI	= bindExpression("dotNI", ctx, dot(n, i));
		const ExprP<T>	k		= bindExpression("k", ctx, getConstOne<T>() - eta * eta *
												 (getConstOne<T>() - dotNI * dotNI));
		return cond(k < getConstZero<T>(),
					genXType<T, Size>(getConstZero<T>()),
					i * eta - n * (eta * dotNI + sqrt(k)));
	}
};

template <class T>
class PreciseFunc1 : public CFloatFunc1<T>
{
public:
			PreciseFunc1	(const string& name, DoubleFunc1& func) : CFloatFunc1<T> (name, func) {}
protected:
	double	precision		(const EvalContext&, double, double) const	{ return 0.0; }
};

template <class T>
class Abs : public PreciseFunc1<T>
{
public:
	Abs (void) : PreciseFunc1<T> ("abs", deAbs) {}
};

template <class T>
class Sign : public PreciseFunc1<T>
{
public:
	Sign (void) : PreciseFunc1<T> ("sign", deSign) {}
};

template <class T>
class Floor : public PreciseFunc1<T>
{
public:
	Floor (void) : PreciseFunc1<T> ("floor", deFloor) {}
};

template <class T>
class Trunc : public PreciseFunc1<T>
{
public:
	Trunc (void) : PreciseFunc1<T> ("trunc", deTrunc) {}
};

template <class T>
class Round : public FloatFunc1<T>
{
public:
	string		getName		(void) const								{ return "round"; }

protected:
	Interval	applyPoint	(const EvalContext&, double x) const
	{
		double			truncated	= 0.0;
		const double	fract		= deModf(x, &truncated);
		Interval		ret;

		if (fabs(fract) <= 0.5)
			ret |= truncated;
		if (fabs(fract) >= 0.5)
			ret |= truncated + deSign(fract);

		return ret;
	}

	double		precision	(const EvalContext&, double, double) const	{ return 0.0; }
};

template <class T>
class RoundEven : public PreciseFunc1<T>
{
public:
	RoundEven (void) : PreciseFunc1<T> ("roundEven", deRoundEven) {}
};

template <class T>
class Ceil : public PreciseFunc1<T>
{
public:
	Ceil (void) : PreciseFunc1<T> ("ceil", deCeil) {}
};

typedef Floor< Signature<float, float> > Floor32Bit;
typedef Floor< Signature<deFloat16, deFloat16> > Floor16Bit;
typedef Floor< Signature<double, double> > Floor64Bit;

typedef Trunc< Signature<float, float> > Trunc32Bit;
typedef Trunc< Signature<deFloat16, deFloat16> > Trunc16Bit;
typedef Trunc< Signature<double, double> > Trunc64Bit;

typedef Trunc< Signature<float, float> > Trunc32Bit;
typedef Trunc< Signature<deFloat16, deFloat16> > Trunc16Bit;

DEFINE_DERIVED_FLOAT1(Fract, fract, x, x - app<Floor32Bit>(x));
DEFINE_DERIVED_FLOAT1_16BIT(Fract16Bit, fract, x, x - app<Floor16Bit>(x));
DEFINE_DERIVED_DOUBLE1(Fract64Bit, fract, x, x - app<Floor64Bit>(x));

template <class T>
class PreciseFunc2 : public CFloatFunc2<T>
{
public:
			PreciseFunc2	(const string& name, DoubleFunc2& func) : CFloatFunc2<T> (name, func) {}
protected:
	double	precision		(const EvalContext&, double, double, double) const { return 0.0; }
};

DEFINE_DERIVED_FLOAT2(Mod32Bit, mod, x, y, x - y * app<Floor32Bit>(x / y));
DEFINE_DERIVED_FLOAT2_16BIT(Mod16Bit, mod, x, y, x - y * app<Floor16Bit>(x / y));
DEFINE_DERIVED_DOUBLE2(Mod64Bit, mod, x, y, x - y * app<Floor64Bit>(x / y));

DEFINE_CASED_DERIVED_FLOAT2(FRem32Bit, frem, x, y, x - y * app<Trunc32Bit>(x / y), SPIRV_CASETYPE_FREM);
DEFINE_CASED_DERIVED_FLOAT2_16BIT(FRem16Bit, frem, x, y, x - y * app<Trunc16Bit>(x / y), SPIRV_CASETYPE_FREM);
DEFINE_CASED_DERIVED_DOUBLE2(FRem64Bit, frem, x, y, x - y * app<Trunc64Bit>(x / y), SPIRV_CASETYPE_FREM);

template <class T>
class Modf : public PrimitiveFunc<T>
{
public:
	typedef typename Modf<T>::IArgs	TIArgs;
	typedef typename Modf<T>::IRet	TIRet;
	string	getName				(void) const
	{
		return "modf";
	}

protected:
	TIRet	doApply				(const EvalContext&, const TIArgs& iargs) const
	{
		Interval	fracIV;
		Interval&	wholeIV		= const_cast<Interval&>(iargs.b);
		double		intPart		= 0;

		TCU_INTERVAL_APPLY_MONOTONE1(fracIV, x, iargs.a, frac, frac = deModf(x, &intPart));
		TCU_INTERVAL_APPLY_MONOTONE1(wholeIV, x, iargs.a, whole,
									 deModf(x, &intPart); whole = intPart);

		if (!iargs.a.isFinite())
		{
			// Behavior on modf(Inf) not well-defined, allow anything as a fractional part
			// See Khronos bug 13907
			fracIV |= TCU_NAN;
		}

		return fracIV;
	}

	int		getOutParamIndex	(void) const
	{
		return 1;
	}
};
typedef Modf< Signature<float, float, float> >				Modf32Bit;
typedef Modf< Signature<deFloat16, deFloat16, deFloat16> >	Modf16Bit;
typedef Modf< Signature<double, double, double> >			Modf64Bit;

template <class T>
class ModfStruct : public Modf<T>
{
public:
	virtual string		getName			(void) const	{ return "modfstruct"; }
	virtual SpirVCaseT	getSpirvCase	(void) const	{ return SPIRV_CASETYPE_MODFSTRUCT; }
};
typedef ModfStruct< Signature<float, float, float> >				ModfStruct32Bit;
typedef ModfStruct< Signature<deFloat16, deFloat16, deFloat16> >	ModfStruct16Bit;
typedef ModfStruct< Signature<double, double, double> >				ModfStruct64Bit;

template <class T>
class Min : public PreciseFunc2<T> { public: Min (void) : PreciseFunc2<T> ("min", deMin) {} };
template <class T>
class Max : public PreciseFunc2<T> { public: Max (void) : PreciseFunc2<T> ("max", deMax) {} };

template <class T>
class Clamp : public FloatFunc3<T>
{
public:
	string	getName		(void) const { return "clamp"; }

	double	applyExact	(double x, double minVal, double maxVal) const
	{
		return de::min(de::max(x, minVal), maxVal);
	}

	double	precision	(const EvalContext&, double, double, double minVal, double maxVal) const
	{
		return minVal > maxVal ? TCU_NAN : 0.0;
	}
};

ExprP<deFloat16> clamp(const ExprP<deFloat16>& x, const ExprP<deFloat16>& minVal, const ExprP<deFloat16>& maxVal)
{
	return app<Clamp< Signature<deFloat16, deFloat16, deFloat16, deFloat16> > >(x, minVal, maxVal);
}

ExprP<float> clamp(const ExprP<float>& x, const ExprP<float>& minVal, const ExprP<float>& maxVal)
{
	return app<Clamp< Signature<float, float, float, float> > >(x, minVal, maxVal);
}

ExprP<double> clamp(const ExprP<double>& x, const ExprP<double>& minVal, const ExprP<double>& maxVal)
{
	return app<Clamp< Signature<double, double, double, double> > >(x, minVal, maxVal);
}

template <class T>
class NanIfGreaterOrEqual : public FloatFunc2<T>
{
public:
	string	getName		(void) const { return "nanIfGreaterOrEqual"; }

	double	applyExact	(double edge0, double edge1) const
	{
		return (edge0 >= edge1) ? TCU_NAN : 0.0;
	}

	double	precision	(const EvalContext&, double, double edge0, double edge1) const
	{
		return (edge0 >= edge1) ? TCU_NAN : 0.0;
	}
};

ExprP<deFloat16> nanIfGreaterOrEqual(const ExprP<deFloat16>& edge0, const ExprP<deFloat16>& edge1)
{
	return app<NanIfGreaterOrEqual< Signature<deFloat16, deFloat16, deFloat16> > >(edge0, edge1);
}

ExprP<float> nanIfGreaterOrEqual(const ExprP<float>& edge0, const ExprP<float>& edge1)
{
	return app<NanIfGreaterOrEqual< Signature<float, float, float> > >(edge0, edge1);
}

ExprP<double> nanIfGreaterOrEqual(const ExprP<double>& edge0, const ExprP<double>& edge1)
{
	return app<NanIfGreaterOrEqual< Signature<double, double, double> > >(edge0, edge1);
}

DEFINE_DERIVED_FLOAT3(Mix, mix, x, y, a, alternatives((x * (constant(1.0f) - a)) + y * a,
													  x + (y - x) * a));

DEFINE_DERIVED_FLOAT3_16BIT(Mix16Bit, mix, x, y, a, alternatives((x * (constant((deFloat16)FLOAT16_1_0) - a)) + y * a,
													  x + (y - x) * a));

DEFINE_DERIVED_DOUBLE3(Mix64Bit, mix, x, y, a, alternatives((x * (constant(1.0) - a)) + y * a,
													  x + (y - x) * a));

static double step (double edge, double x)
{
	return x < edge ? 0.0 : 1.0;
}

template <class T>
class Step : public PreciseFunc2<T> { public: Step (void) : PreciseFunc2<T> ("step", step) {} };

template <class T>
class SmoothStep : public DerivedFunc<T>
{
public:
	typedef typename SmoothStep<T>::ArgExprs	TArgExprs;
	typedef typename SmoothStep<T>::Ret			TRet;
	string		getName		(void) const
	{
		return "smoothstep";
	}

protected:

	ExprP<TRet>	doExpand	(ExpandContext& ctx, const TArgExprs& args) const;
};

template<>
ExprP<SmoothStep< Signature<float, float, float, float> >::Ret>	SmoothStep< Signature<float, float, float, float> >::doExpand (ExpandContext& ctx, const SmoothStep< Signature<float, float, float, float> >::ArgExprs& args) const
{
	const ExprP<float>&		edge0	= args.a;
	const ExprP<float>&		edge1	= args.b;
	const ExprP<float>&		x		= args.c;
	const ExprP<float>		tExpr	= clamp((x - edge0) / (edge1 - edge0), constant(0.0f), constant(1.0f))
									+ nanIfGreaterOrEqual(edge0, edge1); // force NaN (and non-analyzable result) for cases edge0 >= edge1
	const ExprP<float>		t		= bindExpression("t", ctx, tExpr);

	return (t * t * (constant(3.0f) - constant(2.0f) * t));
}

template<>
ExprP<SmoothStep< Signature<deFloat16, deFloat16, deFloat16, deFloat16> >::TRet>	SmoothStep< Signature<deFloat16, deFloat16, deFloat16, deFloat16> >::doExpand (ExpandContext& ctx, const TArgExprs& args) const
{
	const ExprP<deFloat16>&		edge0	= args.a;
	const ExprP<deFloat16>&		edge1	= args.b;
	const ExprP<deFloat16>&		x		= args.c;
	const ExprP<deFloat16>		tExpr	= clamp(( x - edge0 ) / ( edge1 - edge0 ),
											constant((deFloat16)FLOAT16_0_0), constant((deFloat16)FLOAT16_1_0))
										+ nanIfGreaterOrEqual(edge0, edge1); // force NaN (and non-analyzable result) for cases edge0 >= edge1
	const ExprP<deFloat16>		t		= bindExpression("t", ctx, tExpr);

	return (t * t * (constant((deFloat16)FLOAT16_3_0) - constant((deFloat16)FLOAT16_2_0) * t));
}

template<>
ExprP<SmoothStep< Signature<double, double, double, double> >::Ret>	SmoothStep< Signature<double, double, double, double> >::doExpand (ExpandContext& ctx, const SmoothStep< Signature<double, double, double, double> >::ArgExprs& args) const
{
	const ExprP<double>&	edge0	= args.a;
	const ExprP<double>&	edge1	= args.b;
	const ExprP<double>&	x		= args.c;
	const ExprP<double>		tExpr	= clamp((x - edge0) / (edge1 - edge0), constant(0.0), constant(1.0))
									+ nanIfGreaterOrEqual(edge0, edge1); // force NaN (and non-analyzable result) for cases edge0 >= edge1
	const ExprP<double>		t		= bindExpression("t", ctx, tExpr);

	return (t * t * (constant(3.0) - constant(2.0) * t));
}

//Signature<float, float, int>
//Signature<float, deFloat16, int>
//Signature<double, double, int>
template <class T>
class FrExp : public PrimitiveFunc<T>
{
public:
	string	getName			(void) const
	{
		return "frexp";
	}

	typedef typename	FrExp::IRet		IRet;
	typedef typename	FrExp::IArgs	IArgs;
	typedef typename	FrExp::IArg0	IArg0;
	typedef typename	FrExp::IArg1	IArg1;

protected:
	IRet	doApply			(const EvalContext&, const IArgs& iargs) const
	{
		IRet			ret;
		const IArg0&	x			= iargs.a;
		IArg1&			exponent	= const_cast<IArg1&>(iargs.b);

		if (x.hasNaN() || x.contains(TCU_INFINITY) || x.contains(-TCU_INFINITY))
		{
			// GLSL (in contrast to IEEE) says that result of applying frexp
			// to infinity is undefined
			ret = Interval::unbounded() | TCU_NAN;
			exponent = Interval(-deLdExp(1.0, 31), deLdExp(1.0, 31)-1);
		}
		else if (!x.empty())
		{
			int				loExp	= 0;
			const double	loFrac	= deFrExp(x.lo(), &loExp);
			int				hiExp	= 0;
			const double	hiFrac	= deFrExp(x.hi(), &hiExp);

			if (deSign(loFrac) != deSign(hiFrac))
			{
				exponent = Interval(-TCU_INFINITY, de::max(loExp, hiExp));
				ret = Interval();
				if (deSign(loFrac) < 0)
					ret |= Interval(-1.0 + DBL_EPSILON*0.5, 0.0);
				if (deSign(hiFrac) > 0)
					ret |= Interval(0.0, 1.0 - DBL_EPSILON*0.5);
			}
			else
			{
				exponent = Interval(loExp, hiExp);
				if (loExp == hiExp)
					ret = Interval(loFrac, hiFrac);
				else
					ret = deSign(loFrac) * Interval(0.5, 1.0 - DBL_EPSILON*0.5);
			}
		}

		return ret;
	}

	int	getOutParamIndex	(void) const
	{
		return 1;
	}
};
typedef FrExp< Signature<float, float, int> >			Frexp32Bit;
typedef FrExp< Signature<deFloat16, deFloat16, int> >	Frexp16Bit;
typedef FrExp< Signature<double, double, int> >			Frexp64Bit;

template <class T>
class FrexpStruct : public FrExp<T>
{
public:
	virtual string		getName			(void) const	{ return "frexpstruct"; }
	virtual SpirVCaseT	getSpirvCase	(void) const	{ return SPIRV_CASETYPE_FREXPSTRUCT; }
};
typedef FrexpStruct< Signature<float, float, int> >				FrexpStruct32Bit;
typedef FrexpStruct< Signature<deFloat16, deFloat16, int> >		FrexpStruct16Bit;
typedef FrexpStruct< Signature<double, double, int> >			FrexpStruct64Bit;

//Signature<float, float, int>
//Signature<deFloat16, deFloat16, int>
//Signature<double, double, int>
template <class T>
class LdExp : public PrimitiveFunc<T >
{
public:
	typedef typename	LdExp::IRet		IRet;
	typedef typename	LdExp::IArgs	IArgs;

	string		getName			(void) const
	{
		return "ldexp";
	}

protected:
	Interval	doApply			(const EvalContext& ctx, const IArgs& iargs) const
	{
		const int minExp = ctx.format.getMinExp();
		const int maxExp = ctx.format.getMaxExp();
		// Restrictions from the GLSL.std.450 instruction set.
		// See Khronos bugzilla 11180 for rationale.
		bool any = iargs.a.hasNaN() || iargs.b.hi() > (maxExp + 1);
		Interval ret(any, ldexp(iargs.a.lo(), (int)iargs.b.lo()), ldexp(iargs.a.hi(), (int)iargs.b.hi()));
		if (iargs.b.lo() < minExp) ret |= 0.0;
		if (!ret.isFinite()) ret |= TCU_NAN;
		return ctx.format.convert(ret);
	}
};

template <>
Interval LdExp <Signature<double, double, int>>::doApply(const EvalContext& ctx, const IArgs& iargs) const
{
	const int minExp = ctx.format.getMinExp();
	const int maxExp = ctx.format.getMaxExp();
	// Restrictions from the GLSL.std.450 instruction set.
	// See Khronos bugzilla 11180 for rationale.
	bool any = iargs.a.hasNaN() || iargs.b.hi() > (maxExp + 1);
	Interval ret(any, ldexp(iargs.a.lo(), (int)iargs.b.lo()), ldexp(iargs.a.hi(), (int)iargs.b.hi()));
	// Add 1ULP precision tolerance to account for differing rounding modes between the GPU and deLdExp.
	ret += Interval(-ctx.format.ulp(ret.lo()), ctx.format.ulp(ret.hi()));
	if (iargs.b.lo() < minExp) ret |= 0.0;
	if (!ret.isFinite()) ret |= TCU_NAN;
	return ctx.format.convert(ret);
}

template<int Rows, int Columns, class T>
class Transpose : public PrimitiveFunc<Signature<Matrix<T, Rows, Columns>,
												 Matrix<T, Columns, Rows> > >
{
public:
	typedef typename Transpose::IRet	IRet;
	typedef typename Transpose::IArgs	IArgs;

	string		getName		(void) const
	{
		return "transpose";
	}

protected:
	IRet		doApply		(const EvalContext&, const IArgs& iargs) const
	{
		IRet ret;

		for (int rowNdx = 0; rowNdx < Rows; ++rowNdx)
		{
			for (int colNdx = 0; colNdx < Columns; ++colNdx)
				ret(rowNdx, colNdx) = iargs.a(colNdx, rowNdx);
		}

		return ret;
	}
};

template<typename Ret, typename Arg0, typename Arg1>
class MulFunc : public PrimitiveFunc<Signature<Ret, Arg0, Arg1> >
{
public:
	string	getName	(void) const									{ return "mul"; }

protected:
	void	doPrint	(ostream& os, const BaseArgExprs& args) const
	{
		os << "(" << *args[0] << " * " << *args[1] << ")";
	}
};

template<typename T, int LeftRows, int Middle, int RightCols>
class MatMul : public MulFunc<Matrix<T, LeftRows, RightCols>,
							  Matrix<T, LeftRows, Middle>,
							  Matrix<T, Middle, RightCols> >
{
protected:
	typedef typename MatMul::IRet	IRet;
	typedef typename MatMul::IArgs	IArgs;
	typedef typename MatMul::IArg0	IArg0;
	typedef typename MatMul::IArg1	IArg1;

	IRet	doApply	(const EvalContext&	ctx, const IArgs& iargs) const
	{
		const IArg0&	left	= iargs.a;
		const IArg1&	right	= iargs.b;
		IRet			ret;

		for (int row = 0; row < LeftRows; ++row)
		{
			for (int col = 0; col < RightCols; ++col)
			{
				Interval	element	(0.0);

				for (int ndx = 0; ndx < Middle; ++ndx)
					element = call<Add< Signature<T, T, T> > >(ctx, element,
										call<Mul< Signature<T, T, T> > >(ctx, left[ndx][row], right[col][ndx]));

				ret[col][row] = element;
			}
		}

		return ret;
	}
};

template<typename T, int Rows, int Cols>
class VecMatMul : public MulFunc<Vector<T, Cols>,
								 Vector<T, Rows>,
								 Matrix<T, Rows, Cols> >
{
public:
	typedef typename VecMatMul::IRet	IRet;
	typedef typename VecMatMul::IArgs	IArgs;
	typedef typename VecMatMul::IArg0	IArg0;
	typedef typename VecMatMul::IArg1	IArg1;

protected:
	IRet	doApply	(const EvalContext& ctx, const IArgs& iargs) const
	{
		const IArg0&	left	= iargs.a;
		const IArg1&	right	= iargs.b;
		IRet			ret;

		for (int col = 0; col < Cols; ++col)
		{
			Interval	element	(0.0);

			for (int row = 0; row < Rows; ++row)
				element = call<Add< Signature<T, T, T> > >(ctx, element, call<Mul< Signature<T, T, T> > >(ctx, left[row], right[col][row]));

			ret[col] = element;
		}

		return ret;
	}
};

template<int Rows, int Cols, class T>
class MatVecMul : public MulFunc<Vector<T, Rows>,
								 Matrix<T, Rows, Cols>,
								 Vector<T, Cols> >
{
public:
	typedef typename MatVecMul::IRet	IRet;
	typedef typename MatVecMul::IArgs	IArgs;
	typedef typename MatVecMul::IArg0	IArg0;
	typedef typename MatVecMul::IArg1	IArg1;

protected:
	IRet	doApply	(const EvalContext& ctx, const IArgs& iargs) const
	{
		const IArg0&	left	= iargs.a;
		const IArg1&	right	= iargs.b;

		return call<VecMatMul<T, Cols, Rows> >(ctx, right,
											call<Transpose<Rows, Cols, T> >(ctx, left));
	}
};

template<int Rows, int Cols, class T>
class OuterProduct : public PrimitiveFunc<Signature<Matrix<T, Rows, Cols>,
													Vector<T, Rows>,
													Vector<T, Cols> > >
{
public:
	typedef typename OuterProduct::IRet		IRet;
	typedef typename OuterProduct::IArgs	IArgs;

	string	getName	(void) const
	{
		return "outerProduct";
	}

protected:
	IRet	doApply	(const EvalContext& ctx, const IArgs& iargs) const
	{
		IRet	ret;

		for (int row = 0; row < Rows; ++row)
		{
			for (int col = 0; col < Cols; ++col)
				ret[col][row] = call<Mul< Signature<T, T, T> > >(ctx, iargs.a[row], iargs.b[col]);
		}

		return ret;
	}
};

template<int Rows, int Cols, class T>
ExprP<Matrix<T, Rows, Cols> > outerProduct (const ExprP<Vector<T, Rows> >& left,
												const ExprP<Vector<T, Cols> >& right)
{
	return app<OuterProduct<Rows, Cols, T> >(left, right);
}

template<class T>
class DeterminantBase : public DerivedFunc<T>
{
public:
	string	getName	(void) const { return "determinant"; }
};

template<int Size> class Determinant;
template<int Size> class Determinant16bit;
template<int Size> class Determinant64bit;

template<int Size>
ExprP<float> determinant (ExprP<Matrix<float, Size, Size> > mat)
{
	return app<Determinant<Size> >(mat);
}

template<int Size>
ExprP<deFloat16> determinant (ExprP<Matrix<deFloat16, Size, Size> > mat)
{
	return app<Determinant16bit<Size> >(mat);
}

template<int Size>
ExprP<double> determinant (ExprP<Matrix<double, Size, Size> > mat)
{
	return app<Determinant64bit<Size> >(mat);
}

template<>
class Determinant<2> : public DeterminantBase<Signature<float, Matrix<float, 2, 2> > >
{
protected:
	ExprP<Ret>	doExpand (ExpandContext&, const ArgExprs& args)	const
	{
		ExprP<Mat2>	mat	= args.a;

		return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
	}
};

template<>
class Determinant<3> : public DeterminantBase<Signature<float, Matrix<float, 3, 3> > >
{
protected:
	ExprP<Ret> doExpand (ExpandContext&, const ArgExprs& args) const
	{
		ExprP<Mat3>	mat	= args.a;

		return (mat[0][0] * (mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1]) +
				mat[0][1] * (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) +
				mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]));
	}
};

template<>
class Determinant<4> : public DeterminantBase<Signature<float, Matrix<float, 4, 4> > >
{
protected:
	 ExprP<Ret>	doExpand	(ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Mat4>	mat	= args.a;
		ExprP<Mat3>	minors[4];

		for (int ndx = 0; ndx < 4; ++ndx)
		{
			ExprP<Vec4>		minorColumns[3];
			ExprP<Vec3>		columns[3];

			for (int col = 0; col < 3; ++col)
				minorColumns[col] = mat[col < ndx ? col : col + 1];

			for (int col = 0; col < 3; ++col)
				columns[col] = vec3(minorColumns[0][col+1],
									minorColumns[1][col+1],
									minorColumns[2][col+1]);

			minors[ndx] = bindExpression("minor", ctx,
										 mat3(columns[0], columns[1], columns[2]));
		}

		return (mat[0][0] * determinant(minors[0]) -
				mat[1][0] * determinant(minors[1]) +
				mat[2][0] * determinant(minors[2]) -
				mat[3][0] * determinant(minors[3]));
	}
};

template<>
class Determinant16bit<2> : public DeterminantBase<Signature<deFloat16, Matrix<deFloat16, 2, 2> > >
{
protected:
	ExprP<Ret>	doExpand (ExpandContext&, const ArgExprs& args)	const
	{
		ExprP<Mat2_16b>	mat	= args.a;

		return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
	}
};

template<>
class Determinant16bit<3> : public DeterminantBase<Signature<deFloat16, Matrix<deFloat16, 3, 3> > >
{
protected:
	ExprP<Ret> doExpand(ExpandContext&, const ArgExprs& args) const
	{
		ExprP<Mat3_16b>	mat = args.a;

		return (mat[0][0] * (mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1]) +
			mat[0][1] * (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) +
			mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]));
	}
};

template<>
class Determinant16bit<4> : public DeterminantBase<Signature<deFloat16, Matrix<deFloat16, 4, 4> > >
{
protected:
	ExprP<Ret>	doExpand(ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Mat4_16b>	mat = args.a;
		ExprP<Mat3_16b>	minors[4];

		for (int ndx = 0; ndx < 4; ++ndx)
		{
			ExprP<Vec4_16Bit>		minorColumns[3];
			ExprP<Vec3_16Bit>		columns[3];

			for (int col = 0; col < 3; ++col)
				minorColumns[col] = mat[col < ndx ? col : col + 1];

			for (int col = 0; col < 3; ++col)
				columns[col] = vec3(minorColumns[0][col + 1],
					minorColumns[1][col + 1],
					minorColumns[2][col + 1]);

			minors[ndx] = bindExpression("minor", ctx,
				mat3(columns[0], columns[1], columns[2]));
		}

		return (mat[0][0] * determinant(minors[0]) -
			mat[1][0] * determinant(minors[1]) +
			mat[2][0] * determinant(minors[2]) -
			mat[3][0] * determinant(minors[3]));
	}
};

template<>
class Determinant64bit<2> : public DeterminantBase<Signature<double, Matrix<double, 2, 2> > >
{
protected:
	ExprP<Ret>	doExpand (ExpandContext&, const ArgExprs& args)	const
	{
		ExprP<Matrix2d>	mat	= args.a;

		return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
	}
};

template<>
class Determinant64bit<3> : public DeterminantBase<Signature<double, Matrix<double, 3, 3> > >
{
protected:
	ExprP<Ret> doExpand(ExpandContext&, const ArgExprs& args) const
	{
		ExprP<Matrix3d>	mat = args.a;

		return (mat[0][0] * (mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1]) +
			mat[0][1] * (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) +
			mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]));
	}
};

template<>
class Determinant64bit<4> : public DeterminantBase<Signature<double, Matrix<double, 4, 4> > >
{
protected:
	ExprP<Ret>	doExpand(ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Matrix4d>	mat = args.a;
		ExprP<Matrix3d>	minors[4];

		for (int ndx = 0; ndx < 4; ++ndx)
		{
			ExprP<Vec4_64Bit>		minorColumns[3];
			ExprP<Vec3_64Bit>		columns[3];

			for (int col = 0; col < 3; ++col)
				minorColumns[col] = mat[col < ndx ? col : col + 1];

			for (int col = 0; col < 3; ++col)
				columns[col] = vec3(minorColumns[0][col + 1],
					minorColumns[1][col + 1],
					minorColumns[2][col + 1]);

			minors[ndx] = bindExpression("minor", ctx,
				mat3(columns[0], columns[1], columns[2]));
		}

		return (mat[0][0] * determinant(minors[0]) -
			mat[1][0] * determinant(minors[1]) +
			mat[2][0] * determinant(minors[2]) -
			mat[3][0] * determinant(minors[3]));
	}
};

template<int Size> class Inverse;

template <int Size>
ExprP<Matrix<float, Size, Size> > inverse (ExprP<Matrix<float, Size, Size> > mat)
{
	return app<Inverse<Size> >(mat);
}

template<int Size> class Inverse16bit;

template <int Size>
ExprP<Matrix<deFloat16, Size, Size> > inverse (ExprP<Matrix<deFloat16, Size, Size> > mat)
{
	return app<Inverse16bit<Size> >(mat);
}

template<int Size> class Inverse64bit;

template <int Size>
ExprP<Matrix<double, Size, Size> > inverse (ExprP<Matrix<double, Size, Size> > mat)
{
	return app<Inverse64bit<Size> >(mat);
}

template<>
class Inverse<2> : public DerivedFunc<Signature<Mat2, Mat2> >
{
public:
	string		getName	(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand (ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Mat2>		mat = args.a;
		ExprP<float>	det	= bindExpression("det", ctx, determinant(mat));

		return mat2(vec2(mat[1][1] / det, -mat[0][1] / det),
					vec2(-mat[1][0] / det, mat[0][0] / det));
	}
};

template<>
class Inverse<3> : public DerivedFunc<Signature<Mat3, Mat3> >
{
public:
	string		getName		(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand	(ExpandContext& ctx, const ArgExprs& args)			const
	{
		ExprP<Mat3>		mat		= args.a;
		ExprP<Mat2>		invA	= bindExpression("invA", ctx,
												 inverse(mat2(vec2(mat[0][0], mat[0][1]),
															  vec2(mat[1][0], mat[1][1]))));

		ExprP<Vec2>		matB	= bindExpression("matB", ctx, vec2(mat[2][0], mat[2][1]));
		ExprP<Vec2>		matC	= bindExpression("matC", ctx, vec2(mat[0][2], mat[1][2]));
		ExprP<float>	matD	= bindExpression("matD", ctx, mat[2][2]);

		ExprP<float>	schur	= bindExpression("schur", ctx,
												 constant(1.0f) /
												 (matD - dot(matC * invA, matB)));

		ExprP<Vec2>		t1		= invA * matB;
		ExprP<Vec2>		t2		= t1 * schur;
		ExprP<Mat2>		t3		= outerProduct(t2, matC);
		ExprP<Mat2>		t4		= t3 * invA;
		ExprP<Mat2>		t5		= invA + t4;
		ExprP<Mat2>		blockA	= bindExpression("blockA", ctx, t5);
		ExprP<Vec2>		blockB	= bindExpression("blockB", ctx,
												 (invA * matB) * -schur);
		ExprP<Vec2>		blockC	= bindExpression("blockC", ctx,
												 (matC * invA) * -schur);

		return mat3(vec3(blockA[0][0], blockA[0][1], blockC[0]),
					vec3(blockA[1][0], blockA[1][1], blockC[1]),
					vec3(blockB[0], blockB[1], schur));
	}
};

template<>
class Inverse<4> : public DerivedFunc<Signature<Mat4, Mat4> >
{
public:
	string		getName		(void) const { return "inverse"; }

protected:
	ExprP<Ret>			doExpand			(ExpandContext&		ctx,
											 const ArgExprs&	args)			const
	{
		ExprP<Mat4>	mat		= args.a;
		ExprP<Mat2>	invA	= bindExpression("invA", ctx,
											 inverse(mat2(vec2(mat[0][0], mat[0][1]),
														  vec2(mat[1][0], mat[1][1]))));
		ExprP<Mat2>	matB	= bindExpression("matB", ctx,
											 mat2(vec2(mat[2][0], mat[2][1]),
												  vec2(mat[3][0], mat[3][1])));
		ExprP<Mat2>	matC	= bindExpression("matC", ctx,
											 mat2(vec2(mat[0][2], mat[0][3]),
												  vec2(mat[1][2], mat[1][3])));
		ExprP<Mat2>	matD	= bindExpression("matD", ctx,
											 mat2(vec2(mat[2][2], mat[2][3]),
												  vec2(mat[3][2], mat[3][3])));
		ExprP<Mat2>	schur	= bindExpression("schur", ctx,
											 inverse(matD + -(matC * invA * matB)));
		ExprP<Mat2>	blockA	= bindExpression("blockA", ctx,
											 invA + (invA * matB * schur * matC * invA));
		ExprP<Mat2>	blockB	= bindExpression("blockB", ctx,
											 (-invA) * matB * schur);
		ExprP<Mat2>	blockC	= bindExpression("blockC", ctx,
											 (-schur) * matC * invA);

		return mat4(vec4(blockA[0][0], blockA[0][1], blockC[0][0], blockC[0][1]),
					vec4(blockA[1][0], blockA[1][1], blockC[1][0], blockC[1][1]),
					vec4(blockB[0][0], blockB[0][1], schur[0][0], schur[0][1]),
					vec4(blockB[1][0], blockB[1][1], schur[1][0], schur[1][1]));
	}
};

template<>
class Inverse16bit<2> : public DerivedFunc<Signature<Mat2_16b, Mat2_16b> >
{
public:
	string		getName	(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand (ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Mat2_16b>		mat = args.a;
		ExprP<deFloat16>	det	= bindExpression("det", ctx, determinant(mat));

		return mat2(vec2((mat[1][1] / det), (-mat[0][1] / det)),
					vec2((-mat[1][0] / det), (mat[0][0] / det)));
	}
};

template<>
class Inverse16bit<3> : public DerivedFunc<Signature<Mat3_16b, Mat3_16b> >
{
public:
	string		getName(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand(ExpandContext& ctx, const ArgExprs& args)			const
	{
		ExprP<Mat3_16b>		mat = args.a;
		ExprP<Mat2_16b>		invA = bindExpression("invA", ctx,
			inverse(mat2(vec2(mat[0][0], mat[0][1]),
				vec2(mat[1][0], mat[1][1]))));

		ExprP<Vec2_16Bit>		matB = bindExpression("matB", ctx, vec2(mat[2][0], mat[2][1]));
		ExprP<Vec2_16Bit>		matC = bindExpression("matC", ctx, vec2(mat[0][2], mat[1][2]));
		ExprP<Mat3_16b::Scalar>	matD = bindExpression("matD", ctx, mat[2][2]);

		ExprP<Mat3_16b::Scalar>	schur = bindExpression("schur", ctx,
			constant((deFloat16)FLOAT16_1_0) /
			(matD - dot(matC * invA, matB)));

		ExprP<Vec2_16Bit>		t1 = invA * matB;
		ExprP<Vec2_16Bit>		t2 = t1 * schur;
		ExprP<Mat2_16b>		t3 = outerProduct(t2, matC);
		ExprP<Mat2_16b>		t4 = t3 * invA;
		ExprP<Mat2_16b>		t5 = invA + t4;
		ExprP<Mat2_16b>		blockA = bindExpression("blockA", ctx, t5);
		ExprP<Vec2_16Bit>		blockB = bindExpression("blockB", ctx,
			(invA * matB) * -schur);
		ExprP<Vec2_16Bit>		blockC = bindExpression("blockC", ctx,
			(matC * invA) * -schur);

		return mat3(vec3(blockA[0][0], blockA[0][1], blockC[0]),
			vec3(blockA[1][0], blockA[1][1], blockC[1]),
			vec3(blockB[0], blockB[1], schur));
	}
};

template<>
class Inverse16bit<4> : public DerivedFunc<Signature<Mat4_16b, Mat4_16b> >
{
public:
	string		getName(void) const { return "inverse"; }

protected:
	ExprP<Ret>			doExpand(ExpandContext&		ctx,
		const ArgExprs&	args)			const
	{
		ExprP<Mat4_16b>	mat = args.a;
		ExprP<Mat2_16b>	invA = bindExpression("invA", ctx,
			inverse(mat2(vec2(mat[0][0], mat[0][1]),
				vec2(mat[1][0], mat[1][1]))));
		ExprP<Mat2_16b>	matB = bindExpression("matB", ctx,
			mat2(vec2(mat[2][0], mat[2][1]),
				vec2(mat[3][0], mat[3][1])));
		ExprP<Mat2_16b>	matC = bindExpression("matC", ctx,
			mat2(vec2(mat[0][2], mat[0][3]),
				vec2(mat[1][2], mat[1][3])));
		ExprP<Mat2_16b>	matD = bindExpression("matD", ctx,
			mat2(vec2(mat[2][2], mat[2][3]),
				vec2(mat[3][2], mat[3][3])));
		ExprP<Mat2_16b>	schur = bindExpression("schur", ctx,
			inverse(matD + -(matC * invA * matB)));
		ExprP<Mat2_16b>	blockA = bindExpression("blockA", ctx,
			invA + (invA * matB * schur * matC * invA));
		ExprP<Mat2_16b>	blockB = bindExpression("blockB", ctx,
			(-invA) * matB * schur);
		ExprP<Mat2_16b>	blockC = bindExpression("blockC", ctx,
			(-schur) * matC * invA);

		return mat4(vec4(blockA[0][0], blockA[0][1], blockC[0][0], blockC[0][1]),
			vec4(blockA[1][0], blockA[1][1], blockC[1][0], blockC[1][1]),
			vec4(blockB[0][0], blockB[0][1], schur[0][0], schur[0][1]),
			vec4(blockB[1][0], blockB[1][1], schur[1][0], schur[1][1]));
	}
};

template<>
class Inverse64bit<2> : public DerivedFunc<Signature<Matrix2d, Matrix2d> >
{
public:
	string		getName	(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand (ExpandContext& ctx, const ArgExprs& args) const
	{
		ExprP<Matrix2d>		mat = args.a;
		ExprP<double>		det	= bindExpression("det", ctx, determinant(mat));

		return mat2(vec2((mat[1][1] / det), (-mat[0][1] / det)),
					vec2((-mat[1][0] / det), (mat[0][0] / det)));
	}
};

template<>
class Inverse64bit<3> : public DerivedFunc<Signature<Matrix3d, Matrix3d> >
{
public:
	string		getName(void) const
	{
		return "inverse";
	}

protected:
	ExprP<Ret>	doExpand(ExpandContext& ctx, const ArgExprs& args)			const
	{
		ExprP<Matrix3d>		mat = args.a;
		ExprP<Matrix2d>		invA = bindExpression("invA", ctx,
			inverse(mat2(vec2(mat[0][0], mat[0][1]),
				vec2(mat[1][0], mat[1][1]))));

		ExprP<Vec2_64Bit>		matB = bindExpression("matB", ctx, vec2(mat[2][0], mat[2][1]));
		ExprP<Vec2_64Bit>		matC = bindExpression("matC", ctx, vec2(mat[0][2], mat[1][2]));
		ExprP<Matrix3d::Scalar>	matD = bindExpression("matD", ctx, mat[2][2]);

		ExprP<Matrix3d::Scalar>	schur = bindExpression("schur", ctx,
			constant(1.0) /
			(matD - dot(matC * invA, matB)));

		ExprP<Vec2_64Bit>		t1 = invA * matB;
		ExprP<Vec2_64Bit>		t2 = t1 * schur;
		ExprP<Matrix2d>			t3 = outerProduct(t2, matC);
		ExprP<Matrix2d>			t4 = t3 * invA;
		ExprP<Matrix2d>			t5 = invA + t4;
		ExprP<Matrix2d>			blockA = bindExpression("blockA", ctx, t5);
		ExprP<Vec2_64Bit>		blockB = bindExpression("blockB", ctx,
			(invA * matB) * -schur);
		ExprP<Vec2_64Bit>		blockC = bindExpression("blockC", ctx,
			(matC * invA) * -schur);

		return mat3(vec3(blockA[0][0], blockA[0][1], blockC[0]),
			vec3(blockA[1][0], blockA[1][1], blockC[1]),
			vec3(blockB[0], blockB[1], schur));
	}
};

template<>
class Inverse64bit<4> : public DerivedFunc<Signature<Matrix4d, Matrix4d> >
{
public:
	string		getName(void) const { return "inverse"; }

protected:
	ExprP<Ret>			doExpand(ExpandContext&		ctx,
		const ArgExprs&	args)			const
	{
		ExprP<Matrix4d>	mat = args.a;
		ExprP<Matrix2d>	invA = bindExpression("invA", ctx,
			inverse(mat2(vec2(mat[0][0], mat[0][1]),
				vec2(mat[1][0], mat[1][1]))));
		ExprP<Matrix2d>	matB = bindExpression("matB", ctx,
			mat2(vec2(mat[2][0], mat[2][1]),
				vec2(mat[3][0], mat[3][1])));
		ExprP<Matrix2d>	matC = bindExpression("matC", ctx,
			mat2(vec2(mat[0][2], mat[0][3]),
				vec2(mat[1][2], mat[1][3])));
		ExprP<Matrix2d>	matD = bindExpression("matD", ctx,
			mat2(vec2(mat[2][2], mat[2][3]),
				vec2(mat[3][2], mat[3][3])));
		ExprP<Matrix2d>	schur = bindExpression("schur", ctx,
			inverse(matD + -(matC * invA * matB)));
		ExprP<Matrix2d>	blockA = bindExpression("blockA", ctx,
			invA + (invA * matB * schur * matC * invA));
		ExprP<Matrix2d>	blockB = bindExpression("blockB", ctx,
			(-invA) * matB * schur);
		ExprP<Matrix2d>	blockC = bindExpression("blockC", ctx,
			(-schur) * matC * invA);

		return mat4(vec4(blockA[0][0], blockA[0][1], blockC[0][0], blockC[0][1]),
			vec4(blockA[1][0], blockA[1][1], blockC[1][0], blockC[1][1]),
			vec4(blockB[0][0], blockB[0][1], schur[0][0], schur[0][1]),
			vec4(blockB[1][0], blockB[1][1], schur[1][0], schur[1][1]));
	}
};

//Signature<float, float, float, float>
//Signature<deFloat16, deFloat16, deFloat16, deFloat16>
//Signature<double, double, double, double>
template <class T>
class Fma : public DerivedFunc<T>
{
public:
	typedef typename	Fma::ArgExprs		ArgExprs;
	typedef typename	Fma::Ret			Ret;

	string			getName					(void) const
	{
		return "fma";
	}

protected:
	ExprP<Ret>	doExpand				(ExpandContext&, const ArgExprs& x) const
	{
		return x.a * x.b + x.c;
	}
};

} // Functions

using namespace Functions;

template <typename T>
ExprP<typename T::Element> ContainerExprPBase<T>::operator[] (int i) const
{
	return Functions::getComponent(exprP<T>(*this), i);
}

ExprP<float> operator+ (const ExprP<float>& arg0, const ExprP<float>& arg1)
{
	return app<Add< Signature<float, float, float> > >(arg0, arg1);
}

ExprP<deFloat16> operator+ (const ExprP<deFloat16>& arg0, const ExprP<deFloat16>& arg1)
{
	return app<Add< Signature<deFloat16, deFloat16, deFloat16> > >(arg0, arg1);
}

ExprP<double> operator+ (const ExprP<double>& arg0, const ExprP<double>& arg1)
{
	return app<Add< Signature<double, double, double> > >(arg0, arg1);
}

template <typename T>
ExprP<T> operator- (const ExprP<T>& arg0, const ExprP<T>& arg1)
{
	return app<Sub <Signature <T,T,T> > >(arg0, arg1);
}

template <typename T>
ExprP<T> operator- (const ExprP<T>& arg0)
{
	return app<Negate< Signature<T, T> > >(arg0);
}

ExprP<float> operator* (const ExprP<float>& arg0, const ExprP<float>& arg1)
{
	return app<Mul< Signature<float, float, float> > >(arg0, arg1);
}

ExprP<deFloat16> operator* (const ExprP<deFloat16>& arg0, const ExprP<deFloat16>& arg1)
{
	return app<Mul< Signature<deFloat16, deFloat16, deFloat16> > >(arg0, arg1);
}

ExprP<double> operator* (const ExprP<double>& arg0, const ExprP<double>& arg1)
{
	return app<Mul< Signature<double, double, double> > >(arg0, arg1);
}

template <typename T>
ExprP<T> operator/ (const ExprP<T>& arg0, const ExprP<T>& arg1)
{
	return app<Div< Signature<T, T, T> > >(arg0, arg1);
}


template <typename Sig_, int Size>
class GenFunc : public PrimitiveFunc<Signature<
	typename ContainerOf<typename Sig_::Ret, Size>::Container,
	typename ContainerOf<typename Sig_::Arg0, Size>::Container,
	typename ContainerOf<typename Sig_::Arg1, Size>::Container,
	typename ContainerOf<typename Sig_::Arg2, Size>::Container,
	typename ContainerOf<typename Sig_::Arg3, Size>::Container> >
{
public:
	typedef typename GenFunc::IArgs		IArgs;
	typedef typename GenFunc::IRet		IRet;

				GenFunc					(const Func<Sig_>&	scalarFunc) : m_func (scalarFunc) {}

	SpirVCaseT	getSpirvCase			(void) const
	{
		return m_func.getSpirvCase();
	}

	string		getName					(void) const
	{
		return m_func.getName();
	}

	int			getOutParamIndex		(void) const
	{
		return m_func.getOutParamIndex();
	}

	string		getRequiredExtension	(void) const
	{
		return m_func.getRequiredExtension();
	}

	Interval	getInputRange			(const bool is16bit) const
	{
		return m_func.getInputRange(is16bit);
	}

protected:
	void		doPrint					(ostream& os, const BaseArgExprs& args) const
	{
		m_func.print(os, args);
	}

	IRet		doApply					(const EvalContext& ctx, const IArgs& iargs) const
	{
		IRet ret;

		for (int ndx = 0; ndx < Size; ++ndx)
		{
			ret[ndx] =
				m_func.apply(ctx, iargs.a[ndx], iargs.b[ndx], iargs.c[ndx], iargs.d[ndx]);
		}

		return ret;
	}

	IRet		doFail					(const EvalContext& ctx, const IArgs& iargs) const
	{
		IRet ret;

		for (int ndx = 0; ndx < Size; ++ndx)
		{
			ret[ndx] =
				m_func.fail(ctx, iargs.a[ndx], iargs.b[ndx], iargs.c[ndx], iargs.d[ndx]);
		}

		return ret;
	}

	void		doGetUsedFuncs			(FuncSet& dst) const
	{
		m_func.getUsedFuncs(dst);
	}

	const Func<Sig_>&	m_func;
};

template <typename F, int Size>
class VectorizedFunc : public GenFunc<typename F::Sig, Size>
{
public:
	VectorizedFunc	(void) : GenFunc<typename F::Sig, Size>(instance<F>()) {}
};

template <typename Sig_, int Size>
class FixedGenFunc : public PrimitiveFunc <Signature<
	typename ContainerOf<typename Sig_::Ret, Size>::Container,
	typename ContainerOf<typename Sig_::Arg0, Size>::Container,
	typename Sig_::Arg1,
	typename ContainerOf<typename Sig_::Arg2, Size>::Container,
	typename ContainerOf<typename Sig_::Arg3, Size>::Container> >
{
public:
	typedef typename FixedGenFunc::IArgs		IArgs;
	typedef typename FixedGenFunc::IRet			IRet;

	string						getName			(void) const
	{
		return this->doGetScalarFunc().getName();
	}

	SpirVCaseT					getSpirvCase	(void) const
	{
		return this->doGetScalarFunc().getSpirvCase();
	}

protected:
	void						doPrint			(ostream& os, const BaseArgExprs& args) const
	{
		this->doGetScalarFunc().print(os, args);
	}

	IRet						doApply			(const EvalContext& ctx,
												 const IArgs&		iargs) const
	{
		IRet				ret;
		const Func<Sig_>&	func	= this->doGetScalarFunc();

		for (int ndx = 0; ndx < Size; ++ndx)
			ret[ndx] = func.apply(ctx, iargs.a[ndx], iargs.b, iargs.c[ndx], iargs.d[ndx]);

		return ret;
	}

	virtual const Func<Sig_>&	doGetScalarFunc	(void) const = 0;
};

template <typename F, int Size>
class FixedVecFunc : public FixedGenFunc<typename F::Sig, Size>
{
protected:
	const Func<typename F::Sig>& doGetScalarFunc	(void) const { return instance<F>(); }
};

template<typename Sig>
struct GenFuncs
{
	GenFuncs (const Func<Sig>&			func_,
			  const GenFunc<Sig, 2>&	func2_,
			  const GenFunc<Sig, 3>&	func3_,
			  const GenFunc<Sig, 4>&	func4_)
		: func	(func_)
		, func2	(func2_)
		, func3	(func3_)
		, func4	(func4_)
	{}

	const Func<Sig>&		func;
	const GenFunc<Sig, 2>&	func2;
	const GenFunc<Sig, 3>&	func3;
	const GenFunc<Sig, 4>&	func4;
};

template<typename F>
GenFuncs<typename F::Sig> makeVectorizedFuncs (void)
{
	return GenFuncs<typename F::Sig>(instance<F>(),
									 instance<VectorizedFunc<F, 2> >(),
									 instance<VectorizedFunc<F, 3> >(),
									 instance<VectorizedFunc<F, 4> >());
}

template<typename T, int Size>
ExprP<Vector<T, Size> > operator/(const ExprP<Vector<T, Size> >&	arg0,
									  const ExprP<T>&					arg1)
{
	return app<FixedVecFunc<Div< Signature<T, T, T> >, Size> >(arg0, arg1);
}

template<typename T, int Size>
ExprP<Vector<T, Size> > operator-(const ExprP<Vector<T, Size> >& arg0)
{
	return app<VectorizedFunc<Negate< Signature<T, T> >, Size> >(arg0);
}

template<typename T, int Size>
ExprP<Vector<T, Size> > operator-(const ExprP<Vector<T, Size> >& arg0,
									  const ExprP<Vector<T, Size> >& arg1)
{
	return app<VectorizedFunc<Sub<Signature<T, T, T> >, Size> >(arg0, arg1);
}

template<int Size, typename T>
ExprP<Vector<T, Size> > operator*(const ExprP<Vector<T, Size> >&	arg0,
								  const ExprP<T>&					arg1)
{
	return app<FixedVecFunc<Mul< Signature<T, T, T> >, Size> >(arg0, arg1);
}

template<typename T, int Size>
ExprP<Vector<T, Size> > operator*(const ExprP<Vector<T, Size> >& arg0,
								  const ExprP<Vector<T, Size> >& arg1)
{
	return app<VectorizedFunc<Mul< Signature<T, T, T> >, Size> >(arg0, arg1);
}

template<int LeftRows, int Middle, int RightCols, typename T>
ExprP<Matrix<T, LeftRows, RightCols> >
operator* (const ExprP<Matrix<T, LeftRows, Middle> >&	left,
		   const ExprP<Matrix<T, Middle, RightCols> >&	right)
{
	return app<MatMul<T, LeftRows, Middle, RightCols> >(left, right);
}

template<int Rows, int Cols, typename T>
ExprP<Vector<T, Rows> > operator* (const ExprP<Vector<T, Cols> >&		left,
								   const ExprP<Matrix<T, Rows, Cols> >&	right)
{
	return app<VecMatMul<T, Rows, Cols> >(left, right);
}

template<int Rows, int Cols, class T>
ExprP<Vector<T, Cols> > operator* (const ExprP<Matrix<T, Rows, Cols> >&	left,
								   const ExprP<Vector<T, Rows> >&		right)
{
	return app<MatVecMul<Rows, Cols, T> >(left, right);
}

template<int Rows, int Cols, typename T>
ExprP<Matrix<T, Rows, Cols> > operator* (const ExprP<Matrix<T, Rows, Cols> >&	left,
										 const ExprP<T>&						right)
{
	return app<ScalarMatFunc<Mul< Signature<T, T, T> >, Rows, Cols> >(left, right);
}

template<int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols> > operator+ (const ExprP<Matrix<float, Rows, Cols> >&	left,
											 const ExprP<Matrix<float, Rows, Cols> >&	right)
{
	return app<CompMatFunc<Add< Signature<float, float, float> >,float, Rows, Cols> >(left, right);
}

template<int Rows, int Cols>
ExprP<Matrix<deFloat16, Rows, Cols> > operator+ (const ExprP<Matrix<deFloat16, Rows, Cols> >&	left,
												 const ExprP<Matrix<deFloat16, Rows, Cols> >&	right)
{
	return app<CompMatFunc<Add< Signature<deFloat16, deFloat16, deFloat16> >, deFloat16, Rows, Cols> >(left, right);
}

template<int Rows, int Cols>
ExprP<Matrix<double, Rows, Cols> > operator+ (const ExprP<Matrix<double, Rows, Cols> >&	left,
											  const ExprP<Matrix<double, Rows, Cols> >&	right)
{
	return app<CompMatFunc<Add< Signature<double, double, double> >, double, Rows, Cols> >(left, right);
}

template<typename T, int Rows, int Cols>
ExprP<Matrix<T, Rows, Cols> > operator- (const ExprP<Matrix<T, Rows, Cols> >&	mat)
{
	return app<MatNeg<T, Rows, Cols> >(mat);
}

template <typename T>
class Sampling
{
public:
	virtual void	genFixeds			(const FloatFormat&, const Precision, vector<T>&, const Interval&)	const {}
	virtual T		genRandom			(const FloatFormat&,const Precision, Random&, const Interval&)		const { return T(); }
	virtual void	removeNotInRange	(vector<T>&, const Interval&, const Precision)					const {};
};

template <>
class DefaultSampling<Void> : public Sampling<Void>
{
public:
	void	genFixeds	(const FloatFormat&, const Precision, vector<Void>& dst, const Interval&) const { dst.push_back(Void()); }
};

template <>
class DefaultSampling<bool> : public Sampling<bool>
{
public:
	void	genFixeds	(const FloatFormat&, const Precision, vector<bool>& dst, const Interval&) const
	{
		dst.push_back(true);
		dst.push_back(false);
	}
};

template <>
class DefaultSampling<int> : public Sampling<int>
{
public:
	int		genRandom	(const FloatFormat&, const Precision prec, Random& rnd, const Interval&) const
	{
		const int	exp		= rnd.getInt(0, getNumBits(prec)-2);
		const int	sign	= rnd.getBool() ? -1 : 1;

		return sign * rnd.getInt(0, (deInt32)1 << exp);
	}

	void	genFixeds	(const FloatFormat&, const Precision, vector<int>& dst, const Interval&) const
	{
		dst.push_back(0);
		dst.push_back(-1);
		dst.push_back(1);
	}

private:
	static inline int getNumBits (Precision prec)
	{
		switch (prec)
		{
			case glu::PRECISION_LAST:
			case glu::PRECISION_MEDIUMP:	return 16;
			case glu::PRECISION_HIGHP:		return 32;
			default:
				DE_ASSERT(false);
				return 0;
		}
	}
};

template <>
class DefaultSampling<float> : public Sampling<float>
{
public:
	float	genRandom			(const FloatFormat& format, const Precision prec, Random& rnd, const Interval& inputRange)			const;
	void	genFixeds			(const FloatFormat& format, const Precision prec, vector<float>& dst, const Interval& inputRange)	const;
	void	removeNotInRange	(vector<float>& dst, const Interval& inputRange, const Precision prec)								const;
};

template <>
class DefaultSampling<double> : public Sampling<double>
{
public:
	double	genRandom			(const FloatFormat& format, const Precision prec, Random& rnd, const Interval& inputRange)			const;
	void	genFixeds			(const FloatFormat& format, const Precision prec, vector<double>& dst, const Interval& inputRange)	const;
	void	removeNotInRange	(vector<double>& dst, const Interval& inputRange, const Precision prec)								const;
};

static bool isDenorm16(deFloat16 v)
{
	const deUint16 mantissa = 0x03FF;
	const deUint16 exponent = 0x7C00;
	return ((exponent & v) == 0 && (mantissa & v) != 0);
}

//! Generate a random double from a reasonable general-purpose distribution.
double randomDouble(const FloatFormat& format, Random& rnd, const Interval& inputRange)
{
	// No testing of subnormals. TODO: Could integrate float controls for some operations.
	const int		minExp			= format.getMinExp();
	const int		maxExp			= format.getMaxExp();
	const bool		haveSubnormal	= false;
	const double	midpoint		= inputRange.midpoint();

	// Choose exponent so that the cumulative distribution is cubic.
	// This makes the probability distribution quadratic, with the peak centered on zero.
	const double	minRoot			= deCbrt(minExp - 0.5 - (haveSubnormal ? 1.0 : 0.0));
	const double	maxRoot			= deCbrt(maxExp + 0.5);
	const int		fractionBits	= format.getFractionBits();
	const int		exp				= int(deRoundEven(dePow(rnd.getDouble(minRoot, maxRoot), 3.0)));

	// Generate some occasional special numbers
	switch (rnd.getInt(0, 64))
	{
		case 0:		return inputRange.contains(0)				? 0				: midpoint;
		case 1:		return inputRange.contains(TCU_INFINITY)	? TCU_INFINITY	: midpoint;
		case 2:		return inputRange.contains(-TCU_INFINITY)	? -TCU_INFINITY	: midpoint;
		case 3:		return inputRange.contains(TCU_NAN)			? TCU_NAN		: midpoint;
		default:	break;
	}

	DE_ASSERT(fractionBits < std::numeric_limits<double>::digits);

	// Normal number
	double base = deLdExp(1.0, exp);
	double quantum = deLdExp(1.0, exp - fractionBits); // smallest representable difference in the binade
	double significand = 0.0;
	switch (rnd.getInt(0, 16))
	{
		case 0: // The highest number in this binade, significand is all bits one.
			significand = base - quantum;
			break;
		case 1: // Significand is one.
			significand = quantum;
			break;
		case 2: // Significand is zero.
			significand = 0.0;
			break;
		default: // Random (evenly distributed) significand.
		{
			deUint64 intFraction = rnd.getUint64() & ((1 << fractionBits) - 1);
			significand = double(intFraction) * quantum;
		}
	}

	// Produce positive numbers more often than negative.
	double value = (rnd.getInt(0, 3) == 0 ? -1.0 : 1.0) * (base + significand);
	return inputRange.contains(value) ? value : midpoint;
}

//! Generate a random float from a reasonable general-purpose distribution.
float DefaultSampling<float>::genRandom (const FloatFormat&	format,
										 Precision			prec,
										 Random&			rnd,
										 const Interval&	inputRange) const
{
	DE_UNREF(prec);
	return (float)randomDouble(format, rnd, inputRange);
}

//! Generate a standard set of floats that should always be tested.
void DefaultSampling<float>::genFixeds (const FloatFormat& format, const Precision prec, vector<float>& dst, const Interval& inputRange) const
{
	const int			minExp			= format.getMinExp();
	const int			maxExp			= format.getMaxExp();
	const int			fractionBits	= format.getFractionBits();
	const float			minQuantum		= deFloatLdExp(1.0f, minExp - fractionBits);
	const float			minNormalized	= deFloatLdExp(1.0f, minExp);
	const float			maxQuantum		= deFloatLdExp(1.0f, maxExp - fractionBits);

	// NaN
	dst.push_back(TCU_NAN);
	// Zero
	dst.push_back(0.0f);

	for (int sign = -1; sign <= 1; sign += 2)
	{
		// Smallest normalized
		dst.push_back((float)sign * minNormalized);

		// Next smallest normalized
		dst.push_back((float)sign * (minNormalized + minQuantum));

		dst.push_back((float)sign * 0.5f);
		dst.push_back((float)sign * 1.0f);
		dst.push_back((float)sign * 2.0f);

		// Largest number
		dst.push_back((float)sign * (deFloatLdExp(1.0f, maxExp) +
									(deFloatLdExp(1.0f, maxExp) - maxQuantum)));

		dst.push_back((float)sign * TCU_INFINITY);
	}
	removeNotInRange(dst, inputRange, prec);
}

void DefaultSampling<float>::removeNotInRange (vector<float>& dst, const Interval& inputRange, const Precision prec) const
{
	for (vector<float>::iterator it = dst.begin(); it < dst.end();)
	{
		// Remove out of range values. PRECISION_LAST means this is an FP16 test so remove any values that
		// will be denorms when converted to FP16. (This is used in the precision_fp16_storage32b test group).
		if ( !inputRange.contains(static_cast<double>(*it)) || (prec == glu::PRECISION_LAST && isDenorm16(deFloat32To16Round(*it, DE_ROUNDINGMODE_TO_ZERO))))
			it = dst.erase(it);
		else
			++it;
	}
}

//! Generate a random double from a reasonable general-purpose distribution.
double DefaultSampling<double>::genRandom (const FloatFormat&	format,
										   Precision			prec,
										   Random&				rnd,
										   const Interval&		inputRange) const
{
	DE_UNREF(prec);
	return randomDouble(format, rnd, inputRange);
}

//! Generate a standard set of floats that should always be tested.
void DefaultSampling<double>::genFixeds (const FloatFormat& format, const Precision prec, vector<double>& dst, const Interval& inputRange) const
{
	const int			minExp			= format.getMinExp();
	const int			maxExp			= format.getMaxExp();
	const int			fractionBits	= format.getFractionBits();
	const double		minQuantum		= deLdExp(1.0, minExp - fractionBits);
	const double		minNormalized	= deLdExp(1.0, minExp);
	const double		maxQuantum		= deLdExp(1.0, maxExp - fractionBits);

	// NaN
	dst.push_back(TCU_NAN);
	// Zero
	dst.push_back(0.0);

	for (int sign = -1; sign <= 1; sign += 2)
	{
		// Smallest normalized
		dst.push_back((double)sign * minNormalized);

		// Next smallest normalized
		dst.push_back((double)sign * (minNormalized + minQuantum));

		dst.push_back((double)sign * 0.5);
		dst.push_back((double)sign * 1.0);
		dst.push_back((double)sign * 2.0);

		// Largest number
		dst.push_back((double)sign * (deLdExp(1.0, maxExp) + (deLdExp(1.0, maxExp) - maxQuantum)));

		dst.push_back((double)sign * TCU_INFINITY);
	}
	removeNotInRange(dst, inputRange, prec);
}

void DefaultSampling<double>::removeNotInRange (vector<double>& dst, const Interval& inputRange, const Precision) const
{
	for (vector<double>::iterator it = dst.begin(); it < dst.end();)
	{
		if ( !inputRange.contains(*it) )
			it = dst.erase(it);
		else
			++it;
	}
}

template <>
class DefaultSampling<deFloat16> : public Sampling<deFloat16>
{
public:
	deFloat16	genRandom			(const FloatFormat& format, const Precision prec, Random& rnd, const Interval& inputRange) const;
	void		genFixeds			(const FloatFormat& format, const Precision prec, vector<deFloat16>& dst, const Interval& inputRange) const;
private:
	void		removeNotInRange(vector<deFloat16>& dst, const Interval& inputRange, const Precision prec) const;
};

//! Generate a random float from a reasonable general-purpose distribution.
deFloat16 DefaultSampling<deFloat16>::genRandom (const FloatFormat& format, const Precision prec,
												Random& rnd, const Interval& inputRange) const
{
	DE_UNREF(prec);
	return deFloat64To16Round(randomDouble(format, rnd, inputRange), DE_ROUNDINGMODE_TO_NEAREST_EVEN);
}

//! Generate a standard set of floats that should always be tested.
void DefaultSampling<deFloat16>::genFixeds (const FloatFormat& format, const Precision prec, vector<deFloat16>& dst, const Interval& inputRange) const
{
	dst.push_back(deUint16(0x3E00)); //1.5
	dst.push_back(deUint16(0x3D00)); //1.25
	dst.push_back(deUint16(0x3F00)); //1.75
	// Zero
	dst.push_back(deUint16(0x0000));
	dst.push_back(deUint16(0x8000));
	// Infinity
	dst.push_back(deUint16(0x7c00));
	dst.push_back(deUint16(0xfc00));
	// SNaN
	dst.push_back(deUint16(0x7c0f));
	dst.push_back(deUint16(0xfc0f));
	// QNaN
	dst.push_back(deUint16(0x7cf0));
	dst.push_back(deUint16(0xfcf0));
	// Normalized
	dst.push_back(deUint16(0x0401));
	dst.push_back(deUint16(0x8401));
	// Some normal number
	dst.push_back(deUint16(0x14cb));
	dst.push_back(deUint16(0x94cb));

	const int			minExp			= format.getMinExp();
	const int			maxExp			= format.getMaxExp();
	const int			fractionBits	= format.getFractionBits();
	const float			minQuantum		= deFloatLdExp(1.0f, minExp - fractionBits);
	const float			minNormalized	= deFloatLdExp(1.0f, minExp);
	const float			maxQuantum		= deFloatLdExp(1.0f, maxExp - fractionBits);

	for (float sign = -1.0; sign <= 1.0f; sign += 2.0f)
	{
		// Smallest normalized
		dst.push_back(deFloat32To16Round(sign * minNormalized, DE_ROUNDINGMODE_TO_NEAREST_EVEN));

		// Next smallest normalized
		dst.push_back(deFloat32To16Round(sign * (minNormalized + minQuantum), DE_ROUNDINGMODE_TO_NEAREST_EVEN));

		dst.push_back(deFloat32To16Round(sign * 0.5f, DE_ROUNDINGMODE_TO_NEAREST_EVEN));
		dst.push_back(deFloat32To16Round(sign * 1.0f, DE_ROUNDINGMODE_TO_NEAREST_EVEN));
		dst.push_back(deFloat32To16Round(sign * 2.0f, DE_ROUNDINGMODE_TO_NEAREST_EVEN));

		// Largest number
		dst.push_back(deFloat32To16Round(sign * (deFloatLdExp(1.0f, maxExp) +
									(deFloatLdExp(1.0f, maxExp) - maxQuantum)), DE_ROUNDINGMODE_TO_NEAREST_EVEN));

		dst.push_back(deFloat32To16Round(sign * TCU_INFINITY, DE_ROUNDINGMODE_TO_NEAREST_EVEN));
	}
	removeNotInRange(dst, inputRange, prec);
}

void DefaultSampling<deFloat16>::removeNotInRange(vector<deFloat16>& dst, const Interval& inputRange, const Precision) const
{
	for (vector<deFloat16>::iterator it = dst.begin(); it < dst.end();)
	{
		if (inputRange.contains(static_cast<double>(*it)))
			++it;
		else
			it = dst.erase(it);
	}
}

template <typename T, int Size>
class DefaultSampling<Vector<T, Size> > : public Sampling<Vector<T, Size> >
{
public:
	typedef Vector<T, Size>		Value;

	Value	genRandom	(const FloatFormat& fmt, const Precision prec, Random& rnd, const Interval& inputRange) const
	{
		Value ret;

		for (int ndx = 0; ndx < Size; ++ndx)
			ret[ndx] = instance<DefaultSampling<T> >().genRandom(fmt, prec, rnd, inputRange);

		return ret;
	}

	void	genFixeds	(const FloatFormat& fmt, const Precision prec, vector<Value>& dst, const Interval& inputRange) const
	{
		vector<T> scalars;

		instance<DefaultSampling<T> >().genFixeds(fmt, prec, scalars, inputRange);

		for (size_t scalarNdx = 0; scalarNdx < scalars.size(); ++scalarNdx)
			dst.push_back(Value(scalars[scalarNdx]));
	}
};

template <typename T, int Rows, int Columns>
class DefaultSampling<Matrix<T, Rows, Columns> > : public Sampling<Matrix<T, Rows, Columns> >
{
public:
	typedef Matrix<T, Rows, Columns>		Value;

	Value	genRandom	(const FloatFormat& fmt, const Precision prec, Random& rnd, const Interval& inputRange) const
	{
		Value ret;

		for (int rowNdx = 0; rowNdx < Rows; ++rowNdx)
			for (int colNdx = 0; colNdx < Columns; ++colNdx)
				ret(rowNdx, colNdx) = instance<DefaultSampling<T> >().genRandom(fmt, prec, rnd, inputRange);

		return ret;
	}

	void	genFixeds	(const FloatFormat& fmt, const Precision prec, vector<Value>& dst, const Interval& inputRange) const
	{
		vector<T> scalars;

		instance<DefaultSampling<T> >().genFixeds(fmt, prec, scalars, inputRange);

		for (size_t scalarNdx = 0; scalarNdx < scalars.size(); ++scalarNdx)
			dst.push_back(Value(scalars[scalarNdx]));

		if (Columns == Rows)
		{
			Value	mat	(T(0.0));
			T		x	= T(1.0f);
			mat[0][0] = x;
			for (int ndx = 0; ndx < Columns; ++ndx)
			{
				mat[Columns-1-ndx][ndx] = x;
				x = static_cast<T>(x * static_cast<T>(2.0f));
			}
			dst.push_back(mat);
		}
	}
};

struct CaseContext
{
					CaseContext		(const string&							name_,
									 TestContext&							testContext_,
									 const FloatFormat&						floatFormat_,
									 const FloatFormat&						highpFormat_,
									 const Precision						precision_,
									 const ShaderType						shaderType_,
									 const size_t							numRandoms_,
									 const PrecisionTestFeatures	precisionTestFeatures_ = PRECISION_TEST_FEATURES_NONE,
									 const bool						isPackFloat16b_ = false,
									 const bool						isFloat64b_ = false)
						: name						(name_)
						, testContext				(testContext_)
						, floatFormat				(floatFormat_)
						, highpFormat				(highpFormat_)
						, precision					(precision_)
						, shaderType				(shaderType_)
						, numRandoms				(numRandoms_)
						, inputRange				(-TCU_INFINITY, TCU_INFINITY)
						, precisionTestFeatures		(precisionTestFeatures_)
						, isPackFloat16b			(isPackFloat16b_)
						, isFloat64b				(isFloat64b_)
					{}

	string							name;
	TestContext&					testContext;
	FloatFormat						floatFormat;
	FloatFormat						highpFormat;
	Precision						precision;
	ShaderType						shaderType;
	size_t							numRandoms;
	Interval						inputRange;
	PrecisionTestFeatures	precisionTestFeatures;
	bool							isPackFloat16b;
	bool					isFloat64b;
};

template<typename In0_ = Void, typename In1_ = Void, typename In2_ = Void, typename In3_ = Void>
struct InTypes
{
	typedef	In0_	In0;
	typedef	In1_	In1;
	typedef	In2_	In2;
	typedef	In3_	In3;
};

template <typename In>
int numInputs (void)
{
	return (!isTypeValid<typename In::In0>() ? 0 :
			!isTypeValid<typename In::In1>() ? 1 :
			!isTypeValid<typename In::In2>() ? 2 :
			!isTypeValid<typename In::In3>() ? 3 :
			4);
}

template<typename Out0_, typename Out1_ = Void>
struct OutTypes
{
	typedef	Out0_	Out0;
	typedef	Out1_	Out1;
};

template <typename Out>
int numOutputs (void)
{
	return (!isTypeValid<typename Out::Out0>() ? 0 :
			!isTypeValid<typename Out::Out1>() ? 1 :
			2);
}

template<typename In>
struct Inputs
{
	vector<typename In::In0>	in0;
	vector<typename In::In1>	in1;
	vector<typename In::In2>	in2;
	vector<typename In::In3>	in3;
};

template<typename Out>
struct Outputs
{
	Outputs	(size_t size) : out0(size), out1(size) {}

	vector<typename Out::Out0>	out0;
	vector<typename Out::Out1>	out1;
};

template<typename In, typename Out>
struct Variables
{
	VariableP<typename In::In0>		in0;
	VariableP<typename In::In1>		in1;
	VariableP<typename In::In2>		in2;
	VariableP<typename In::In3>		in3;
	VariableP<typename Out::Out0>	out0;
	VariableP<typename Out::Out1>	out1;
};

template<typename In>
struct Samplings
{
	Samplings	(const Sampling<typename In::In0>&	in0_,
				 const Sampling<typename In::In1>&	in1_,
				 const Sampling<typename In::In2>&	in2_,
				 const Sampling<typename In::In3>&	in3_)
		: in0 (in0_), in1 (in1_), in2 (in2_), in3 (in3_) {}

	const Sampling<typename In::In0>&	in0;
	const Sampling<typename In::In1>&	in1;
	const Sampling<typename In::In2>&	in2;
	const Sampling<typename In::In3>&	in3;
};

template<typename In>
struct DefaultSamplings : Samplings<In>
{
	DefaultSamplings	(void)
		: Samplings<In>(instance<DefaultSampling<typename In::In0> >(),
						instance<DefaultSampling<typename In::In1> >(),
						instance<DefaultSampling<typename In::In2> >(),
						instance<DefaultSampling<typename In::In3> >()) {}
};

template <typename In, typename Out>
class BuiltinPrecisionCaseTestInstance : public TestInstance
{
public:
									BuiltinPrecisionCaseTestInstance	(Context&						context,
																		 const CaseContext				caseCtx,
																		 const ShaderSpec&				shaderSpec,
																		 const Variables<In, Out>		variables,
																		 const Samplings<In>&			samplings,
																		 const StatementP				stmt,
																		 bool							modularOp = false)
										: TestInstance	(context)
										, m_caseCtx		(caseCtx)
										, m_variables	(variables)
										, m_samplings	(samplings)
										, m_stmt		(stmt)
										, m_executor	(createExecutor(context, caseCtx.shaderType, shaderSpec))
										, m_modularOp	(modularOp)
									{
									}
	virtual tcu::TestStatus			iterate								(void);

protected:
	CaseContext						m_caseCtx;
	Variables<In, Out>				m_variables;
	const Samplings<In>&			m_samplings;
	StatementP						m_stmt;
	de::UniquePtr<ShaderExecutor>	m_executor;
	bool							m_modularOp;
};

template<class In, class Out>
tcu::TestStatus BuiltinPrecisionCaseTestInstance<In, Out>::iterate (void)
{
	typedef typename	In::In0		In0;
	typedef typename	In::In1		In1;
	typedef typename	In::In2		In2;
	typedef typename	In::In3		In3;
	typedef typename	Out::Out0	Out0;
	typedef typename	Out::Out1	Out1;

	areFeaturesSupported(m_context, m_caseCtx.precisionTestFeatures);
	Inputs<In>			inputs		= generateInputs(m_samplings, m_caseCtx.floatFormat, m_caseCtx.precision, m_caseCtx.numRandoms, 0xdeadbeefu + m_caseCtx.testContext.getCommandLine().getBaseSeed(), m_caseCtx.inputRange);
	const FloatFormat&	fmt			= m_caseCtx.floatFormat;
	const int			inCount		= numInputs<In>();
	const int			outCount	= numOutputs<Out>();
	const size_t		numValues	= (inCount > 0) ? inputs.in0.size() : 1;
	Outputs<Out>		outputs		(numValues);
	const FloatFormat	highpFmt	= m_caseCtx.highpFormat;
	const int			maxMsgs		= 100;
	int					numErrors	= 0;
	Environment			env;		// Hoisted out of the inner loop for optimization.
	ResultCollector		status;
	TestLog&			testLog		= m_context.getTestContext().getLog();

	// Module operations need exactly two inputs and have exactly one output.
	if (m_modularOp)
	{
		DE_ASSERT(inCount == 2);
		DE_ASSERT(outCount == 1);
	}

	const void*			inputArr[]	=
	{
		inputs.in0.data(), inputs.in1.data(), inputs.in2.data(), inputs.in3.data(),
	};
	void*				outputArr[]	=
	{
		outputs.out0.data(), outputs.out1.data(),
	};

	// Print out the statement and its definitions
	testLog << TestLog::Message << "Statement: " << m_stmt << TestLog::EndMessage;
	{
		ostringstream	oss;
		FuncSet			funcs;

		m_stmt->getUsedFuncs(funcs);
		for (FuncSet::const_iterator it = funcs.begin(); it != funcs.end(); ++it)
		{
			(*it)->printDefinition(oss);
		}
		if (!funcs.empty())
			testLog << TestLog::Message << "Reference definitions:\n" << oss.str()
				  << TestLog::EndMessage;
	}
	switch (inCount)
	{
		case 4:
			DE_ASSERT(inputs.in3.size() == numValues);
		// Fallthrough
		case 3:
			DE_ASSERT(inputs.in2.size() == numValues);
		// Fallthrough
		case 2:
			DE_ASSERT(inputs.in1.size() == numValues);
		// Fallthrough
		case 1:
			DE_ASSERT(inputs.in0.size() == numValues);
		// Fallthrough
		default:
			break;
	}

	m_executor->execute(int(numValues), inputArr, outputArr);

	// Initialize environment with dummy values so we don't need to bind in inner loop.
	{
		const typename Traits<In0>::IVal		in0;
		const typename Traits<In1>::IVal		in1;
		const typename Traits<In2>::IVal		in2;
		const typename Traits<In3>::IVal		in3;
		const typename Traits<Out0>::IVal		reference0;
		const typename Traits<Out1>::IVal		reference1;

		env.bind(*m_variables.in0, in0);
		env.bind(*m_variables.in1, in1);
		env.bind(*m_variables.in2, in2);
		env.bind(*m_variables.in3, in3);
		env.bind(*m_variables.out0, reference0);
		env.bind(*m_variables.out1, reference1);
	}

	// For each input tuple, compute output reference interval and compare
	// shader output to the reference.
	for (size_t valueNdx = 0; valueNdx < numValues; valueNdx++)
	{
		bool						result			= true;
		const bool					isInput16Bit	= m_executor->areInputs16Bit();
		const bool					isInput64Bit	= m_executor->areInputs64Bit();

		DE_ASSERT(!(isInput16Bit && isInput64Bit));

		typename Traits<Out0>::IVal	reference0;
		typename Traits<Out1>::IVal	reference1;

		if (valueNdx % (size_t)TOUCH_WATCHDOG_VALUE_FREQUENCY == 0)
			m_context.getTestContext().touchWatchdog();

		env.lookup(*m_variables.in0) = convert<In0>(fmt, round(fmt, inputs.in0[valueNdx]));
		env.lookup(*m_variables.in1) = convert<In1>(fmt, round(fmt, inputs.in1[valueNdx]));
		env.lookup(*m_variables.in2) = convert<In2>(fmt, round(fmt, inputs.in2[valueNdx]));
		env.lookup(*m_variables.in3) = convert<In3>(fmt, round(fmt, inputs.in3[valueNdx]));

		{
			EvalContext	ctx (fmt, m_caseCtx.precision, env, 0);
			m_stmt->execute(ctx);

			switch (outCount)
			{
				case 2:
					reference1 = convert<Out1>(highpFmt, env.lookup(*m_variables.out1));
					if (!status.check(contains(reference1, outputs.out1[valueNdx], m_caseCtx.isPackFloat16b), "Shader output 1 is outside acceptable range"))
						result = false;
				// Fallthrough
				case 1:
					{
						// Pass b from mod(a, b) if we are in the modulo operation.
						const tcu::Maybe<In1> modularDivisor = (m_modularOp ? tcu::just(inputs.in1[valueNdx]) : tcu::nothing<In1>());

						reference0 = convert<Out0>(highpFmt, env.lookup(*m_variables.out0));
						if (!status.check(contains(reference0, outputs.out0[valueNdx], m_caseCtx.isPackFloat16b, modularDivisor), "Shader output 0 is outside acceptable range"))
						{
							m_stmt->failed(ctx);
							reference0 = convert<Out0>(highpFmt, env.lookup(*m_variables.out0));
							if (!status.check(contains(reference0, outputs.out0[valueNdx], m_caseCtx.isPackFloat16b, modularDivisor), "Shader output 0 is outside acceptable range"))
								result = false;
						}
					}
				// Fallthrough
				default: break;
			}

		}
		if (!result)
			++numErrors;

		if ((!result && numErrors <= maxMsgs) || GLS_LOG_ALL_RESULTS)
		{
			MessageBuilder	builder	= testLog.message();

			builder << (result ? "Passed" : "Failed") << " sample:\n";

			if (inCount > 0)
			{
				builder << "\t" << m_variables.in0->getName() << " = "
						<< (isInput64Bit ? value64ToString(highpFmt, inputs.in0[valueNdx]) : (isInput16Bit ? value16ToString(highpFmt, inputs.in0[valueNdx]) : value32ToString(highpFmt, inputs.in0[valueNdx]))) << "\n";
			}

			if (inCount > 1)
			{
				builder << "\t" << m_variables.in1->getName() << " = "
						<< (isInput64Bit ? value64ToString(highpFmt, inputs.in1[valueNdx]) : (isInput16Bit ? value16ToString(highpFmt, inputs.in1[valueNdx]) : value32ToString(highpFmt, inputs.in1[valueNdx]))) << "\n";
			}

			if (inCount > 2)
			{
				builder << "\t" << m_variables.in2->getName() << " = "
						<< (isInput64Bit ? value64ToString(highpFmt, inputs.in2[valueNdx]) : (isInput16Bit ? value16ToString(highpFmt, inputs.in2[valueNdx]) : value32ToString(highpFmt, inputs.in2[valueNdx]))) << "\n";
			}

			if (inCount > 3)
			{
				builder << "\t" << m_variables.in3->getName() << " = "
						<< (isInput64Bit ? value64ToString(highpFmt, inputs.in3[valueNdx]) : (isInput16Bit ? value16ToString(highpFmt, inputs.in3[valueNdx]) : value32ToString(highpFmt, inputs.in3[valueNdx]))) << "\n";
			}

			if (outCount > 0)
			{
				if (m_executor->spirvCase() == SPIRV_CASETYPE_COMPARE)
				{
					builder << "Output:\n"
							<< comparisonMessage(outputs.out0[valueNdx])
							<< "Expected result:\n"
							<< comparisonMessageInterval<typename Out::Out0>(reference0) << "\n";
				}
				else
				{
					builder << "\t" << m_variables.out0->getName() << " = "
						<< (m_executor->isOutput64Bit(0u) ? value64ToString(highpFmt, outputs.out0[valueNdx]) : (m_executor->isOutput16Bit(0u) || m_caseCtx.isPackFloat16b ? value16ToString(highpFmt, outputs.out0[valueNdx]) : value32ToString(highpFmt, outputs.out0[valueNdx]))) << "\n"
						<< "\tExpected range: "
						<< intervalToString<typename Out::Out0>(highpFmt, reference0) << "\n";
				}
			}

			if (outCount > 1)
			{
				builder << "\t" << m_variables.out1->getName() << " = "
						<< (m_executor->isOutput64Bit(1u) ? value64ToString(highpFmt, outputs.out1[valueNdx]) : (m_executor->isOutput16Bit(1u) || m_caseCtx.isPackFloat16b ? value16ToString(highpFmt, outputs.out1[valueNdx]) : value32ToString(highpFmt, outputs.out1[valueNdx]))) << "\n"
						<< "\tExpected range: "
						<< intervalToString<typename Out::Out1>(highpFmt, reference1) << "\n";
			}

			builder << TestLog::EndMessage;
		}
	}

	if (numErrors > maxMsgs)
	{
		testLog << TestLog::Message << "(Skipped " << (numErrors - maxMsgs) << " messages.)"
			  << TestLog::EndMessage;
	}

	if (numErrors == 0)
	{
		testLog << TestLog::Message << "All " << numValues << " inputs passed."
			  << TestLog::EndMessage;
	}
	else
	{
		testLog << TestLog::Message << numErrors << "/" << numValues << " inputs failed."
			  << TestLog::EndMessage;
	}

	if (numErrors)
		return tcu::TestStatus::fail(de::toString(numErrors) + string(" test failed. Check log for the details"));
	else
		return tcu::TestStatus::pass("Pass");

}

class PrecisionCase : public TestCase
{
protected:
						PrecisionCase	(const CaseContext& context, const string& name, const Interval& inputRange, const string& extension = "")
							: TestCase		(context.testContext, name.c_str(), name.c_str())
							, m_ctx			(context)
							, m_extension	(extension)
							{
								m_ctx.inputRange = inputRange;
								m_spec.packFloat16Bit = context.isPackFloat16b;
							}

	virtual void		initPrograms	(vk::SourceCollections& programCollection) const
	{
		generateSources(m_ctx.shaderType, m_spec, programCollection);
	}

	const FloatFormat&	getFormat		(void) const			{ return m_ctx.floatFormat; }

	template <typename In, typename Out>
	void				testStatement	(const Variables<In, Out>& variables, const Statement& stmt, SpirVCaseT spirvCase);

	template<typename T>
	Symbol				makeSymbol		(const Variable<T>& variable)
	{
		return Symbol(variable.getName(), getVarTypeOf<T>(m_ctx.precision));
	}

	CaseContext			m_ctx;
	const string		m_extension;
	ShaderSpec			m_spec;
};

template <typename In, typename Out>
void PrecisionCase::testStatement (const Variables<In, Out>& variables, const Statement& stmt, SpirVCaseT spirvCase)
{
	const int		inCount		= numInputs<In>();
	const int		outCount	= numOutputs<Out>();
	Environment		env;		// Hoisted out of the inner loop for optimization.

	// Initialize ShaderSpec from precision, variables and statement.
	if (m_ctx.precision != glu::PRECISION_LAST)
	{
		ostringstream os;
		os << "precision " << glu::getPrecisionName(m_ctx.precision) << " float;\n";
		m_spec.globalDeclarations = os.str();
	}

	if (!m_extension.empty())
		m_spec.globalDeclarations = "#extension " + m_extension + " : require\n";

	m_spec.inputs.resize(inCount);

	switch (inCount)
	{
		case 4:
			m_spec.inputs[3] = makeSymbol(*variables.in3);
		// Fallthrough
		case 3:
			m_spec.inputs[2] = makeSymbol(*variables.in2);
		// Fallthrough
		case 2:
			m_spec.inputs[1] = makeSymbol(*variables.in1);
		// Fallthrough
		case 1:
			m_spec.inputs[0] = makeSymbol(*variables.in0);
		// Fallthrough
		default:
			break;
	}

	bool inputs16Bit = false;
	for (vector<Symbol>::const_iterator symIter = m_spec.inputs.begin(); symIter != m_spec.inputs.end(); ++symIter)
		inputs16Bit = inputs16Bit || glu::isDataTypeFloat16OrVec(symIter->varType.getBasicType());

	if (inputs16Bit || m_spec.packFloat16Bit)
		m_spec.globalDeclarations += "#extension GL_EXT_shader_explicit_arithmetic_types: require\n";

	m_spec.outputs.resize(outCount);

	switch (outCount)
	{
		case 2:
			m_spec.outputs[1] = makeSymbol(*variables.out1);
		// Fallthrough
		case 1:
			m_spec.outputs[0] = makeSymbol(*variables.out0);
		// Fallthrough
		default:
			break;
	}

	m_spec.source = de::toString(stmt);
	m_spec.spirvCase = spirvCase;
}

template <typename T>
struct InputLess
{
	bool operator() (const T& val1, const T& val2) const
	{
		return val1 < val2;
	}
};

template <typename T>
bool inputLess (const T& val1, const T& val2)
{
	return InputLess<T>()(val1, val2);
}

template <>
struct InputLess<float>
{
	bool operator() (const float& val1, const float& val2) const
	{
		if (deIsNaN(val1))
			return false;
		if (deIsNaN(val2))
			return true;
		return val1 < val2;
	}
};

template <typename T, int Size>
struct InputLess<Vector<T, Size> >
{
	bool operator() (const Vector<T, Size>& vec1, const Vector<T, Size>& vec2) const
	{
		for (int ndx = 0; ndx < Size; ++ndx)
		{
			if (inputLess(vec1[ndx], vec2[ndx]))
				return true;
			if (inputLess(vec2[ndx], vec1[ndx]))
				return false;
		}

		return false;
	}
};

template <typename T, int Rows, int Cols>
struct InputLess<Matrix<T, Rows, Cols> >
{
	bool operator() (const Matrix<T, Rows, Cols>& mat1,
					 const Matrix<T, Rows, Cols>& mat2) const
	{
		for (int col = 0; col < Cols; ++col)
		{
			if (inputLess(mat1[col], mat2[col]))
				return true;
			if (inputLess(mat2[col], mat1[col]))
				return false;
		}

		return false;
	}
};

template <typename In>
struct InTuple :
	public Tuple4<typename In::In0, typename In::In1, typename In::In2, typename In::In3>
{
	InTuple	(const typename In::In0& in0,
			 const typename In::In1& in1,
			 const typename In::In2& in2,
			 const typename In::In3& in3)
		: Tuple4<typename In::In0, typename In::In1, typename In::In2, typename In::In3>
		  (in0, in1, in2, in3) {}
};

template <typename In>
struct InputLess<InTuple<In> >
{
	bool operator() (const InTuple<In>& in1, const InTuple<In>& in2) const
	{
		if (inputLess(in1.a, in2.a))
			return true;
		if (inputLess(in2.a, in1.a))
			return false;
		if (inputLess(in1.b, in2.b))
			return true;
		if (inputLess(in2.b, in1.b))
			return false;
		if (inputLess(in1.c, in2.c))
			return true;
		if (inputLess(in2.c, in1.c))
			return false;
		if (inputLess(in1.d, in2.d))
			return true;
		return false;
	};
};

template<typename In>
Inputs<In> generateInputs (const Samplings<In>&		samplings,
						   const FloatFormat&		floatFormat,
						   Precision				intPrecision,
						   size_t					numSamples,
						   deUint32					seed,
						   const Interval&			inputRange)
{
	Random										rnd(seed);
	Inputs<In>									ret;
	Inputs<In>									fixedInputs;
	set<InTuple<In>, InputLess<InTuple<In> > >	seenInputs;

	samplings.in0.genFixeds(floatFormat, intPrecision, fixedInputs.in0, inputRange);
	samplings.in1.genFixeds(floatFormat, intPrecision, fixedInputs.in1, inputRange);
	samplings.in2.genFixeds(floatFormat, intPrecision, fixedInputs.in2, inputRange);
	samplings.in3.genFixeds(floatFormat, intPrecision, fixedInputs.in3, inputRange);

	for (size_t ndx0 = 0; ndx0 < fixedInputs.in0.size(); ++ndx0)
	{
		for (size_t ndx1 = 0; ndx1 < fixedInputs.in1.size(); ++ndx1)
		{
			for (size_t ndx2 = 0; ndx2 < fixedInputs.in2.size(); ++ndx2)
			{
				for (size_t ndx3 = 0; ndx3 < fixedInputs.in3.size(); ++ndx3)
				{
					const InTuple<In>	tuple	(fixedInputs.in0[ndx0],
												 fixedInputs.in1[ndx1],
												 fixedInputs.in2[ndx2],
												 fixedInputs.in3[ndx3]);

					seenInputs.insert(tuple);
					ret.in0.push_back(tuple.a);
					ret.in1.push_back(tuple.b);
					ret.in2.push_back(tuple.c);
					ret.in3.push_back(tuple.d);
				}
			}
		}
	}

	for (size_t ndx = 0; ndx < numSamples; ++ndx)
	{
		const typename In::In0	in0		= samplings.in0.genRandom(floatFormat, intPrecision, rnd, inputRange);
		const typename In::In1	in1		= samplings.in1.genRandom(floatFormat, intPrecision, rnd, inputRange);
		const typename In::In2	in2		= samplings.in2.genRandom(floatFormat, intPrecision, rnd, inputRange);
		const typename In::In3	in3		= samplings.in3.genRandom(floatFormat, intPrecision, rnd, inputRange);
		const InTuple<In>		tuple	(in0, in1, in2, in3);

		if (de::contains(seenInputs, tuple))
			continue;

		seenInputs.insert(tuple);
		ret.in0.push_back(in0);
		ret.in1.push_back(in1);
		ret.in2.push_back(in2);
		ret.in3.push_back(in3);
	}

	return ret;
}

class FuncCaseBase : public PrecisionCase
{
protected:
				FuncCaseBase	(const CaseContext& context, const string& name, const FuncBase& func)
									: PrecisionCase	(context, name, func.getInputRange(!context.isFloat64b && (context.precision == glu::PRECISION_LAST || context.isPackFloat16b)), func.getRequiredExtension())
								{
								}

	StatementP	m_stmt;
};

template <typename Sig>
class FuncCase : public FuncCaseBase
{
public:
	typedef Func<Sig>						CaseFunc;
	typedef typename Sig::Ret				Ret;
	typedef typename Sig::Arg0				Arg0;
	typedef typename Sig::Arg1				Arg1;
	typedef typename Sig::Arg2				Arg2;
	typedef typename Sig::Arg3				Arg3;
	typedef InTypes<Arg0, Arg1, Arg2, Arg3>	In;
	typedef OutTypes<Ret>					Out;

											FuncCase		(const CaseContext& context, const string& name, const CaseFunc& func, bool modularOp = false)
												: FuncCaseBase	(context, name, func)
												, m_func		(func)
												, m_modularOp	(modularOp)
												{
													buildTest();
												}

	virtual	TestInstance*					createInstance	(Context& context) const
	{
		return new BuiltinPrecisionCaseTestInstance<In, Out>(context, m_ctx, m_spec, m_variables, getSamplings(), m_stmt, m_modularOp);
	}

protected:
	void									buildTest		(void);
	virtual const Samplings<In>&			getSamplings	(void) const
	{
		return instance<DefaultSamplings<In> >();
	}

private:
	const CaseFunc&							m_func;
	Variables<In, Out>						m_variables;
	bool									m_modularOp;
};

template <typename Sig>
void FuncCase<Sig>::buildTest (void)
{
	m_variables.out0	= variable<Ret>("out0");
	m_variables.out1	= variable<Void>("out1");
	m_variables.in0		= variable<Arg0>("in0");
	m_variables.in1		= variable<Arg1>("in1");
	m_variables.in2		= variable<Arg2>("in2");
	m_variables.in3		= variable<Arg3>("in3");

	{
		ExprP<Ret> expr	= applyVar(m_func, m_variables.in0, m_variables.in1, m_variables.in2, m_variables.in3);
		m_stmt			= variableAssignment(m_variables.out0, expr);

		this->testStatement(m_variables, *m_stmt, m_func.getSpirvCase());
	}
}

template <typename Sig>
class InOutFuncCase : public FuncCaseBase
{
public:
	typedef Func<Sig>					CaseFunc;
	typedef typename Sig::Ret			Ret;
	typedef typename Sig::Arg0			Arg0;
	typedef typename Sig::Arg1			Arg1;
	typedef typename Sig::Arg2			Arg2;
	typedef typename Sig::Arg3			Arg3;
	typedef InTypes<Arg0, Arg2, Arg3>	In;
	typedef OutTypes<Ret, Arg1>			Out;

										InOutFuncCase	(const CaseContext& context, const string& name, const CaseFunc& func, bool modularOp = false)
											: FuncCaseBase	(context, name, func)
											, m_func		(func)
											, m_modularOp	(modularOp)
											{
												buildTest();
											}
	virtual TestInstance*				createInstance	(Context& context) const
	{
		return new BuiltinPrecisionCaseTestInstance<In, Out>(context, m_ctx, m_spec, m_variables, getSamplings(), m_stmt, m_modularOp);
	}

protected:
	void								buildTest		(void);
	virtual const Samplings<In>&		getSamplings	(void) const
	{
		return instance<DefaultSamplings<In> >();
	}

private:
	const CaseFunc&						m_func;
	Variables<In, Out>					m_variables;
	bool								m_modularOp;
};

template <typename Sig>
void InOutFuncCase<Sig>::buildTest (void)
{
	m_variables.out0	= variable<Ret>("out0");
	m_variables.out1	= variable<Arg1>("out1");
	m_variables.in0		= variable<Arg0>("in0");
	m_variables.in1		= variable<Arg2>("in1");
	m_variables.in2		= variable<Arg3>("in2");
	m_variables.in3		= variable<Void>("in3");

	{
		ExprP<Ret> expr	= applyVar(m_func, m_variables.in0, m_variables.out1, m_variables.in1, m_variables.in2);
		m_stmt			= variableAssignment(m_variables.out0, expr);

		this->testStatement(m_variables, *m_stmt, m_func.getSpirvCase());
	}
}

template <typename Sig>
PrecisionCase* createFuncCase (const CaseContext& context, const string& name, const Func<Sig>&	func, bool modularOp = false)
{
	switch (func.getOutParamIndex())
	{
		case -1:
			return new FuncCase<Sig>(context, name, func, modularOp);
		case 1:
			return new InOutFuncCase<Sig>(context, name, func, modularOp);
		default:
			DE_FATAL("Impossible");
	}
	return DE_NULL;
}

class CaseFactory
{
public:
	virtual						~CaseFactory	(void) {}
	virtual MovePtr<TestNode>	createCase		(const CaseContext& ctx) const = 0;
	virtual string				getName			(void) const = 0;
	virtual string				getDesc			(void) const = 0;
};

class FuncCaseFactory : public CaseFactory
{
public:
	virtual const FuncBase&		getFunc			(void) const = 0;
	string						getName			(void) const { return de::toLower(getFunc().getName()); }
	string						getDesc			(void) const { return "Function '" + getFunc().getName() + "'";	}
};

template <typename Sig>
class GenFuncCaseFactory : public CaseFactory
{
public:
						GenFuncCaseFactory	(const GenFuncs<Sig>& funcs, const string& name, bool modularOp = false)
							: m_funcs			(funcs)
							, m_name			(de::toLower(name))
							, m_modularOp		(modularOp)
							{
							}

	MovePtr<TestNode>	createCase			(const CaseContext& ctx) const
	{
		TestCaseGroup* group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

		group->addChild(createFuncCase(ctx, "scalar",	m_funcs.func,	m_modularOp));
		group->addChild(createFuncCase(ctx, "vec2",		m_funcs.func2,	m_modularOp));
		group->addChild(createFuncCase(ctx, "vec3",		m_funcs.func3,	m_modularOp));
		group->addChild(createFuncCase(ctx, "vec4",		m_funcs.func4,	m_modularOp));
		return MovePtr<TestNode>(group);
	}

	string				getName				(void) const { return m_name; }
	string				getDesc				(void) const { return "Function '" + m_funcs.func.getName() + "'"; }

private:
	const GenFuncs<Sig>	m_funcs;
	string				m_name;
	bool				m_modularOp;
};

template <template <int, class> class GenF, typename T>
class TemplateFuncCaseFactory : public FuncCaseFactory
{
public:
	MovePtr<TestNode>	createCase		(const CaseContext& ctx) const
	{
		TestCaseGroup*	group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

		group->addChild(createFuncCase(ctx, "scalar", instance<GenF<1, T> >()));
		group->addChild(createFuncCase(ctx, "vec2", instance<GenF<2, T> >()));
		group->addChild(createFuncCase(ctx, "vec3", instance<GenF<3, T> >()));
		group->addChild(createFuncCase(ctx, "vec4", instance<GenF<4, T> >()));

		return MovePtr<TestNode>(group);
	}

	const FuncBase&		getFunc			(void) const { return instance<GenF<1, T> >(); }
};

template <template <int> class GenF>
class SquareMatrixFuncCaseFactory : public FuncCaseFactory
{
public:
	MovePtr<TestNode>	createCase		(const CaseContext& ctx) const
	{
		TestCaseGroup* group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

		group->addChild(createFuncCase(ctx, "mat2", instance<GenF<2> >()));
#if 0
		// disabled until we get reasonable results
		group->addChild(createFuncCase(ctx, "mat3", instance<GenF<3> >()));
		group->addChild(createFuncCase(ctx, "mat4", instance<GenF<4> >()));
#endif

		return MovePtr<TestNode>(group);
	}

	const FuncBase&		getFunc			(void) const { return instance<GenF<2> >(); }
};

template <template <int, int, class> class GenF, typename T>
class MatrixFuncCaseFactory : public FuncCaseFactory
{
public:
	MovePtr<TestNode>	createCase		(const CaseContext& ctx) const
	{
		TestCaseGroup*	const group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

		this->addCase<2, 2>(ctx, group);
		this->addCase<3, 2>(ctx, group);
		this->addCase<4, 2>(ctx, group);
		this->addCase<2, 3>(ctx, group);
		this->addCase<3, 3>(ctx, group);
		this->addCase<4, 3>(ctx, group);
		this->addCase<2, 4>(ctx, group);
		this->addCase<3, 4>(ctx, group);
		this->addCase<4, 4>(ctx, group);

		return MovePtr<TestNode>(group);
	}

	const FuncBase&		getFunc			(void) const { return instance<GenF<2,2, T> >(); }

private:
	template <int Rows, int Cols>
	void				addCase			(const CaseContext& ctx, TestCaseGroup* group) const
	{
		const char*	const name = dataTypeNameOf<Matrix<float, Rows, Cols> >();
		group->addChild(createFuncCase(ctx, name, instance<GenF<Rows, Cols, T> >()));
	}
};

template <typename Sig>
class SimpleFuncCaseFactory : public CaseFactory
{
public:
						SimpleFuncCaseFactory	(const Func<Sig>& func) : m_func(func) {}

	MovePtr<TestNode>	createCase				(const CaseContext& ctx) const	{ return MovePtr<TestNode>(createFuncCase(ctx, ctx.name.c_str(), m_func)); }
	string				getName					(void) const					{ return de::toLower(m_func.getName()); }
	string				getDesc					(void) const					{ return "Function '" + getName() + "'"; }

private:
	const Func<Sig>&	m_func;
};

template <typename F>
SharedPtr<SimpleFuncCaseFactory<typename F::Sig> > createSimpleFuncCaseFactory (void)
{
	return SharedPtr<SimpleFuncCaseFactory<typename F::Sig> >(new SimpleFuncCaseFactory<typename F::Sig>(instance<F>()));
}

class CaseFactories
{
public:
	virtual											~CaseFactories	(void) {}
	virtual const std::vector<const CaseFactory*>	getFactories	(void) const = 0;
};

class BuiltinFuncs : public CaseFactories
{
public:
	const vector<const CaseFactory*>		getFactories	(void) const
	{
		vector<const CaseFactory*> ret;

		for (size_t ndx = 0; ndx < m_factories.size(); ++ndx)
			ret.push_back(m_factories[ndx].get());

		return ret;
	}

	void									addFactory		(SharedPtr<const CaseFactory> fact) { m_factories.push_back(fact); }

private:
	vector<SharedPtr<const CaseFactory> >	m_factories;
};

template <typename F>
void addScalarFactory (BuiltinFuncs& funcs, string name = "", bool modularOp = false)
{
	if (name.empty())
		name = instance<F>().getName();

	funcs.addFactory(SharedPtr<const CaseFactory>(new GenFuncCaseFactory<typename F::Sig>(makeVectorizedFuncs<F>(), name, modularOp)));
}

MovePtr<const CaseFactories> createBuiltinCases ()
{
	MovePtr<BuiltinFuncs>	funcs	(new BuiltinFuncs());

	// Tests for ES3 builtins
	addScalarFactory<Comparison< Signature<int, float, float> > >(*funcs);
	addScalarFactory<Add< Signature<float, float, float> > >(*funcs);
	addScalarFactory<Sub< Signature<float, float, float> > >(*funcs);
	addScalarFactory<Mul< Signature<float, float, float> > >(*funcs);
	addScalarFactory<Div< Signature<float, float, float> > >(*funcs);

	addScalarFactory<Radians>(*funcs);
	addScalarFactory<Degrees>(*funcs);
	addScalarFactory<Sin<Signature<float, float> > >(*funcs);
	addScalarFactory<Cos<Signature<float, float> > >(*funcs);
	addScalarFactory<Tan>(*funcs);

	addScalarFactory<ASin>(*funcs);
	addScalarFactory<ACos>(*funcs);
	addScalarFactory<ATan2< Signature<float, float, float> > >(*funcs, "atan2");
	addScalarFactory<ATan<Signature<float, float> > >(*funcs);
	addScalarFactory<Sinh>(*funcs);
	addScalarFactory<Cosh>(*funcs);
	addScalarFactory<Tanh>(*funcs);
	addScalarFactory<ASinh>(*funcs);
	addScalarFactory<ACosh>(*funcs);
	addScalarFactory<ATanh>(*funcs);

	addScalarFactory<Pow>(*funcs);
	addScalarFactory<Exp<Signature<float, float> > >(*funcs);
	addScalarFactory<Log< Signature<float, float> > >(*funcs);
	addScalarFactory<Exp2<Signature<float, float> > >(*funcs);
	addScalarFactory<Log2< Signature<float, float> > >(*funcs);
	addScalarFactory<Sqrt32Bit>(*funcs);
	addScalarFactory<InverseSqrt< Signature<float, float> > >(*funcs);

	addScalarFactory<Abs< Signature<float, float> > >(*funcs);
	addScalarFactory<Sign< Signature<float, float> > >(*funcs);
	addScalarFactory<Floor32Bit>(*funcs);
	addScalarFactory<Trunc32Bit>(*funcs);
	addScalarFactory<Round< Signature<float, float> > >(*funcs);
	addScalarFactory<RoundEven< Signature<float, float> > >(*funcs);
	addScalarFactory<Ceil< Signature<float, float> > >(*funcs);
	addScalarFactory<Fract>(*funcs);

	addScalarFactory<Mod32Bit>(*funcs, "mod", true);
	addScalarFactory<FRem32Bit>(*funcs);

	addScalarFactory<Modf32Bit>(*funcs);
	addScalarFactory<ModfStruct32Bit>(*funcs);
	addScalarFactory<Min< Signature<float, float, float> > >(*funcs);
	addScalarFactory<Max< Signature<float, float, float> > >(*funcs);
	addScalarFactory<Clamp< Signature<float, float, float, float> > >(*funcs);
	addScalarFactory<Mix>(*funcs);
	addScalarFactory<Step< Signature<float, float, float> > >(*funcs);
	addScalarFactory<SmoothStep< Signature<float, float, float, float> > >(*funcs);

	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Length, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Distance, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Dot, float>()));
	funcs->addFactory(createSimpleFuncCaseFactory<Cross>());
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Normalize, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<FaceForward, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Reflect, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Refract, float>()));

	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<MatrixCompMult, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<OuterProduct, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<Transpose, float>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Determinant>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Inverse>()));

	addScalarFactory<Frexp32Bit>(*funcs);
	addScalarFactory<FrexpStruct32Bit>(*funcs);
	addScalarFactory<LdExp <Signature<float, float, int> > >(*funcs);
	addScalarFactory<Fma  <Signature<float, float, float, float> > >(*funcs);

	return MovePtr<const CaseFactories>(funcs.release());
}

MovePtr<const CaseFactories> createBuiltinDoubleCases ()
{
	MovePtr<BuiltinFuncs>	funcs	(new BuiltinFuncs());

	// Tests for ES3 builtins
	addScalarFactory<Comparison<Signature<int, double, double>>>(*funcs);
	addScalarFactory<Add<Signature<double, double, double>>>(*funcs);
	addScalarFactory<Sub<Signature<double, double, double>>>(*funcs);
	addScalarFactory<Mul<Signature<double, double, double>>>(*funcs);
	addScalarFactory<Div<Signature<double, double, double>>>(*funcs);

	// Radians, degrees, sin, cos, tan, asin, acos, atan, sinh, cosh, tanh, asinh, acosh, atanh, atan2, pow, exp, log, exp2 and log2
	// only work with 16-bit and 32-bit floating point types according to the spec.
#if 0
	addScalarFactory<Radians64>(*funcs);
	addScalarFactory<Degrees64>(*funcs);
	addScalarFactory<Sin<Signature<double, double>>>(*funcs);
	addScalarFactory<Cos<Signature<double, double>>>(*funcs);
	addScalarFactory<Tan64Bit>(*funcs);
	addScalarFactory<ASin64Bit>(*funcs);
	addScalarFactory<ACos64Bit>(*funcs);
	addScalarFactory<ATan2<Signature<double, double, double>>>(*funcs, "atan2");
	addScalarFactory<ATan<Signature<double, double>>>(*funcs);
	addScalarFactory<Sinh64Bit>(*funcs);
	addScalarFactory<Cosh64Bit>(*funcs);
	addScalarFactory<Tanh64Bit>(*funcs);
	addScalarFactory<ASinh64Bit>(*funcs);
	addScalarFactory<ACosh64Bit>(*funcs);
	addScalarFactory<ATanh64Bit>(*funcs);

	addScalarFactory<Pow64>(*funcs);
	addScalarFactory<Exp<Signature<double, double>>>(*funcs);
	addScalarFactory<Log<Signature<double, double>>>(*funcs);
	addScalarFactory<Exp2<Signature<double, double>>>(*funcs);
	addScalarFactory<Log2<Signature<double, double>>>(*funcs);
#endif
	addScalarFactory<Sqrt64Bit>(*funcs);
	addScalarFactory<InverseSqrt<Signature<double, double>>>(*funcs);

	addScalarFactory<Abs<Signature<double, double>>>(*funcs);
	addScalarFactory<Sign<Signature<double, double>>>(*funcs);
	addScalarFactory<Floor64Bit>(*funcs);
	addScalarFactory<Trunc64Bit>(*funcs);
	addScalarFactory<Round<Signature<double, double>>>(*funcs);
	addScalarFactory<RoundEven<Signature<double, double>>>(*funcs);
	addScalarFactory<Ceil<Signature<double, double>>>(*funcs);
	addScalarFactory<Fract64Bit>(*funcs);

	addScalarFactory<Mod64Bit>(*funcs, "mod", true);
	addScalarFactory<FRem64Bit>(*funcs);

	addScalarFactory<Modf64Bit>(*funcs);
	addScalarFactory<ModfStruct64Bit>(*funcs);
	addScalarFactory<Min<Signature<double, double, double>>>(*funcs);
	addScalarFactory<Max<Signature<double, double, double>>>(*funcs);
	addScalarFactory<Clamp<Signature<double, double, double, double>>>(*funcs);
	addScalarFactory<Mix64Bit>(*funcs);
	addScalarFactory<Step<Signature<double, double, double>>>(*funcs);
	addScalarFactory<SmoothStep<Signature<double, double, double, double>>>(*funcs);

	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Length, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Distance, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Dot, double>()));
	funcs->addFactory(createSimpleFuncCaseFactory<Cross64Bit>());
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Normalize, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<FaceForward, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Reflect, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Refract, double>()));

	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<MatrixCompMult, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<OuterProduct, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<Transpose, double>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Determinant64bit>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Inverse64bit>()));

	addScalarFactory<Frexp64Bit>(*funcs);
	addScalarFactory<FrexpStruct64Bit>(*funcs);
	addScalarFactory<LdExp<Signature<double, double, int>>>(*funcs);
	addScalarFactory<Fma<Signature<double, double, double, double>>>(*funcs);

	return MovePtr<const CaseFactories>(funcs.release());
}

MovePtr<const CaseFactories> createBuiltinCases16Bit(void)
{
	MovePtr<BuiltinFuncs>	funcs(new BuiltinFuncs());

	addScalarFactory<Comparison< Signature<int, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Add< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Sub< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Mul< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Div< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);

	addScalarFactory<Radians16>(*funcs);
	addScalarFactory<Degrees16>(*funcs);

	addScalarFactory<Sin<Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Cos<Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Tan16Bit>(*funcs);
	addScalarFactory<ASin16Bit>(*funcs);
	addScalarFactory<ACos16Bit>(*funcs);
	addScalarFactory<ATan2< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs, "atan2");
	addScalarFactory<ATan<Signature<deFloat16, deFloat16> > >(*funcs);

	addScalarFactory<Sinh16Bit>(*funcs);
	addScalarFactory<Cosh16Bit>(*funcs);
	addScalarFactory<Tanh16Bit>(*funcs);
	addScalarFactory<ASinh16Bit>(*funcs);
	addScalarFactory<ACosh16Bit>(*funcs);
	addScalarFactory<ATanh16Bit>(*funcs);

	addScalarFactory<Pow16>(*funcs);
	addScalarFactory<Exp< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Log< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Exp2< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Log2< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Sqrt16Bit>(*funcs);
	addScalarFactory<InverseSqrt16Bit>(*funcs);

	addScalarFactory<Abs< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Sign< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Floor16Bit>(*funcs);
	addScalarFactory<Trunc16Bit>(*funcs);
	addScalarFactory<Round< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<RoundEven< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Ceil< Signature<deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Fract16Bit>(*funcs);

	addScalarFactory<Mod16Bit>(*funcs, "mod", true);
	addScalarFactory<FRem16Bit>(*funcs);

	addScalarFactory<Modf16Bit>(*funcs);
	addScalarFactory<ModfStruct16Bit>(*funcs);
	addScalarFactory<Min< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Max< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Clamp< Signature<deFloat16, deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<Mix16Bit>(*funcs);
	addScalarFactory<Step< Signature<deFloat16, deFloat16, deFloat16> > >(*funcs);
	addScalarFactory<SmoothStep< Signature<deFloat16, deFloat16, deFloat16, deFloat16> > >(*funcs);

	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Length, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Distance, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Dot, deFloat16>()));
	funcs->addFactory(createSimpleFuncCaseFactory<Cross16Bit>());
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Normalize, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<FaceForward, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Reflect, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Refract, deFloat16>()));

	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<OuterProduct, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<Transpose, deFloat16>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Determinant16bit>()));
	funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Inverse16bit>()));

	addScalarFactory<Frexp16Bit>(*funcs);
	addScalarFactory<FrexpStruct16Bit>(*funcs);
	addScalarFactory<LdExp <Signature<deFloat16, deFloat16, int> > >(*funcs);
	addScalarFactory<Fma <Signature<deFloat16, deFloat16, deFloat16, deFloat16> > >(*funcs);

	return MovePtr<const CaseFactories>(funcs.release());
}

TestCaseGroup* createFuncGroup (TestContext& ctx, const CaseFactory& factory, int numRandoms)
{
	TestCaseGroup* const	group	= new TestCaseGroup(ctx, factory.getName().c_str(), factory.getDesc().c_str());
	const FloatFormat		highp		(-126, 127, 23, true,
										 tcu::MAYBE,	// subnormals
										 tcu::YES,		// infinities
										 tcu::MAYBE);	// NaN
	const FloatFormat       mediump		(-14, 13, 10, false, tcu::MAYBE);

	for (int precNdx = glu::PRECISION_MEDIUMP; precNdx < glu::PRECISION_LAST; ++precNdx)
	{
		const Precision		precision	= Precision(precNdx);
		const string		precName	(glu::getPrecisionName(precision));
		const FloatFormat&	fmt			= precNdx == glu::PRECISION_MEDIUMP ? mediump : highp;

		const CaseContext	caseCtx		(precName, ctx, fmt, highp, precision, glu::SHADERTYPE_COMPUTE, numRandoms);

		group->addChild(factory.createCase(caseCtx).release());
	}

	return group;
}

TestCaseGroup* createFuncGroupDouble (TestContext& ctx, const CaseFactory& factory, int numRandoms)
{
	TestCaseGroup* const	group		= new TestCaseGroup(ctx, factory.getName().c_str(), factory.getDesc().c_str());
	const Precision			precision	= Precision(glu::PRECISION_LAST);
	const FloatFormat		highp		(-1022, 1023, 52, true,
										 tcu::MAYBE,	// subnormals
										 tcu::YES,		// infinities
										 tcu::MAYBE);	// NaN

	PrecisionTestFeatures precisionTestFeatures = PRECISION_TEST_FEATURES_64BIT_SHADER_FLOAT;

	const CaseContext caseCtx("compute", ctx, highp, highp, precision, glu::SHADERTYPE_COMPUTE, numRandoms, precisionTestFeatures, false, true);
	group->addChild(factory.createCase(caseCtx).release());

	return group;
}

TestCaseGroup* createFuncGroup16Bit(TestContext& ctx, const CaseFactory& factory, int numRandoms, bool storage32)
{
	TestCaseGroup* const	group = new TestCaseGroup(ctx, factory.getName().c_str(), factory.getDesc().c_str());
	const Precision			precision = Precision(glu::PRECISION_LAST);
	const FloatFormat		float16	(-14, 15, 10, true, tcu::MAYBE);

	PrecisionTestFeatures precisionTestFeatures = PRECISION_TEST_FEATURES_16BIT_SHADER_FLOAT;
	if (!storage32)
		precisionTestFeatures |= PRECISION_TEST_FEATURES_16BIT_UNIFORM_AND_STORAGE_BUFFER_ACCESS;

	const CaseContext caseCtx("compute", ctx, float16, float16, precision, glu::SHADERTYPE_COMPUTE, numRandoms, precisionTestFeatures, storage32);
	group->addChild(factory.createCase(caseCtx).release());

	return group;
}

const int defRandoms	= 16384;

void addBuiltinPrecisionTests (TestContext&				ctx,
								TestCaseGroup&			dstGroup,
								const bool				test16Bit = false,
								const bool				storage32Bit = false)
{
	const int userRandoms	= ctx.getCommandLine().getTestIterationCount();
	const int numRandoms	= userRandoms > 0 ? userRandoms : defRandoms;

	MovePtr<const CaseFactories> cases = (test16Bit && !storage32Bit)	? createBuiltinCases16Bit()
																		: createBuiltinCases();
	for (size_t ndx = 0; ndx < cases->getFactories().size(); ++ndx)
	{
		if (!test16Bit)
			dstGroup.addChild(createFuncGroup(ctx, *cases->getFactories()[ndx], numRandoms));
		else
			dstGroup.addChild(createFuncGroup16Bit(ctx, *cases->getFactories()[ndx], numRandoms, storage32Bit));
	}
}

void addBuiltinPrecisionDoubleTests (TestContext&		ctx,
									 TestCaseGroup&		dstGroup)
{
	const int userRandoms	= ctx.getCommandLine().getTestIterationCount();
	const int numRandoms	= userRandoms > 0 ? userRandoms : defRandoms;

	MovePtr<const CaseFactories> cases = createBuiltinDoubleCases();
	for (size_t ndx = 0; ndx < cases->getFactories().size(); ++ndx)
	{
		dstGroup.addChild(createFuncGroupDouble(ctx, *cases->getFactories()[ndx], numRandoms));
	}
}

BuiltinPrecisionTests::BuiltinPrecisionTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup(testCtx, "precision", "Builtin precision tests")
{
}

BuiltinPrecisionTests::~BuiltinPrecisionTests (void)
{
}

void BuiltinPrecisionTests::init (void)
{
	addBuiltinPrecisionTests(m_testCtx, *this);
}

BuiltinPrecisionDoubleTests::BuiltinPrecisionDoubleTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup(testCtx, "precision_double", "Builtin precision tests")
{
}

BuiltinPrecisionDoubleTests::~BuiltinPrecisionDoubleTests (void)
{
}

void BuiltinPrecisionDoubleTests::init (void)
{
	addBuiltinPrecisionDoubleTests(m_testCtx, *this);
}

BuiltinPrecision16BitTests::BuiltinPrecision16BitTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup(testCtx, "precision_fp16_storage16b", "Builtin precision tests")
{
}

BuiltinPrecision16BitTests::~BuiltinPrecision16BitTests (void)
{
}

void BuiltinPrecision16BitTests::init (void)
{
	addBuiltinPrecisionTests(m_testCtx, *this, true);
}

BuiltinPrecision16Storage32BitTests::BuiltinPrecision16Storage32BitTests(tcu::TestContext& testCtx)
	: tcu::TestCaseGroup(testCtx, "precision_fp16_storage32b", "Builtin precision tests")
{
}

BuiltinPrecision16Storage32BitTests::~BuiltinPrecision16Storage32BitTests(void)
{
}

void BuiltinPrecision16Storage32BitTests::init(void)
{
	addBuiltinPrecisionTests(m_testCtx, *this, true, true);
}

} // shaderexecutor
} // vkt
