/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the flock miracles: doves for a good caster, bats for an evil one, and a pack of wolves
// swept out towards villagers, each cast through the hand and swept across the land as a player does

#include <utility>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The hand casts a little south of the spot and sweeps east across it, the camera looking north
constexpr glm::vec2 k_HandFrom {-12.0f, 0.0f};
constexpr glm::vec2 k_Spot {0.0f, 30.0f};
constexpr glm::vec3 k_SweepEast {16.0f, 0.0f, 0.0f};
/// Cast at a point, the hand over the land just south of it, so the animals head north
constexpr glm::vec2 k_HandBehind {0.0f, 25.0f};
/// The hand lets go once armed, then sweeps on for the second or so the flock takes to pour out
constexpr float k_HoldSeconds = 0.4f;
constexpr float k_SweepSeconds = 1.6f;
/// On the second land, from the middle of the land: the flocks are cast 90 metres south of the player's first town and
/// go north into it
constexpr glm::vec2 k_LandTwoBase {991.5f, 586.5f};
} // namespace

void testbed_scenarios::AddFlockScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.flock_doves",
	    .name = "Flying flock: doves",
	    .facet = Facet::Miracles,
	    .description = "A good player (alignment 0.8) casts the flying flock by hand and sweeps the hand east across the "
	                   "land for a second and a half.",
	    .expected = "A dozen white doves, all 2.8 to 3 times a dove's size, pour out of the hand along its sweep with a "
	                "single coo as the first appears, each trailing pale blue sparkles scaled to its size; a sparkling trail "
	                "follows the hand until the last is out. They climb to about 45 m, fanning out north up to two radians "
	                "either side of the view, the first leading and the rest following it, banking as they turn, each "
	                "flapping on its own beat. The leader roams on about wherever it is, not about the cast point. After 25 "
	                "seconds the miracle ends; the doves fly on whole for two seconds, then vanish.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 120.0f}}},
	    .miracles = {{.type = MagicType::FlockFlying,
	                  .point = k_Spot,
	                  .handOffset = k_HandFrom,
	                  .handHeight = 12.0f,
	                  .throwVelocity = k_SweepEast,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = k_HoldSeconds,
	                  .byHand = true,
	                  .sweepSeconds = k_SweepSeconds,
	                  .casterAlignment = 0.8f}},
	});

	all.push_back({
	    .id = "miracles.flock_bats",
	    .name = "Flying flock: bats",
	    .facet = Facet::Miracles,
	    .description = "An evil player (alignment -0.8) casts the flying flock by hand and sweeps it east, a creature "
	                   "standing to the north.",
	    .expected = "Black bats, 2.8 to 3 times a bat's size, flap out of the hand along the sweep with a single screech "
	                "as the first appears, each trailing black smoke puffs, and a dark smoky trail follows the hand. They fly "
	                "north at about 45 m in a fan, the first leading. The "
	                "bats count as frightening to creatures (doves don't). They always flap. After 25 seconds they fly on "
	                "whole for two seconds, then vanish.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 120.0f}}},
	    .creatures = {{.label = "watching", .offset = {0.0f, 70.0f}, .facingDegrees = 0.0f}},
	    .miracles = {{.type = MagicType::FlockFlying,
	                  .point = k_Spot,
	                  .handOffset = k_HandFrom,
	                  .handHeight = 12.0f,
	                  .throwVelocity = k_SweepEast,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = k_HoldSeconds,
	                  .byHand = true,
	                  .sweepSeconds = k_SweepSeconds,
	                  .casterAlignment = -0.8f}},
	});

	all.push_back({
	    .id = "miracles.flock_wolves",
	    .name = "Ground flock: wolves hunting villagers",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast by hand and swept east, with villagers standing in the wolves' way to the "
	                   "north and one well off to the side.",
	    .expected = "Fourteen wolves, 1.5 to 2 times a wolf's size, appear along the sweep, each with a puff and a trail "
	                "of dust the colour of the ground, a single howl as the first appears, and run north. The villagers "
	                "within 45 m either side of their run are chased, pounced on and brought down, and the wolves stop to "
	                "eat; a villager is eaten over 30 seconds and then dies as villagers die, falling and lying dead. "
	                "The villager off to the side is left alone. Fed wolves run on; the miracle ends after 60 seconds and "
	                "the wolves fade away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 110.0f}}, .distance = 0.7f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-5.0f, 55.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {8.0f, 70.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 90.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-12.0f, 105.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {110.0f, 40.0f}}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandFrom,
	                  .handHeight = 12.0f,
	                  .throwVelocity = k_SweepEast,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = k_HoldSeconds,
	                  .byHand = true,
	                  .sweepSeconds = k_SweepSeconds}},
	});

	all.push_back({
	    .id = "animals.predators_village",
	    .name = "Hunters frighten a village",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point south of a village of two huts, its wolves running north through "
	                   "it: four villagers of the player's town beside the huts and two cows south of them.",
	    .expected = "Each wolf carries a frightening reaction reaching 25 m. A villager it reaches is frightened (half the "
	                "table's urgency, 80): from a wolf running at it, or one time in six, it heads along the paths for the "
	                "nearest hut to hide, staying inside while a wolf is within 30 m and 15 to 30 seconds after, then "
	                "steps out and looks about, going back in if a wolf is still within 25 m; otherwise it runs 10 m away "
	                "from a wolf coming at it or nearer than 30 m, or turns to watch one further off, giving up once the "
	                "wolf is 75 m away. A wolf gives up a villager that has got inside. The cows take fright at a running "
	                "wolf and run 10 m from it at their fleeing speed, then flee or watch it until it is 75 m away. The "
	                "wolves run at 10 m a second, so most of the frightened are caught before they reach a hut.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-45.0f, 0.0f}, {45.0f, 110.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-30.0f, 75.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {32.0f, 80.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-30.0f, 69.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {32.0f, 74.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-24.0f, 75.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {26.0f, 80.0f}, .joinTown = true},
	                {.type = AnimalInfo::Cow, .offset = {-18.0f, 48.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 52.0f}}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "animals.predators_creche",
	    .name = "Hunters frighten a village with a crèche",
	    .facet = Facet::Miracles,
	    .description = "As the hunters frightening a village, but the village has one crèche and no huts: four villagers "
	                   "of the player's town stand beside the crèche and two cows south of them.",
	    .expected = "A frightened villager that heads off to hide goes to the crèche. It goes to the middle of the "
	                "crèche, not to its door, and goes inside there. When it comes out to look about, it steps out from "
	                "the middle. The children of a town still go to the crèche's door.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-45.0f, 0.0f}, {45.0f, 110.0f}}},
	    .objects = {{.type = AbodeInfo::CelticCreche, .offset = {0.0f, 78.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, 69.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {12.0f, 69.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, 66.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {6.0f, 66.0f}, .joinTown = true},
	                {.type = AnimalInfo::Cow, .offset = {-18.0f, 48.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 52.0f}}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "animals.wolf_pounce_downed",
	    .name = "A wolf brings a villager down",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point south of one villager, so that the wolves run at the villager. "
	                   "Pick the villager up from the wolf while it lies on the ground to rescue it.",
	    .expected = "A wolf pounces and the villager falls (the clip of an attack by a lion), with a life of 0.05. Then the "
	                "villager lies in the clip of a death while the wolf eats it for 30 seconds, and then it dies. Picked "
	                "up before then, it lives on: put down, it crawls. Its wounds never send it home, and the wolves do "
	                "not hunt it again. A wolf that comes back to it only eats.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 80.0f}}, .distance = 0.6f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 50.0f}}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "animals.cow_rescued_from_wolf",
	    .name = "The hand takes a cow from a wolf",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point south of a cow, so that the wolves run at the cow. Pick the cow "
	                   "up while it lies on the ground, then put it down or throw it.",
	    .expected = "A wolf pounces and the cow falls in its dying clip, with a life of 0.05. Then the cow lies in its dead "
	                "clip while the wolf eats it for 30 seconds, and then it dies and its body goes after 5 seconds. "
	                "Picked up before then, the cow plays its clip for the hand and the count stops. Put down or thrown, "
	                "it gets up, joins a herd near it if it can and wanders off. It lives on, and the wolves do not hunt "
	                "it again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 80.0f}}, .distance = 0.6f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 50.0f}, .living = true}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	});

	// The hand takes the cow that a wolf eats: the wolf lets go of its meal at once and runs on
	all.push_back({
	    .id = "animals.wolf_meal_in_hand",
	    .name = "The hand takes the meal of a wolf",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point south of a cow, so that the wolves run at the cow. When a wolf "
	                   "eats the cow, the hand takes the cow. Five seconds later the hand lets it go.",
	    .expected = "A wolf pounces and the cow falls in its dying clip. The wolf goes to the cow and eats. On the turn "
	                "that the hand takes the cow, the wolf stops its meal and runs on to where the miracle sent it. Let "
	                "go, the cow gets up and wanders off.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {8.0f, 9.0f, 34.0f}, .look = {0.0f, 2.0f, 50.0f}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 50.0f}, .living = true}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	    .commands = {{.kind = Command::Kind::HandTake, .delaySeconds = 10.0f, .object = 0},
	                 {.kind = Command::Kind::HandRelease, .delaySeconds = 5.0f}},
	});

	// A cow that a kill made dying, in the hand for a time and then let go: in the hand it does not die or go
	all.push_back({
	    .id = "animals.dying_cow_in_hand",
	    .name = "The hand holds a dying cow",
	    .facet = Facet::Miracles,
	    .description = "A cow with no life falls dying. After a second the hand takes it. Eight seconds later the hand "
	                   "lets it go.",
	    .expected = "In the hand the cow plays its clip for the hand. The inspector shows the state of the hand, the mark "
	                "of a kill, and a hunger that grows each turn. The cow does not fall dead in the hand. Let go, it lies "
	                "dead at once where it lands, and its body goes after 60 seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 7.0f, 38.0f}, .look = {0.0f, 3.0f, 50.0f}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 50.0f}, .life = 0.0f, .living = true}},
	    .commands = {{.kind = Command::Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Command::Kind::HandRelease, .delaySeconds = 8.0f}},
	});

	all.push_back({
	    .id = "animals.wolves_spare_indestructible_cow",
	    .name = "Wolves leave alone a cow that nothing can kill",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point south of a cow that a script made indestructible.",
	    .expected = "The wolves run past the cow and do not hunt it. The cow continues to graze and wander as before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 80.0f}}, .distance = 0.6f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 50.0f}, .indestructible = true, .living = true}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "animals.thrown_and_landed",
	    .name = "Farm animals thrown",
	    .facet = Facet::Miracles,
	    .description = "A cow, a sheep, a pig and a horse are each thrown from the west every ten seconds. A herd of "
	                   "three cows stands to the north for the hand to pick up.",
	    .expected = "Each animal plays its kind's clip while it flies. It comes down and plays the clip to get up of how it "
	                "lies: on its feet, on its right side, or on its left side. Its herd's home moves to where it came "
	                "down. Then it joins a herd near it if it can and wanders off. Held in the hand, an animal plays its "
	                "kind's clip for the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 10.0f}, {30.0f, 70.0f}}, .distance = 0.7f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {-12.0f, 30.0f}, .living = true},
	                {.type = AnimalInfo::Sheep, .offset = {-4.0f, 30.0f}, .living = true},
	                {.type = AnimalInfo::Pig, .offset = {4.0f, 30.0f}, .living = true},
	                {.type = AnimalInfo::Horse, .offset = {12.0f, 30.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {-6.0f, 60.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {0.0f, 62.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {6.0f, 60.0f}, .living = true}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {-30.0f, 30.0f},
	                .height = 4.0f,
	                .velocity = {14.0f, 9.0f, 0.0f},
	                .delaySeconds = 1.0f,
	                .repeatSeconds = 10.0f},
	               {.object = 1,
	                .from = glm::vec2 {-30.0f, 38.0f},
	                .height = 4.0f,
	                .velocity = {14.0f, 9.0f, 0.0f},
	                .delaySeconds = 1.5f,
	                .repeatSeconds = 10.0f},
	               {.object = 2,
	                .from = glm::vec2 {-30.0f, 22.0f},
	                .height = 4.0f,
	                .velocity = {14.0f, 9.0f, 0.0f},
	                .delaySeconds = 2.0f,
	                .repeatSeconds = 10.0f},
	               {.object = 3,
	                .from = glm::vec2 {-30.0f, 46.0f},
	                .height = 4.0f,
	                .velocity = {14.0f, 9.0f, 0.0f},
	                .delaySeconds = 2.5f,
	                .repeatSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "animals.thrown_bird_lands",
	    .name = "A bird thrown",
	    .facet = Facet::Miracles,
	    .description = "A dove is thrown from the west every ten seconds. Two more doves fly to the north, each in a "
	                   "flock of its own.",
	    .expected = "The thrown dove flies and comes down. Then it joins a dove flock near it if the flock has room, and it "
	                "flies off on a leg of its own or follows its new leader.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 10.0f}, {30.0f, 70.0f}}, .distance = 0.7f},
	    .objects = {{.type = AnimalInfo::Dove, .offset = {0.0f, 30.0f}, .living = true},
	                {.type = AnimalInfo::Dove, .offset = {-6.0f, 60.0f}, .living = true},
	                {.type = AnimalInfo::Dove, .offset = {6.0f, 60.0f}, .living = true}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {-30.0f, 30.0f},
	                .height = 4.0f,
	                .velocity = {14.0f, 9.0f, 0.0f},
	                .delaySeconds = 1.0f,
	                .repeatSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "animals.wild_hunters_hunt",
	    .name = "Wild hunters at noon",
	    .facet = Facet::Animals,
	    .description = "At noon a hungry lion and a hungry wolf, each made as a land's script makes one, stand 60 m apart, "
	                   "30 m south of three cows. A villager stands to the north east of the cows.",
	    .expected = "The lion finds the nearest prey in the map cells about it and goes after it. From 50 m to 100 m from "
	                "its prey it creeps in its prowl clip. Nearer than 50 m it runs. Near enough and facing the prey, it "
	                "leaps, and the prey falls. The lion goes to the prey at its hunting speed, settles down and eats 15 "
	                "to 24 mouthfuls, then decides again. A lion that does not face its prey, or chases it for more than "
	                "200 turns, gives up and wanders off. The wolf goes to its lair, lies down and stays there until late "
	                "at night. The cows and the villager flee from a hunter that comes near.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-35.0f, 15.0f}, {35.0f, 90.0f}}, .distance = 0.7f},
	    .objects = {{.type = AnimalInfo::Lion, .offset = {-30.0f, 30.0f}, .living = true, .hungry = true},
	                {.type = AnimalInfo::Wolf, .offset = {30.0f, 30.0f}, .living = true, .hungry = true},
	                {.type = AnimalInfo::Cow, .offset = {-8.0f, 60.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {8.0f, 62.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {0.0f, 70.0f}, .living = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {22.0f, 75.0f}}},
	});

	all.push_back({
	    .id = "animals.wolf_hunts_at_night",
	    .name = "A wolf leaves its lair late at night",
	    .facet = Facet::Animals,
	    .description = "At 23:20 a hungry wolf, made as a land's script makes one, stands 30 m south of three cows. The "
	                   "clock runs.",
	    .expected = "The wolf goes to its lair and lies down. When its need to sleep is gone and the hour is past 23:00, it "
	                "gets up and wanders about its lair. It finds the nearest cow, goes after it and leaps at it. The cow "
	                "falls, and the wolf eats it. At noon a hungry wolf stays in its lair (animals.wild_hunters_hunt).",
	    .environment = {.hour = 23.33f, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-35.0f, 15.0f}, {35.0f, 90.0f}}, .distance = 0.7f},
	    .objects = {{.type = AnimalInfo::Wolf, .offset = {0.0f, 30.0f}, .living = true, .hungry = true},
	                {.type = AnimalInfo::Cow, .offset = {-8.0f, 60.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {8.0f, 62.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {0.0f, 70.0f}, .living = true}},
	});

	all.push_back({
	    .id = "miracles.flock_wolves_fade",
	    .name = "Ground flock: wolves fade as it ends",
	    .facet = Facet::Miracles,
	    .description = "The ground flock cast at a point and closed down three seconds later, before the wolves are far.",
	    .expected = "Each wolf runs its own run, its legs in step with the ground it covers, bigger wolves striding slower. "
	                "The wolves are lit white rather than by the land. When the miracle ends they keep running while they "
	                "fade: slowly at first, fastest after a second and easing out, truly see-through (blended, not "
	                "speckled) until they are gone two seconds later.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 90.0f}}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 3.0f}},
	});

	all.push_back({
	    .id = "miracles.flock_doves_vanish",
	    .name = "Flying flock: doves vanish as it ends",
	    .facet = Facet::Miracles,
	    .description = "The flying flock cast at a point by a good player and closed down three seconds later.",
	    .expected = "Each dove flaps on its own beat, set by how fast it flies and how big it is, lit by the brightest of "
	                "the land's light. When the miracle ends the doves fly on whole for two seconds and then are gone at "
	                "once: the game counts their fade down but never draws it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 120.0f}}},
	    .miracles = {{.type = MagicType::FlockFlying,
	                  .point = k_Spot,
	                  .handOffset = k_HandBehind,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 3.0f,
	                  .casterAlignment = 0.8f}},
	});

	all.push_back({
	    .id = "miracles.l2_flock_bats",
	    .name = "Flying flock: bats at a village of the second land",
	    .facet = Facet::Miracles,
	    .description = "The second land of the story. An evil player (alignment -0.8) casts the flying flock by hand "
	                   "south of the player's first town and sweeps it east, the player's tiger standing to the north.",
	    .expected = "Black bats flap out of the hand along the sweep with a single screech, not one for each bat, and a "
	                "dark smoky trail follows the hand until the last bat is out. They fly north over the town. The bats "
	                "frighten the tiger when it sees them. After 25 seconds they fly on whole for two seconds, then vanish.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_LandTwoBase + glm::vec2(-40.0f, -10.0f), k_LandTwoBase + glm::vec2(40.0f, 120.0f)}},
	    .creatures = {{.label = "watching", .offset = k_LandTwoBase + glm::vec2(0.0f, 70.0f), .facingDegrees = 180.0f}},
	    .miracles = {{.type = MagicType::FlockFlying,
	                  .point = k_LandTwoBase + k_Spot,
	                  .handOffset = k_LandTwoBase + k_HandFrom,
	                  .handHeight = 12.0f,
	                  .throwVelocity = k_SweepEast,
	                  .delaySeconds = 8.0f,
	                  .holdSeconds = k_HoldSeconds,
	                  .byHand = true,
	                  .sweepSeconds = k_SweepSeconds,
	                  .casterAlignment = -0.8f}},
	});

	all.push_back({
	    .id = "miracles.l2_flock_wolves",
	    .name = "Ground flock: wolves at a village of the second land",
	    .facet = Facet::Miracles,
	    .description = "The second land of the story. The ground flock cast by hand south of the player's first town and "
	                   "swept east, with Norse villagers standing in the wolves' way to the north and one well off to "
	                   "the side.",
	    .expected = "The wolves appear along the sweep with a single howl, not one for each wolf, and run north. The "
	                "villagers in their way are chased and brought down: each one falls with the clip of a villager that "
	                "a lion attacks and lies there while a wolf eats it. A villager that a wolf cannot reach, or whose "
	                "kind has no food that the wolf eats, is not hunted. The villager off to the side is left alone. A "
	                "wolf set alight burns, drawn charred with flames.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_LandTwoBase + glm::vec2(-20.0f, 0.0f), k_LandTwoBase + glm::vec2(20.0f, 110.0f)},
	                .distance = 0.7f},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_LandTwoBase + glm::vec2(-5.0f, 55.0f)},
	                {.type = VillagerInfo::NorseForesterMale, .offset = k_LandTwoBase + glm::vec2(8.0f, 70.0f)},
	                {.type = VillagerInfo::NorseFarmerMale, .offset = k_LandTwoBase + glm::vec2(0.0f, 90.0f)},
	                {.type = VillagerInfo::NorseForesterMale, .offset = k_LandTwoBase + glm::vec2(-12.0f, 105.0f)},
	                {.type = VillagerInfo::NorseFarmerMale, .offset = k_LandTwoBase + glm::vec2(110.0f, 40.0f)}},
	    .miracles = {{.type = MagicType::FlockGround,
	                  .point = k_LandTwoBase + k_Spot,
	                  .handOffset = k_LandTwoBase + k_HandFrom,
	                  .handHeight = 12.0f,
	                  .throwVelocity = k_SweepEast,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = k_HoldSeconds,
	                  .byHand = true,
	                  .sweepSeconds = k_SweepSeconds},
	                 {.type = MagicType::Fireball,
	                  .point = k_LandTwoBase + glm::vec2(0.0f, 60.0f),
	                  .handOffset = k_LandTwoBase + glm::vec2(0.0f, 45.0f),
	                  .handHeight = 15.0f,
	                  .delaySeconds = 6.0f}},
	});

	// The ground flock of the second land, cast by hand as in the scenario above, at one villager in the way of the wolves
	const auto wolvesAtOneVillager = [](std::string_view id, std::string_view name, std::string_view description,
	                                    std::string_view expected, float takeSeconds, float holdSeconds) {
		return Scenario {
		    .id = id,
		    .name = name,
		    .facet = Facet::Miracles,
		    .description = description,
		    .expected = expected,
		    .environment = {.land = Land::Story,
		                    .storyLand = "Land2.txt",
		                    .dispenserGrid = false,
		                    .cursor = glm::vec2(0.5f, 0.5f)},
		    .framing = {.shot = Shot::Overview,
		                .include = {k_LandTwoBase + glm::vec2(-20.0f, 20.0f), k_LandTwoBase + glm::vec2(20.0f, 90.0f)},
		                .distance = 0.7f},
		    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_LandTwoBase + glm::vec2(0.0f, 55.0f)}},
		    .miracles = {{.type = MagicType::FlockGround,
		                  .point = k_LandTwoBase + k_Spot,
		                  .handOffset = k_LandTwoBase + k_HandFrom,
		                  .handHeight = 12.0f,
		                  .throwVelocity = k_SweepEast,
		                  .delaySeconds = 1.0f,
		                  .holdSeconds = k_HoldSeconds,
		                  .byHand = true,
		                  .sweepSeconds = k_SweepSeconds}},
		    .commands = {{.kind = Command::Kind::HandTake, .delaySeconds = takeSeconds, .object = 0},
		                 {.kind = Command::Kind::HandRelease, .delaySeconds = holdSeconds}},
		};
	};

	// The hand takes a villager from the wolf that eats it. The villager lives on with little life
	all.push_back(wolvesAtOneVillager(
	    "villagers.l2_wolf_prey_taken", "Ground flock: the hand takes a villager from a wolf on the second land",
	    "The second land of the story. The ground flock cast by hand south of the player's first town, with a Norse "
	    "farmer in the way of the wolves. While a wolf eats the farmer, the hand takes the farmer and puts it down again "
	    "below the hand half a second later.",
	    "A wolf brings the farmer down. The farmer falls with the clip of a villager that a lion attacks, and a wolf eats "
	    "it. The hand takes the farmer and puts it down. The farmer does not die: it lives on with a twentieth of its "
	    "life and crawls. No wolf hunts it again, also when it crawls in the way of the wolves. Its wounds do not send it "
	    "home.",
	    7.5f, 0.5f));

	// The hand takes a villager that a wolf chases. The wolf gives up its prey
	all.push_back(wolvesAtOneVillager(
	    "villagers.l2_wolf_chased_prey_taken", "Ground flock: the hand takes a villager that a wolf chases",
	    "The second land of the story. The ground flock cast by hand south of the player's first town, with a Norse "
	    "farmer in the way of the wolves. While a wolf chases the farmer, the hand takes the farmer. The hand puts the "
	    "farmer down again below the hand 20 seconds later.",
	    "A wolf chases the farmer. On the turn that the hand takes the farmer, the wolf stops the chase and runs to "
	    "where the miracle sent it. The wolf does not go to the last place of its prey. When the hand puts the farmer "
	    "down, the farmer has all of its life and continues its life in the town.",
	    4.5f, 20.0f));

	// The hand throws a fireball at the wolf that eats a villager. The wolf burns
	auto burning = wolvesAtOneVillager(
	    "miracles.l2_flock_wolf_burns", "Ground flock: a wolf burns on the second land",
	    "The second land of the story. The ground flock cast by hand south of the player's first town, with a Norse "
	    "farmer in the way of the wolves. While a wolf eats the farmer, the hand throws a fireball at the wolf.",
	    "The fireball sets the wolf alight. The burning wolf is grey, as a charred thing is, with the red glow of the "
	    "fire on it and flames on its body. It keeps its fade when the miracle ends.",
	    0.0f, 0.0f);
	burning.commands.clear();
	burning.miracles.push_back({.type = MagicType::Fireball,
	                            .point = k_LandTwoBase + glm::vec2(0.0f, 55.0f),
	                            .handOffset = k_LandTwoBase + glm::vec2(0.0f, 42.0f),
	                            .handHeight = 8.0f,
	                            .throwVelocity = {0.0f, 0.0f, 10.0f},
	                            .delaySeconds = 6.0f});
	all.push_back(std::move(burning));
}
