/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Ray Tracing Pipeline Flags tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingPipelineFlagsTests.hpp"

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuStringTemplate.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <tuple>

#ifdef INTERNAL_DEBUG
#include <iostream>
#endif

#define ALL_RAY_TRACING_STAGES	(VK_SHADER_STAGE_RAYGEN_BIT_KHR			\
								| VK_SHADER_STAGE_ANY_HIT_BIT_KHR		\
								| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	\
								| VK_SHADER_STAGE_MISS_BIT_KHR			\
								| VK_SHADER_STAGE_INTERSECTION_BIT_KHR	\
								| VK_SHADER_STAGE_CALLABLE_BIT_KHR)
namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace vkt;

#define ALIGN_STD430(type_) alignas(sizeof(type_)) type_

#define INRANGE(x_, a_, b_) (((x_) >= (a_) && (x_) <= (b_)) || ((x_) >= (b_) && (x_) <= (a_)))

enum class GeometryTypes : deUint32
{
	None			= 0x0,
	Triangle		= 0x1,
	Box				= 0x2,
	TriangleAndBox	= Triangle | Box
};

struct TestParams
{
	deUint32				width;
	deUint32				height;
	VkBool32				onHhost;
	VkPipelineCreateFlags	flags;
	bool					useLibs;
	bool					useMaintenance5;
	deUint32				instCount;
	GeometryTypes			geomTypes;
	deUint32				geomCount;
	deUint32				stbRecStride;
	deUint32				stbRecOffset;
	float					accuracy;
#define ENABLED(val_, mask_) (((val_)&(mask_))==(mask_))
	bool miss()		const {	return ENABLED(flags, VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR);	}
	bool ahit()		const { return ENABLED(flags, VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR); }
	bool chit()		const { return ENABLED(flags, VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR); }
	bool isect()	const { return ENABLED(flags, VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR); }
};

tcu::Vec3 rotateCcwZ (const tcu::Vec3& p, const tcu::Vec3& center, const float& radians)
{
	const float s = std::sin(radians);
	const float c = std::cos(radians);
	const auto  t = p - center;
	return tcu::Vec3(c * t.x() - s * t.y(), s * t.x() + c * t.y(), t.z()) + center;
}

bool pointInRect2D(const tcu::Vec3& p, const tcu::Vec3& p0, const tcu::Vec3& p1)
{
	return INRANGE(p.x(), p0.x(), p1.x()) && INRANGE(p.y(), p0.y(), p1.y());
}

deUint32 computeEffectiveShaderGroupCount(const TestParams& p)
{
	DE_ASSERT(p.instCount && p.geomCount);
	return (p.geomCount * p.stbRecStride + p.stbRecOffset + 1);
}

class RayTracingTestPipeline;

class PipelineFlagsCase : public TestCase
{
public:
							PipelineFlagsCase		(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const TestParams&	testParams);
	virtual					~PipelineFlagsCase		(void) = default;
	virtual void			initPrograms			(SourceCollections&	programCollection) const override;
	virtual TestInstance*	createInstance			(Context&			context) const override;
	virtual void			checkSupport			(Context&			context) const override;

	const deUint32&			shaderGroupHandleSize;
	const deUint32&			shaderGroupBaseAlignment;

private:
	const TestParams		m_params;

	const tcu::IVec4		m_rgenPayload;
	const deInt32			m_defMissRetGreenComp;
	const deInt32			m_defTriRetGreenComp;
	const deInt32			m_defBoxRetGreenComp;
	static	deInt32			calcDefBoxRetGreenComp	(const TestParams&	params,
													 deInt32			defTriRetGreenComp);

	static deUint32			m_shaderGroupHandleSize;
	static deUint32			m_shaderGroupBaseAlignment;
};

deInt32 PipelineFlagsCase::calcDefBoxRetGreenComp (const TestParams& params, deInt32 defTriRetGreenComp)
{
	const deUint32	nameCount		= params.stbRecStride ? (params.geomCount * params.instCount) : params.instCount;
	const deUint32	triangleCount	= (params.geomTypes == GeometryTypes::Triangle || params.geomTypes == GeometryTypes::TriangleAndBox) ? nameCount : 0u;
	return defTriRetGreenComp + std::max(triangleCount, 32u);
}
deUint32 PipelineFlagsCase::m_shaderGroupHandleSize;
deUint32 PipelineFlagsCase::m_shaderGroupBaseAlignment;

class PipelineFlagsInstance : public TestInstance
{
	using TopLevelASPtr		= de::SharedPtr<TopLevelAccelerationStructure>;
	using BottomLevelASPtr	= de::SharedPtr<BottomLevelAccelerationStructure>;
	using BottomLevelASPtrs	= std::vector<BottomLevelASPtr>;
	using TriGeometry		= std::array<tcu::Vec3, 3>;
	using BoxGeometry		= std::array<tcu::Vec3, 2>;

	friend class RayTracingTestPipeline;
public:
									PipelineFlagsInstance					(Context&					context,
																			 const TestParams&			params,
																			 const deUint32&			shaderGroupHandleSize_,
																			 const deUint32&			shaderGroupBaseAlignment_,
																			 const tcu::IVec4&			rgenPayload_,
																			 deInt32					defMissRetGreenComp_,
																			 deInt32					defTriRetGreenComp_,
																			 deInt32					defBoxRetGreenComp_);
	virtual							~PipelineFlagsInstance					(void) =  default;

	struct ShaderRecordEXT
	{
		ALIGN_STD430(GeometryTypes)	geomType;
		ALIGN_STD430(deUint32)		geomIndex;
		ALIGN_STD430(tcu::IVec4)	retValue;

		ShaderRecordEXT	();
		ShaderRecordEXT	(GeometryTypes type, deUint32 index, const tcu::IVec4& ret);
	};

	virtual tcu::TestStatus			iterate									(void) override;

	const tcu::IVec4				rgenPayload;
	const deInt32					defMissRetGreenComp;
	const deInt32					defTriRetGreenComp;
	const deInt32					defBoxRetGreenComp;

	const deUint32					shaderGroupHandleSize;
	const deUint32					shaderGroupBaseAlignment;

private:
	struct HitGroup;
	using ShaderRecordEntry	= std::tuple<VkShaderStageFlags, HitGroup, ShaderRecordEXT, bool /* initalized */>;

	VkImageCreateInfo				makeImageCreateInfo						() const;
	std::vector<TriGeometry>		prepareTriGeometries					(const float						zCoord)			const;
	std::vector<BoxGeometry>		prepareBoxGeometries					(const float						zFront,
																			 const float						zBack)			const;
	std::vector<ShaderRecordEntry>	prepareShaderBindingTable				(void)												const;
	BottomLevelASPtrs				createBottomLevelAccelerationStructs	(VkCommandBuffer					cmdBuffer)		const;
	TopLevelASPtr					createTopLevelAccelerationStruct		(VkCommandBuffer					cmdBuffer,
																			 const BottomLevelASPtrs&			blasPtrs)		const;
	bool							verifyResult							(const BufferWithMemory*			resultBuffer)	const;
#ifdef INTERNAL_DEBUG
	void							printImage								(const tcu::IVec4*					image)			const;
#endif

