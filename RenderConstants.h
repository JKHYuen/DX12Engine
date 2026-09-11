#pragma once
#include <cstdint>

namespace RenderEnums {

// Based on DEFINE_ENUM_FLAG_OPERATORS from <winnt.h> and https://stackoverflow.com/a/69183821
#define ENUM_FLAG_OPERATORS(ENUMTYPE) \
inline constexpr ENUMTYPE operator| (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) | static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator |= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) | static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \
inline constexpr ENUMTYPE operator& (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) & static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator&= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) & static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \
inline constexpr ENUMTYPE operator~ (ENUMTYPE a) noexcept { return static_cast<ENUMTYPE>(~static_cast<std::underlying_type_t<ENUMTYPE>>(a)); } \
inline constexpr ENUMTYPE operator^ (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) ^ static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator^= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) ^ static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \

/// Note: render flags are just a convenience for now, PSOs that are needed are hard coded in PSO class constructors, ideally,
///       a more generalized solution is needed, PSO variants are all hardcoded in "xxxPSO" classes for now
//  - Only PSO flags relevant to this project are included, not an exhaustive list of PSO properties
// 	- not all flags are tied to dx12 PSO properties, some are exclusive to this project e.g. tessellation options
//  - "RenderFlags_None" represents default PSO values according to d3dx12_core.h e.g. depth enable, back cull, etc.
//    Other flags represent values that should not be default
enum RenderFlags : uint32_t {
	RenderFlags_None                = 0, 
	RenderFlags_Wireframe           = 1 << 0,
	RenderFlags_UniformTessellation = 1 << 1,
	RenderFlags_EdgeTessellation    = 1 << 2,
	RenderFlags_NoTessellation      = 1 << 3,
	RenderFlags_DepthDisable        = 1 << 4,
	RenderFlags_CullModeNone        = 1 << 5,
	RenderFlags_CullModeFront       = 1 << 6,
};
ENUM_FLAG_OPERATORS(RenderFlags);

}

namespace RenderGlobals {
	constexpr uint32_t gk_MaxPointLightCount = 3;
}
