#ifndef _VKTTRANSFORMFEEDBACKFUZZLAYOUTCASE_HPP
#define _VKTTRANSFORMFEEDBACKFUZZLAYOUTCASE_HPP
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
 * \brief Vulkan Transform Feedback Fuzz Layout Tests
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "vktTestCase.hpp"
#include "tcuDefs.hpp"
#include "gluShaderUtil.hpp"

#include <map>

namespace vkt
{
namespace TransformFeedback
{

// Interface block details.

enum InterfaceFlags
{
	PRECISION_LOW			= (1<<0),
	PRECISION_MEDIUM		= (1<<1),
	PRECISION_HIGH			= (1<<2),
	PRECISION_MASK			= PRECISION_LOW|PRECISION_MEDIUM|PRECISION_HIGH,

	LAYOUT_XFBBUFFER		= (1<<3),
	LAYOUT_XFBOFFSET		= (1<<4),
	LAYOUT_XFBSTRIDE		= (1<<5),
	LAYOUT_MASK				= LAYOUT_XFBBUFFER|LAYOUT_XFBOFFSET|LAYOUT_XFBSTRIDE,

	FIELD_UNASSIGNED		= (1<<6),	//!< Interface or struct member is not used in shader.
	FIELD_MISSING			= (1<<7),	//!< Interface or struct member will be commented out in shader.
	FIELD_OPTIONS			= FIELD_UNASSIGNED|FIELD_MISSING,
};

enum MatrixLoadFlags
{
	LOAD_FULL_MATRIX		= 0,
	LOAD_MATRIX_COMPONENTS	= 1,
};

enum TestStageFlags
{
	TEST_STAGE_VERTEX	= 0,
	TEST_STAGE_GEOMETRY	= 1,
};

class StructType;

class VarType
{
public:
						VarType			(void);
						VarType			(const VarType& other);
						VarType			(glu::DataType basicType, deUint32 flags);
						VarType			(const VarType& elementType, int arraySize);
	explicit			VarType			(const StructType* structPtr, deUint32 flags = 0u);
						~VarType		(void);

	bool				isBasicType		(void) const	{ return m_type == TYPE_BASIC;	}
	bool				isArrayType		(void) const	{ return m_type == TYPE_ARRAY;	}
	bool				isStructType	(void) const	{ return m_type == TYPE_STRUCT;	}

	deUint32			getFlags		(void) const	{ return m_flags;					}
	glu::DataType		getBasicType	(void) const	{ return m_data.basicType;			}

	const VarType&		getElementType	(void) const	{ return *m_data.array.elementType;	}
	int					getArraySize	(void) const	{ return m_data.array.size;			}

	const StructType&	getStruct		(void) const	{ return *m_data.structPtr;			}

	VarType&			operator=		(const VarType& other);

private:
	enum Type
	{
		TYPE_BASIC,
		TYPE_ARRAY,
		TYPE_STRUCT,

		TYPE_LAST
	};

	Type				m_type;
	deUint32			m_flags;
	union Data
	{
		glu::DataType		basicType;
		struct
		{
			VarType*		elementType;
			int				size;
		} array;
		const StructType*	structPtr;

		Data (void)
		{
			array.elementType	= DE_NULL;
			array.size			= 0;
		}
	} m_data;
};

class StructMember
{
public:
						StructMember	(const std::string& name, const VarType& type, deUint32 flags)
							: m_name(name)
							, m_type(type)
							, m_flags(flags)
						{}

						StructMember	(void)
							: m_flags(0)
						{}

	const std::string&	getName			(void) const { return m_name;	}
	const VarType&		getType			(void) const { return m_type;	}
	deUint32			getFlags		(void) const { return m_flags;	}

private:
	std::string			m_name;
	VarType				m_type;
	deUint32			m_flags;
};

class StructType
{
public:
	typedef std::vector<StructMember>::iterator			Iterator;
	typedef std::vector<StructMember>::const_iterator	ConstIterator;

								StructType		(const std::string& typeName) : m_typeName(typeName) {}
								~StructType		(void) {}

	const std::string&			getTypeName		(void) const	{ return m_typeName;			}
	bool						hasTypeName		(void) const	{ return !m_typeName.empty();	}

	inline Iterator				begin			(void)			{ return m_members.begin();		}
	inline ConstIterator		begin			(void) const	{ return m_members.begin();		}
	inline Iterator				end				(void)			{ return m_members.end();		}
	inline ConstIterator		end				(void) const	{ return m_members.end();		}

	void						addMember		(const std::string& name, const VarType& type, deUint32 flags = 0);

private:
	std::string					m_typeName;
	std::vector<StructMember>	m_members;
};

class InterfaceBlockMember
{
public:
						InterfaceBlockMember	(const std::string& name, const VarType& type, deUint32 flags = 0);

	const std::string&	getName					(void) const { return m_name;	}
	const VarType&		getType					(void) const { return m_type;	}
	deUint32			getFlags				(void) const { return m_flags;	}

private:
	std::string			m_name;
	VarType				m_type;
	deUint32			m_flags;
};

class InterfaceBlock
{
public:
	typedef std::vector<InterfaceBlockMember>::iterator			Iterator;
	typedef std::vector<InterfaceBlockMember>::const_iterator	ConstIterator;

										InterfaceBlock		(const std::string& blockName);

	const std::string&					getBlockName		(void) const { return m_blockName;				}
	bool								hasInstanceName		(void) const { return !m_instanceName.empty();	}
	const std::string&					getInstanceName		(void) const { return m_instanceName;			}
	bool								isArray				(void) const { return m_arraySize > 0;			}
	int									getArraySize		(void) const { return m_arraySize;				}
	int									getXfbBuffer		(void) const { return m_xfbBuffer;				}
	deUint32							getFlags			(void) const { return m_flags;					}

