/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a town growing from the buildings it planned

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using creature_desires::Desire;

namespace
{
/// Where the town's village centre, storage pit and one built hut stand
constexpr glm::vec2 k_Centre {0.0f, 50.0f};
constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
constexpr glm::vec2 k_Hut {25.0f, 30.0f};
/// Where the town plans three more huts, and a crèche
constexpr std::array<glm::vec2, 3> k_PlannedHuts {{{-15.0f, 5.0f}, {5.0f, 5.0f}, {25.0f, 5.0f}}};
constexpr glm::vec2 k_PlannedCreche {-40.0f, 60.0f};
/// A crèche standing nearly built, for its town's builders to finish
constexpr glm::vec2 k_NearlyBuiltCreche {-5.0f, -20.0f};
constexpr float k_CrecheBuilt = 0.95f;
/// Plenty of wood and food in the pit, so that building is never held up for either
constexpr uint32_t k_PitStock = 5000;
/// The town's grown villagers, too many for its one hut
constexpr size_t k_Couples = 6;
constexpr float k_VillagerSpacing = 3.0f;
/// Four couples, room enough in three huts
constexpr size_t k_MendingCouples = 4;
/// Two fields east of the town
constexpr std::array<glm::vec2, 2> k_Fields {{{55.0f, 40.0f}, {55.0f, 15.0f}}};
} // namespace

