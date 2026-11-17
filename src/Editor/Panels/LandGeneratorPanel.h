/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Editor/EditorContext.h"

namespace openblack::editor
{

/// The tab of the land generator. Before the generator starts, the tab has the button that starts it. While the
/// generator is on, the tab shows the seed of the land, the main choices of the generator, the button that makes a
/// land from them, and the button that stops the generator.
class LandGeneratorPanel
{
public:
	void Draw(EditorContext& context) noexcept;
};

} // namespace openblack::editor
