/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2020 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan Reconvergence tests
 *//*--------------------------------------------------------------------*/

#include "vktReconvergenceTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <bitset>
#include <string>
#include <sstream>
#include <set>
#include <vector>

namespace vkt
{
namespace Reconvergence
{
namespace
{
using namespace vk;
using namespace std;

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT;

typedef enum {
	TT_SUCF_ELECT,	// subgroup_uniform_control_flow using elect (subgroup_basic)
	TT_SUCF_BALLOT,	// subgroup_uniform_control_flow using ballot (subgroup_ballot)
	TT_WUCF_ELECT,	// workgroup uniform control flow using elect (subgroup_basic)
	TT_WUCF_BALLOT,	// workgroup uniform control flow using ballot (subgroup_ballot)
	TT_MAXIMAL,		// maximal reconvergence
} TestType;

struct CaseDef
{
	TestType testType;
	deUint32 maxNesting;
	deUint32 seed;

	bool isWUCF() const { return testType == TT_WUCF_ELECT || testType == TT_WUCF_BALLOT; }
	bool isSUCF() const { return testType == TT_SUCF_ELECT || testType == TT_SUCF_BALLOT; }
	bool isUCF() const { return isWUCF() || isSUCF(); }
	bool isElect() const { return testType == TT_WUCF_ELECT || testType == TT_SUCF_ELECT; }
};

deUint64 subgroupSizeToMask(deUint32 subgroupSize)
{
	if (subgroupSize == 64)
		return ~0ULL;
	else
		return (1ULL << subgroupSize) - 1;
}

typedef std::bitset<128> bitset128;

// Take a 64-bit integer, mask it to the subgroup size, and then
// replicate it for each subgroup
bitset128 bitsetFromU64(deUint64 mask, deUint32 subgroupSize)
{
	mask &= subgroupSizeToMask(subgroupSize);
	bitset128 result(mask);
	for (deUint32 i = 0; i < 128 / subgroupSize - 1; ++i)
	{
		result = (result << subgroupSize) | bitset128(mask);
	}
	return result;
}

// Pick out the mask for the subgroup that invocationID is a member of
deUint64 bitsetToU64(const bitset128 &bitset, deUint32 subgroupSize, deUint32 invocationID)
{
	bitset128 copy(bitset);
	copy >>= (invocationID / subgroupSize) * subgroupSize;
	copy &= bitset128(subgroupSizeToMask(subgroupSize));
	deUint64 mask = copy.to_ullong();
	mask &= subgroupSizeToMask(subgroupSize);
	return mask;
}

class ReconvergenceTestInstance : public TestInstance
{
public:
						ReconvergenceTestInstance	(Context& context, const CaseDef& data);
						~ReconvergenceTestInstance	(void);
	tcu::TestStatus		iterate				(void);
private:
	CaseDef			m_data;
};

ReconvergenceTestInstance::ReconvergenceTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

ReconvergenceTestInstance::~ReconvergenceTestInstance (void)
{
}

class ReconvergenceTestCase : public TestCase
{
	public:
								ReconvergenceTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~ReconvergenceTestCase	(void);
	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance		(Context& context) const;
	virtual void				checkSupport		(Context& context) const;

private:
	CaseDef					m_data;
};

ReconvergenceTestCase::ReconvergenceTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

ReconvergenceTestCase::~ReconvergenceTestCase	(void)
{
}

void ReconvergenceTestCase::checkSupport(Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");

	vk::VkPhysicalDeviceSubgroupProperties subgroupProperties;
	deMemset(&subgroupProperties, 0, sizeof(subgroupProperties));
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

	vk::VkPhysicalDeviceProperties2 properties2;
	deMemset(&properties2, 0, sizeof(properties2));
	properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

	if (m_data.isElect() && !(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT))
		TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BASIC_BIT not supported");

	if (!m_data.isElect() && !(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT))
		TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BALLOT_BIT not supported");

	if (!(context.getSubgroupProperties().supportedStages & VK_SHADER_STAGE_COMPUTE_BIT))
		TCU_THROW(NotSupportedError, "compute stage does not support subgroup operations");

	// Both subgroup- AND workgroup-uniform tests are enabled by shaderSubgroupUniformControlFlow.
	if (m_data.isUCF() && !context.getShaderSubgroupUniformControlFlowFeatures().shaderSubgroupUniformControlFlow)
		TCU_THROW(NotSupportedError, "shaderSubgroupUniformControlFlow not supported");

	// XXX TODO: Check for maximal reconvergence support
	// if (m_data.testType == TT_MAXIMAL ...)
}

typedef enum
{
	// store subgroupBallot().
	// For OP_BALLOT, OP::caseValue is initialized to zero, and then
	// set to 1 by simulate if the ballot is not workgroup- (or subgroup-_uniform.
	// Only workgroup-uniform ballots are validated for correctness in
	// WUCF modes.
	OP_BALLOT,

	// store literal constant
	OP_STORE,

	// if ((1ULL << gl_SubgroupInvocationID) & mask).
	// Special case if mask = ~0ULL, converted into "if (inputA.a[idx] == idx)"
	OP_IF_MASK,
	OP_ELSE_MASK,
	OP_ENDIF,

	// if (gl_SubgroupInvocationID == loopIdxN) (where N is most nested loop counter)
	OP_IF_LOOPCOUNT,
	OP_ELSE_LOOPCOUNT,

	// if (gl_LocalInvocationIndex >= inputA.a[N]) (where N is most nested loop counter)
	OP_IF_LOCAL_INVOCATION_INDEX,
	OP_ELSE_LOCAL_INVOCATION_INDEX,

	// break/continue
	OP_BREAK,
	OP_CONTINUE,

	// if (subgroupElect())
	OP_ELECT,

	// Loop with uniform number of iterations (read from a buffer)
	OP_BEGIN_FOR_UNIF,
	OP_END_FOR_UNIF,

	// for (int loopIdxN = 0; loopIdxN < gl_SubgroupInvocationID + 1; ++loopIdxN)
	OP_BEGIN_FOR_VAR,
	OP_END_FOR_VAR,

	// for (int loopIdxN = 0;; ++loopIdxN, OP_BALLOT)
	// Always has an "if (subgroupElect()) break;" inside.
	// Does the equivalent of OP_BALLOT in the continue construct
	OP_BEGIN_FOR_INF,
	OP_END_FOR_INF,

	// do { loopIdxN++; ... } while (loopIdxN < uniformValue);
	OP_BEGIN_DO_WHILE_UNIF,
	OP_END_DO_WHILE_UNIF,

	// do { ... } while (true);
	// Always has an "if (subgroupElect()) break;" inside
	OP_BEGIN_DO_WHILE_INF,
	OP_END_DO_WHILE_INF,

	// return;
	OP_RETURN,

	// function call (code bracketed by these is extracted into a separate function)
	OP_CALL_BEGIN,
	OP_CALL_END,

	// switch statement on uniform value
	OP_SWITCH_UNIF_BEGIN,
	// switch statement on gl_SubgroupInvocationID & 3 value
	OP_SWITCH_VAR_BEGIN,
	// switch statement on loopIdx value
	OP_SWITCH_LOOP_COUNT_BEGIN,

	// case statement with a (invocation mask, case mask) pair
	OP_CASE_MASK_BEGIN,
	// case statement used for loop counter switches, with a value and a mask of loop iterations
	OP_CASE_LOOP_COUNT_BEGIN,

	// end of switch/case statement
	OP_SWITCH_END,
	OP_CASE_END,

	// Extra code with no functional effect. Currently inculdes:
	// - value 0: while (!subgroupElect()) {}
	// - value 1: if (condition_that_is_false) { infinite loop }
	OP_NOISE,
} OPType;

typedef enum
{
	// Different if test conditions
	IF_MASK,
	IF_UNIFORM,
	IF_LOOPCOUNT,
	IF_LOCAL_INVOCATION_INDEX,
} IFType;

class OP
{
public:
	OP(OPType _type, deUint64 _value, deUint32 _caseValue = 0)
		: type(_type), value(_value), caseValue(_caseValue)
	{}

