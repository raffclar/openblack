/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRegistry.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/FlatLand.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Particles/ParticleTypes.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioBuilders.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

// How to add a scenario: append one to the list Build() returns, in the section of its facet. Give it a new id
// ("facet.what", never reused or renamed, as tests and the command line pick scenarios by it), a name, its facet, a
// description of what it sets up and what to look for, and then its data: the environment (land, hour, weather, body
// time), the framing, the creatures with the bodies, needs and desires they start with, the objects on the land, and
// the commands, which play in turn (commands on objects name them by their place in the scenario's objects). Offsets are from
// the middle of the map, x east and y north; the testbed's camera looks north from 120 units south of the middle, and the lake
// lies beyond the middle (k_Lake, its open water 100 units a side, ringed by 20 units of shallows and a bank). Keep other
// scenarios clear of it. A new facet goes into the Facet enum and its Name. test_testbed_scenarios
// checks the data of every scenario; anything a scenario needs that the runner can't yet do goes into these types and
// TestbedScenarioRunner together.

namespace
{
void AddWorkshopScenarios(std::vector<Scenario>& all)
{
	constexpr glm::vec2 k_Workshop {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.workshop_makes_scaffolds",
	    .name = "A workshop makes scaffolds from its wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the wood of "
	                   "four scaffolds. The yard of the workshop faces the camera.",
	    .expected = "A pile of logs stands at the back of the workshop and holds its wood. The workshop uses the wood of "
	                "one scaffold from the pile, and the pile becomes smaller. The work sound starts. When the time to make a "
	                "scaffold is over, a small scaffold appears in the first free place of the yard, a horn sounds and "
	                "the work sound stops. The workshop makes scaffolds until it has three in its yard. The hand can "
	                "take a scaffold from the yard, and the workshop then makes one more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	all.push_back({
	    .id = "objects.workshop_graveyard_burial_scaffold",
	    .name = "A burial on a workshop that stands in for the graveyard makes a scaffold with no wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a workshop with no wood, its yard towards the "
	                   "camera, and a graveyard. After one second the graveyard goes, and the workshop becomes the "
	                   "graveyard of the town. After two seconds a villager of the town with little life catches fire.",
	    .expected = "The villager burns and dies. The workshop counts the burial: its scaffold shows over the workshop "
	                "from then on. On the next turn of the workshop a small scaffold appears in the first place of the "
	                "yard and a horn sounds, although the workshop has no wood. The work sound does not play. The "
	                "inspector shows the stand-in graves count 1 on the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = AbodeInfo::CelticGraveyard, .offset = {30.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .joinTown = true,
	                 .life = 0.05f}},
	    .commands = {{.kind = Command::Kind::RemoveObject, .delaySeconds = 1.0f, .object = 2},
	                 {.kind = Command::Kind::SetAlight, .delaySeconds = 1.0f, .value = 1, .object = 3}},
	    .townBeliefInPlayer = 1.0f,
	});

	// The scenario turns the workshop so that its yard faces the camera. The first place of the Celtic workshop's yard is
	// 7.8 m in front of the workshop.
	constexpr glm::vec2 k_FirstPlace = k_Workshop + glm::vec2 {0.0f, -7.8f};
	all.push_back({
	    .id = "objects.scaffold_snaps_into_yard",
	    .name = "A scaffold put down near a workshop's yard goes into its place",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop with no wood, its yard towards the "
	                   "camera, and a loose "
	                   "scaffold that no workshop counts. After one second the scaffold is lifted and let go one metre "
	                   "from the first place of the yard, as the hand puts one down.",
	    .expected = "The scaffold comes to rest and moves into the first place of the yard, turned as the place is "
	                "turned. No puff shows. The workshop now counts the scaffold as its own (the inspector shows it), "
	                "and the physics does not move the scaffold while it stands there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop + glm::vec2 {10.0f, 10.0f}, .scale = 0.7f}},
	    .throws = {{.object = 2,
	                .from = k_FirstPlace + glm::vec2 {0.7f, -0.7f},
	                .height = 1.0f,
	                .velocity = {0.0f, -1.0f, 0.0f},
	                .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "objects.scaffold_tap_breaks",
	    .name = "A tap breaks one scaffold off a scaffold, and a storage pit takes a scaffold as wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit, and a loose scaffold that "
	                   "is worth three scaffolds and that no workshop counts. Put the hand over the scaffold, tap it, "
	                   "then pick up a part and hold it over the other part and over the storage pit.",
	    .expected = "Over the scaffold the tooltip shows \"Scaffold Value\" with 3, and \"Tap\". A tap breaks one small "
	                "scaffold off. The two parts move apart along a quarter turn of the scaffold and one of four sounds "
	                "plays. The old scaffold is now worth 2. A held part over the other part shows \"Combine "
	                "Scaffolds\". Over the storage pit it shows \"Supply\", and the pit takes the wood of all the "
	                "scaffolds that the part is worth. Elsewhere the tooltip shows the name of the building that the "
	                "phantom shows, or \"Build\", then \"Throw\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-15.0f, 0.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop, .scale = 1.1f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_by_hand",
	    .name = "The hand takes wood from a workshop's pile and gives it back",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood, less "
	                   "than the wood of one scaffold, so the workshop makes no scaffold. The pile of logs of the "
	                   "workshop faces the camera. The input tools/sim/hand_inputs/workshop_wood.json moves the hand.",
	    .expected = "With a held press on the pile, the hand takes logs from it, and the pile becomes smaller. The "
	                "tooltip of the workshop shows less wood. When the hand lets the logs go on the workshop, the "
	                "workshop takes them into its pile again, and the tooltip shows the wood that it had at the start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "objects.workshop_takes_thrown_wood",
	    .name = "A workshop takes a handful of wood that the hand throws at it",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood. A "
	                   "handful of 600 wood is lifted 20 m west of the workshop and thrown at it as the hand throws it.",
	    .expected = "The handful flies east and comes down at the workshop. It goes, and the workshop takes the 600 wood "
	                "into its pile: the workshop then holds 1600 wood, still less than the wood of one scaffold.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {25.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000},
	                {.type = PotInfo::HandWood, .offset = k_Workshop + glm::vec2 {-20.0f, 0.0f}, .amount = 600}},
	    .throws = {{.object = 2,
	                .from = k_Workshop + glm::vec2 {-20.0f, 0.0f},
	                .height = 6.0f,
	                .velocity = {11.0f, 3.0f, 0.0f},
	                .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_flag",
	    .name = "The flag of a workshop shows the wood that the workshop needs for a scaffold",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold. The input tools/sim/hand_inputs/workshop_wood.json moves the hand: "
	                   "it takes logs from the pile of the workshop, then gives them back.",
	    .expected = "The wood flag flies beside the workshop and turns to face the camera. When the hand takes logs, the "
	                "flag moves up smoothly, because the workshop needs more wood. When the hand gives the logs back, the "
	                "flag moves down to its first height.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_wood_flag",
	    .name = "The hand over the wood flag of a workshop",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold, and has room for three scaffolds. Put the hand over the wood flag "
	                   "beside the workshop, also out of the influence of the player.",
	    .expected = "Over the flag the tooltip shows \"Wood Required\" with the wood of three scaffolds less 1000, at "
	                "once and with no mouse button drawn. It shows also where the hand is out of the influence.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_production",
	    .name = "The hand over a workshop that makes a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the "
	                   "wood of four scaffolds. The hand is over the workshop, in the influence of the player.",
	    .expected = "The workshop starts a scaffold, and its chimney smokes. Over the workshop the tooltip shows "
	                "\"Production Completed\" with the part of the scaffold that the workshop made, as a whole percent "
	                "that goes up from 0 to 99. When the scaffold is finished, the percent starts again at 0 for the next "
	                "scaffold. When the yard holds three scaffolds, the workshop stops, the tooltip shows 0, and the "
	                "smoke stops.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Workshop.x, 22.0f, k_Workshop.y - 32.0f},
	                .look = {k_Workshop.x, 3.0f, k_Workshop.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	// The place where the hand puts the villager down, in front of the workshop and beside its yard
	constexpr glm::vec2 k_DropPlace = k_Workshop + glm::vec2 {6.0f, -6.0f};
	all.push_back({
	    .id = "villagers.craftsman_supplies_workshop",
	    .name = "Craftsmen bring the wood of the storage pit to the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village centre, "
	                   "a storage pit that holds 3000 wood 25 m to the west, and a built workshop with no wood. A "
	                   "craftsman disciple stands by the workshop. A grown villager of the town, not a disciple, stands "
	                   "to the east. After one second the hand takes that villager, and after three more seconds it puts "
	                   "the villager down by the workshop. Best watched at game speed 4.",
	    .expected = "The villager that the hand puts down by the workshop becomes a craftsman disciple: it glows and the "
	                "craftsman's line plays. Each craftsman walks to the storage pit, takes as much wood as it can carry, "
	                "walks to the workshop and puts the wood in. When the workshop has the wood of a scaffold, it makes a "
	                "scaffold. The town's desire for wood (the inspector's living.town_desires, desire 1) adds the share of "
	                "the adults that are craftsmen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_DropPlace.x - 10.0f, 30.0f, k_DropPlace.y - 40.0f},
	                .look = {k_DropPlace.x, 0.0f, k_DropPlace.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 3000},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {-6.0f, -8.0f},
	                 .joinTown = true,
	                 .disciple = VillagerDisciple::Craftsman},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {20.0f, -8.0f},
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 4},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    // The town belongs to the player at the start. Thus the hand of the player holds a villager of its own.
	    .townBeliefInPlayer = 1000.0f,
	});

	constexpr glm::vec2 k_LooseScaffold {-25.0f, 30.0f};
	constexpr glm::vec2 k_BuildPlace {-5.0f, 25.0f};
	constexpr glm::vec2 k_FoundingScaffold {-60.0f, 80.0f};
	all.push_back({
	    .id = "objects.scaffold_starts_building",
	    .name = "A scaffold put down near a town starts the building that it offers",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a loose scaffold of the value of one "
	                   "scaffold. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still near the village centre, the phantom of the building that "
	                "the town wants grows below the scaffold and turns slowly. When the hand puts the scaffold down, no "
	                "puff shows, a sound plays, and a building site of that building opens where the scaffold is. The "
	                "scaffold stays on the site. While nothing of the building is built, the hand can take the scaffold "
	                "again, and the site then goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_JoinTarget {-25.0f, 18.0f};
	all.push_back({
	    .id = "hand.scaffold_phantom_sounds_and_join",
	    .name = "A held scaffold plays the sounds of its phantom, names the building, and joins a scaffold",
	    .facet = Facet::Hand,
	    .description = "A Celtic town of the player's with a village centre, a loose scaffold of the value of two "
	                   "scaffolds and, 12 m south of it, a loose scaffold of the value of one. Take the larger "
	                   "scaffold and hold it still near the village centre. Then hold it over the smaller scaffold and "
	                   "press the Action button. Run with the audio log at the debug level to see the sounds.",
	    .expected = "Near the village centre the phantom of the building that the town wants grows below the scaffold, "
	                "and the appear sound plays one time. The tooltip shows the name of that building, then \"Throw\". "
	                "When the offer changes or goes, the phantom becomes small and the vanish sound plays one time. Over "
	                "the smaller scaffold the tooltip shows \"Combine Scaffolds\". The press removes the held scaffold. "
	                "The scaffold on the land now has the value of three. A puff shows at it, and the first of the four "
	                "join sounds plays at the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_JoinTarget - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.9f},
	                {.type = ScaffoldInfo::Normal, .offset = k_JoinTarget, .scale = 0.7f}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "light.snow_on_scaffold_phantom",
	    .name = "The phantom of a scaffold carries snow in the hand and none on its site",
	    .facet = Facet::Light,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, in deep snow over the "
	                   "whole island. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the building grows below the scaffold, and "
	                "snow comes onto it in about four seconds. When the hand puts the scaffold down, the phantom on the "
	                "building site shows no snow, and it never shows snow again. The scaffold itself shows snow as "
	                "other things do.",
	    .environment = {.dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_ImpressedHut {-25.0f, 47.0f};
	all.push_back({
	    .id = "objects.scaffold_impresses_town",
	    .name = "A scaffold that the player puts down impresses the people of the near town",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with a hut of four "
	                   "villagers of the town beside the village centre. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand puts the scaffold down, a building site opens. At the same time, symbols of belief in "
	                "the player rise from the first villagers of the hut: from a part of the town, and from one villager "
	                "at least. The belief of the town in the player becomes larger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = AbodeInfo::CelticHut, .offset = k_ImpressedHut},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_ImpressedHut + glm::vec2 {-6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {-3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_ImpressedHut + glm::vec2 {3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_offers_wonder",
	    .name = "A scaffold of the value of seven offers the town a wonder",
	    .facet = Facet::Objects,
	    .description = "The town and the camera of objects.scaffold_starts_building. The town believes in the player, "
	                   "so it wants a wonder. The loose scaffold has the value of seven scaffolds. An oak stands near the "
	                   "place where the hand puts the scaffold down. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the Celtic wonder grows below the scaffold. "
	                "Its size comes from the wonder power at that place: the desire of the town for a wonder, and the "
	                "things near the place. When the hand puts the scaffold down, a building site of the wonder opens. "
	                "The inspector query town.wonders shows the wonder and its power. When the wonder is finished, the "
	                "most Celtic power of the player becomes larger by the power of the wonder.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 1.9f},
	                {.type = TreeInfo::Oak, .offset = k_BuildPlace + glm::vec2 {-4.0f, -6.0f}}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "objects.scaffold_site_destroyed_starts_again",
	    .name = "A scaffold starts a new site when an effect destroys its unfinished building",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies, as when the hand puts it down. "
	                   "After four seconds more, an effect destroys the unfinished building of the site, as when a fire "
	                   "takes the last of its life.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building. When the effect destroys "
	                "the building, a ghost of the building flickers out, and at once the scaffold opens a new site where "
	                "it lies. The new site can show another building or another angle, because the town looks for its "
	                "offer again. The planting sound plays, and the hand can take the scaffold again for a time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::DestroyScaffoldSite, .delaySeconds = 4.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_site_phantom_see_through",
	    .name = "The phantom of the building on a scaffold's site draws in the see-through modes",
	    .facet = Facet::Objects,
	    .description = "The town and the scaffold of objects.scaffold_starts_building, with the camera near the scaffold. "
	                   "After one second the scaffold starts the building that the town offers where it lies, as when "
	                   "the hand puts it down.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building over the site at about a "
	                "third of its alpha. Each material of the phantom draws in its see-through mode with the alpha "
	                "reference 10, which becomes 0 at the alpha of the phantom. Thus no part of the phantom is cut: also "
	                "the edges that its textures make clear show at the alpha of their texels, and write depth.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_LooseScaffold - glm::vec2 {12.0f}, k_LooseScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_script_active",
	    .name = "A script sets a scaffold active, and the scaffold starts its building at once",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second a "
	                   "script sets the scaffold inactive. After one second more, the script sets the scaffold active.",
	    .expected = "The first call does not change the scaffold. At the second call, a building site opens at the "
	                "scaffold, with the phantom of the building that the town offers where the scaffold lies. No player "
	                "put the scaffold down, so the planting sound does not play for the local player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}}},
	});

	all.push_back({
	    .id = "objects.wood_poured_on_scaffold_site",
	    .name = "The hand pours wood onto a scaffold on a site, and the site gets the wood",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies. After three seconds more, the "
	                   "hand of the player pours 300 wood over the scaffold.",
	    .expected = "A building site opens at the scaffold. The poured wood goes into the site, as into a store. No pile "
	                "of wood stays on the ground at the scaffold. The inspector shows the wood of the site go up by the "
	                "wood that the hand poured.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::HandPour, .delaySeconds = 3.0f, .point = k_LooseScaffold, .value = 300}},
	});

	all.push_back({
	    .id = "objects.scaffold_founds_town",
	    .name = "A scaffold put down away from every town founds a new town",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's in the middle of the map, with no buildings, and a scaffold of the "
	                   "town with the value of five scaffolds, 100 m away from the town. After one second the scaffold "
	                   "starts the building that it offers where it lies, as when the hand puts it down.",
	    .expected = "A building site of a Celtic village centre opens at the scaffold, with the phantom of the village "
	                "centre. The inspector query town.list shows a new Celtic town of the player's with the id 703058. "
	                "The new town believes in the player with the starting belief of a new town, and has no reach of "
	                "its own until it has buildings.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {20.0f}, k_FoundingScaffold + glm::vec2 {20.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.5f, .joinTown = true}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.burning_scaffold_tap",
	    .name = "A tap on a burning scaffold breaks off a burning piece, and the fire burns the scaffolds away",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that is worth three scaffolds and that no workshop counts, alight as at the start "
	                   "of a fire, with no town near. After one second the hand of the player taps the scaffold.",
	    .expected = "The tap breaks one small scaffold off. The two parts move apart and one of four sounds plays. The "
	                "piece burns as the old scaffold burns: the inspector shows a Fire on the two scaffolds. The fire "
	                "takes the life of each scaffold. When a scaffold has no life, it goes and leaves nothing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {12.0f}, k_FoundingScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.1f, .alight = true}},
	    .commands = {{.kind = Kind::HandTapObject, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground",
	    .name = "A scaffold that a script marked clears its ground when the hand lets it go",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. A script marked the "
	                   "scaffold to destroy the things in its way. A bush and a ball stand where the hand puts the "
	                   "scaffold down. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand lets the scaffold go, the bush and the ball jump out of the way. Each starts from the "
	                "height of the scaffold above the land and flies a short distance. A building site then opens where "
	                "the scaffold is.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_BuildPlace},
	                {.type = MobileObjectInfo::Ball, .offset = k_BuildPlace + glm::vec2 {0.0f, -1.5f}}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_creature",
	    .name = "A scaffold that clears its ground leaves a creature alone",
	    .facet = Facet::Objects,
	    .description =
	        "A loose scaffold that a script marked to destroy the things in its way, away from every town. The "
	        "player's tiger stands still 3 m east of the scaffold, and a bush stands 1.4 m north-east of it. After one "
	        "second the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "The bush jumps out of the way. The tiger stays where it is and stays in the game: the inspector "
	                "still shows it. The game log tells of the push of the bush and of no removal.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_FoundingScaffold + glm::vec2 {3.0f, 0.0f},
	                   .pauseMind = true}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.0f, 1.0f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_fixed_rock",
	    .name = "A scaffold that clears its ground removes a rock and pushes a ball",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way, away from every town. A "
	                   "rock stands 1.5 m east of it and a ball 1.5 m north of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The rock has the fixed mark of the land, so the scaffold does not push it: the rock goes from the "
	                "game at once. The ball has no mark, so it jumps out of the way and stays in the game. The game log "
	                "tells of the removal of the rock and of the push of the ball.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = MobileStaticInfo::Rock, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the rock goes; the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 50,
	                .atLeast = 2,
	                .atMost = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_on_high_land",
	    .name = "A thing that a clearing scaffold pushes starts under high land",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way lies on the land, which is "
	                   "about 21 m high here. A bush stands 1.5 m east of it and a ball 1.5 m north of it. After one second "
	                   "the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "As in the game, the bush and the ball start at the height of the scaffold above the land plus half a "
	                "metre, twice as far from the scaffold as the edge of the scaffold. That is a height of the world, so "
	                "each starts under the land here. The land throws each one far across the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_straight_west_or_south",
	    .name = "A clearing scaffold pushes a thing straight west and a thing straight south",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way. A bush stands 1.5 m "
	                   "straight west of it and a ball 1.5 m straight south of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The bush starts west and the ball starts south of the scaffold, under the land, as in the game. The "
	                "land throws each one far, and the bush becomes a dead tree. The game divides the push by 0 here and "
	                "the things go from the map. openblack divides by the larger size, so both things stay on the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {-1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, -1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 100,
	                .atLeast = 2},
	               {.label = "the ball lands near the scaffold, not at the corner of the map",
	                .quantity = checks::Quantity::ObjectDistance,
	                .index = 2,
	                .point = k_FoundingScaffold,
	                .afterTurns = 100,
	                .atMost = 100.0}},
	});

	all.push_back({
	    .id = "objects.workshop_site_wood_on_its_pile",
	    .name = "The wood for a workshop that goes up lies on the pile of the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit that holds 6000 wood 25 m to the "
	                   "west, four builders of the town, and a workshop that is a tenth built. The workshop holds 1000 "
	                   "wood. Best watched at game speed 4.",
	    .expected = "The wood of the workshop lies on the pile of logs at the back of the workshop. No other pile of wood "
	                "comes for the site. The builders take wood from this pile to build, and they bring wood from the "
	                "storage pit to the workshop. When the workshop is finished, the wood that is left stays on the pile "
	                "as the wood of the workshop. The villagers do not take it away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects =
	        {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	         {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 6000},
	         {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 1000, .built = 0.1f},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-6.0f, -10.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticForesterMale, .offset = k_Workshop + glm::vec2 {3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {6.0f, -10.0f}, .joinTown = true}},
	});

	all.push_back({
	    .id = "story.reward_chest_scaffold",
	    .name = "A reward chest gives a town with a workshop a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a built workshop with no wood. After one "
	                   "second a reward chest of a scaffold lies opened in the town, near the workshop.",
	    .expected = "A small scaffold, worth one scaffold and not turned, appears where the chest is. The workshop counts "
	                "it as its own (the inspector shows it in the scaffolds of the workshop), but it does not move into "
	                "the yard.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0}},
	    .rewards = {{.type = RewardObjectInfo::Scaffold,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .fromSky = false,
	                 .inTown = true,
	                 .delaySeconds = 1.0f,
	                 .opened = true}},
	});

	// The tiger and a second loose scaffold stand inside the shot of objects.scaffold_starts_building, away from the
	// hand's way, so that the shot and the hand's input stay the same. The tiger stays still until the input is over.
	// The second scaffold is far from the village centre: the creature walks round the centre, and a scaffold near its
	// edge is out of the creature's reach from there.
	constexpr glm::vec2 k_SiteWatcher {-32.0f, 47.0f};
	constexpr glm::vec2 k_SecondScaffold {-24.0f, 42.0f};
	constexpr float k_WatcherWaits = 20.0f;
	all.push_back({
	    .id = "objects.creature_scaffold_on_site",
	    .name = "A creature leaves a scaffold that a building site keeps",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with the player's tiger "
	                   "and a second loose scaffold north of the scaffold. The tiger stays still for 20 seconds. The input "
	                   "tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds it still "
	                   "near the village centre, and puts it down. After 100 seconds a script tells the tiger to examine "
	                   "the scaffold on the site by picking it up, and after 40 seconds more, the loose scaffold.",
	    .expected = "The scaffold opens a building site. After the time that a hand can take it back, the tiger cannot "
	                "pick it up: a creature takes a scaffold only when a hand can take it. The tiger picks up the loose "
	                "scaffold and looks at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_SiteWatcher,
	                   .pauseMind = true,
	                   .thinksAfterSeconds = k_WatcherWaits}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = ScaffoldInfo::Normal, .offset = k_SecondScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 100.0f, .object = 1, .action = "ExamineByPickingUp"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 2, .action = "ExamineByPickingUp"}},
	});

	// The abode number of a house and the size, as a land script gives Khazar's scaffold for a house
	constexpr float k_HouseNumber = 4.0f;
	constexpr float k_ScriptSize = 2.0f;
	all.push_back({
	    .id = "objects.script_gives_scaffold",
	    .name = "A script makes a scaffold, gives it a building and a size, and locks it",
	    .facet = Facet::Objects,
	    .description = "The town of objects.scaffold_starts_building with no loose scaffold. After one second, the "
	                   "script lines of a land give a scaffold: CREATE makes a scaffold (type 39) where the loose "
	                   "scaffold was, and SET_SCAFFOLD_PROPERTIES gives it the house of abode number 4 and the size 2. "
	                   "The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds "
	                   "it still near the village centre, and puts it down. After ten more seconds, SET_ID_PICKUPABLE "
	                   "and SET_ID_MOVEABLE lock the scaffold.",
	    .expected = "A scaffold appears on the land. It has the size of a scaffold of the value of two scaffolds. While "
	                "the hand holds it still, the phantom shows only a house of abode number 4. When the hand puts it "
	                "down, a building site of that house opens. After the lock, the hand can not take the scaffold, and "
	                "the inspector shows CannotBePickedUp and Immovable on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_LooseScaffold}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HouseNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_ScriptSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 10.0f,
	                  .script = "SET_ID_PICKUPABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ID_MOVEABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	});

	// The workshop's abode number and the size, as the third land's script lays out the workshop of the home town
	constexpr float k_WorkshopNumber = 9.0f;
	constexpr float k_WorkshopSize = 3.0f;
	constexpr glm::vec2 k_RockyPlace {-45.0f, 15.0f};
	all.push_back({
	    .id = "objects.scaffold_builds_among_rocks",
	    .name = "A script scaffold starts its workshop among rocks",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. Four rocks lie 4 m round a place 57 m from the centre. After one second, CREATE makes a "
	                   "scaffold (type 39) at that place, and "
	                   "SET_SCAFFOLD_PROPERTIES gives it the workshop (abode number 9) and the size 3. Then SET_ACTIVE "
	                   "sets the scaffold active, as the third land's script does with the workshop of the home town.",
	    .expected = "A building site of a workshop opens at the scaffold, with the phantom of the workshop over the "
	                "rocks. The rocks do not stop the building. The scaffold stays on the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {-4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, 4.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, -4.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    // A town of the neutral player wants no workshop, so the town stays the player's
	    .townBeliefInPlayer = 1000.0f,
	});
	// A fish farm 10 m from the scaffold, in the middle of the next cell and inside the outline of the workshop
	constexpr glm::vec2 k_FarmPlace {10.0f, 0.0f};
	all.push_back({
	    .id = "objects.scaffold_stopped_by_fish_farm",
	    .name = "A fish farm stops the workshop of a script scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. A fish farm stands 10 m from a place 57 m from the centre. After one second, CREATE makes "
	                   "a scaffold (type 39) at that place, and SET_SCAFFOLD_PROPERTIES gives it the workshop (abode "
	                   "number 9) and the size 3. Then SET_ACTIVE sets the scaffold active.",
	    .expected = "No building site opens at the scaffold, because the fish farm is in the outline of the workshop. "
	                "The scaffold stays where it is, with no phantom of the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = FishFarmInfo::Normal, .offset = k_RockyPlace + k_FarmPlace}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    .townBeliefInPlayer = 1000.0f,
	});

	// The Workshop of the second land with a home town of more than 40 grown ups. The quest's script counts at most 40
	// of them when it finds the share of homeless grown ups. The places are the script's, from the middle of the land.
	constexpr glm::vec2 k_LandTwoMiddle {2240.0f, 2720.0f};
	const auto onLandTwo = [k_LandTwoMiddle](float x, float z) { return glm::vec2(x, z) - k_LandTwoMiddle; };
	constexpr uint32_t k_HomeTown = 0;
	const auto homeWorkshop = onLandTwo(3252.0894f, 3341.0461f);
	std::vector<ObjectSetup> homeTown {
	    {.type = AbodeInfo::NorseWorkshop,
	     .offset = homeWorkshop,
	     .yawDegrees = 160.0f,
	     .landTown = k_HomeTown,
	     .storedWood = 0},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3190.0f, 3440.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3215.0f, 3445.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3170.0f, 3420.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3240.0f, 3450.0f), .landTown = k_HomeTown}};
	constexpr size_t k_GrownUps = 45;
	for (size_t i = 0; i < k_GrownUps; ++i)
	{
		homeTown.push_back(
		    {.type = i % 2 == 0 ? VillagerInfo::NorseHousewifeFemale : VillagerInfo::NorseFarmerMale,
		     .offset = onLandTwo(3226.0f + static_cast<float>(i % 6) * 2.0f, 3405.0f + static_cast<float>(i / 6) * 2.0f),
		     .landTown = k_HomeTown});
	}
	all.push_back({
	    .id = "story.workshop_homeless_cap",
	    .name = "Story: The Workshop with a home town of more than 40 grown ups",
	    .facet = Facet::Story,
	    .description = "The second land of the story. The player's first town gets a workshop at the place of the "
	                   "quest, four houses and 45 grown ups. It then has more than 40 grown ups and room for 20 of "
	                   "them. The quest's script for the finished workshop runs. Play the quest by hand: click the "
	                   "scroll, give wood to the workshop, put its scaffold down as a house and build the house.",
	    .expected = "In the housing step the quest counts 40 grown ups, not more. With room for 20, half of the 40 have "
	                "no home and the step waits. When the room is 25 or more, fewer than 40 in 100 have no home and the "
	                "step ends: the second scroll appears at the workshop. With no limit of 40 the step would wait "
	                "until the room is 30.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {homeWorkshop + glm::vec2(-60.0f, -20.0f), homeWorkshop + glm::vec2(30.0f, 110.0f)}},
	    .objects = homeTown,
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "TheWorkshopComplete",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});
}

std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddRestAndHome(all);
	AddGrowth(all);
	AddAppearance(all);
	AddLight(all);
	AddMovement(all);
	AddFootprints(all);
	AddAudio(all);
	AddObjects(all);
	AddHand(all);
	AddLeash(all);
	AddCombat(all);
	AddMind(all);
	AddParticles(all);
	AddEditor(all);
	AddMiracleScenarios(all);
	AddGlobeScenarios(all);
	AddLifeLightScenarios(all);
	AddCreatureCastingScenarios(all);
	AddFriendActionScenarios(all);
	AddBenchmark(all);
	AddCreatureModeScenarios(all);
	AddSavedCameraScenarios(all);
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddBirdScenarios(all);
	AddTeleportScenarios(all);
	AddVortexScenarios(all);
	AddLandFiveQuestScenarios(all);
	AddLandsFourFiveScrollScenarios(all);
	AddLand4StoneScenarios(all);
	AddLand4SlegScenarios(all);
	AddPuzzleQuestScenarios(all);
	AddUndeadTotemScenarios(all);
	AddRewardScenarios(all);
	AddTornadoScenarios(all);
	AddPhysicsScenarios(all);
	AddNatureScenarios(all);
	AddHandNavigationScenarios(all);
	AddHandLookScenarios(all);
	AddGateScenarios(all);
	AddVillageTotemScenarios(all);
	AddTempleVillagerScenarios(all);
	AddFishScenarios(all);
	AddKnockScenarios(all);
	AddFootballScenarios(all);
	AddVillagerSpeedScenarios(all);
	AddDiscipleScenarios(all);
	AddVillagerLabelScenarios(all);
	AddTempleKeyScenarios(all);
	AddHighlightScenarios(all);
	AddSkyScenarios(all);
	AddAdvisorScenarios(all);
	AddStory(all);
	AddExplorersBoat(all);
	AddTownGrowthScenarios(all);
	AddStatisticsScenarios(all);
	AddMultiplayerFrontEndScenarios(all);
	AddOtherGodScenarios(all);
	AddJoinOnlineScenarios(all);
	AddTutorialScenarios(all);
	AddCreatureDanceScenarios(all);
	AddCreatureLearningScenarios(all);
	AddComputerPlayerScenarios(all);
	AddGameEndScenarios(all);
	AddEndingScenarios(all);
	AddLastStandScenarios(all);
	AddWorldSoundScenarios(all);
	AddTownMiracleScenarios(all);
	AddFixedKindScenarios(all);
	AddCreatureExplorationScenarios(all);
	AddCreatureActionScenarios(all);
	AddScriptTaskRelease(all);
	AddLandOneScriptStubScenarios(all);
	AddVillagerSizeAndReachScenarios(all);
	AddSwayAndFrightScenarios(all);
	AddStoreScenarios(all);
	AddPoisonScenarios(all);
	AddWorkshopScenarios(all);
	AddGodsRingMapScenarios(all);
	AddSelfCheckScenarios(all);
	AddWorshipSiteScenarios(all);
	AddLateSilverScrollScenarios(all);
	return all;
}

bool InRange(float value, float low, float high)
{
	return value >= low && value <= high;
}

void CheckNeeds(const NeedOverrides& needs, std::vector<std::string>& problems, std::string_view who)
{
	const std::array<std::pair<std::string_view, std::optional<float>>, 5> k_UnitNeeds {{
	    {"exhaustion", needs.exhaustion},
	    {"dehydration", needs.dehydration},
	    {"poo", needs.poo},
	    {"life", needs.life},
	    {"energy", needs.energy},
	}};
	for (const auto& [name, value] : k_UnitNeeds)
	{
		if (value.has_value() && !InRange(*value, 0.0f, creature_physiology::k_MaxGrownSize))
		{
			problems.push_back(fmt::format("{}: {} {} out of range", who, name, *value));
		}
	}
	if (needs.warmth.has_value() && !InRange(*needs.warmth, -1.0f, 1.0f))
	{
		problems.push_back(fmt::format("{}: warmth {} out of range", who, *needs.warmth));
	}
}

bool ValidAnimation(Kind kind, size_t animation)
{
	switch (kind)
	{
	case Kind::PlayAction:
		return animation >= animations::k_FirstAction && animation < animations::k_FirstAction + animations::k_ActionCount;
	case Kind::PlayGesture:
		return animation >= animations::k_FirstGesture && animation < animations::k_FirstGesture + animations::k_GestureCount;
	case Kind::PullFace:
		return animation >= animations::k_FirstFace && animation < animations::k_FirstFace + animations::k_FaceCount;
	default:
		return true;
	}
}

bool ValidOffset(glm::vec2 offset)
{
	return std::abs(offset.x) <= k_MaxOffset && std::abs(offset.y) <= k_MaxOffset;
}

/// A script run names a script, and every object, creature and marker it hands over is the scenario's and on the map.
/// An object that a native call gave back must come from a native call before this command.
bool ScriptArgumentsFit(const Command& command, const Scenario& scenario)
{
	const auto before =
	    std::ranges::find_if(scenario.commands, [&command](const Command& other) { return &other == &command; });
	const auto earlierCalls = static_cast<size_t>(
	    std::count_if(scenario.commands.begin(), before, [](const Command& other) { return other.kind == Kind::CallNative; }));
	const auto fits = [&scenario, &command, earlierCalls](const ScriptArgument& argument) {
		switch (argument.kind)
		{
		case ScriptArgument::Kind::Number:
		case ScriptArgument::Kind::Integer:
		case ScriptArgument::Kind::Result:
		case ScriptArgument::Kind::LastResult:
		case ScriptArgument::Kind::Town:
			return true;
		case ScriptArgument::Kind::Marker:
		case ScriptArgument::Kind::Position:
			return ValidOffset(argument.point);
		case ScriptArgument::Kind::Object:
			return argument.index < scenario.objects.size();
		case ScriptArgument::Kind::Creature:
			return argument.index < scenario.creatures.size();
		case ScriptArgument::Kind::Temple:
			return argument.index < scenario.temples.size();
		case ScriptArgument::Kind::Flock:
			// The flock needs a villager or an animal of the scenario
			return std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) {
				return std::holds_alternative<VillagerInfo>(object.type) || std::holds_alternative<AnimalInfo>(object.type);
			});
		case ScriptArgument::Kind::Made:
			return argument.index < earlierCalls;
		case ScriptArgument::Kind::Text:
			return command.kind == Command::Kind::CallNative && !argument.text.empty();
		}
		return false;
	};
	return !command.script.empty() && std::ranges::all_of(command.scriptArguments, fits) &&
	       std::ranges::all_of(command.scriptGlobals,
	                           [&fits](const auto& global) { return !global.first.empty() && fits(global.second); });
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be of a kind a creature may pick
/// up (a thing, pot, animal, villager, static, tree or magic mushroom), what it knocks down a thing or a tree, the
/// part it strokes, the way it looks something over and the leash put on real ones
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type) || std::holds_alternative<PotInfo>(object->type) ||
		                std::holds_alternative<MobileStaticInfo>(object->type) ||
		                std::holds_alternative<TreeInfo>(object->type) ||
		                (std::holds_alternative<FeatureInfo>(object->type) &&
		                 std::get<FeatureInfo>(object->type) == FeatureInfo::MagicMushroom))
		           ? ""
		           : "picks up no thing";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::TieLeash:
		return object != nullptr ? "" : "ties the leash to nothing";
	case Kind::JoinFootball:
		return object != nullptr && std::holds_alternative<VillagerInfo>(object->type) ? ""
		                                                                               : "sends no villager to the football";
	case Kind::Examine:
		return command.value < creature_object_actions::k_KeepAnimationCount ? "" : "no such way of looking it over";
	case Kind::HandStroke:
		return command.bodyPart < creature_feedback::k_BodyPartCount ? "" : "no such part of the body";
	case Kind::HandSlap:
		return command.slapHeight > 0.0f && command.slapHeight < creature_feedback::k_SlapAbove ? "" : "slaps above it";
	case Kind::PutOnLeash:
		return command.leash == LeashType::Rope || command.leash == LeashType::Evil || command.leash == LeashType::Good
		           ? ""
		           : "no such leash";
	case Kind::ConfineToHome:
		return command.radius > 0.0f ? "" : "keeps it nowhere";
	case Kind::LeashKey:
		return command.value < 3 ? "" : "no such leash key";
	case Kind::HandTapLeash:
		return command.player < PlayerNames::_COUNT ? "" : "no such player";
	case Kind::FightBlow:
		return command.value < 3 && command.chargeMs >= 0.0f && command.chargeMs <= creature_fight::k_MaxChargeMs
		           ? ""
		           : "no such blow or charge";
	case Kind::FightStep:
		return command.value < 4 ? "" : "no such step";
	default:
		return "";
	}
}
} // namespace

