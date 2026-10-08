/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

namespace openblack::ecs::systems
{
/// The game's land balance: the 8 multipliers the script's global land balance command sets, and the lost-town scale
/// (land_balance goes through it). Their values are in the map script's globals (landBalance, lostTownScale)
class LandBalanceSystemInterface
{
public:
	virtual ~LandBalanceSystemInterface() = default;

	/// Every multiplier and the lost-town scale back to 1
	virtual void Reset() = 0;
	/// Out of range: nothing
	virtual void Set(int index, float value) = 0;
	/// Out of range: 1
	[[nodiscard]] virtual float Get(std::size_t index) const = 0;
	virtual void SetLostTownScale(float scale) = 0;
	[[nodiscard]] virtual float LostTownScale() const = 0;
};
} // namespace openblack::ecs::systems
