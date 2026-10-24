/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the teleport miracle: two stones cast far apart by hand, villagers walking from one to the
// other that take the shortcut, a villager put down on a stone that jumps at once, and creatures on the leash that a
// stone takes or refuses

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The two stones, south-west and north-east of the middle
constexpr glm::vec2 k_StoneA {-40.0f, 20.0f};
constexpr glm::vec2 k_StoneB {40.0f, 70.0f};
/// The villagers start beside the first and walk to beside the second, and back
constexpr glm::vec2 k_Start {-34.0f, 24.0f};
constexpr glm::vec2 k_Goal {46.0f, 74.0f};
/// They set off once both stones are down, and turn back after this long
constexpr float k_WalkAfter = 5.0f;
constexpr float k_WalkRepeat = 30.0f;
/// A creature told to go at the walk time is in the stone this long after it is told: it nods, turns aside, walks to the stone
/// and jumps
constexpr float k_TapWhileCarried = 4.5f;
/// By this long after the tap in the stone, the creature is out of the far stone and walks on
constexpr float k_TapAfterArrival = 8.0f;
/// The leash goes on just before the order, so that the leash, which holds the creature near the hand, does not make
/// it walk away first
constexpr float k_LeashBefore = 0.5f;
/// A creature told to go to the goal from here passes the first stone after a walk of some 35
constexpr glm::vec2 k_FarStart {-70.0f, 2.0f};
/// The second stone goes this long after the scenario starts, while the creature walks to the first stone
constexpr float k_RemoveStoneAfter = 9.0f;
/// A body that wants nothing: full, rested, watered, comfortable
constexpr NeedOverrides k_Content {
    .energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f};
/// Two stones too far apart for a villager to walk between to worship
constexpr glm::vec2 k_WorshipStoneA {-300.0f, 0.0f};
constexpr glm::vec2 k_WorshipStoneB {300.0f, 0.0f};
} // namespace