	// The type of operation and an optional value.
	// The value could be a mask for an if test, the index of the loop
	// header for an end of loop, or the constant value for a store instruction
	OPType type;
	deUint64 value;
	deUint32 caseValue;
};

static int findLSB (deUint64 value)
{
	for (int i = 0; i < 64; i++)
	{
		if (value & (1ULL<<i))
			return i;
	}
	return -1;
}

// For each subgroup, pick out the elected invocationID, and accumulate
// a bitset of all of them
static bitset128 bitsetElect (const bitset128& value, deInt32 subgroupSize)
{
	bitset128 ret; // zero initialized

	for (deInt32 i = 0; i < 128; i += subgroupSize)
	{
		deUint64 mask = bitsetToU64(value, subgroupSize, i);
		int lsb = findLSB(mask);
		ret |= bitset128(lsb == -1 ? 0 : (1ULL << lsb)) << i;
	}
	return ret;
}

class RandomProgram
{
public:
	RandomProgram(const CaseDef &c)
		: caseDef(c), numMasks(5), nesting(0), maxNesting(c.maxNesting), loopNesting(0), loopNestingThisFunction(0), callNesting(0), minCount(30), indent(0), isLoopInf(100, false), doneInfLoopBreak(100, false), storeBase(0x10000)
	{
		deRandom_init(&rnd, caseDef.seed);
		for (int i = 0; i < numMasks; ++i)
			masks.push_back(deRandom_getUint64(&rnd));
	}

	const CaseDef caseDef;
	deRandom rnd;
	vector<OP> ops;
	vector<deUint64> masks;
	deInt32 numMasks;
	deInt32 nesting;
	deInt32 maxNesting;
	deInt32 loopNesting;
	deInt32 loopNestingThisFunction;
	deInt32 callNesting;
	deInt32 minCount;
	deInt32 indent;
	vector<bool> isLoopInf;
	vector<bool> doneInfLoopBreak;
	// Offset the value we use for OP_STORE, to avoid colliding with fully converged
	// active masks with small subgroup sizes (e.g. with subgroupSize == 4, the SUCF
	// tests need to know that 0xF is really an active mask).
	deInt32 storeBase;

	void genIf(IFType ifType)
	{
		deUint32 maskIdx = deRandom_getUint32(&rnd) % numMasks;
		deUint64 mask = masks[maskIdx];
		if (ifType == IF_UNIFORM)
			mask = ~0ULL;

		deUint32 localIndexCmp = deRandom_getUint32(&rnd) % 128;
		if (ifType == IF_LOCAL_INVOCATION_INDEX)
			ops.push_back({OP_IF_LOCAL_INVOCATION_INDEX, localIndexCmp});
		else if (ifType == IF_LOOPCOUNT)
			ops.push_back({OP_IF_LOOPCOUNT, 0});
		else
			ops.push_back({OP_IF_MASK, mask});

		nesting++;

		size_t thenBegin = ops.size();
		pickOP(2);
		size_t thenEnd = ops.size();

		deUint32 randElse = (deRandom_getUint32(&rnd) % 100);
		if (randElse < 50)
		{
			if (ifType == IF_LOCAL_INVOCATION_INDEX)
				ops.push_back({OP_ELSE_LOCAL_INVOCATION_INDEX, localIndexCmp});
			else if (ifType == IF_LOOPCOUNT)
				ops.push_back({OP_ELSE_LOOPCOUNT, 0});
			else
				ops.push_back({OP_ELSE_MASK, 0});

			if (randElse < 10)
			{
				// Sometimes make the else block identical to the then block
				for (size_t i = thenBegin; i < thenEnd; ++i)
					ops.push_back(ops[i]);
			}
			else
				pickOP(2);
		}
		ops.push_back({OP_ENDIF, 0});
		nesting--;
	}

	void genForUnif()
	{
		deUint32 iterCount = (deRandom_getUint32(&rnd) % 5) + 1;
		ops.push_back({OP_BEGIN_FOR_UNIF, iterCount});
		deUint32 loopheader = (deUint32)ops.size()-1;
		nesting++;
		loopNesting++;
		loopNestingThisFunction++;
		pickOP(2);
		ops.push_back({OP_END_FOR_UNIF, loopheader});
		loopNestingThisFunction--;
		loopNesting--;
		nesting--;
	}

	void genDoWhileUnif()
	{
		deUint32 iterCount = (deRandom_getUint32(&rnd) % 5) + 1;
		ops.push_back({OP_BEGIN_DO_WHILE_UNIF, iterCount});
		deUint32 loopheader = (deUint32)ops.size()-1;
		nesting++;
		loopNesting++;
		loopNestingThisFunction++;
		pickOP(2);
		ops.push_back({OP_END_DO_WHILE_UNIF, loopheader});
		loopNestingThisFunction--;
		loopNesting--;
		nesting--;
	}

	void genForVar()
	{
		ops.push_back({OP_BEGIN_FOR_VAR, 0});
		deUint32 loopheader = (deUint32)ops.size()-1;
		nesting++;
		loopNesting++;
		loopNestingThisFunction++;
		pickOP(2);
		ops.push_back({OP_END_FOR_VAR, loopheader});
		loopNestingThisFunction--;
		loopNesting--;
		nesting--;
	}

	void genForInf()
	{
		ops.push_back({OP_BEGIN_FOR_INF, 0});
		deUint32 loopheader = (deUint32)ops.size()-1;

		nesting++;
		loopNesting++;
		loopNestingThisFunction++;
		isLoopInf[loopNesting] = true;
		doneInfLoopBreak[loopNesting] = false;

		pickOP(2);

		genElect(true);
		doneInfLoopBreak[loopNesting] = true;

		pickOP(2);

		ops.push_back({OP_END_FOR_INF, loopheader});

		isLoopInf[loopNesting] = false;
		doneInfLoopBreak[loopNesting] = false;
		loopNestingThisFunction--;
		loopNesting--;
		nesting--;
	}

	void genDoWhileInf()
	{
		ops.push_back({OP_BEGIN_DO_WHILE_INF, 0});
		deUint32 loopheader = (deUint32)ops.size()-1;

		nesting++;
		loopNesting++;
		loopNestingThisFunction++;
		isLoopInf[loopNesting] = true;
		doneInfLoopBreak[loopNesting] = false;

		pickOP(2);

		genElect(true);
		doneInfLoopBreak[loopNesting] = true;

		pickOP(2);

		ops.push_back({OP_END_DO_WHILE_INF, loopheader});

		isLoopInf[loopNesting] = false;
		doneInfLoopBreak[loopNesting] = false;
		loopNestingThisFunction--;
		loopNesting--;
		nesting--;
	}

	void genBreak()
	{
		if (loopNestingThisFunction > 0)
		{
			// Sometimes put the break in a divergent if
			if ((deRandom_getUint32(&rnd) % 100) < 10)
			{
				ops.push_back({OP_IF_MASK, masks[0]});
				ops.push_back({OP_BREAK, 0});
				ops.push_back({OP_ELSE_MASK, 0});
				ops.push_back({OP_BREAK, 0});
				ops.push_back({OP_ENDIF, 0});
			}
			else
				ops.push_back({OP_BREAK, 0});
		}
	}

