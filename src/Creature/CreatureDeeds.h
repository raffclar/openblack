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

#include <functional>
#include <optional>

#include "Enums.h"

/// The deeds that a player does by hand, which the player's creature may see and copy. Each deed is a row in the game's
/// table of what creatures copy.
namespace openblack::creature_deeds
{

inline constexpr uint32_t k_PutFoodInWorshipSite = 0;
inline constexpr uint32_t k_PutFoodInStoragePit = 2;
inline constexpr uint32_t k_PutWoodInStoragePit = 4;
/// A scaffold that stops after it moved, which the copy makes into wood to a building site
inline constexpr uint32_t k_DropScaffold = 6;
inline constexpr uint32_t k_PutWoodInBuildingSite = 7;
inline constexpr uint32_t k_PutWoodInWorkshop = 9;
inline constexpr uint32_t k_ThrowInTheSea = 21;
inline constexpr uint32_t k_TakeObjectHome = 32;
inline constexpr uint32_t k_StealObjectAndPutInTown = 35;
inline constexpr uint32_t k_StealObjectAndPutByTemple = 36;
/// A villager or an animal sacrificed at a worship site's altar
inline constexpr uint32_t k_Sacrifice = 40;
inline constexpr uint32_t k_PlayWithToy = 41;
inline constexpr uint32_t k_StealFoodFromFarm = 43;
inline constexpr uint32_t k_StealFoodFromStoragePit = 44;
inline constexpr uint32_t k_StealWoodFromStoragePit = 45;
/// Past the last row: nothing a creature copies
inline constexpr uint32_t k_NoDeed = 46;

/// The kind of thing that a player gave a resource to
enum class Receiver : uint8_t
{
	StoragePit,
	WorshipSite,
	Workshop,
	/// Anything else, such as a house
	Other,
};

/// What the creature sees when its player gives a resource to a thing
struct Gift
{
	Receiver receiver {Receiver::Other};
	/// The thing is a building that goes up, which takes wood onto its site
	bool goingUp {false};
	ResourceType resource {ResourceType::None};
	/// The player gives what the player's hand last took from where the player had the most influence, or from the
	/// player's own pile. When not, a gift to a storage pit counts as a theft.
	bool ownResource {false};
};

/// The deed of a gift, none when the gift shows no deed. Wood to a building that goes up is always the building site
/// deed. Food to a storage pit is a gift of the player's own food, or else one of the two food thefts by random(2) (0
/// from a farm, else from a storage pit). Anything else to a storage pit is a gift of wood, or a wood theft. Wood to a
/// workshop and food to a worship site are their deeds. random(n) is from 0 to n - 1.
[[nodiscard]] std::optional<uint32_t> GiftDeed(const Gift& gift, const std::function<uint32_t(uint32_t)>& random);

/// A thing that a player's hand dropped comes to rest within this many metres of the player's temple
inline constexpr float k_TempleReach = 80.0f;

/// What the creature sees when a thing that its player's hand dropped comes to rest
struct Landing
{
	PlayerNames player {PlayerNames::NEUTRAL};
	/// The thing came to rest nearer than k_TempleReach to the player's temple
	bool nearOwnTemple {false};
	/// The player of the town that the thing belongs to, none when it belongs to no town
	std::optional<PlayerNames> thingsTown;
	/// The player of the town nearest to the thing, none when there is no town
	std::optional<PlayerNames> nearestTown;
};
/// The deed of a landing, none when it shows no deed. Near the player's temple, a thing of another player's town is a
/// theft put by the temple, and anything else is a thing taken home. Farther away, a thing of another player's town that
/// comes to rest with the player's own town nearest is a theft put in the town.
[[nodiscard]] std::optional<uint32_t> LandingDeed(const Landing& landing);

/// The deed of a disciple that a player puts down: the deeds of the disciples follow the sea deed in the order of the
/// disciple types. The game adds the type as it is, so from the trader on each type shows the deed of the next type
/// (vanilla bug). None for a type past the last deed.
[[nodiscard]] std::optional<uint32_t> DiscipleDeed(VillagerDisciple type);

} // namespace openblack::creature_deeds
