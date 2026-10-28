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
	    .description = "A Norse town centre of the player's town, which believes only in the player and has no worship site. "
	                   "The player has no temple.",
	    .expected = "The tribe's totem, the column in the middle of the centre, stands at the centre's totem point, turned "
	                "as the centre is, with the hand's icon on its pole (the player has no creature), sunk in the centre "
	                "with no rise: with nowhere to worship the town has no one at worship. Holding the Action button on "
	                "it takes no hold, as the player has no temple and the town has no worship site. The inspector's pick says "
	                "why.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "objects.village_totem_worship",
	    .name = "A town centre's totem with a temple",
	    .facet = Facet::Objects,
	    .description = "The same Norse town centre of the player's town, which believes only in the player and has one "
	                   "villager, with the player's temple standing built beside it and the town's worship site put up "
	                   "built, no one at worship.",
	    .expected = "Holding the Action button on the totem takes hold of it: the "
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
	    // Only a town of the player's own, with its worship site built, lets the hand take its totem
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
	// The creature stands under the hand, so that it sees the hand, and the camera follows it
	const glm::vec2 watcher = k_Centre + glm::vec2(0.0f, -20.0f);
	all.push_back({
	    .id = "mind.totem_grip_seen",
	    .name = "The creature sees the hand take the totem",
	    .facet = Facet::Mind,
	    .description = "The player's tiger stands under the hand near the player's town centre. Two times, the hand takes "
	                   "hold of the totem and lets go.",
	    .expected = "Each time, the tiger sees the hand take the totem, and its wish to impress grows 2.5 times faster. "
	                "The log shows the time that Impress takes to grow by 1: it falls to a 2.5th each time.",
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = watcher, .pauseMind = true}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::HandGripTotem, .delaySeconds = 10.0f, .object = 0},
	                 {.kind = Command::Kind::HandGripTotem, .delaySeconds = 2.0f, .object = 0}},
	    .hand = HandHold {.offset = watcher, .height = 12.0f},
	    .townBeliefInPlayer = 1.0f,
	});
	all.push_back({
	    .id = "objects.map_cells_refile",
	    .name = "A totem and a pot go into the map cells again when they change",
	    .facet = Facet::Objects,
	    .description = "The same Norse town centre with the player's temple and the town's worship site, and the player's "
	                   "tiger beside a pot of food on the land. The totem turns to face the worship site and takes the "
	                   "tiger's icon. The tiger is told to eat from the pot.",
	    .expected = "The totem stands turned to the worship site, side on, with the tiger on its pole, and it stays drawn "
	                "as the camera moves. The map cells around it hold it at the front of their fixed things, so the tiger "
	                "and the villager walk round it. The tiger takes a handful from the pot: the pot becomes smaller and "
	                "the cells that hold it change with it.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(25.0f), k_Centre + glm::vec2(25.0f)},
	                .distance = 0.5f},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_Centre + glm::vec2(-20.0f, -20.0f),
	                   .needs = {.energy = 1.0f, .dehydration = 0.0f, .poo = 0.0f}}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true},
	                {.type = PotInfo::FoodPot, .offset = k_Centre + glm::vec2(-20.0f, -10.0f), .amount = 400}},
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::DoAction, .delaySeconds = 2.0f, .object = 2, .action = "EatFromFoodPile"}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
	all.push_back({
	    .id = "interface.floating_number_pause",
	    .name = "The share floats up and stops while the game pauses",
	    .facet = Facet::Interface,
	    .description = "The player's Norse town centre, with its worship site built. The hand takes hold of the totem and "
	                   "lets go, so that the share floats up over it. One second later the game pauses. Press P to "
	                   "continue the game.",
	    .expected = "The grey share rises over the totem and fades in its last second. While the game pauses, the share "
	                "stands still and keeps its strength. When the game continues, it rises and fades again from where it "
	                "stopped. A faster game speed makes it rise faster.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::HandGripTotem, .delaySeconds = 2.0f, .object = 0},
	                 {.kind = Command::Kind::HandGripTotem, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Command::Kind::PauseGame, .delaySeconds = 1.0f, .value = 1}},
	    .townBeliefInPlayer = 1.0f,
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
	all.push_back({
	    .id = "town.belief_symbols_frames",
	    .name = "The symbols of belief of two players over a totem, stepped in the drawn frames",
	    .facet = Facet::Objects,
	    .description = "A Norse town centre of the player's town, which believes in the player and in a rival, seen from "
	                   "the south south west a little above the top of the totem, with the camera still. The symbols "
	                   "move on by a whole turn's time in each drawn frame, no more often than 60 times a second.",
	    .expected = "One symbol sits over the totem; the other circles it and now and then rises to fight it, with "
	                "sparkles. The symbols move at the same speed at 60 frames a second and at hundreds of frames a "
	                "second, and they continue to move while the game is paused. Their random numbers are only this "
	                "computer's, so the world stays the same.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Centre.x - 12.7f, 24.0f, k_Centre.y - 27.2f},
	                .look = {k_Centre.x, 21.0f, k_Centre.y}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townBeliefInRival = k_TownBelief,
	});
}
