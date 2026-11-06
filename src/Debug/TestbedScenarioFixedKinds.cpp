/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the things that the game marks as fixed to the land

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// A hut and an oak in the same cell of the land, the oak made after the hut, so the oak is first in the cell's list
constexpr glm::vec2 k_HutWithOak {13.0f, 13.0f};
constexpr glm::vec2 k_OakBeforeHut {18.5f, 18.5f};
/// A hut with nothing else in its cells
constexpr glm::vec2 k_HutAlone {-17.0f, 13.0f};
/// Where the forest miracle falls, between the two huts
constexpr glm::vec2 k_ForestPoint {0.0f, 15.0f};

/// The temple of the town's player, and the altar of its worship site, which goes up on the side of the temple towards
/// the town. The town's two buildings stand either side of the point five metres west of the altar, so that point is the
/// town's gathering place and the altar stands where the town wants its temporary wood pile.
constexpr glm::vec2 k_PileTemple {-10.0f, -60.0f};
constexpr glm::vec2 k_Altar {-9.641f, -31.804f};
constexpr glm::vec2 k_Gathering {k_Altar.x - 5.0f, k_Altar.y};
constexpr glm::vec2 k_BuildingGap {25.0f, 0.0f};

/// A burning field and a burning hut side by side, and a rock that burns between them
constexpr glm::vec2 k_BurningField {-15.0f, 40.0f};
constexpr glm::vec2 k_BurningHut {15.0f, 40.0f};
constexpr glm::vec2 k_BurningRock {0.0f, 40.0f};
constexpr glm::vec2 k_BurningDeadTree {0.0f, 58.0f};

/// Where the hand puts a villager down, four metres west of the altar of the town's worship site
constexpr glm::vec2 k_BuilderDrop {k_Altar.x - 4.0f, k_Altar.y};
/// A hut of the town that is not built, and a food pot between it and the place where the hand puts a villager down
constexpr glm::vec2 k_PotDrop {0.0f, 20.0f};
constexpr glm::vec2 k_PotNearDrop {-2.0f, 20.0f};
constexpr glm::vec2 k_UnbuiltHut {7.0f, 20.0f};
/// A statue of the land that is only part built, east of the place where the hand puts a villager down
constexpr glm::vec2 k_PartBuiltStatue {7.0f, 20.0f};
/// The town's storage pit and village centre west of the place where the hand puts a villager down
constexpr glm::vec2 k_DropPit {k_PotDrop.x - 30.0f, k_PotDrop.y - 15.0f};
constexpr glm::vec2 k_DropCentre {k_PotDrop.x - 30.0f, k_PotDrop.y + 15.0f};

/// Where the hand puts a villager down, south of a hurt temple, and the temple. The town stands north of the temple, so its
/// worship site goes up on the far side of the temple.
constexpr glm::vec2 k_HurtTemple {-10.0f, -60.0f};
constexpr glm::vec2 k_TempleDrop {12.0f, -60.0f};
constexpr glm::vec2 k_TempleTown {-10.0f, -110.0f};

/// Where a boulder flies from towards the altar of the town's worship site: 20 metres north of the altar
constexpr glm::vec2 k_BoulderFrom {k_Altar.x, k_Altar.y + 20.0f};
} // namespace