bool testbed_scenarios::NeedsNoCreature(Command::Kind kind)
{
	return kind == Kind::SetHour || kind == Kind::RestoreDeviceInDomeFollow || kind == Kind::RestoreDevice ||
	       kind == Kind::PutLoadedArena || kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed ||
	       kind == Kind::PressKey || kind == Kind::HandTakeFireBall || kind == Kind::SetAlignment || kind == Kind::WideScreen ||
	       kind == Kind::SetInterfaceLevel || kind == Kind::FlyToBookmark || kind == Kind::TabToTown ||
	       kind == Kind::SetOpenClose || kind == Kind::LayGateStone || kind == Kind::HandTapObject || kind == Kind::Advisor ||
	       kind == Kind::RunText || kind == Kind::PlayHandDemo || kind == Kind::Reframe || kind == Kind::DestroyTemple ||
	       kind == Kind::RunHelpMessage || kind == Kind::EndGuidanceWaits || kind == Kind::SetTownBeliefInRival ||
	       kind == Kind::GiveLandTown || kind == Kind::RecordChallenge || kind == Kind::EnterTemple || kind == Kind::HandTake ||
	       kind == Kind::HandApply || kind == Kind::HandTakeShownSeed || kind == Kind::RunScript || kind == Kind::ClickScroll ||
	       kind == Kind::ClickCreatureAt || kind == Kind::CameraLooksAt || kind == Kind::HandTakeNear ||
	       kind == Kind::ClickCreatureNear || kind == Kind::TapRewardNear || kind == Kind::PlayBoatFilm ||
	       kind == Kind::HandPour || kind == Kind::HandRelease || kind == Kind::SetReportLatency ||
	       kind == Kind::RemoveObject || kind == Kind::ReadTip || kind == Kind::LookAtTempleScroll ||
	       kind == Kind::HandGripTotem || kind == Kind::ProfileFolderCheck || kind == Kind::PlayCustomMusic ||
	       kind == Kind::ShowCustomMusicControls || kind == Kind::OwnTattooPicture || kind == Kind::OpenTattooMenu ||
	       kind == Kind::OpenControlsBox || kind == Kind::OpenMenuPage || kind == Kind::PressMenuKey ||
	       kind == Kind::StopScript || kind == Kind::StartScriptMusic || kind == Kind::SaveGame || kind == Kind::LoadGame ||
	       kind == Kind::QuickSave || kind == Kind::ContinueGame || kind == Kind::PlayLogoEffect ||
	       kind == Kind::PlayFallingSpellFilm || kind == Kind::PlayIntroFilm || kind == Kind::SayLine ||
	       kind == Kind::PlayGuidanceVoice || kind == Kind::ScriptRelease || kind == Kind::FillSiteIcons ||
	       kind == Kind::ChargeIconWithSeedHeld || kind == Kind::TakeIconSeed || kind == Kind::TapSiteIcon ||
	       kind == Kind::TapTempleEntrance || kind == Kind::FinishBuilding || kind == Kind::FillBuiltAmount ||
	       kind == Kind::ScaffoldStartsSite || kind == Kind::DestroyScaffoldSite || kind == Kind::ScaffoldClearsGround ||
	       kind == Kind::CallNative || kind == Kind::PauseGame || kind == Kind::SetGameSpeed ||
	       kind == Kind::RemoveTeleportStone || kind == Kind::MakeDance || kind == Kind::OpenMultiplayerBox ||
	       kind == Kind::BuildWorshipSites || kind == Kind::ScriptBuildsWorshipSites || kind == Kind::PressPlainKey ||
	       kind == Kind::PlainKeyDown || kind == Kind::PlainKeyRepeat || kind == Kind::PlainKeyUp ||
	       kind == Kind::OtherGodHand || kind == Kind::ObserverCamera || kind == Kind::OwnGod || kind == Kind::JoinOnline ||
	       kind == Kind::EndBox || kind == Kind::BringCreature || kind == Kind::TattooEditorAsNetworkGame ||
	       kind == Kind::ShowDemoPing || kind == Kind::AddIncomingMessage || kind == Kind::Chat ||
	       kind == Kind::ComputerPersonality || kind == Kind::ComputerUntaughtSends || kind == Kind::SkirmishBox ||
	       kind == Kind::LookAtSaveGamePicture || kind == Kind::HandCastOnDispenser || kind == Kind::HandCastOnCreature ||
	       kind == Kind::CloseHeldMiracle || kind == Kind::JoinFootball || IsPointerCommand(kind);
}