	void genContinue()
	{
		// continues are allowed if we're in a loop and the loop is not infinite,
		// or if it is infinite and we've already done a subgroupElect+break.
		// However, adding more continues seems to reduce the failure rate, so
		// disable it for now
		if (loopNestingThisFunction > 0 && !(isLoopInf[loopNesting] /*&& !doneInfLoopBreak[loopNesting]*/))
		{
			// Sometimes put the continue in a divergent if
			if ((deRandom_getUint32(&rnd) % 100) < 10)
			{
				ops.push_back({OP_IF_MASK, masks[0]});
				ops.push_back({OP_CONTINUE, 0});
				ops.push_back({OP_ELSE_MASK, 0});
				ops.push_back({OP_CONTINUE, 0});
				ops.push_back({OP_ENDIF, 0});
			}
			else
				ops.push_back({OP_CONTINUE, 0});
		}
	}

	// doBreak is used to generate "if (subgroupElect()) { ... break; }" inside infinite loops
	void genElect(bool doBreak)
	{
		ops.push_back({OP_ELECT, 0});
		nesting++;
		if (doBreak)
		{
			// Put something interestign before the break
			optBallot();
			optBallot();
			if ((deRandom_getUint32(&rnd) % 100) < 10)
				pickOP(1);

			// if we're in a function, sometimes  use return instead
			if (callNesting > 0 && (deRandom_getUint32(&rnd) % 100) < 30)
				ops.push_back({OP_RETURN, 0});
			else
				genBreak();

		}
		else
			pickOP(2);

		ops.push_back({OP_ENDIF, 0});
		nesting--;
	}

	void genReturn()
	{
		deUint32 r = deRandom_getUint32(&rnd) % 100;
		if (nesting > 0 &&
			// Use return rarely in main, 20% of the time in a singly nested loop in a function
			// and 50% of the time in a multiply nested loop in a function
			(r < 5 ||
			 (callNesting > 0 && loopNestingThisFunction > 0 && r < 20) ||
			 (callNesting > 0 && loopNestingThisFunction > 1 && r < 50)))
		{
			optBallot();
			if ((deRandom_getUint32(&rnd) % 100) < 10)
			{
				ops.push_back({OP_IF_MASK, masks[0]});
				ops.push_back({OP_RETURN, 0});
				ops.push_back({OP_ELSE_MASK, 0});
				ops.push_back({OP_RETURN, 0});
				ops.push_back({OP_ENDIF, 0});
			}
			else
				ops.push_back({OP_RETURN, 0});
		}
	}

	// Generate a function call. Save and restore some loop information, which is used to
	// determine when it's safe to use break/continue
	void genCall()
	{
		ops.push_back({OP_CALL_BEGIN, 0});
		callNesting++;
		nesting++;
		deInt32 saveLoopNestingThisFunction = loopNestingThisFunction;
		loopNestingThisFunction = 0;

		pickOP(2);

		loopNestingThisFunction = saveLoopNestingThisFunction;
		nesting--;
		callNesting--;
		ops.push_back({OP_CALL_END, 0});
	}

