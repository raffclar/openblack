/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "ECS/Systems/LandBalanceSystemInterface.h"
#include "LandBalance.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The land balance kept for the whole game; Game::LoadMap resets it for every land (land_balance::Reset)
class LandBalanceSystem final: public LandBalanceSystemInterface
{
public:
	void Reset() override;
	void Set(int index, float value) override;
	[[nodiscard]] float Get(std::size_t index) const override;
	void SetLostTownScale(float scale) override;
	[[nodiscard]] float LostTownScale() const override;

private:
	std::array<float, land_balance::k_Count> _values {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	float _lostTownScale {1.0f};
};
} // namespace openblack::ecs::systems