std::string_view testbed_scenarios::Name(Facet facet)
{
	constexpr std::array<std::string_view, k_FacetCount> k_Names {
	    "Idle",       "Expressions", "Senses",    "Needs",         "Growth",      "Appearance", "Light",   "Movement",
	    "Footprints", "Audio",       "Objects",   "Hand",          "Leash",       "Combat",     "Mind",    "Particles",
	    "Editor",     "Miracles",    "Benchmark", "Creature Mode", "Physics",     "Nature",     "Animals", "Sky",
	    "Advisors",   "Story",       "Tutorial",  "Interface",     "Multiplayer",
	};
	return k_Names.at(static_cast<size_t>(facet));
}

std::string_view testbed_scenarios::Name(Weather weather)
{
	constexpr std::array<std::string_view, 5> k_Names {"clear", "rain", "thunderstorm", "snow", "blizzard"};
	return k_Names.at(static_cast<size_t>(weather));
}

std::string_view testbed_scenarios::Name(Shot shot)
{
	constexpr std::array<std::string_view, 5> k_Names {"testbed", "overview", "follow", "head", "placed"};
	return k_Names.at(static_cast<size_t>(shot));
}

std::string_view testbed_scenarios::Name(Command::Kind kind)
{
	constexpr std::array<std::string_view, 231> k_Names {
	    "walk to",
	    "run to",
	    "follow",
	    "flee from",
	    "turn to face",
	    "face the camera",
	    "stop",
	    "play action",
	    "gesture",
	    "pull face",
	    "draw a gesture",
	    "show feeling",
	    "sit down",
	    "stand up",
	    "sleep",
	    "wake",
	    "eat",
	    "drink",
	    "poo",
	    "fart",
	    "puke",
	    "faint",
	    "stroke",
	    "slap",
	    "set hour",
	    "pick up",
	    "put down",
	    "toss away",
	    "eat it",
	    "look it over",
	    "throw at",
	    "knock down",
	    "point at",
	    "hand strokes",
	    "hand slaps",
	    "hand lets go",
	    "hand takes hold",
	    "put on leash",
	    "tie leash to",
	    "tie leash to desire flag",
	    "tie leash to worship site flag",
	    "untie leash",
	    "take off leash",
	    "keep at home",
	    "make leashable",
	    "hand taps to leash",
	    "leash key",
	    "shake the hand",
	    "order to a point",
	    "order on the object",
	    "hang the temple's leashes",
	    "tap a temple leash",
	    "tap a building with the leash",
	    "start fight",
	    "fight blow",
	    "fight block",
	    "fight step",
	    "fight special",
	    "fight by itself",
	    "fight miracle",
	    "show the fight",
	    "pointer on opponent",
	    "pointer on the cave creature",
	    "fight autopilot",
	    "fight lesson check",
	    "knock out",
	    "bring round",
	    "abort fight",
	    "tie leash to creature",
	    "set desire",
	    "set stage",
	    "reward if",
	    "see skill",
	    "see miracle",
	    "player did",
	    "player did it to the object",
	    "press C",
	    "double click",
	    "camera keys",
	    "clear the view",
	    "give the camera back",
	    "saved follow camera",
	    "saved turning camera",
	    "saved free camera",
	    "saved camera below the land",
	    "press F5",
	    "tattoo",
	    "take tattoo off",
	    "open tattoo editor",
	    "hold seed",
	    "draw gesture",
	    "summon seed",
	    "know miracle",
	    "cast miracle",
	    "press key",
	    "hand takes fireball",
	    "pointer to",
	    "press button",
	    "let go of button",
	    "move mouse",
	    "turn wheel",
	    "set alignment",
	    "open or close",
	    "lay gate stone",
	    "cinema bars",
	    "set interface level",
	    "set fight lean",
	    "set miracle sightings",
	    "hand tap",
	    "tell an advisor",
	    "walk home",
	    "run a help message",
	    "end the guidance's waits",
	    "set town belief in rival",
	    "destroy temple",
	    "script play",
	    "run text",
	    "play hand demo",
	    "reframe",
	    "hand take",
	    "hand give",
	    "hand take shown seed",
	    "hand apply",
	    "swap minds",
	    "record challenge",
	    "enter temple",
	    "play boat film",
	    "run script",
	    "hand release",
	    "hand pour",
	    "do action",
	    "make friends",
	    "only desire",
	    "remove object",
	    "read tip",
	    "look at temple scroll",
	    "look at a save game picture",
	    "set alight",
	    "hand grips totem",
	    "pointer on altar",
	    "pointer on rope",
	    "pointer on object",
	    "set fizz",
	    "check the profile folder",
	    "give the player a picture of their own",
	    "open the tattoo editor from the menu",
	    "open the box of the controls page",
	    "open a page of the menu",
	    "press a key in the menu",
	    "play custom music",
	    "show the custom music controls",
	    "save the game",
	    "load the game",
	    "script reaction",
	    "script place",
	    "stop script",
	    "pointer on creature",
	    "start script music",
	    "write the web page",
	    "fly to a bookmark",
	    "press the Tab key",
	    "keep the creature in a bookmark and follow it",
	    "play the falling spell film",
	    "play the intro film",
	    "give hand glows",
	    "give a creed power",
	    "attach a script sound",
	    "attach a script sound to the object",
	    "say a line",
	    "play a guidance voice",
	    "stop at once",
	    "turn to face when still",
	    "script release",
	    "fill the worship site icons",
	    "quick save",
	    "continue the game",
	    "play the logo effect",
	    "charge an icon with its seed in the hand",
	    "finish the building",
	    "fill the built amount",
	    "start a site with the scaffold",
	    "destroy the site of the scaffold",
	    "clear the ground of the scaffold",
	    "call a script native",
	    "animal flees",
	    "hold the route search",
	    "pause the game",
	    "set the game's speed",
	    "force an action",
	    "remove the teleport stone",
	    "make a dance",
	    "open a multiplayer box",
	    "build the worship sites",
	    "press a key on the keyboard",
	    "show another god's hand",
	    "show an observer's camera",
	    "be the god of a player at this computer",
	    "press Join Online Game of the page",
	    "end box",
	    "bring a creature into a network game",
	    "give the hand's reports a latency",
	    "tattoo editor as in a network game",
	    "show a made-up ping",
	    "add an incoming message",
	    "chat",
	    "computer personality",
	    "computer sends with no lesson",
	    "skirmish box",
	    "put a key down",
	    "repeat a held key",
	    "let go of a key",
	    "a god orders its creature",
	    "take a seed from its icon",
	    "tap a worship site icon",
	    "change the body's look",
	    "tap the temple's entrance",
	    "give a town of the land to the player",
	    "cast the held miracle on a dispenser",
	    "cast the held miracle on a creature",
	    "close the held miracle and let go",
	    "click a scroll",
	    "put the camera to look at a point",
	    "join the football of the town",
	    "a script builds the worship sites",
	    "take the mobile object nearest a point into the hand",
	    "click the creature nearest a point",
	    "tap the reward chest nearest a point",
	    "click a creature",
	    "make the graphics device again while the dome follows the sky",
	    "make the graphics device again",
	    "put an arena of a loaded file between two creatures",
	};
	return k_Names.at(static_cast<size_t>(kind));
}

