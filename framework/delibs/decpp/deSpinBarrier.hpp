#ifndef _DESPINBARRIER_HPP
#define _DESPINBARRIER_HPP
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

#include "deDefs.hpp"
#include "deAtomic.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Cross-thread barrier
 *
 * SpinBarrier provides barrier implementation that uses spin loop for
 * waiting for other threads. Threads may choose to wait in tight loop
 * (WAIT_MODE_BUSY) or yield between iterations (WAIT_MODE_YIELD).
 *//*--------------------------------------------------------------------*/
class SpinBarrier
{
public:
	enum WaitMode
	{
		WAIT_MODE_BUSY = 0,
		WAIT_MODE_YIELD,

		WAIT_MODE_LAST
	};

						SpinBarrier		(deInt32 numThreads);
						~SpinBarrier	(void);

	void				sync			(WaitMode mode);

private:
						SpinBarrier		(const SpinBarrier&);
	SpinBarrier			operator=		(const SpinBarrier&);

	const deInt32		m_numThreads;
	volatile deInt32	m_numEntered;
	volatile deInt32	m_numLeaving;
};

void	SpinBarrier_selfTest	(void);

} // de

#endif // _DESPINBARRIER_HPP
