/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the movement of the creature and its footprints.

#include <array>

#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddMovement(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "movement.course",
	    .name = "Walk, run, turn and step course",
	    .facet = Facet::Movement,
	    .description = "A tiger goes round a square course: walking one side, running the next, turning on the spot to "
	                   "face the middle, walking, running, then a short step off.",
	    .expected = "Its legs blend between standing, walking and running with its speed, it turns on the spot without "
	                "sliding its feet, and the short last leg is a step rather than a walk; the route shows over the "
	                "land in the spawner.",
	    .framing = {.shot = Shot::Overview, .include = {{70.0f, 70.0f}, {-70.0f, -70.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {60.0f, -60.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {60.0f, 60.0f}, 1.0f), Go(Kind::RunTo, 0, {-60.0f, 60.0f}, 0.5f),
	                 Go(Kind::TurnToFace, 0, {0.0f, 0.0f}, 0.5f), Go(Kind::WalkTo, 0, {-60.0f, -60.0f}, 1.0f),
	                 Go(Kind::RunTo, 0, {50.0f, -60.0f}, 0.5f), Go(Kind::WalkTo, 0, {60.0f, -60.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.lake",
	    .name = "Round the lake and through its shallows",
	    .facet = Facet::Movement,
	    .description = "A tiger on the west bank of the testbed's lake walks to the east bank, then down into the near "
	                   "shallows and back up onto the plain.",
	    .expected = "It never sets foot on the open water, which is too deep: its route bends round the lake on the bank "
	                "or wades its shallows; it walks into the shallows and stands with its feet at the water, then "
	                "climbs the bank.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x - k_LakeHalf.x - 50.0f, k_Lake.y}, 90.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {k_Lake.x + k_LakeHalf.x + 50.0f, k_Lake.y}, 1.0f),
	                 Go(Kind::WalkTo, 0, {k_Lake.x + 20.0f, k_NearShallows}, 1.0f),
	                 Go(Kind::WalkTo, 0, {k_Lake.x - k_LakeHalf.x - 50.0f, k_Lake.y}, 1.0f)},
	    .repeatFrom = 0,
	});

	// A tiger walks to and fro between the grass and the near shallows, near the camera
	const glm::vec2 onGrass {k_Lake.x - 20.0f, k_NearShallows - 30.0f};
	const glm::vec2 inShallows {k_Lake.x + 10.0f, k_NearShallows};
	all.push_back({
	    .id = "movement.footstep_dust",
	    .name = "Dust from the footsteps",
	    .facet = Facet::Movement,
	    .description = "A tiger walks to and fro between the grass and the near shallows of the testbed's lake at noon. "
	                   "The camera looks at it from near.",
	    .expected = "Each footstep makes a small cloud of brown dust at the tiger's feet, on the grass and in the "
	                "shallows. The dust moves out a little and goes in about a second.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.05f)},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x + 22.0f, 11.0f, k_NearShallows - 42.0f},
	                .look = {k_Lake.x - 5.0f, 2.0f, k_NearShallows - 15.0f}},
	    .creatures = {Posed(CreatureType::Tiger, onGrass, 45.0f)},
	    .commands = {Go(Kind::WalkTo, 0, inShallows, 1.0f), Go(Kind::WalkTo, 0, onGrass, 1.0f)},
	    .repeatFrom = 0,
	});

	// A tiger walks down the bank of the lake and up again, then along the bank, seen from the side
	const glm::vec2 aboveBank {k_Lake.x, k_NearShallows - 34.0f};
	const glm::vec2 belowBank {k_Lake.x, k_NearShallows - 2.0f};
	const glm::vec2 alongBank {k_Lake.x - 30.0f, k_NearShallows - 16.0f};
	const glm::vec2 alongBankEnd {k_Lake.x + 30.0f, k_NearShallows - 16.0f};
	all.push_back({
	    .id = "movement.land_tilt",
	    .name = "The body leans with the slope of the land",
	    .facet = Facet::Movement,
	    .description = "A tiger walks down the steep bank of the lake to the shallows and back up to the grass. Then it "
	                   "walks along the middle of the bank and back. The camera looks at it from the side.",
	    .expected = "On the flat grass the tiger stands level. On the bank its body leans with the slope: its head is "
	                "lower than its tail when it walks down, and higher when it walks up. Along the bank, the side of "
	                "the body that is up the slope is higher. Its legs and its neck stay upright, and do not lean over "
	                "with the land. The lean changes smoothly as the slope under it changes.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.05f)},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x + 55.0f, 14.0f, k_NearShallows - 20.0f},
	                .look = {k_Lake.x, 2.0f, k_NearShallows - 18.0f}},
	    .creatures = {Posed(CreatureType::Tiger, aboveBank, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, belowBank, 1.0f), Go(Kind::WalkTo, 0, aboveBank, 1.0f),
	                 Go(Kind::WalkTo, 0, alongBank, 1.0f), Go(Kind::WalkTo, 0, alongBankEnd, 1.0f),
	                 Go(Kind::WalkTo, 0, aboveBank, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.footstep_dust_snow",
	    .name = "Dust from the footsteps in the snow",
	    .facet = Facet::Movement,
	    .description = "Deep snow lies over the island and more snow falls. A tiger walks to and fro on the land near "
	                   "the camera. The camera looks low across the land, so the snow falls near and far.",
	    .expected = "The snow falls over all the land in view, to the far side of the view. Each footstep makes a "
	                "small cloud of dust with the colour of the land's light, not brown.",
	    .environment = {.hour = 12.0f,
	                    .clockRuns = false,
	                    .weather = Weather::Snow,
	                    .dispenserGrid = false,
	                    .cursor = glm::vec2(0.5f, 0.05f),
	                    .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 10.0f, -35.0f}, .look = {0.0f, 3.0f, 30.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {15.0f, 0.0f}, 1.0f), Go(Kind::WalkTo, 0, {-15.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	std::vector<ObjectSetup> wall;
	for (int i = -3; i <= 3; ++i)
	{
		wall.push_back({.type = TreeInfo::Conifer, .offset = {static_cast<float>(i) * 12.0f, 0.0f}, .scale = 1.5f});
	}
	wall.push_back({.type = FeatureInfo::FatPilarChalk, .offset = {-60.0f, 0.0f}});
	all.push_back({
	    .id = "movement.route_obstacles",
	    .name = "Route around obstacles",
	    .facet = Facet::Movement,
	    .description = "A small tiger walks back and forth past a row of tall conifers and a pillar of rock, with a cow "
	                   "standing still beyond them in its way.",
	    .expected = "Its route bends round the end of the row of trees, which are taller than it, and round the standing "
	                "cow rather than through them; the route shows over the land in the spawner.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 120.0f}, {0.0f, -100.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, -100.0f},
	                                 .facingDegrees = 180.0f,
	                                 .size = 0.6f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true},
	                  CreatureSetup {.label = "in the way",
	                                 .species = CreatureType::Cow,
	                                 .offset = {0.0f, 60.0f},
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = wall,
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 120.0f}, 1.5f), Go(Kind::WalkTo, 0, {0.0f, -100.0f}, 1.5f)},
	    .repeatFrom = 0,
	});

	// A pocket of tall conifers, open behind the tiger, with the destination beyond its far end, makes the search for
	// the route take long
	std::vector<ObjectSetup> pocket;
	for (int i = 0; i <= 40; ++i)
	{
		const auto along = -100.0f + (static_cast<float>(i) * 5.0f);
		pocket.push_back({.type = TreeInfo::Conifer, .offset = {along, 40.0f}, .scale = 1.5f});
		const auto back = 40.0f - (static_cast<float>(i) * 5.0f);
		pocket.push_back({.type = TreeInfo::Conifer, .offset = {-100.0f, back}, .scale = 1.5f});
		pocket.push_back({.type = TreeInfo::Conifer, .offset = {100.0f, back}, .scale = 1.5f});
	}
	all.push_back({
	    .id = "movement.point_at_route",
	    .name = "Point at the route while it waits for it",
	    .facet = Facet::Movement,
	    .description = "A small tiger stands in a pocket of tall conifers that is open behind it. It is told to walk to "
	                   "a point beyond the far end of the pocket. The search for its route takes long.",
	    .expected = "While it waits for its route, its first fidget is a point at the route ahead. It turns on the spot "
	                "first when the point is far round. The arm stays up at the point for about six seconds, then comes "
	                "down to the end of the animation. Then it waits again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, 0.0f},
	                                 .facingDegrees = 90.0f,
	                                 .size = 0.6f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = pocket,
	    .commands = {{.kind = Kind::HoldRouteSearch, .creature = 0, .delaySeconds = 1.0f, .value = 1},
	                 Go(Kind::WalkTo, 0, {0.0f, 100.0f}, 0.0f),
	                 {.kind = Kind::HoldRouteSearch, .creature = 0, .delaySeconds = 14.0f, .value = 0}},
	});

	// Two pockets of tall conifers side by side, each open behind its tiger: the two searches for the routes run at the
	// same time and share the search steps of each turn
	std::vector<ObjectSetup> twoPockets;
	for (const float side : {-130.0f, 130.0f})
	{
		for (const auto& tree : pocket)
		{
			auto moved = tree;
			moved.offset.x = (tree.offset.x * 0.5f) + side;
			twoPockets.push_back(moved);
		}
	}
	all.push_back({
	    .id = "movement.route_search_share",
	    .name = "Two creatures share the route search",
	    .facet = Facet::Movement,
	    .description = "Two small tigers stand in two pockets of tall conifers that are open behind them. Both are told "
	                   "to walk to a point beyond the far end of their pockets at the same time.",
	    .expected = "The two searches share the search steps of each turn, and both tigers find their routes after the "
	                "same number of turns. Both tigers start to walk in the same turn. Each tiger walks out of the open end "
	                "of its pocket and round to its point.",
	    .framing = {.shot = Shot::Overview, .include = {{-180.0f, 120.0f}, {180.0f, -60.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {-130.0f, 0.0f},
	                                 .facingDegrees = 90.0f,
	                                 .size = 0.6f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true},
	                  CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {130.0f, 0.0f},
	                                 .facingDegrees = 90.0f,
	                                 .size = 0.6f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = twoPockets,
	    .commands = {Go(Kind::WalkTo, 0, {-130.0f, 100.0f}, 1.0f, false), Go(Kind::WalkTo, 1, {130.0f, 100.0f}, 0.0f, false)},
	});

	all.push_back({
	    .id = "movement.walk_step_off",
	    .name = "Step off into the walk",
	    .facet = Facet::Movement,
	    .description = "A tiger walks to a far point on its one side, then to a far point on its other side, again "
	                   "and again.",
	    .expected = "At each start it steps off to the side into the walk without a turn on the spot first. The step "
	                "lasts as long as the step takes at its walking speed, and the walk then continues from the step.",
	    .framing = {.shot = Shot::Overview, .include = {{90.0f, 20.0f}, {-90.0f, -20.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {90.0f, 0.0f}, 0.5f), Go(Kind::WalkTo, 0, {-90.0f, 0.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.walk_turn_first",
	    .name = "Turn on the spot before a short walk",
	    .facet = Facet::Movement,
	    .description = "A tiger walks to a point a short way behind it, then back to where it started, again and again.",
	    .expected = "The point is nearer than its steps go and it does not face it, so it turns on the spot first. "
	                "Then it walks at once, from the start of its walk, and slows to stop at the point.",
	    .framing = {.shot = Shot::Overview, .include = {{25.0f, 25.0f}, {-25.0f, -25.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -8.0f}, 0.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 8.0f}, 0.5f), Go(Kind::WalkTo, 0, {0.0f, -8.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.walk_run_and_arrive",
	    .name = "Run, then slow down to arrive",
	    .facet = Facet::Movement,
	    .description = "A tiger runs a long way across the plain to a point, then walks back to where it started.",
	    .expected = "It speeds up to its run and its legs blend from the walk to the run. Near the end of the route it "
	                "slows down so that it stops at the point, and its legs blend back to the walk and the stand.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 120.0f}, {0.0f, -120.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -110.0f}, 180.0f)},
	    .commands = {Go(Kind::RunTo, 0, {0.0f, 110.0f}, 1.0f), Go(Kind::WalkTo, 0, {0.0f, -110.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.walk_round_rock",
	    .name = "Walk round a rock",
	    .facet = Facet::Movement,
	    .description = "A tiger runs to and fro past a pillar of rock that stands in the middle of its way.",
	    .expected = "Its route goes round the rock on an arc. It slows down before the arc to the speed that the arc "
	                "allows, and it does not stop and start again on the arc.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 90.0f}, {0.0f, -90.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -80.0f}, 180.0f)},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 0.0f}}},
	    .commands = {Go(Kind::RunTo, 0, {0.0f, 80.0f}, 1.0f), Go(Kind::RunTo, 0, {0.0f, -80.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.face_slight_turn",
	    .name = "Turn a little to face a point",
	    .facet = Facet::Movement,
	    .description = "A tiger turns to face a point a little to its left, then a point a little to its right, again "
	                   "and again.",
	    .expected = "Each time it turns on the spot to the point, also for a turn this small. It does not jump to "
	                "the new heading.",
	    .framing = {.shot = Shot::Overview, .include = {{20.0f, 20.0f}, {-20.0f, -20.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {Go(Kind::TurnToFace, 0, {6.0f, 40.0f}, 1.0f), Go(Kind::TurnToFace, 0, {-6.0f, 40.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.walk_join_new_route",
	    .name = "Join a new route while walking",
	    .facet = Facet::Movement,
	    .description = "A tiger walks towards a far point. On its way it is sent to a point off to one side, then back "
	                   "to where it started.",
	    .expected = "It finds the new route while it walks and joins it from where it is, with no stop to search. A "
	                "sharp turn of the new route stops it and it starts again from the turn.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 100.0f}, {-60.0f, -100.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 0.5f), Go(Kind::WalkTo, 0, {50.0f, 40.0f}, 3.0f, false),
	                 Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.step_join_new_route",
	    .name = "Join a new route during a step",
	    .facet = Facet::Movement,
	    .description = "A tiger steps off to the side towards a far point. During the step it is sent to a point "
	                   "ahead of it, then back to where it started.",
	    .expected = "It finds the new route during the step and joins it from the end of the step's move, so it does "
	                "not go back to where the step started.",
	    .framing = {.shot = Shot::Overview, .include = {{90.0f, 90.0f}, {-90.0f, -20.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {90.0f, 0.0f}, 0.5f), Go(Kind::WalkTo, 0, {60.0f, 80.0f}, 0.3f, false),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.stop_while_walking",
	    .name = "Stop while walking",
	    .facet = Facet::Movement,
	    .description = "A tiger walks towards a far point. On its way it is told to stop. Then it walks back and is told "
	                   "to stop again.",
	    .expected = "It does not stop dead. It walks on a short distance, slows down and stands. A faster creature walks "
	                "on further.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, 100.0f}, {-40.0f, -100.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 0.5f), Go(Kind::Stop, 0, {0.0f, 0.0f}, 4.0f, false),
	                 Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.0f), Go(Kind::Stop, 0, {0.0f, 0.0f}, 4.0f, false)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.turn_while_walking",
	    .name = "Turn while walking",
	    .facet = Facet::Movement,
	    .description = "A tiger walks towards a far point. On its way it is told to turn to face a point to its side, "
	                   "as the game's turn stages tell it. Then it walks back and turns again.",
	    .expected = "It does not stop dead to turn. It walks on a short distance and slows down to a stand. Then it "
	                "turns on the spot to face the point.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 100.0f}, {-60.0f, -100.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 0.5f), Go(Kind::TurnToFaceWhenStill, 0, {60.0f, 0.0f}, 4.0f, false),
	                 Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 2.0f), Go(Kind::TurnToFaceWhenStill, 0, {-60.0f, 0.0f}, 4.0f, false)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.stop_at_once_while_walking",
	    .name = "Stop at once while walking",
	    .facet = Facet::Movement,
	    .description = "A tiger walks towards a far point. On its way it stops at once, as a new body action or a "
	                   "placement stops it. Then it walks back and stops at once again.",
	    .expected = "It stops dead where it is, with no walk on. Compare with Stop while walking.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, 100.0f}, {-40.0f, -100.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 0.5f), Go(Kind::StopAtOnce, 0, {0.0f, 0.0f}, 4.0f, false),
	                 Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.0f), Go(Kind::StopAtOnce, 0, {0.0f, 0.0f}, 4.0f, false)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.turn_while_acting",
	    .name = "Turn while acting",
	    .facet = Facet::Movement,
	    .description = "A tiger that stands plays an action. While it plays, it is told to turn to face a point to its "
	                   "side, as the game's turn stages tell it. Then it plays the action again and turns back.",
	    .expected = "It does not turn while it plays the action. The action plays to its end. Then it turns on the spot "
	                "to face the point.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Impress, 0.5f, false),
	                 Go(Kind::TurnToFaceWhenStill, 0, {60.0f, 0.0f}, 0.3f, false),
	                 Play(Kind::PlayAction, 0, animations::k_Impress, 10.0f, false),
	                 Go(Kind::TurnToFaceWhenStill, 0, {0.0f, 60.0f}, 0.3f, false)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.freeze_while_walking",
	    .name = "Freeze while walking",
	    .facet = Facet::Movement,
	    .description = "A tiger walks towards a far point. On its way the player casts the freeze spell on it.",
	    .expected = "It does not stop dead. It walks on a short distance and slows to a stand, while the ice grows on it. "
	                "The freeze does not slow its walk to the stand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f)},
	    .miracles = {{.type = MagicType::CreatureSpellFreeze, .target = MiracleCast::Target::Creature, .delaySeconds = 4.0f}},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 0.5f)},
	});

	all.push_back({
	    .id = "appearance.body_spread",
	    .name = "Three new tigers of one species",
	    .facet = Facet::Appearance,
	    .description = "Three tigers are made one after the other. Each new creature draws its strength and its fatness "
	                   "from the random numbers of the game, as the game makes a creature.",
	    .expected = "The three tigers are not all the same. Each one is a little stronger or weaker, and a little fatter "
	                "or thinner, than the tiger of the species' table.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, -60.0f}, .look = {0.0f, 8.0f, 0.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 0.0f}, 180.0f), Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f),
	                  Posed(CreatureType::Tiger, {20.0f, 0.0f}, 180.0f)},
	});

	std::vector<ObjectSetup> copse;
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			const auto type = (x + y) % 2 == 0 ? TreeInfo::Bush : TreeInfo::Birch;
			copse.push_back({.type = type,
			                 .offset = {static_cast<float>(x) * 9.0f, static_cast<float>(y) * 9.0f},
			                 .yawDegrees = static_cast<float>((x * 40) + (y * 70))});
		}
	}
	all.push_back({
	    .id = "movement.trampling",
	    .name = "Walking through small trees",
	    .facet = Facet::Movement,
	    .description = "A full grown giant ape walks back and forth through a copse of bushes and birches much shorter "
	                   "than it.",
	    .expected = "It walks straight through the copse rather than round it, as big creatures walk through trees under "
	                "one and a half times their height; the trees stay standing.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 90.0f}, {0.0f, -90.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe,
	                                 .offset = {0.0f, -90.0f},
	                                 .facingDegrees = 180.0f,
	                                 .size = 2.0f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = copse,
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 1.0f), Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	const auto loop = Circle({0.0f, 0.0f}, 70.0f, 6);
	std::vector<Command> chase {{.kind = Kind::Follow, .creature = 1, .delaySeconds = 1.0f, .value = 0}};
	WalkRound(chase, 0, loop);
	chase.push_back(Go(Kind::FleeFrom, 2, {0.0f, 0.0f}, 0.0f, false));
	chase.push_back(Go(Kind::WalkTo, 2, {0.0f, 20.0f}, 3.0f, true));
	all.push_back({
	    .id = "movement.follow_flee",
	    .name = "Follow and flee",
	    .facet = Facet::Movement,
	    .description = "A cow walks round a circle with a tiger following it; a wolf in the middle keeps running away "
	                   "and coming back.",
	    .expected = "The tiger keeps within a short distance of the cow, catching up when it falls behind; the wolf runs "
	                "off from the middle, then walks back.",
	    .framing = {.shot = Shot::Overview, .include = {{80.0f, 80.0f}, {-80.0f, -80.0f}}},
	    .creatures = {Posed(CreatureType::Cow, loop.back(), 0.0f, "leader"),
	                  Posed(CreatureType::Tiger, loop.back() + glm::vec2(0.0f, -30.0f), 0.0f, "follower"),
	                  Posed(CreatureType::Wolf, {0.0f, 20.0f}, 0.0f, "flees")},
	    .commands = chase,
	    .repeatFrom = 1,
	});

	// A wolf runs away from a point beside it, then walks back, again and again
	all.push_back({
	    .id = "movement.run_away_point",
	    .name = "Run away point",
	    .facet = Facet::Movement,
	    .description = "A wolf stands still. Every few seconds it runs away from a point 15 units to its west, then walks "
	                   "back to where it started.",
	    .expected = "The wolf runs east by its species' distance and up to 39 units more, and stops at the corner of a map "
	                "cell. Then it walks back.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -40.0f}, {160.0f, 40.0f}}},
	    .creatures = {Posed(CreatureType::Wolf, {0.0f, 0.0f}, 0.0f, "runs")},
	    .commands = {Go(Kind::FleeFrom, 0, {-15.0f, 0.0f}, 1.0f, false), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 2.0f, true)},
	    .repeatFrom = 0,
	});

	// A tiger hurls a rock at a cow that walks across in front of it
	all.push_back({
	    .id = "objects.throw_ahead_of_a_walker",
	    .name = "Throwing ahead of a walker",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it. A cow walks east and west across the land 60 units to "
	                   "the north. A script tells the tiger to hurl the rock at the cow, again and again.",
	    .expected = "The tiger aims ahead of the cow, by the time that the rock takes to get there. The rock comes down "
	                "near where the cow has got to, not behind it. It aims at the full height of the cow.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-70.0f, -80.0f}, {70.0f, 10.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}), Posed(CreatureType::Cow, {-60.0f, -60.0f}, 90.0f, "walks")},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -10.0f}}},
	    .commands = {{.kind = Kind::DoAction,
	                  .delaySeconds = 1.0f,
	                  .waitUntilFree = true,
	                  .action = "Hurl",
	                  .with = 0,
	                  .atCreature = 1},
	                 Go(Kind::WalkTo, 1, {60.0f, -60.0f}, 0.5f, false),
	                 Go(Kind::WalkTo, 1, {-60.0f, -60.0f}, 12.0f, true)},
	    .repeatFrom = 0,
	});

	// A ring of fence round a pen, with two villagers walking to the far side of it, one aiming a little to each side
	constexpr glm::vec2 k_Pen {-60.0f, -40.0f};
	constexpr float k_PenRadius = 12.0f;
	constexpr int k_PenPieces = 24;
	std::vector<ObjectSetup> pen;
	for (const auto& point : Circle(k_Pen, k_PenRadius, k_PenPieces))
	{
		// Each piece runs round the ring
		const glm::vec2 out = point - k_Pen;
		const float yaw = glm::degrees(portable_math::Atan2(out.y, out.x)) + 90.0f;
		pen.push_back({.type = MobileStaticInfo::CeltFenceShort, .offset = point, .yawDegrees = yaw});
	}
	pen.push_back({.type = VillagerInfo::CelticForesterMale,
	               .offset = k_Pen + glm::vec2(-2.0f, -25.0f),
	               .walkTo = k_Pen + glm::vec2(4.0f, 25.0f),
	               .walkAfterSeconds = 2.0f,
	               .walkRepeatSeconds = 300.0f});
	pen.push_back({.type = VillagerInfo::CelticHousewifeFemale,
	               .offset = k_Pen + glm::vec2(2.0f, -25.0f),
	               .walkTo = k_Pen + glm::vec2(-4.0f, 25.0f),
	               .walkAfterSeconds = 2.0f,
	               .walkRepeatSeconds = 300.0f});
	all.push_back({
	    .id = "movement.villager_fence_pen",
	    .name = "Villagers walking round a fenced pen",
	    .facet = Facet::Movement,
	    .description = "A ring of fence round a pen stands between two villagers and the points they walk to beyond it, "
	                   "one a little to each side. Meeting the fence, each walker's way is tried both ways round, once "
	                   "a turn for one walker, and it is turned round if the other way gets there sooner. They walk "
	                   "there and back every five minutes; the scenario log gives each walk's game turns.",
	    .expected = "Each villager goes round the fence on the side nearer its goal and gets there in a few hundred "
	                "turns, rather than going round and round the pen. The inspector shows a WallHugLookahead on a "
	                "walker that has had its trial (turns to spare each way, and whether it was turned round).",
	    .framing = {.shot = Shot::Overview, .include = {k_Pen + glm::vec2(-30.0f, -30.0f), k_Pen + glm::vec2(30.0f, 30.0f)}},
	    .objects = pen,
	});
}

void testbed_scenarios::AddFootprints(std::vector<Scenario>& all)
{
	constexpr std::array<CreatureType, 3> k_Walkers {CreatureType::Tiger, CreatureType::Cow, CreatureType::GiantApe};
	std::vector<CreatureSetup> walkers;
	std::vector<Command> trails;
	for (size_t i = 0; i < k_Walkers.size(); ++i)
	{
		const auto y = 30.0f - (static_cast<float>(i) * 30.0f);
		walkers.push_back(Posed(k_Walkers.at(i), {110.0f, y}, 90.0f));
		trails.push_back(Go(Kind::WalkTo, i, {-110.0f, y}, i == 0 ? 1.0f : 0.0f, false));
	}
	for (size_t i = 0; i < k_Walkers.size(); ++i)
	{
		const auto y = 30.0f - (static_cast<float>(i) * 30.0f);
		trails.push_back(Go(Kind::WalkTo, i, {110.0f, y}, 0.0f, true));
	}
	all.push_back({
	    .id = "footprints.trail",
	    .name = "Footprints trail per species",
	    .facet = Facet::Footprints,
	    .description = "A tiger, a cow and a giant ape walk side by side across the testbed and back.",
	    .expected = "Each leaves its own prints where its feet fall: paws for the tiger, hooves for the cow and broad "
	                "feet for the ape, fading after a while.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = walkers,
	    .commands = trails,
	    .repeatFrom = 0,
	});

	std::vector<Command> circling;
	WalkRound(circling, 0, Circle({0.0f, 0.0f}, 50.0f, 8));
	all.push_back({
	    .id = "footprints.april_fools",
	    .name = "1 April smiley footprints",
	    .facet = Facet::Footprints,
	    .description = "As on the first of April, whatever the date: a tiger walks round in a circle.",
	    .expected = "Its footprints are smiley faces.",
	    .environment = {.aprilFools = true},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {50.0f, 0.0f}, 180.0f)},
	    .commands = circling,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "footprints.april_fools_date",
	    .name = "1 April by the computer's date",
	    .facet = Facet::Footprints,
	    .description = "The computer's clock is set to noon on 1 April 2026; nothing else is changed. A tiger walks round "
	                   "in a circle.",
	    .expected = "Its footprints are smiley faces, each the size of a tiger's own print.",
	    .environment = {.realDate = RealDate {.year = 2026, .month = 4, .day = 1, .hour = 12, .minute = 0}},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {50.0f, 0.0f}, 180.0f)},
	    .commands = circling,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "footprints.walk_and_run_close",
	    .name = "Footprints close up, walking and running",
	    .facet = Facet::Footprints,
	    .description = "The camera keeps close behind a tiger. It walks across the testbed, then runs back.",
	    .expected = "Each footstep, walking or running, lays one dark paw print under the lower foot, turned the way "
	                "the foot points, the left foot's print a mirror of the right. The prints fade in small steps and "
	                "go after about five seconds. The sea does not show them.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {110.0f, 0.0f}, 90.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {-110.0f, 0.0f}, 0.5f, true), Go(Kind::RunTo, 0, {110.0f, 0.0f}, 0.0f, true)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "footprints.buildings_overlap_and_go",
	    .name = "Building footprints overlap, and one goes",
	    .facet = Facet::Footprints,
	    .description = "Two Celtic huts stand so close that their footprints overlap. An Egyptian well stands to the "
	                   "side. After eight seconds the well is taken away.",
	    .expected = "The footprints show crisp texels of the land's own size. Where the two huts' footprints overlap, the "
	                "second is painted over the first. The well's footprint goes with the well.",
	    .framing = {.shot = Shot::Placed, .eye = {12.0f, 45.0f, -30.0f}, .look = {-6.0f, 0.0f, 2.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {7.0f, 5.0f}, .yawDegrees = 30.0f},
	                {.type = FeatureInfo::EgptWell, .offset = {-25.0f, 0.0f}}},
	    .commands = {OnObject(Kind::RemoveObject, 0, 2, 8.0f, false)},
	});
}
