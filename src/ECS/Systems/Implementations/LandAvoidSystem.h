/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/LandAvoidState.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the LandAvoid state, as the game starts it
class LandAvoidSystem final: public LandAvoidSystemInterface
{
public:
	[[nodiscard]] openblack::land_avoid::State& GetState() noexcept override { return _state; }

private:
	openblack::land_avoid::State _state;
};
} // namespace openblack::ecs::systems
