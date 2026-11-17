/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::graphics
{

struct BufferPage;

/// The elements (vertices or indices) of one buffer in a page of BufferPages: the page, the first element and the count
/// of elements. A draw binds the buffer of the page from the first element on.
struct BufferRun
{
	BufferPage* page {nullptr};
	uint32_t first {0};
	uint32_t count {0};
};

} // namespace openblack::graphics
