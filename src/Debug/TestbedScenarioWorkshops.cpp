/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the workshops and the scaffolds.

#include "3D/FlatLand.h"
#include "Creature/CreatureIdleMind.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddWorkshopScenarios(std::vector<Scenario>& all)
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
	    .id = "objects.founded_town_gets_reach",
	    .name = "A town that a scaffold founded gets its reach when the hand can no longer take the scaffold",
	    .facet = Facet::Objects,
	    .description = "The town and the scaffold of objects.scaffold_founds_town. After one second the scaffold starts "
	                   "a village centre where it lies, and founds a new town. Nobody builds the village centre.",
	    .expected = "The new town has no reach while the hand can take its scaffold: the inspector shows the town's "
	                "TownInfluence radius at 0, and no border shows round the site. The workshop keeps the scaffold for "
	                "150 game turns, about 15 seconds. Then the town gets its base reach at its next turn, and its border "
	                "shows round the site at the next draw of the borders. A load does not keep the mark.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {40.0f}, k_FoundingScaffold + glm::vec2 {40.0f}}},
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