	void								setInstanceName		(const std::string& name)							{ m_instanceName = name;						}
	void								setFlags			(deUint32 flags)									{ m_flags = flags;								}
	void								setFlag				(deUint32 flag)										{ m_flags |= flag;								}
	void								setArraySize		(int arraySize)										{ m_arraySize = arraySize;						}
	void								setXfbBuffer		(int xfbBuffer)										{ m_xfbBuffer = xfbBuffer;						}
	void								addInterfaceMember	(const InterfaceBlockMember& interfaceBlockMember)	{ m_members.push_back(interfaceBlockMember);	}

	inline Iterator						begin				(void)			{ return m_members.begin();		}
	inline ConstIterator				begin				(void) const	{ return m_members.begin();		}
	inline Iterator						end					(void)			{ return m_members.end();		}
	inline ConstIterator				end					(void) const	{ return m_members.end();		}

private:
	std::string							m_blockName;
	std::string							m_instanceName;
	std::vector<InterfaceBlockMember>	m_members;
	int									m_xfbBuffer;
	int									m_arraySize;	//!< Array size or 0 if not interface block array.
	deUint32							m_flags;
};

typedef de::SharedPtr<StructType>		StructTypeSP;
typedef de::SharedPtr<InterfaceBlock>	InterfaceBlockSP;

class ShaderInterface
{
public:
									ShaderInterface				(void);
									~ShaderInterface			(void);

	StructType&						allocStruct					(const std::string& name);
	void							getNamedStructs				(std::vector<const StructType*>& structs) const;

	InterfaceBlock&					allocBlock					(const std::string& name);

	int								getNumInterfaceBlocks		(void) const	{ return (int)m_interfaceBlocks.size();	}
	const InterfaceBlock&			getInterfaceBlock			(int ndx) const	{ return *m_interfaceBlocks[ndx];		}
	InterfaceBlock&					getInterfaceBlockForModify	(int ndx)		{ return *m_interfaceBlocks[ndx];		}

private:
	std::vector<StructTypeSP>		m_structs;
	std::vector<InterfaceBlockSP>	m_interfaceBlocks;
};

struct BlockLayoutEntry
{
	BlockLayoutEntry (void)
		: xfbBuffer				(-1)
		, xfbOffset				(-1)
		, xfbSize				(0)
		, xfbStride				(0)
		, blockDeclarationNdx	(-1)
		, instanceNdx			(-1)
		, locationNdx			(-1)
		, locationSize			(-1)
	{
	}

	std::string			name;
	int					xfbBuffer;
	int					xfbOffset;
	int					xfbSize;
	int					xfbStride;
	std::vector<int>	activeInterfaceIndices;
	int					blockDeclarationNdx;
	int					instanceNdx;
	// Location are not used for transform feedback, but they must be not overlap to pass GLSL compiler
	int					locationNdx;
	int					locationSize;
};

struct InterfaceLayoutEntry
{
	InterfaceLayoutEntry (void)
		: type			(glu::TYPE_LAST)
		, arraySize		(0)
		, blockLayoutNdx(-1)
		, offset		(-1)
		, arrayStride	(-1)
		, matrixStride	(-1)
		, instanceNdx	(0)
		, locationNdx	(-1)
		, validate		(true)
	{
	}

	std::string			name;
	glu::DataType		type;
	int					arraySize;
	int					blockLayoutNdx;
	int					offset;
	int					arrayStride;
	int					matrixStride;
	int					instanceNdx;
	// Location are not used for transform feedback, but they must be not overlap to pass GLSL compiler
	int					locationNdx;
	bool				validate;
};

class InterfaceLayout
{
public:
	std::vector<BlockLayoutEntry>		blocks;
	std::vector<InterfaceLayoutEntry>	interfaces;

	int									getInterfaceLayoutIndex	(int blockDeclarationNdx, const std::string& name) const;
	int									getBlockLayoutIndex		(int blockDeclarationNdx, int instanceNdx) const;
};

typedef std::vector<vk::VkDeviceSize> DeviceSizeVector;

class InterfaceBlockCase : public vkt::TestCase
{
public:
							InterfaceBlockCase			(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 MatrixLoadFlags		matrixLoadFlag,
														 TestStageFlags			testStageFlag,
														 bool					shuffleInterfaceMembers = false);
							~InterfaceBlockCase			(void);

	virtual void			delayedInit					(void);
	virtual	void			initPrograms				(vk::SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance				(Context&				context) const;

protected:
	ShaderInterface			m_interface;
	MatrixLoadFlags			m_matrixLoadFlag;
	TestStageFlags			m_testStageFlags;
	bool					m_shuffleInterfaceMembers;	//!< Used with explicit offsets to test out of order member offsets
	deUint32				m_locationsRequired;

private:
	std::string				m_vertShaderSource;
	std::string				m_geomShaderSource;

	std::vector<deUint8>	m_data;						//!< Data.
	DeviceSizeVector		m_tfBufBindingSizes;
	DeviceSizeVector		m_tfBufBindingOffsets;
	std::map<int, void*>	m_blockPointers;			//!< Reference block pointers.
	InterfaceLayout			m_interfaceLayout;			//!< interface layout.
};

} // TransformFeedback
} // vkt

#endif // _VKTTRANSFORMFEEDBACKFUZZLAYOUTCASE_HPP