	template<class rayPayloadEXT, class shaderRecordEXT>
	struct Shader
	{
		virtual bool			ignoreIntersection	(const rayPayloadEXT&, const shaderRecordEXT&) const { return false; }
		virtual	rayPayloadEXT	invoke				(const rayPayloadEXT&, const shaderRecordEXT&) const = 0;
	};
	struct ShaderBase : Shader<tcu::IVec4, ShaderRecordEXT>
	{
		typedef tcu::IVec4			rayPayloadEXT;
		typedef ShaderRecordEXT		shaderRecordEXT;
		static const rayPayloadEXT	dummyPayload;
		virtual	rayPayloadEXT	invoke				(const rayPayloadEXT&, const shaderRecordEXT&) const override {
			return rayPayloadEXT();
		}
	};
	struct ClosestHitShader : ShaderBase
	{
		virtual	rayPayloadEXT	invoke				(const rayPayloadEXT& hitAttr, const shaderRecordEXT& rec) const override {
			return (rec.geomType == GeometryTypes::Triangle) ? rec.retValue : hitAttr;
		}
	};
	struct AnyHitShader : ShaderBase
	{
		virtual bool			ignoreIntersection(const rayPayloadEXT&, const shaderRecordEXT& rec) const override {
			return (rec.geomIndex % 2 == 1);
		}
	};
	struct IntersectionShader : ShaderBase
	{
		virtual	rayPayloadEXT	invoke(const rayPayloadEXT&, const shaderRecordEXT& rec) const override {
			return (rec.retValue + tcu::IVec4(0, 2, 3, 4));
		}
	};
	struct MissShader : ShaderBase
	{
		virtual	rayPayloadEXT	invoke(const rayPayloadEXT&, const shaderRecordEXT& rec) const override {
			return rec.retValue; }
	};
	struct HitGroup
	{
		de::SharedPtr<AnyHitShader>			ahit;
		de::SharedPtr<ClosestHitShader>		chit;
		de::SharedPtr<IntersectionShader>	isect;
	};

	tcu::IVec2					rayToImage								(const tcu::Vec2&								rayCoords) const;
	tcu::Vec2					imageToRay								(const tcu::IVec2&								imageCoords) const;
	deUint32					computeSamePixelCount					(const std::vector<tcu::IVec4>&					image,
																		 const tcu::Vec2								pixelCoords,
																		 const tcu::IVec4&								requiredColor,
																		 const tcu::IVec4&								floodColor,
																		 const std::function<bool(const tcu::Vec3&)>&	pointInGeometry,
																		 std::vector<std::pair<tcu::IVec4, tcu::IVec2>>	&auxBuffer) const;
	void						travelRay								(std::vector<tcu::IVec4>&						outImage,
																		 const deUint32									glLaunchIdExtX,
																		 const deUint32									glLaunchIdExtY,
																		 const std::vector<ShaderRecordEntry>&			shaderBindingTable,
																		 const MissShader&								missShader,
																		 const std::vector<TriGeometry>&				triangleGeometries,
																		 const std::vector<BoxGeometry>&				boxGeometries) const;
	const TestParams			m_params;
	const VkFormat				m_format;
};
PipelineFlagsInstance::ShaderBase::rayPayloadEXT const PipelineFlagsInstance::ShaderBase::dummyPayload{};

PipelineFlagsInstance::ShaderRecordEXT::ShaderRecordEXT ()
	: geomType	(GeometryTypes::None)
	, geomIndex	(~0u)
	, retValue	()
{
}

PipelineFlagsInstance::ShaderRecordEXT::ShaderRecordEXT (GeometryTypes type, deUint32 index, const tcu::IVec4& ret)
	: geomType(type)
	, geomIndex(index)
	, retValue(ret)
{
}

class RayTracingTestPipeline : protected RayTracingPipeline
{
public:
	RayTracingTestPipeline (Context& context, const PipelineFlagsInstance& testInstance, const TestParams& params)
		: m_context			(context)
		, m_vkd				(context.getDeviceInterface())
		, m_device			(context.getDevice())
		, m_allocator		(context.getDefaultAllocator())
		, m_testInstance	(testInstance)
		, m_params			(params)
	{
		m_rgenModule = createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("rgen"), 0);