	// Generate switch on a uniform value:
	// switch (inputA.a[r]) {
	// case r+1: ... break; // should not execute
	// case r:   ... break; // should branch uniformly
	// case r+2: ... break; // should not execute
	// }
	void genSwitchUnif()
	{
		deUint32 r = deRandom_getUint32(&rnd) % 5;
		ops.push_back({OP_SWITCH_UNIF_BEGIN, r});
		nesting++;

		ops.push_back({OP_CASE_MASK_BEGIN, 0, 1u<<(r+1)});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, ~0ULL, 1u<<r});
		pickOP(2);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, 0, 1u<<(r+2)});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_SWITCH_END, 0});
		nesting--;
	}

	// switch (gl_SubgroupInvocationID & 3) with four unique targets
	void genSwitchVar()
	{
		ops.push_back({OP_SWITCH_VAR_BEGIN, 0});
		nesting++;

		ops.push_back({OP_CASE_MASK_BEGIN, 0x1111111111111111ULL, 1<<0});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, 0x2222222222222222ULL, 1<<1});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, 0x4444444444444444ULL, 1<<2});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, 0x8888888888888888ULL, 1<<3});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_SWITCH_END, 0});
		nesting--;
	}

	// switch (gl_SubgroupInvocationID & 3) with two shared targets.
	// XXX TODO: The test considers these two targets to remain converged,
	// though we haven't agreed to that behavior yet.
	void genSwitchMulticase()
	{
		ops.push_back({OP_SWITCH_VAR_BEGIN, 0});
		nesting++;

		ops.push_back({OP_CASE_MASK_BEGIN, 0x3333333333333333ULL, (1<<0)|(1<<1)});
		pickOP(2);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_MASK_BEGIN, 0xCCCCCCCCCCCCCCCCULL, (1<<2)|(1<<3)});
		pickOP(2);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_SWITCH_END, 0});
		nesting--;
	}

	// switch (loopIdxN) {
	// case 1:  ... break;
	// case 2:  ... break;
	// default: ... break;
	// }
	void genSwitchLoopCount()
	{
		deUint32 r = deRandom_getUint32(&rnd) % loopNesting;
		ops.push_back({OP_SWITCH_LOOP_COUNT_BEGIN, r});
		nesting++;

		ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, 1ULL<<1, 1});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, 1ULL<<2, 2});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		// default:
		ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, ~6ULL, 0xFFFFFFFF});
		pickOP(1);
		ops.push_back({OP_CASE_END, 0});

		ops.push_back({OP_SWITCH_END, 0});
		nesting--;
	}

	void pickOP(deUint32 count)
	{
		// Pick "count" instructions. These can recursively insert more instructions,
		// so "count" is just a seed
		for (deUint32 i = 0; i < count; ++i)
		{
			optBallot();
			if (nesting < maxNesting)
			{
				deUint32 r = deRandom_getUint32(&rnd) % 11;
				switch (r)
				{
				default:
					DE_ASSERT(0);
					// fallthrough
				case 2:
					if (loopNesting)
					{
						genIf(IF_LOOPCOUNT);
						break;
					}
					// fallthrough
				case 10:
					genIf(IF_LOCAL_INVOCATION_INDEX);
					break;
				case 0:
					genIf(IF_MASK);
					break;
				case 1:
					genIf(IF_UNIFORM);
					break;
				case 3:
					{
						// don't nest loops too deeply, to avoid extreme memory usage or timeouts
						if (loopNesting <= 3)
						{
							deUint32 r2 = deRandom_getUint32(&rnd) % 3;
							switch (r2)
							{
							default: DE_ASSERT(0); // fallthrough
							case 0: genForUnif(); break;
							case 1: genForInf(); break;
							case 2: genForVar(); break;
							}
						}
					}
					break;
				case 4:
					genBreak();
					break;
				case 5:
					genContinue();
					break;
				case 6:
					genElect(false);
					break;
				case 7:
					{
						deUint32 r2 = deRandom_getUint32(&rnd) % 5;
						if (r2 == 0 && callNesting == 0 && nesting < maxNesting - 2)
							genCall();
						else
							genReturn();
						break;
					}
				case 8:
					{
						// don't nest loops too deeply, to avoid extreme memory usage or timeouts
						if (loopNesting <= 3)
						{
							deUint32 r2 = deRandom_getUint32(&rnd) % 2;
							switch (r2)
							{
							default: DE_ASSERT(0); // fallthrough
							case 0: genDoWhileUnif(); break;
							case 1: genDoWhileInf(); break;
							}
						}
					}
					break;
				case 9:
					{
						deUint32 r2 = deRandom_getUint32(&rnd) % 4;
						switch (r2)
						{
						default:
							DE_ASSERT(0);
							// fallthrough
						case 0:
							genSwitchUnif();
							break;
						case 1:
							if (loopNesting > 0) {
								genSwitchLoopCount();
								break;
							}
							// fallthrough
						case 2:
							if (caseDef.testType != TT_MAXIMAL)
							{
								// multicase doesn't have fully-defined behavior for MAXIMAL tests,
								// but does for SUCF tests
								genSwitchMulticase();
								break;
							}
							// fallthrough
						case 3:
							genSwitchVar();
							break;
						}
					}
					break;
				}
			}
			optBallot();
		}
	}

	void optBallot()
	{
		// optionally insert ballots, stores, and noise. Ballots and stores are used to determine
		// correctness.
		if ((deRandom_getUint32(&rnd) % 100) < 20)
		{
			if (ops.size() < 2 ||
			   !(ops[ops.size()-1].type == OP_BALLOT ||
				 (ops[ops.size()-1].type == OP_STORE && ops[ops.size()-2].type == OP_BALLOT)))
			{
				// do a store along with each ballot, so we can correlate where
				// the ballot came from
				if (caseDef.testType != TT_MAXIMAL)
					ops.push_back({OP_STORE, (deUint32)ops.size() + storeBase});
				ops.push_back({OP_BALLOT, 0});
			}
		}

		if ((deRandom_getUint32(&rnd) % 100) < 10)
		{
			if (ops.size() < 2 ||
			   !(ops[ops.size()-1].type == OP_STORE ||
				 (ops[ops.size()-1].type == OP_BALLOT && ops[ops.size()-2].type == OP_STORE)))
			{
				// SUCF does a store with every ballot. Don't bloat the code by adding more.
				if (caseDef.testType == TT_MAXIMAL)
					ops.push_back({OP_STORE, (deUint32)ops.size() + storeBase});
			}
		}

		deUint32 r = deRandom_getUint32(&rnd) % 10000;
		if (r < 3)
			ops.push_back({OP_NOISE, 0});
		else if (r < 10)
			ops.push_back({OP_NOISE, 1});
	}

	void generateRandomProgram()
	{
		do {
			ops.clear();
			while ((deInt32)ops.size() < minCount)
				pickOP(1);

			// Retry until the program has some UCF results in it
			if (caseDef.isUCF())
			{
				const deUint32 invocationStride = 128;
				// Simulate for all subgroup sizes, to determine whether OP_BALLOTs are nonuniform
				for (deInt32 subgroupSize = 4; subgroupSize <= 64; subgroupSize *= 2) {
					simulate(true, subgroupSize, invocationStride, DE_NULL);
				}
			}
		} while (caseDef.isUCF() && !hasUCF());
	}

	void printIndent(std::stringstream &css)
	{
		for (deInt32 i = 0; i < indent; ++i)
			css << " ";
	}

	std::string genPartitionBallot()
	{
		std::stringstream ss;
		ss << "subgroupBallot(true).xy";
		return ss.str();
	}

	void printBallot(std::stringstream *css)
	{
		*css << "outputC.loc[gl_LocalInvocationIndex]++,";
		// When inside loop(s), use partitionBallot rather than subgroupBallot to compute
		// a ballot, to make sure the ballot is "diverged enough". Don't do this for
		// subgroup_uniform_control_flow, since we only validate results that must be fully
		// reconverged.
		if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
		{
			*css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = " << genPartitionBallot();
		}
		else if (caseDef.isElect())
		{
			*css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex].x = elect()";
		}
		else
		{
			*css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = subgroupBallot(true).xy";
		}
	}

	void genCode(std::stringstream &functions, std::stringstream &main)
	{
		std::stringstream *css = &main;
		indent = 4;
		loopNesting = 0;
		int funcNum = 0;
		for (deInt32 i = 0; i < (deInt32)ops.size(); ++i)
		{
			switch (ops[i].type)
			{
			case OP_IF_MASK:
				printIndent(*css);
				if (ops[i].value == ~0ULL)
				{
					// This equality test will always succeed, since inputA.a[i] == i
					int idx = deRandom_getUint32(&rnd) % 4;
					*css << "if (inputA.a[" << idx << "] == " << idx << ") {\n";
				}
				else
					*css << "if (testBit(uvec2(0x" << std::hex << (ops[i].value & 0xFFFFFFFF) << ", 0x" << (ops[i].value >> 32) << "), gl_SubgroupInvocationID)) {\n";

				indent += 4;
				break;
			case OP_IF_LOOPCOUNT:
				printIndent(*css); *css << "if (gl_SubgroupInvocationID == loopIdx" << loopNesting - 1 << ") {\n";
				indent += 4;
				break;
			case OP_IF_LOCAL_INVOCATION_INDEX:
				printIndent(*css); *css << "if (gl_LocalInvocationIndex >= inputA.a[0x" << std::hex << ops[i].value << "]) {\n";
				indent += 4;
				break;
			case OP_ELSE_MASK:
			case OP_ELSE_LOOPCOUNT:
			case OP_ELSE_LOCAL_INVOCATION_INDEX:
				indent -= 4;
				printIndent(*css); *css << "} else {\n";
				indent += 4;
				break;
			case OP_ENDIF:
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_BALLOT:
				printIndent(*css); printBallot(css); *css << ";\n";
				break;
			case OP_STORE:
				printIndent(*css); *css << "outputC.loc[gl_LocalInvocationIndex]++;\n";
				printIndent(*css); *css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex].x = 0x" << std::hex << ops[i].value << ";\n";
				break;
			case OP_BEGIN_FOR_UNIF:
				printIndent(*css); *css << "for (int loopIdx" << loopNesting << " = 0;\n";
				printIndent(*css); *css << "         loopIdx" << loopNesting << " < inputA.a[" << ops[i].value << "];\n";
				printIndent(*css); *css << "         loopIdx" << loopNesting << "++) {\n";
				indent += 4;
				loopNesting++;
				break;
			case OP_END_FOR_UNIF:
				loopNesting--;
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_BEGIN_DO_WHILE_UNIF:
				printIndent(*css); *css << "{\n";
				indent += 4;
				printIndent(*css); *css << "int loopIdx" << loopNesting << " = 0;\n";
				printIndent(*css); *css << "do {\n";
				indent += 4;
				printIndent(*css); *css << "loopIdx" << loopNesting << "++;\n";
				loopNesting++;
				break;
			case OP_BEGIN_DO_WHILE_INF:
				printIndent(*css); *css << "{\n";
				indent += 4;
				printIndent(*css); *css << "int loopIdx" << loopNesting << " = 0;\n";
				printIndent(*css); *css << "do {\n";
				indent += 4;
				loopNesting++;
				break;
			case OP_END_DO_WHILE_UNIF:
				loopNesting--;
				indent -= 4;
				printIndent(*css); *css << "} while (loopIdx" << loopNesting << " < inputA.a[" << ops[(deUint32)ops[i].value].value << "]);\n";
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_END_DO_WHILE_INF:
				loopNesting--;
				printIndent(*css); *css << "loopIdx" << loopNesting << "++;\n";
				indent -= 4;
				printIndent(*css); *css << "} while (true);\n";
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_BEGIN_FOR_VAR:
				printIndent(*css); *css << "for (int loopIdx" << loopNesting << " = 0;\n";
				printIndent(*css); *css << "         loopIdx" << loopNesting << " < gl_SubgroupInvocationID + 1;\n";
				printIndent(*css); *css << "         loopIdx" << loopNesting << "++) {\n";
				indent += 4;
				loopNesting++;
				break;
			case OP_END_FOR_VAR:
				loopNesting--;
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_BEGIN_FOR_INF:
				printIndent(*css); *css << "for (int loopIdx" << loopNesting << " = 0;;loopIdx" << loopNesting << "++,";
				loopNesting++;
				printBallot(css);
				*css << ") {\n";
				indent += 4;
				break;
			case OP_END_FOR_INF:
				loopNesting--;
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_BREAK:
				printIndent(*css); *css << "break;\n";
				break;
			case OP_CONTINUE:
				printIndent(*css); *css << "continue;\n";
				break;
			case OP_ELECT:
				printIndent(*css); *css << "if (subgroupElect()) {\n";
				indent += 4;
				break;
			case OP_RETURN:
				printIndent(*css); *css << "return;\n";
				break;
			case OP_CALL_BEGIN:
				printIndent(*css); *css << "func" << funcNum << "(";
				for (deInt32 n = 0; n < loopNesting; ++n)
				{
					*css << "loopIdx" << n;
					if (n != loopNesting - 1)
						*css << ", ";
				}
				*css << ");\n";
				css = &functions;
				printIndent(*css); *css << "void func" << funcNum << "(";
				for (deInt32 n = 0; n < loopNesting; ++n)
				{
					*css << "int loopIdx" << n;
					if (n != loopNesting - 1)
						*css << ", ";
				}
				*css << ") {\n";
				indent += 4;
				funcNum++;
				break;
			case OP_CALL_END:
				indent -= 4;
				printIndent(*css); *css << "}\n";
				css = &main;
				break;
			case OP_NOISE:
				if (ops[i].value == 0)
				{
					printIndent(*css); *css << "while (!subgroupElect()) {}\n";
				}
				else
				{
					printIndent(*css); *css << "if (inputA.a[0] == 12345) {\n";
					indent += 4;
					printIndent(*css); *css << "while (true) {\n";
					indent += 4;
					printIndent(*css); printBallot(css); *css << ";\n";
					indent -= 4;
					printIndent(*css); *css << "}\n";
					indent -= 4;
					printIndent(*css); *css << "}\n";
				}
				break;
			case OP_SWITCH_UNIF_BEGIN:
				printIndent(*css); *css << "switch (inputA.a[" << ops[i].value << "]) {\n";
				indent += 4;
				break;
			case OP_SWITCH_VAR_BEGIN:
				printIndent(*css); *css << "switch (gl_SubgroupInvocationID & 3) {\n";
				indent += 4;
				break;
			case OP_SWITCH_LOOP_COUNT_BEGIN:
				printIndent(*css); *css << "switch (loopIdx" << ops[i].value << ") {\n";
				indent += 4;
				break;
			case OP_SWITCH_END:
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			case OP_CASE_MASK_BEGIN:
				for (deInt32 b = 0; b < 32; ++b)
				{
					if ((1u<<b) & ops[i].caseValue)
					{
						printIndent(*css); *css << "case " << b << ":\n";
					}
				}
				printIndent(*css); *css << "{\n";
				indent += 4;
				break;
			case OP_CASE_LOOP_COUNT_BEGIN:
				if (ops[i].caseValue == 0xFFFFFFFF)
				{
					printIndent(*css); *css << "default: {\n";
				}
				else
				{
					printIndent(*css); *css << "case " << ops[i].caseValue << ": {\n";
				}
				indent += 4;
				break;
			case OP_CASE_END:
				printIndent(*css); *css << "break;\n";
				indent -= 4;
				printIndent(*css); *css << "}\n";
				break;
			default:
				DE_ASSERT(0);
				break;
			}
		}
	}

	// Simulate execution of the program. If countOnly is true, just return
	// the max number of outputs written. If it's false, store out the result
	// values to ref
	deUint32 simulate(bool countOnly, deUint32 subgroupSize, deUint32 invocationStride, deUint64 *ref)
	{
		// State of the subgroup at each level of nesting
		struct SubgroupState
		{
			// Currently executing
			bitset128 activeMask;
			// Have executed a continue instruction in this loop
			bitset128 continueMask;
			// index of the current if test or loop header
			deUint32 header;
			// number of loop iterations performed
			deUint32 tripCount;
			// is this nesting a loop?
			deUint32 isLoop;
			// is this nesting a function call?
			deUint32 isCall;
			// is this nesting a switch?
			deUint32 isSwitch;
		};
		SubgroupState stateStack[10];
		deMemset(&stateStack, 0, sizeof(stateStack));

		const deUint64 fullSubgroupMask = subgroupSizeToMask(subgroupSize);

		// Per-invocation output location counters
		deUint32 outLoc[128] = {0};

		nesting = 0;
		loopNesting = 0;
		stateStack[nesting].activeMask = ~bitset128(); // initialized to ~0

		deInt32 i = 0;
		while (i < (deInt32)ops.size())
		{
			switch (ops[i].type)
			{
			case OP_BALLOT:

				// Flag that this ballot is workgroup-nonuniform
				if (caseDef.isWUCF() && stateStack[nesting].activeMask.any() && !stateStack[nesting].activeMask.all())
					ops[i].caseValue = 1;

				if (caseDef.isSUCF())
				{
					for (deUint32 id = 0; id < 128; id += subgroupSize)
					{
						deUint64 subgroupMask = bitsetToU64(stateStack[nesting].activeMask, subgroupSize, id);
						// Flag that this ballot is subgroup-nonuniform
						if (subgroupMask != 0 && subgroupMask != fullSubgroupMask)
							ops[i].caseValue = 1;
					}
				}

				for (deUint32 id = 0; id < 128; ++id)
				{
					if (stateStack[nesting].activeMask.test(id))
					{
						if (countOnly)
						{
							outLoc[id]++;
						}
						else
						{
							if (ops[i].caseValue)
							{
								// Emit a magic value to indicate that we shouldn't validate this ballot
								ref[(outLoc[id]++)*invocationStride + id] = bitsetToU64(0x12345678, subgroupSize, id);
							}
							else
								ref[(outLoc[id]++)*invocationStride + id] = bitsetToU64(stateStack[nesting].activeMask, subgroupSize, id);
						}
					}
				}
				break;
			case OP_STORE:
				for (deUint32 id = 0; id < 128; ++id)
				{
					if (stateStack[nesting].activeMask.test(id))
					{
						if (countOnly)
							outLoc[id]++;
						else
							ref[(outLoc[id]++)*invocationStride + id] = ops[i].value;
					}
				}
				break;
			case OP_IF_MASK:
				nesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
				stateStack[nesting].header = i;
				stateStack[nesting].isLoop = 0;
				stateStack[nesting].isSwitch = 0;
				break;
			case OP_ELSE_MASK:
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & ~bitsetFromU64(ops[stateStack[nesting].header].value, subgroupSize);
				break;
			case OP_IF_LOOPCOUNT:
				{
					deUint32 n = nesting;
					while (!stateStack[n].isLoop)
						n--;

					nesting++;
					stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
					stateStack[nesting].header = i;
					stateStack[nesting].isLoop = 0;
					stateStack[nesting].isSwitch = 0;
					break;
				}
			case OP_ELSE_LOOPCOUNT:
				{
					deUint32 n = nesting;
					while (!stateStack[n].isLoop)
						n--;

					stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & ~bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
					break;
				}
			case OP_IF_LOCAL_INVOCATION_INDEX:
				{
					// all bits >= N
					bitset128 mask(0);
					for (deInt32 j = (deInt32)ops[i].value; j < 128; ++j)
						mask.set(j);

					nesting++;
					stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & mask;
					stateStack[nesting].header = i;
					stateStack[nesting].isLoop = 0;
					stateStack[nesting].isSwitch = 0;
					break;
				}
			case OP_ELSE_LOCAL_INVOCATION_INDEX:
				{
					// all bits < N
					bitset128 mask(0);
					for (deInt32 j = 0; j < (deInt32)ops[i].value; ++j)
						mask.set(j);

					stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & mask;
					break;
				}
			case OP_ENDIF:
				nesting--;
				break;
			case OP_BEGIN_FOR_UNIF:
				// XXX TODO: We don't handle a for loop with zero iterations
				nesting++;
				loopNesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].header = i;
				stateStack[nesting].tripCount = 0;
				stateStack[nesting].isLoop = 1;
				stateStack[nesting].isSwitch = 0;
				stateStack[nesting].continueMask = 0;
				break;
			case OP_END_FOR_UNIF:
				stateStack[nesting].tripCount++;
				stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
				stateStack[nesting].continueMask = 0;
				if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
					stateStack[nesting].activeMask.any())
				{
					i = stateStack[nesting].header+1;
					continue;
				}
				else
				{
					loopNesting--;
					nesting--;
				}
				break;
			case OP_BEGIN_DO_WHILE_UNIF:
				// XXX TODO: We don't handle a for loop with zero iterations
				nesting++;
				loopNesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].header = i;
				stateStack[nesting].tripCount = 1;
				stateStack[nesting].isLoop = 1;
				stateStack[nesting].isSwitch = 0;
				stateStack[nesting].continueMask = 0;
				break;
			case OP_END_DO_WHILE_UNIF:
				stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
				stateStack[nesting].continueMask = 0;
				if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
					stateStack[nesting].activeMask.any())
				{
					i = stateStack[nesting].header+1;
					stateStack[nesting].tripCount++;
					continue;
				}
				else
				{
					loopNesting--;
					nesting--;
				}
				break;
			case OP_BEGIN_FOR_VAR:
				// XXX TODO: We don't handle a for loop with zero iterations
				nesting++;
				loopNesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].header = i;
				stateStack[nesting].tripCount = 0;
				stateStack[nesting].isLoop = 1;
				stateStack[nesting].isSwitch = 0;
				stateStack[nesting].continueMask = 0;
				break;
			case OP_END_FOR_VAR:
				stateStack[nesting].tripCount++;
				stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
				stateStack[nesting].continueMask = 0;
				stateStack[nesting].activeMask &= bitsetFromU64(stateStack[nesting].tripCount == subgroupSize ? 0 : ~((1ULL << (stateStack[nesting].tripCount)) - 1), subgroupSize);
				if (stateStack[nesting].activeMask.any())
				{
					i = stateStack[nesting].header+1;
					continue;
				}
				else
				{
					loopNesting--;
					nesting--;
				}
				break;
			case OP_BEGIN_FOR_INF:
			case OP_BEGIN_DO_WHILE_INF:
				nesting++;
				loopNesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].header = i;
				stateStack[nesting].tripCount = 0;
				stateStack[nesting].isLoop = 1;
				stateStack[nesting].isSwitch = 0;
				stateStack[nesting].continueMask = 0;
				break;
			case OP_END_FOR_INF:
				stateStack[nesting].tripCount++;
				stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
				stateStack[nesting].continueMask = 0;
				if (stateStack[nesting].activeMask.any())
				{
					// output expected OP_BALLOT values
					for (deUint32 id = 0; id < 128; ++id)
					{
						if (stateStack[nesting].activeMask.test(id))
						{
							if (countOnly)
								outLoc[id]++;
							else
								ref[(outLoc[id]++)*invocationStride + id] = bitsetToU64(stateStack[nesting].activeMask, subgroupSize, id);
						}
					}

					i = stateStack[nesting].header+1;
					continue;
				}
				else
				{
					loopNesting--;
					nesting--;
				}
				break;
			case OP_END_DO_WHILE_INF:
				stateStack[nesting].tripCount++;
				stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
				stateStack[nesting].continueMask = 0;
				if (stateStack[nesting].activeMask.any())
				{
					i = stateStack[nesting].header+1;
					continue;
				}
				else
				{
					loopNesting--;
					nesting--;
				}
				break;
			case OP_BREAK:
				{
					deUint32 n = nesting;
					bitset128 mask = stateStack[nesting].activeMask;
					while (true)
					{
						stateStack[n].activeMask &= ~mask;
						if (stateStack[n].isLoop || stateStack[n].isSwitch)
							break;

						n--;
					}
				}
				break;
			case OP_CONTINUE:
				{
					deUint32 n = nesting;
					bitset128 mask = stateStack[nesting].activeMask;
					while (true)
					{
						stateStack[n].activeMask &= ~mask;
						if (stateStack[n].isLoop)
						{
							stateStack[n].continueMask |= mask;
							break;
						}
						n--;
					}
				}
				break;
			case OP_ELECT:
				{
					nesting++;
					stateStack[nesting].activeMask = bitsetElect(stateStack[nesting-1].activeMask, subgroupSize);
					stateStack[nesting].header = i;
					stateStack[nesting].isLoop = 0;
					stateStack[nesting].isSwitch = 0;
				}
				break;
			case OP_RETURN:
				{
					bitset128 mask = stateStack[nesting].activeMask;
					for (deInt32 n = nesting; n >= 0; --n)
					{
						stateStack[n].activeMask &= ~mask;
						if (stateStack[n].isCall)
							break;
					}
				}
				break;

			case OP_CALL_BEGIN:
				nesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].isLoop = 0;
				stateStack[nesting].isSwitch = 0;
				stateStack[nesting].isCall = 1;
				break;
			case OP_CALL_END:
				stateStack[nesting].isCall = 0;
				nesting--;
				break;
			case OP_NOISE:
				break;

			case OP_SWITCH_UNIF_BEGIN:
			case OP_SWITCH_VAR_BEGIN:
			case OP_SWITCH_LOOP_COUNT_BEGIN:
				nesting++;
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
				stateStack[nesting].header = i;
				stateStack[nesting].isLoop = 0;
				stateStack[nesting].isSwitch = 1;
				break;
			case OP_SWITCH_END:
				nesting--;
				break;
			case OP_CASE_MASK_BEGIN:
				stateStack[nesting].activeMask = stateStack[nesting-1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
				break;
			case OP_CASE_LOOP_COUNT_BEGIN:
				{
					deUint32 n = nesting;
					deUint32 l = loopNesting;

					while (true)
					{
						if (stateStack[n].isLoop)
						{
							l--;
							if (l == ops[stateStack[nesting].header].value)
								break;
						}
						n--;
					}

					if ((1ULL << stateStack[n].tripCount) & ops[i].value)
						stateStack[nesting].activeMask = stateStack[nesting-1].activeMask;
					else
						stateStack[nesting].activeMask = 0;
					break;
				}
			case OP_CASE_END:
				break;

			default:
				DE_ASSERT(0);
				break;
			}
			i++;
		}
		deUint32 maxLoc = 0;
		for (deUint32 id = 0; id < ARRAYSIZE(outLoc); ++id)
			maxLoc = de::max(maxLoc, outLoc[id]);

		return maxLoc;
	}

	bool hasUCF() const
	{
		for (deInt32 i = 0; i < (deInt32)ops.size(); ++i)
		{
			if (ops[i].type == OP_BALLOT && ops[i].caseValue == 0)
				return true;
		}
		return false;
	}
};

