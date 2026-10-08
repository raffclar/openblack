/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/LandBalanceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The land balance's behaviour over the map script's globals (Locator::mapScriptSystem), which hold its values; it
/// keeps none of its own. Game::LoadMap resets them for every land (land_balance::Reset)
class LandBalanceSystem final: public LandBalanceSystemInterface
{
public:
	void Reset() override;
	void Set(int index, float value) override;
	[[nodiscard]] float Get(std::size_t index) const override;
	void SetLostTownScale(float scale) override;
	[[nodiscard]] float LostTownScale() const override;
};
} // namespace openblack::ecs::systems
