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
#include <span>

#include "3D/MapCoords.h"

/// The rules by which a computer god finds how much it wants a town's need or a miracle. Pure functions of what the god
/// sees.
namespace openblack::computer_mind_rules
{

/// The worship site's icon of a miracle, as the god sees it
struct IconFacts
{
	/// The icon belongs to a worship site
	bool site {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// The prayer power that the miracle costs to make
	float cost {0.0f};
	/// The site never runs out of prayer power
	bool infinite {false};
	/// The prayer power that the site keeps, and what its dancers give this turn and what was used of it
	float battery {0.0f};
	float available {0.0f};
	float used {0.0f};
	/// The prayer power that the dancers of the site chant each turn
	float capacity {0.0f};
};

/// The prayer power that the site reports when it never runs out
constexpr float k_InfiniteChants = 1.0e6f;
/// A small amount that keeps a share from a division by zero
constexpr float k_SmallChants = 0.001f;
/// How much each request for a miracle adds to the god's wish to charge it
constexpr float k_RequestShare = 0.01f;
/// The turns to charge a miracle at which the god no longer wants it
constexpr float k_MostChargeTurns = 2000.0f;

/// The part of the miracle's cost that the icon and its site could give now. With no dancers and less than the cost, 0.
[[nodiscard]] float ChargeShare(const IconFacts& icon);
/// The turns that the dancers need to charge the rest of the miracle: 0 when the site has enough now, -1 when the icon
/// has no site or its site has no dancers
[[nodiscard]] float TurnsToCharge(const IconFacts& icon);
/// How much the god wants to charge a miracle that it asked for this many times, 0 to 1
[[nodiscard]] float RequestShare(uint32_t requests);
/// How much the god wants to cast a miracle: the share of the charge that is there, less the further the charge is from
/// done, and the place's own pull. Never more than 1.
[[nodiscard]] float SpellValue(float charge, float turnsToCharge, float placePull);

/// The town that wants a thing most: the first with the largest desire over 0
[[nodiscard]] std::optional<size_t> MostWanting(std::span<const float> desires);

/// An icon of a worship site that the god could charge
struct ChargeCandidate
{
	/// The worshippers chant at the icon now
	bool charging {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// How many times the god asked for the miracle of the icon
	uint32_t requests {0};
};

/// The icon that the god wants to charge most, and how much: the first with the largest wish over 0 among the icons that
/// do not charge and have nothing stored
struct ChargeChoice
{
	size_t index {0};
	float value {0.0f};
};
[[nodiscard]] std::optional<ChargeChoice> ChooseCharge(std::span<const ChargeCandidate> icons);

/// The distance in metres at which a field no longer pulls the hand
constexpr float k_FieldReach = 250.0f;
/// A field must be wanted more than this before the god takes food from it
constexpr float k_LeastFieldWish = 0.1f;

/// How much the god wants to take food from a field: the share of the field's full crop that is in it, less the further
/// the field is from the hand, times the pull of the field's place
[[nodiscard]] float FieldWish(float food, float totalFood, float distanceFromHand, float placePull);
/// The field that the god wants most: the first with the largest wish over k_LeastFieldWish
[[nodiscard]] std::optional<size_t> MostWantedField(std::span<const float> wishes);

/// The share of the worship that a town wants filled at which the god sees it as fully short of worshippers, and the
/// share under which the god sees no need
constexpr float k_FullWorshipNeed = 0.7f;
constexpr float k_LeastWorshipNeed = 0.2f;
/// The dancers that each icon of a worship site wants
constexpr float k_DancersPerIcon = 4.0f;
/// A villager must be further than this, in metres, from the gate of the worship site before the god carries it there
constexpr float k_CarryToWorshipDistance = 50.0f;

/// How much a town needs more worshippers, from 0 to 1, from its people and those that worship or go to worship: the
/// cube of the share that does not worship. A need of k_FullWorshipNeed or more is full; under k_LeastWorshipNeed it is
/// none.
[[nodiscard]] float TownWorshipNeed(uint32_t population, uint32_t worshipping, uint32_t onTheWay);
/// How much a built worship site needs more dancers, from 0 to 1: one less the fourth power of the share of its dancers
/// against k_DancersPerIcon for each icon. A site with no icons needs none.
[[nodiscard]] float SiteDancerNeed(uint32_t dancers, uint32_t icons);

/// A circle of a player's own influence: its middle and how far it reaches, in metres
struct InfluenceCircle
{
	map_coords::MapCoords centre;
	float radius {0.0f};
};

/// The god measures its distance to the edge of its influence from this distance at first
constexpr float k_FarFromInfluence = 9999999.0f;
/// Under this pull a place outside the influence counts for nothing
constexpr double k_LeastPlacePull = 0.01;

/// The point on the edge of the player's influence nearest to a place, from the circles of the citadel first and then
/// of the towns in their order. The first circle of the smallest distance wins. None when there are no circles.
[[nodiscard]] std::optional<map_coords::MapCoords> NearestInfluenceEdge(const map_coords::MapCoords& place,
                                                                        std::span<const InfluenceCircle> circles);
/// How much a place outside the god's influence pulls, from 0 to 1: less the further it is from the edge of the
/// influence, out to the reach of the hand past the border. A place that pulls less than a hundredth gives 0.
[[nodiscard]] float OutsidePlacePull(float distanceToEdge, float reach);

} // namespace openblack::computer_mind_rules
