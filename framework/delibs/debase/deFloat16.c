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
	int sign;
	int expotent;
	int mantissa;
	union
	{
		float	f;
		int		i;
	} x;

	x.f 		= val32;
	sign		= (x.i >> 16) & 0x00008000;
	expotent	= ((x.i >> 23) & 0x000000ff) - (127 - 15);
	mantissa	= x.i & 0x007fffff;

	if (expotent <= 0)
	{
		if (expotent < -10)
		{
			/* Rounds to zero. */
			return (deFloat16) sign;
		}

		/* Converted to denormalized half, add leading 1 to significand. */
		mantissa = mantissa | 0x00800000;

		/* Round mantissa to nearest (10+e) */
		{
			int t = 14 - expotent;
			int a = (1 << (t - 1)) - 1;
			int b = (mantissa >> t) & 1;

			mantissa = (mantissa + a + b) >> t;
		}

		return (deFloat16) (sign | mantissa);
	}
	else if (expotent == 0xff - (127 - 15))
	{
		if (mantissa == 0)
		{
			/* InF */
			return (deFloat16) (sign | 0x7c00);
		}
		else
		{
			/* NaN */
			mantissa >>= 13;
			return (deFloat16) (sign | 0x7c00 | mantissa | (mantissa == 0));
		}
	}
	else
	{
		/* Normalized float. */
		mantissa = mantissa + 0x00000fff + ((mantissa >> 13) & 1);

		if (mantissa & 0x00800000)
		{
			/* Overflow in mantissa. */
			mantissa  = 0;
			expotent += 1;
		}

		if (expotent > 30)
		{
			/* \todo [pyry] Cause hw fp overflow */
			return (deFloat16) (sign | 0x7c00);
		}

		return (deFloat16) (sign | (expotent << 10) | (mantissa >> 13));
	}
}

float deFloat16To32 (deFloat16 val16)
{
	int sign;
	int expotent;
	int mantissa;
	union
	{
		float	f;
		int		i;
	} x;

	x.i			= 0;

	sign		= ((int) val16 >> 15) & 0x00000001;
	expotent	= ((int) val16 >> 10) & 0x0000001f;
	mantissa	= (int) val16 & 0x000003ff;

	if (expotent == 0)
	{
		if (mantissa == 0)
		{
			/* +/- 0 */
			x.i = sign << 31;
			return x.f;
		}
		else
		{
			/* Denormalized, normalize it. */

			while (!(mantissa & 0x00000400))
			{
				mantissa <<= 1;
				expotent -=  1;
			}

			expotent += 1;
			mantissa &= ~0x00000400;
		}
	}
	else if (expotent == 31)
	{
		if (mantissa == 0)
		{
			/* +/- InF */
			x.i = (sign << 31) | 0x7f800000;
			return x.f;
		}
		else
		{
			/* +/- NaN */
			x.i = (sign << 31) | 0x7f800000 | (mantissa << 13);
			return x.f;
		}
	}

	expotent = expotent + (127 - 15);
	mantissa = mantissa << 13;

	x.i = (sign << 31) | (expotent << 23) | mantissa;
	return x.f;
}

DE_END_EXTERN_C
