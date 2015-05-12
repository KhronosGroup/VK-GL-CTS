/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
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
 * \brief
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestHierarchyIterator.hpp"
#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vktTestPackage.hpp"

#include <iostream>

namespace vkt
{

using std::vector;
using std::string;
using de::UniquePtr;

tcu::TestPackageRoot* createRoot (tcu::TestContext& testCtx)
{
	vector<tcu::TestNode*>	children;
	children.push_back(new TestPackage(testCtx));
	return new tcu::TestPackageRoot(testCtx, children);
}

void buildPrograms (tcu::TestContext& testCtx)
{
	const UniquePtr<tcu::TestPackageRoot>	root		(createRoot(testCtx));
	tcu::DefaultHierarchyInflater			inflater	(testCtx);
	tcu::TestHierarchyIterator				iterator	(*root, inflater, testCtx.getCommandLine());

	while (iterator.getState() != tcu::TestHierarchyIterator::STATE_FINISHED)
	{
		if (iterator.getState() == tcu::TestHierarchyIterator::STATE_ENTER_NODE &&
			tcu::isTestNodeTypeExecutable(iterator.getNode()->getNodeType()))
		{
			const TestCase* const		testCase	= dynamic_cast<TestCase*>(iterator.getNode());
			const string				path		= iterator.getNodePath();
			vk::SourceCollection		progs;

			tcu::print("%s\n", path.c_str());

			testCase->initPrograms(progs);

			for (vk::SourceCollection::Iterator progIter = progs.begin(); progIter != progs.end(); ++progIter)
			{
				tcu::print("    %s\n", progIter.getName().c_str());

				// \todo [2015-03-20 pyry] This is POC level, next steps:
				//  - actually build programs
				//  - eliminate duplicates
				//  - store as binaries + name -> prog file map
			}
		}

		iterator.next();
	}
}

} // vkt

int main (int argc, const char* argv[])
{
	try
	{
		const tcu::CommandLine	cmdLine		(argc, argv);
		tcu::DirArchive			archive		(".");
		tcu::TestLog			log			(cmdLine.getLogFileName(), cmdLine.getLogFlags());
		tcu::Platform			platform;
		tcu::TestContext		testCtx		(platform, archive, log, cmdLine, DE_NULL);

		vkt::buildPrograms(testCtx);
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}

	return 0;
}
