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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The worship sites by the players' temples (components::WorshipSite): one for each tribe a player has towns of, made
/// as the player's towns come to need one, and the link between each of the player's towns and its tribe's site
class WorshipSiteSystemInterface
{
public:
	virtual ~WorshipSiteSystemInterface() = default;

	/// A temple is made: it takes its worship sites' places, facing the way it was set down. Made already standing, its
	/// player's towns are given their worship sites at once, each asked to build its site.
	virtual void AddTemple(entt::entity temple, float facing, bool standing) = 0;
	/// A temple still to be built is finished: its player's towns are given their worship sites, each asked to build
	/// its site
	virtual void TempleBuilt(entt::entity temple) = 0;
	/// A person joins a town: the town's first, with its player's temple, has it given its tribe's worship site
	virtual void PersonJoinedTown(entt::entity town) = 0;
	/// A town has changed hands: it leaves its worship site, which no longer asks it to build, and is given its new
	/// player's site of its tribe when they have a temple
	virtual void TownChangedHands(entt::entity town) = 0;
	/// The land has been laid out: each town of a player with a temple and no worship site is given one
	virtual void LandLaidOut() = 0;
	/// The land's script puts down a worship site already built: the player's site of the tribe, made if it isn't
	/// there, is built and given to the first of the player's towns of that tribe. The site, none without the player's
	/// temple or a place for it.
	virtual entt::entity MakeBuiltSite(PlayerNames player, Tribe tribe) = 0;
	/// A script lets a town or a temple have worship sites made, or stops it
	virtual void SetCanHaveSites(entt::entity townOrTemple, bool can) = 0;
	/// A temple loses all its worship sites at once, the newest first, as when it starts to be destroyed. Each goes
	/// with its altar, its dance (whose dancers decide afresh what to do), its food pot and food, its prayer power and
	/// its building site; its towns are left without a site, keeping their share of people at worship.
	virtual void RemoveSites(entt::entity temple) = 0;

	/// Every game turn: each site wears its temple's look as it is now
	virtual void UpdateTurn() = 0;

	// Prayer power

	/// Every game turn, before any miracle's upkeep is drawn: each player's sites in turn, by their places round the
	/// temple, charge their spell icons from what their dancers chant and store the rest
	virtual void ProcessChants() = 0;
	/// How many villagers dance at a site, chanting; villager life keeps it as they join and leave the dance
	virtual void SetDancers(entt::entity site, uint32_t dancers) = 0;
	[[nodiscard]] virtual uint32_t Dancers(entt::entity site) const = 0;
	/// Draws prayer power from a site, at most what is left this turn; what it gives counts in its player's statistics.
	/// Returns what it gave.
	virtual float UseChants(entt::entity site, float amount) = 0;
	/// Charges a seed from a site as UseChants does, except that a site that never runs dry gives all without drawing
	virtual float UseCreateChants(entt::entity site, float amount) = 0;
	/// A miracle cast from the site asks for its upkeep: free with the free maintenance cheat
	virtual float MaintainSpell(entt::entity site, float amount) = 0;
	/// Prayer power goes back into a site's battery, as a dropped seed's does
	virtual void ReturnChants(entt::entity site, float amount) = 0;
	/// What can still be drawn from a site this turn
	[[nodiscard]] virtual float ChantsAvailable(entt::entity site) const = 0;
	/// The hand acting outside its player's influence takes from each of the player's sites what it may this turn,
	/// shared among the player's hands. Returns all it asked for.
	virtual float TakeChantsForVirtualInfluence(PlayerNames player, uint32_t interfaces) = 0;

	/// A site is built up by a share of the whole, its altar with it. Reaching the whole, no town is asked to build it
	/// any more.
	virtual void BuildBy(entt::entity site, float share) = 0;
	/// Whether a site stands wholly built
	[[nodiscard]] virtual bool IsBuilt(entt::entity site) const = 0;
	/// The temple of a player, none without one
	[[nodiscard]] virtual entt::entity TempleOf(PlayerNames player) const = 0;

	// Worshippers

	/// The places at a site its worshippers go to
	enum class SitePlace : uint8_t
	{
		/// Where they arrive, and where the footpath from their town ends
		Gate,
		/// Where the hungry rest by the altar
		Rest,
		/// The middle of the dance, at the altar
		Altar,
		/// Its door, where those who can't be seen dancing worship hidden
		Door,
		/// Its middle, which villagers are judged near or far from
		Centre,
	};
	/// Where a place of a site is on the land, none without the site's model
	[[nodiscard]] virtual std::optional<glm::vec2> PlaceAt(entt::entity site, SitePlace place) const = 0;
	/// A villager is counted among a site's worshippers, newest first, unless it is already
	virtual void AddWorshipper(entt::entity site, entt::entity villager) = 0;
	/// A villager is no longer counted among a site's worshippers
	virtual void RemoveWorshipper(entt::entity site, entt::entity villager) = 0;
	[[nodiscard]] virtual bool IsWorshipper(entt::entity site, entt::entity villager) const = 0;
	/// How fast the site's dance is asked to go, 0 to 1; 0 without a dance
	[[nodiscard]] virtual float WorshipSpeed(entt::entity site) const = 0;
	/// What each dancer chanted at the site last turn
	[[nodiscard]] virtual float ChantsPerDancer(entt::entity site) const = 0;
	/// The food in the site's pot
	[[nodiscard]] virtual uint32_t FoodInPot(entt::entity site) const = 0;
	/// The food the site wants its towns to bring: what its dancers want for dinner, less what it has and what is on its
	/// way
	[[nodiscard]] virtual int32_t FoodNeededFromTown(entt::entity site) const = 0;
	/// Food counted as on its way to the site is no longer, whether or not it came
	virtual void RemoveFoodOnTheWay(entt::entity site, int32_t amount) = 0;
};

} // namespace openblack::ecs::systems
