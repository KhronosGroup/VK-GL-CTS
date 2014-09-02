/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
const char*	getErrorName							(int value);
const char*	getTypeName								(int value);
const char*	getParamQueryName						(int value);
const char*	getProgramParamName						(int value);
const char*	getUniformParamName						(int value);
const char*	getFramebufferAttachmentName			(int value);
const char*	getFramebufferAttachmentParameterName	(int value);
const char*	getFramebufferTargetName				(int value);
const char*	getFramebufferStatusName				(int value);
const char*	getFramebufferAttachmentTypeName		(int value);
const char*	getFramebufferColorEncodingName			(int value);
const char*	getFramebufferParameterName				(int value);
const char*	getRenderbufferParameterName			(int value);
const char*	getPrimitiveTypeName					(int value);
const char*	getBlendFactorName						(int value);
const char*	getBlendEquationName					(int value);
const char*	getBufferTargetName						(int value);
const char*	getBufferBindingName					(int value);
const char*	getUsageName							(int value);
const char*	getBufferQueryName						(int value);
const char*	getFaceName								(int value);
const char*	getCompareFuncName						(int value);
const char*	getEnableCapName						(int value);
const char*	getWindingName							(int value);
const char*	getHintModeName							(int value);
const char*	getHintName								(int value);
const char*	getStencilOpName						(int value);
const char*	getShaderTypeName						(int value);
const char*	getBufferName							(int value);
const char*	getInvalidateAttachmentName				(int value);
const char*	getDrawReadBufferName					(int value);
const char*	getTextureTargetName					(int value);
const char*	getTextureParameterName					(int value);
const char*	getTextureLevelParameterName			(int value);
const char*	getRepeatModeName						(int value);
const char*	getTextureFilterName					(int value);
const char*	getTextureWrapModeName					(int value);
const char*	getTextureSwizzleName					(int value);
const char*	getTextureCompareModeName				(int value);
const char*	getCubeMapFaceName						(int value);
const char*	getPixelStoreParameterName				(int value);
const char*	getPixelFormatName						(int value);
const char*	getCompressedTexFormatName				(int value);
const char*	getShaderVarTypeName					(int value);
const char*	getShaderParamName						(int value);
const char*	getVertexAttribParameterNameName		(int value);
const char*	getBooleanName							(int value);
const char*	getGettableStateName					(int value);
const char*	getGettableIndexedStateName				(int value);
const char*	getGettableStringName					(int value);
const char*	getInternalFormatParameterName			(int value);
const char*	getInternalFormatTargetName				(int value);
const char*	getMultisampleParameterName				(int value);
const char*	getQueryTargetName						(int value);
const char*	getQueryParamName						(int value);
const char*	getQueryObjectParamName					(int value);
const char*	getImageAccessName						(int value);
const char*	getProgramInterfaceName					(int value);
const char*	getProgramResourcePropertyName			(int value);
const char*	getPrecisionFormatTypeName				(int value);
const char*	getTransformFeedbackTargetName			(int value);
const char*	getProvokingVertexName					(int value);
const char*	getDebugMessageSourceName				(int value);
const char*	getDebugMessageTypeName					(int value);
const char*	getDebugMessageSeverityName				(int value);
const char*	getPipelineParamName					(int value);

tcu::Format::Bitfield<16>	getBufferMaskStr			(int value);
tcu::Format::Bitfield<16>	getBufferMapFlagsStr		(int value);
tcu::Format::Bitfield<16>	getMemoryBarrierFlagsStr	(int value);

