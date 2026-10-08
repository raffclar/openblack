/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <random>
#include <vector>

#include "ECS/Systems/CreaturePhysiologySystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreaturePhysiologySystem final: public CreaturePhysiologySystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float seconds) override;
	[[nodiscard]] std::span<const PukeDrop> GetPukeDrops() const override { return _pukeDrops; }

	void Eat(entt::entity creature, float foodValue) override;
	void Drink(entt::entity creature) override;
	void Poo(entt::entity creature) override;
	void Puke(entt::entity creature) override;
	void WakeFromFaint(entt::entity creature) override;
	void FinishAction(entt::entity creature, std::string_view action) override;
	void ModifyStrength(entt::entity creature, float amount) override;

	void SetTimeScale(float scale) override;
	[[nodiscard]] float GetTimeScale() const override;
	void SetFaintingEnabled(bool enabled) override;
	[[nodiscard]] bool IsFaintingEnabled() const override;

private:
	float _timeScale {1.0f};
	/// Turns of the body owed from a fractional time scale
	float _owedTurns {0.0f};
	bool _fainting {true};
	/// The drops of sick out now. They are drawn only, so they are kept here rather than as entities.
	std::vector<PukeDrop> _pukeDrops;
	/// The sick's spray is drawn only and has no counterpart in the game's random numbers: it is scattered by a
	/// generator of its own, which no game state reads
	static constexpr uint32_t k_PukeSeed = 0x5EED;
	std::minstd_rand _pukeRandom {k_PukeSeed};
};

} // namespace openblack::ecs::systems
