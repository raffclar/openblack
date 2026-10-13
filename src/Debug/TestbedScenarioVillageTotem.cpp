/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a town centre's totem

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, well clear of the town centre
constexpr glm::vec2 k_Temple {0.0f, -60.0f};

/// A species whose carving the player's totem wears, or none for the hand the totem wears without a creature
struct Carving
{
	std::string_view id;
	std::string_view name;
	std::optional<CreatureType> species;
};
constexpr std::array k_Carvings {
    Carving {"town.totem_symbols_ape", "The player's symbol over the ape's totem", CreatureType::GiantApe},
    Carving {"town.totem_symbols_bear", "The player's symbol over the bear's totem", CreatureType::BrownBear},
    Carving {"town.totem_symbols_cow", "The player's symbol over the cow's totem", CreatureType::Cow},
    Carving {"town.totem_symbols_horse", "The player's symbol over the horse's totem", CreatureType::Horse},
    Carving {"town.totem_symbols_leopard", "The player's symbol over the leopard's totem", CreatureType::Leopard},
    Carving {"town.totem_symbols_lion", "The player's symbol over the lion's totem", CreatureType::Lion},
    Carving {"town.totem_symbols_polar_bear", "The player's symbol over the polar bear's totem", CreatureType::PolarBear},
    Carving {"town.totem_symbols_sheep", "The player's symbol over the sheep's totem", CreatureType::Sheep},
    Carving {"town.totem_symbols_tiger", "The player's symbol over the tiger's totem", CreatureType::Tiger},
    Carving {"town.totem_symbols_tortoise", "The player's symbol over the tortoise's totem", CreatureType::Tortoise},
    Carving {"town.totem_symbols_wolf", "The player's symbol over the wolf's totem", CreatureType::Wolf},
    Carving {"town.totem_symbols_zebra", "The player's symbol over the zebra's totem", CreatureType::Zebra},
    Carving {"town.totem_symbols_hand", "The player's symbol over the hand's totem", std::nullopt},
};
/// The player's creature stands well away to the east, out of the shot
constexpr glm::vec2 k_CreatureAway {60.0f, 40.0f};
/// The town believes in the player, so that it stays theirs
constexpr float k_TownBelief = 1000.0f;
} // namespace

void openblack::testbed_scenarios::AddVillageTotemScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.village_totem",
	    .name = "A town centre's totem",
	    .facet = Facet::Objects,
	    .description = "A Norse town centre of the player's town, which has no worship site.",
	    .expected = "The tribe's totem, the column in the middle of the centre, stands at the centre's totem point, turned "
	                "as the centre is, with the hand's icon on its pole (the player has no creature), sunk in the centre "
	                "with no rise: with nowhere to worship the town has no one at worship. Holding the Action button on "
	                "it takes no hold, as there is no worship site.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	});
	all.push_back({
	    .id = "objects.village_totem_worship",
	    .name = "A town centre's totem with a temple",
	    .facet = Facet::Objects,
	    .description = "The same Norse town centre of the player's town, with the player's temple standing built "
	                   "beside it, which opens the worship site of the town, which has one villager.",
	    .expected = "Once the town's worship site is built, holding the Action button on the totem takes hold of it: the "
	                "hand goes to the icon, tipped forwards over it in its side hold, and the pointer stays on the hand. "
	                "Sliding the mouse up raises it, a see-through second totem staying at the town's share, with the "
	                "People Worshipping tooltip by the hand. Let go, the share floats up over it in grey and it eases "
	                "there, the second totem waiting where it is going.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                // A town with no one in it has no worship site
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .temples = {{.offset = k_Temple}},
	});

	for (const auto& carving : k_Carvings)
	{
		std::vector<CreatureSetup> creatures;
		if (carving.species.has_value())
		{
			creatures.push_back({.species = *carving.species, .offset = k_CreatureAway, .pauseMind = true});
		}
		all.push_back({
		    .id = carving.id,
		    .name = carving.name,
		    .facet = Facet::Objects,
		    .description = "A Norse town centre of the player's town, which believes only in the player, and the "
		                   "player's creature of one species standing away to the east (none for the hand's totem). "
		                   "Seen from the south south west, a little above the top of the totem.",
		    .expected = "The totem wears the carving of the player's creature (the hand without one). The player's "
		                "symbol sits still over the totem, the town believing in no one else: as high over the top of "
		                "the carving as the symbols are set to sit, whatever the carving's height, never sunk into it.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_Centre.x - 12.7f, 18.0f, k_Centre.y - 27.2f},
		                .look = {k_Centre.x, 14.0f, k_Centre.y}},
		    .creatures = std::move(creatures),
		    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
		    .townBeliefInPlayer = k_TownBelief,
		});
	}
}
