/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::land_avoid
{
struct State;
} // namespace openblack::land_avoid

namespace openblack::ecs::systems
{
/// The creature's walkable mask, built by land_avoid::Validate when a landscape opens (Locator::landAvoidSystem)
class LandAvoidSystemInterface
{
public:
	virtual ~LandAvoidSystemInterface() = default;

	[[nodiscard]] virtual openblack::land_avoid::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