inline tcu::Format::Enum	getErrorStr								(int value)	{ return tcu::Format::Enum(getErrorName,							value); }
inline tcu::Format::Enum	getTypeStr								(int value)	{ return tcu::Format::Enum(getTypeName,								value); }
inline tcu::Format::Enum	getParamQueryStr						(int value)	{ return tcu::Format::Enum(getParamQueryName,						value); }
inline tcu::Format::Enum	getProgramParamStr						(int value)	{ return tcu::Format::Enum(getProgramParamName,						value); }
inline tcu::Format::Enum	getUniformParamStr						(int value)	{ return tcu::Format::Enum(getUniformParamName,						value); }
inline tcu::Format::Enum	getFramebufferAttachmentStr				(int value)	{ return tcu::Format::Enum(getFramebufferAttachmentName,			value); }
inline tcu::Format::Enum	getFramebufferAttachmentParameterStr	(int value)	{ return tcu::Format::Enum(getFramebufferAttachmentParameterName,	value); }
inline tcu::Format::Enum	getFramebufferTargetStr					(int value)	{ return tcu::Format::Enum(getFramebufferTargetName,				value); }
inline tcu::Format::Enum	getFramebufferStatusStr					(int value)	{ return tcu::Format::Enum(getFramebufferStatusName,				value); }
inline tcu::Format::Enum	getFramebufferAttachmentTypeStr			(int value)	{ return tcu::Format::Enum(getFramebufferAttachmentTypeName,		value); }
inline tcu::Format::Enum	getFramebufferColorEncodingStr			(int value)	{ return tcu::Format::Enum(getFramebufferColorEncodingName,			value); }
inline tcu::Format::Enum	getFramebufferParameterStr				(int value)	{ return tcu::Format::Enum(getFramebufferParameterName,				value); }
inline tcu::Format::Enum	getRenderbufferParameterStr				(int value)	{ return tcu::Format::Enum(getRenderbufferParameterName,			value); }
inline tcu::Format::Enum	getPrimitiveTypeStr						(int value)	{ return tcu::Format::Enum(getPrimitiveTypeName,					value); }
inline tcu::Format::Enum	getBlendFactorStr						(int value)	{ return tcu::Format::Enum(getBlendFactorName,						value); }
inline tcu::Format::Enum	getBlendEquationStr						(int value)	{ return tcu::Format::Enum(getBlendEquationName,					value); }
inline tcu::Format::Enum	getBufferTargetStr						(int value)	{ return tcu::Format::Enum(getBufferTargetName,						value); }
inline tcu::Format::Enum	getBufferBindingStr						(int value)	{ return tcu::Format::Enum(getBufferBindingName,					value); }
inline tcu::Format::Enum	getUsageStr								(int value)	{ return tcu::Format::Enum(getUsageName,							value); }
inline tcu::Format::Enum	getBufferQueryStr						(int value)	{ return tcu::Format::Enum(getBufferQueryName,						value); }
inline tcu::Format::Enum	getFaceStr								(int value)	{ return tcu::Format::Enum(getFaceName,								value); }
inline tcu::Format::Enum	getCompareFuncStr						(int value)	{ return tcu::Format::Enum(getCompareFuncName,						value); }
inline tcu::Format::Enum	getEnableCapStr							(int value)	{ return tcu::Format::Enum(getEnableCapName,						value); }
inline tcu::Format::Enum	getWindingStr							(int value)	{ return tcu::Format::Enum(getWindingName,							value); }
inline tcu::Format::Enum	getHintModeStr							(int value)	{ return tcu::Format::Enum(getHintModeName,							value); }
inline tcu::Format::Enum	getHintStr								(int value)	{ return tcu::Format::Enum(getHintName,								value); }
inline tcu::Format::Enum	getStencilOpStr							(int value)	{ return tcu::Format::Enum(getStencilOpName,						value); }
inline tcu::Format::Enum	getShaderTypeStr						(int value)	{ return tcu::Format::Enum(getShaderTypeName,						value); }
inline tcu::Format::Enum	getBufferStr							(int value)	{ return tcu::Format::Enum(getBufferName,							value); }
inline tcu::Format::Enum	getInvalidateAttachmentStr				(int value)	{ return tcu::Format::Enum(getInvalidateAttachmentName,				value); }
inline tcu::Format::Enum	getDrawReadBufferStr					(int value)	{ return tcu::Format::Enum(getDrawReadBufferName,					value); }
inline tcu::Format::Enum	getTextureTargetStr						(int value)	{ return tcu::Format::Enum(getTextureTargetName,					value); }
inline tcu::Format::Enum	getTextureParameterStr					(int value)	{ return tcu::Format::Enum(getTextureParameterName,					value); }
inline tcu::Format::Enum	getTextureLevelParameterStr				(int value)	{ return tcu::Format::Enum(getTextureLevelParameterName,			value); }
inline tcu::Format::Enum	getRepeatModeStr						(int value)	{ return tcu::Format::Enum(getRepeatModeName,						value); }
inline tcu::Format::Enum	getTextureFilterStr						(int value)	{ return tcu::Format::Enum(getTextureFilterName,					value); }
inline tcu::Format::Enum	getTextureWrapModeStr					(int value)	{ return tcu::Format::Enum(getTextureWrapModeName,					value); }
inline tcu::Format::Enum	getTextureSwizzleStr					(int value)	{ return tcu::Format::Enum(getTextureSwizzleName,					value); }
inline tcu::Format::Enum	getTextureCompareModeStr				(int value)	{ return tcu::Format::Enum(getTextureCompareModeName,				value); }
inline tcu::Format::Enum	getCubeMapFaceStr						(int value)	{ return tcu::Format::Enum(getCubeMapFaceName,						value); }
inline tcu::Format::Enum	getPixelStoreParameterStr				(int value)	{ return tcu::Format::Enum(getPixelStoreParameterName,				value); }
inline tcu::Format::Enum	getPixelFormatStr						(int value)	{ return tcu::Format::Enum(getPixelFormatName,						value); }
inline tcu::Format::Enum	getCompressedTexFormatStr				(int value)	{ return tcu::Format::Enum(getCompressedTexFormatName,				value); }
inline tcu::Format::Enum	getShaderVarTypeStr						(int value)	{ return tcu::Format::Enum(getShaderVarTypeName,					value); }
inline tcu::Format::Enum	getShaderParamStr						(int value)	{ return tcu::Format::Enum(getShaderParamName,						value); }
inline tcu::Format::Enum	getVertexAttribParameterNameStr			(int value)	{ return tcu::Format::Enum(getVertexAttribParameterNameName,		value); }
inline tcu::Format::Enum	getBooleanStr							(int value)	{ return tcu::Format::Enum(getBooleanName,							value); }
inline tcu::Format::Enum	getGettableStateStr						(int value)	{ return tcu::Format::Enum(getGettableStateName,					value); }
inline tcu::Format::Enum	getGettableIndexedStateStr				(int value)	{ return tcu::Format::Enum(getGettableIndexedStateName,				value); }
inline tcu::Format::Enum	getGettableStringStr					(int value)	{ return tcu::Format::Enum(getGettableStringName,					value); }
inline tcu::Format::Enum	getInternalFormatParameterStr			(int value)	{ return tcu::Format::Enum(getInternalFormatParameterName,			value); }
inline tcu::Format::Enum	getInternalFormatTargetStr				(int value)	{ return tcu::Format::Enum(getInternalFormatTargetName,				value); }
inline tcu::Format::Enum	getMultisampleParameterStr				(int value)	{ return tcu::Format::Enum(getMultisampleParameterName,				value); }
inline tcu::Format::Enum	getQueryTargetStr						(int value)	{ return tcu::Format::Enum(getQueryTargetName,						value); }
inline tcu::Format::Enum	getQueryParamStr						(int value)	{ return tcu::Format::Enum(getQueryParamName,						value); }
inline tcu::Format::Enum	getQueryObjectParamStr					(int value)	{ return tcu::Format::Enum(getQueryObjectParamName,					value); }
inline tcu::Format::Enum	getImageAccessStr						(int value)	{ return tcu::Format::Enum(getImageAccessName,						value); }
inline tcu::Format::Enum	getProgramInterfaceStr					(int value)	{ return tcu::Format::Enum(getProgramInterfaceName,					value); }
inline tcu::Format::Enum	getProgramResourcePropertyStr			(int value)	{ return tcu::Format::Enum(getProgramResourcePropertyName,			value); }
inline tcu::Format::Enum	getPrecisionFormatTypeStr				(int value)	{ return tcu::Format::Enum(getPrecisionFormatTypeName,				value); }
inline tcu::Format::Enum	getTransformFeedbackTargetStr			(int value)	{ return tcu::Format::Enum(getTransformFeedbackTargetName,			value); }
inline tcu::Format::Enum	getProvokingVertexStr					(int value)	{ return tcu::Format::Enum(getProvokingVertexName,					value); }
inline tcu::Format::Enum	getDebugMessageSourceStr				(int value)	{ return tcu::Format::Enum(getDebugMessageSourceName,				value); }
inline tcu::Format::Enum	getDebugMessageTypeStr					(int value)	{ return tcu::Format::Enum(getDebugMessageTypeName,					value); }
inline tcu::Format::Enum	getDebugMessageSeverityStr				(int value)	{ return tcu::Format::Enum(getDebugMessageSeverityName,				value); }
inline tcu::Format::Enum	getPipelineParamStr						(int value)	{ return tcu::Format::Enum(getPipelineParamName,					value); }
