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

/// Where the hand puts a villager down, south of a hurt temple, and the temple. The town stands north of the temple, so its
/// worship site goes up on the far side of the temple.
constexpr glm::vec2 k_HurtTemple {-10.0f, -60.0f};
constexpr glm::vec2 k_TempleDrop {12.0f, -60.0f};
constexpr glm::vec2 k_TempleTown {-10.0f, -110.0f};
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
	                .eye = {k_BuilderDrop.x, 22.0f, k_BuilderDrop.y - 30.0f},
	                .look = {k_BuilderDrop.x, 0.0f, k_BuilderDrop.y}},
	    .objects = std::move(builderTown),
	    .temples = {{.offset = k_PileTemple}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 2},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
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
