#pragma once
#include <cstdint>

namespace RenderEnums {

// Based DEFINE_ENUM_FLAG_OPERATORS from <winnt.h> and https://stackoverflow.com/a/69183821
#define ENUM_FLAG_OPERATORS(ENUMTYPE) \
inline constexpr ENUMTYPE operator| (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) | static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator |= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) | static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \
inline constexpr ENUMTYPE operator& (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) & static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator&= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) & static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \
inline constexpr ENUMTYPE operator~ (ENUMTYPE a) noexcept { return static_cast<ENUMTYPE>(~static_cast<std::underlying_type_t<ENUMTYPE>>(a)); } \
inline constexpr ENUMTYPE operator^ (ENUMTYPE a, ENUMTYPE b) noexcept { return static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) ^ static_cast<std::underlying_type_t<ENUMTYPE>>(b)); } \
inline ENUMTYPE& operator^= (ENUMTYPE& a, ENUMTYPE b) noexcept { a = static_cast<ENUMTYPE>(static_cast<std::underlying_type_t<ENUMTYPE>>(a) ^ static_cast<std::underlying_type_t<ENUMTYPE>>(b)); return a; } \

	// Only PSO flags relevant to this project are included
	enum RenderFlags : uint32_t {
		RenderFlags_None                = 0,
		RenderFlags_Wireframe           = 1 << 0,
		RenderFlags_UniformTessellation = 1 << 1,
		RenderFlags_EdgeTessellation    = 1 << 2,
		RenderFlags_NoTessellation      = 1 << 3,
		RenderFlags_EnableDepth         = 1 << 4,
		RenderFlags_CullModeNone        = 1 << 5,
		RenderFlags_CullModeFront       = 1 << 6,
		RenderFlags_CullModeBack        = 1 << 7,
	};
	ENUM_FLAG_OPERATORS(RenderFlags);
}