/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuResource.hpp"


namespace vkt
{
namespace cts_amber
{

class AmberIndexFileParser
{
	std::string			m_str;
	size_t				m_idx;
	size_t				m_len;
	static const int	m_fieldLen = 256;
	char				m_scratch[m_fieldLen];
	char				m_filenameField[m_fieldLen];
	char				m_testnameField[m_fieldLen];
	char				m_descField[m_fieldLen];

	bool isWhitespace (char c)
	{
		if (c == ' '  ||
			c == '\t' ||
			c == '\r' ||
			c == '\n')
		{
			return true;
		}
		return false;
	}

	void skipWhitespace (void)
	{
		while (m_idx < m_len && isWhitespace(m_str[m_idx]))
			m_idx++;
	}

	bool skipCommentLine (void)
	{
		skipWhitespace();
		if (m_str[m_idx] == '#')
		{
			while (m_idx < m_len && m_str[m_idx] != '\n')
				m_idx++;
			return true;
		}
		return false;
	}

	void accept (char c)
	{
		if (m_str[m_idx] == c)
			m_idx++;
	}

	void expect (char c)
	{
		if (m_str[m_idx] != c || m_idx >= m_len)
			TCU_THROW(ResourceError, "Error parsing amber index file");

		m_idx++;
	}

	void captureString (char* field)
	{
		int i = 0;

		while (m_idx < m_len && i < m_fieldLen && m_str[m_idx] != '"')
		{
			field[i] = m_str[m_idx];
			i++;
			m_idx++;
		}

		field[i] = 0;
		m_idx++;
	}


public:
	AmberIndexFileParser (tcu::TestContext& testCtx, const char* filename, const char* category)
	{
		std::string	indexFilename("vulkan/amber/");
		indexFilename.append(category);
		indexFilename.append("/");
		indexFilename.append(filename);

		m_str = ShaderSourceProvider::getSource(testCtx.getArchive(), indexFilename.c_str());
		m_len = m_str.length();
		m_idx = 0;
	}

	~AmberIndexFileParser (void) { }

	AmberTestCase* parse (const char* category, tcu::TestContext& testCtx)
	{
		// Format:
		// {"filename","test name","description"[,requirement[,requirement[,requirement..]]]}[,]
		// Things inside [] are optional. Whitespace is allowed everywhere.
		//
		// Comments are allowed starting with "#" character.
		//
		// For example, test without requirements might be:
		// {"testname.amber","test name","test description"},

		while (skipCommentLine());

		if (m_idx < m_len)
		{
			skipWhitespace();
			expect('{');
			skipWhitespace();
			expect('"');
			captureString(m_filenameField);
			skipWhitespace();
			expect(',');
			skipWhitespace();
			expect('"');
			captureString(m_testnameField);
			skipWhitespace();
			expect(',');
			skipWhitespace();
			expect('"');
			captureString(m_descField);
			skipWhitespace();

			std::string testFilename("vulkan/amber/");
			testFilename.append(category);
			testFilename.append("/");
			testFilename.append(m_filenameField);
			AmberTestCase *testCase = new AmberTestCase(testCtx, m_testnameField, m_descField, testFilename);

			while (m_idx < m_len && m_str[m_idx] == ',')
			{
				accept(',');
				skipWhitespace();
				expect('"');
				captureString(m_scratch);
				skipWhitespace();
				testCase->addRequirement(m_scratch);
			}

			expect('}');
			skipWhitespace();
			accept(',');
			skipWhitespace();
			return testCase;
		}
		return 0;
	}
};

void createAmberTestsFromIndexFile (tcu::TestContext& testCtx, tcu::TestCaseGroup* group, const std::string filename, const char* category)
{
	AmberTestCase*			testCase = 0;
	AmberIndexFileParser	parser(testCtx, filename.c_str(), category);

	do
	{
		testCase = parser.parse(category, testCtx);
		if (testCase)
		{
			group->addChild(testCase);
		}
	} while (testCase);
}

AmberTestCase* createAmberTestCase (tcu::TestContext&							testCtx,
									const char*									name,
									const char*									description,
									const char*									category,
									const std::string&							filename,
									const std::vector<std::string>				requirements,
									const std::vector<vk::VkImageCreateInfo>	imageRequirements,
									const std::vector<BufferRequirement>		bufferRequirements)

{
	// shader_test files are saved in <path>/external/vulkancts/data/vulkan/amber/<categoryname>/
	std::string readFilename("vulkan/amber/");
	readFilename.append(category);
	readFilename.append("/");
	readFilename.append(filename);

	AmberTestCase *testCase = new AmberTestCase(testCtx, name, description, readFilename);

	for (auto req : requirements)
		testCase->addRequirement(req);

	for (auto req : imageRequirements)
		testCase->addImageRequirement(req);

	for (auto req : bufferRequirements)
		testCase->addBufferRequirement(req);

	return testCase;
}

} // cts_amber
} // vkt
