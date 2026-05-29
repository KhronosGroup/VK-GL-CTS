/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #ifndef _APP_MAIN_H_
 #define _APP_MAIN_H_

typedef struct RunStatus
{
	int		numExecuted;		//!< Total number of cases executed.
	int		numPassed;			//!< Number of cases passed.
	int		numFailed;			//!< Number of cases failed.
	int		numNotSupported;	//!< Number of cases not supported.
	int		numWarnings;		//!< Number of QualityWarning / CompatibilityWarning results.
	int		numWaived;			//!< Number of waived tests.
	bool	isComplete;			//!< Is run complete.
} TestRunStatus_t;

 __attribute__((visibility("default"))) TestRunStatus_t runTest(int argc, char **argv);

 #endif