		// miss shader is loaded into each test regardless m_params.miss() is set
		m_missModule = createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("miss"), 0);

		// cloest hit shader is loaded into each test regardless m_params.chit() is set
		m_chitModule = createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("chit"), 0);

		if (m_params.ahit())
			m_ahitModule = createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("ahit"), 0);

		if (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox))
			m_isectModule = createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("isect"), 0);

		setCreateFlags(m_params.flags);
		if (m_params.useMaintenance5)
			setCreateFlags2(translateCreateFlag(m_params.flags));

		setMaxPayloadSize(sizeof(PipelineFlagsInstance::ShaderRecordEXT::retValue));
		setMaxAttributeSize(sizeof(PipelineFlagsInstance::ShaderRecordEXT::retValue));
	}

	template<class ShaderRecord> struct SBT
	{
		static const deUint32 recordSize = static_cast<deUint32>(sizeof(ShaderRecord));

		const DeviceInterface&			m_vkd;
		const VkDevice					m_dev;
		const VkPipeline				m_pipeline;
		const deUint32					m_groupCount;
		const deUint32					m_handleSize;
		const deUint32					m_alignment;
		de::MovePtr<BufferWithMemory>	m_buffer;
		deUint8*						m_content;

		SBT (const DeviceInterface& vkd, VkDevice dev, Allocator& allocator, VkPipeline pipeline,
			 deUint32 groupCount, deUint32 shaderGroupHandleSize,  deUint32 shaderGroupBaseAlignment)
			: m_vkd(vkd), m_dev(dev), m_pipeline(pipeline)
			, m_groupCount(groupCount), m_handleSize(shaderGroupHandleSize)
			, m_alignment(deAlign32(shaderGroupHandleSize + recordSize, shaderGroupBaseAlignment))
			, m_buffer(), m_content()
		{
			const deUint32				size		= groupCount * m_alignment;
			const VkBufferUsageFlags	flags		= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			VkBufferCreateInfo			info		= makeBufferCreateInfo(size, flags);
			const MemoryRequirement		memReq		= MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress;
			m_buffer	= de::MovePtr<BufferWithMemory>(new BufferWithMemory(m_vkd, m_dev, allocator, info, memReq));
			m_content	= (deUint8*)m_buffer->getAllocation().getHostPtr();
		}

		void updateAt (deUint32 index, const deUint8* handle, const ShaderRecord& rec)
		{
			DE_ASSERT(index < m_groupCount);
			deUint8* groupPos = m_content + index * m_alignment;
			deMemcpy(groupPos, handle, m_handleSize);
			deMemcpy(groupPos + m_handleSize, &rec, recordSize);
		}

		void flush ()
		{
			Allocation&					alloc	= m_buffer->getAllocation();
			flushMappedMemoryRange(m_vkd, m_dev, alloc.getMemory(), alloc.getOffset(), VK_WHOLE_SIZE);
		}

		deUint32	getAlignment () const { return m_alignment; }

		de::MovePtr<BufferWithMemory> get () { return m_buffer; }
	};

	struct PLDeleter
	{
		const RayTracingTestPipeline*				pipeline;
		std::function<void(RayTracingPipeline*)>	whenDestroying;
		PLDeleter (RayTracingTestPipeline* pl, std::function<void(RayTracingPipeline*)>	doWhenDestroying)
			: pipeline(pl), whenDestroying(doWhenDestroying) {}
		void operator()(RayTracingPipeline* pl)
		{
			if (pipeline != pl && pipeline->m_params.useLibs)
			{
				if (whenDestroying) whenDestroying(pl);
				delete pl;
			}
		}
	};

	auto createLibraryPipeline (std::function<void(RayTracingPipeline*)> doWhenDestroying)
		-> de::UniquePtr<RayTracingPipeline, PLDeleter>
	{
		RayTracingPipeline* pl = this;
		if (m_params.useLibs) {
			pl = new RayTracingPipeline;
			pl->setCreateFlags(m_params.flags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
			pl->setMaxPayloadSize(sizeof(PipelineFlagsInstance::ShaderRecordEXT::retValue));
			pl->setMaxAttributeSize(sizeof(PipelineFlagsInstance::ShaderRecordEXT::retValue));
		}
		return de::UniquePtr<RayTracingPipeline, PLDeleter>(pl, PLDeleter(this, doWhenDestroying));
	}

	Move<VkPipeline>	createPipeline (const VkPipelineLayout pipelineLayout)
	{
		deUint32	groupIndex	= 0;
		const bool	checkIsect	= (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox);

		auto		appendPipelineLibrary	= [this, &pipelineLayout](RayTracingPipeline* pl) -> void
		{
			m_libraries.emplace_back(makeVkSharedPtr(pl->createPipeline(m_vkd, m_device, pipelineLayout)));
		};

		DE_ASSERT(						(VkShaderModule(0) != *m_rgenModule));
		DE_ASSERT(						(VkShaderModule(0) != *m_missModule));
		DE_ASSERT(m_params.ahit()	==	(VkShaderModule(0) != *m_ahitModule));
		DE_ASSERT(						(VkShaderModule(0) != *m_chitModule));
		DE_ASSERT(checkIsect		==	(VkShaderModule(0) != *m_isectModule));

		// rgen in the main pipeline only
		addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, *m_rgenModule, groupIndex++);

		createLibraryPipeline(appendPipelineLibrary)->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, *m_missModule, (m_params.useLibs ? 0 : groupIndex++));

		{
			const deUint32 hitGroupIndex = m_params.useLibs ? 0 : groupIndex;
			auto pipeline = createLibraryPipeline(appendPipelineLibrary);
			if (m_params.ahit())	pipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		*m_ahitModule, hitGroupIndex);
			pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	*m_chitModule, hitGroupIndex);
			if (checkIsect)			pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	*m_isectModule, hitGroupIndex);
		}

		for (const auto& sg : m_shadersGroupCreateInfos)
		{
			static_cast<void>(sg);
			DE_ASSERT(sg.type != VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR);
		}

		return RayTracingPipeline::createPipeline(m_vkd, m_device, pipelineLayout, m_libraries);
	}

	std::pair<de::SharedPtr<BufferWithMemory>, VkStridedDeviceAddressRegionKHR>	createRaygenShaderBindingTable (VkPipeline pipeline)
	{
		de::MovePtr<BufferWithMemory>	sbt	= createShaderBindingTable(m_vkd, m_device, pipeline, m_allocator,
																	   m_testInstance.shaderGroupHandleSize,
																	   m_testInstance.shaderGroupBaseAlignment, 0, 1);
		VkStridedDeviceAddressRegionKHR	rgn	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(m_vkd, m_device, **sbt, 0),
																				m_testInstance.shaderGroupHandleSize,
																				m_testInstance.shaderGroupHandleSize);
		return { de::SharedPtr<BufferWithMemory>(sbt.release()), rgn };
	}

	std::pair<de::SharedPtr<BufferWithMemory>, VkStridedDeviceAddressRegionKHR>	createMissShaderBindingTable (VkPipeline pipeline)
	{
		const auto		entries			= m_testInstance.prepareShaderBindingTable();
		const void*		shaderRecPtr	= static_cast<PipelineFlagsInstance::ShaderRecordEXT const*>(&std::get<2>(entries[1]));
		const deUint32	shaderRecSize	= static_cast<deUint32>(sizeof(PipelineFlagsInstance::ShaderRecordEXT));
		const deUint32	alignment		= deAlign32(m_testInstance.shaderGroupHandleSize + shaderRecSize, m_testInstance.shaderGroupBaseAlignment);
		const deUint32	sbtOffset		= 0;

		de::MovePtr<BufferWithMemory>	sbt	= createShaderBindingTable(m_vkd, m_device, pipeline, m_allocator,
																	   m_testInstance.shaderGroupHandleSize,
																	   m_testInstance.shaderGroupBaseAlignment,
																	   1, 1,
																	   VkBufferCreateFlags(0u),
																	   VkBufferUsageFlags(0u),
																	   MemoryRequirement::Any,
																	   VkDeviceAddress(0),
																	   sbtOffset,
																	   shaderRecSize,
																	   &shaderRecPtr);

		VkStridedDeviceAddressRegionKHR	rgn	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(m_vkd, m_device, **sbt, 0),
																				alignment,
																				m_testInstance.shaderGroupHandleSize);
		return { de::SharedPtr<BufferWithMemory>(sbt.release()), rgn };
	}

	std::pair<de::SharedPtr<BufferWithMemory>, VkStridedDeviceAddressRegionKHR>	createHitShaderBindingTable (VkPipeline pipeline)
	{
		de::MovePtr<BufferWithMemory>	buf;
		VkStridedDeviceAddressRegionKHR	rgn;

		std::vector<deUint8>	handles			(m_testInstance.shaderGroupHandleSize);
		auto					records			= m_testInstance.prepareShaderBindingTable();
		const deUint32			hitGroupCount	= deUint32(records.size() - 2);

		SBT<PipelineFlagsInstance::ShaderRecordEXT> sbt(m_vkd, m_device, m_allocator, pipeline, hitGroupCount,
														m_testInstance.shaderGroupHandleSize, m_testInstance.shaderGroupBaseAlignment);

		VK_CHECK(m_vkd.getRayTracingShaderGroupHandlesKHR(m_device, pipeline, 2, 1, handles.size(), handles.data()));

		for (deUint32 i = 0; i < hitGroupCount; ++i)
		{
			// copy the SBT record if it was initialized in prepareShaderBindingTable()
			if (std::get<3>(records[i + 2]))
			{
				const PipelineFlagsInstance::ShaderRecordEXT& rec = std::get<2>(records[i + 2]);
				sbt.updateAt(i, handles.data(), rec);
			}
		}

		sbt.flush();
		buf	= sbt.get();
		rgn = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(m_vkd, m_device, **buf, 0),
																		sbt.getAlignment(),	m_testInstance.shaderGroupHandleSize);

		return { de::SharedPtr<BufferWithMemory>(buf.release()), rgn };
	}

private:
	Context&										m_context;
	const DeviceInterface&							m_vkd;
	const VkDevice									m_device;
	Allocator&										m_allocator;
	const PipelineFlagsInstance&					m_testInstance;
	const TestParams								m_params;
	Move<VkShaderModule>							m_rgenModule;
	Move<VkShaderModule>							m_chitModule;
	Move<VkShaderModule>							m_ahitModule;
	Move<VkShaderModule>							m_isectModule;
	Move<VkShaderModule>							m_missModule;
	Move<VkShaderModule>							m_gapModule;
	std::vector<de::SharedPtr<Move<VkPipeline>>>	m_libraries;
};

template<class T, class P = T(*)[1], class R = decltype(std::begin(*std::declval<P>()))>
auto makeStdBeginEnd(T* p, deUint32 n) -> std::pair<R, R>
{
	auto tmp = std::begin(*P(p));
	auto begin = tmp;
	std::advance(tmp, n);
	return { begin, tmp };
}

PipelineFlagsCase::PipelineFlagsCase (tcu::TestContext& testCtx, const std::string& name, const TestParams& params)
	: TestCase					(testCtx, name, std::string())
	, shaderGroupHandleSize		(m_shaderGroupHandleSize)
	, shaderGroupBaseAlignment	(m_shaderGroupBaseAlignment)
	, m_params					(params)
	, m_rgenPayload				(0, ':', 0, 0)
	, m_defMissRetGreenComp		('-')
	, m_defTriRetGreenComp		('A')
	, m_defBoxRetGreenComp		(calcDefBoxRetGreenComp(params, m_defTriRetGreenComp))
{
}

