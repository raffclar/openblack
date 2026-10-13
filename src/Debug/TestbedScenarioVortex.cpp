/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the vortices between the lands: a way out of a land opening on the lake's bank, levelling
// the bank and the water's edge towards their average, and closing again; and the volcano's mouth, which leaves the land
// as it is

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// On the lake's south bank: the levelled square takes in the plane, the bank and the shallows
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
constexpr glm::vec2 k_OnTheBank = k_Lake - glm::vec2(0.0f, flat_land::k_LakeHalfExtent.y + 5.0f);
/// At the top of the lake's south bank, where the slope begins, in the middle of its land block: the volcano's mouth is
/// only ever opened high above the water (its rock under the land would show through the shallows' clear land as it
/// would in the game), and its hole and ring are only on the block under its middle
constexpr glm::vec2 k_AboveTheBank = k_OnTheBank + glm::vec2(80.0f, -80.0f);
/// Out on the plane, where the land is already flat
constexpr glm::vec2 k_OnThePlane {-120.0f, 60.0f};

/// A thing thrown east from where it lies, 6 m up, at 25 m a second and 6 m a second upwards: from 60 m west of a
/// vortex it reaches the vortex's cylinder in about 1.6 seconds, about 3 m above the land
ThrowSetup ThrownIn(size_t object, glm::vec2 from, float delaySeconds)
{
	return {.object = object, .from = from, .height = 6.0f, .velocity = {25.0f, 6.0f, 0.0f}, .delaySeconds = delaySeconds};
}
} // namespace