void testbed_scenarios::AddTeleportScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.teleport_pool_frames",
	    .name = "A teleport pool close up, stepped in the drawn frames",
	    .facet = Facet::Miracles,
	    .description = "One teleport miracle cast by hand on open land, with the camera close and still. The pool moves on "
	                   "in each frame that draws its stone, no more often than 60 times a second, not once a turn.",
	    .expected = "The pool of land turns and its rim fades in and out smoothly, with no jerk ten times a second. It "
	                "continues to move when the camera stops, also while the pool is at the edge of the screen. Its "
	                "random numbers are only this computer's, so the world stays the same.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, 12.0f}, .look = {0.0f, 0.0f, 25.0f}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = {0.0f, 25.0f},
	                  .handOffset = {0.0f, 25.0f},
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true}},
	});

	all.push_back({
	    .id = "miracles.teleport_two_points",
	    .name = "Teleport between two points",
	    .facet = Facet::Miracles,
	    .description = "Two teleport miracles cast by hand some 95 apart, then three villagers beside the first stone "
	                   "set off to walk home to beside the second, and back every 30 seconds. A fourth villager is put down "
	                   "on the first stone after 15 seconds, as the hand drops one.",
	    .expected = "Each cast leaves a swirling pool of land on the ground, a disc in its owner's colour in the middle "
	                "and fading at the rim, with a low hum. The walkers turn aside into the near stone, walking or running "
	                "as they were going, vanish from it once within a step of it in a burst of pale yellow sparks with a "
	                "whoosh and come out of the other in another burst, then walk the last few steps; the way back goes "
	                "through the stones too. The dropped villager decides what to do as it is put down and jumps at once "
	                "to the other stone. The debug window's prayer power of each teleport goes up a little with every "
	                "useful jump, and is topped up from the player's store after every jump.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_Start, k_Goal}, .distance = 0.8f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Start,
	                 .walkTo = k_Goal,
	                 .walkAfterSeconds = k_WalkAfter,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_Start + glm::vec2(2.0f, -2.0f),
	                 .walkTo = k_Goal + glm::vec2(2.0f, -2.0f),
	                 .walkAfterSeconds = k_WalkAfter + 1.0f,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticLeaderMale,
	                 .offset = k_Start + glm::vec2(-2.0f, 2.0f),
	                 .walkTo = k_Goal + glm::vec2(-2.0f, 2.0f),
	                 .walkAfterSeconds = k_WalkAfter + 2.0f,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(-8.0f, -4.0f),
	                 .dropOnStoneSeconds = 15.0f}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	});

	all.push_back({
	    .id = "miracles.teleport_creature",
	    .name = "A creature takes the teleport",
	    .facet = Facet::Miracles,
	    .description = "The same two stones cast by hand, then a content tiger beside the first sets off to walk to beside "
	                   "the second.",
	    .expected = "The tiger turns aside into the near stone, curious: it walks to the stone with a puzzled face, and "
	                "within 5 m of it, it bursts into yellow sparks there and at the far stone while it fizzes out of sight "
	                "over two seconds. It comes out of the far stone with the sound of arriving (and no sound of going), "
	                "fizzes back into sight over two seconds, then walks on to within 5 m of where it was going and decides "
	                "what to do next.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_Start, k_Goal}, .distance = 0.8f},
	    .creatures = {{.label = "traveller",
	                   .species = CreatureType::Tiger,
	                   .offset = k_Start + glm::vec2(-6.0f, 2.0f),
	                   .facingDegrees = 180.0f,
	                   .needs = k_Content,
	                   .hold = true}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Command::Kind::WalkTo, .creature = 0, .delaySeconds = k_WalkAfter, .point = k_Goal}},
	});

	all.push_back({
	    .id = "miracles.teleport_creature_refused",
	    .name = "A teleport stone refuses a creature",
	    .facet = Facet::Miracles,
	    .description = "The same two stones cast by hand. A content tiger on the learning leash, some 35 from the first "
	                   "stone, is told with the leash to go to beside the second. While it walks to the first stone, the "
	                   "second stone goes.",
	    .expected = "The tiger turns aside into the near stone, curious: it walks to the stone with a puzzled face. Within "
	                "5 m of the stone it asks for the jump, but with no other stone the stone refuses it. The tiger does "
	                "not jump and does not wait: its plan stops there. Then the leash takes it back towards the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_FarStart, k_Goal}, .distance = 0.8f},
	    .creatures = {{.label = "traveller",
	                   .species = CreatureType::Tiger,
	                   .offset = k_FarStart,
	                   .facingDegrees = 60.0f,
	                   .needs = k_Content,
	                   .hold = true}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Command::Kind::PutOnLeash,
	                  .creature = 0,
	                  .delaySeconds = k_WalkAfter - k_LeashBefore,
	                  .leash = LeashType::Rope},
	                 {.kind = Command::Kind::LeashOrderAt, .creature = 0, .delaySeconds = k_LeashBefore, .point = k_Goal},
	                 {.kind = Command::Kind::RemoveTeleportStone,
	                  .delaySeconds = k_RemoveStoneAfter - k_WalkAfter,
	                  .point = k_StoneB}},
	});

	all.push_back({
	    .id = "miracles.teleport_worship",
	    .name = "A worshipper goes through the stones",
	    .facet = Facet::Miracles,
	    .description = "Two teleport miracles cast 600 apart, then a villager beside the first sets off to worship at a "
	                   "point beside the second, further than villagers walk to worship.",
	    .expected = "The villager turns aside into the near stone at once, vanishes in a burst of pale yellow sparks and "
	                "comes out of the far one, near the worship point, and stands there on its way to worship: with no "
	                "worship site there, its way to worship walks it nowhere further.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_WorshipStoneB + glm::vec2(-20.0f, -20.0f), k_WorshipStoneB + glm::vec2(20.0f, 20.0f)}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_WorshipStoneA + glm::vec2(4.0f, 3.0f),
	                 .walkAfterSeconds = 6.0f,
	                 .worshipAt = k_WorshipStoneB + glm::vec2(6.0f, 4.0f)}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_WorshipStoneA,
	                  .handOffset = k_WorshipStoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_WorshipStoneB,
	                  .handOffset = k_WorshipStoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	});

	all.push_back({
	    .id = "leash.tap_while_teleported",
	    .name = "A tap on a hut while a teleport carries the creature on the leash",
	    .facet = Facet::Leash,
	    .description = "The same two stones cast by hand, a hut beside the first. A tiger on the learning leash is told "
	                   "to go to beside the second stone. While the stone carries it, the hand taps the hut. When it "
	                   "is out of the far stone, the hand taps the hut again.",
	    .expected = "The first tap is not an order: the hand knocks on the hut, and the tiger continues to the far "
	                "stone. The second tap is an order, so the hand does not knock, and the tiger goes back to act on "
	                "the hut. The debug log gives each result of the command.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_Start, k_Goal}, .distance = 0.8f},
	    .creatures = {{.label = "traveller",
	                   .species = CreatureType::Tiger,
	                   .offset = k_Start + glm::vec2(-6.0f, 2.0f),
	                   .facingDegrees = 180.0f,
	                   .needs = k_Content,
	                   .hold = true}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_StoneA + glm::vec2(-14.0f, -12.0f)}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Command::Kind::PutOnLeash,
	                  .creature = 0,
	                  .delaySeconds = k_WalkAfter - k_LeashBefore,
	                  .leash = LeashType::Rope},
	                 {.kind = Command::Kind::LeashOrderAt, .creature = 0, .delaySeconds = k_LeashBefore, .point = k_Goal},
	                 {.kind = Command::Kind::LeashTapBuilding, .creature = 0, .delaySeconds = k_TapWhileCarried},
	                 {.kind = Command::Kind::LeashTapBuilding, .creature = 0, .delaySeconds = k_TapAfterArrival}},
	});
}
