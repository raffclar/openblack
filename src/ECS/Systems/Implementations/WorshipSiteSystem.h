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
#include <unordered_map>
#include <vector>

#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorshipSiteLooks.h"
#include "ECS/WorshipSiteWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Dance;
} // namespace openblack::ecs::components

namespace openblack::particles
{
class LightSheet;
} // namespace openblack::particles

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
	entt::entity DebugAllowSite(entt::entity town, bool anyLand) override;
	void RemoveSites(entt::entity temple) override;
	void UpdateTurn() override;
	void UpdateFrame(std::chrono::milliseconds gameTime, std::chrono::milliseconds realTime, bool landDrawn) override;
	void RemakeModelsAfterLoad() override;
	void ProcessChants() override;
	void SetDancers(entt::entity site, uint32_t dancers) override;
	[[nodiscard]] uint32_t Dancers(entt::entity site) const override;
	float UseChants(entt::entity site, float amount) override;
	float UseCreateChants(entt::entity site, float amount) override;
	float MaintainSpell(entt::entity site, float amount) override;
	void AddMote(entt::entity site, glm::vec3 from, glm::vec3 to) override;
	void ReturnChants(entt::entity site, float amount) override;
	[[nodiscard]] float StoredChants(entt::entity site) const override;
	void SetStoredChants(entt::entity site, float chants) override;
	[[nodiscard]] float ChantsAvailable(entt::entity site) const override;
	[[nodiscard]] float TotalChantsAvailable(entt::entity site) const override;
	float TakeChantsForVirtualInfluence(PlayerNames player, uint32_t interfaces) override;
	void BuildBy(entt::entity site, float share) override;
	void SetBuilt(entt::entity site, float built) override;
	[[nodiscard]] bool IsBuilt(entt::entity site) const override;
	[[nodiscard]] entt::entity TempleOf(PlayerNames player) const override;
	[[nodiscard]] std::vector<entt::entity> TemplesOfPlayers(size_t count) const override;
	[[nodiscard]] std::optional<glm::vec2> PlaceAt(entt::entity site, SitePlace place) const override;
	[[nodiscard]] std::optional<glm::vec3> DancePosition(entt::entity site) const override;
	[[nodiscard]] std::optional<float> StandingHeight(entt::entity site) const override;
	void AddWorshipper(entt::entity site, entt::entity villager) override;
	void RemoveWorshipper(entt::entity site, entt::entity villager) override;
	[[nodiscard]] bool IsWorshipper(entt::entity site, entt::entity villager) const override;
	[[nodiscard]] float WorshipSpeed(entt::entity site) const override;
	[[nodiscard]] float ChantsPerDancer(entt::entity site) const override;
	[[nodiscard]] uint32_t FoodInPot(entt::entity site) const override;
	[[nodiscard]] int32_t FoodNeededFromTown(entt::entity site) const override;
	[[nodiscard]] float DesireForFood(entt::entity site) const override;
	void RemoveFoodOnTheWay(entt::entity site, int32_t amount) override;

private:
	/// Whether a town may have a worship site: on a land with sites, with people, and not stopped by a script
	[[nodiscard]] bool TownMayHaveSite(entt::entity town) const;
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
	/// A site goes, with everything of it, leaving its towns and its place round the temple
	void Remove(entt::entity site);
	/// A site's dance, if it has one
	/// The food that the villagers who dance at the site want for their dinner
	[[nodiscard]] float DancersFoodNeed(entt::entity site) const;
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

	/// A site's turn works out what its need flags show, and gives it the flags it lacks
	void UpdateNeeds(entt::entity site);
	/// How much a site's dancers want rest, 0 to 1: none without a dance
	[[nodiscard]] float DesireForRest(entt::entity site) const;
	/// Whether a frame draws a site: the frame draws the land, the list of things to draw has it, and it is on the
	/// screen
	[[nodiscard]] bool DrawnThisFrame(entt::entity site) const;
	/// The need flags of the drawn built sites ease to their needs and turn to the camera
	void UpdateNeedFlags(int32_t gameMilliseconds);
	/// The ring of light round a drawn built site's dance is set from the site and moves on
	void UpdateRing(entt::entity site, bool drawn, int32_t realMilliseconds);

	/// What the frames keep of a site: the ring of light round its dance and the throb of its strain. A saved game
	/// does not keep these.
	struct SiteLook
	{
		std::shared_ptr<particles::LightSheet> ring;
		/// Where the ring stands; it stands again when the site is somewhere else, as after a load
		glm::vec3 ringCentre {0.0f};
		worship_site_looks::Throb throb;
	};
	std::unordered_map<entt::entity, SiteLook> _looks;

	std::unique_ptr<worship_site::WorldInterface> _world;
	/// Only while the debug tools let a town have its site: the land without worship sites lets it have one too
	bool _debugAnyLand {false};
};

} // namespace openblack::ecs::systems
