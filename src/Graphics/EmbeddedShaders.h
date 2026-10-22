/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The shader binaries that the game embeds for each renderer. Include this file, not bgfx/embedded_shader.h.

#include <bgfx/embedded_shader.h>

// bgfx can use the Direct3D of Windows through WSL on Linux. The Linux build does not use it, so it embeds no Direct3D
// shaders.
#if BX_PLATFORM_LINUX
#undef BGFX_EMBEDDED_SHADER_DXBC
#define BGFX_EMBEDDED_SHADER_DXBC(...)
#endif

// Direct3D 12 uses the same Shader Model 5 binaries as Direct3D 11, which the game used before. The Shader Model 6
// compiler gives small changes in the pictures (one step of 255 in some pixels), so the game does not use it.
#if BX_PLATFORM_WINDOWS
#undef BGFX_EMBEDDED_SHADER_DXIL
#define BGFX_EMBEDDED_SHADER_DXIL(_renderer, _name) BGFX_EMBEDDED_SHADER_DXBC(_renderer, _name)
#endif
