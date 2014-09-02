/* WARNING! THIS IS A PROGRAMMATICALLY GENERATED CODE. DO NOT MODIFY THE CODE,
 * SINCE THE CHANGES WILL BE LOST! MODIFY THE GENERATING PYTHON INSTEAD.
 */

const char*	getBooleanName				(int value);
const char*	getBoolDontCareName			(int value);
const char*	getAPIName					(int value);
const char*	getErrorName				(int value);
const char*	getContextAttribName		(int value);
const char*	getConfigAttribName			(int value);
const char*	getSurfaceAttribName		(int value);
const char*	getSurfaceTargetName		(int value);
const char*	getColorBufferTypeName		(int value);
const char*	getConfigCaveatName			(int value);
const char*	getTransparentTypeName		(int value);
const char*	getMultisampleResolveName	(int value);
const char*	getRenderBufferName			(int value);
const char*	getSwapBehaviorName			(int value);
const char*	getTextureFormatName		(int value);
const char*	getTextureTargetName		(int value);
const char*	getVGAlphaFormatName		(int value);
const char*	getVGColorspaceName			(int value);

tcu::Format::Bitfield<16>	getAPIBitsStr		(int value);
tcu::Format::Bitfield<16>	getSurfaceBitsStr	(int value);

inline tcu::Format::Enum	getBooleanStr				(int value)	{ return tcu::Format::Enum(getBooleanName,				value); }
inline tcu::Format::Enum	getBoolDontCareStr			(int value)	{ return tcu::Format::Enum(getBoolDontCareName,			value); }
inline tcu::Format::Enum	getAPIStr					(int value)	{ return tcu::Format::Enum(getAPIName,					value); }
inline tcu::Format::Enum	getErrorStr					(int value)	{ return tcu::Format::Enum(getErrorName,				value); }
inline tcu::Format::Enum	getContextAttribStr			(int value)	{ return tcu::Format::Enum(getContextAttribName,		value); }
inline tcu::Format::Enum	getConfigAttribStr			(int value)	{ return tcu::Format::Enum(getConfigAttribName,			value); }
inline tcu::Format::Enum	getSurfaceAttribStr			(int value)	{ return tcu::Format::Enum(getSurfaceAttribName,		value); }
inline tcu::Format::Enum	getSurfaceTargetStr			(int value)	{ return tcu::Format::Enum(getSurfaceTargetName,		value); }
inline tcu::Format::Enum	getColorBufferTypeStr		(int value)	{ return tcu::Format::Enum(getColorBufferTypeName,		value); }
inline tcu::Format::Enum	getConfigCaveatStr			(int value)	{ return tcu::Format::Enum(getConfigCaveatName,			value); }
inline tcu::Format::Enum	getTransparentTypeStr		(int value)	{ return tcu::Format::Enum(getTransparentTypeName,		value); }
inline tcu::Format::Enum	getMultisampleResolveStr	(int value)	{ return tcu::Format::Enum(getMultisampleResolveName,	value); }
inline tcu::Format::Enum	getRenderBufferStr			(int value)	{ return tcu::Format::Enum(getRenderBufferName,			value); }
inline tcu::Format::Enum	getSwapBehaviorStr			(int value)	{ return tcu::Format::Enum(getSwapBehaviorName,			value); }
inline tcu::Format::Enum	getTextureFormatStr			(int value)	{ return tcu::Format::Enum(getTextureFormatName,		value); }
inline tcu::Format::Enum	getTextureTargetStr			(int value)	{ return tcu::Format::Enum(getTextureTargetName,		value); }
inline tcu::Format::Enum	getVGAlphaFormatStr			(int value)	{ return tcu::Format::Enum(getVGAlphaFormatName,		value); }
inline tcu::Format::Enum	getVGColorspaceStr			(int value)	{ return tcu::Format::Enum(getVGColorspaceName,			value); }
