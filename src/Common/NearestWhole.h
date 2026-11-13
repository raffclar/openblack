/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#define OPENBLACK_NEAREST_WHOLE_SSE 1
#endif

namespace openblack
{

/// A float to the nearest whole number, a half to the even one, as the processor rounds it now.
///
/// The result is the same as std::lrint. The processor's own conversion gives it without a call into the C library,
/// which costs much more on the desktop build. A value that does not fit in an int with room to spare uses the library.
[[nodiscard]] inline int32_t NearestWhole(float value)
{
#if defined(OPENBLACK_NEAREST_WHOLE_SSE)
	constexpr float k_Fits = 1073741824.0f;
	if (value > -k_Fits && value < k_Fits)
	{
		return _mm_cvt_ss2si(_mm_set_ss(value));
	}
#endif
	return static_cast<int32_t>(std::lrint(value));
}

} // namespace openblack