bool testbed_scenarios::IsPointerCommand(Command::Kind kind)
{
	return kind == Kind::PointerTo || kind == Kind::PointerPress || kind == Kind::PointerRelease ||
	       kind == Kind::PointerSweep || kind == Kind::WheelTurn || kind == Kind::PointerOnOpponent ||
	       kind == Kind::PointerOnCaveCreature || kind == Kind::PointerOnCreature || kind == Kind::PointerOnAltar ||
	       kind == Kind::PointerOnRope || kind == Kind::PointerOnObject;
}

bool NeedOverrides::Empty() const
{
	return !energy && !exhaustion && !dehydration && !poo && !life && !warmth && !age;
}

std::span<const Scenario> testbed_scenarios::All()
{
	static const std::vector<Scenario> k_All = Build();
	return k_All;
}

const Scenario* testbed_scenarios::Find(std::string_view id)
{
	const auto all = All();
	const auto found = std::ranges::find(all, id, &Scenario::id);
	return found != all.end() ? &*found : nullptr;
}

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	std::vector<std::string> problems;
	if (scenario.id.empty() || scenario.name.empty() || scenario.description.empty() || scenario.expected.empty())
	{
		problems.emplace_back("missing its id, name, description or what to expect");
	}
	if (static_cast<size_t>(scenario.facet) >= k_FacetCount)
	{
		problems.emplace_back("no such facet");
	}
	const auto& environment = scenario.environment;
	if (!InRange(environment.hour, 0.0f, k_HoursPerDay) || environment.bodyTimeScale < 0.0f)
	{
		problems.emplace_back("hour or body time out of range");
	}
	// A land other than the testbed brings its own things
	if (environment.land == Land::Testbed && scenario.creatures.empty() && scenario.particles.empty() &&
	    scenario.miracles.empty() && scenario.dispensers.empty() && scenario.birdFlocks.empty() && scenario.temples.empty() &&
	    !environment.dispenserGrid && !scenario.crowd.has_value() && !environment.playerAlignment.has_value() &&
	    scenario.throws.empty() && scenario.objects.empty() && scenario.vortices.empty() && scenario.rewards.empty() &&
	    scenario.fireflyRewards.empty() && scenario.landStorms.empty() && scenario.highlights.empty() &&
	    scenario.landScript.empty() &&
	    std::ranges::none_of(scenario.commands, [](const Command& command) { return NeedsNoCreature(command.kind); }))
	{
		problems.emplace_back("no creatures, things, particles, miracles, dispensers, birds, temples, vortices, reward "
		                      "chests, crowd, fireflies' rewards, land storms, scrolls or signs, land script lines, player's "
		                      "commands or alignment for the "
		                      "hand");
	}
	for (const auto& storm : scenario.landStorms)
	{
		if (!ValidOffset(storm.offset) || !ValidOffset(storm.destination) || storm.age < 0.0f || storm.cloudCount < 0)
		{
			problems.emplace_back("a land storm off the map, or with a negative age or cloud count");
		}
	}
	if ((environment.playerAlignment && !InRange(*environment.playerAlignment, -1.0f, 1.0f)) ||
	    (environment.cursor && (!InRange(environment.cursor->x, 0.0f, 1.0f) || !InRange(environment.cursor->y, 0.0f, 1.0f))))
	{
		problems.emplace_back("alignment or cursor out of range");
	}
	if (scenario.crowd.has_value() && (scenario.crowd->count == 0 || scenario.crowd->perFrame == 0))
	{
		problems.emplace_back("a crowd of no one, or spawned none a frame");
	}
	for (const auto& miracle : scenario.miracles)
	{
		const auto type = static_cast<size_t>(miracle.type);
		if (type == 0 || type >= k_Miracles)
		{
			problems.emplace_back("a miracle of no such magic type");
		}
		if (miracle.target == MiracleCast::Target::Creature && miracle.creature >= scenario.creatures.size())
		{
			problems.emplace_back("a miracle cast on a creature that isn't there");
		}
		if (!ValidOffset(miracle.point) || !ValidOffset(miracle.handOffset) || miracle.delaySeconds < 0.0f ||
		    miracle.holdSeconds.value_or(0.0f) < 0.0f || miracle.repeatSeconds.value_or(1.0f) <= 0.0f)
		{
			problems.emplace_back("a miracle's points or times out of range");
		}
	}
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto type = static_cast<size_t>(dispenser.type);
		if (type == 0 || type >= k_Miracles || !ValidOffset(dispenser.offset))
		{
			problems.emplace_back("a dispenser of no such magic type, or off the map");
		}
	}
	for (const auto& particle : scenario.particles)
	{
		if (particle.file.empty() && particles::ParticleTypeFile(particle.type).empty())
		{
			problems.emplace_back("a particle type that has no file");
		}
		if (particle.targetsCreatures && scenario.creatures.empty())
		{
			problems.emplace_back("a particle effect given creatures to act on, without creatures");
		}
		if (particle.magnitude < 0.0f || particle.restartSeconds < 0.0f)
		{
			problems.emplace_back("a particle effect's magnitude or restart out of range");
		}
	}
	const auto creatures = scenario.creatures.size();
	// The commands can also name the creatures of the filled slots: one for each temple at most
	const auto others = creatures + (scenario.fillEmptySlots ? scenario.temples.size() : 0);
	const auto& framing = scenario.framing;
	if ((framing.shot == Shot::Follow || framing.shot == Shot::Head) && framing.creature >= creatures)
	{
		problems.emplace_back("the camera frames a creature that isn't there");
	}
	if (framing.distance <= 0.0f || !std::ranges::all_of(framing.include, ValidOffset))
	{
		problems.emplace_back("the framing's distance or points are out of range");
	}
	for (size_t i = 0; i < creatures; ++i)
	{
		const auto& creature = scenario.creatures.at(i);
		const auto who = fmt::format("creature {}", i);
		const auto species = static_cast<int32_t>(creature.species);
		if (species < 1 || species >= static_cast<int32_t>(CreatureType::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such species", who));
		}
		if (static_cast<size_t>(creature.owner) >= static_cast<size_t>(PlayerNames::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such owner", who));
		}
		if (!ValidOffset(creature.offset))
		{
			problems.push_back(fmt::format("{}: off the map", who));
		}
		if ((creature.alignment && !InRange(*creature.alignment, -1.0f, 1.0f)) ||
		    (creature.fatness && !InRange(*creature.fatness, 0.0f, 1.0f)) ||
		    (creature.strength && !InRange(*creature.strength, 0.0f, 1.0f)) ||
		    (creature.size && !InRange(*creature.size, 0.05f, 4.0f)))
		{
			problems.push_back(fmt::format("{}: body out of range", who));
		}
		if (!creature.mindFile.empty() && !creature.mindFile.starts_with("chosen:") && !creature.mindFile.starts_with("game:"))
		{
			problems.push_back(fmt::format("{}: a mind file is chosen: or game:", who));
		}
		if (creature.phase && *creature.phase > k_LastPhase)
		{
			problems.push_back(fmt::format("{}: no such stage of growing up", who));
		}
		CheckNeeds(creature.needs, problems, who);
		for (const auto& desire : creature.desires)
		{
			if (static_cast<size_t>(desire.desire) >= creature_desires::k_DesireCount || !InRange(desire.fraction, 0.0f, 1.0f))
			{
				problems.push_back(fmt::format("{}: desire out of range", who));
			}
		}
		for (const auto& tattoo : creature.tattoos)
		{
			if (tattoo.design >= creature_tattoo::k_DesignCount || tattoo.Empty())
			{
				problems.push_back(fmt::format("{}: no such tattoo design or site", who));
			}
		}
		if (creature.tattoos.size() > creature_tattoo::k_SlotCount)
		{
			problems.push_back(fmt::format("{}: more tattoos than slots", who));
		}
		for (const auto& mark : creature.wounds)
		{
			if (mark.type >= creature_marks::k_WoundLifetimes.size() || mark.column >= 8 || mark.skin >= 4)
			{
				problems.push_back(fmt::format("{}: no such wound", who));
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const bool valid = std::visit(
		    []<typename T>(T type) {
			    const auto index = static_cast<int32_t>(type);
			    return index >= 0 && index < static_cast<int32_t>(T::_COUNT);
		    },
		    object.type);
		if (!valid || object.scale <= 0.0f || !ValidOffset(object.offset))
		{
			problems.emplace_back("an object of no such kind, size or place");
		}
		// The model of this kind of food pile is the grain at the side of a worship site. It lies about 50 m from the
		// point where the pile is, so you do not see the pile there. Use a different kind of food pile.
		if (const auto* pot = std::get_if<PotInfo>(&object.type); pot != nullptr && *pot == PotInfo::FoodPile)
		{
			problems.emplace_back("a food pile of the kind whose model is far from its point");
		}
		// On a land of the story, the scenario's town is a town of the testbed, not a town of the land. Name the land's
		// town with landTown. Only a land of the story has towns of its own.
		const bool storyLand = scenario.environment.land == Land::Story;
		if (object.joinTown && storyLand)
		{
			problems.emplace_back("an object that joins the scenario's town on a land of the story; use landTown");
		}
		if (object.landTown.has_value() && !storyLand)
		{
			problems.emplace_back("an object with a town of the land on a land that is not of the story");
		}
	}
	for (const auto& footpath : scenario.footpaths)
	{
		const auto outOfRange = [&scenario](size_t object) { return object >= scenario.objects.size(); };
		if ((footpath.startsAt.has_value() && outOfRange(*footpath.startsAt)) ||
		    std::ranges::any_of(footpath.leadsFrom, outOfRange) ||
		    !std::ranges::all_of(footpath.nodes, [](glm::vec2 node) { return ValidOffset(node); }) || footpath.nodes.empty() ||
		    footpath.leadsFrom.empty())
		{
			problems.emplace_back("a footpath with no nodes, a node off the map or an object that isn't there");
		}
	}
	for (const auto& throwing : scenario.throws)
	{
		const auto things = throwing.reward ? scenario.rewards.size() : scenario.objects.size();
		if (throwing.object >= things || (throwing.from.has_value() && !ValidOffset(*throwing.from)) || throwing.height < 0.0f)
		{
			problems.emplace_back("a throw of no such object, or from no such place");
		}
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		// An object set alight (value 1 or 2) needs no creature, but the object must be there
		const bool objectAlight = command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2);
		if (objectAlight && command.object >= scenario.objects.size())
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if (!NeedsNoCreature(command.kind) && !objectAlight && command.creature >= creatures)
		{
			problems.push_back(fmt::format("{}: no such creature", what));
		}
		if (command.kind == Kind::SetAlignment && !InRange(command.alignment, -1.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: alignment out of range", what));
		}
		if ((command.kind == Kind::SetOpenClose && command.object >= scenario.objects.size()) ||
		    (command.kind == Kind::LayGateStone &&
		     (command.object >= scenario.objects.size() || command.value >= scenario.objects.size())))
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if ((command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature ||
		     command.kind == Kind::MakeFriends) &&
		    (command.value >= others || command.value == command.creature))
		{
			problems.push_back(fmt::format("{}: acts on no other creature", what));
		}
		if (!ValidAnimation(command.kind, command.value))
		{
			problems.push_back(fmt::format("{}: animation {} isn't of its kind", what, command.value));
		}
		if (const auto problem = CommandProblem(command, scenario.objects); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
		if ((command.kind == Kind::RunScript || command.kind == Kind::CallNative) && !ScriptArgumentsFit(command, scenario))
		{
			problems.push_back(fmt::format("{}: runs no script, or hands it what the scenario hasn't", what));
		}
		const bool offMapOrder = command.kind == Kind::LeashOrderAt && std::abs(command.point.x) <= k_MaxOrderOffset &&
		                         std::abs(command.point.y) <= k_MaxOrderOffset;
		if ((!ValidOffset(command.point) && !offMapOrder) || command.delaySeconds < 0.0f ||
		    !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: point, delay or hour out of range", what));
		}
		if ((command.kind == Kind::SetDesire &&
		     (command.value >= creature_desires::k_DesireCount || !InRange(command.amount, 0.0f, 1.0f))) ||
		    (command.kind == Kind::SetPhase && command.value > k_LastPhase) ||
		    (command.kind == Kind::SetFightLean && !InRange(command.amount, -1.0f, 1.0f)) ||
		    (command.kind == Kind::SetMiracleSightings && (command.value >= k_Miracles || command.amount < 0.0f)) ||
		    (command.kind == Kind::ShowFeeling && command.value >= creature_face::k_CueCount) ||
		    (command.kind == Kind::SeeSkill && command.value >= k_Skills) ||
		    ((command.kind == Kind::SeeMiracle || command.kind == Kind::KnowMiracle || command.kind == Kind::CastMiracle ||
		      command.kind == Kind::FightSpell) &&
		     command.value >= k_Miracles) ||
		    (command.kind == Kind::CastMiracle &&
		     (command.atCreature.has_value() ? *command.atCreature >= scenario.creatures.size()
		                                     : command.object >= scenario.objects.size())) ||
		    ((command.kind == Kind::PlayerDid || command.kind == Kind::PlayerDidOn) && command.value >= k_Deeds) ||
		    (command.kind == Kind::CameraKeys && (command.value >= 4 || command.amount <= 0.0f)) ||
		    (command.kind == Kind::PointerTo &&
		     (!InRange(command.point.x, 0.0f, 1.0f) || !InRange(command.point.y, 0.0f, 1.0f))) ||
		    ((command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease) &&
		     (command.value < 1 || command.value > 3)) ||
		    (command.kind == Kind::PointerSweep && command.amount <= 0.0f) ||
		    (command.kind == Kind::OpenCreatureCave && command.value >= creature_cave::k_PageCount) ||
		    ((command.kind == Kind::ApplyTattoo || command.kind == Kind::RemoveTattoo) &&
		     (command.value >= creature_tattoo::k_DesignCount || command.bodyPart >= creature_tattoo::k_SlotCount)) ||
		    ((command.kind == Kind::HoldSeed || command.kind == Kind::SummonSeed) && command.value >= k_SeedCount) ||
		    (command.kind == Kind::DrawGesture && (command.value == 0 || command.value > k_LastGesture)))
		{
			problems.push_back(fmt::format("{}: value {} out of range", what, command.value));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}

glm::vec2 testbed_scenarios::MapPoint(glm::vec2 middle, glm::vec2 offset)
{
	return middle + offset;
}

std::optional<uint32_t> testbed_scenarios::HelpTextNumber(std::span<const std::string> names, std::string_view name)
{
	const auto found = std::ranges::find(names, name);
	if (found == names.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(names.begin(), found));
}

float testbed_scenarios::CreatureHeight(float size)
{
	// A creature of size 1 stands about 15 units tall, whatever its species' mesh
	constexpr float k_HeightPerSize = 15.0f;
	return k_HeightPerSize * std::max(size, 0.05f);
}

CameraPlacement testbed_scenarios::Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor)
{
	// Looking down from the south at this angle, far enough back for the box's width to fit across the view and its
	// depth, foreshortened, up and down it, with a margin round it for the creatures' heights
	constexpr float k_Pitch = 0.65f;
	constexpr float k_MinDistance = 40.0f;
	constexpr float k_Margin = 25.0f;
	const auto halfAngles = glm::clamp(fieldOfView * 0.5f, 0.1f, 1.5f);
	// The near side of the box is closer to the camera than its middle, so its width takes a wider angle
	const auto across =
	    ((halfSize.x + k_Margin) / portable_math::Tan(halfAngles.x)) + ((halfSize.y + k_Margin) * portable_math::Cos(k_Pitch));
	const auto upAndDown = (halfSize.y + k_Margin) * portable_math::Sin(k_Pitch) / portable_math::Tan(halfAngles.y);
	const auto distance = distanceFactor * std::max({across, upAndDown, k_MinDistance});
	return {
	    .origin = centre + glm::vec3(0.0f, distance * portable_math::Sin(k_Pitch), -distance * portable_math::Cos(k_Pitch)),
	    .focus = centre,
	};
}

CameraPlacement testbed_scenarios::Follow(glm::vec3 position, float height, float distanceFactor)
{
	constexpr float k_MinDistance = 25.0f;
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	const auto distance = distanceFactor * std::max(height * 3.5f, k_MinDistance);
	return {.origin = focus + glm::vec3(0.0f, distance * 0.45f, -distance * 0.9f), .focus = focus};
}

CameraPlacement testbed_scenarios::Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor)
{
	const auto length = glm::length(ahead);
	const auto forward = length > 0.0f ? ahead / length : glm::vec2(0.0f, -1.0f);
	const glm::vec3 forward3 {forward.x, 0.0f, forward.y};
	// The head is high on the body and a little forward of its middle
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.75f, 0.0f) + (forward3 * (height * 0.3f));
	const auto distance = distanceFactor * height;
	return {.origin = focus + (forward3 * distance) + glm::vec3(0.0f, height * 0.15f, 0.0f), .focus = focus};
}

Bounds testbed_scenarios::BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize)
{
	if (points.empty())
	{
		return {.centre = glm::vec2(0.0f), .halfSize = minimumHalfSize};
	}
	auto low = points.front();
	auto high = points.front();
	for (const auto& point : points)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	return {.centre = (low + high) * 0.5f, .halfSize = glm::max((high - low) * 0.5f, minimumHalfSize)};
}

void testbed_scenarios::Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size)
{
	const auto set = [](float& value, const std::optional<float>& wanted, float low, float high) {
		if (wanted.has_value())
		{
			value = std::clamp(*wanted, low, high);
		}
	};
	set(needs.energy, overrides.energy, 0.0f, std::max(1.0f, size));
	set(needs.exhaustion, overrides.exhaustion, 0.0f, 1.0f);
	set(needs.dehydration, overrides.dehydration, 0.0f, 1.0f);
	set(needs.poo, overrides.poo, 0.0f, 1.0f);
	set(needs.life, overrides.life, 0.0f, 1.0f);
	set(needs.warmth, overrides.warmth, -1.0f, 1.0f);
	if (overrides.age.has_value())
	{
		needs.age = *overrides.age;
	}
}

void testbed_scenarios::Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires)
{
	for (const auto& override : overrides)
	{
		auto& state = desires[override.desire];
		state.activated = true;
		state.value = std::clamp(override.fraction, 0.0f, 1.0f) * std::max(state.max, 0.0f);
	}
}

