/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FallingSpellSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class FallingSpellSystem final: public FallingSpellSystemInterface
{
public:
	[[nodiscard]] std::optional<magic::falling_spell::FallingSpell>& Spell() override { return _spell; }
	[[nodiscard]] bool& SparksSeen() override { return _sparksSeen; }

private:
	std::optional<magic::falling_spell::FallingSpell> _spell;
	bool _sparksSeen {false};
};
} // namespace openblack::ecs::systems
