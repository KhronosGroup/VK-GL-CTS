#ifndef _VKTBUFFEREDREADER_HPP
#define _VKTBUFFEREDREADER_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Igalia S.L
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
 */
#include <fstream>
#include <vector>

#include "tcuDefs.hpp"

namespace vkt
{
namespace video
{

class BufferedReader
{
public:
	BufferedReader(std::istream& in)
		: m_istream(in)
	{
		if (!m_istream.good())
		{
			throw tcu::ResourceError(std::string("failed to open input"));
		}
	}

	void read(std::vector<uint8_t>& buffer)
	{
		m_istream.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
	}

	void read(uint8_t* out, size_t n)
	{
		m_istream.read(reinterpret_cast<char *>(out), n);
	}

	void readChecked(uint8_t* out, size_t n, const char* msg)
	{
		read(out, n);
		if (isError())
			TCU_THROW(InternalError, msg);
	}

	uint8_t readByteChecked(const char* msg)
	{
		uint8_t v{};
		read(&v, 1);
		if (isEof())
			return 0;
		if (isError())
			TCU_THROW(InternalError, msg);
		return v;
	}

	bool isError() const { return m_istream.bad() || m_istream.fail(); }
	bool isEof() const { return m_istream.eof(); }

private:
	std::istream&					m_istream;
};

} // namespace video
} // namespace vkt

#endif // _VKTBUFFEREDREADER_HPP
