/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a fish farm on the lake's shore: its shoal swimming, and the hand finding the farm through
// its fish, scooping them and scaring them

#include <vector>

#include "3D/FlatLand.h"
#include "Creature/CreatureDesires.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The Action button, and the button that grips the land to move about
constexpr size_t k_Action = 3;
constexpr size_t k_Grip = 1;

/// The farm stands on the lake's southern shore, its town's hut on the bank behind it. Its shoal lies in the first open
/// water going out from it, a few metres north and east of it.
constexpr glm::vec2 k_Farm {5.0f, 165.0f};
constexpr glm::vec2 k_Hut {-10.0f, 140.0f};
constexpr glm::vec2 k_Shoal {8.0f, 172.0f};

std::vector<ObjectSetup> FarmAndTown()
{
	return {
	    {.type = AbodeInfo::NorseHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = FishFarmInfo::Normal, .offset = k_Farm},
	};
}

/// Looking steeply down onto the shoal from the shore, the pointer in the middle of the window over the shoal
Framing OverTheShoal()
{
	return {.shot = Shot::Placed, .eye = {k_Shoal.x, 45.0f, k_Shoal.y - 25.0f}, .look = {k_Shoal.x, 0.0f, k_Shoal.y}};
}

Environment PointerOnTheShoal()
{
	return {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)};
}

Command Press(size_t button, float delay)
{
	return {.kind = Kind::PointerPress, .delaySeconds = delay, .value = button};
}

Command Release(size_t button, float delay)
{
	return {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = button};
}

/// The pond north-west of the middle, a fish farm on its south-eastern bank whose shoal lies in the pond's shallows,
/// where a creature can stand; the town's storage pit and three of its people on the plain beyond
constexpr glm::vec2 k_PondFarm = glm::vec2(2295.0f, 2945.0f) - flat_land::k_MapMiddle;
constexpr glm::vec2 k_PondPit = k_PondFarm + glm::vec2(25.0f, -45.0f);
constexpr glm::vec2 k_PondCreature = k_PondFarm + glm::vec2(45.0f, -5.0f);

std::vector<ObjectSetup> PondFarmAndPit()
{
	return {
	    {.type = FishFarmInfo::Normal, .offset = k_PondFarm},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_PondPit, .amount = 0},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = k_PondPit + glm::vec2(12.0f, -6.0f), .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_PondPit + glm::vec2(-10.0f, -8.0f), .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = k_PondPit + glm::vec2(6.0f, -14.0f), .joinTown = true},
	};
}
} // namespace

void testbed_scenarios::AddFishScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.fish_farm",
	    .name = "A fish farm's shoal",
	    .facet = Facet::Hand,
	    .description = "A fish farm on the lake's southern shore, a hut of its town on the bank behind it, seen steeply "
	                   "from above.",
	    .expected = "Fifteen small flat fish swim under the water a few metres out from the shore, each turning after a "
	                "point that moves about within 7 m of the shoal's middle, their tails flicking as they go. Nothing is "
	                "drawn for the farm itself.",
	    .environment = PointerOnTheShoal(),
	    .framing = OverTheShoal(),
	    .objects = FarmAndTown(),
	    // The pointer is held over the shoal, the hand resting on the water
	    .commands = {{.kind = Kind::PointerTo, .point = glm::vec2(0.5f, 0.5f)}},
	});

	all.push_back({
	    .id = "hand.fish_scoop",
	    .name = "Scooping fish out of a fish farm",
	    .facet = Facet::Hand,
	    .description = "The pointer rests over the farm's shoal. Every few seconds the Action button is pressed and held "
	                   "for four seconds, then let go.",
	    .expected = "A press with a fish under the hand locks onto the farm at once: a handful of 25 food comes into the "
	                "hand, a stream of fish leaps from the farm into it and the handful grows every turn while the button "
	                "is held. A press with no fish under the hand grips the water instead, its splash sending the fish "
	                "darting away. As the farm empties fewer fish show.",
	    .environment = PointerOnTheShoal(),
	    .framing = OverTheShoal(),
	    .objects = FarmAndTown(),
	    .commands = {Press(k_Action, 3.0f), Release(k_Action, 4.0f), Press(k_Action, 3.0f), Release(k_Action, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "hand.fish_scare",
	    .name = "Scaring a fish farm's fish",
	    .facet = Facet::Hand,
	    .description = "The pointer rests a little off the farm's shoal, over open water. Every three seconds the button "
	                   "that grips the land is pressed and let go at once, the hand gripping the water.",
	    .expected = "Each grip splashes, and the fish within 8 m of it turn straight away from it and dart off four times "
	                "as fast, slowing over two seconds; the whole shoal then heads for a new point 2 m from its middle.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.35f)},
	    .framing = OverTheShoal(),
	    .objects = FarmAndTown(),
	    .commands = {Press(k_Grip, 3.0f), Release(k_Grip, 0.1f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "needs.fish_and_eat",
	    .name = "Hungry creature fishes",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger on the bank above the lake, a fish farm on the shore below it and no other food "
	                   "anywhere.",
	    .expected = "It walks down to within 15 m of the farm's shoal, a bundle of food half as big again as it appears at "
	                "its feet out of the water, and it picks it up and eats it; the farm's fish stay as they were.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 120.0f},
	        .facingDegrees = 180.0f,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = creature_desires::Desire::Hunger, .fraction = 1.0f}},
	    }},
	    .objects = FarmAndTown(),
	});

	all.push_back({
	    .id = "needs.fish_for_town",
	    .name = "Compassionate creature fishes for its town",
	    .facet = Facet::Needs,
	    .description = "A tiger that wants above all to be kind stands on the plain by the pond north-west of the "
	                   "middle; a fish farm is on the pond's bank, the town's storage pit and three of its people beyond.",
	    .expected = "Its compassion is for the town, and while the town wants food it sets about giving fish to the "
	                "storage pit: it walks into the pond's shallows to within 15 m of the shoal, a bundle of food appears at "
	                "its feet out of the water and it picks it up, walks back to throwing distance of the pit, turns to "
	                "face it and throws the food in. It stands until the food has come down, then plans again.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = k_PondCreature,
	        .facingDegrees = 0.0f,
	        .needs = {.energy = 1.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = creature_desires::Desire::Compassion, .fraction = 1.0f}},
	    }},
	    .objects = PondFarmAndPit(),
	});
}
