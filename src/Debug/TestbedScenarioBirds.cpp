/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the land's birds: a flock of every kind the land scripts place, seagulls over the lake,
// the doves or bats about a temple, and many flocks at once for measuring their cost

#include <array>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The testbed's lake, north of the middle of the map
constexpr glm::vec2 k_Lake {0.0f, 220.0f};
/// A temple north of the camera, well short of the lake
constexpr glm::vec2 k_Temple {0.0f, 90.0f};

/// A dove killed where it is made, on the ground
ObjectSetup DeadDove(glm::vec2 offset)
{
	return {.type = AnimalInfo::Dove, .offset = offset, .life = 0.0f};
}
} // namespace

void testbed_scenarios::AddBirdScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "birds.every_kind",
	    .name = "Birds: a flock of every kind",
	    .facet = Facet::Animals,
	    .description = "Flocks of ten crows, doves, swallows, pigeons and bats across the land, and ten seagulls over the "
	                   "lake, each made as a land script makes it and wandering up to 40 m about its home.",
	    .expected = "Each flock's leader flies legs about its home, its followers catching up and then keeping a formation "
	                "behind it. Crows, swallows and seagulls fly about 40 m up, doves, pigeons and bats about 20 m, each "
	                "kind rising and falling within its own band, slowly, and banking gently into turns over two seconds. "
	                "Crows, doves, pigeons and seagulls each flap or glide, chosen afresh as they change course; swallows "
	                "flap, glide calmly or glide erratically; bats always flap. Every 10 seconds a leader gives up its leg "
	                "and heads off on a new one. No bird lands, none sleeps, and none reacts to the hand. All are lit by "
	                "the brightest of the land's light and cast no shadow. Crows caw and seagulls cry now and then as they "
	                "flap.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-160.0f, 0.0f}, {160.0f, 260.0f}}},
	    .birdFlocks = {{.kind = AnimalInfo::Crow, .offset = {-120.0f, 60.0f}, .reach = 40.0f},
	                   {.kind = AnimalInfo::Dove, .offset = {-40.0f, 60.0f}, .reach = 40.0f},
	                   {.kind = AnimalInfo::Swallow, .offset = {40.0f, 60.0f}, .reach = 40.0f},
	                   {.kind = AnimalInfo::Pigeon, .offset = {120.0f, 60.0f}, .reach = 40.0f},
	                   {.kind = AnimalInfo::Bat, .offset = {-100.0f, 160.0f}, .reach = 40.0f},
	                   {.kind = AnimalInfo::Seagull, .offset = k_Lake, .reach = 40.0f}},
	});

	all.push_back({
	    .id = "birds.close_up",
	    .name = "Birds: doves and crows close up",
	    .facet = Facet::Animals,
	    .description = "A flock of ten doves and one of ten crows wandering up to 25 m about a point 40 m north of a "
	                   "camera on the ground, looking up at them.",
	    .expected = "Each bird is its kind's model, sized for its age (the young smaller), posed by its own clip on its own "
	                "beat: a dove flaps or glides, a crow flaps or glides with its wings spread. They tilt half a radian "
	                "into their turns, easing in over two seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -5.0f}, .look = {0.0f, 25.0f, 40.0f}},
	    .birdFlocks = {{.kind = AnimalInfo::Dove, .offset = {-10.0f, 40.0f}, .reach = 25.0f},
	                   {.kind = AnimalInfo::Crow, .offset = {10.0f, 40.0f}, .reach = 25.0f}},
	});

	all.push_back({
	    .id = "birds.seagulls_lake",
	    .name = "Birds: seagulls over the lake",
	    .facet = Facet::Animals,
	    .description = "Twenty seagulls made over the lake, wandering up to 45 m about it, seen from its southern shore.",
	    .expected = "The seagulls fly legs over the lake and its shores about 40 m up, rising and sinking quicker than "
	                "other birds, flapping or gliding, and crying now and then while they flap. They keep over the land's "
	                "blocks but nothing keeps them to the water: the game gives seagulls no liking for the coast.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Lake - glm::vec2(60.0f), k_Lake + glm::vec2(60.0f)}},
	    .birdFlocks = {{.kind = AnimalInfo::Seagull, .offset = k_Lake, .count = 20, .reach = 45.0f}},
	});

	all.push_back({
	    .id = "birds.temple_doves",
	    .name = "Birds: doves about a good temple",
	    .facet = Facet::Animals,
	    .description = "A built temple of a wholly good player (alignment 1).",
	    .expected = "Every ten seconds one more dove is born over the temple, up to twenty. They wander up to 30 m about it, "
	                "following each other within 15 m, at the temple's height and 10 m over it.",
	    .environment = {.dispenserGrid = false, .playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2(50.0f), k_Temple + glm::vec2(50.0f)}},
	    .temples = {{.offset = k_Temple}},
	});

	all.push_back({
	    .id = "birds.temple_bats",
	    .name = "Birds: bats about an evil temple, then doves",
	    .facet = Facet::Animals,
	    .description = "A built temple of an evil player (alignment -0.5); after 60 seconds the player turns good (0.3).",
	    .expected = "Every ten seconds one more bat is born over the temple, up to ten. Once the player turns good, all the "
	                "bats vanish together the next time the flock is seen to, within ten seconds, and doves are born one every "
	                "ten seconds, up to six.",
	    .environment = {.dispenserGrid = false, .playerAlignment = -0.5f},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2(50.0f), k_Temple + glm::vec2(50.0f)}},
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::SetAlignment, .delaySeconds = 60.0f, .alignment = 0.3f}},
	});

	using Kind = Command::Kind;
	all.push_back({
	    .id = "birds.dead_doves",
	    .name = "Birds: dead doves, picked up and gone in smoke",
	    .facet = Facet::Animals,
	    .description = "Two doves killed on the ground before a tiger. After a second the tiger walks to the nearer one, "
	                   "picks it up and puts it down again.",
	    .expected = "Both doves lie dead in their dead clip. The tiger can pick a dove up because it lies under a metre "
	                "above the land (it couldn't reach a flying one). Each dead dove lies 60 seconds (the one put down "
	                "lies its time afresh from landing), then goes in a faint grey puff of smoke that drifts slowly off "
	                "and fades away over three seconds, half the dove's height out from where it lay. No sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -12.0f}, .look = {0.0f, 0.0f, 18.0f}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = {-6.0f, 0.0f},
	                   .facingDegrees = 180.0f,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                   .hold = true,
	                   .pauseMind = true}},
	    .objects = {DeadDove({-3.0f, 14.0f}), DeadDove({4.0f, 18.0f})},
	    .commands = {{.kind = Kind::PickUp, .creature = 0, .delaySeconds = 1.0f, .waitUntilFree = true, .object = 0},
	                 {.kind = Kind::PutDown, .creature = 0, .delaySeconds = 1.0f, .waitUntilFree = true}},
	});

	// Many flocks across the land, more birds than any land of the game's places
	std::vector<BirdFlockSetup> flocks;
	constexpr std::array k_Kinds {AnimalInfo::Crow,   AnimalInfo::Dove, AnimalInfo::Swallow,
	                              AnimalInfo::Pigeon, AnimalInfo::Bat,  AnimalInfo::Seagull};
	constexpr int k_Columns = 6;
	constexpr int k_Rows = 5;
	for (int row = 0; row < k_Rows; ++row)
	{
		for (int column = 0; column < k_Columns; ++column)
		{
			flocks.push_back(
			    {.kind = k_Kinds.at(static_cast<size_t>((row + column) % k_Columns)),
			     .offset = {static_cast<float>(column - (k_Columns / 2)) * 90.0f, 40.0f + (static_cast<float>(row) * 90.0f)},
			     .count = 20});
		}
	}
	all.push_back({
	    .id = "birds.many_flocks",
	    .name = "Birds: thirty flocks at once",
	    .facet = Facet::Animals,
	    .description = "Thirty flocks of twenty birds of every kind, 600 birds, spread over the land north of the camera, "
	                   "each wandering as far as its kind does (80 m).",
	    .expected = "All 600 birds fly as the land's birds do, every one posed by its own clip, and the game keeps above "
	                "100 frames a second.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-300.0f, 0.0f}, {300.0f, 480.0f}}},
	    .birdFlocks = flocks,
	});
}