void ReconvergenceTestCase::initPrograms (SourceCollections& programCollection) const
{
	RandomProgram program(m_data);
	program.generateRandomProgram();

	std::stringstream css;
	css << "#version 450 core\n";
	css << "#extension GL_KHR_shader_subgroup_ballot : enable\n";
	css << "#extension GL_KHR_shader_subgroup_vote : enable\n";
	css << "#extension GL_NV_shader_subgroup_partitioned : enable\n";
	css << "#extension GL_EXT_subgroup_uniform_control_flow : enable\n";
	css << "layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;\n";
	css << "layout(set=0, binding=0) coherent buffer InputA { uint a[]; } inputA;\n";
	css << "layout(set=0, binding=1) coherent buffer OutputB { uvec2 b[]; } outputB;\n";
	css << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
	css << "layout(push_constant) uniform PC {\n"
			"   // set to the real stride when writing out ballots, or zero when just counting\n"
			"   int invocationStride;\n"
			"};\n";
	css << "int outLoc = 0;\n";

	css << "bool testBit(uvec2 mask, uint bit) { return (bit < 32) ? ((mask.x >> bit) & 1) != 0 : ((mask.y >> (bit-32)) & 1) != 0; }\n";

	css << "uint elect() { return int(subgroupElect()) + 1; }\n";

	std::stringstream functions, main;
	program.genCode(functions, main);

	css << functions.str() << "\n\n";

	css <<
		"void main()\n"
		<< (m_data.isSUCF() ? "[[subgroup_uniform_control_flow]]\n" : "") <<
		"{\n";

	css << main.str() << "\n\n";

	css << "}\n";

	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
}