void openblack::testbed_scenarios::AddTownGrowthScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	};
	for (const auto& place : k_PlannedHuts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = place, .yawDegrees = 180.0f, .planned = true});
	}
	objects.push_back({.type = AbodeInfo::CelticCreche, .offset = k_PlannedCreche, .planned = true});
	for (size_t i = 0; i < k_Couples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_Couples) * 0.5f)) * k_VillagerSpacing;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.growth_plans",
	    .name = "A town growing from its plans",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit full of wood and food and one "
	                   "built hut, and twelve grown villagers, too many for the hut. The town plans three more huts in a "
	                   "row in front and a crèche to the west; none of them stands. Best watched at game speed 4.",
	    .expected = "The town wants homes, so its villagers ask it, at most once a game turn, to start the hut it wants "
	                "most: the first planned hut appears with nothing built and a site, builders fetch wood from the pit "
	                "and raise it stroke by stroke. Each further hut starts only once no site wants more builders, and "
	                "huts stop being started once there is room for a tenth more than the town's people. The crèche is "
	                "started only once the town wants a civic building. A hut destroyed by the inspector "
	                "(edit.destroy) is planned again by the town where it stood, and built again when the town wants it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_PlannedCreche, k_Hut, k_PlannedHuts[0]}, .distance = 0.6f},
	    .objects = std::move(objects),
	});

	// A town that rebuilds a finished building it lost, unasked, as a repair
	std::vector<ObjectSetup> mending {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[0], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[2], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[1], .yawDegrees = 180.0f, .planned = true, .wasBuilt = true},
	};
	for (size_t i = 0; i < k_MendingCouples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_MendingCouples) * 0.5f)) * k_VillagerSpacing;
		mending.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		mending.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.mending",
	    .name = "A town rebuilding a building it lost",
	    .facet = Facet::Objects,
	    .description = "A Celtic town with a village centre, a full storage pit and three built huts for its four "
	                   "couples. Where the middle hut of the front row stood is the town's plan "
	                   "of it, a hut that had been finished and went from the land. Best watched at game speed 4.",
	    .expected = "At its first turn the town puts the lost hut up for mending before any villager asks it to (its "
	                "askedToBuildTurn stays empty): it appears at "
	                "once with nothing built and a site, and builders fetch wood from the pit and raise it again. A "
	                "built hut hurt with the inspector gets a site at once, as before; the town puts up no more than "
	                "one building a turn of its own accord.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit, k_Hut, k_PlannedHuts[0]}, .distance = 0.6f},
	    .objects = std::move(mending),
	});

	// A town finishing a crèche its people then dance round
	std::vector<ObjectSetup> finishing {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[0], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[2], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticCreche, .offset = k_NearlyBuiltCreche, .built = k_CrecheBuilt},
	};
	for (size_t i = 0; i < k_MendingCouples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_MendingCouples) * 0.5f)) * k_VillagerSpacing;
		finishing.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		finishing.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "villagers.new_building_dance",
	    .name = "Villagers dancing round a new building",
	    .facet = Facet::Objects,
	    .description = "A Celtic town with a village centre, a full storage pit, three huts for its four couples and a "
	                   "crèche just south of the middle nearly built, its site open. Best watched at game speed 4.",
	    .expected = "Builders fetch wood and finish the crèche. As it is finished the town's people within the reaction's "
	                "reach come to dance round it in the small or large building dance by how wide it is, "
	                "not impressed by it; one the dance has no room for stands looking at it, impressed. Once the "
	                "reaction's time is over the dancers go back to what they were doing and the dance goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit, k_Hut, k_NearlyBuiltCreche}, .distance = 0.6f},
	    .objects = std::move(finishing),
	});

	// Two couples sent to make love, one side by side and one face to face, as a breeder sends a villager
	all.push_back({
	    .id = "villagers.kissing",
	    .name = "Villagers meeting to kiss",
	    .facet = Facet::Objects,
	    .description = "Two grown couples on open land: the man of each is sent to make love with his woman a second in, "
	                   "as a breeder sends one. The first couple stand eight metres apart side by side, the second "
	                   "twelve metres apart facing each other.",
	    .expected = "Each pair walk towards each other to meet halfway, stopping a little apart, turn to face each "
	                "other and kiss, each in their own kiss, then go about their business when their time together is "
	                "up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, -2.0f}, {6.0f, 10.0f}}, .distance = 0.6f},
	    .objects = {{.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, 0.0f}, .mateWith = 1},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 2.0f}, .yawDegrees = 0.0f, .mateWith = 3},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 14.0f}, .yawDegrees = 180.0f}},
	});

	// A town's people come across a creature standing still on the edge of their village
	all.push_back({
	    .id = "villagers.creature_curious",
	    .name = "Villagers inspecting a creature",
	    .facet = Facet::Objects,
	    .description = "A hut of the player's town north of the middle with six of its people about it, and the player's "
	                   "tiger standing still, its mind paused, thirty metres to the east.",
	    .expected = "The town has never seen the creature, so it is curious: the villagers within reach take up its "
	                "reaction and stand looking at it from where they are for a second and a half to four and a half, "
	                "then walk to twenty metres from it and look again for one and a half to eight and a half seconds, "
	                "and if there is time step to twelve metres, then four. Fifteen seconds after the town first noticed it, "
	                "with the creature doing "
	                "nothing, the town grows indifferent and they all go back to what they were doing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 25.0f}, {35.0f, 55.0f}}, .distance = 0.7f},
	    .creatures = {{.label = "watched",
	                   .species = CreatureType::Tiger,
	                   .offset = {30.0f, 40.0f},
	                   .facingDegrees = 270.0f,
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                   .hold = true,
	                   .pauseMind = true}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, 34.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, 46.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 40.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 30.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-14.0f, 50.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 34.0f}, .joinTown = true}},
	});

	// An evil creature does a fart in a village. The villagers that are very near to it faint
	all.push_back({
	    .id = "villagers.creature_fart",
	    .name = "Villagers fainting from a creature's stink",
	    .facet = Facet::Objects,
	    .description = "A hut of the player's town with six of its people about it, three of them standing right at an "
	                   "evil tiger's feet, and the tiger, facing west, told to break wind every so often.",
	    .expected = "The tiger squats as for a poo. All through the squat's loop, every fifth frame drawn, a cloud of "
	                "grey dust blows out behind it and a white smoke starts afresh at its groin; the last smoke is left "
	                "rising where it squatted. No lump is dropped. The tiger looking evil, each burst makes a fainting "
	                "reaction, which starts a metre wide and grows only slowly, so only the villagers right at its feet "
	                "faint: they stop where they are (no clip of their own shows it) until the reaction's time is up, "
	                "then go back to what they were doing. Those further off don't faint.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 25.0f}, {15.0f, 55.0f}}, .distance = 0.7f},
	    .creatures = {{.label = "evil",
	                   .species = CreatureType::Tiger,
	                   .offset = {0.0f, 40.0f},
	                   .facingDegrees = 270.0f,
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .alignment = -1.0f,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.9f, .life = 1.0f},
	                   .hold = true}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.4f, 40.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-0.4f, 40.3f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 39.6f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 30.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-14.0f, 50.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 34.0f}, .joinTown = true}},
	    .commands = {{.kind = Command::Kind::Fart, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Command::Kind::Fart, .creature = 0, .delaySeconds = 20.0f, .waitUntilFree = true}},
	    .repeatFrom = 1,
	});

	// A creature that needs to poo, near a village. It chooses a thing to poo by
	all.push_back({
	    .id = "needs.poo_by_village",
	    .name = "Pooing by a village",
	    .facet = Facet::Needs,
	    .description = "A tiger full of poo, its desire to go as strong as it gets, 30 m east of a hut of the player's "
	                   "town with four of its people, two trees and a rock about it.",
	    .expected = "The tiger squats for a poo or for a fart. A third of the time it shows it needs a poo first. For a "
	                "fart it does the steps of a poo by itself, so it does not walk and does not turn. In a poo, all "
	                "through the squat's loop, every fifth frame drawn, a lump of poo comes out at its rear and is thrown "
	                "back along the ground, and its desire to poo is held back for a minute.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 25.0f}, {35.0f, 55.0f}}, .distance = 0.7f},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = {30.0f, 40.0f},
	                   .facingDegrees = 90.0f,
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.95f, .life = 1.0f},
	                   .desires = {{.desire = Desire::Poo, .fraction = 1.0f}}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, 34.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, 46.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 40.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 30.0f}, .joinTown = true},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 60.0f}},
	                {.type = TreeInfo::Beech, .offset = {14.0f, 22.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {20.0f, 48.0f}}},
	});

	// A creature told to poo. The lumps come out during the squat
	all.push_back({
	    .id = "needs.poo_lumps",
	    .name = "Lumps of poo",
	    .facet = Facet::Needs,
	    .description = "A tiger full of poo, facing east, told to poo every so often.",
	    .expected = "A third of the time the tiger shows it needs a poo first. Then it squats. All through the squat's "
	                "loop, every fifth frame drawn, a lump of poo comes out at its rear and is thrown back along the "
	                "ground behind it, so a row of lumps lies behind it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = {0.0f, 40.0f},
	                   .facingDegrees = 90.0f,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.95f, .life = 1.0f},
	                   .hold = true}},
	    .commands = {{.kind = Command::Kind::Poo, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Command::Kind::Poo, .creature = 0, .delaySeconds = 20.0f, .waitUntilFree = true}},
	    .repeatFrom = 1,
	});

	// The same village, set to respect or to fear a creature standing near it
	const auto creatureFeeling = [&all](std::string_view id, std::string_view name, PlayerNames owner,
	                                    ecs::creature_attitude::Attitude feeling, bool thinks, std::string_view description,
	                                    std::string_view expected) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Objects,
		    .description = description,
		    .expected = expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 25.0f}, {35.0f, 55.0f}}, .distance = 0.7f},
		    .creatures = {{.label = "watched",
		                   .species = CreatureType::Tiger,
		                   .offset = {30.0f, 40.0f},
		                   .facingDegrees = 270.0f,
		                   .owner = owner,
		                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
		                   .hold = true,
		                   .pauseMind = !thinks}},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, 40.0f}},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, 34.0f}, .joinTown = true},
		                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, 46.0f}, .joinTown = true},
		                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 40.0f}, .joinTown = true},
		                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 30.0f}, .joinTown = true},
		                {.type = VillagerInfo::CelticFishermanMale, .offset = {-14.0f, 50.0f}, .joinTown = true},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 34.0f}, .joinTown = true}},
		    .townFeelingAboutCreatures = feeling,
		});
	};
	creatureFeeling(
	    "villagers.creature_respect", "Villagers bowing to a creature", PlayerNames::PLAYER_TWO,
	    ecs::creature_attitude::Attitude::Respect, false,
	    "The village of six by its hut, with another player's tiger standing still thirty metres to the east. The town "
	    "respects the tiger from the start, as if it had just done something kind.",
	    "The villagers within reach take up the tiger's reaction and, respecting it, walk towards it, turn quickly to face "
	    "it and bow for four seconds, turning to face it again if it moves round them, then go back to what they were "
	    "doing. Ten seconds after the start the town's respect lapses into indifference and no one takes the reaction "
	    "up again.");
	creatureFeeling("villagers.creature_fear", "Villagers running from a creature", PlayerNames::PLAYER_ONE,
	                ecs::creature_attitude::Attitude::Fear, true,
	                "The village of six by its hut, with the player's tiger thirty metres to the east, left to its own "
	                "mind. The town fears the tiger from the start, as if it had just thrown something.",
	                "The villagers within reach take up the tiger's reaction and, frightened, run from it: while the tiger "
	                "means to act on something each sets off now and then (one turn in ten) at half its fleeing speed "
	                "to a point straight away from it, 1.2 times the reaction's reach off; with the tiger meaning nothing "
	                "they stand. Thirty seconds after the tiger last frightened the town its fear lapses into "
	                "indifference and they go back to what they were doing.");

	// A burning cart near a town's people, taken up in the hand
	all.push_back({
	    .id = "villagers.burning_in_hand",
	    .name = "Villagers running from a burning thing in the hand",
	    .facet = Facet::Objects,
	    .description = "A hut of the player's town north of the middle with six of its people about it, and a cart burning "
	                   "a hundred metres south of them, too far off for its fire to alarm them. A second in, the hand, held "
	                   "still over the village, takes the burning cart up.",
	    .expected = "Taken into the hand, the cart's fire stops making its fire reaction and makes a burning thing in "
	                "the hand's reaction instead: the villagers no further from it than the "
	                "reaction's least running distance (or any in reach while it moves at 3 metres a second or more) run "
	                "straight away from it, then turn to watch it from a safe distance, giving up once it is out of reach.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 25.0f}, {20.0f, 55.0f}}, .distance = 0.7f},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {-4.0f, -60.0f}, .alight = true},
	                {.type = AbodeInfo::CelticHut, .offset = {-10.0f, 46.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, 34.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-8.0f, 36.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 40.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 30.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {4.0f, 44.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, 32.0f}, .joinTown = true}},
	    .commands = {{.kind = Command::Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	    .hand = HandHold {.offset = {-4.0f, 38.0f}, .height = 6.0f},
	});

	// Villagers near and far from a thing that came to rest, each watching it for its own time
	all.push_back({
	    .id = "villagers.reaction_by_distance",
	    .name = "Villagers watching a thing near and far",
	    .facet = Facet::Objects,
	    .description = "A cart in the middle of open land makes a reaction to look at it, of the kind a miracle makes to "
	                   "be looked at (one that never runs out by itself). Six villagers stand in a line east of it, 2, "
	                   "12, 22, 32, 42 and 52 metres away.",
	    .expected = "Each villager takes the reaction up and turns to watch the cart. How long each keeps watching goes by "
	                "its distance, as the reaction table's row says: the furthest gives up first, about nine seconds in, "
	                "and each nearer one a little later, the nearest last, about twelve seconds in; each soon takes it "
	                "up again and watches for as long again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-4.0f, -6.0f}, {56.0f, 6.0f}}, .distance = 0.6f},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, 0.0f}, .reaction = Reaction::LookAtSpell},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {2.0f, 0.0f}, .yawDegrees = 0.0f},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {12.0f, 0.0f}, .yawDegrees = 0.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {22.0f, 0.0f}, .yawDegrees = 0.0f},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {32.0f, 0.0f}, .yawDegrees = 0.0f},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {42.0f, 0.0f}, .yawDegrees = 0.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {52.0f, 0.0f}, .yawDegrees = 0.0f}},
	});

	// A creature seeing a thing crushed in front of it and another behind it
	all.push_back({
	    .id = "creatures.reaction_examine",
	    .name = "A creature looking at a thing crushed",
	    .facet = Facet::Objects,
	    .description = "A creature stands facing south, towards the camera. A cart 20 metres in front of it makes the "
	                   "reaction of a thing crushed by the player; a second cart 20 metres behind it makes the same "
	                   "reaction.",
	    .expected = "The creature drops what it was doing and goes to look at the cart in front of it, which it sees; the "
	                "one behind it, out of its sight, it takes no notice of. For a minute after it takes no notice of "
	                "another thing crushed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {35.0f, 12.0f, -10.0f}, .look = {0.0f, 6.0f, -10.0f}},
	    .creatures = {{.offset = {0.0f, 0.0f}, .facingDegrees = 0.0f}},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToObjectCrushed},
	                {.type = MobileObjectInfo::Cart, .offset = {0.0f, 20.0f}, .reaction = Reaction::ReactToObjectCrushed}},
	});

	// A town whose store has run out of food, with fields to farm
	std::vector<ObjectSetup> hungry {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 0, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[0], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[2], .yawDegrees = 180.0f},
	};
	for (const auto& place : k_Fields)
	{
		hungry.push_back({.type = FieldTypeInfo::Wheat, .offset = place});
	}
	for (size_t i = 0; i < k_MendingCouples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_MendingCouples) * 0.5f)) * k_VillagerSpacing;
		hungry.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		hungry.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.food_jobs",
	    .name = "A town out of food, with fields",
	    .facet = Facet::Objects,
	    .description = "A Celtic town with a village centre, three huts for its four couples, a storage "
	                   "pit with no food in it (wood plenty) and two bare wheat fields to the east. Best watched at game "
	                   "speed 4.",
	    .expected = "The town wants food. Its villagers deciding what to do are offered to its desires, the most wanted "
	                "first, and take up farming the fields, sowing them, then harvesting and carrying the food to the "
	                "pit.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit, k_Hut, k_Fields[1]}, .distance = 0.6f},
	    .objects = std::move(hungry),
	});
}
