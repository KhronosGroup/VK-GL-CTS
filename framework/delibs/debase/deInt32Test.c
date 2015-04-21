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
 * \brief Testing of deInt32 functions.
 *//*--------------------------------------------------------------------*/

#include "deInt32.h"
#include "deRandom.h"

#include <stdio.h> /* printf() */

DE_BEGIN_EXTERN_C

void deInt32_computeLUTs (void)
{
	enum
	{
		RCP_LUT_BITS = 8
	};

	int ndx;

	printf("enum { RCP_LUT_BITS = %d };\n", RCP_LUT_BITS);
	printf("static const deUint32 s_rcpLUT[1<<RCP_LUT_BITS] =\n");
	printf("{\n");

	for (ndx = 0; ndx < (1<<RCP_LUT_BITS); ndx++)
	{
		deUint32	val = (1u << RCP_LUT_BITS) | ndx;
		deUint32	rcp = (int)((1u << DE_RCP_FRAC_BITS) / ((double)val / (1<<RCP_LUT_BITS)));

		if ((ndx & 3) == 0)
			printf("\t");

		printf("0x%08x", rcp);

		if ((ndx & 3) == 3)
		{
			if (ndx != (1<<RCP_LUT_BITS)-1)
				printf(",");
			printf("\n");
		}
		else
			printf(", ");
	}

	printf("};\n");
}

void deInt32_selfTest (void)
{
	const int	NUM_ACCURATE_BITS	= 29;

	deRandom	rnd;
	deUint32	rcp;
	int			exp;
	int			numBits;

	deRandom_init(&rnd, 0xdeadbeefu-1);

	/* Test deClz32(). */
	DE_TEST_ASSERT(deClz32(0) == 32);
	DE_TEST_ASSERT(deClz32(1) == 31);
	DE_TEST_ASSERT(deClz32(0xF1) == 24);
	DE_TEST_ASSERT(deClz32(0xBC12) == 16);
	DE_TEST_ASSERT(deClz32(0xABBACD) == 8);
	DE_TEST_ASSERT(deClz32(0x10000000) == 3);
	DE_TEST_ASSERT(deClz32(0x20000000) == 2);
	DE_TEST_ASSERT(deClz32(0x40000000) == 1);
	DE_TEST_ASSERT(deClz32(0x80000000) == 0);

	/* Test simple inputs for dePop32(). */
	DE_TEST_ASSERT(dePop32(0) == 0);
	DE_TEST_ASSERT(dePop32(~0) == 32);
	DE_TEST_ASSERT(dePop32(0xFF) == 8);
	DE_TEST_ASSERT(dePop32(0xFF00FF) == 16);
	DE_TEST_ASSERT(dePop32(0x3333333) == 14);
	DE_TEST_ASSERT(dePop32(0x33333333) == 16);

	/* dePop32(): Check exp2(N) values and inverses. */
	for (numBits = 0; numBits < 32; numBits++)
	{
		DE_TEST_ASSERT(dePop32(1<<numBits) == 1);
		DE_TEST_ASSERT(dePop32(~(1<<numBits)) == 31);
	}

	/* Check exp2(N) values. */
	for (numBits = 0; numBits < 32; numBits++)
	{
		deUint32 val = (1u<<numBits);
		deRcp32(val, &rcp, &exp);

		DE_TEST_ASSERT(rcp == (1u<<DE_RCP_FRAC_BITS));
		DE_TEST_ASSERT(exp == numBits);
	}

	/* Check random values. */
	for (numBits = 0; numBits < 32; numBits++)
	{
		int NUM_ITERS = deMax32(16, 1 << (numBits/2));
		int iter;

		for (iter = 0; iter < NUM_ITERS; iter++)
		{
			const int	EPS = 1 << (DE_RCP_FRAC_BITS - NUM_ACCURATE_BITS);

			deUint32	val = (deRandom_getUint32(&rnd) & ((1u<<numBits)-1)) | (1u<<numBits);
			deUint32	ref = (deUint32)(((1.0f / (double)val) * (double)(1<<DE_RCP_FRAC_BITS)) * (double)(1u<<numBits));

			deRcp32(val, &rcp, &exp);

			DE_TEST_ASSERT(rcp >= ref-EPS && rcp < ref+EPS);
			DE_TEST_ASSERT(exp == numBits);
		}
	}

	DE_TEST_ASSERT(deBitMask32(0, 0) == 0);
	DE_TEST_ASSERT(deBitMask32(8, 0) == 0);
	DE_TEST_ASSERT(deBitMask32(16, 0) == 0);
	DE_TEST_ASSERT(deBitMask32(31, 0) == 0);
	DE_TEST_ASSERT(deBitMask32(32, 0) == 0);

	DE_TEST_ASSERT(deBitMask32(0, 2) == 3);
	DE_TEST_ASSERT(deBitMask32(0, 32) == 0xFFFFFFFFu);

	DE_TEST_ASSERT(deBitMask32(16, 16) == 0xFFFF0000u);
	DE_TEST_ASSERT(deBitMask32(31, 1) == 0x80000000u);
	DE_TEST_ASSERT(deBitMask32(8, 4) == 0xF00u);

	DE_TEST_ASSERT(deUintMaxValue32(1) == 1);
	DE_TEST_ASSERT(deUintMaxValue32(2) == 3);
	DE_TEST_ASSERT(deUintMaxValue32(32) == 0xFFFFFFFFu);

	DE_TEST_ASSERT(deIntMaxValue32(1) == 0);
	DE_TEST_ASSERT(deIntMaxValue32(2) == 1);
	DE_TEST_ASSERT(deIntMaxValue32(32) == 0x7FFFFFFF);

	DE_TEST_ASSERT(deIntMinValue32(1) == -1);
	DE_TEST_ASSERT(deIntMinValue32(2) == -2);
	DE_TEST_ASSERT(deIntMinValue32(32) == -0x7FFFFFFF - 1);
}

DE_END_EXTERN_C
