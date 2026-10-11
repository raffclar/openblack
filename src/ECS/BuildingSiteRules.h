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
#include <functional>
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The rules of a building site and its builders: how many builders it wants, how much it wants them, where they stand
/// round the building, whether one should fetch wood first, how much wood a stroke uses and how far it raises the
/// building. Free of the game's state so they can be tested on their own.
namespace openblack::building_site
{

/// A site keeps this many places round its building for its builders to stand at
inline constexpr size_t k_Places = 128;
using Places = std::array<glm::vec3, k_Places>;

/// The builders a site still wants: the most its building takes less those working at it, none once the building is
/// built and either whole or not wanted repaired. It goes below none when more work at it than it takes.
struct BuildersNeededInputs
{
	int32_t maxBuilders;
	int32_t workers;
	bool built;
	bool repaired;
	float repairDesire;
};
[[nodiscard]] int32_t BuildersNeeded(const BuildersNeededInputs& in);

/// How much a site wants villagers: the share of its builders it still wants plus the desire a script forced on it,
/// never below none nor above all
[[nodiscard]] float DesireForVillagers(int32_t needed, int32_t maxBuilders, float forcedDesire);

/// How much wood a building's site counts it worth: its size times its kind's wood, over the tribal power its site's
/// player has of the fifth tribe
[[nodiscard]] float WoodValue(float scale, uint32_t woodValue, float tribalPower);

/// The wood still wanted to build a building: what isn't built yet of it (or for a built one, what isn't whole) of its
/// worth, less what lies at its site
[[nodiscard]] float WoodNeededToBuild(float builtOrLife, float woodValue, uint32_t woodAtSite);

/// Whether a villager about to build should fetch wood first. Near the building it fetches only when none lies at the
/// site. Farther off, while what lies at the site and what its builders carry falls short of what is still wanted, it
/// fetches when the way to its drop-off point scores better than the way to the building, the building's score weighed
/// by how well the site is stocked for one more builder and what the villager itself carries.
struct ShouldFetchInputs
{
	uint32_t woodAtSite;
	/// The wood carried by each builder working at the site
	std::span<const int16_t> workersWood;
	float woodNeeded;
	/// The villager's distance to the building, and to where it would take or leave wood
	float distanceToBuilding;
	float distanceToDropOff;
	int16_t woodHeld;
	int32_t maxWoodCarried;
	int32_t woodPerBuilderWanted;
};
inline constexpr float k_NearBuilding = 50.0f;
inline constexpr float k_FetchDistanceScale = 5000.0f;
[[nodiscard]] bool ShouldFetchWood(const ShouldFetchInputs& in);

/// The wood a builder uses in a stroke: its kind's wood a stroke, a little more where the land leans evil (up to a
/// fifth more), truncated
[[nodiscard]] int32_t WoodPerStroke(float woodUsedPerBuildCycle, float landAlignment);

/// Which of a site's places lies in a direction from the building, an angle in radians: an angle beyond three turns
/// either way gives the first (or, beyond three turns forward, half a turn's)
[[nodiscard]] uint32_t PlaceAt(float angle);

/// A builder's place after a stroke: some places on round the building, about the places in two metres of its outline
/// and up to half as many again, one way or the other. The draws are taken in the game's order, the float first.
using FloatRandom = std::function<float(float)>;
using IntRandom = std::function<uint32_t(uint32_t)>;
[[nodiscard]] uint32_t NextPlace(uint32_t place, float buildingRadius, const FloatRandom& floatRandom,
                                 const IntRandom& intRandom);

/// The angle a builder makes for round the building first: towards itself, give or take an eighth of a turn
[[nodiscard]] float FirstPlaceAngle(float angleToBuilder, const FloatRandom& floatRandom);

/// The places round a building, found from its model: for each triangle whose two lowest corners stand within 2.5 of
/// the model's foot, every direction from the middle of the model's box is followed out to the line through those
/// corners, and the farthest crossing on the ground within the model's reach is kept. A direction that crosses nothing
/// keeps the building's place. Every place is then put a metre farther out from the box's middle.
struct Triangle
{
	std::array<glm::vec3, 3> corners;
};
inline constexpr float k_FootHeight = 2.5f;
[[nodiscard]] Places OutlinePlaces(std::span<const Triangle> modelTriangles, const glm::mat4& model, glm::vec3 boxCentre,
                                   float reach);

/// The places round a round building: a circle of its reach about the middle of its box, each at the land's height
[[nodiscard]] Places CirclePlaces(glm::vec3 centre, float reach, const std::function<float(glm::vec2)>& landHeight);

/// A temple's site keeps six piles of wood round it, each a set distance out from the heart, a seventh of a turn apart
/// from where its worship sites start, less a turn's offset
inline constexpr size_t k_TemplePiles = 6;
inline constexpr float k_TemplePileDistance = 22.0f;
[[nodiscard]] float TemplePileAngle(float heartYAngle, size_t pile);

/// Of the sites wanting builders (or all of them for a builder disciple), the one whose building's nearest edge is
/// nearest, each distance weighed by how much the site wants villagers. The earlier one on a tie.
struct SiteCandidate
{
	bool wantsBuilders;
	float distanceToEdge;
	float desireForVillagers;
};
[[nodiscard]] std::optional<size_t> BestSite(std::span<const SiteCandidate> sites, bool anyway);

/// Where a villager gets wood from: its store, a big forest or a forest, by how far each is and what it already
/// carries. For a building, the store counts only when it holds more than the villager has room for, and a forest
/// counts half.
enum class WoodSource : uint8_t
{
	None,
	Store,
	BigForest,
	Forest,
};
struct WoodSourceInputs
{
	bool forBuilding;
	float distanceToStore;
	uint32_t storeWood;
	int32_t room;
	int32_t maxWoodCarried;
	std::optional<float> distanceToForest;
	bool forestHasBigForest;
	float maxDistance;
};
[[nodiscard]] WoodSource DecideWoodSource(const WoodSourceInputs& in);

} // namespace openblack::building_site
