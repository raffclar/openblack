/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <functional>
#include <optional>
#include <span>

#include "ECS/Components/Scaffold.h"

/// The rules of the building that a held scaffold offers: which building a town wants from the scaffold, and how the
/// phantom building of the scaffold changes from one offer to the next. Free of the game's state, so that they can be
/// tested on their own.
namespace openblack::ecs::scaffold_offer
{

/// The milliseconds that the phantom takes to grow to its full size, or to become small again
inline constexpr int32_t k_ChangeMs = 400;
/// The milliseconds of one game turn, by which the change of the phantom continues each turn
inline constexpr int32_t k_TurnMs = 100;
/// The turns about the up axis that a search tries, one eighth of a full turn apart
inline constexpr size_t k_Tries = 8;
inline constexpr float k_TryStep = std::bit_cast<float>(0x3F490FDBU);  // pi / 4
inline constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBU); // 2 pi
/// The count of the turns since the scaffold last showed that it has no offer goes back to 0 after this many
inline constexpr uint8_t k_NoOfferTurnsWrap = 16;

/// A scaffold is near a workshop when the workshop is this near and the scaffold is near the flag or a place of its yard,
/// in metres
inline constexpr float k_WorkshopReach = 50.0f;
inline constexpr float k_FlagReach = 10.0f;
inline constexpr float k_PlaceReach = 5.0f;
/// The special point of the workshop's model where its flag stands
inline constexpr size_t k_FlagPoint = 10;

/// The distances from a scaffold to a workshop, to the workshop's flag and to the three places of its yard
struct WorkshopDistances
{
	float workshop {0.0f};
	float flag {0.0f};
	std::array<float, 3> places {};
};

/// Whether a scaffold is near a workshop: the workshop is 50 metres away or nearer, and the scaffold is nearer than 10
/// metres to the flag or nearer than 5 metres to a place of the yard. A town offers no building near its workshops.
[[nodiscard]] bool NearWorkshop(const WorkshopDistances& distances);

/// A building that a town can build from a scaffold, as a search reads it
struct Candidate
{
	/// The building at a turn of 0. The search puts the turn of the try in.
	components::ScaffoldOffer offer;
	/// The scaffolds that the building needs. A building that needs no scaffold is never offered.
	uint32_t scaffoldsRequired {0};
	/// How much the town wants the building, with the value of the scaffolds (see planned_building::DesireToBeBuilt)
	float desire {0.0f};
};

/// Whether a building fits on the land at the scaffold, turned by an angle. Empty: every building fits.
using Fits = std::function<bool(const components::ScaffoldOffer& offer, float yAngle)>;

/// The building that a town wants most at one turn. The candidates are in the order of their abode numbers. A candidate
/// must need scaffolds, no more than the value, and must fit. A candidate replaces the best so far only when the town
/// wants it more. None when the town wants no candidate.
[[nodiscard]] std::optional<components::ScaffoldOffer> BestAtTurn(std::span<const Candidate> candidates, uint32_t value,
                                                                  float yAngle, const Fits& fits);

/// The building that a town offers for a scaffold. The search tries eight turns, from the start turn and one eighth of a
/// full turn more each time, and stops at the first turn where the town wants a building.
[[nodiscard]] std::optional<components::ScaffoldOffer> SearchTown(std::span<const Candidate> candidates, uint32_t value,
                                                                  float startYAngle, const Fits& fits);

/// The village centre that a scaffold with no town near it offers, to start a new town. The centre must need scaffolds,
/// no more than the value. The search tries eight turns from 0 and stops at the first turn where the centre fits.
[[nodiscard]] std::optional<components::ScaffoldOffer> SearchNewTown(const Candidate& centre, uint32_t value, const Fits& fits);

/// The change of a game turn to the phantom of a held scaffold:
/// 1. The count of the turns since the scaffold showed that it has no offer continues.
/// 2. The phantom turns by its turn for each game turn, unless its turn is fixed.
/// 3. The change of its size continues by one game turn.
/// 4. When the phantom is small and its change is complete, the next offer shows. The phantom then grows.
void StepTurn(components::ScaffoldPhantom& phantom, float turnPerGameTurn);

/// The phantom takes the next offer that a search found, or no offer:
/// - With no offer shown, the next offer shows at once and the phantom grows.
/// - An offer with the same model replaces the shown offer at once.
/// - Else the phantom becomes small first. The next offer shows when the phantom is small (see StepTurn).
void TakeNextOffer(components::ScaffoldPhantom& phantom);

/// The result of a search for an offer
struct Found
{
	components::ScaffoldOffer offer;
	/// The offer is a village centre that starts a new town
	bool foundsTown {false};
};

/// What the game knows of a held scaffold in a game turn
struct HeldFacts
{
	/// The hand that holds the scaffold stays still in this turn
	bool handStill {false};
	/// The turn of the phantom for each game turn, in radians
	float turnPerGameTurn {0.0f};
};

/// What a held scaffold does in a game turn, apart from its phantom
enum class HeldEvent : uint8_t
{
	/// Nothing more
	None,
	/// The scaffold shows with a puff that it has no offer
	ShowsNoOffer,
};

/// The search for an offer at the place of the scaffold. None when no building is offered.
using Search = std::function<std::optional<Found>()>;
/// Whether the shown offer stays while the hand moves
using KeepsOffer = std::function<bool(const components::ScaffoldPhantom& phantom)>;

/// The game turn of a held scaffold:
/// 1. The phantom does its turn (see StepTurn).
/// 2. While the hand stays still, the scaffold looks for an offer once. It does not look again while it shows an offer.
///    When it finds no offer, it shows that once (see HeldEvent::ShowsNoOffer).
/// 3. While the hand moves, the shown offer stays when it keeps (see KeepsOffer). Else the phantom takes the next
///    offer when its change is complete.
[[nodiscard]] HeldEvent HeldTurn(components::ScaffoldPhantom& phantom, const HeldFacts& facts, const Search& search,
                                 const KeepsOffer& keeps);

} // namespace openblack::ecs::scaffold_offer
