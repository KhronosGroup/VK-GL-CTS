/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Test hierarchy utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuTestHierarchyUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "qpXmlWriter.h"

#include <fstream>

namespace tcu
{

using std::string;

static const char* getNodeTypeName (TestNodeType nodeType)
{
	switch (nodeType)
	{
		case NODETYPE_SELF_VALIDATE:	return "SelfValidate";
		case NODETYPE_CAPABILITY:		return "Capability";
		case NODETYPE_ACCURACY:			return "Accuracy";
		case NODETYPE_PERFORMANCE:		return "Performance";
		case NODETYPE_GROUP:			return "TestGroup";
		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

// Utilities

static std::string makePackageFilename (const std::string& pattern, const std::string& packageName, const std::string& typeExtension)
{
	std::map<string, string> args;
	args["packageName"]		= packageName;
	args["typeExtension"]	= typeExtension;
	return StringTemplate(pattern).specialize(args);
}

void writeXmlCaselists (TestPackageRoot& root, TestContext& testCtx, const tcu::CommandLine& cmdLine)
{
	const  char* const			filenamePattern	= "${packageName}-cases.${typeExtension}";	// \todo [2015-02-27 pyry] Make this command line argument
	DefaultHierarchyInflater	inflater		(testCtx);
	TestHierarchyIterator		iter			(root, inflater, cmdLine);
	FILE*						curFile			= DE_NULL;
	qpXmlWriter*				writer			= DE_NULL;

	try
	{
		while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
		{
			const TestNode* const	node		= iter.getNode();
			const TestNodeType		nodeType	= node->getNodeType();
			const bool				isEnter		= iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE;

			DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE ||
					  iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE);

			if (nodeType == NODETYPE_PACKAGE)
			{
				if (isEnter)
				{
					const string	filename	= makePackageFilename(filenamePattern, node->getName(), "xml");
					qpXmlAttribute	attribs[2];
					int				numAttribs	= 0;

					DE_ASSERT(!curFile && !writer);

					print("Writing test cases from '%s' to file '%s'..\n", node->getName(), filename.c_str());

					curFile = fopen(filename.c_str(), "wb");
					if (!curFile)
						throw Exception("Failed to open " + filename);

					writer = qpXmlWriter_createFileWriter(curFile, DE_FALSE);
					if (!writer)
						throw Exception("Failed to create qpXmlWriter");

					attribs[numAttribs++] = qpSetStringAttrib("PackageName",	node->getName());
					attribs[numAttribs++] = qpSetStringAttrib("Description",	node->getDescription());
					DE_ASSERT(numAttribs <= DE_LENGTH_OF_ARRAY(attribs));

					if (!qpXmlWriter_startDocument(writer) ||
						!qpXmlWriter_startElement(writer, "TestCaseList", numAttribs, attribs))
						throw Exception("Failed to start XML document");
				}
				else
				{
					if (!qpXmlWriter_endElement(writer, "TestCaseList") ||
						!qpXmlWriter_endDocument(writer))
						throw Exception("Failed to terminate XML document");

					qpXmlWriter_destroy(writer);
					fclose(curFile);

					writer	= DE_NULL;
					curFile	= DE_NULL;
				}
			}
			else
			{
				if (isEnter)
				{
					const string	caseName	= node->getName();
					const string	description	= node->getDescription();
					qpXmlAttribute	attribs[3];
					int				numAttribs = 0;

					attribs[numAttribs++] = qpSetStringAttrib("Name",			caseName.c_str());
					attribs[numAttribs++] = qpSetStringAttrib("CaseType",		getNodeTypeName(nodeType));
					attribs[numAttribs++] = qpSetStringAttrib("Description",	description.c_str());
					DE_ASSERT(numAttribs <= DE_LENGTH_OF_ARRAY(attribs));

					if (!qpXmlWriter_startElement(writer, "TestCase", numAttribs, attribs))
						throw Exception("Writing to case list file failed");
				}
				else
				{
					if (!qpXmlWriter_endElement(writer, "TestCase"))
						throw tcu::Exception("Writing to case list file failed");
				}
			}

			iter.next();
		}
	}
	catch (...)
	{
		if (writer)
			qpXmlWriter_destroy(writer);

		if (curFile)
			fclose(curFile);

		throw;
	}

	DE_ASSERT(!curFile && !writer);
}

void writeTxtCaselists (TestPackageRoot& root, TestContext& testCtx, const tcu::CommandLine& cmdLine)
{
	const  char* const			filenamePattern	= "${packageName}-cases.${typeExtension}";	// \todo [2015-02-27 pyry] Make this command line argument
	DefaultHierarchyInflater	inflater		(testCtx);
	TestHierarchyIterator		iter			(root, inflater, cmdLine);

	while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
	{
		DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
				  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);

		const char*		pkgName		= iter.getNode()->getName();
		const string	filename	= makePackageFilename(filenamePattern, pkgName, "txt");
		std::ofstream	out			(filename.c_str(), std::ios_base::binary);

		if (!out.is_open() || !out.good())
			throw Exception("Failed to open " + filename);

		print("Writing test cases from '%s' to file '%s'..\n", pkgName, filename.c_str());

		iter.next();

		while (iter.getNode()->getNodeType() != NODETYPE_PACKAGE)
		{
			if (iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE)
				out << (isTestNodeTypeExecutable(iter.getNode()->getNodeType()) ? "TEST" : "GROUP") << ": " << iter.getNodePath() << "\n";
			iter.next();
		}

		DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
				  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
		iter.next();
	}
}

} // tcu
