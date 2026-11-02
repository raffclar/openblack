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

#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
	/// A town goes from the game: it leaves its worship site, which no longer asks it to build
	virtual void TownRemoved(entt::entity town) = 0;
	/// The land has been laid out: each town of a player with a temple and no worship site is given one
	virtual void LandLaidOut() = 0;
	/// The land's script puts down a worship site already built: the player's site of the tribe, made if it isn't
	/// there, is built and given to the first of the player's towns of that tribe. The site, none without the player's
	/// temple or a place for it.
	virtual entt::entity MakeBuiltSite(PlayerNames player, Tribe tribe) = 0;
	/// A script lets a town or a temple have worship sites made, or stops it
	virtual void SetCanHaveSites(entt::entity townOrTemple, bool can) = 0;
	/// For the debug tools only. A town is let have its worship site as a land's script lets it: first the town, then its
	/// player's temple. A new site goes up, and the town is asked to build it. With anyLand, the land without worship
	/// sites lets the town have one too. The town's site, none when it can have none.
	virtual entt::entity DebugAllowSite(entt::entity town, bool anyLand) = 0;
	/// A temple loses all its worship sites at once, the newest first, as when it starts to be destroyed. Each goes
	/// with its altar, its dance (whose dancers decide afresh what to do), its food pot and food, its prayer power and
	/// its building site; its towns are left without a site, keeping their share of people at worship.
	virtual void RemoveSites(entt::entity temple) = 0;

	/// Every game turn: each site wears its temple's look as it is now, and works out what its need flags show
	virtual void UpdateTurn() = 0;
	/// Every frame that draws the land, by the frame's game time and its real time: a built site that is drawn eases its
	/// need flags to its needs, turns them to the camera, and shows how full it is by the ring of light round its dance.
	/// Then each straining site throbs. A frame that does not draw the land does none of it.
	virtual void UpdateFrame(std::chrono::milliseconds /*gameTime*/, std::chrono::milliseconds /*realTime*/, bool /*landDrawn*/)
	{
	}
	/// After a load: the models that the game made for these things while it played are made again. A save keeps the
	/// names of these models but not the models, and the game that loads it may not have them.
	virtual void RemakeModelsAfterLoad() {}

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
	/// A mote of prayer power flies from a point to another in the colour of the site's player, as the site tops up a
	/// miracle. The site's turn moves its motes on.
	virtual void AddMote(entt::entity /*site*/, glm::vec3 /*from*/, glm::vec3 /*to*/) {}
	/// Prayer power goes back into a site's battery, as a dropped seed's does
	virtual void ReturnChants(entt::entity site, float amount) = 0;
	/// The prayer power stored in the battery of a site. A site without prayer power has none.
	[[nodiscard]] virtual float StoredChants(entt::entity site) const = 0;
	/// A script sets the prayer power in the battery of a site. What the site can give this turn stays the same until
	/// the end of the turn of the site.
	virtual void SetStoredChants(entt::entity site, float chants) = 0;
	/// What can still be drawn from a site this turn
	[[nodiscard]] virtual float ChantsAvailable(entt::entity site) const = 0;
	/// All the prayer power of a site this turn, with what it gave already: a request for a miracle by gesture asks
	/// for this
	[[nodiscard]] virtual float TotalChantsAvailable(entt::entity site) const = 0;
	/// The hand acting outside its player's influence takes from each of the player's sites what it may this turn,
	/// shared among the player's hands. Returns all it asked for.
	virtual float TakeChantsForVirtualInfluence(PlayerNames player, uint32_t interfaces) = 0;

	/// A site is built up by a share of the whole. Reaching the whole, no town is asked to build it any more. Each share,
	/// also a share on a built site, builds the altar of the site by the same share until the altar is built.
	virtual void BuildBy(entt::entity site, float share) = 0;
	/// An altar not yet built is built up by a share of the whole, at least none. A built altar takes no more. At the
	/// whole, a site at the altar for its builders goes.
	virtual void BuildAltarBy(entt::entity altar, float share) = 0;
	/// A script sets how much of a site is built, at least none. At the whole or more, the site stands built as when
	/// its builders finish it, also when it stood built before.
	virtual void SetBuilt(entt::entity site, float built) = 0;
	/// Whether a site stands wholly built
	[[nodiscard]] virtual bool IsBuilt(entt::entity site) const = 0;
	/// The temple of a player, none without one
	[[nodiscard]] virtual entt::entity TempleOf(PlayerNames player) const = 0;
	/// The temple of each player number below the count, as TempleOf gives it, with one walk of the temples. The list is
	/// in the order of the player numbers.
	[[nodiscard]] virtual std::vector<entt::entity> TemplesOfPlayers(size_t count) const = 0;

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
		/// The point where the game holds the site to stand: the place of its first miracle icon on the model
		Standing,
	};
	/// Where a place of a site is on the land, none without the site's model
	[[nodiscard]] virtual std::optional<glm::vec2> PlaceAt(entt::entity site, SitePlace place) const = 0;
	/// The middle of a site's dance in the world: the place of its altar's point, as high as the land there and the
	/// point's own height. None without the site's model.
	[[nodiscard]] virtual std::optional<glm::vec3> DancePosition(entt::entity /*site*/) const { return std::nullopt; }
	/// How high above the sea the point where the game holds the site to stand is, none without the site's model
	[[nodiscard]] virtual std::optional<float> StandingHeight(entt::entity site) const = 0;
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
	/// How much the site wants food, 0 to 1, from the need of its dancers and the food in its pot
	[[nodiscard]] virtual float DesireForFood(entt::entity site) const = 0;
	/// Food counted as on its way to the site is no longer, whether or not it came
	virtual void RemoveFoodOnTheWay(entt::entity site, int32_t amount) = 0;
};

} // namespace openblack::ecs::systems
