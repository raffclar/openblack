/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Magic/Objects/FallingSpell.h"

namespace openblack::ecs::systems
{
/// The game's falling spell, made on first use with its game hooks (falling_spell::Get), and whether the running
/// film's sparks were already seen; both are kept across lands (Locator::fallingSpellSystem)
class FallingSpellSystemInterface
{
public:
	virtual ~FallingSpellSystemInterface() = default;

	/// Empty until falling_spell::Get first makes it
	[[nodiscard]] virtual std::optional<magic::falling_spell::FallingSpell>& Spell() = 0;
	/// Set once the film's sparkles were turned on for the running fall; a new fall clears it
	[[nodiscard]] virtual bool& SparksSeen() = 0;
};
} // namespace openblack::ecs::systems