void openblack::testbed_scenarios::AddFixedKindScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	all.push_back({
	    .id = "nature.forest_miracle_tree_before_hut",
	    .name = "A forest miracle between a hut with an oak in its cell and a hut alone",
	    .facet = Facet::Objects,
	    .description = "Two Norse huts of no town stand 30 metres apart. An oak stands in the same cell of the land as "
	                   "the east hut, and the oak is made after the hut, so the oak is first in the list of things that "
	                   "stay put in that cell. One second in, a script casts a forest miracle between the huts.",
	    .expected = "The miracle plants trees round its point. In the cell of the east hut and the oak, new trees grow "
	                "beside the hut, because the game asks only the first thing of the cell, the oak, and a tree has no "
	                "fixed mark. In the cells where a hut is first, no new tree grows.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_HutWithOak, k_HutAlone, k_OakBeforeHut}, .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_HutWithOak, .yawDegrees = 180.0f},
	                {.type = AbodeInfo::NorseHut, .offset = k_HutAlone, .yawDegrees = 180.0f},
	                {.type = TreeInfo::Oak, .offset = k_OakBeforeHut}},
	    .miracles = {{.type = MagicType::Forest,
	                  .point = k_ForestPoint,
	                  .handOffset = k_ForestPoint,
	                  .delaySeconds = 1.0f,
	                  .script = ScriptSpell {.radius = 15.0f, .seconds = 600.0f, .curl = 0.0f}}},
	});

	all.push_back({
	    .id = "appearance.fire_lights_land_by_fixed_mark",
	    .name = "A burning field lights the land as a burning hut does",
	    .facet = Facet::Appearance,
	    .description = "At night, a ripe wheat field and a Norse hut of the player's town stand 30 metres apart, with a "
	                   "rock between them, and a dead oak north of the rock. The four are set alight as they are made.",
	    .expected = "The field, the hut, the rock and the dead oak each have the fixed mark of the land and are wider "
	                "than 2 metres across the ground, so the land round each glows and flickers with the light of the "
	                "fire. Before, the field and the dead oak did not light the land. When the fire has eaten the food of "
	                "the field so that its crop no longer shows, the field and its flames and light are no longer drawn.",
	    .environment = {.hour = 23.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_BurningField, k_BurningHut}, .distance = 0.4f},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_BurningHut, .yawDegrees = 180.0f, .alight = true},
	                {.type = FieldTypeInfo::Wheat, .offset = k_BurningField, .ripeCrop = true, .alight = true},
	                {.type = MobileStaticInfo::Rock, .offset = k_BurningRock, .alight = true},
	                {.type = TreeInfo::Oak, .offset = k_BurningDeadTree, .alight = true, .dead = true}},
	});

	std::vector<ObjectSetup> town {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseGraveyard, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f},
	    {.type = VillagerInfo::NorseForesterMale,
	     .offset = k_Gathering + glm::vec2(0.0f, 15.0f),
	     .joinTown = true,
	     .woodHeld = 300}};
	all.push_back({
	    .id = "town.temporary_pile_clear_of_worship_site",
	    .name = "A temporary wood pile kept clear of a worship site",
	    .facet = Facet::Objects,
	    .description =
	        "A Norse town of the player with a village centre and a graveyard but no storage pit and no home, and a forester "
	        "of the town that carries 300 wood. The town's worship site goes up by the temple as the scenario "
	        "starts. The two buildings stand so that the altar of the site is where the town wants its "
	        "temporary wood pile: five metres east of its gathering place.",
	    .expected = "The forester takes its wood to the town's store. With no storage pit, the town makes its temporary "
	                "food and wood piles near its gathering place. Each pile goes to the first place in its search that "
	                "is clear of the worship site, its altar and its icons (the inspector's Town temporaryStores). No "
	                "pile stands in the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_PileTemple, k_Gathering - k_BuildingGap, k_Gathering + k_BuildingGap},
	                .distance = 0.6f},
	    .objects = std::move(town),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 0.0f}},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	});

	std::vector<ObjectSetup> builderTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_BuilderDrop, .joinTown = true}};
	all.push_back({
	    .id = "villagers.builder_takes_up_worship_site",
	    .name = "A builder put down by an unbuilt worship site takes up its site",
	    .facet = Facet::Hand,
	    .description = "A Norse town of the player with a village centre, a storage pit and one villager, and the town's "
	                   "worship site, which goes up by the temple as the scenario starts and is not built. The villager "
	                   "stands four metres west of the altar, under the hand. After two seconds the hand takes the "
	                   "villager, and three seconds later it puts the villager down at the same place.",
	    .expected = "Over the unbuilt site the villager glows and shows the builder's symbol. It lands, turns to the site "
	                "and takes up the work there at once: the site has the fixed mark of the land, as every building "
	                "has. It goes to the storage pit for wood or goes to the site to build. The inspector shows the "
	                "villager as one of the workers of the town's site at the worship site.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_BuilderDrop.x, 22.0f, k_BuilderDrop.y + 30.0f},
	                .look = {k_BuilderDrop.x, 0.0f, k_BuilderDrop.y}},
	    .objects = std::move(builderTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 2},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = 1000.0f,
	});
	std::vector<ObjectSetup> scriptBuiltTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_BuilderDrop, .joinTown = true, .woodHeld = 250}};
	all.push_back({
	    .id = "worship.script_built_site_keeps_altar_unbuilt",
	    .name = "A script builds a worship site, and its altar stays unbuilt",
	    .facet = Facet::Objects,
	    .description = "A Norse town of the player with a village centre, a storage pit and one villager that carries 250 "
	                   "wood. The town's worship site goes up by the temple as the scenario starts. After one second a "
	                   "script sets the site as built.",
	    .expected = "The script builds the site only. The site stands built, but its altar does not show, because the "
	                "altar keeps its own built amount: the inspector shows WorshipAltar built 0 and no Mesh on the altar. "
	                "Only work on the site, for example the repair of a damaged site, or a builder that the hand puts down "
	                "by the altar (worship.builder_put_down_by_unbuilt_altar) builds the altar, and the altar shows when "
	                "all of it is built. Before, the altar showed when the script built the site.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_BuilderDrop.x, 22.0f, k_BuilderDrop.y + 30.0f},
	                .look = {k_BuilderDrop.x, 0.0f, k_BuilderDrop.y}},
	    .objects = std::move(scriptBuiltTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::ScriptBuildsWorshipSites, .delaySeconds = 1.0f, .amount = 1.0f}},
	    .townBeliefInPlayer = 1000.0f,
	});
	std::vector<ObjectSetup> altarBuilderTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f, .storedWood = 2000},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_BuilderDrop, .joinTown = true, .woodHeld = 250}};
	all.push_back({
	    .id = "worship.builder_put_down_by_unbuilt_altar",
	    .name = "A villager put down by the unbuilt altar of a worship site that a script built",
	    .facet = Facet::Hand,
	    .description =
	        "A Norse town of the player with a village centre, a storage pit with 2000 wood and one villager "
	        "that carries 250 wood. The town's worship site goes up by the temple as the scenario starts. After "
	        "one second a script sets the site as built, and the altar stays unbuilt. The villager stands four metres west "
	        "of the altar, under the hand. One second later the hand takes the villager, and three seconds "
	        "later it puts the villager down at the same place.",
	    .expected = "The altar and the site are both at the villager's feet and interest it the same. The altar is made after "
	                "its site, so the search meets the altar first. Over the unbuilt altar the villager glows and shows "
	                "the builder's symbol. It lands, turns to the altar, and its town opens a site at the altar: the "
	                "inspector shows BuildingSites on the altar with the villager as its worker. The villager puts its "
	                "wood on a pile beyond the altar, builds, and goes to the storage pit for more wood. WorshipAltar "
	                "built goes up with each stroke, and the altar shows when all of it is built. Before, the site at "
	                "the altar took no wood, and the villager stood at the altar and built nothing.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_BuilderDrop.x, 22.0f, k_BuilderDrop.y + 30.0f},
	                .look = {k_BuilderDrop.x, 0.0f, k_BuilderDrop.y}},
	    .objects = std::move(altarBuilderTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::ScriptBuildsWorshipSites, .delaySeconds = 1.0f, .amount = 1.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 2},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = 1000.0f,
	});

	std::vector<ObjectSetup> boulderTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f},
	    {.type = MobileStaticInfo::Boulder1Chalk, .offset = k_BoulderFrom},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_Gathering + glm::vec2(0.0f, 15.0f), .joinTown = true}};
	all.push_back({
	    .id = "worship.boulder_hits_built_altar",
	    .name = "A boulder thrown at a built altar hits it, and the temple takes the blow",
	    .facet = Facet::Objects,
	    .description = "A Norse town of the player with a village centre, a storage pit and one villager. The town's "
	                   "worship site goes up by the temple and is built at once with its altar. After three seconds a small "
	                   "boulder flies at the altar from 20 metres north of it.",
	    .expected = "The boulder hits the altar and stops or bounces off it: a built altar is in the way of what flies, as "
	                "a feature is. The altar does not take the blow itself. The temple's heart takes it, and passes it "
	                "on to a building of the town, as a blow on the temple does. Before, the boulder flew through the "
	                "altar.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Altar.x + 25.0f, 15.0f, k_Altar.y + 10.0f},
	                .look = {k_Altar.x, 2.0f, k_Altar.y + 5.0f}},
	    .objects = std::move(boulderTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 0.5f}},
	    .townBeliefInPlayer = 1000.0f,
	    .throws =
	        {{.object = 2, .from = k_BoulderFrom, .height = 4.0f, .velocity = {0.0f, 3.0f, -20.0f}, .delaySeconds = 3.0f}},
	});

	std::vector<ObjectSetup> burningSiteTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_Gathering - k_BuildingGap},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_Gathering + k_BuildingGap, .yawDegrees = 180.0f}};
	all.push_back({
	    .id = "appearance.worship_site_flames",
	    .name = "The flames of a burning worship site are three tenths of its height",
	    .facet = Facet::Appearance,
	    .description = "A Norse town of the player with a village centre and a storage pit. The town's worship site goes up "
	                   "by the temple and is built at once. After two seconds the site catches fire at the most heat of "
	                   "its fire.",
	    .expected = "The flames on the site are three tenths of its height, as the flames on a creature or on the temple. "
	                "Before, they were half of its height, as on a hut.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Altar.x + 30.0f, 20.0f, k_Altar.y + 15.0f},
	                .look = {k_Altar.x, 2.0f, k_Altar.y - 5.0f}},
	    .objects = std::move(burningSiteTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 0.5f},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 3}},
	    .townBeliefInPlayer = 1000.0f,
	});

	std::vector<ObjectSetup> potTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_PotDrop + glm::vec2(-30.0f, 15.0f)},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_PotDrop + glm::vec2(-30.0f, -15.0f), .yawDegrees = 180.0f},
	    {.type = AbodeInfo::NorseHut, .offset = k_UnbuiltHut, .yawDegrees = 270.0f, .built = 0.3f},
	    {.type = PotInfo::FoodPot, .offset = k_PotNearDrop},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_PotDrop, .joinTown = true, .woodHeld = 250}};
	all.push_back({
	    .id = "villagers.put_down_looks_at_pot_first",
	    .name = "A builder put down by a food pot looks at the pot before the hut",
	    .facet = Facet::Hand,
	    .description = "A Norse town of the player with a village centre, a storage pit, a hut that is only part built, and "
	                   "one villager that carries 250 wood. A food pot stands 2 metres west of the villager, and the hut "
	                   "stands 7 metres east of it. After two seconds the hand takes the villager, and three seconds "
	                   "later it puts the villager down at the same place.",
	    .expected = "Over the part built hut the villager glows and shows the builder's symbol: the pot is nearer, but it "
	                "offers no work, so the hut gives the work. After it lands, the villager turns to the pot first, "
	                "because the pot interests a villager most. A pot is not a building, so the villager then decides "
	                "again as a builder and goes to build the hut. Before, the villager looked only at buildings, "
	                "fields, trees and villagers, and turned to the hut at once.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_PotDrop.x, 7.0f, k_PotDrop.y - 11.0f},
	                .look = {k_PotDrop.x, 0.0f, k_PotDrop.y}},
	    .objects = std::move(potTown),
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 4},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = 1000.0f,
	});

	std::vector<ObjectSetup> statueTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_DropCentre},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_DropPit, .yawDegrees = 180.0f},
	    {.type = FeatureInfo::AztcStatue, .offset = k_PartBuiltStatue, .yawDegrees = 90.0f, .built = 0.5f}};
	all.push_back({
	    .id = "building.feature_site_wood_pile",
	    .name = "Wood poured on the site of a part built statue goes on a new pile by the statue",
	    .facet = Facet::Hand,
	    .description = "A Norse town of the player with a village centre and a storage pit, and a statue of the land that is "
	                   "half built, with a site of the town at it. After three seconds the hand pours 300 wood over the "
	                   "statue.",
	    .expected = "The site of the statue takes the wood. It makes its one pile of wood for the first wood, about 4 "
	                "metres beyond the statue, turned an eighth of a turn either way at random, and the pile holds the 300 "
	                "wood: the inspector shows the pile in the BuildingSites piles of the statue. Every site of a thing "
	                "with the fixed mark (a feature, flowers, an animated static, a field, a building) makes its pile so. "
	                "Before, the site of a statue made no pile, and the wood brought to it was lost.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_PartBuiltStatue.x - 3.0f, 6.0f, k_PartBuiltStatue.y - 10.0f},
	                .look = {k_PartBuiltStatue.x, 0.0f, k_PartBuiltStatue.y}},
	    .objects = std::move(statueTown),
	    .commands = {{.kind = Kind::HandPour, .delaySeconds = 3.0f, .point = k_PartBuiltStatue, .value = 300}},
	    .townBeliefInPlayer = 1000.0f,
	});

	std::vector<ObjectSetup> noWoodTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_DropCentre},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_DropPit, .yawDegrees = 180.0f, .storedWood = 0},
	    {.type = AbodeInfo::NorseHut, .offset = k_UnbuiltHut, .yawDegrees = 270.0f, .built = 0.3f},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_PotDrop, .joinTown = true}};
	all.push_back({
	    .id = "villagers.builder_waits_with_no_wood",
	    .name = "A builder with no wood at its site and none to fetch stays at the site",
	    .facet = Facet::Hand,
	    .description = "A Norse town of the player with a village centre, an empty storage pit and a hut that is only part "
	                   "built, and one villager of the town that carries no wood. No forest is near. After two seconds the "
	                   "hand takes the villager, and three seconds later it puts the villager down at the same place. "
	                   "Forty seconds later the hand pours 300 wood over the storage pit, and sixty seconds after that "
	                   "300 wood again.",
	    .expected = "The villager looks at the hut as a builder. It has nowhere to get wood, so it does not take up the site "
	                "and has nothing to do. After the first pour the store holds more wood than the villager can carry: "
	                "the villager takes as much as it can carry, puts it on the hut's site and builds until the site has "
	                "no wood. The store now holds less wood than the villager can carry, and a builder takes wood from "
	                "the store only when the store holds more. So the villager stays in the building state (Building in "
	                "the inspector's LivingAction) at its place by the hut and plays its building clip again and again, "
	                "with no wood used. Each time the clip ends, it tries again to get wood. After the second pour, at "
	                "the end of its clip, it goes to the storage pit for wood and builds on.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_PotDrop.x, 22.0f, k_PotDrop.y - 30.0f},
	                .look = {k_PotDrop.x, 0.0f, k_PotDrop.y}},
	    .objects = std::move(noWoodTown),
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 3},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f},
	                 {.kind = Kind::HandPour, .delaySeconds = 40.0f, .point = k_DropPit, .value = 300},
	                 {.kind = Kind::HandPour, .delaySeconds = 60.0f, .point = k_DropPit, .value = 300}},
	    .townBeliefInPlayer = 1000.0f,
	});

	std::vector<ObjectSetup> templeTown {
	    {.type = AbodeInfo::NorseTownCentre, .offset = k_TempleTown + glm::vec2(-30.0f, 0.0f)},
	    {.type = AbodeInfo::NorseStoragePit, .offset = k_TempleTown + glm::vec2(30.0f, 0.0f), .yawDegrees = 180.0f},
	    {.type = VillagerInfo::NorseForesterMale, .offset = k_TempleDrop, .joinTown = true, .woodHeld = 250}};
	all.push_back({
	    .id = "villagers.builder_mends_hurt_temple",
	    .name = "A builder put down by the hurt temple mends it",
	    .facet = Facet::Hand,
	    .description = "The player's temple has half of its life. A Norse town of the player with a village centre and a "
	                   "storage pit stands 50 metres from the temple, so its worship site goes up away from the temple. One "
	                   "villager of the town that carries 250 wood stands 22 metres east of the middle of the temple, under "
	                   "the hand. After two seconds the hand takes the villager, and three seconds later it puts the "
	                   "villager down at the same place.",
	    .expected = "Over the hurt temple the villager glows and shows the builder's symbol. It lands, turns to the temple "
	                "and takes up the work there: the town opens a site at the temple, and the temple wants its builders "
	                "by its lost life, as each thing with the fixed mark that is not a building of a town. The villager "
	                "builds at the temple, and the life of the temple goes up (the inspector's ObjectLife of the "
	                "temple). Before, the villager did not look at the temple, and a site at a hurt temple wanted no "
	                "builders.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_TempleDrop.x + 10.0f, 25.0f, k_TempleDrop.y + 35.0f},
	                .look = {k_TempleDrop.x, 0.0f, k_TempleDrop.y}},
	    .objects = std::move(templeTown),
	    .temples = {{.offset = k_HurtTemple, .life = 0.5f}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 2},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = 1000.0f,
	});
}