void testbed_scenarios::AddVortexScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.vortex_levels_the_bank",
	    .name = "A vortex out of the land levels the lake's bank",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as a land's script opens one, "
	                   "and another out on the flat plane; both are told to fade out 20 seconds after they open.",
	    .expected =
	        "For 2 seconds after each opens nothing happens; then over 5 seconds the bank and the water's edge in "
	        "an 11 by 11 cell square round the first are pulled towards the square's average height, the slope "
	        "easing out as it goes, wholly within 50 of the middle and less towards 56; the plane's stays flat. "
	        "Fading out does not undo it: the levelled bank stays after the vortices have gone, 7 seconds after "
	        "they were told to fade out. Each opens a hole in its land block under its middle, the swirl showing through it, "
	        "with its ring of light and glow on the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_OnTheBank, k_OnThePlane, k_Lake}, .distance = 0.8f},
	    .vortices = {{.type = VortexType::In, .offset = k_OnTheBank, .delaySeconds = 1.0f, .fadeOutAfterSeconds = 20.0f},
	                 {.type = VortexType::In, .offset = k_OnThePlane, .delaySeconds = 1.0f, .fadeOutAfterSeconds = 20.0f}},
	});

	all.push_back({
	    .id = "story.vortex_levels_under_the_living",
	    .name = "The living go down with the land a vortex levels",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as Land 1's opens beside the "
	                   "fisherman's wife waiting on the cliff. On the slope above it, within the square it levels, a "
	                   "villager stands still held by a script, a cow grazes, and an oak and a hut stand.",
	    .expected = "As the bank is levelled over 5 seconds, the villager and the cow go down (or up) with the land under "
	                "them, their feet and shadows on the ground the whole time, never floating. The oak and the hut keep "
	                "the height they were put at, as in the game, which draws them where they were made.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnTheBank.x - 30.0f, 13.0f, k_OnTheBank.y - 22.0f},
	                .look = {k_OnTheBank.x - 8.0f, 3.0f, k_OnTheBank.y - 22.0f}},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_OnTheBank + glm::vec2(-8.0f, -18.0f),
	                 .indestructible = true,
	                 .inScript = true},
	                {.type = AnimalInfo::Cow, .offset = k_OnTheBank + glm::vec2(-8.0f, -25.0f)},
	                {.type = TreeInfo::Oak, .offset = k_OnTheBank + glm::vec2(-8.0f, -11.0f)},
	                {.type = AbodeInfo::NorseHut, .offset = k_OnTheBank + glm::vec2(-8.0f, -34.0f), .yawDegrees = 90.0f}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnTheBank, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "story.creature_puts_down_actor",
	    .name = "A creature puts a villager down on land a vortex levelled",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as Land 1's opens beside the "
	                   "fisherman's wife on the cliff. On the slope above, within the square it levels but beyond its "
	                   "pull, two villagers stand still held by a script: on the right one the script has said can't be "
	                   "picked up or moved, as the wife is while she waits for the player; on the left one it has let "
	                   "be picked up, as the wife is after the rescue's first cinema. When the land is level the tiger "
	                   "tries to pick up the right one, then picks up the left one and puts her down; a while later it "
	                   "picks her up again and throws her a little way along the slope.",
	    .expected = "Both go down with the land as it is levelled. The tiger can't pick up the right one: it gives up at "
	                "once and she stays where she stands. Each time the left one is put down or thrown she falls to the "
	                "levelled land and stands on it, her feet and shadow on the ground, never floating at the height the "
	                "land had before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnTheBank.x - 30.0f, 14.0f, k_OnTheBank.y - 52.0f},
	                .look = {k_OnTheBank.x - 8.0f, 6.0f, k_OnTheBank.y - 44.0f}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_OnTheBank + glm::vec2(-8.0f, -60.0f),
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                   .hold = true,
	                   .pauseMind = true}},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_OnTheBank + glm::vec2(-8.0f, -44.0f),
	                 .indestructible = true,
	                 .inScript = true},
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_OnTheBank + glm::vec2(-8.0f, -38.0f),
	                 .indestructible = true,
	                 .inScript = true,
	                 .cannotBePickedUp = true,
	                 .immovable = true}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnTheBank, .delaySeconds = 1.0f}},
	    .commands = {{.kind = Command::Kind::PickUp, .creature = 0, .delaySeconds = 9.0f, .waitUntilFree = true, .object = 1},
	                 {.kind = Command::Kind::PutDown, .creature = 0, .delaySeconds = 1.0f, .waitUntilFree = true},
	                 {.kind = Command::Kind::PickUp, .creature = 0, .delaySeconds = 2.0f, .waitUntilFree = true, .object = 0},
	                 {.kind = Command::Kind::PutDown, .creature = 0, .delaySeconds = 1.0f, .waitUntilFree = true},
	                 {.kind = Command::Kind::PickUp, .creature = 0, .delaySeconds = 4.0f, .waitUntilFree = true, .object = 0},
	                 {.kind = Command::Kind::ThrowAt,
	                  .creature = 0,
	                  .delaySeconds = 1.0f,
	                  .waitUntilFree = true,
	                  .point = k_OnTheBank + glm::vec2(-14.0f, -58.0f)}},
	});

	all.push_back({
	    .id = "story.vortex_pulls_things_in",
	    .name = "A vortex out of the land pulls things in",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane after a second, as Land 1's opens for the player to "
	                   "leave by. Within 25 m of it stand a villager held by a script, a cow, an oak, a rock, a pile of "
	                   "food and a hut; a second cow grazes 45 m to the east.",
	    .expected = "Nothing is taken while the vortex fades in (7 seconds). Once it is fully open, the villager, the cow, "
	                "the oak and the rock leave the land at once and are drawn round the middle, slowly at first, faster "
	                "and faster as they come nearer, rising or sinking onto the funnel, tumbling, and gone into the "
	                "middle about 8 seconds after each was taken. The pile gives up pots of food, a smaller pot every few "
	                "turns, that go the same way, until it is empty. The hut stays, and so does the far cow: the vortex "
	                "only reaches 30 m along its sides (42 m to its corners). The crossing file vortex.txt in the "
	                "profile's saved games gets one line for each thing taken.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 30.0f, 45.0f, k_OnThePlane.y - 60.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_OnThePlane + glm::vec2(-10.0f, 8.0f),
	                 .inScript = true},
	                {.type = AnimalInfo::Cow, .offset = k_OnThePlane + glm::vec2(12.0f, -6.0f)},
	                {.type = TreeInfo::Oak, .offset = k_OnThePlane + glm::vec2(6.0f, 14.0f)},
	                {.type = MobileStaticInfo::Rock, .offset = k_OnThePlane + glm::vec2(-14.0f, -10.0f)},
	                {.type = PotInfo::FoodPile, .offset = k_OnThePlane + glm::vec2(0.0f, -18.0f), .amount = 3000},
	                {.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(20.0f, 18.0f), .yawDegrees = 90.0f},
	                {.type = AnimalInfo::Cow, .offset = k_OnThePlane + glm::vec2(45.0f, 0.0f)}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnThePlane, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "story.vortex_throws_things_out",
	    .name = "A vortex into the land throws out what crossed",
	    .facet = Facet::Miracles,
	    .description = "A vortex into a land opens on the flat plane after a second, as Land 2's opens when the player "
	                   "arrives, with a crossing file written as Land 1's vortex would have left it: a villager, two cows "
	                   "and a sheep, an oak, a rock, a pot of food and a dead tree. A hut stands 40 m off; the animals "
	                   "gather 30 m to the south, kept within 27 m and 48 m as Land 2's script says. It is told to fade "
	                   "out 3 seconds after it opens, long before all are out.",
	    .expected = "Every fifth of a second, in a random direction, something is thrown out of the vortex about 16 m from "
	                "its middle, flying 8 to 13 m a second outwards and 10 to 15 upwards, tumbling: first the file's "
	                "things, each where it lay about the old vortex (the dead tree is whole, at its model's size), then "
	                "new villagers until 30 have come out, every fifth a child. The villagers join the hut's town; the "
	                "two cows gather as one flock, the sheep in a new one. Nothing thrown out can be destroyed while the "
	                "vortex lasts. Though told to fade out, the vortex stays open until the last villager is out, "
	                "then fades over 5 seconds and goes 7 seconds after the last.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 40.0f, 50.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(40.0f, 0.0f), .yawDegrees = 90.0f}},
	    .vortices = {{.type = VortexType::Out,
	                  .offset = k_OnThePlane,
	                  .delaySeconds = 1.0f,
	                  .fadeOutAfterSeconds = 3.0f,
	                  .crossingLines = {"CREATE_TOWN_VILLAGER(-1, \"-10.00,8.00\", NORSE_FARMER, 30)",
	                                    "CREATE_NEW_ANIMAL(\"12.00,-6.00\", 8, -1, -1, 6)",
	                                    "CREATE_NEW_ANIMAL(\"10.00,-9.00\", 8, -1, -1, 3)",
	                                    "CREATE_NEW_ANIMAL(\"-4.00,12.00\", 4, -1, -1, 2)",
	                                    "CREATE_NEW_TREE(-1, \"6.00,14.00\", 5, 1, 0.000000, 0.800000, 1.000000)",
	                                    "CREATE_MOBILE_STATIC(\"-14.00,-10.00\", 2, 0.000000, 0.000000, 0.500000, 0.000000, "
	                                    "1.000000)",
	                                    "CREATE_POT(\"0.00,-18.00\", 12, 0, 1000)",
	                                    "CREATE_DEAD_TREE(\"8.00,-12.00\", \"NEUTRAL\", 5, 1.000000, 0.000000, 1.000000, "
	                                    "0.000000)"},
	                  .gatheringOffset = k_OnThePlane + glm::vec2(0.0f, -30.0f),
	                  .flockDistance = 27.0f,
	                  .flockRadius = 48.0f}},
	});

	all.push_back({
	    .id = "story.vortex_throws_out_the_last_crossing",
	    .name = "A vortex into the land throws out the last crossing",
	    .facet = Facet::Miracles,
	    .description = "A vortex into a land opens on the flat plane after a second, as Land 2's opens when the player "
	                   "arrives, reading the crossing file as the last vortex out of a land left it in this profile's "
	                   "saved games (play Land 1 to its vortex first, or any scenario whose vortex takes things). A hut "
	                   "stands 40 m off; the animals gather 30 m to the south.",
	    .expected = "Everything the last vortex out of a land took comes out, a thing every fifth of a second, each where it "
	                "lay about that vortex, thrown out about 16 m from the middle: villagers, animals of every kind at "
	                "their ages, trees, rocks and other statics at the height they were put down at (one for a pile of "
	                "identical ones), a rock that was a player's artefact still that player's artefact of the same worth, "
	                "and miracle globes holding the same miracle. Then new villagers until 30 have come out.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 40.0f, 50.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = AbodeInfo::NorseHut, .offset = k_OnThePlane + glm::vec2(40.0f, 0.0f), .yawDegrees = 90.0f}},
	    .vortices = {{.type = VortexType::Out,
	                  .offset = k_OnThePlane,
	                  .delaySeconds = 1.0f,
	                  .gatheringOffset = k_OnThePlane + glm::vec2(0.0f, -30.0f),
	                  .flockDistance = 27.0f,
	                  .flockRadius = 48.0f}},
	});

	all.push_back({
	    .id = "story.vortex_opens_a_hole",
	    .name = "A vortex out of the land opens a hole in it",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane after a second, seen from close above, and is told "
	                   "to fade out 40 seconds after it opens.",
	    .expected = "For 2 seconds nothing shows; then over 5 seconds a hole opens in the land from the swirl's darkest "
	                "strokes outwards until the whole disc, 45 across, is open, the spinning funnel showing down through "
	                "it over the sea's colour, with a ring of light laid on the land around it, the glow on the ground "
	                "and the stars spinning over it. The hole ends at the edge of its land block. Fading out closes it "
	                "again over 5 seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 25.0f, 60.0f, k_OnThePlane.y - 70.0f},
	                .look = {k_OnThePlane.x - 25.0f, 0.0f, k_OnThePlane.y + 10.0f}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnThePlane, .delaySeconds = 1.0f, .fadeOutAfterSeconds = 40.0f}},
	});

	all.push_back({
	    .id = "story.volcano_vortex_keeps_the_land",
	    .name = "The volcano's mouth leaves the land as it is",
	    .facet = Facet::Miracles,
	    .description = "The glowing mouth of a volcano is opened at the top of the lake's south bank, out of the water, "
	                   "as the last land's script opens one on its mountain.",
	    .expected = "It starts fully open and the slope of the bank under it is kept: the volcano's mouth never levels "
	                "the land. Its fiery hole, ring and glow show on the ground and it rumbles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_AboveTheBank, k_Lake}, .distance = 0.8f},
	    .vortices = {{.type = VortexType::Volcano, .offset = k_AboveTheBank, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "story.vortex_takes_what_is_thrown_in",
	    .name = "A vortex out of the land takes what is thrown into it",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane at once, as Land 1's opens for the player to leave by, "
	                   "with two rocks, a villager and a cow lying 60 m to its west, out of its reach. The player's hand "
	                   "throws the first rock at it from where it lies 3 seconds in, while it still fades in; then, once "
	                   "it is fully open, the second rock at 9 seconds and the villager at 10, and the cow at 11 from "
	                   "40 m off.",
	    .expected = "The vortex's unseen cylinder (20 m round its middle, 60 m high) is in the way of what flies from the "
	                "moment it is made: the first rock strikes it and is slowed in it, but nothing is taken while the vortex "
	                "fades in; the rock is still rolling inside the cylinder when the vortex opens fully, about 7 seconds "
	                "in, and is taken at once. The second rock, the villager and the cow are each taken as they strike "
	                "the cylinder, still flying: they leave the physics there and are drawn round the middle from that "
	                "place at the speed they flew with, curving into the swirl rather than falling, and are gone into "
	                "the middle some seconds later. Each gets its line in the crossing file.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 30.0f, 40.0f, k_OnThePlane.y - 75.0f},
	                .look = {k_OnThePlane.x - 20.0f, 0.0f, k_OnThePlane.y}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = k_OnThePlane + glm::vec2(-60.0f, 6.0f)},
	                {.type = MobileStaticInfo::Rock, .offset = k_OnThePlane + glm::vec2(-60.0f, 2.0f)},
	                {.type = VillagerInfo::NorseHousewifeFemale, .offset = k_OnThePlane + glm::vec2(-60.0f, -2.0f)},
	                {.type = AnimalInfo::Cow, .offset = k_OnThePlane + glm::vec2(-60.0f, -6.0f)}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnThePlane}},
	    .throws = {ThrownIn(0, k_OnThePlane + glm::vec2(-60.0f, 6.0f), 3.0f),
	               ThrownIn(1, k_OnThePlane + glm::vec2(-60.0f, 2.0f), 9.0f),
	               ThrownIn(2, k_OnThePlane + glm::vec2(-60.0f, -2.0f), 10.0f),
	               ThrownIn(3, k_OnThePlane + glm::vec2(-40.0f, -6.0f), 11.0f)},
	});

	all.push_back({
	    .id = "story.vortex_creature_keeps_clear",
	    .name = "The creature walks round a vortex out of the land",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the flat plane, as Land 1's opens for the player to leave by. The "
	                   "player's creature, off the leash and not let through by a script, stands 70 m south of it and "
	                   "after 2 seconds walks to a point 70 m north of it, straight through the vortex's middle, and "
	                   "then back.",
	    .expected = "Both ways the creature walks round the vortex, keeping about 20 m (the vortex's unseen cylinder) "
	                "plus part of its own reach from the middle, and is never taken in. A creature on the leash that a "
	                "script has let through, or one a script controls, would walk straight in instead.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OnThePlane.x - 90.0f, 80.0f, k_OnThePlane.y - 40.0f},
	                .look = {k_OnThePlane.x, 0.0f, k_OnThePlane.y}},
	    .creatures = {{.offset = k_OnThePlane + glm::vec2(0.0f, -70.0f), .facingDegrees = 180.0f, .pauseMind = true}},
	    .vortices = {{.type = VortexType::In, .offset = k_OnThePlane}},
	    .commands = {{.kind = Command::Kind::WalkTo, .delaySeconds = 2.0f, .point = k_OnThePlane + glm::vec2(0.0f, 70.0f)},
	                 {.kind = Command::Kind::WalkTo,
	                  .delaySeconds = 1.0f,
	                  .waitUntilFree = true,
	                  .point = k_OnThePlane + glm::vec2(0.0f, -70.0f)}},
	});
}