void PipelineFlagsCase::checkSupport (Context& context) const
{
	if ((VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR & m_params.flags)
		&& (GeometryTypes::Triangle == m_params.geomTypes))
	{
		TCU_THROW(InternalError, "Illegal params combination: VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR and Triangles");
	}

	if (!context.isDeviceFunctionalitySupported("VK_KHR_ray_tracing_pipeline"))
		TCU_THROW(NotSupportedError, "VK_KHR_ray_tracing_pipeline not supported");

	// VK_KHR_acceleration_structure is required by VK_KHR_ray_tracing_pipeline.
	if (!context.isDeviceFunctionalitySupported("VK_KHR_acceleration_structure"))
		TCU_FAIL("VK_KHR_acceleration_structure not supported but VK_KHR_ray_tracing_pipeline supported");

	// The same for VK_KHR_buffer_device_address.
	if (!context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
		TCU_FAIL("VK_KHR_buffer_device_address not supported but VK_KHR_acceleration_structure supported");

	if (m_params.useLibs && !context.isDeviceFunctionalitySupported("VK_KHR_pipeline_library"))
		TCU_FAIL("VK_KHR_pipeline_library not supported but VK_KHR_ray_tracing_pipeline supported");

	if (m_params.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR& accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (m_params.onHhost && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	checkAccelerationStructureVertexBufferFormat(context.getInstanceInterface(), context.getPhysicalDevice(), VK_FORMAT_R32G32B32_SFLOAT);

	auto rayTracingProperties	= makeRayTracingProperties(context.getInstanceInterface(), context.getPhysicalDevice());
	m_shaderGroupHandleSize		= rayTracingProperties->getShaderGroupHandleSize();
	m_shaderGroupBaseAlignment	= rayTracingProperties->getShaderGroupBaseAlignment();
}

void PipelineFlagsCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const char						endl			= '\n';
	const deUint32					missIdx			= 0;

	const std::string				payloadInDecl	=
		"layout(location = 0) rayPayloadInEXT ivec4 payload;";

	const std::string				recordDecl		=
		"layout(shaderRecordEXT, std430) buffer Rec {\n"
		"  uint  geomType;\n"
		"  uint  geomIndex;\n"
		"  ivec4 retValue;\n"
		"} record;";

	{
		std::stringstream str;
		str << "#version 460 core"																			<< endl
			<< "#extension GL_EXT_ray_tracing : require"													<< endl
			<< "layout(location = 0) rayPayloadEXT ivec4 payload;"											<< endl
			<< "layout(rgba32i, set = 0, binding = 0) uniform iimage2D result;"								<< endl
			<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;"					<< endl
			<< "void main()"																				<< endl
			<< "{"																							<< endl
			<< "  float rx           = (float(gl_LaunchIDEXT.x * 2) / float(gl_LaunchSizeEXT.x)) - 1.0;"	<< endl
			<< "  float ry           = (float(gl_LaunchIDEXT.y) + 0.5) / float(gl_LaunchSizeEXT.y);"		<< endl
			<< "  payload            = ivec4" << m_rgenPayload << ";"										<< endl
			<< "  uint  rayFlags     = gl_RayFlagsNoneEXT;"													<< endl
			<< "  uint  cullMask     = 0xFFu;"																<< endl
			<< "  uint  stbRecOffset = " << m_params.stbRecOffset << "u;"									<< endl
			<< "  uint  stbRecStride = " << m_params.stbRecStride << "u;"									<< endl
			<< "  uint  missIdx      = " << missIdx << "u;"													<< endl
			<< "  vec3  orig         = vec3(rx, ry, 1.0);"													<< endl
			<< "  float tmin         = 0.0;"																<< endl
			<< "  vec3  dir          = vec3(0.0, 0.0, -1.0);"												<< endl
			<< "  float tmax         = 1000.0;"																<< endl
			<< "  traceRayEXT(topLevelAS, rayFlags, cullMask, stbRecOffset, stbRecStride, missIdx, orig, tmin, dir, tmax, 0);"	<< endl
			<< "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);"									<< endl
			<< "}";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(str.str()) << buildOptions;
	}

	// miss shader is created in each test regardless the m_params.miss() is set
	{
		std::stringstream str;
		str	<< "#version 460 core"							<< endl
			<< "#extension GL_EXT_ray_tracing : require"	<< endl
			<< payloadInDecl								<< endl
			<< recordDecl									<< endl
			<< "void main()"								<< endl
			<< "{"											<< endl
			<< "  payload = record.retValue;"				<< endl
			<< "}";
		programCollection.glslSources.add("miss") << glu::MissSource(str.str()) << buildOptions;
	}

	// closest hit shader is created in each test regardless the m_params.chit() is set
	{
		std::stringstream str;
		str << "#version 460 core"														<< endl
			<< "#extension GL_EXT_ray_tracing : require"								<< endl
			<< "hitAttributeEXT ivec4 hitAttribute;"									<< endl
			<< payloadInDecl															<< endl
			<< recordDecl																<< endl
			<< "void main()"															<< endl
			<< "{"																		<< endl
			<< "  if (record.geomType == " << deUint32(GeometryTypes::Triangle) << ")"	<< endl
			<< "    payload = record.retValue;"											<< endl
			<< "  else payload = hitAttribute;"											<< endl
			<< "}";
		programCollection.glslSources.add("chit") << glu::ClosestHitSource(str.str()) << buildOptions;
	}

	if (m_params.ahit())
	{
		std::stringstream str;
		str << "#version 460 core"							<< endl
			<< "#extension GL_EXT_ray_tracing : require"	<< endl
			<< recordDecl									<< endl
			<< "void main()"								<< endl
			<< "{"											<< endl
			<< "  if (record.geomIndex % 2 == 1)"			<< endl
			<< "    ignoreIntersectionEXT;"					<< endl
			<< "}";
		programCollection.glslSources.add("ahit") << glu::AnyHitSource(str.str()) << buildOptions;
	}

	if (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox))
	{
		std::stringstream str;
		str << "#version 460 core"								<< endl
			<< "#extension GL_EXT_ray_tracing : require"		<< endl
			<< "hitAttributeEXT ivec4 hitAttribute;"			<< endl
			<< recordDecl										<< endl
			<< "void main()"									<< endl
			<< "{"												<< endl
			<< "  hitAttribute = ivec4(record.retValue.x + 0"	<< endl
			<< "                      ,record.retValue.y + 2"	<< endl
			<< "                      ,record.retValue.z + 3"	<< endl
			<< "                      ,record.retValue.w + 4);"	<< endl
			<< "  reportIntersectionEXT(0.0, 0);"				<< endl
			<< "}";
		programCollection.glslSources.add("isect") << glu::IntersectionSource(str.str()) << buildOptions;
	}
}

TestInstance* PipelineFlagsCase::createInstance (Context& context) const
{
	return new PipelineFlagsInstance(context, m_params, shaderGroupHandleSize, shaderGroupBaseAlignment,
													m_rgenPayload, m_defMissRetGreenComp, m_defTriRetGreenComp, m_defBoxRetGreenComp);
}

PipelineFlagsInstance::PipelineFlagsInstance (Context&			context,
											  const TestParams&	params,
											  const deUint32&	shaderGroupHandleSize_,
											  const deUint32&	shaderGroupBaseAlignment_,
											  const tcu::IVec4& rgenPayload_,
											  deInt32			defMissRetGreenComp_,
											  deInt32			defTriRetGreenComp_,
											  deInt32			defBoxRetGreenComp_)
	: TestInstance				(context)
	, rgenPayload				(rgenPayload_)
	, defMissRetGreenComp		(defMissRetGreenComp_)
	, defTriRetGreenComp		(defTriRetGreenComp_)
	, defBoxRetGreenComp		(defBoxRetGreenComp_)
	, shaderGroupHandleSize		(shaderGroupHandleSize_)
	, shaderGroupBaseAlignment	(shaderGroupBaseAlignment_)
	, m_params					(params)
	, m_format					(VK_FORMAT_R32G32B32A32_SINT)
{
}

VkImageCreateInfo PipelineFlagsInstance::makeImageCreateInfo () const
{
	const deUint32				familyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkImageCreateInfo		imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																					// VkImageType				imageType;
		m_format,																							// VkFormat					format;
		makeExtent3D(m_params.width, m_params.height, 1u),													// VkExtent3D				extent;
		1u,																									// deUint32					mipLevels;
		1u,																									// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,																				// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,																			// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																			// VkSharingMode			sharingMode;
		1u,																									// deUint32					queueFamilyIndexCount;
		&familyIndex,																						// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED																			// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

std::vector<PipelineFlagsInstance::TriGeometry>
PipelineFlagsInstance::prepareTriGeometries (const float zCoord) const
{
	const tcu::Vec3	center(-0.5f, 0.5f, zCoord);
	const tcu::Vec3	start(0.0f, 0.5f, zCoord);
	const deUint32 maxTriangles = m_params.instCount * m_params.geomCount;
	const deUint32 trianglesCount = deMaxu32(maxTriangles, 3);
	const float angle = (4.0f * std::acos(0.0f)) / float(trianglesCount);

	tcu::Vec3	point(start);
	std::vector<TriGeometry> geometries(maxTriangles);
	for (deUint32 inst = 0, idx = 0; inst < m_params.instCount; ++inst)
	{
		for (deUint32 geom = 0; geom < m_params.geomCount; ++geom, ++idx)
		{
			TriGeometry& geometry = geometries[idx];

			geometry[0] = center;
			geometry[1] = point;
			geometry[2] = (maxTriangles >= 3 && trianglesCount - idx == 1u) ? start : rotateCcwZ(point, center, angle);

			point = geometry[2];
		}
	}

	return geometries;
}

std::vector<PipelineFlagsInstance::BoxGeometry>
PipelineFlagsInstance::prepareBoxGeometries (const float zFront, const float zBack) const
{
	const deUint32				maxBoxes	= m_params.instCount * m_params.geomCount;

	std::vector<BoxGeometry>	boxes		(maxBoxes);
	deUint32					boxesPerDim	= 0u;
	float						boxWidth	= 0.0f;
	float						boxHeight	= 0.0f;

	// find nearest square ceil number
	do
	{
		++boxesPerDim;
		boxWidth = 1.0f / float(boxesPerDim);
		boxHeight = 1.0f / float(boxesPerDim);
	} while (boxesPerDim * boxesPerDim < maxBoxes);

	for (deUint32 boxY = 0, boxIdx = 0; boxY < boxesPerDim && boxIdx < maxBoxes; ++boxY)
	{
		for (deUint32 boxX = 0; boxX < boxesPerDim && boxIdx < maxBoxes; ++boxX, ++boxIdx)
		{
			const float x = float(boxX) * boxWidth;
			const float y = float(boxY) * boxHeight;
			BoxGeometry box = { { tcu::Vec3(x, y, zFront), tcu::Vec3((x + boxWidth), (y + boxHeight), zBack) } };
			boxes[boxIdx].swap(box);
		}
	}

	return boxes;
}

PipelineFlagsInstance::BottomLevelASPtrs
PipelineFlagsInstance::createBottomLevelAccelerationStructs (VkCommandBuffer cmdBuffer) const
{
	const DeviceInterface&		vkd			= m_context.getDeviceInterface();
	const VkDevice				device		= m_context.getDevice();
	Allocator&					allocator	= m_context.getDefaultAllocator();
	const VkGeometryFlagsKHR	geomFlags	= m_params.ahit() ? VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR : VK_GEOMETRY_OPAQUE_BIT_KHR;

	BottomLevelASPtrs			result;

	if (!m_params.isect() && ((m_params.geomTypes == GeometryTypes::Triangle) || (m_params.geomTypes == GeometryTypes::TriangleAndBox)))
	{
		const auto	geometries = prepareTriGeometries(0.0f);

		for (deUint32 inst = 0, idx = 0; inst < m_params.instCount; ++inst)
		{
			auto blas = makeBottomLevelAccelerationStructure();
			blas->setBuildType(m_params.onHhost ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);

			for (deUint32 geom = 0; geom < m_params.geomCount; ++geom, ++idx)
			{
				const TriGeometry& triangle = geometries[idx];
				blas->addGeometry(std::vector<tcu::Vec3>(triangle.begin(), triangle.end()), true, geomFlags);
			}

			blas->createAndBuild(vkd, device, cmdBuffer, allocator);
			result.emplace_back(de::SharedPtr<BottomLevelAccelerationStructure>(blas.release()));
		}
	}

	if (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox))
	{
		const auto	geometries = prepareBoxGeometries(0.0f, 0.0f);

		for (deUint32 inst = 0, idx = 0; inst < m_params.instCount; ++inst)
		{
			auto blas = makeBottomLevelAccelerationStructure();
			blas->setUseMaintenance5(m_params.useMaintenance5);
			blas->setBuildType(m_params.onHhost ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);

			for (deUint32 geom = 0; geom < m_params.geomCount; ++geom, ++idx)
			{
				const BoxGeometry& box = geometries[idx];
				blas->addGeometry(std::vector<tcu::Vec3>(box.begin(), box.end()), false, geomFlags);
			}

			blas->createAndBuild(vkd, device, cmdBuffer, allocator);
			result.emplace_back(de::SharedPtr<BottomLevelAccelerationStructure>(blas.release()));
		}
	}

	return result;
}

PipelineFlagsInstance::TopLevelASPtr
PipelineFlagsInstance::createTopLevelAccelerationStruct (VkCommandBuffer cmdBuffer, const BottomLevelASPtrs& blasPtrs) const
{
	const DeviceInterface&	vkd							= m_context.getDeviceInterface();
	const VkDevice			device						= m_context.getDevice();
	Allocator&				allocator					= m_context.getDefaultAllocator();
	const deUint32			groupsAndGapsPerInstance	= computeEffectiveShaderGroupCount(m_params);

	auto					tlas		= makeTopLevelAccelerationStructure();

	tlas->setBuildType(m_params.onHhost ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
	tlas->setInstanceCount(blasPtrs.size());
	for (auto begin = blasPtrs.begin(), end = blasPtrs.end(), i = begin; i != end; ++i)
	{
		const deUint32 instanceShaderBindingTableRecordOffset = static_cast<deUint32>(std::distance(begin, i) * groupsAndGapsPerInstance);
		tlas->addInstance(*i, identityMatrix3x4, 0, 0xFF, instanceShaderBindingTableRecordOffset);
	}
	tlas->createAndBuild(vkd, device, cmdBuffer, allocator);

	return TopLevelASPtr(tlas.release());
}

std::vector<PipelineFlagsInstance::ShaderRecordEntry> PipelineFlagsInstance::prepareShaderBindingTable() const
{
	const bool						includeTriangles = (!m_params.isect() && ((m_params.geomTypes == GeometryTypes::Triangle) || (m_params.geomTypes == GeometryTypes::TriangleAndBox)));
	const bool						includeBoxes = (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox));
	const deUint32					groupsAndGapsPerInstance = computeEffectiveShaderGroupCount(m_params);
	const deUint32					commonGroupCount = 2; // general groups for rgen and miss
	const deUint32					triangleGroupCount = includeTriangles ? (groupsAndGapsPerInstance * m_params.instCount) : 0;
	const deUint32					proceduralGroupCount = includeBoxes ? (groupsAndGapsPerInstance * m_params.instCount) : 0;
	const deUint32					totalGroupCount = commonGroupCount + triangleGroupCount + proceduralGroupCount;

	std::vector<ShaderRecordEntry>	shaderRecords(totalGroupCount);

	shaderRecords[0] = std::tuple<VkShaderStageFlags, HitGroup, ShaderRecordEXT, bool>( VK_SHADER_STAGE_RAYGEN_BIT_KHR, {}, {}, true );
	shaderRecords[1] = std::tuple<VkShaderStageFlags, HitGroup, ShaderRecordEXT, bool>( VK_SHADER_STAGE_MISS_BIT_KHR, {}, { GeometryTypes::Box, (~0u), tcu::IVec4(0, defMissRetGreenComp, 0, 0) }, true );

	de::SharedPtr<AnyHitShader>			ahit(new AnyHitShader);
	de::SharedPtr<ClosestHitShader>		chit(new ClosestHitShader);
	de::SharedPtr<IntersectionShader>	isect(new IntersectionShader);

	if (includeTriangles)
	{
		std::set<deUint32>	usedIndexes;
		deInt32	greenComp = defTriRetGreenComp;

		const deUint32 recordsToSkip = commonGroupCount;

		for (deUint32 instance = 0; instance < m_params.instCount; ++instance)
		{
			const deUint32 instanceShaderBindingTableRecordOffset = recordsToSkip + instance * groupsAndGapsPerInstance;
			for (deUint32 geometryIndex = 0; geometryIndex < m_params.geomCount; ++geometryIndex)
			{
				const deUint32 shaderGroupIndex = instanceShaderBindingTableRecordOffset + geometryIndex * m_params.stbRecStride + m_params.stbRecOffset;
				if (usedIndexes.find(shaderGroupIndex) == usedIndexes.end())
				{
					HitGroup			hitGroup;
					VkShaderStageFlags	flags = 0;
					if (m_params.ahit()) {
						flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
						hitGroup.ahit = ahit;
					}
					flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
					hitGroup.chit = chit;
					shaderRecords[shaderGroupIndex] = std::tuple<VkShaderStageFlags, HitGroup, ShaderRecordEXT, bool>( flags, hitGroup, { GeometryTypes::Triangle, geometryIndex, tcu::IVec4(0, greenComp++, 0, 0) }, true );
					usedIndexes.insert(shaderGroupIndex);
				}
			}
		}
	}

	if (includeBoxes)
	{
		std::set<deUint32>	usedIndexes;
		deInt32	greenComp = defBoxRetGreenComp;

		const deUint32 recordsToSkip = triangleGroupCount + commonGroupCount;

		for (deUint32 instance = 0; instance < m_params.instCount; ++instance)
		{
			const deUint32 instanceShaderBindingTableRecordOffset = recordsToSkip + instance * groupsAndGapsPerInstance;
			for (deUint32 geometryIndex = 0; geometryIndex < m_params.geomCount; ++geometryIndex)
			{
				const deUint32 shaderGroupIndex = instanceShaderBindingTableRecordOffset + geometryIndex * m_params.stbRecStride + m_params.stbRecOffset;
				if (usedIndexes.find(shaderGroupIndex) == usedIndexes.end())
				{
					HitGroup			hitGroup;
					VkShaderStageFlags	flags = 0;
					if (m_params.ahit()) {
						flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
						hitGroup.ahit = ahit;
					}
					flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
					hitGroup.chit = chit;
					{ //In the case of AABB isect must be provided, otherwise we will process AABB with TRIANGLES_HIT_GROUP
						flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
						hitGroup.isect = isect;
					}
					shaderRecords[shaderGroupIndex] = std::tuple<VkShaderStageFlags, HitGroup, ShaderRecordEXT, bool>( flags, hitGroup, { GeometryTypes::Box, geometryIndex, tcu::IVec4(0, greenComp++, 0, 0) }, true );
					usedIndexes.insert(shaderGroupIndex);
				}
			}
		}
	}

	return shaderRecords;
}

tcu::IVec2 PipelineFlagsInstance::rayToImage(const tcu::Vec2& rayCoords) const
{
	return tcu::IVec2(
		deInt32(((rayCoords.x() + 1.0f) * float(m_params.width)) / 2.0f),
		deInt32((rayCoords.y() * float(m_params.height)) - 0.5f)
	);
}

tcu::Vec2 PipelineFlagsInstance::imageToRay(const tcu::IVec2& imageCoords) const
{
	const float		rx = (float(imageCoords.x() * 2) / float(m_params.width)) - 1.0f;
	const float		ry = (float(imageCoords.y()) + 0.5f) / float(m_params.height);
	return tcu::Vec2(rx, ry);
}

deUint32 PipelineFlagsInstance::computeSamePixelCount (const std::vector<tcu::IVec4>&						image,
																     const tcu::Vec2									pixelCoords,
																	 const tcu::IVec4&									requiredColor,
																	 const tcu::IVec4&									floodColor,
																	 const std::function<bool(const tcu::Vec3&)>&		pointInGeometry,
																	 std::vector<std::pair<tcu::IVec4, tcu::IVec2>>&	auxBuffer) const
{
	if (!pointInGeometry(tcu::Vec3(pixelCoords.x(), pixelCoords.y(), 0.0f))) return 0;

	auxBuffer.resize(image.size() * 4);
	std::transform(image.begin(), image.end(), auxBuffer.begin(), [](const tcu::IVec4& c) -> std::pair<tcu::IVec4, tcu::IVec2> { return { c, tcu::IVec2() }; });

	tcu::Vec2		rayCoord;
	tcu::IVec2		imageCoords = rayToImage(pixelCoords);
	deUint32		pixelIndex	= imageCoords.y() * m_params.width + imageCoords.x();
	tcu::IVec4		imageColor	= image[pixelIndex];

	if (requiredColor != imageColor) return 0;

	deInt32			stackIndex	= 0;
	deUint32		sameCount	= 1;
	auxBuffer[stackIndex].second = imageCoords;

	while (stackIndex >= 0)
	{
		imageCoords = auxBuffer[stackIndex].second;		--stackIndex;

		if (imageCoords.x() < 0 || imageCoords.x() >= deInt32(m_params.width)
			|| imageCoords.y() < 0 || imageCoords.y() >= deInt32(m_params.height)) continue;

		rayCoord = imageToRay(imageCoords);
		if (!pointInGeometry(tcu::Vec3(rayCoord.x(), rayCoord.y(), 0.0f))) continue;

		pixelIndex = imageCoords.y() * m_params.width + imageCoords.x();
		imageColor = auxBuffer[pixelIndex].first;
		if (requiredColor != imageColor) continue;

		auxBuffer[pixelIndex].first = floodColor;
		sameCount += 1;

		auxBuffer[++stackIndex].second = tcu::IVec2(imageCoords.x() - 1, imageCoords.y());
		auxBuffer[++stackIndex].second = tcu::IVec2(imageCoords.x() + 1, imageCoords.y());
		auxBuffer[++stackIndex].second = tcu::IVec2(imageCoords.x(), imageCoords.y() - 1);
		auxBuffer[++stackIndex].second = tcu::IVec2(imageCoords.x(), imageCoords.y() + 1);
	}

	return sameCount;
}

void PipelineFlagsInstance::travelRay (std::vector<tcu::IVec4>&					outImage,
									   const deUint32							glLaunchIdExtX,
									   const deUint32							glLaunchIdExtY,
									   const std::vector<ShaderRecordEntry>&	shaderBindingTable,
									   const MissShader&						missShader,
									   const std::vector<TriGeometry>&			triangleGeometries,
									   const std::vector<BoxGeometry>&			boxGeometries) const
{
	const tcu::Vec2	rayCoords					= imageToRay(tcu::IVec2(glLaunchIdExtX, glLaunchIdExtY));
	const bool		includeTriangles			= (!m_params.isect() && ((m_params.geomTypes == GeometryTypes::Triangle) || (m_params.geomTypes == GeometryTypes::TriangleAndBox)));
	const bool		includeBoxes				= (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox));
	const deUint32	commonGroupCount			= 2; // general groups for rgen and miss
	const deUint32	groupsAndGapsPerInstance	= computeEffectiveShaderGroupCount(m_params);
	const deUint32	triangleGroupCount			= includeTriangles ? (groupsAndGapsPerInstance * m_params.instCount) : 0;

	bool			hitHappened					(false);
	deUint32		shaderGroupIndex			(~0u);
	tcu::IVec4		payload						(rgenPayload);
	const tcu::Vec3	origin						(rayCoords.x(), rayCoords.y(), 1.0f);

	if (includeTriangles)
	{
		const deUint32 recordsToSkip = commonGroupCount;
		for (deUint32 instance = 0; !hitHappened && (instance < m_params.instCount); ++instance)
		{
			const deUint32 instanceShaderBindingTableRecordOffset = recordsToSkip + instance * groupsAndGapsPerInstance;
			for (deUint32 geometryIndex = 0; !hitHappened && (geometryIndex < m_params.geomCount); ++geometryIndex)
			{
				const TriGeometry& geometry = triangleGeometries[instance * m_params.geomCount + geometryIndex];
				shaderGroupIndex = instanceShaderBindingTableRecordOffset + geometryIndex * m_params.stbRecStride + m_params.stbRecOffset;
				if (pointInTriangle2D(origin, geometry[0], geometry[1], geometry[2]))
				{
					hitHappened = true;
				}
			}
		}
	}

	if (includeBoxes)
	{
		const deUint32 recordsToSkip = triangleGroupCount + commonGroupCount;
		for (deUint32 instance = 0; !hitHappened && (instance < m_params.instCount); ++instance)
		{
			const deUint32 instanceShaderBindingTableRecordOffset = recordsToSkip + instance * groupsAndGapsPerInstance;
			for (deUint32 geometryIndex = 0; !hitHappened && (geometryIndex < m_params.geomCount); ++geometryIndex)
			{
				const BoxGeometry& geometry = boxGeometries[instance * m_params.geomCount + geometryIndex];
				shaderGroupIndex = instanceShaderBindingTableRecordOffset + geometryIndex * m_params.stbRecStride + m_params.stbRecOffset;
				if (pointInRect2D(origin, geometry[0], geometry[1]))
				{
					hitHappened = true;
				}
			}
		}
	}

	if (hitHappened)
	{
		const ShaderRecordEXT&		shaderRecord	= std::get<2>(shaderBindingTable[shaderGroupIndex]);
		const HitGroup&				hitGroup		= std::get<1>(shaderBindingTable[shaderGroupIndex]);
		const VkShaderStageFlags	flags			= std::get<0>(shaderBindingTable[shaderGroupIndex]);
		auto						hitAttribute	= rgenPayload;
		bool						ignoreIsect		= false;

		// check if the SBT entry was was initialized
		DE_ASSERT(std::get<3>(shaderBindingTable[shaderGroupIndex]));

		if (flags & VK_SHADER_STAGE_INTERSECTION_BIT_KHR)
		{
			hitAttribute = hitGroup.isect->invoke(IntersectionShader::dummyPayload, shaderRecord);
		}
		if (flags & VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
		{
			ignoreIsect = hitGroup.ahit->ignoreIntersection(AnyHitShader::dummyPayload, shaderRecord);
		}
		if (ignoreIsect)
		{
			payload = missShader.invoke(MissShader::dummyPayload, std::get<2>(shaderBindingTable[1]));
		}
		else if (flags & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		{
			payload = hitGroup.chit->invoke(hitAttribute, shaderRecord);
		}
	}
	else
	{
		payload = missShader.invoke(MissShader::dummyPayload, std::get<2>(shaderBindingTable[1]));
	}

	outImage[glLaunchIdExtY * m_params.width + glLaunchIdExtX] = payload;
}

#ifdef INTERNAL_DEBUG
void PipelineFlagsInstance::printImage(const tcu::IVec4* image) const
{
	for (deUint32 y = 0; y < m_params.height; ++y)
	{
		for (deUint32 x = 0; x < m_params.width; ++x)
			std::cout << static_cast<char>(image[(m_params.height - y - 1) * m_params.width + x].y());
		std::cout << std::endl;
	}
}
#endif

bool PipelineFlagsInstance::verifyResult (const BufferWithMemory* resultBuffer) const
{
	const auto								triangleGeometries	= prepareTriGeometries(0.0f);
	const auto								boxGeometries		= prepareBoxGeometries(0.0f, 0.0f);

	const bool								includeTriangles	= (!m_params.isect() && ((m_params.geomTypes == GeometryTypes::Triangle) || (m_params.geomTypes == GeometryTypes::TriangleAndBox)));
	const bool								includeBoxes		= (m_params.isect() || (m_params.geomTypes == GeometryTypes::Box) || (m_params.geomTypes == GeometryTypes::TriangleAndBox));

	const tcu::IVec4*						resultImageData		= (tcu::IVec4*)(resultBuffer->getAllocation().getHostPtr());
	auto									resultImageBounds	= makeStdBeginEnd(resultImageData, (m_params.width * m_params.height));
	const std::vector<tcu::IVec4>			resultImage			(resultImageBounds.first, resultImageBounds.second);
	std::vector<tcu::IVec4>					referenceImage		(m_params.width * m_params.height);

	const std::vector<ShaderRecordEntry>	shaderBindingTable	= prepareShaderBindingTable();

	MissShader								missShader{};

	// perform offline ray-tracing
	for (deUint32 glLaunchIdExtY = 0; glLaunchIdExtY < m_params.height; ++glLaunchIdExtY)
	{
		for (deUint32 glLaunchIdExtX = 0; glLaunchIdExtX < m_params.width; ++glLaunchIdExtX)
		{
			travelRay(referenceImage, glLaunchIdExtX, glLaunchIdExtY,
					 shaderBindingTable, missShader,
					 triangleGeometries, boxGeometries);
		}
	}

#ifdef INTERNAL_DEBUG
	std::cout << "===== RES =====" << std::endl;
	printImage(resultImageData);
	std::cout << std::endl;
	std::cout << "===== REF =====" << std::endl;
	printImage(referenceImage.data());
	std::cout << std::endl;
#endif

	const tcu::IVec4								floodColor(0, '*', 0, 0);
	std::vector<std::pair<tcu::IVec4, tcu::IVec2>>	auxBuffer(referenceImage.size() * 4);

	if (includeTriangles)
	{
		TriGeometry								tri;
		std::function<bool(const tcu::Vec3&)>	pointInGeometry = std::bind(pointInTriangle2D, std::placeholders::_1, std::ref(tri[0]), std::ref(tri[1]), std::ref(tri[2]));

		for (deUint32 instance = 0; instance < m_params.instCount; ++instance)
		{
			for (deUint32 geometryIndex = 0; geometryIndex < m_params.geomCount; ++geometryIndex)
			{
				if (!(m_params.ahit() && (geometryIndex % 2 == 1)))
				{
					tri = triangleGeometries[instance * m_params.geomCount + geometryIndex];
					const tcu::Vec2 center((tri[0].x() + tri[1].x() + tri[2].x()) / 3.0f, (tri[0].y() + tri[1].y() + tri[2].y()) / 3.0f);

					const tcu::IVec2	refImageCoords	= rayToImage(center);
					const tcu::IVec4	requiredColor	= referenceImage[refImageCoords.y() * m_params.width + refImageCoords.x()];

					deUint32 resultPixelCount = computeSamePixelCount(resultImage, center, requiredColor, floodColor, pointInGeometry, auxBuffer);
					deUint32 referencePixelCount = computeSamePixelCount(referenceImage, center, requiredColor, floodColor, pointInGeometry, auxBuffer);

					if (!resultPixelCount || !referencePixelCount) return false;
					if (resultPixelCount > referencePixelCount) std::swap(resultPixelCount, referencePixelCount);

					const float similarity = float(resultPixelCount) / float(referencePixelCount);
					if (similarity < m_params.accuracy) return false;
				}
			}
		}
	}

	if (includeBoxes)
	{
		BoxGeometry								box;
		std::function<bool(const tcu::Vec3&)>	pointInGeometry = std::bind(pointInRect2D, std::placeholders::_1, std::ref(box[0]), std::ref(box[1]));

		for (deUint32 instance = 0; instance < m_params.instCount; ++instance)
		{
			for (deUint32 geometryIndex = 0; geometryIndex < m_params.geomCount; ++geometryIndex)
			{
				if (!(m_params.ahit() && (geometryIndex % 2 == 1)))
				{
					box = boxGeometries[instance * m_params.geomCount + geometryIndex];
					const tcu::Vec2 center((box[0].x() + box[1].x()) / 2.0f, (box[0].y() + box[1].y()) / 2.0f);

					const tcu::IVec2	refImageCoords = rayToImage(center);
					const tcu::IVec4	requiredColor = referenceImage[refImageCoords.y() * m_params.width + refImageCoords.x()];

					deUint32 resultPixelCount = computeSamePixelCount(resultImage, center, requiredColor, floodColor, pointInGeometry, auxBuffer);
					deUint32 referencePixelCount = computeSamePixelCount(referenceImage, center, requiredColor, floodColor, pointInGeometry, auxBuffer);

					if (!resultPixelCount || !referencePixelCount) return false;
					if (resultPixelCount > referencePixelCount) std::swap(resultPixelCount, referencePixelCount);

					const float similarity = float(resultPixelCount) / float(referencePixelCount);
					if (similarity < m_params.accuracy) return false;
				}
			}
		}
	}

	return true;
}

tcu::TestStatus PipelineFlagsInstance::iterate(void)
{
	const DeviceInterface& vkd = m_context.getDeviceInterface();
	const VkDevice							device = m_context.getDevice();
	const deUint32							familyIndex = m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue = m_context.getUniversalQueue();
	Allocator& allocator = m_context.getDefaultAllocator();

	const VkImageCreateInfo					imageCreateInfo = makeImageCreateInfo();
	const VkImageSubresourceRange			imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>		image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>					imageView = makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, m_format, imageSubresourceRange);
	const VkDescriptorImageInfo				descriptorImageInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const deUint32							resultBufferSize = (m_params.width * m_params.height * mapVkFormat(m_format).getPixelSize());
	const VkBufferCreateInfo				resultBufferCreateInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers			resultBufferImageSubresourceLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy					resultBufferImageRegion = makeBufferImageCopy(makeExtent3D(m_params.width, m_params.height, 1u), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>			resultBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const Move<VkDescriptorSetLayout>		descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
		.build(vkd, device);
	const Move<VkDescriptorPool>			descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>				descriptorSet = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

	de::MovePtr<RayTracingTestPipeline>		rayTracingPipeline = de::newMovePtr<RayTracingTestPipeline>(std::ref(m_context), std::cref(*this), m_params);
	const Move<VkPipelineLayout>			pipelineLayout = makePipelineLayout(vkd, device, *descriptorSetLayout);
	Move<VkPipeline>						pipeline = rayTracingPipeline->createPipeline(*pipelineLayout);

	de::SharedPtr<BufferWithMemory>			raygenShaderBindingTable;
	VkStridedDeviceAddressRegionKHR			raygenShaderBindingTableRegion;
	std::tie(raygenShaderBindingTable, raygenShaderBindingTableRegion) = rayTracingPipeline->createRaygenShaderBindingTable(*pipeline);

	de::SharedPtr<BufferWithMemory>			missShaderBindingTable;
	VkStridedDeviceAddressRegionKHR			missShaderBindingTableRegion;
	std::tie(missShaderBindingTable, missShaderBindingTableRegion) = rayTracingPipeline->createMissShaderBindingTable(*pipeline);

	de::SharedPtr<BufferWithMemory>			hitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR			hitShaderBindingTableRegion;
	std::tie(hitShaderBindingTable, hitShaderBindingTableRegion) = rayTracingPipeline->createHitShaderBindingTable(*pipeline);

	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const Move<VkCommandPool>				cmdPool = createCommandPool(vkd, device, 0, familyIndex);
	const Move<VkCommandBuffer>				cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vkd, *cmdBuffer);

	BottomLevelASPtrs						blasPtrs = createBottomLevelAccelerationStructs(*cmdBuffer);
	TopLevelASPtr							tlasPtr = createTopLevelAccelerationStruct(*cmdBuffer, blasPtrs);

	VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		tlasPtr->getPtr()													//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

	const VkImageSubresourceRange	subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageMemoryBarrier				imageMemoryBarrier = makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, image->get(), subresourceRange);
	cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &imageMemoryBarrier);

	cmdTraceRays(vkd,
		*cmdBuffer,
		&raygenShaderBindingTableRegion,	// rgen
		&missShaderBindingTableRegion,		// miss
		&hitShaderBindingTableRegion,		// hit
		&callableShaderBindingTableRegion,	// call
		m_params.width, m_params.height, 1);

	const VkMemoryBarrier				postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

	vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

	cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), resultBufferSize);

	return verifyResult(resultBuffer.get()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

class NoNullShadersFlagGenerator
{
public:
	using FlagsSet = std::set<VkPipelineCreateFlags>;
	struct BitAndName {
		VkPipelineCreateFlagBits	bit;
		const char* name;
	};
	static const BitAndName	bits[4];
	static std::string	name (VkPipelineCreateFlags flags) {
		int count = 0;
		std::stringstream ss;
		for (auto begin = std::begin(bits), end = std::end(bits), i = begin; i != end; ++i) {
			if (flags & i->bit) {
				if (count++) ss << "_or_";
				ss << i->name;
			}
		}
		return count ? ss.str() : "none";
	}
	static VkPipelineCreateFlags mask (const FlagsSet& flags) {
		VkPipelineCreateFlags result{};
		for (auto f : flags) result |= f;
		return result;
	}
	NoNullShadersFlagGenerator() : m_next(0) {
		FlagsSet fs;
		for (const auto& b : bits) fs.insert(b.bit);
		combine(m_combs, fs);
	}
	void	reset() { m_next = 0; }
	bool	next(VkPipelineCreateFlags& flags) {
		if (m_next < m_combs.size()) {
			flags = mask(m_combs[m_next++]);
			return true;
		}
		return false;
	}
	void combine(std::vector<FlagsSet>& result, const FlagsSet& v) {
		if (v.empty() || result.end() != std::find(result.begin(), result.end(), v)) return;
		result.push_back(v);
		for (deUint32 i = 0; i < v.size(); ++i) {
			FlagsSet w(v);
			w.erase(std::next(w.begin(), i));
			combine(result, w);
		}
	}
private:
	size_t					m_next;
	std::vector<FlagsSet>	m_combs;
};
const NoNullShadersFlagGenerator::BitAndName	NoNullShadersFlagGenerator::bits[] = {
	{ VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR,		"any"	},
	{ VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR,	"chit"	},
	{ VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR,	"isect"	},
	{ VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR,			"miss"	},
};

} // unnamed

