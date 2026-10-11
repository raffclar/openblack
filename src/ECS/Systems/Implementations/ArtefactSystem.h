/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/ArtefactSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct TownArtefact;
}

namespace openblack::ecs::systems
{

class ArtefactSystem final: public ArtefactSystemInterface
{
public:
	void AddToTown(entt::entity object, entt::entity town, PlayerNames player) override;
	void AddToSite(entt::entity object, entt::entity site, PlayerNames player) override;
	void Taken(entt::entity object, PlayerNames player) override;
	void ProcessTurn() override;
	bool OfferRelaxation(entt::entity town, entt::entity villager) override;
	void PowerUpSite(entt::entity site, uint32_t dancers) override;
	void UpdateFrame(float gameMilliseconds) override;
	bool SetWorth(entt::entity object, float worth) override;
	[[nodiscard]] uint32_t CountFor(PlayerNames player) const override;
	[[nodiscard]] float ImpressiveModifier(entt::entity object) const override;

private:
	/// A town's artefacts, newest first
	[[nodiscard]] std::vector<entt::entity> ArtefactsOf(entt::entity town) const;
	/// A town's turn of one of its artefacts
	void Process(entt::entity object, components::TownArtefact& artefact);
	/// The dance is over, or made to end: the worth its dancers raised, and where they go
	void StopDanceIfNecessary(entt::entity object, components::TownArtefact& artefact, bool force);
	/// While its dance is on in a town that isn't its owner's, the town gives the owner belief now and then
	void GiveBeliefIfNecessary(entt::entity object, components::TownArtefact& artefact);
	/// A villager's wish to join the artefact's dance
	[[nodiscard]] float JoiningDesire(entt::entity object, const components::TownArtefact& artefact,
	                                  entt::entity villager) const;
	/// The villager joins the artefact's dance, made if need be, and waits for it to start. Whether it joined.
	bool JoinDance(entt::entity object, components::TownArtefact& artefact, entt::entity villager);
	/// Its worth is set: it glows once worth enough, as strongly as it is worth
	void SetValue(entt::entity object, components::TownArtefact& artefact, float value);
	/// It starts glowing, and its owner's symbol is made, once it is worth enough and has no glow
	void CreateEffectIfNecessary(entt::entity object, components::TownArtefact& artefact);
	/// It leaves its town's and site's artefacts: a dance under way sends its dancers back to their lives
	void Remove(components::TownArtefact& artefact);
	/// Every villager of the dance goes into a state
	void SetDancersState(entt::entity dance, VillagerStates state) const;

	/// How many artefacts have joined towns, to keep each town's newest first
	uint64_t _joined {0};
};

} // namespace openblack::ecs::systems