TestInstance* ReconvergenceTestCase::createInstance (Context& context) const
{
	return new ReconvergenceTestInstance(context, m_data);
}

tcu::TestStatus ReconvergenceTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	tcu::TestLog&			log						= m_context.getTestContext().getLog();

	deRandom rnd;
	deRandom_init(&rnd, m_data.seed);

	vk::VkPhysicalDeviceSubgroupProperties subgroupProperties;
	deMemset(&subgroupProperties, 0, sizeof(subgroupProperties));
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

	vk::VkPhysicalDeviceProperties2 properties2;
	deMemset(&properties2, 0, sizeof(properties2));
	properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &subgroupProperties;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

	const deUint32 subgroupSize = subgroupProperties.subgroupSize;
	const deUint32 invocationStride = 128;

	if (subgroupSize > 64)
		TCU_THROW(TestError, "Subgroup size greater than 64 not handled.");

	RandomProgram program(m_data);
	program.generateRandomProgram();

	deUint32 maxLoc = program.simulate(true, subgroupSize, invocationStride, DE_NULL);

	// maxLoc is per-invocation. Add one (to make sure no additional writes are done) and multiply by
	// the number of invocations
	maxLoc++;
	maxLoc *= invocationStride;

	// buffer[0] is an input filled with a[i] == i
	// buffer[1] is the output
	// buffer[2] is the location counts
	de::MovePtr<BufferWithMemory> buffers[3];
	vk::VkDescriptorBufferInfo bufferDescriptors[3];

	VkDeviceSize sizes[3] =
	{
		128 * sizeof(deUint32),
		maxLoc * sizeof(deUint64),
		invocationStride * sizeof(deUint32),
	};

	for (deUint32 i = 0; i < 3; ++i)
	{
		if (sizes[i] > properties2.properties.limits.maxStorageBufferRange)
			TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

		try
		{
			buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
				vk, device, allocator, makeBufferCreateInfo(sizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
				MemoryRequirement::HostVisible | MemoryRequirement::Cached));
		}
		catch(const tcu::TestError&)
		{
			// Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
		}
		bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
	}

	deUint32 *ptrs[3];
	for (deUint32 i = 0; i < 3; ++i)
	{
		ptrs[i] = (deUint32 *)buffers[i]->getAllocation().getHostPtr();
	}
	for (deUint32 i = 0; i < sizes[0] / sizeof(deUint32); ++i)
	{
		ptrs[0][i] = i;
	}
	deMemset(ptrs[1], 0, (size_t)sizes[1]);
	deMemset(ptrs[2], 0, (size_t)sizes[2]);

	vk::DescriptorSetLayoutBuilder layoutBuilder;

	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));

	vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const deUint32 specData[1] =
	{
		invocationStride,
	};
	const vk::VkSpecializationMapEntry entries[1] =
	{
		{0, (deUint32)(sizeof(deUint32) * 0), sizeof(deUint32)},
	};
	const vk::VkSpecializationInfo specInfo =
	{
		1,						// mapEntryCount
		entries,				// pMapEntries
		sizeof(specData),		// dataSize
		specData				// pData
	};

	const VkPushConstantRange				pushConstantRange				=
	{
		allShaderStages,											// VkShaderStageFlags					stageFlags;
		0u,															// deUint32								offset;
		sizeof(deInt32)												// deUint32								size;
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		1,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		1u,															// pushConstantRangeCount
		&pushConstantRange,											// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

	flushAlloc(vk, device, buffers[0]->getAllocation());
	flushAlloc(vk, device, buffers[1]->getAllocation());
	flushAlloc(vk, device, buffers[2]->getAllocation());

	const VkBool32 computeFullSubgroups	=	(subgroupProperties.subgroupSize <= 64) &&
											(m_context.getSubgroupSizeControlFeatures().computeFullSubgroups) &&
											(m_context.getSubgroupSizeControlProperties().requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT);

	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupSizeCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,	// VkStructureType		sType;
		DE_NULL,																		// void*				pNext;
		subgroupProperties.subgroupSize													// uint32_t				requiredSubgroupSize;
	};

	const void *shaderPNext = computeFullSubgroups ? &subgroupSizeCreateInfo : DE_NULL;
	VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags =
		(VkPipelineShaderStageCreateFlags)(computeFullSubgroups ? VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT : 0);

	const Unique<VkShaderModule>			shader						(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));
	const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		shaderPNext,
		pipelineShaderStageCreateFlags,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*shader,													// shader
		"main",
		&specInfo,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		shaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};
	Move<VkPipeline> pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

	const VkQueue					queue					= m_context.getUniversalQueue();
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);


	vk::DescriptorSetUpdateBuilder setUpdateBuilder;
	setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
	setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
	setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);
	setUpdateBuilder.update(vk, device);

	// compute "maxLoc", the maximum number of locations written
	beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	deInt32 pcinvocationStride = 0;
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages, 0, sizeof(pcinvocationStride), &pcinvocationStride);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	invalidateAlloc(vk, device, buffers[1]->getAllocation());
	invalidateAlloc(vk, device, buffers[2]->getAllocation());

	// Clear any writes to buffer[1] during the counting pass
	deMemset(ptrs[1], 0, invocationStride * sizeof(deUint64));

	// Take the max over all invocations. Add one (to make sure no additional writes are done) and multiply by
	// the number of invocations
	deUint32 newMaxLoc = 0;
	for (deUint32 id = 0; id < invocationStride; ++id)
		newMaxLoc = de::max(newMaxLoc, ptrs[2][id]);
	newMaxLoc++;
	newMaxLoc *= invocationStride;

	// If we need more space, reallocate buffers[1]
	if (newMaxLoc > maxLoc)
	{
		maxLoc = newMaxLoc;
		sizes[1] = maxLoc * sizeof(deUint64);

		if (sizes[1] > properties2.properties.limits.maxStorageBufferRange)
			TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

		try
		{
			buffers[1] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
				vk, device, allocator, makeBufferCreateInfo(sizes[1], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
				MemoryRequirement::HostVisible | MemoryRequirement::Cached));
		}
		catch(const tcu::TestError&)
		{
			// Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Failed device memory allocation " + de::toString(sizes[1]) + " bytes");
		}
		bufferDescriptors[1] = makeDescriptorBufferInfo(**buffers[1], 0, sizes[1]);
		ptrs[1] = (deUint32 *)buffers[1]->getAllocation().getHostPtr();
		deMemset(ptrs[1], 0, (size_t)sizes[1]);

		vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
		setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
		setUpdateBuilder2.update(vk, device);
	}

	flushAlloc(vk, device, buffers[1]->getAllocation());

	// run the actual shader
	beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	pcinvocationStride = invocationStride;
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages, 0, sizeof(pcinvocationStride), &pcinvocationStride);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	invalidateAlloc(vk, device, buffers[1]->getAllocation());

	qpTestResult res = QP_TEST_RESULT_PASS;

	// Simulate execution on the CPU, and compare against the GPU result
	std::vector<deUint64> ref;
	try
	{
		ref.resize(maxLoc, 0ull);
	}
	catch (const std::bad_alloc&)
	{
		// Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Failed system memory allocation " + de::toString(maxLoc * sizeof(deUint64)) + " bytes");
	}

	program.simulate(false, subgroupSize, invocationStride, &ref[0]);

	const deUint64 *result = (const deUint64 *)ptrs[1];

	if (m_data.testType == TT_MAXIMAL)
	{
		// With maximal reconvergence, we should expect the output to exactly match
		// the reference.
		for (deUint32 i = 0; i < maxLoc; ++i)
		{
			if (result[i] != ref[i])
			{
				log << tcu::TestLog::Message << "first mismatch at " << i << tcu::TestLog::EndMessage;
				res = QP_TEST_RESULT_FAIL;
				break;
			}
		}

		if (res != QP_TEST_RESULT_PASS)
		{
			for (deUint32 i = 0; i < maxLoc; ++i)
			{
				// This log can be large and slow, ifdef it out by default
#if 0
				log << tcu::TestLog::Message << "result " << i << "(" << (i/invocationStride) << ", " << (i%invocationStride) << "): " << tcu::toHex(result[i]) << " ref " << tcu::toHex(ref[i]) << (result[i] != ref[i] ? " different" : "") << tcu::TestLog::EndMessage;
#endif
			}
		}
	}
	else
	{
		deUint64 fullMask = subgroupSizeToMask(subgroupSize);
		// For subgroup_uniform_control_flow, we expect any fully converged outputs in the reference
		// to have a corresponding fully converged output in the result. So walk through each lane's
		// results, and for each reference value of fullMask, find a corresponding result value of
		// fullMask where the previous value (OP_STORE) matches. That means these came from the same
		// source location.
		vector<deUint32> firstFail(invocationStride, 0);
		for (deUint32 lane = 0; lane < invocationStride; ++lane)
		{
			deUint32 resLoc = lane + invocationStride, refLoc = lane + invocationStride;
			while (refLoc < maxLoc)
			{
				while (refLoc < maxLoc && ref[refLoc] != fullMask)
					refLoc += invocationStride;
				if (refLoc >= maxLoc)
					break;

				// For TT_SUCF_ELECT, when the reference result has a full mask, we expect lane 0 to be elected
				// (a value of 2) and all other lanes to be not elected (a value of 1). For TT_SUCF_BALLOT, we
				// expect a full mask. Search until we find the expected result with a matching store value in
				// the previous result.
				deUint64 expectedResult = m_data.isElect() ? ((lane % subgroupSize) == 0 ? 2 : 1)
															 : fullMask;

				while (resLoc < maxLoc && !(result[resLoc] == expectedResult && result[resLoc-invocationStride] == ref[refLoc-invocationStride]))
					resLoc += invocationStride;

				// If we didn't find this output in the result, flag it as an error.
				if (resLoc >= maxLoc)
				{
					firstFail[lane] = refLoc;
					log << tcu::TestLog::Message << "lane " << lane << " first mismatch at " << firstFail[lane] << tcu::TestLog::EndMessage;
					res = QP_TEST_RESULT_FAIL;
					break;
				}
				refLoc += invocationStride;
				resLoc += invocationStride;
			}
		}

		if (res != QP_TEST_RESULT_PASS)
		{
			for (deUint32 i = 0; i < maxLoc; ++i)
			{
				// This log can be large and slow, ifdef it out by default
#if 0
				log << tcu::TestLog::Message << "result " << i << "(" << (i/invocationStride) << ", " << (i%invocationStride) << "): " << tcu::toHex(result[i]) << " ref " << tcu::toHex(ref[i]) << (i == firstFail[i%invocationStride] ? " first fail" : "") << tcu::TestLog::EndMessage;
#endif
			}
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx, const std::string& name, bool createExperimental)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, name.c_str(), "reconvergence tests"));

	typedef struct
	{
		deUint32				value;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	TestGroupCase ttCases[] =
	{
		{ TT_SUCF_ELECT,				"subgroup_uniform_control_flow_elect",	"subgroup_uniform_control_flow_elect"		},
		{ TT_SUCF_BALLOT,				"subgroup_uniform_control_flow_ballot",	"subgroup_uniform_control_flow_ballot"		},
		{ TT_WUCF_ELECT,				"workgroup_uniform_control_flow_elect",	"workgroup_uniform_control_flow_elect"		},
		{ TT_WUCF_BALLOT,				"workgroup_uniform_control_flow_ballot","workgroup_uniform_control_flow_ballot"		},
		{ TT_MAXIMAL,					"maximal",								"maximal"									},
	};

	for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
		de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(testCtx, "compute", ""));

		for (deUint32 nNdx = 2; nNdx <= 6; nNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> nestGroup(new tcu::TestCaseGroup(testCtx, ("nesting" + de::toString(nNdx)).c_str(), ""));

			deUint32 seed = 0;

			for (int sNdx = 0; sNdx < 8; sNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> seedGroup(new tcu::TestCaseGroup(testCtx, de::toString(sNdx).c_str(), ""));

				deUint32 numTests = 0;
				switch (nNdx)
				{
				default:
					DE_ASSERT(0);
					// fallthrough
				case 2:
				case 3:
				case 4:
					numTests = 250;
					break;
				case 5:
					numTests = 100;
					break;
				case 6:
					numTests = 50;
					break;
				}

				if (ttCases[ttNdx].value != TT_MAXIMAL)
				{
					if (nNdx >= 5)
						continue;
				}

				for (deUint32 ndx = 0; ndx < numTests; ndx++)
				{
					CaseDef c =
					{
						(TestType)ttCases[ttNdx].value,		// TestType testType;
						nNdx,								// deUint32 maxNesting;
						seed,								// deUint32 seed;
					};
					seed++;

					bool isExperimentalTest = !c.isUCF() || (ndx >= numTests / 5);

					if (createExperimental == isExperimentalTest)
						seedGroup->addChild(new ReconvergenceTestCase(testCtx, de::toString(ndx).c_str(), "", c));
				}
				if (!seedGroup->empty())
					nestGroup->addChild(seedGroup.release());
			}
			if (!nestGroup->empty())
				computeGroup->addChild(nestGroup.release());
		}
		if (!computeGroup->empty())
		{
			ttGroup->addChild(computeGroup.release());
			group->addChild(ttGroup.release());
		}
	}
	return group.release();
}

}	// anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTests(testCtx, name, false);
}

tcu::TestCaseGroup* createTestsExperimental (tcu::TestContext& testCtx, const std::string& name)
{
	return createTests(testCtx, name, true);
}

}	// Reconvergence
}	// vkt
