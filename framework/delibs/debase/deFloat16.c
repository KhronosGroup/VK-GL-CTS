/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief 16-bit floating-point math.
 *//*--------------------------------------------------------------------*/

#include "deFloat16.h"

DE_BEGIN_EXTERN_C

deFloat16 deFloat32To16 (float val32)
{
	deUint32	sign;
	int			expotent;
	deUint32	mantissa;
	union
	{
		float		f;
		deUint32	u;
	} x;

	x.f 		= val32;
	sign		= (x.u >> 16u) & 0x00008000u;
	expotent	= (int)((x.u >> 23u) & 0x000000ffu) - (127 - 15);
	mantissa	= x.u & 0x007fffffu;

	if (expotent <= 0)
	{
		if (expotent < -10)
		{
			/* Rounds to zero. */
			return (deFloat16) sign;
		}

		/* Converted to denormalized half, add leading 1 to significand. */
		mantissa = mantissa | 0x00800000u;

		/* Round mantissa to nearest (10+e) */
		{
			deUint32 t = 14u - expotent;
			deUint32 a = (1u << (t - 1u)) - 1u;
			deUint32 b = (mantissa >> t) & 1u;

			mantissa = (mantissa + a + b) >> t;
		}

		return (deFloat16) (sign | mantissa);
	}
	else if (expotent == 0xff - (127 - 15))
	{
		if (mantissa == 0u)
		{
			/* InF */
			return (deFloat16) (sign | 0x7c00u);
		}
		else
		{
			/* NaN */
			mantissa >>= 13u;
			return (deFloat16) (sign | 0x7c00u | mantissa | (mantissa == 0u));
		}
	}
	else
	{
		/* Normalized float. */
		mantissa = mantissa + 0x00000fffu + ((mantissa >> 13u) & 1u);

		if (mantissa & 0x00800000u)
		{
			/* Overflow in mantissa. */
			mantissa  = 0u;
			expotent += 1;
		}

		if (expotent > 30)
		{
			/* \todo [pyry] Cause hw fp overflow */
			return (deFloat16) (sign | 0x7c00u);
		}

		return (deFloat16) (sign | ((deUint32)expotent << 10u) | (mantissa >> 13u));
	}
}

float deFloat16To32 (deFloat16 val16)
{
	deUint32 sign;
	deUint32 expotent;
	deUint32 mantissa;
	union
	{
		float		f;
		deUint32	u;
	} x;

	x.u			= 0u;

	sign		= ((deUint32)val16 >> 15u) & 0x00000001u;
	expotent	= ((deUint32)val16 >> 10u) & 0x0000001fu;
	mantissa	= (deUint32)val16 & 0x000003ffu;

	if (expotent == 0u)
	{
		if (mantissa == 0u)
		{
			/* +/- 0 */
			x.u = sign << 31u;
			return x.f;
		}
		else
		{
			/* Denormalized, normalize it. */

			while (!(mantissa & 0x00000400u))
			{
				mantissa <<= 1u;
				expotent -=  1u;
			}

			expotent += 1u;
			mantissa &= ~0x00000400u;
		}
	}
	else if (expotent == 31u)
	{
		if (mantissa == 0u)
		{
			/* +/- InF */
			x.u = (sign << 31u) | 0x7f800000u;
			return x.f;
		}
		else
		{
			/* +/- NaN */
			x.u = (sign << 31u) | 0x7f800000u | (mantissa << 13u);
			return x.f;
		}
	}

	expotent = expotent + (127u - 15u);
	mantissa = mantissa << 13u;

	x.u = (sign << 31u) | (expotent << 23u) | mantissa;
	return x.f;
}

DE_END_EXTERN_C
