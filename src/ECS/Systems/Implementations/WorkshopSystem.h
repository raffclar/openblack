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
	void ProcessHeld(entt::entity scaffold) override;
	void SiteClosed(entt::entity building) override;
	void PlaceGhostsForFrame(std::optional<glm::vec3> drawnHand) override;

private:
	/// Whether a scaffold at a place is near a workshop of a town (see scaffold_offer::NearWorkshop)
	[[nodiscard]] bool NearWorkshops(entt::entity town, glm::vec3 place) const;
	/// The building that a scaffold offers to the town near it, none when it offers none. A search of a town turns
	/// from a random angle.
	std::optional<scaffold_offer::Found> SearchOffer(entt::entity scaffold, entt::entity town);
	/// The search for an offer, then the phantom takes the offer
	bool SearchAndTakeOffer(entt::entity scaffold, entt::entity town);
	/// A scaffold that a hand put down gently starts the building that it offers. Whether it started the building.
	bool BuildBuilding(entt::entity scaffold, std::optional<PlayerNames> player);
	/// The town starts the building that the scaffold offers, at the place of the scaffold. Whether it started it.
	bool TryToBuild(entt::entity scaffold, entt::entity town);
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
	/// The sounds of the phantom in the hand that played. Only the frames change them.
	scaffold_ghost::SoundsPlayed _ghostSounds;
};

} // namespace openblack::ecs::systems
