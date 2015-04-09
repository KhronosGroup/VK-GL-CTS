/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Cross-thread barrier.
 *//*--------------------------------------------------------------------*/

#include "deSpinBarrier.hpp"
#include "deThread.hpp"
#include "deRandom.hpp"
#include "deInt32.h"

#include <vector>

namespace de
{

SpinBarrier::SpinBarrier (deInt32 numThreads)
	: m_numThreads	(numThreads)
	, m_numEntered	(0)
	, m_numLeaving	(0)
{
	DE_ASSERT(numThreads > 0);
}

SpinBarrier::~SpinBarrier (void)
{
	DE_ASSERT(m_numEntered == 0 && m_numLeaving == 0);
}

void SpinBarrier::sync (WaitMode mode)
{
	DE_ASSERT(mode == WAIT_MODE_YIELD || mode == WAIT_MODE_BUSY);

	deMemoryReadWriteFence();

	if (m_numLeaving > 0)
	{
		for (;;)
		{
			if (m_numLeaving == 0)
				break;

			if (mode == WAIT_MODE_YIELD)
				deYield();
		}
	}

	if (deAtomicIncrement32(&m_numEntered) == m_numThreads)
	{
		m_numLeaving = m_numThreads;
		deMemoryReadWriteFence();
		m_numEntered = 0;
	}
	else
	{
		for (;;)
		{
			if (m_numEntered == 0)
				break;

			if (mode == WAIT_MODE_YIELD)
				deYield();
		}
	}

	deAtomicDecrement32(&m_numLeaving);
	deMemoryReadWriteFence();
}

namespace
{

void singleThreadTest (SpinBarrier::WaitMode mode)
{
	SpinBarrier barrier(1);

	barrier.sync(mode);
	barrier.sync(mode);
	barrier.sync(mode);
}

class TestThread : public de::Thread
{
public:
	TestThread (SpinBarrier& barrier, volatile deInt32* sharedVar, int numThreads, int threadNdx, bool busyOk)
		: m_barrier		(barrier)
		, m_sharedVar	(sharedVar)
		, m_numThreads	(numThreads)
		, m_threadNdx	(threadNdx)
		, m_busyOk		(busyOk)
	{
	}

	void run (void)
	{
		const int	numIters	= 10000;
		de::Random	rnd			(deInt32Hash(m_numThreads) ^ deInt32Hash(m_threadNdx));

		for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
		{
			// Phase 1: count up
			deAtomicIncrement32(m_sharedVar);

			// Verify
			m_barrier.sync(getWaitMode(rnd));

			DE_TEST_ASSERT(*m_sharedVar == m_numThreads);

			m_barrier.sync(getWaitMode(rnd));

			// Phase 2: count down
			deAtomicDecrement32(m_sharedVar);

			// Verify
			m_barrier.sync(getWaitMode(rnd));

			DE_TEST_ASSERT(*m_sharedVar == 0);

			m_barrier.sync(getWaitMode(rnd));
		}
	}

private:
	SpinBarrier&		m_barrier;
	volatile deInt32*	m_sharedVar;
	int					m_numThreads;
	int					m_threadNdx;
	bool				m_busyOk;

	SpinBarrier::WaitMode getWaitMode (de::Random& rnd)
	{
		if (m_busyOk && rnd.getBool())
			return SpinBarrier::WAIT_MODE_BUSY;
		else
			return SpinBarrier::WAIT_MODE_YIELD;
	}
};

void multiThreadTest (int numThreads)
{
	SpinBarrier					barrier		(numThreads);
	volatile deInt32			sharedVar	= 0;
	std::vector<TestThread*>	threads		(numThreads, static_cast<TestThread*>(DE_NULL));

	// Going over logical cores with busy-waiting will cause priority inversion and make tests take
	// excessive amount of time. Use busy waiting only when number of threads is at most one per
	// core.
	const bool					busyOk		= (deUint32)numThreads <= deGetNumAvailableLogicalCores();

	for (int ndx = 0; ndx < numThreads; ndx++)
	{
		threads[ndx] = new TestThread(barrier, &sharedVar, numThreads, ndx, busyOk);
		DE_TEST_ASSERT(threads[ndx]);
		threads[ndx]->start();
	}

	for (int ndx = 0; ndx < numThreads; ndx++)
	{
		threads[ndx]->join();
		delete threads[ndx];
	}

	DE_TEST_ASSERT(sharedVar == 0);
}

} // namespace

void SpinBarrier_selfTest (void)
{
	singleThreadTest(SpinBarrier::WAIT_MODE_YIELD);
	singleThreadTest(SpinBarrier::WAIT_MODE_BUSY);
	multiThreadTest(1);
	multiThreadTest(2);
	multiThreadTest(4);
	multiThreadTest(8);
	multiThreadTest(16);
}

} // de