tcu::TestCaseGroup*	createPipelineFlagsTests (tcu::TestContext& testCtx)
{
	const deUint32 strides[] = { 3, 5 };
	const deUint32 offsets[] = { 7 };

	struct {
		bool		type;
		const char*	name;
	} const processors[] = { { false, "gpu" }, { true, "cpu" } };
	struct {
		bool		type;
		const char* name;
	} const libs[]{ { true, "use_libs" }, { false, "no_libs" } };
	struct {
		GeometryTypes	type;
		const char*		name;
	} const geometries[] = { { GeometryTypes::Triangle, "triangles" }, { GeometryTypes::Box, "boxes" }, { GeometryTypes::TriangleAndBox, "tri_and_box" } };

	NoNullShadersFlagGenerator	flagsGenerator;

	TestParams		p;
#ifdef INTERNAL_DEBUG
	p.width				= 30;
	p.height			= 8;
	p.accuracy			= 0.80f;
#else
	p.width				= 256;
	p.height			= 256;
	p.accuracy			= 0.95f;
#endif
	p.onHhost			= false;
	p.useLibs			= false;
	p.useMaintenance5	= false;
	p.flags				= 0;
	p.geomTypes			= GeometryTypes::None;
	p.instCount			= 3;
	p.geomCount			= 2;
	p.stbRecStride		= 0;
	p.stbRecOffset		= 0;

	auto group = new tcu::TestCaseGroup(testCtx, "pipeline_no_null_shaders_flag", "Pipeline NO_NULL_*_SHADER flags tests");

	for (auto& processor : processors)
	{
		auto processorGroup = new tcu::TestCaseGroup(testCtx, processor.name, "");

		for (auto& geometry : geometries)
		{
			auto geometryGroup = new tcu::TestCaseGroup(testCtx, geometry.name, "");

			for (auto& stride : strides)
			{
				auto strideGroup = new tcu::TestCaseGroup(testCtx, ("stride_" + std::to_string(stride)).c_str(), "");

				for (auto& offset : offsets)
				{
					auto offsetGroup = new tcu::TestCaseGroup(testCtx, ("offset_" + std::to_string(offset)).c_str(), "");

					for (auto& lib : libs)
					{
						auto libGroup = new tcu::TestCaseGroup(testCtx, lib.name, "");

						VkPipelineCreateFlags	flags;

						flagsGenerator.reset();

						while (flagsGenerator.next(flags))
						{
							if ((VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR & flags)
								&& (GeometryTypes::Triangle == geometry.type)) continue;

							p.onHhost		= processor.type;
							p.geomTypes		= geometry.type;
							p.stbRecStride	= stride;
							p.stbRecOffset	= offset;
							p.flags			= flags;
							p.useLibs		= lib.type;

							libGroup->addChild(new PipelineFlagsCase(testCtx, flagsGenerator.name(flags), p));
						}
						offsetGroup->addChild(libGroup);
					}
					strideGroup->addChild(offsetGroup);
				}
				geometryGroup->addChild(strideGroup);
			}
			processorGroup->addChild(geometryGroup);
		}
		group->addChild(processorGroup);
	}

	de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));

	p.onHhost			= false;
	p.geomTypes			= GeometryTypes::Box;
	p.stbRecStride		= 3;
	p.stbRecOffset		= 7;
	p.useLibs			= true;
	p.useMaintenance5	= true;

	for(const auto flag : NoNullShadersFlagGenerator::bits)
	{
		p.flags = flag.bit;
		miscGroup->addChild(new PipelineFlagsCase(testCtx, std::string(flag.name) + "_maintenance5", p));
	}

	group->addChild(miscGroup.release());

	return group;
}

}	// RayTracing
}	// vkt
