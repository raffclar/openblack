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

#include <algorithm>
#include <limits>
#include <optional>

#include <entt/entity/entity.hpp>

namespace openblack::particles::draw
{

/// The thing that draws an effect in the game's frame. The game has no one list of effects: each owner draws its effects
/// when the frame draws the owner. In each step of the frame the owners draw in the order of TurnOf, and the scene submits
/// the effects in the same order, so that at an equal distance an effect comes after the effects that the game draws
/// before it. A save keeps the value of the drawer, so a new drawer gets the next value.
enum class Drawer : uint8_t
{
	/// An object of the land's draw list, at the place of the object in the list. The objects draw before the hand.
	ListedObject,
	/// The sparkles of the fight moves, which the game draws at once before the leashes
	FightMarks,
	/// The drops, the dust, the smoke and the motes, which come first after the hand
	Liquids,
	/// The lasting effects of the hand and the interface, each in its slot (see HandSlot)
	Hand,
	/// The sheets of light of the gesture trails, then those of the objects (see SheetList), the newest first in each
	LightSheets,
	/// The motes of the worship sites, the sites of each player in the order of the players
	WorshipSites,
	/// The effects of the shields, the newest shield first
	Shields,
	/// The miracles, the newest first. A miracle draws its own effect first, then the other effects that it draws.
	Spells,
	/// The spot visuals and the effects of the scripts, the newest first
	Containers,
	/// The symbols of the players over their artefacts
	Players,
	/// An effect whose drawer in the game is not yet known. These effects come last.
	Others,
	/// A step of the land's draw after the objects of the list, as the puffs of dust and the wake of the boat. The stage of
	/// the effect gives the step.
	LandStep,
	/// The rings of light round the arenas, the newest arena first, after the liquids
	Arenas,
	/// The fires of the fireballs, the newest fireball first, after the shields and before the miracles
	FireBalls,
	/// The shells of light that the effects lay over objects, the newest first, after the rings of the arenas
	VolumeGlows,
};

/// The turn of a drawer in its step of the game's frame, the first turn first
[[nodiscard]] constexpr uint8_t TurnOf(Drawer drawer)
{
	switch (drawer)
	{
	case Drawer::ListedObject:
		return 0;
	case Drawer::LandStep:
		return 1;
	case Drawer::FightMarks:
		return 2;
	case Drawer::Liquids:
		return 3;
	case Drawer::Arenas:
		return 4;
	case Drawer::VolumeGlows:
		return 5;
	case Drawer::Hand:
		return 6;
	case Drawer::LightSheets:
		return 7;
	case Drawer::WorshipSites:
		return 8;
	case Drawer::Shields:
		return 9;
	case Drawer::FireBalls:
		return 10;
	case Drawer::Spells:
		return 11;
	case Drawer::Containers:
		return 12;
	case Drawer::Players:
		return 13;
	case Drawer::Others:
		return 14;
	}
	return 14;
}

/// The lasting effects of the hand and the interface, in the order that the game draws them
enum class HandSlot : uint8_t
{
	/// The arcs that crawl over what the lightning struck
	Arcs,
	/// The stream that the hand scoops from a store
	Scoop,
	/// The stream that the hand pours
	Pour,
	/// The pieces of the objects that break
	Pieces,
	/// The sparks of the mana path out of the hand
	ManaPath,
	/// The symbols of belief that rise
	BeliefSymbols,
	/// The forked bolts of the storms
	StormBolts,
	/// The effect of the gesture trails
	GestureTrails,
	/// The chain behind the hand
	Chain,
	/// The sparks of the miracle selection round the hand
	Selection,
};

/// The lists of the sheets of light, in the order that the game draws them
enum class SheetList : uint8_t
{
	/// The sheets of the gesture trails
	GestureTrails,
	/// The sheets that the effects put round objects
	Objects,
};

/// The places round a temple where its worship sites stand
constexpr uint32_t k_WorshipSitePlaces = 6;
/// The places of the players' temples: the seven players, then the neutral player
constexpr uint32_t k_TemplePlayers = 8;

/// The thing that draws an effect, which its owner gives the effect when it starts it
struct DrawnBy
{
	Drawer drawer {Drawer::Others};
	/// The place of the effect among the effects of its drawer: a HandSlot for the hand, a SheetList for a sheet of
	/// light, the player and the place round the temple for a worship site (see WorshipSiteEffect), and the place of the
	/// effect in its object or its miracle for an object or a miracle (its own effect is 0)
	uint32_t slot {0};
	/// The object of the land's draw list that draws the effect
	entt::entity object {entt::null};
	/// The effect that the same owner draws first, as a miracle draws its own effect before the swirl of its cast
	uint32_t after {0};

	bool operator==(const DrawnBy&) const = default;
};

/// The bits of the place of an effect in its object or its miracle, under the place of the object or the miracle
constexpr uint32_t k_SlotBits = 4;
/// The place in the land's draw list of an object that the list does not hold. Its effects come after those of the listed
/// objects.
constexpr uint32_t k_NotListed = std::numeric_limits<uint32_t>::max() >> k_SlotBits;

/// The effect drawn by one slot of the hand
[[nodiscard]] constexpr DrawnBy HandEffect(HandSlot slot)
{
	return {.drawer = Drawer::Hand, .slot = static_cast<uint32_t>(slot)};
}

/// The sheet of light of one list
[[nodiscard]] constexpr DrawnBy SheetEffect(SheetList list)
{
	return {.drawer = Drawer::LightSheets, .slot = static_cast<uint32_t>(list)};
}

/// The effect of a worship site. The game draws the sites of each player in the order of the players, the neutral player
/// last, and the sites of one temple in the order of their places round the temple.
[[nodiscard]] constexpr DrawnBy WorshipSiteEffect(uint32_t player, uint32_t place)
{
	return {.drawer = Drawer::WorshipSites,
	        .slot = (std::min(player, k_TemplePlayers - 1) * k_WorshipSitePlaces) + std::min(place, k_WorshipSitePlaces - 1)};
}

/// The place of the fire of a fireball among the fires of the fireballs, from the place of the fireball in the order of
/// the things made. The newest fireball comes first.
[[nodiscard]] constexpr uint32_t FireBallPlace(uint32_t made)
{
	return std::numeric_limits<uint32_t>::max() - made;
}

/// The effect drawn by an object of the land's draw list
[[nodiscard]] constexpr DrawnBy ObjectEffect(entt::entity object)
{
	return {.drawer = Drawer::ListedObject, .object = object};
}

/// The place of an effect among the effects of its drawer, the first place first. The index is the place of the effect in
/// the list of the running effects, the newest first. The index of the first effect of the same owner (DrawnBy::after) is
/// none when that effect does not run. An effect of an object of the land's draw list gets only its place in its object:
/// the place of the object in the list comes later, when the frame knows the list.
[[nodiscard]] uint32_t PlaceOf(const DrawnBy& by, uint32_t index, std::optional<uint32_t> afterIndex);

} // namespace openblack::particles::draw
