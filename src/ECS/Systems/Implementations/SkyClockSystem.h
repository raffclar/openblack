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

#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/Sky.h"
#include "ECS/Systems/SkyClockSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SkyClockSystem final: public SkyClockSystemInterface
{
public:
	SkyClockSystem();

	void Initialize() override;
	void KeepForNextLand() override;
	void FindAfterLoad() override;
	void ProcessTurn() override;
	void SetTime(float time) override;
	[[nodiscard]] float GetCurrentSkyType() const override;
	[[nodiscard]] float GetTime() const override { return GetClock().GetVisualTime(); }
	[[nodiscard]] DayNightTimes GetDayNightTimes() const override { return GetClock().GetDayNightTimes(); }
	[[nodiscard]] DayNightClock& GetClock() override;
	[[nodiscard]] const DayNightClock& GetClock() const override;
	[[nodiscard]] const archetypes::SkyArchetype::Entities& GetEntities() const override { return _entities; }

private:
	/// The sky's components while the registry is emptied between lands
	struct Kept
	{
		components::SkyDome dome;
		components::DayNightCycle cycle;
		components::Sun sun;
		components::Moon moon;
	};

	archetypes::SkyArchetype::Entities _entities;
	std::optional<Kept> _kept;
};

} // namespace openblack::ecs::systems