std::vector<size_t> testbed_scenarios::GivesAt(const std::map<uint32_t, std::vector<size_t>>& gives, uint32_t turn,
                                               size_t commands)
{
	std::vector<size_t> given;
	if (const auto found = gives.find(turn); found != gives.end())
	{
		std::ranges::copy_if(found->second, std::back_inserter(given), [commands](size_t place) { return place < commands; });
	}
	return given;
}

std::vector<size_t> testbed_scenarios::Advance(Timeline& timeline, std::span<const Command> commands,
                                               std::optional<size_t> repeatFrom, float seconds,
                                               const std::function<bool(const Command& command)>& isFree)
{
	std::vector<size_t> due;
	if (commands.empty() || timeline.next >= commands.size())
	{
		timeline.done = true;
	}
	if (timeline.done)
	{
		return due;
	}
	timeline.seconds += seconds;
	timeline.sinceGiven += seconds;
	// At most a round of the commands at once, so commands that go round again without waiting can't run away
	while (!timeline.done && due.size() < commands.size())
	{
		const auto& command = commands[timeline.next];
		if (command.waitUntilFree && !timeline.freed)
		{
			if (timeline.sinceGiven < k_SettleSeconds || !isFree(command))
			{
				// Its delay counts from when the creature is free
				timeline.seconds = 0.0f;
				break;
			}
			timeline.freed = true;
		}
		if (timeline.seconds < command.delaySeconds)
		{
			break;
		}
		due.push_back(timeline.next);
		timeline.seconds = 0.0f;
		timeline.sinceGiven = 0.0f;
		timeline.freed = false;
		if (++timeline.next >= commands.size())
		{
			if (repeatFrom.has_value() && *repeatFrom < commands.size())
			{
				timeline.next = *repeatFrom;
			}
			else
			{
				timeline.done = true;
			}
		}
	}
	return due;
}

bool testbed_scenarios::KeepsDispenserGrid(const Scenario& scenario)
{
	if (!scenario.environment.dispenserGrid)
	{
		return false;
	}
	const auto onGrid = [](const auto& setup) { return testbed_dispensers::InGridArea(setup.offset); };
	return std::ranges::none_of(scenario.creatures, onGrid) && std::ranges::none_of(scenario.objects, onGrid);
}
