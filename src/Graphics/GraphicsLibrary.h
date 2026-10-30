/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::graphics
{

/// True from the start of the graphics library until its stop. The check does not start the library.
[[nodiscard]] bool GraphicsLibraryStarted() noexcept;

} // namespace openblack::graphics
