/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FireSoundSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The fire crackle kept for the whole game; fire::Clear stops and empties it for every land (fire::sound::Clear)
class FireSoundSystem final: public FireSoundSystemInterface
{
public:
	[[nodiscard]] Slots& GetSlots() override;
	[[nodiscard]] float MaxDistance() const override;
	void SetMaxDistance(float distance) override;
	[[nodiscard]] Owners& GetOwners() override;

private:
	Slots _slots {};
	float _maxDistance {0.0f};
	Owners _owners;
};
} // namespace openblack::ecs::systems
