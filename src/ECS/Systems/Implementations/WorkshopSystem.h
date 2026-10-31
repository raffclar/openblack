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
#include <utility>
#include <vector>

#include <entt/signal/sigh.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/ScaffoldGhostRules.h"
#include "ECS/ScaffoldOfferRules.h"
#include "ECS/Systems/WorkshopSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class WorkshopSystem final: public WorkshopSystemInterface
{
public:
	WorkshopSystem();

	void ProcessWorkshop(entt::entity workshop) override;
	[[nodiscard]] int32_t RoomOf(entt::entity workshop) const override;
	entt::entity WoodPileOf(entt::entity workshop) override;
	entt::entity CreateScaffold(glm::vec3 place, entt::entity town, entt::entity workshop, float yAngle, float scale) override;
	[[nodiscard]] bool CanTake(entt::entity scaffold) const override;
	void TakenByHand(entt::entity scaffold, PlayerNames player) override;
	void LetGoByHand(entt::entity scaffold, bool forced) override;
	void NotPutDownByHand(entt::entity scaffold) override;
	void CameToRest(entt::entity scaffold, bool fromHand, bool inMap) override;
	[[nodiscard]] bool CanMove(entt::entity scaffold) const override;
	[[nodiscard]] entt::entity TownForBuilding(glm::vec3 place) const override;
	[[nodiscard]] bool CanJoin(entt::entity held, entt::entity target) const override;
	void Join(entt::entity held, entt::entity target, std::optional<glm::vec3> hand) override;
	[[nodiscard]] bool GoesIntoStore(entt::entity scaffold, entt::entity store) const override;
	bool GiveToStore(entt::entity scaffold, entt::entity store, PlayerNames player) override;
	entt::entity SplitOffRest(entt::entity scaffold) override;
	[[nodiscard]] bool ValidToTap(entt::entity scaffold) const override;
	bool Tap(entt::entity scaffold, glm::vec3 hand) override;
	[[nodiscard]] std::optional<components::ScaffoldOffer> OfferAt(entt::entity town, glm::vec3 place, uint32_t value) override;
	void ProcessHeld(entt::entity scaffold) override;
	void SiteClosed(entt::entity building) override;
	entt::entity RewardScaffold(entt::entity town, glm::vec3 place) override;
	entt::entity LeaveDestroyedSite(entt::entity building) override;
	bool ForceBuild(entt::entity scaffold, PlayerNames player) override;
	void PlaceGhostsForFrame(std::optional<glm::vec3> drawnHand) override;
	void UpdateFlagsForFrame(float seconds) override;

private:
	/// Whether a scaffold at a place is near a workshop of a town (see scaffold_offer::NearWorkshop)
	[[nodiscard]] bool NearWorkshops(entt::entity town, glm::vec3 place) const;
	/// The building that a scaffold offers to the town near it, none when it offers none. A search of a town turns
	/// from a random angle.
	std::optional<scaffold_offer::Found> SearchOffer(entt::entity scaffold, entt::entity town);
	/// The building that a town offers at a place for scaffolds of a value, only of the abode number when one is given.
	/// The fit check ignores the building `ignored`, when it is given. With `skipFit`, every building fits. A wonder is
	/// of the wonder tribe when it is given, else of the tribe of the town.
	std::optional<components::ScaffoldOffer> SearchTownAt(entt::entity town, glm::vec3 place, uint32_t value,
	                                                      std::optional<AbodeNumber> only, entt::entity ignored = entt::null,
	                                                      bool skipFit = false,
	                                                      std::optional<Tribe> wonderTribe = std::nullopt);
	/// The scaffold under a scaffold that a hand holds or let go, which the held scaffold can join, none when there is
	/// none. See the definition for the conditions.
	[[nodiscard]] entt::entity ScaffoldUnder(entt::entity scaffold) const;
	/// The search for an offer, then the phantom takes the offer
	bool SearchAndTakeOffer(entt::entity scaffold, entt::entity town);
	/// A scaffold that a hand put down gently starts the building that it offers. Whether it started the building. With
	/// `skipFit`, the building does not have to fit on the land.
	bool BuildBuilding(entt::entity scaffold, std::optional<PlayerNames> player, bool skipFit = false);
	/// The scaffold makes its reaction for the player who put it down. The first people of the near towns are impressed
	/// by the reaction at once.
	void ImpressNearTowns(entt::entity scaffold, PlayerNames player);
	/// A scaffold founds a new town of the tribe of its town `home` at its place: the new town lists its forests, holds
	/// the miracles of `home` and gives the player the starting belief of a new town. The new town.
	entt::entity FoundTown(entt::entity scaffold, entt::entity home, std::optional<PlayerNames> player);
	/// The town holds every miracle that the other town holds, with the plain miracle of each seed
	void GiveMiraclesOfTown(entt::entity town, entt::entity other);
	/// A scaffold that a script marked clears its ground as a hand lets it go. It pushes the things that can fly out of
	/// its way, and removes the other things in its reach.
	void DestroyThingsInWay(entt::entity scaffold);
	/// The scaffold at `scaffoldAt` (its middle `centre` in metres) clears one thing from its way. The edge point towards
	/// the thing is `edgeDistance` from the scaffold. Whether the thing left the map: a pushed thing that does not start
	/// to fly stays in it.
	bool ClearThing(entt::entity thing, const map_coords::MapCoords& scaffoldAt, glm::vec2 centre, float edgeDistance,
	                bool building);
	/// Makes the wood flag of a workshop
	entt::entity MakeFlag(entt::entity workshop);
	/// The town starts the building that the scaffold offers, at the place of the scaffold. Whether it started it. With
	/// `skipFit`, the building does not have to fit on the land.
	bool TryToBuild(entt::entity scaffold, entt::entity town, bool skipFit);
	/// The scaffold leaves the building site that it is on, and the building of the site goes
	void LeaveSite(entt::entity scaffold);
	/// The game turn makes the see-through copy that shows the phantom of the scaffold, if the scaffold has none
	void MakeGhost(entt::entity scaffold);
	/// The frame shows the phantom of a scaffold in the hand of the local player. Whether it shows.
	bool PlaceHeldGhost(entt::entity scaffold, entt::entity ghost, glm::vec3 drawnHand);
	/// The frame shows the phantom on the building site of a scaffold. Whether it shows.
	bool PlaceSiteGhost(entt::entity scaffold, entt::entity ghost);

	/// The workshop starts a scaffold: it uses the wood and its work sound starts
	void StartScaffold(entt::entity workshop);
	/// The workshop finishes its scaffold, which stands in the first free place of its yard
	void FinishScaffold(entt::entity workshop);
	/// The workshop stops to count the scaffold as its own
	void ReleaseScaffold(entt::entity scaffold);
	/// The scaffold goes into a place of a workshop's yard when it lies near one; false when it does not
	bool SnapToPlace(entt::entity workshop, entt::entity scaffold);
	/// The scaffold stands in its place of its workshop's yard, turned as the place is turned
	void MoveToPlace(entt::entity workshop, entt::entity scaffold);

	void OnScaffoldGone(entt::registry& registry, entt::entity scaffold);
	void OnWorkshopGone(entt::registry& registry, entt::entity workshop);

	std::vector<entt::scoped_connection> _connections;
	/// The next of the four sounds that a join of scaffolds plays. It changes each join and only selects the sound.
	uint32_t _joinSound {0};
	/// The next of the four sounds of a tap that breaks a scaffold
	uint32_t _tapSound {0};
	/// The sounds of the phantom in the hand that played. Only the frames change them.
	scaffold_ghost::SoundsPlayed _ghostSounds;
};

} // namespace openblack::ecs::systems
