/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorshipSiteWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Dance;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class WorshipSiteSystem final: public WorshipSiteSystemInterface
{
public:
	/// On the game's world
	WorshipSiteSystem();
	explicit WorshipSiteSystem(std::unique_ptr<worship_site::WorldInterface> world);
	~WorshipSiteSystem() override;

	void AddTemple(entt::entity temple, float facing, bool standing) override;
	void TempleBuilt(entt::entity temple) override;
	void PersonJoinedTown(entt::entity town) override;
	void TownChangedHands(entt::entity town) override;
	void LandLaidOut() override;
	entt::entity MakeBuiltSite(PlayerNames player, Tribe tribe) override;
	void SetCanHaveSites(entt::entity townOrTemple, bool can) override;
	void UpdateTurn() override;
	void ProcessChants() override;
	void SetDancers(entt::entity site, uint32_t dancers) override;
	[[nodiscard]] uint32_t Dancers(entt::entity site) const override;
	float UseChants(entt::entity site, float amount) override;
	float UseCreateChants(entt::entity site, float amount) override;
	float MaintainSpell(entt::entity site, float amount) override;
	void ReturnChants(entt::entity site, float amount) override;
	[[nodiscard]] float ChantsAvailable(entt::entity site) const override;
	float TakeChantsForVirtualInfluence(PlayerNames player, uint32_t interfaces) override;
	void BuildBy(entt::entity site, float share) override;
	[[nodiscard]] bool IsBuilt(entt::entity site) const override;
	[[nodiscard]] entt::entity TempleOf(PlayerNames player) const override;

private:
	/// A town of a player with a temple is given its tribe's site, if it may have one
	void CheckAddSite(entt::entity town);
	/// Each of the player's towns is given its tribe's site, and asked to build it, wanting to by the boost more
	void OpenSites(entt::entity temple, float desireBoost);
	/// A town's tribe's site, made if the temple may make it and the town may have one; none otherwise
	entt::entity FindOrMakeForTown(entt::entity temple, entt::entity town);
	/// The temple's site of the tribe, made if there is none; none without a place for it
	entt::entity FindOrMake(entt::entity temple, Tribe tribe);
	[[nodiscard]] entt::entity FindForTribe(entt::entity temple, Tribe tribe) const;
	/// A new site in the free place nearest the town of the tribe nearest the temple
	entt::entity Make(entt::entity temple, Tribe tribe);
	/// A site's dance, if it has one
	[[nodiscard]] components::Dance* DanceOf(entt::entity site) const;
	/// A new site is given its dance and its food pot
	void Init(entt::entity site);
	/// The town's people worship at the site from now on
	void AddTown(entt::entity site, entt::entity town);
	/// The site has been wholly built: no town is asked to build it any more
	void Built(entt::entity site);
	/// The towns of a player in the order the player gained them
	[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player) const;
	/// What a site's player has drawn from it counts in the player's statistics
	void CountChantsUsed(entt::entity site, float amount);
	/// The site is given to a town if it hasn't it yet
	void AddTownIfMissing(entt::entity site, entt::entity town);

	std::unique_ptr<worship_site::WorldInterface> _world;
};

} // namespace openblack::ecs::systems
