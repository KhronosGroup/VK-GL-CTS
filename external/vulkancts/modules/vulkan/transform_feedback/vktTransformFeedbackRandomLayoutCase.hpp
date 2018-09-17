#ifndef _VKTTRANSFORMFEEDBACKRANDOMLAYOUTCASE_HPP
#define _VKTTRANSFORMFEEDBACKRANDOMLAYOUTCASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Transform Feedback Fuzz Random Layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackFuzzLayoutCase.hpp"

namespace de
{
class Random;
} // de

namespace vkt
{
namespace TransformFeedback
{

enum FeatureBits
{
	FEATURE_VECTORS					= (1<<0),
	FEATURE_MATRICES				= (1<<1),
	FEATURE_ARRAYS					= (1<<2),
	FEATURE_STRUCTS					= (1<<3),
	FEATURE_NESTED_STRUCTS			= (1<<4),
	FEATURE_INSTANCE_ARRAYS			= (1<<5),
	FEATURE_ARRAYS_OF_ARRAYS		= (1<<6),
	FEATURE_DOUBLES					= (1<<7),
	FEATURE_UNASSIGNED_FIELDS		= (1<<8),
	FEATURE_UNASSIGNED_BLOCK_MEMBERS= (1<<9),
	FEATURE_MISSING_BLOCK_MEMBERS	= (1<<10),	// Add holes into XFB buffer
	FEATURE_OUT_OF_ORDER_OFFSETS	= (1<<11),
};

class RandomInterfaceBlockCase : public InterfaceBlockCase
{
public:
								RandomInterfaceBlockCase	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const std::string&		description,
															 const TestStageFlags	testStageFlags,
															 deUint32				features,
															 deUint32				seed);

private:
	void						generateBlock				(de::Random& rnd, deUint32 layoutFlags);
	void						generateBlockMember			(de::Random& rnd, InterfaceBlock& block, const int numBlockMembers, int& numMissing);
	VarType						generateType				(de::Random& rnd, int typeDepth, bool arrayOk);
	std::vector<glu::DataType>	fillTypeCandidates			(void);

	const deUint32				m_features;
	const bool					m_explicitXfbOffsets;
	const int					m_maxBlocks;
	const int					m_maxInstances;
	const int					m_maxArrayLength;
	const int					m_maxStructDepth;
	const int					m_maxBlockMembers;
	const int					m_maxStructMembers;
	const deUint32				m_seed;

	int							m_blockNdx;
	int							m_interfaceNdx;
	int							m_structNdx;
	std::vector<glu::DataType>	m_primitiveTypeCandidates;
};

} // TransformFeedback
} // vkt

#endif // _VKTTRANSFORMFEEDBACKRANDOMLAYOUTCASE_HPP
