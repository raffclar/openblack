/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/AnimalAISystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The animals' shared state, kept for the whole game (never cleared)
class AnimalAISystem final: public AnimalAISystemInterface
{
public:
	[[nodiscard]] float VisualTime() const override;
	void SetVisualTime(float hours) override;

	[[nodiscard]] uint32_t AddDeathListener(DeathCallback callback) override;
	void RemoveDeathListener(uint32_t id) override;
	[[nodiscard]] const DeathListeners& GetDeathListeners() const override;
	[[nodiscard]] uint32_t SingleSlotId() const override;
	void SetSingleSlotId(uint32_t id) override;

	void SetSpeciesDying(std::size_t species, DeathCallback dying) override;
	[[nodiscard]] const DeathCallback* SpeciesDying(std::size_t species) const override;

private:
	float _visualTime {12.0f};
	DeathListeners _deathListeners;
	uint32_t _nextListenerId {1};
	uint32_t _singleSlotId {0};
	std::vector<DeathCallback> _speciesDying;
};
} // namespace openblack::ecs::systems
