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
#include <numbers>

#include <SDL_scancode.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

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
#include "ECS/IncomingMessageRules.h"
#include "Input/BindableActions.h"
#include "Particles/ParticleTypes.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"
#include "Sound/SoundId.h"
#include "TestbedDispenserGrid.h"

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
using Kind = Command::Kind;
using creature_desires::Desire;
namespace animations = creature_layers::animations;

/// How far apart the creatures of a lineup stand
constexpr float k_RowSpacing = 32.0f;
/// The highest hour of the day
constexpr float k_HoursPerDay = 24.0f;
/// Offsets further from the middle of the map than this would be off it
constexpr float k_MaxOffset = 2400.0f;
/// A leash order can name a place off the map, as the player can, but no further than this from the middle
constexpr float k_MaxOrderOffset = 2.0f * k_MaxOffset;
/// A command that waits for its creature to be free first gives the last command this long to get it going
constexpr float k_SettleSeconds = 0.5f;
/// The last stage of growing up, and how many skills, miracles and deeds to copy the game's tables have
constexpr size_t k_LastPhase = 13;
constexpr size_t k_Skills = 6;
constexpr size_t k_Miracles = 42;
constexpr size_t k_Deeds = 46;
/// The game's spell seeds, and its last gesture (the square wave)
constexpr size_t k_SeedCount = 30;
constexpr size_t k_LastGesture = 23;
/// The seed of every benchmark's crowd, so that runs lay it out the same
constexpr uint32_t k_BenchmarkSeed = 2026;
/// The testbed's lake, from the middle of the map: the middle of its open water, half its width, and the width of the
/// shallows round it
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
constexpr glm::vec2 k_LakeHalf = flat_land::k_LakeHalfExtent;
constexpr float k_ShallowsWidth = 20.0f;
/// The near and far shallows of the lake, as the testbed's camera looks north across it
constexpr float k_NearShallows = k_Lake.y - k_LakeHalf.y - (k_ShallowsWidth * 0.5f);
constexpr float k_FarShallows = k_Lake.y + k_LakeHalf.y + (k_ShallowsWidth * 0.5f);

/// The lake's open water, for the overview to keep in view
std::vector<glm::vec2> LakeInView()
{
	return {k_Lake - k_LakeHalf, k_Lake + k_LakeHalf};
}

Command Go(Kind kind, size_t creature, glm::vec2 point, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .point = point};
}

Command Play(Kind kind, size_t creature, size_t animation, float delay, bool wait)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .value = animation};
}

Command Act(Kind kind, size_t creature, float delay = 0.0f, bool wait = false)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait};
}

Command AtHour(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}

/// A command on one of the scenario's objects: picking it up, knocking it down, tying the leash to it
Command OnObject(Kind kind, size_t creature, size_t object, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .object = object};
}

Command Stroke(size_t creature, creature_feedback::BodyPart part, float delay)
{
	return {.kind = Kind::HandStroke,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .bodyPart = static_cast<size_t>(part)};
}

Command Slap(size_t creature, float height, bool gentle, bool sweepsRight, float delay)
{
	return {.kind = Kind::HandSlap,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .slapHeight = height,
	        .gentle = gentle,
	        .sweepsRight = sweepsRight};
}

/// Pulling the face the creature's mind would for a feeling
Command Feel(size_t creature, creature_face::Cue cue, float delay)
{
	return {.kind = Kind::ShowFeeling, .creature = creature, .delaySeconds = delay, .value = static_cast<size_t>(cue)};
}

Command Leash(size_t creature, LeashType type, float delay)
{
	return {.kind = Kind::PutOnLeash, .creature = creature, .delaySeconds = delay, .leash = type};
}

/// A body wanting nothing: full, rested, watered, comfortable
NeedOverrides Content()
{
	return {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f};
}

/// Every desire gone but one, which is as strong as it gets
std::vector<DesireOverride> OnlyDesire(Desire wanted)
{
	std::vector<DesireOverride> overrides;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		overrides.push_back({.desire = desire, .fraction = desire == wanted ? 1.0f : 0.0f});
	}
	return overrides;
}

/// A creature kept content, so that its needs stay out of what the scenario shows
CreatureSetup Content(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	return {.label = label, .species = species, .offset = offset, .facingDegrees = facing, .needs = Content(), .hold = true};
}

/// A content creature whose mind is paused, so that it only does what the scenario tells it: its idle mind would
/// otherwise wander off or sit down in the middle of a walk
CreatureSetup Posed(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	auto creature = Content(species, offset, facing, label);
	creature.pauseMind = true;
	return creature;
}

/// How far east of the middle the item of a row stands, the first furthest west so that the testbed's camera,
/// looking north, sees the row in order from left to right
float RowX(size_t index, size_t count, float spacing = k_RowSpacing)
{
	return (static_cast<float>(index) - ((static_cast<float>(count) - 1.0f) * 0.5f)) * spacing;
}

/// Walking round the points in turn, each once it has arrived at the last
void WalkRound(std::vector<Command>& commands, size_t creature, std::span<const glm::vec2> points, bool run = false)
{
	for (const auto& point : points)
	{
		commands.push_back(Go(run ? Kind::RunTo : Kind::WalkTo, creature, point, 0.0f, true));
	}
}

std::vector<glm::vec2> Circle(glm::vec2 centre, float radius, size_t points)
{
	std::vector<glm::vec2> circle;
	for (size_t i = 0; i < points; ++i)
	{
		const auto angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(points);
		circle.emplace_back(centre + (radius * glm::vec2(portable_math::Cos(angle), portable_math::Sin(angle))));
	}
	return circle;
}

void AddIdle(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "idle.fidget",
	    .name = "Idle fidgeting",
	    .facet = Facet::Idle,
	    .description = "A content tiger in the middle of the testbed with nothing to do and nothing it needs.",
	    .expected = "It stands breathing and looks about, pulls faces now and then, sits down for a while and gets up "
	                "again, and every so often wanders off a little way to sit somewhere else.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	std::vector<Command> hangAround;
	const std::array<glm::vec2, 3> spots {glm::vec2 {50.0f, 40.0f}, glm::vec2 {-45.0f, 55.0f}, glm::vec2 {0.0f, -10.0f}};
	for (const auto& spot : spots)
	{
		hangAround.push_back(Go(Kind::WalkTo, 0, spot, 0.5f, true));
		hangAround.push_back(Act(Kind::SitDown, 0, 0.5f, true));
		hangAround.push_back(Act(Kind::StandUp, 0, 6.0f));
	}
	all.push_back({
	    .id = "idle.hang_around",
	    .name = "Hang around",
	    .facet = Facet::Idle,
	    .description = "A tiger walks off a little way and sits there, then on to another spot, as its idle mind does "
	                   "when it hangs around; here its mind is paused and it is told to, so it happens every time.",
	    .expected = "It walks to each of three spots near the middle, sits down there for a while, gets up and goes on "
	                "to the next.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 180.0f)},
	    .commands = hangAround,
	    .repeatFrom = 0,
	});

	// A script puts the tiger somewhere else while it walks
	const std::vector<Command> placed {
	    Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 1.0f, false),
	    Go(Kind::ScriptPlace, 0, {-40.0f, 0.0f}, 3.0f, false),
	    Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 4.0f, false),
	    Go(Kind::ScriptPlace, 0, {40.0f, 0.0f}, 3.0f, false),
	};
	all.push_back({
	    .id = "movement.script_place_stops_walk",
	    .name = "Script place stops the walk",
	    .facet = Facet::Movement,
	    .description = "A tiger walks north. After three seconds, a script puts it to one side at once, as SET_POSITION "
	                   "does.",
	    .expected = "It shows at the new place at once and stands there. It does not continue its walk, and its head "
	                "looks straight ahead. Then it walks north again, and the script puts it on the other side.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 0.0f)},
	    .commands = placed,
	    .repeatFrom = 0,
	});
}

void AddExpressions(std::vector<Scenario>& all)
{
	// The desires a creature shows with an emote but doesn't act on by itself
	constexpr std::array<Desire, 10> k_Shown {
	    Desire::Impress,   Desire::Compassion, Desire::Anger,
	    Desire::Play,      Desire::Fear,       Desire::Curiosity,
	    Desire::BeFriends, Desire::Sadness,    Desire::AttractAttention,
	    Desire::GetWarmer,
	};
	std::vector<CreatureSetup> emoters;
	for (size_t i = 0; i < k_Shown.size(); ++i)
	{
		const auto row = i / 5;
		auto creature = Content(CreatureType::Tiger, {RowX(i % 5, 5, 36.0f), static_cast<float>(row) * 40.0f}, 0.0f,
		                        creature_desires::Name(k_Shown.at(i)));
		creature.desires = OnlyDesire(k_Shown.at(i));
		emoters.push_back(creature);
	}
	all.push_back({
	    .id = "expressions.desire_emotes",
	    .name = "Desire emotes",
	    .facet = Facet::Expressions,
	    .description = "Ten tigers in two rows, each with one desire as strong as it gets and every other desire gone: "
	                   "impress, compassion, anger, play, fear (front row, left to right), curiosity, be friends, "
	                   "sadness, attract attention and get warmer (back row).",
	    .expected = "Every few seconds each shows its desire with its emote: summoning, feeling nice, taunting, feeling "
	                "playful, frightened, confused, happy, sad, waving and cold. The readout names each one's desire.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = emoters,
	});

	std::vector<Command> faces;
	for (size_t i = 0; i < animations::k_IdleFaceCount; ++i)
	{
		faces.push_back(Play(Kind::PullFace, 0, animations::k_FirstFace + i, 2.5f, false));
	}
	for (size_t i = 0; i < animations::k_GestureCount; ++i)
	{
		faces.push_back(Play(Kind::PlayGesture, 0, animations::k_FirstGesture + i, 3.0f, false));
	}
	all.push_back({
	    .id = "expressions.faces_gestures",
	    .name = "Faces and gestures gallery",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused, close up, pulls each of its ten faces in turn, then plays each "
	                   "gesture on top of its body.",
	    .expected =
	        "Smile, grimace, growl, scared, sad, amazed, puzzled, laugh, ooh and aah, each held then easing out of the last, "
	        "then a nod, a shake of the head, a yawn, thirst, squirting water and talking.",
	    .framing = {.shot = Shot::Head, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = faces,
	    .repeatFrom = 0,
	});

	// The creature draws gestures in the air: the spiral of the game with villagers, then the circle, the star and the
	// heart, each a few seconds after the last
	std::vector<Command> drawn;
	for (const auto gesture : {GestureType::Spiral, GestureType::Circle, GestureType::Star, GestureType::Heart})
	{
		drawn.push_back(Play(Kind::CreatureDrawGesture, 0, static_cast<size_t>(gesture), 7.0f, true));
	}
	all.push_back({
	    .id = "expressions.draw_gesture",
	    .name = "Gestures drawn in the air",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused, seen from the front, draws four gestures in the air one after the "
	                   "other: the spiral, the circle, the star and the heart. In the game only the data can start a "
	                   "drawing (a miracle with a gesture, or a dance move), and no shipped data does.",
	    .expected = "For each gesture the tiger raises an arm and points in front of its head. The point moves along the "
	                "gesture's shape on an upright square to the arm's side, from the top, and a glowing chain in the "
	                "player's colour follows the hand. A gesture as long as the circle takes three seconds. At the end of "
	                "the shape the arm holds still for most of a second, then comes down, and the chain fades.",
	    .framing = {.shot = Shot::Head, .distance = 3.0f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = drawn,
	    .repeatFrom = 0,
	});

	// Each shows one feeling in its face as the game's minds do: as its activity starts and again every four seconds
	struct Feeling
	{
		std::string_view label;
		std::optional<creature_face::Cue> cue;
	};
	const std::array<Feeling, 7> k_Feelings {{
	    {"stroked", std::nullopt},
	    {"slapped", std::nullopt},
	    {"frightened", creature_face::Cue::Fear},
	    {"angry", creature_face::Cue::Anger},
	    {"exhausted", creature_face::Cue::Exhausted},
	    {"amazed", creature_face::Cue::Amazed},
	    {"curious", creature_face::Cue::Curiosity},
	}};
	std::vector<CreatureSetup> feelers;
	std::vector<Command> feelings {Act(Kind::Stroke, 0, 0.5f), Act(Kind::Slap, 1, 0.1f)};
	for (size_t i = 0; i < k_Feelings.size(); ++i)
	{
		auto creature = Content(CreatureType::Tiger, {RowX(i, k_Feelings.size(), 24.0f), 0.0f}, 0.0f, k_Feelings.at(i).label);
		creature.pauseMind = k_Feelings.at(i).cue.has_value();
		feelers.push_back(creature);
		if (const auto cue = k_Feelings.at(i).cue)
		{
			feelings.push_back(Feel(i, *cue, 0.1f));
		}
	}
	feelings.push_back(Act(Kind::Stop, 0, creature_mind::k_FaceRepeatSeconds));
	all.push_back({
	    .id = "expressions.faces_emotions",
	    .name = "Faces and emotions",
	    .facet = Facet::Expressions,
	    .description = "Seven tigers in a row, each feeling something different: one stroked and one slapped over and "
	                   "over with their minds running, then (paused) frightened, angry, exhausted, amazed and curious, "
	                   "each pulling its feeling's face every four seconds as its mind would.",
	    .expected = "The stroked tiger shows it is happy with a smile, the slapped one sad with a sad face. The "
	                "frightened one is scared or goes ooh, the angry one growls or grimaces, the exhausted one grimaces, "
	                "the amazed one is amazed, the curious one puzzled, amazed or aah. Each face plays once and holds, "
	                "then eases back; no mouth opens and closes over and over. The spawner's mind panel names each "
	                "face's reason.",
	    .framing = {.shot = Shot::Overview, .distance = 0.8f},
	    .creatures = feelers,
	    .commands = feelings,
	    .repeatFrom = 0,
	});

	std::vector<Command> actions;
	for (size_t i = 0; i < animations::k_ActionCount; ++i)
	{
		actions.push_back(Play(Kind::PlayAction, 0, animations::k_FirstAction + i, 0.5f, true));
	}
	all.push_back({
	    .id = "expressions.actions",
	    .name = "Action gallery",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused plays every one of its actions in turn, from summoning to the "
	                   "friendly wave and pick me, each once the last has finished.",
	    .expected = "Each action plays through once and the body goes back to standing between them; the spawner's mind "
	                "panel names the one playing.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = actions,
	    .repeatFrom = 0,
	});

	// The guide plays static action 47 before each miracle it makes for the player
	auto staticPlay = [](float plays, float delay) {
		return Command {.kind = Kind::ScriptPlay, .delaySeconds = delay, .waitUntilFree = true, .value = 47, .amount = plays};
	};
	all.push_back({
	    .id = "expressions.script_static",
	    .name = "A script's static action",
	    .facet = Facet::Expressions,
	    .description = "A tiger is given static action 47 by a script, as the guide is before it makes a miracle "
	                   "(\"Guide play 47 loop 1\"): once, then twice in a row.",
	    .expected = "It plays the start (46), loops 47 until ten seconds after the start began, then the end (48), "
	                "left to right half the time; the second time it does it all twice, then stands. No \"not "
	                "implemented\" in the log.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true}},
	    .commands = {staticPlay(1.0f, 1.0f), staticPlay(2.0f, 14.0f)},
	    .repeatFrom = 0,
	});
}

void AddSenses(std::vector<Scenario>& all)
{
	const std::array<glm::vec2, 4> loop {glm::vec2 {45.0f, 45.0f}, glm::vec2 {-45.0f, 45.0f}, glm::vec2 {-45.0f, -35.0f},
	                                     glm::vec2 {45.0f, -35.0f}};
	std::vector<Command> commands {Act(Kind::SitDown, 0, 1.0f)};
	WalkRound(commands, 1, loop);
	all.push_back({
	    .id = "senses.watch_walker",
	    .name = "Look-around: watching a walker",
	    .facet = Facet::Senses,
	    .description = "A tiger sits in the middle while a cow walks round and round it.",
	    .expected = "The tiger's head and eyes turn to follow the cow as it goes round, looking away to something else "
	                "now and then and back again; once it loses interest it watches something new.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "watcher"),
	                  Posed(CreatureType::Cow, loop.at(3), 180.0f, "walker")},
	    .commands = commands,
	    .repeatFrom = 0,
	});
}

void AddNeeds(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "needs.thirst",
	    .name = "Thirsty creature finds water",
	    .facet = Facet::Needs,
	    .description = "A parched tiger starts north of the middle, short of the testbed's lake, with its desire for "
	                   "water as strong as it gets.",
	    .expected = "It walks down the bank to the nearest edge of the lake, bends down and drinks; its thirst clears "
	                "and the desire for water is held back for a while.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 60.0f},
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.95f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Water, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.forced_walk_kept_at_home",
	    .name = "Forced walk out of the area that holds a creature",
	    .facet = Facet::Needs,
	    .description = "A tiger starts north of the middle, short of the testbed's lake. It is kept within 20 units of "
	                   "where it stands, as a young creature is kept near its home. After one second its player orders "
	                   "it to look at its reflection, whose walk goes to the shore of the lake.",
	    .expected = "The forced plan fails as it starts, because its walk leaves the area. The tiger does not go to the "
	                "lake and stays near where it stands. The log tells that it cannot walk out of its area.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 60.0f},
	    }},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 20.0f},
	                 {.kind = Kind::ForceAction, .delaySeconds = 1.0f, .action = "LookAtReflection"}},
	});

	all.push_back({
	    .id = "needs.thirst_kept_at_home",
	    .name = "Thirsty creature kept far from water",
	    .facet = Facet::Needs,
	    .description = "A parched tiger starts north of the middle, short of the testbed's lake, with its desire for "
	                   "water as strong as it gets. It is kept within 20 units of where it stands, as a young creature "
	                   "is kept near its home.",
	    .expected = "It does not walk to the lake. Each plan whose walk leaves the area fails as it starts, and the "
	                "creature does not want that action again while the area holds it. It stays near where it stands.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 60.0f},
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.95f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Water, .fraction = 1.0f}},
	    }},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 20.0f}},
	});

	all.push_back({
	    .id = "needs.on_fire_drinks",
	    .name = "A creature on fire goes to the sea",
	    .facet = Facet::Needs,
	    .description = "A player's tiger stands north of the middle, short of the testbed's lake, and catches fire. It "
	                   "does not know the water miracle.",
	    .expected = "At once it stops what it does and walks to the water point at the lake, then bends down and drinks. "
	                "A player's creature walks, because it goes back to its walk speed as it stops what it did.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "needs.on_fire_drinks_neutral",
	    .name = "A creature of no player on fire runs to the sea",
	    .facet = Facet::Needs,
	    .description = "A tiger of no player stands north of the middle, short of the testbed's lake, and catches fire.",
	    .expected = "At once it stops what it does and runs to the water point at the lake, then bends down and drinks.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .owner = PlayerNames::NEUTRAL}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "needs.on_fire_casts_water",
	    .name = "A creature on fire casts water on itself",
	    .facet = Facet::Needs,
	    .description = "A player's tiger that knows the water miracle well stands north of the middle, and catches fire.",
	    .expected = "At once it stops what it does, finds a clear place near it and casts the water miracle on itself. "
	                "The fire on it goes out.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "mind.cast_sides",
	    .name = "Creatures cast from the pose on the two sides",
	    .facet = Facet::Mind,
	    .description = "Four player's tigers in a row know the water miracle well. Each one catches fire.",
	    .expected = "Each tiger casts the water miracle on itself from the cast pose. Some tigers cast with the right "
	                "arm and some with the left arm, about one time in two.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {-45.0f, 60.0f}},
	                  CreatureSetup {.species = CreatureType::Tiger, .offset = {-15.0f, 60.0f}},
	                  CreatureSetup {.species = CreatureType::Tiger, .offset = {15.0f, 60.0f}},
	                  CreatureSetup {.species = CreatureType::Tiger, .offset = {45.0f, 60.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::KnowMiracle, .creature = 1, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::KnowMiracle, .creature = 2, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::KnowMiracle, .creature = 3, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f},
	         {.kind = Kind::SetAlight, .creature = 1, .delaySeconds = 1.0f},
	         {.kind = Kind::SetAlight, .creature = 2, .delaySeconds = 1.0f},
	         {.kind = Kind::SetAlight, .creature = 3, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "mind.sleep_sides",
	    .name = "Creatures sleep on the two sides",
	    .facet = Facet::Mind,
	    .description = "Four worn out tigers in a row at midday, each with a strong desire to sleep, their bodies' time "
	                   "ten times as fast.",
	    .expected = "Each tiger lies down and sleeps until it is rested. Some tigers lie on the right side and some on "
	                "the left side, about one time in two.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {-45.0f, 60.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	                                 .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}},
	                  CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {-15.0f, 60.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	                                 .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}},
	                  CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {15.0f, 60.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	                                 .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}},
	                  CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {45.0f, 60.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	                                 .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}}},
	});

	all.push_back({
	    .id = "mind.faint_cuts",
	    .name = "A creature that falls dead stops what it does at once",
	    .facet = Facet::Mind,
	    .description = "Two player's tigers. The left tiger walks away to the west and the right tiger plays the impress "
	                   "action. One second later a script tells each tiger to fall dead.",
	    .expected = "Each tiger falls down at once. The left tiger stops its walk where it is, and the right tiger stops "
	                "its action part of the way through. Neither tiger finishes what it did first.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {-20.0f, 60.0f}},
	                  CreatureSetup {.species = CreatureType::Tiger, .offset = {20.0f, 60.0f}}},
	    .commands = {Go(Kind::WalkTo, 0, {-160.0f, 60.0f}, 0.5f, false),
	                 Play(Kind::PlayAction, 1, animations::k_Impress, 0.0f, false),
	                 {.kind = Kind::DoAction, .creature = 0, .delaySeconds = 1.0f, .action = "DeadForever", .atCreature = 0},
	                 {.kind = Kind::DoAction, .creature = 1, .delaySeconds = 0.0f, .action = "DeadForever", .atCreature = 1}},
	});

	all.push_back({
	    .id = "mind.faint_holding",
	    .name = "A creature that falls dead while it lets go of a thing keeps the thing",
	    .facet = Facet::Mind,
	    .description = "A player's tiger picks up a rock. Half a second after it holds the rock, a script tells it to fall "
	                   "dead.",
	    .expected = "The tiger starts to let go of the rock. On the next turn it falls down at once, before the rock "
	                "leaves its hand, and it lies out cold with the rock still in its hand.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {4.0f, 54.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 {.kind = Kind::DoAction,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .waitUntilFree = true,
	                  .action = "DeadForever",
	                  .atCreature = 0}},
	});

	all.push_back({
	    .id = "mind.die_for_good",
	    .name = "A creature that a script tells to die for good falls and lies dead",
	    .facet = Facet::Mind,
	    .description = "A player's tiger. A script tells it to cower on the spot, as the death of a rival god's creature "
	                   "on the second land does. Four seconds later the script tells it to die for good.",
	    .expected = "The tiger cowers, then falls down at once and lies still with its eyes closed. It does not get up "
	                "again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .commands =
	        {{.kind = Kind::DoAction, .creature = 0, .delaySeconds = 0.5f, .action = "BeFrightenedOnTheSpot", .atCreature = 0},
	         {.kind = Kind::DoAction, .creature = 0, .delaySeconds = 4.0f, .action = "DiePermanently", .atCreature = 0}},
	});

	all.push_back({
	    .id = "combat.knockout_holding",
	    .name = "A creature knocked out while it holds a thing keeps the thing",
	    .facet = Facet::Combat,
	    .description = "A player's tiger picks up a rock. Half a second after it holds the rock, it is knocked out as a "
	                   "fight's loser is.",
	    .expected = "The tiger starts to let go of the rock. On the next turn it falls down at once, before the rock "
	                "leaves its hand, and it lies out cold with the rock still in its hand.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {4.0f, 54.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::KnockOut, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "needs.badly_hurt",
	    .name = "A badly hurt creature wants to restore its health",
	    .facet = Facet::Needs,
	    .description = "A player's tiger with three tenths of its life stands north of the middle.",
	    .expected = "Its desire to restore its health becomes the strongest for ten seconds, and most other desires stay "
	                "down. Each time that it stops what it does with low life, the log shows that it is badly hurt "
	                "again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .needs = {.life = 0.3f}}},
	});
	all.push_back({
	    .id = "needs.town_fire_water",
	    .name = "A creature puts out a fire in the nearest town",
	    .facet = Facet::Needs,
	    .description = "A kind player's tiger that knows the water miracle well stands near a hut of the town. The hut "
	                   "catches fire.",
	    .expected = "At once it stops what it does, walks near the hut and casts the water miracle on it. The fire goes "
	                "out.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {40.0f, 44.0f},
	        .desires = {{.desire = Desire::Compassion, .fraction = 1.0f}, {.desire = Desire::Anger, .fraction = 0.0f}}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {66.0f, 44.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
	});

	all.push_back({
	    .id = "needs.hunger",
	    .name = "Hungry creature finds food",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger with two pots of food on the land in front of it, each half a meal.",
	    .expected = "It walks up to the nearer food and eats it, then, if still hungry, the other; its energy fills up "
	                "and the hunger desire falls.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -50.0f}, .amount = 400},
	                {.type = PotInfo::FoodPot, .offset = {40.0f, -90.0f}, .amount = 400}},
	});

	all.push_back({
	    .id = "needs.fart_by_hut",
	    .name = "Creature poos or farts by a hut",
	    .facet = Facet::Needs,
	    .description = "A tiger that needs a poo, with a hut on the land in front of it. Its opinions choose a poo or "
	                   "a fart; run it again until it farts.",
	    .expected = "For a poo or a fart, it walks up to the hut and looks at it, turns its back to the hut, then "
	                "squats there, not where it stood.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.9f},
	        .desires = {{.desire = Desire::Poo, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {30.0f, -60.0f}}},
	});

	all.push_back({
	    .id = "mind.fart_again_when_stroked",
	    .name = "A creature stroked for a fart by a hut farts again by a hut",
	    .facet = Facet::Mind,
	    .description = "A tiger that needs a poo, with two huts on the land in front of it. When it has done its poo or "
	                   "fart by a hut, the hand strokes it and lets go. Its opinions choose a poo or a fart; run it again "
	                   "until it farts.",
	    .expected = "After a fart, the log shows \"does its action again: Fart on N\", N a hut, and the tiger walks to "
	                "that hut and farts there again.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.9f},
	        .desires = {{.desire = Desire::Poo, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {30.0f, -60.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-40.0f, -70.0f}}},
	    // The first command only waits while the tiger starts to walk; the stroke then waits until the tiger is free
	    .commands = {Act(Kind::HandLetGo, 0, 4.0f),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 0.5f,
	                  .waitUntilFree = true,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 1.0f)},
	});

	all.push_back({
	    .id = "needs.eats_animals",
	    .name = "Hungry creature eats an animal",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger with a pig and a cow on the land in front of it, and no other food.",
	    .expected = "It walks up to an animal, picks it up and eats it; its energy fills up and the hunger desire falls.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -40.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {30.0f, -60.0f}, .living = true}},
	});

	all.push_back({
	    .id = "needs.hungry_kept_at_home",
	    .name = "Starving creature kept at home",
	    .facet = Facet::Needs,
	    .description = "A starving tiger with its hunger desire as strong as it gets. A script keeps it within 10 units of "
	                   "where it stands. A pig stands inside the area and a cow stands far outside it.",
	    .expected = "It chooses only food that it believes is inside the area. It walks to the pig, picks it up and eats "
	                "it. It does not walk to the cow.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.05f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -6.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {30.0f, -60.0f}, .living = true}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 10.0f}},
	});

	all.push_back({
	    .id = "needs.toy_in_creche",
	    .name = "Playful creature and a toy inside a creche",
	    .facet = Facet::Needs,
	    .description = "A young tiger, playful and hungry, at the stage of the first land's food lesson. A Norse creche "
	                   "stands in front of it, with a cuddly toy at the middle of the creche. The creche, its turn and "
	                   "the toy are as on the first land. A pig stands to the side.",
	    .expected = "It chooses to stroke the toy. It walks to the edge of the creche, out of reach of the toy, and stays "
	                "there. Its walk ends at once on each turn. When it has not moved for a span of 5 seconds, it gives up the "
	                "toy, and then it eats the pig. The game does not give up and the creature starves until it faints "
	                "(vanilla-bugs row 389, deviation 90).",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -60.0f}, {20.0f, 10.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .size = 0.25f,
	        .phase = 9u,
	        .needs = {.energy = 0.3f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Play, .fraction = 1.0f}, {.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = AbodeInfo::NorseCreche, .offset = {0.0f, -40.0f}, .yawDegrees = 281.15f},
	                {.type = MobileStaticInfo::ToyCuddly, .offset = {1.35f, -39.21f}, .scale = 0.381f},
	                {.type = AnimalInfo::Pig, .offset = {40.0f, -10.0f}, .living = true}},
	});

	all.push_back({
	    .id = "needs.fresh_eats_villager",
	    .name = "Hungry new creature eats a villager",
	    .facet = Facet::Needs,
	    .description = "A new hungry tiger that has learnt nothing, with two villagers of no player in front of it, and "
	                   "no other food. Every new creature knows that an enemy villager is good to eat.",
	    .expected = "It walks up to a villager, picks it up and eats it; its energy fills up and the hunger desire falls. "
	                "Its first example for hunger is an enemy villager with feedback 0.8.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -60.0f}, {20.0f, 10.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -40.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {15.0f, -50.0f}}},
	});

	all.push_back({
	    .id = "needs.eats_from_field",
	    .name = "Hungry creature eats from a field",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger with a ripe wheat field of its town in front of it, and no other food.",
	    .expected = "It walks to the field, faces it, pulls a handful of wheat from it and eats it. The handful holds no "
	                "food, as in the game, so it brings the wheat back up and is not hungry for a while. The field's crop "
	                "stays as it was.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -50.0f}, {15.0f, 10.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -40.0f}, .ripeCrop = true}},
	});

	all.push_back({
	    .id = "needs.eats_from_store",
	    .name = "Hungry creature eats from a storage pit",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger of size 1.3 with its town's storage pit in front of it, full of food, and no other "
	                   "food. A creature eats from a pit only when the pit is no wider than fifteen times its size: a "
	                   "Celtic pit needs a size of about 1.16.",
	    .expected = "It walks to the pit's food pile, faces it, takes a handful of wheat from it and eats it. The pit's "
	                "food falls by the handful (1300, a thousand for each unit of the tiger's size), and its hunger falls.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -65.0f}, {20.0f, 10.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .size = 1.3f,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -50.0f}, .storedFood = 5000, .storedWood = 0}},
	});

	all.push_back({
	    .id = "needs.eats_from_tree",
	    .name = "Creature eats from a tree",
	    .facet = Facet::Needs,
	    .description = "A tiger with an oak in front of it. A script tells it to eat from the tree, as a script or a "
	                   "copied deed makes it.",
	    .expected = "It walks to the oak, faces it, takes a handful of fruit from it and eats it. The handful holds no "
	                "food, as in the game, so it brings the fruit back up. The tree stays as it was.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -40.0f}, {15.0f, 10.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "EatFromTree"}},
	});

	all.push_back({
	    .id = "objects.field_food_to_store",
	    .name = "Creature gives food from a field to a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit in front of it and a ripe wheat field to one side. A script tells it to "
	                   "give food from the field to the pit, as a town's need for food makes it.",
	    .expected = "It walks to the field, faces it and pulls a handful of wheat from it. It goes to where it can throw "
	                "into the pit, faces the pit with a look of compassion and throws the wheat in. The handful holds no "
	                "food, as in the game, so the pit's food stays as it was.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {65.0f, 45.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 2000, .storedWood = 0},
	                {.type = FieldTypeInfo::Wheat, .offset = {50.0f, 35.0f}, .ripeCrop = true}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "GiveFoodFromFieldToStoragePit", .with = 1}},
	});

	all.push_back({
	    .id = "objects.cow_to_store",
	    .name = "Creature drops a cow in a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit in front of it and a cow to one side. A script tells it to drop the cow "
	                   "in the pit.",
	    .expected = "It walks to the cow and picks it up, faces the pit, goes to where it can throw into the pit with a "
	                "look of compassion and throws the cow in. The pit takes the cow as food.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -75.0f}, {40.0f, 10.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 2000, .storedWood = 0},
	                {.type = AnimalInfo::Cow, .offset = {25.0f, -10.0f}, .living = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "DropCowInStoragePit", .with = 1}},
	});

	all.push_back({
	    .id = "objects.steal_food_from_store",
	    .name = "Creature steals food from a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with an empty storage pit in front of it and a second pit full of food to one side. A "
	                   "script tells it to steal food from the second pit for the first, as a copy of the player's theft "
	                   "makes it.",
	    .expected = "It goes to the full pit, looking at it, faces it and takes a handful of wheat. It goes to where it can "
	                "throw into the empty pit with a look of compassion, faces the pit and throws the wheat in. The empty "
	                "pit gets the food, and the full pit keeps all of its food, as in the game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {85.0f, 50.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {70.0f, 30.0f}, .storedFood = 5000, .storedWood = 0}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StealFoodFromStoragePit", .with = 1}},
	});

	all.push_back({
	    .id = "objects.steal_wood_from_store",
	    .name = "Creature steals wood from a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit with no wood in front of it and a second pit full of wood to one side. "
	                   "A script tells it to steal wood from the second pit for the first.",
	    .expected = "It goes to the full pit, faces it and takes a handful of logs. It goes to where it can throw into the "
	                "first pit with a look of compassion, faces the pit and throws the logs in. The first pit gets the "
	                "wood, and the full pit keeps all of its wood, as in the game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {85.0f, 50.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {70.0f, 30.0f}, .storedFood = 0, .storedWood = 5000}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StealWoodFromStoragePit", .with = 1}},
	});

	all.push_back({
	    .id = "objects.steal_toy_to_town",
	    .name = "Creature steals a toy and takes it to a town",
	    .facet = Facet::Objects,
	    .description = "A tiger with a cuddly toy in front of it, and the storage pit of the player's town behind it. "
	                   "A script tells it to steal the toy and take it to the town.",
	    .expected = "It picks the toy up, looking at it. It goes near the storage pit of the town, to within three times its "
	                "height, looking at the pit, and puts the toy down there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 90.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "StealObjectAndTakeToTown"}},
	    .townBeliefInPlayer = 1000.0f,
	});

	all.push_back({
	    .id = "objects.kiss_villager",
	    .name = "Creature kisses a villager in play",
	    .facet = Facet::Objects,
	    .description = "A tiger with a villager in front of it. A script tells it to kiss the villager in play.",
	    .expected = "It walks to two times its height from the villager, faces it and plays the kiss.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -45.0f}, {15.0f, 10.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -40.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "PlayfullyKissVillager"}},
	});

	// A tiger that stands still between two cows: the left one a script made and holds, as Land 1 holds the leash
	// lesson's cow, and the right one free
	all.push_back({
	    .id = "animals.script_cow_beside_creature",
	    .name = "A script's cow stays beside a creature",
	    .facet = Facet::Animals,
	    .description = "A tiger stands still between two cows, eight metres from each. The cow on the left was made by "
	                   "a script that holds it: it can't be moved, picked up or killed, as the cow of Land 1's leash "
	                   "lesson. The cow on the right is free.",
	    .expected = "The free cow sees the tiger and runs from it. The script's cow stays where it was put, in the clip "
	                "of an animal a script holds still, and takes no notice of the tiger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -10.0f}, {20.0f, 10.0f}}, .distance = 0.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .pauseMind = true}},
	    .objects = {{.type = AnimalInfo::Cow,
	                 .offset = {-8.0f, 0.0f},
	                 .indestructible = true,
	                 .inScript = true,
	                 .cannotBePickedUp = true,
	                 .immovable = true},
	                {.type = AnimalInfo::Cow, .offset = {8.0f, 0.0f}, .living = true}},
	});

	// A free cow beside a tiger that stands still: the cow runs from the tiger, and its fright ends after a time
	all.push_back({
	    .id = "animals.cow_after_fright",
	    .name = "A cow goes back to its ways after a fright",
	    .facet = Facet::Animals,
	    .description = "A tiger stands still with a free cow eight metres from it. The cow takes fright at the tiger.",
	    .expected = "The cow runs from the tiger, then turns to watch it. When its fright ends, it goes back to what it "
	                "did before the fright: it decides again and wanders with its herd. It does not stay in its fright.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {40.0f, 20.0f}}, .distance = 0.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .pauseMind = true}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {8.0f, 0.0f}, .living = true}},
	});

	// A script slows one of two cows and then sends both, as Greedy Farmer slows its cow
	all.push_back({
	    .id = "animals.script_slows_cow",
	    .name = "A script slows a cow",
	    .facet = Facet::Animals,
	    .description = "Two cows stand side by side, eight metres apart. After one second, SET_PROPERTY gives the left "
	                   "cow a speed of 0.05 metres a turn. Then MOVE_GAME_THING sends each cow 30 metres to the north. "
	                   "The scenario keeps each cow as a script variable does.",
	    .expected = "The right cow walks to its point at the speed of its kind. The left cow walks slowly in its walk "
	                "clip, and arrives a long time after the right cow. A script that reads the speed of the left cow "
	                "gets 0.05.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {15.0f, 35.0f}}, .distance = 0.6f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {-4.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {4.0f, 0.0f}, .living = true}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_PROPERTY",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 11.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.05f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-4.0f, 30.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {4.0f, 30.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}}},
	});

	// A script slows a flock of animals and then sends each one, as The Slavers slows its animal flock
	all.push_back({
	    .id = "animals.script_slows_flock",
	    .name = "A script slows a flock of animals",
	    .facet = Facet::Animals,
	    .description = "A horse, a sheep and a cow stand in a row, six metres apart. After one second, a script makes a "
	                   "flock of them, and SET_PROPERTY gives the flock a speed of 0.05 metres a turn. Then "
	                   "MOVE_GAME_THING sends each animal 30 metres to the north. The scenario keeps each animal as a "
	                   "script variable does.",
	    .expected = "Each animal of the flock takes the speed: the three animals walk slowly in their walk clips, side "
	                "by side, and arrive together. A script that reads the speed of the flock gets 0.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {15.0f, 35.0f}}, .distance = 0.6f},
	    .objects = {{.type = AnimalInfo::Horse, .offset = {-6.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Sheep, .offset = {0.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {6.0f, 0.0f}, .living = true}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_PROPERTY",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 11.0f},
	                                      {.kind = ScriptArgument::Kind::Flock},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.05f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-6.0f, 30.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {0.0f, 30.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 2, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {6.0f, 30.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}}},
	});

	// A free crow and a free cow, each the leader of a flock or herd of its own. Each picks its next place about its
	// home, in map units, and goes there.
	all.push_back({
	    .id = "animals.herd_new_leg",
	    .name = "A flock's leader goes to new places about its home",
	    .facet = Facet::Animals,
	    .description = "A free crow and a free cow stand at the homes of their own flock and herd. The crow flies from "
	                   "place to place at once. A cow stays 200 turns at one place before it leads its herd on.",
	    .expected = "The crow flies to places about where it was put and stays over that area. The cow grazes, and later "
	                "walks to a new place within 80 metres of where it was put. Neither goes off in a straight line out "
	                "of the reach of its home.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-80.0f, -80.0f}, {80.0f, 80.0f}}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Crow, .offset = {10.0f, 0.0f}, .living = true}},
	});

	// A cart that makes a reaction to look at it, with two cows near it
	all.push_back({
	    .id = "animals.cows_look_at_cart",
	    .name = "Cows look at a thing to look at",
	    .facet = Facet::Animals,
	    .description = "A cart in open land makes a reaction to look at it, of the kind a thing that came to rest makes. "
	                   "Two free cows stand 8 and 16 metres east of it.",
	    .expected = "Each cow stops and stands. It turns to the cart a little at a time until it faces the cart. When "
	                "its reaction ends, it goes back to what it did before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-4.0f, -8.0f}, {20.0f, 8.0f}}, .distance = 0.6f},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, 0.0f}, .reaction = Reaction::LookAtObject},
	                {.type = AnimalInfo::Cow, .offset = {8.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Cow, .offset = {16.0f, 0.0f}, .living = true}},
	});

	// A burning barrel west of a cow, and a hut east of the cow: the cow's run from the fire leads it into the hut
	all.push_back({
	    .id = "animals.cow_runs_round_a_hut",
	    .name = "A cow runs round a hut in its way",
	    .facet = Facet::Animals,
	    .description = "A barrel burns in open land. A free cow stands 6 metres east of it, and a hut stands 4.5 metres east "
	                   "of the cow, in the way of its run from the fire.",
	    .expected = "The cow runs east from the fire. It does not walk through the hut: it goes round the side of the "
	                "hut, then on to where it runs to. Then it turns to watch the fire.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, -15.0f}, {40.0f, 15.0f}}, .distance = 0.6f},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {0.0f, 0.0f}, .alight = true},
	                {.type = AnimalInfo::Cow, .offset = {6.0f, 0.0f}, .living = true},
	                {.type = AbodeInfo::CelticHut, .offset = {10.5f, 0.0f}}},
	});

	all.push_back({
	    .id = "needs.starving",
	    .name = "Starving creature faints",
	    .facet = Facet::Needs,
	    .description = "A tiger with almost no energy left and no food anywhere, its body's time ten times as fast.",
	    .expected = "Its hunger desire grows and it shows it is hungry; once its energy runs out it faints where it "
	                "stands, lies out cold, and gets up again with a little energy back.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.02f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.sleep_day",
	    .name = "Tired creature sleeps by day",
	    .facet = Facet::Needs,
	    .description = "A worn out tiger at midday, its desire to sleep strong, its body's time ten times as fast.",
	    .expected = "It may yawn, then lies down and sleeps, eyes closed, until it is rested, and gets up again.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.sleep_night",
	    .name = "Tired creature sleeps at night",
	    .facet = Facet::Needs,
	    .description = "Ten at night, the clock standing still: a tiger a little tired, which the night makes more so.",
	    .expected = "Under the night sky its desire to sleep grows with the dark; it lies down and sleeps, and sleeps "
	                "longer than by day.",
	    .environment = {.hour = 22.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.5f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Tiredness, .fraction = 0.6f}},
	    }},
	});

	all.push_back({
	    .id = "needs.poo",
	    .name = "Needs to poo",
	    .facet = Facet::Needs,
	    .description = "A tiger full of poo with its desire to go as strong as it gets, and an oak 30 m in front of it. "
	                   "A creature poos only by a thing that is not alive and is taller than 1.5 m.",
	    .expected = "It may show it needs a poo first, then walks to the oak, turns its back to it and squats. All through the "
	                "squat's loop, every fifth game turn, "
	                "a lump of poo comes out at its rear and is thrown back along the ground behind it, and the desire "
	                "is held back for a minute.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.95f},
	        .desires = {{.desire = Desire::Poo, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	});

	all.push_back({
	    .id = "needs.puke",
	    .name = "Puke",
	    .facet = Facet::Needs,
	    .description = "A tiger is told to be sick every so often, as eating something bad would make it.",
	    .expected = "It bends down and is sick. During the loop of the puke animation, twelve drops come out of its lips "
	                "each game turn. They fall down and to the sides, meet the ground, slide a short way and go after "
	                "three seconds.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Act(Kind::Puke, 0, 2.0f), Act(Kind::Puke, 0, 10.0f, true)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "liquid.puke",
	    .name = "Sick as liquid particles",
	    .facet = Facet::Needs,
	    .description = "From behind, a tiger is sick at once, and again eight seconds after each puke ends.",
	    .expected = "During the loop of the puke animation, twelve drops come out of the lips each game turn. Each drop "
	                "has a random speed down and to the sides, in no relation to where the tiger looks. The drops have "
	                "brown and yellow shades of the light of the land where the tiger stands. They fall, meet the "
	                "ground, slide a short way, slow down and go three seconds after they meet the ground. Each drop "
	                "shows a different blob frame at random each time the game draws it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Act(Kind::Puke, 0, 0.5f), Act(Kind::Puke, 0, 8.0f, true)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "liquid.storm_shadow",
	    .name = "Sick under a storm's shadow",
	    .facet = Facet::Needs,
	    .description = "Rain at noon, the clock standing still: the rain's storm stands over the middle of the map, where "
	                   "a tiger is sick at once, and again eight seconds after each puke ends.",
	    .expected = "The storm's shadow darkens a square of land about 400 units across round the middle of the map, "
	                "darkest in its middle and soft at its edges, but never darker than the land's darkest warm level. "
	                "Only the land in view is shaded. The drops take the light of the land where the tiger stands as "
	                "the land was laid: the game makes them in a turn, when the shadows are not on the land.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .weather = Weather::Rain, .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Act(Kind::Puke, 0, 0.5f), Act(Kind::Puke, 0, 8.0f, true)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "liquid.village_light",
	    .name = "Sick at night by a village light",
	    .facet = Facet::Needs,
	    .description = "Eleven at night, the clock standing still: a Norse gate with its two lanterns stands beside a "
	                   "tiger, which is sick at once, and again eight seconds after each puke ends.",
	    .expected = "Each lantern's light stamps a round patch of the land's warm levels round the lantern. The patch "
	                "lights dark land, and lights bright land only where it outshines it. The hand's light does the "
	                "same round the hand. The drops take the light of the land where the tiger stands as the land was "
	                "laid: the game makes them in a turn, when the lights are not on the land.",
	    .environment = {.hour = 23.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimatedStaticInfo::NorseGate, .offset = {30.0f, 20.0f}}},
	    .commands = {Act(Kind::Puke, 0, 0.5f), Act(Kind::Puke, 0, 8.0f, true)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "liquid.temple",
	    .name = "The drops go on in the temple",
	    .facet = Facet::Needs,
	    .description = "The player has a temple west of the middle. A tiger is sick at once. Five seconds later the "
	                   "player is put in the temple's main room while the drops are on the land.",
	    .expected = "The world stops in the temple, but the drops do not: they step on by the time of the frames and "
	                "are drawn there. The inspector's particles.liquid count falls to no drops within three seconds in "
	                "the temple. When the player leaves the temple, no drops are left hanging in the air.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Act(Kind::Puke, 0, 0.5f), {.kind = Kind::EnterTemple, .delaySeconds = 5.0f, .value = 3}},
	});

	all.push_back({
	    .id = "needs.exhaustion",
	    .name = "Exhaustion and fainting from running",
	    .facet = Facet::Needs,
	    .description = "An already tired tiger is made to run back and forth across the testbed, its body's time twenty "
	                   "times as fast.",
	    .expected = "Running tires it; past 0.8 exhaustion it slows down, and at 1 it faints and lies out cold, then is "
	                "carried in a fizz to its pen, the middle of the testbed, where it rests and gets up again before "
	                "running on.",
	    .environment = {.bodyTimeScale = 20.0f},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {-150.0f, 0.0f},
	        .facingDegrees = 270.0f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.6f, .dehydration = 0.0f, .poo = 0.0f},
	    }},
	    // Each run is long enough to cross; the commands don't wait for it to be free, as its idle mind sits it down
	    // whenever it stops
	    .commands = {Go(Kind::RunTo, 0, {150.0f, 0.0f}, 0.5f, false), Go(Kind::RunTo, 0, {-150.0f, 0.0f}, 16.0f, false),
	                 Go(Kind::RunTo, 0, {150.0f, 0.0f}, 16.0f, false)},
	    .repeatFrom = 1,

	});

	// The needs left to themselves: nothing is held and no desire is set, only the body's time runs faster, so the
	// needs build up as they would in play and the creature's mind sees to them when it chooses to
	all.push_back({
	    .id = "needs.left_alone",
	    .name = "Looks after itself",
	    .facet = Facet::Needs,
	    .description = "A tiger left to itself with three pots of food about it and the lake to the north, nothing held "
	                   "and no desire set, its body's time ten times as fast.",
	    .expected = "Over a few minutes its energy runs down, it gets hungry and goes to eat a pot of food, picking it up "
	                "first; the meal fills it with poo and it goes; it tires and sleeps, and it gets thirsty and walks to "
	                "the lake to drink. In between it sees to its other desires, such as calling for attention.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {40.0f, -40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {-60.0f, -30.0f}},
	                {.type = PotInfo::FoodPot, .offset = {20.0f, -90.0f}}},
	});

	all.push_back({
	    .id = "needs.gets_hungry",
	    .name = "Gets hungry and eats",
	    .facet = Facet::Needs,
	    .description = "A tiger half full, two pots of food on the land in front of it, nothing held and no desire set, "
	                   "its body's time ten times as fast.",
	    .expected = "Its energy runs down and its hunger grows by itself until it is the strongest desire; it walks up "
	                "to the nearer pot, picks it up, looks at it and eats it, and its energy fills up.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.energy = 0.55f}}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {50.0f, -80.0f}}},
	});

	all.push_back({
	    .id = "needs.gets_thirsty",
	    .name = "Gets thirsty and drinks at the lake",
	    .facet = Facet::Needs,
	    .description = "A tiger south of the lake and already rather thirsty, nothing held and no desire set, its body's "
	                   "time ten times as fast.",
	    .expected = "Its thirst grows until its desire for water is the strongest; it walks to the near edge of the lake, "
	                "bends down and drinks, its thirst clears and it doesn't want water for a while.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 80.0f}, .needs = {.dehydration = 0.5f}}},
	});

	all.push_back({
	    .id = "needs.gets_tired",
	    .name = "Gets tired at night and sleeps",
	    .facet = Facet::Needs,
	    .description = "Late evening with the clock running, a tiger a little worn out, nothing held and no desire set, "
	                   "its body's time ten times as fast.",
	    .expected = "As night falls its tiredness grows from the dark and its exhaustion; it may yawn, then lies down and "
	                "sleeps with its eyes closed, resting, and wakes once rested, looking sleepy.",
	    .environment = {.hour = 21.5f, .bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.exhaustion = 0.5f}}},
	});

	all.push_back({
	    .id = "needs.needs_to_poo",
	    .name = "Needs to poo after a meal",
	    .facet = Facet::Needs,
	    .description = "A tiger that has just eaten, half full of poo, nothing held and no desire set, its body's time "
	                   "ten times as fast. An oak stands 30 m in front of it, as a creature poos only by a thing that is "
	                   "not alive and is taller than 1.5 m.",
	    .expected = "Its desire to poo grows from the poo inside it; it may show it needs to go, then walks to the oak, "
	                "turns its back to it, squats and goes, a "
	                "lump drops behind it and the desire is held back for a while.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.poo = 0.6f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	});

	all.push_back({
	    .id = "needs.cold",
	    .name = "Cold creature",
	    .facet = Facet::Needs,
	    .description = "A blizzard over the whole island, freezing cold, and a tiger out in it, its body's time ten "
	                   "times as fast. Its other needs are kept met, and its desires to be friends and for attention, "
	                   "which would show before the cold, are held at nothing.",
	    .expected = "Its warmth falls to -1 within seconds, its desire to get warmer grows to its most and it shows it "
	                "is cold with its emote; the snow starts to lie on the land round it.",
	    .environment = {.weather = Weather::Blizzard, .bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	        .desires = {{.desire = Desire::BeFriends, .fraction = 0.0f},
	                    {.desire = Desire::AttractAttention, .fraction = 0.0f}},
	        .hold = true,
	    }},
	});
}

void AddGrowth(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "growth.timelapse",
	    .name = "Growth time-lapse",
	    .facet = Facet::Growth,
	    .description = "A baby tiger, small and early in growing up, kept fed and still, its body's time a thousand "
	                   "times as fast.",
	    .expected = "It grows fast while young and more slowly as it ages, up to size 2, the most a creature grows by "
	                "itself; the readout shows its size.",
	    .environment = {.bodyTimeScale = 1000.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, -40.0f}, {50.0f, 60.0f}}},
	    .creatures = {CreatureSetup {
	        .label = "baby",
	        .species = CreatureType::Tiger,
	        .size = 0.3f,
	        .phase = 3,
	        .needs = Content(),
	        .hold = true,
	        .pauseMind = true,
	    }},
	});

	all.push_back({
	    .id = "growth.first_stage_age",
	    .name = "A creature in the first stage lives turns but does not age",
	    .facet = Facet::Growth,
	    .description = "A tiger at the first stage of growing up, stage 0, its body's time a thousand times as fast. "
	                   "After 20 seconds it goes to stage 1.",
	    .expected = "At stage 0 the count of the turns that it lived increases and its age stays 0. At stage 1 its age "
	                "increases each time the count gets to a full tick of age.",
	    .environment = {.bodyTimeScale = 1000.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .phase = 0,
	        .needs = Content(),
	        .hold = true,
	        .pauseMind = true,
	    }},
	    .commands = {{.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 20.0f, .value = 1}},
	});

	struct Shape
	{
		std::string_view label;
		float alignment;
		float fatness;
		float strength;
	};
	constexpr std::array<Shape, 7> k_Shapes {{
	    {"thin", 0.0f, 0.0f, 0.5f},
	    {"fat", 0.0f, 1.0f, 0.5f},
	    {"weak", 0.0f, 0.5f, 0.0f},
	    {"strong", 0.0f, 0.5f, 1.0f},
	    {"evil", -1.0f, 0.5f, 0.5f},
	    {"good", 1.0f, 0.5f, 0.5f},
	    {"neutral", 0.0f, 0.5f, 0.5f},
	}};
	std::vector<CreatureSetup> lineup;
	for (size_t i = 0; i < k_Shapes.size(); ++i)
	{
		const auto& shape = k_Shapes.at(i);
		auto creature = Content(CreatureType::Tiger, {RowX(i, k_Shapes.size()), 0.0f}, 0.0f, shape.label);
		creature.alignment = shape.alignment;
		creature.fatness = shape.fatness;
		creature.strength = shape.strength;
		creature.size = 1.0f;
		creature.pauseMind = true;
		lineup.push_back(creature);
	}
	all.push_back({
	    .id = "growth.morph_lineup",
	    .name = "Morph lineup",
	    .facet = Facet::Growth,
	    .description = "Seven tigers of the same size side by side, left to right: thin, fat, weak, strong, evil, good "
	                   "and neutral.",
	    .expected = "Each body is pulled towards its axis' mesh: a lean and a heavy belly, slight and bulging muscles, "
	                "the evil one hunched and dark and the good one upright and bright, the last as the base mesh.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = lineup,
	});

	// A young and an adult of two species, each young one to the left of its adult
	struct Pair
	{
		CreatureType species;
		std::string_view young;
		std::string_view adult;
	};
	constexpr std::array<Pair, 2> k_Pairs {{
	    {CreatureType::Tiger, "young tiger", "adult tiger"},
	    {CreatureType::GiantApe, "young ape", "adult ape"},
	}};
	constexpr float k_YoungSize = 0.4f;
	constexpr float k_AdultSize = 2.0f;
	std::vector<CreatureSetup> ages;
	for (size_t i = 0; i < k_Pairs.size(); ++i)
	{
		const auto& pair = k_Pairs.at(i);
		auto young = Posed(pair.species, {RowX(i * 2, k_Pairs.size() * 2, 40.0f), 0.0f}, 0.0f, pair.young);
		young.size = k_YoungSize;
		ages.push_back(young);
		auto adult = Posed(pair.species, {RowX((i * 2) + 1, k_Pairs.size() * 2, 40.0f), 0.0f}, 0.0f, pair.adult);
		adult.size = k_AdultSize;
		ages.push_back(adult);
	}
	all.push_back({
	    .id = "growth.young_proportions",
	    .name = "Young and adult proportions",
	    .facet = Facet::Growth,
	    .description = "Left to right: a young tiger of size 0.4, a tiger of size 2, a young giant ape of size 0.4 and a "
	                   "giant ape of size 2. They stand still and breathe.",
	    .expected = "Each young creature is not a small adult. Its head is about 1.6 times as large for its size, its "
	                "hands and feet 1.4 times and its eyes 1.3 times. Each adult has a head 0.8 times as large and normal "
	                "hands, feet and eyes.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = ages,
	});

	std::vector<CreatureSetup> parade;
	constexpr size_t k_Species = static_cast<size_t>(CreatureType::_COUNT) - 1;
	constexpr size_t k_FrontRow = 9;
	std::vector<Command> parading;
	for (size_t i = 0; i < k_Species; ++i)
	{
		const auto row = i < k_FrontRow ? 0 : 1;
		const auto inRow = row == 0 ? i : i - k_FrontRow;
		const auto count = row == 0 ? k_FrontRow : k_Species - k_FrontRow;
		const glm::vec2 offset {RowX(inRow, count, 40.0f), 60.0f + (static_cast<float>(row) * 50.0f)};
		auto creature = Posed(static_cast<CreatureType>(i + 1), offset);
		parade.push_back(creature);
		parading.push_back(Go(Kind::WalkTo, i, offset - glm::vec2(0.0f, 60.0f), i == 0 ? 3.0f : 0.0f, false));
	}
	for (size_t i = 0; i < k_Species; ++i)
	{
		parading.push_back(Act(Kind::FaceCamera, i, i == 0 ? 8.0f : 0.0f));
	}
	all.push_back({
	    .id = "growth.species_parade",
	    .name = "All species parade",
	    .facet = Facet::Growth,
	    .description = "One creature of every species, all at size 1, in two rows: cow, tiger, leopard, wolf, lion, "
	                   "horse, tortoise, zebra and brown bear in front; polar bear, sheep, chimp, ogre, mandrill, rhino, "
	                   "gorilla and giant ape behind.",
	    .expected = "After a moment they all walk forward together, then turn to face the camera, each "
	                "with its own walk.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -10.0f}}},
	    .creatures = parade,
	    .commands = parading,
	});

	// A temple well west of the middle, turned as Land 1's, and a tiger standing off to its east
	constexpr glm::vec2 k_Temple {-300.0f, 100.0f};
	constexpr float k_TempleTurn = 36.0f;
	constexpr glm::vec2 k_OutOfThePen {-240.0f, 40.0f};
	all.push_back({
	    .id = "growth.temple_pen",
	    .name = "Shrinking in the temple's pen",
	    .facet = Facet::Growth,
	    .description = "A tiger of size 2 belonging to the player, whose temple stands to the west, walks home to the "
	                   "temple's pen, waits there and walks back out, and again.",
	    .expected = "Its home is the pen in front of the temple. Walking in between the pen's two walls it shrinks, from its "
	                "own size 16 units from the middle of the pen to a newborn's size (0.22) 14 units from it; small, it "
	                "walks more slowly and its animations play faster. Walking out it grows back the same way. Its own "
	                "size, which the spawner shows, stays 2 throughout.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = Posed(CreatureType::Tiger, k_OutOfThePen, 90.0f);
		    tiger.size = 2.0f;
		    return tiger;
	    }()},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {Act(Kind::WalkHome, 0, 1.0f, true), Go(Kind::WalkTo, 0, k_OutOfThePen, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "growth.temple_pen_unbuilt",
	    .name = "No shrinking at a temple still going up",
	    .facet = Facet::Growth,
	    .description = "As the temple pen scenario, but the player's temple is only nine tenths built, as builders leave it "
	                   "on Land 1 before it is finished.",
	    .expected = "Its home is still the pen in front of the temple, and it walks there, but it keeps its own size (2) in "
	                "the pen: a temple shrinks its creature only once it is built.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = Posed(CreatureType::Tiger, k_OutOfThePen, 90.0f);
		    tiger.size = 2.0f;
		    return tiger;
	    }()},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn, .built = 0.9f}},
	    .commands = {Act(Kind::WalkHome, 0, 1.0f, true), Go(Kind::WalkTo, 0, k_OutOfThePen, 4.0f)},
	    .repeatFrom = 0,
	});
}

/// The first land's Creature Breeder, run by the game's own script at the same places of the map as on the first land
/// (the testbed spans the whole map), each as high above the land as there; the kennels are a marker, as on the second
/// and fifth lands. The player's Cow stands by; the scenario clicks the kennels' scroll, and the breeder offers the
/// bonus creatures the debug option takes as unlocked
void AddBreederOffers(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The first land's breeder places, from the middle of the map, and how far above the land each is there
	constexpr glm::vec2 k_BreederStart {-661.839f, 850.803f};
	constexpr glm::vec2 k_SoapBox {-663.838f, 849.415f};
	constexpr glm::vec2 k_Kennels {-660.73f, 851.21f};
	constexpr glm::vec2 k_CreaturePlace {-667.115f, 843.713f};
	const auto marker = [](glm::vec2 point, float height = 0.0f) {
		return Argument {.kind = Argument::Kind::Marker, .point = point, .height = height};
	};
	const Command runBreeder {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "CreatureBreeder",
	    // Land, the breeder's start, the soap box, the kennels, where the creatures stand, then two camera shots
	    .scriptArguments = {{.kind = Argument::Kind::Number, .number = 1.0f},
	                        marker(k_BreederStart),
	                        marker(k_SoapBox),
	                        marker(k_Kennels),
	                        marker(k_CreaturePlace),
	                        marker({-671.651f, 844.151f}, 4.151f),
	                        marker({-653.575f, 856.752f}, -1.247f),
	                        marker({-696.66f, 839.907f}, 12.69f),
	                        marker({-674.738f, 851.945f}, 0.719f)},
	    // As the first land sets them up: the player's creature, and no swap at the breeder yet
	    .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"GlobalCurrentCreature", {.number = -1.0f}},
	                      {"GlobalOldCreature", {.number = -1.0f}},
	                      {"BreederRunning", {.number = 0.0f}},
	                      {"NrOfChoosenCreature", {.number = 0.0f}},
	                      {"GlobalReset", {.number = 0.0f}}},
	};
	// The kennels' scroll goes up ten seconds in; the pointer at the middle of the screen, which looks at it, clicks it
	// with the Action button
	const std::vector<Command> commands {
	    runBreeder,
	    {.kind = Kind::PointerTo, .delaySeconds = 11.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	};
	const auto offer = [&](std::string_view id, std::string_view name, std::string_view description, std::string_view expected,
	                       creature_swap::Unlocked unlocked) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Story,
		    .description = description,
		    .expected = expected,
		    .environment = {.dispenserGrid = false, .unlockedBonusCreatures = unlocked},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_Kennels.x - 20.0f, 10.0f, k_Kennels.y - 22.0f},
		                .look = {k_Kennels.x, 6.0f, k_Kennels.y}},
		    .creatures = {Content(CreatureType::Cow, k_CreaturePlace + glm::vec2 {10.0f, -12.0f}, 0.0f, "player's cow")},
		    .commands = commands,
		});
	};
	creature_swap::Unlocked leopard;
	leopard.set(static_cast<size_t>(creature_swap::Bonus::Leopard));
	auto leopardAndHorse = leopard;
	leopardAndHorse.set(static_cast<size_t>(creature_swap::Bonus::Horse));
	offer("creature.breeder_offer", "The Creature Breeder offers all five bonus creatures",
	      "All five bonus creatures taken as unlocked (the debug option). The first land's breeder script runs at its "
	      "places; the player's Cow stands by. Twelve seconds in the kennels' scroll is clicked.",
	      "The breeder walks to the soap box and the camera to him; sparkles and the Leopard, Horse, Mandrill, Gorilla and "
	      "Rhino appear one beside the next; between the first and the last the breeder says \"Look. I have these "
	      "wondrous Creatures you can choose from.\" A scroll stands over each. Click one with the Cow near to choose it; "
	      "click it again to swap. The offer ends after a swap or after 60 to 120 seconds, the others vanishing.",
	      creature_swap::Unlocked {}.set());
	offer("creature.breeder_offer_one", "The Creature Breeder offers one bonus creature",
	      "Only the Leopard taken as unlocked; otherwise as creature.breeder_offer.",
	      "The Leopard alone appears, with its scroll; the camera takes in it and the breeder, who says the line for one "
	      "creature.",
	      leopard);
	offer("creature.breeder_offer_two", "The Creature Breeder offers two bonus creatures",
	      "The Leopard and the Horse taken as unlocked; otherwise as creature.breeder_offer.",
	      "The Leopard and the Horse appear side by side, each with a scroll, and the breeder says \"Look. I have these "
	      "wondrous Creatures you can choose from.\"",
	      leopardAndHorse);

	// The swap a quest offers at its end, as The Lost Flock offers its sheep: the game's own SwapCreatures script over a
	// sheep of nobody's, the player's Cow nearby. The pointer looks at the sheep and clicks it twice with the Action
	// button: once to choose it, and once more, after the Cow has come over, to swap
	auto sheep = Content(CreatureType::Sheep, {15.0f, 0.0f}, 270.0f, "the flock's sheep");
	sheep.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "creature.field_swap",
	    .name = "A quest's offer of its creature, as The Lost Flock's sheep",
	    .facet = Facet::Story,
	    .description = "The game's SwapCreatures script runs over a sheep of nobody's, the player's Cow 25 away. Three "
	                   "seconds in the sheep is clicked; twelve seconds later it is clicked again.",
	    .expected = "A scroll stands over the sheep, which looks at the camera. At the first click the scroll goes, the "
	                "Cow walks over and the two look each other over, the good advisor saying the reminder. At the second "
	                "click a cinema shows both: sparkles, both point at each other, the swap with its beams and the swap "
	                "sound, then the evil advisor says the sheep is now the player's. The offer starts again over the "
	                "Cow, so they can swap back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {5.0f, 22.0f, -50.0f}, .look = {15.0f, 8.0f, 0.0f}},
	    .creatures = {Content(CreatureType::Cow, {-10.0f, 0.0f}, 90.0f, "player's cow"), sheep},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SwapCreatures",
	                  .scriptArguments = {{.kind = Argument::Kind::Creature, .index = 1}},
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                                    {"GlobalReset", {.number = 0.0f}}}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 12.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});

	// The end of a cinema, as the films of the creature trainer end: the script sends the camera up and away, and gives
	// the camera back before it arrives
	const auto move = [](std::string_view native, glm::vec2 point, float height, float seconds, float delay) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = native,
		    .scriptArguments = {{.kind = Argument::Kind::Position, .point = point, .height = height}, {.number = seconds}}};
	};
	all.push_back({
	    .id = "story.cinema_end_glide",
	    .name = "The camera at the end of a cinema",
	    .facet = Facet::Story,
	    .description = "A script takes the camera. It sends the camera up over 2 seconds, looking north. After 1.2 "
	                   "seconds it sends the camera north over 3 seconds and gives the camera back at once.",
	    .expected = "The camera rises and turns to look north. It does not stop when the script gives it back. It "
	                "continues smoothly to the last place that the script chose and stops there about 1.5 seconds "
	                "later.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 8.0f, -40.0f}, .look = {0.0f, 4.0f, 0.0f}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 move("MOVE_CAMERA_POSITION", {0.0f, -40.0f}, 45.0f, 2.0f, 0.5f),
	                 move("MOVE_CAMERA_FOCUS", {0.0f, 60.0f}, 0.0f, 2.0f, 0.0f),
	                 move("MOVE_CAMERA_POSITION", {5.0f, -20.0f}, 45.0f, 3.0f, 1.2f),
	                 {.kind = Kind::CallNative, .delaySeconds = 0.0f, .script = "END_CAMERA_CONTROL"}},
	});
}

/// The computer gods' creatures of the third and fifth lands: the game's own scripts load each from its mind file for
/// player two, as the two lands do. The player's Cow stands near, for the scripts that wait for it.
void AddRivalCreatureLoads(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	static constexpr glm::vec2 k_Place {20.0f, 0.0f};
	const auto load = [](std::string_view script, float scale) {
		return Command {.kind = Kind::RunScript,
		                .delaySeconds = 0.5f,
		                .script = script,
		                // The player, the place and the scale of the creature
		                .scriptArguments = {{.kind = Argument::Kind::Number, .number = 2.0f},
		                                    {.kind = Argument::Kind::Marker, .point = k_Place},
		                                    {.kind = Argument::Kind::Number, .number = scale}}};
	};
	const auto cow = Content(CreatureType::Cow, {-15.0f, 0.0f}, 90.0f, "player's cow");
	all.push_back({
	    .id = "creature.load_lethys",
	    .name = "The third land loads the creature of Lethys from his mind file",
	    .facet = Facet::Story,
	    .description = "The game's SetupLethysCreature script runs for player two with the scale 1.5, as the third land "
	                   "runs it.",
	    .expected = "A Wolf of player two appears on the ground at the place, fully grown. It takes the body, the "
	                "alignment and the tattoos of the LethysCreature mind file. It knows every leash, and it is the "
	                "creature of player two. The script sets its strength, warmth, fatness, energy, itchiness, poo, "
	                "exhaustion, thirst and fight health to 0.2 in the turn that it loads, and these values stay: the "
	                "Creature and CreatureNeeds components of the wolf show them after its first turns. Only the "
	                "warmth then moves to the temperature of the land.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -55.0f}, .look = {10.0f, 8.0f, 0.0f}},
	    .creatures = {cow},
	    .commands = {load("SetupLethysCreature", 1.5f)},
	});
	// As in the big fight of the fifth land, player two has a creature already, which the new creature replaces
	auto old = Content(CreatureType::Lion, {20.0f, 25.0f}, 180.0f, "player two's old lion");
	old.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "creature.load_nemesis",
	    .name = "The fifth land loads the creature of Nemesis from his mind file over his old creature",
	    .facet = Facet::Story,
	    .description = "Player two has a Lion. The game's SetupNemesisCreature script runs for player two with the scale "
	                   "2, as the fifth land runs it.",
	    .expected = "A second Lion of player two appears on the ground at the place, fully grown, from the "
	                "NemesisCreature mind file. The script log says that the player has a creature already and that the "
	                "game overwrites it. The new Lion is now the creature of player two.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -60.0f}, .look = {10.0f, 8.0f, 10.0f}},
	    .creatures = {cow, old},
	    .commands = {load("SetupNemesisCreature", 2.0f)},
	});
}

/// The Pied Piper of the first land. The game's script runs at the same map positions as on the first land, with the
/// Norse town, the creche and the piper's cave made there. The scenario clicks the creche, and the round starts.
void AddPiedPiperRound(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The first land's Norse town, as the lines of the first land make it, from the middle of the map. The town has the
	// number of the scenario's town, thus the scenario gives this town to the script.
	constexpr std::string_view k_NorseTown = R"(CREATE_TOWN(900, "-694.39,81.24", "PLAYER_ONE", 7, "NORSE")
SET_TOWN_BELIEF(900, "PLAYER_ONE", 9.238012)
CREATE_PLANNED_ABODE(900, "-637.52,157.17", "NORSE_ABODE_WONDER", 916, 250, 0, 0)
CREATE_ABODE(900, "-804.91,132.52", "NORSE_ABODE_C", 4710, 990, 0, 0)
CREATE_VILLAGER_POS("-797.15,132.48", "-804.91,132.52", "NORSE_FORESTER", 21)
CREATE_VILLAGER_POS("-797.15,132.48", "-804.91,132.52", "NORSE_HOUSEWIFE", 24)
CREATE_VILLAGER_POS("-797.15,132.48", "-811.55,119.58", "NORSE_FORESTER", 6)
CREATE_VILLAGER_POS("-797.15,132.48", "-811.55,119.58", "NORSE_HOUSEWIFE", 6)
CREATE_ABODE(900, "-779.62,153.34", "NORSE_ABODE_E", 6282, 1000, 0, 0)
CREATE_VILLAGER_POS("-779.69,141.47", "-782.63,152.23", "NORSE_HOUSEWIFE", 3)
CREATE_VILLAGER_POS("-779.69,141.47", "-782.63,152.23", "NORSE_HOUSEWIFE", 36)
CREATE_VILLAGER_POS("-779.69,141.47", "-767.31,151.49", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-779.69,141.47", "-767.31,151.49", "NORSE_HOUSEWIFE", 18)
CREATE_VILLAGER_POS("-779.69,141.47", "-704.62,122.31", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-702.20,105.53", "NORSE_ABODE_F", 1177, 888, 0, 0)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_HOUSEWIFE", 48)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_HOUSEWIFE", 1)
CREATE_ABODE(900, "-755.27,90.97", "NORSE_ABODE_A", 6217, 1000, 0, 0)
CREATE_VILLAGER_POS("-754.97,93.93", "-755.27,90.97", "NORSE_FORESTER", 1)
CREATE_VILLAGER_POS("-754.97,93.93", "-765.56,86.56", "NORSE_HOUSEWIFE", 24)
CREATE_ABODE(900, "-774.79,92.58", "NORSE_ABODE_F", 5756, 888, 0, 0)
CREATE_VILLAGER_POS("-772.92,95.61", "-811.55,119.58", "NORSE_HOUSEWIFE", 6)
CREATE_VILLAGER_POS("-772.92,95.61", "-765.56,86.56", "NORSE_FORESTER", 13)
CREATE_ABODE(900, "-906.86,94.09", "NORSE_ABODE_E", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.63,152.23", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.63,152.23", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-906.91,82.21", "-906.86,94.09", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-906.91,82.21", "-906.86,94.09", "NORSE_HOUSEWIFE", 3)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.57,97.55", "NORSE_FORESTER", 20)
CREATE_VILLAGER_POS("-906.91,82.21", "-709.75,113.09", "NORSE_HOUSEWIFE", 30)
CREATE_ABODE(900, "-881.77,176.70", "NORSE_ABODE_GRAVEYARD", 0, 1000, 0, 0)
CREATE_ABODE(900, "-846.17,32.59", "NORSE_ABODE_C", 1588, 1000, 0, 0)
CREATE_VILLAGER_POS("-854.01,32.48", "-755.27,90.97", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-854.01,32.48", "-846.17,32.59", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-854.01,32.48", "-846.17,32.59", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-854.01,32.48", "-788.12,103.35", "NORSE_FORESTER", 35)
CREATE_ABODE(900, "-695.01,209.27", "NORSE_ABODE_C", 2486, 1000, 0, 0)
CREATE_VILLAGER_POS("-699.81,203.07", "-755.27,90.97", "NORSE_HOUSEWIFE", 6)
CREATE_VILLAGER_POS("-699.81,203.07", "-695.01,209.27", "NORSE_FORESTER", 24)
CREATE_VILLAGER_POS("-699.81,203.07", "-695.01,209.27", "NORSE_HOUSEWIFE", 30)
CREATE_ABODE(900, "-733.21,81.36", "NORSE_ABODE_STORAGE_PIT", 3730, 1000, 20000, 30000)
CREATE_ABODE(900, "-802.99,107.32", "NORSE_ABODE_D", 5039, 1000, 0, 0)
CREATE_VILLAGER_POS("-796.25,104.55", "-802.99,107.32", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-796.25,104.55", "-802.99,107.32", "NORSE_HOUSEWIFE", 17)
CREATE_VILLAGER_POS("-796.25,104.55", "-713.05,105.14", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-796.25,104.55", "-713.05,105.14", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-796.25,104.55", "-704.62,122.31", "NORSE_FORESTER", 30)
CREATE_TOWN_CENTRE(900, "-754.78,131.31", "NORSE_ABODE_TOWN_CENTRE", 6282, 990, 0)
CREATE_ABODE(900, "-795.14,92.64", "NORSE_ABODE_A", 5366, 1000, 0, 0)
CREATE_VILLAGER_POS("-792.71,94.37", "-802.99,107.32", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-792.71,94.37", "-795.14,92.64", "NORSE_FORESTER", 28)
CREATE_VILLAGER_POS("-792.71,94.37", "-795.14,92.64", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-709.16,133.52", "NORSE_ABODE_F", 1963, 1000, 0, 0)
CREATE_VILLAGER_POS("-712.91,132.09", "-709.16,133.52", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-712.91,132.09", "-709.16,133.52", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-709.75,113.09", "NORSE_ABODE_A", 1439, 1000, 0, 0)
CREATE_VILLAGER_POS("-712.69,113.59", "-774.79,92.58", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-712.69,113.59", "-709.75,113.09", "NORSE_FORESTER", 13)
CREATE_ABODE(900, "-691.99,170.12", "NORSE_ABODE_CRECHE", 4907, 1000, 0, 0)
CREATE_ABODE(900, "-707.14,145.21", "NORSE_ABODE_B", 1700, 1000, 0, 0)
CREATE_VILLAGER_POS("-711.13,144.80", "-774.79,92.58", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-711.13,144.80", "-707.48,145.18", "NORSE_FORESTER", 27)
CREATE_VILLAGER_POS("-711.13,144.80", "-707.14,145.21", "NORSE_HOUSEWIFE", 50)
CREATE_NEW_TOWN_FIELD(900, "-825.00,135.00", 2, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-735.00,195.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-725.00,195.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-725.00,185.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-735.00,185.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-825.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-815.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-805.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-935.00,95.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-925.00,95.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-925.00,105.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-935.00,105.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-855.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-855.00,45.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-845.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-835.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-835.00,45.00", 0, 0.000000)
)";
	// The positions on the first land, from the middle of the map: the creche, the piper's cave and where the player's
	// creature stands, far from the town and the cave
	constexpr glm::vec2 k_Creche {-691.99f, 170.12f};
	constexpr glm::vec2 k_Cave {-589.2f, 252.12f};
	constexpr glm::vec2 k_CreaturePlace {-645.0f, -451.0f};
	const Command runRound {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "PiedPiper",
	    // As the first land sets them up: the piper's cave, the Norse town, the player's creature, and no round yet
	    .scriptGlobals = {{"PiperCave", {.kind = Argument::Kind::Object, .index = 0}},
	                      {"NorseTown", {.kind = Argument::Kind::Town}},
	                      {"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"PiedPiperInitiated", {.number = 0.0f}},
	                      {"PiedPiperFinished", {.number = 0.0f}},
	                      {"PiedPiperCompletedNow", {.number = 0.0f}},
	                      {"PiedPiperChildrenInCave", {.number = 0.0f}},
	                      {"PiedPiperDead", {.number = 0.0f}},
	                      {"CaveOpen", {.number = 0.0f}}},
	};
	all.push_back({
	    .id = "story.pied_piper_round",
	    .name = "The Pied Piper takes the children of the Norse town",
	    .facet = Facet::Story,
	    .description = "The first land's Norse town, its creche and the piper's cave stand at their places on the first "
	                   "land. The player's Cow stands 500 metres south of the town. The game's own PiedPiper script runs, "
	                   "and three seconds in the creche is clicked.",
	    .expected = "A scroll stands over the creche. On the click a film plays: the creche leader walks to the soap box "
	                "and asks for help, the camera shows the piper at his cave and the cave's sounds. After the film the "
	                "piper dances near the town, children join his flock, and he takes them to the cave. He leaves the "
	                "dance after ten minutes or with five children, and the round starts again. When the Cow comes "
	                "near the piper, he runs from it to his cave, and then the round starts again.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Creche.x - 20.0f, 12.0f, k_Creche.y - 22.0f},
	                .look = {k_Creche.x, 4.0f, k_Creche.y}},
	    .creatures = {Content(CreatureType::Cow, k_CreaturePlace, 0.0f, "player's cow")},
	    .objects = {{.type = AnimatedStaticInfo::PiperCaveEntrance, .offset = k_Cave, .yawDegrees = -131.895f}},
	    .landScript = k_NorseTown,
	    .commands = {runRound,
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	    .objectsAfterLandScript = true,
	});
}

/// The Ogre quest of the first land. The game's script runs at the same map positions as on the first land. The
/// player's Cow is in the valley below the lair, where the script sends it. The scenario sets the flag of the fight
/// lesson, thus the script makes the Ogre at the lair immediately. The scenario clicks the scroll after ten seconds.
void AddOgreLair(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The positions on the first land, from the middle of the map: the lair, the scroll and the position where the
	// script sends the player's creature
	constexpr glm::vec2 k_Lair {-21.307f, 600.6f};
	constexpr glm::vec2 k_Scroll {-6.428f, 624.607f};
	constexpr glm::vec2 k_SentBackTo {-28.15f, 484.55f};
	const Command runQuest {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "CreatureGuardian",
	    .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"CreatureDevelopment_Fight", {.number = 1.0f}},
	                      {"CreatureGuardianFinished", {.number = 0.0f}},
	                      {"EndSendMyCreatureAwayFromGuardianFlag", {.number = 0.0f}}},
	};
	// The scroll appears when the script makes the Ogre. The pointer clicks the scroll at the middle of the screen with
	// the Action button.
	const std::vector<Command> introduction {
	    runQuest,
	    {.kind = Kind::PointerTo, .delaySeconds = 10.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	};
	const Framing framing {
	    .shot = Shot::Placed, .eye = {k_Scroll.x - 15.0f, 30.0f, k_Scroll.y - 45.0f}, .look = {k_Scroll.x, 0.0f, k_Scroll.y}};
	// The player's creature. Its mind is free, thus the script can control it and release it.
	auto cow = Content(CreatureType::Cow, k_SentBackTo, 0.0f, "player's cow");
	cow.hold = false;
	all.push_back({
	    .id = "story.ogre_lair",
	    .name = "The Ogre at his lair, and the film that brings him in",
	    .facet = Facet::Story,
	    .description = "The first land's Ogre quest: the player's Cow stands down the valley from the lair and the game's "
	                   "own script runs as after the fight lesson. Ten seconds in, the Ogre's scroll is clicked.",
	    .expected = "An ogre 1.2 times the Cow's size, named Sleg, sleeps at the lair beside a silver scroll. On the click "
	                "a film plays: music, the Ogre scratches himself, the advisors and the Ogre talk, the Cow points at "
	                "the Ogre, who looks it over, plays his angry animation and goes back to sleep. Then the Cow is the "
	                "player's again.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .commands = introduction,
	});
	all.push_back({
	    .id = "story.ogre_fed_twice",
	    .name = "Feeding the Ogre twice",
	    .facet = Facet::Story,
	    .description = "As story.ogre_lair, with two piles of food such as the hand drops lying 22 metres from the lair, "
	                   "far from the Cow.",
	    .expected = "After the film the script finds the nearer pile. A film takes the camera to it: the Ogre walks to it, "
	                "picks up a handful and eats it, and the rest of the pile goes. The good advisor asks whether he is "
	                "getting drowsy and he walks back to the lair. Then the same for the other pile, after which he falls "
	                "asleep, the evil advisor calls him thick, and the camera flies through the pass to the reward.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .objects = {{.type = PotInfo::MagicFood, .offset = k_Lair + glm::vec2 {10.0f, -20.0f}, .amount = 1000},
	                {.type = PotInfo::MagicFood, .offset = k_Lair + glm::vec2 {-12.0f, -20.0f}, .amount = 1000}},
	    .commands = introduction,
	});
	auto fight = introduction;
	fight.push_back(Go(Kind::WalkTo, 0, k_Lair + glm::vec2 {0.0f, -20.0f}, 70.0f, false));
	all.push_back({
	    .id = "story.ogre_fight",
	    .name = "The player's creature fights the Ogre",
	    .facet = Facet::Story,
	    .description = "As story.ogre_lair; 70 seconds after the click the Cow walks to 20 metres from the lair.",
	    .expected = "The evil advisor tells the player to prepare for battle, the Cow's leash comes off and the Ogre picks "
	                "a fight with it in an arena. If the Cow wins, a film shows the Ogre fall dead and flies through the "
	                "pass to the reward. If the Ogre wins, he says so and walks back to his lair, and another fight starts "
	                "30 seconds later while the Cow stays near.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .commands = fight,
	});
}

/// The last lesson of the first land's guide. The game's own script runs at the same map positions as on the first land.
/// The player's Tiger waits where the script makes it watch the guide. The scenario sets the flag of the Ogre quest, thus
/// the script does not show the Ogre's place.
void AddGuideStorm(std::vector<Scenario>& all)
{
	// The positions on the first land, from the middle of the map: the place where the guide wakes, the place where the
	// creature watches it, and the top of the mountain
	constexpr glm::vec2 k_GuideWakes {-271.88f, 251.481f};
	constexpr glm::vec2 k_CreatureWatches {-245.011f, 266.323f};
	constexpr glm::vec2 k_MountainTop {-241.71f, -46.942f};
	// Near enough to the top for the storm to start, then near enough for the last words of the guide
	constexpr glm::vec2 k_StormStarts = k_MountainTop + glm::vec2 {0.0f, 60.0f};
	constexpr glm::vec2 k_LastWords = k_MountainTop + glm::vec2 {0.0f, 25.0f};
	const Command runStorm {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "TheStorm",
	    .scriptGlobals = {{"CreatureGuardianInitiated", {.number = 1.0f}}},
	};
	// The guide sleeps for five minutes of game time, then walks to the framed place. The pointer clicks the middle of
	// the screen with the Action button three times, a minute apart, because the five minutes take longer in a slow
	// build. Later the Tiger walks up the mountain, first for the storm and then for the last words. The script moves
	// the Tiger back during the storm's film, thus it walks up again.
	const Command click {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3};
	const Command letGo {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3};
	const std::vector<Command> commands {
	    runStorm,
	    {.kind = Kind::PointerTo, .delaySeconds = 330.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    {.kind = Kind::PointerTo, .delaySeconds = 60.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    {.kind = Kind::PointerTo, .delaySeconds = 60.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    Go(Kind::WalkTo, 0, k_StormStarts, 60.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 120.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 60.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 120.0f, false),
	};
	auto tiger = Content(CreatureType::Tiger, k_CreatureWatches, 0.0f, "player's tiger");
	tiger.hold = false;
	all.push_back({
	    .id = "story.guide_storm",
	    .name = "The guide's last lesson, Nemesis's storm and the death of the guide",
	    .facet = Facet::Story,
	    .description = "The first land's TheStorm script runs on the flat testbed. The script makes the guide (a lion for "
	                   "the Tiger). The guide walks to the fight clearing and stays there for five minutes under a gold "
	                   "scroll. Then it walks to the framed place. From 330 seconds the pointer clicks the middle of the "
	                   "screen three times, a minute apart. Later the Tiger walks up to the mountain top. Click the lines "
	                   "that wait for a click.",
	    .expected = "The guide's music plays while the guide talks and walks to the mountain top. A rain storm forms and "
	                "comes to the top with Nemesis's music, Nemesis talks and the guide answers. Six lightning bolts and "
	                "an explosion from the storm hit the guide, the evil advisor holds on to the screen edge, and the "
	                "guide falls dead and stays down. Then the storm moves away towards the player's village. On the flat "
	                "land, the camera places of the script and the mountain top are above the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_GuideWakes.x + 30.0f, 40.0f, k_GuideWakes.y + 50.0f},
	                .look = {k_GuideWakes.x, 0.0f, k_GuideWakes.y}},
	    .creatures = {tiger},
	    .commands = commands,
	});
}

/// The stone circle of The Singing Stones, as the first land makes it at its places
void AddSingingStones(std::vector<Scenario>& all)
{
	// The circle's centre on the map, from the middle of the map
	constexpr glm::vec2 k_Circle {2496.6653f - 2560.0f, 2246.3257f - 2560.0f};
	all.push_back({
	    .id = "story.singing_stones_circle",
	    .name = "The stone circle of The Singing Stones",
	    .facet = Facet::Story,
	    .description = "The game's SingingStoneCircle script runs as the first land starts it. It makes the circle south "
	                   "of the middle: eight holders on a ring, three stones in them and the other stones far away.",
	    .expected = "Eight prints of the holders show on the ground, on a ring 17.5 from the centre, one every eighth of "
	                "a turn. The three fixed stones stand on the land in the first, second and sixth holders. The "
	                "holders' models are under the land and do not show. Two seconds in, the circle plays the notes of "
	                "its three stones in turn, one holder every half second.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Circle.x + 10.0f, 30.0f, k_Circle.y - 40.0f},
	                .look = {k_Circle.x, 0.0f, k_Circle.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SingingStoneCircle"}},
	});

	// The funeral march of the second land's singing stones brings back the dead in the ring. The script finds the dead
	// about a fixed place of the second land, from the middle of the map here.
	constexpr glm::vec2 k_Ring {2103.497f - 2560.0f, 3000.377f - 2560.0f};
	all.push_back({
	    .id = "story.singing_stones_raise_dead",
	    .name = "The singing stones bring back the dead in their ring",
	    .facet = Facet::Story,
	    .description = "A cow 3 east of the ring's centre and a villager 6 west of it are killed. Four seconds in, the "
	                   "game's BringBackTheDead script runs, as the funeral march on the second land starts it.",
	    .expected = "A healing glow 13.5 times its usual size covers the ring. The nearest dead thing comes back first: "
	                "the cow puffs steam and comes back alive at the ring's centre, not where it lay, as in the game. "
	                "About 3 seconds later the villager puffs steam and comes back where it lay, of the same kind and "
	                "age, as a skeleton. Each revived thing is let go 3 seconds after it comes back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Ring.x + 4.0f, 14.0f, k_Ring.y - 22.0f},
	                .look = {k_Ring.x, 0.0f, k_Ring.y}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = k_Ring + glm::vec2 {3.0f, 0.0f}, .life = 0.0f},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Ring + glm::vec2 {-6.0f, 0.0f}, .life = 0.0f}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 4.0f,
	                  .script = "BringBackTheDead",
	                  .scriptGlobals = {{"SSBringBackToLifeActive", {.number = 0.0f}}}}},
	});
}

/// The beach temple puzzle of the second land, as its script makes it: turned a half turn, at its normal size
void AddBeachTemplePuzzle(std::vector<Scenario>& all)
{
	// The puzzle's place, from the middle of the map. The half turn puts its three bases in a row to the south.
	constexpr glm::vec2 k_Puzzle {0.0f, -40.0f};
	constexpr float k_PuzzleObjectType = 32.0f;
	constexpr float k_HanoiKind = 3.0f;
	all.push_back({
	    .id = "story.beach_temple_puzzle",
	    .name = "The rings of The Beach Temple Puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's beach temple puzzle south of the middle, turned a half turn, as "
	                   "its quest makes it on the beach.",
	    .expected = "Three bases stand in a row from north to south, 20 apart. The four rings are stacked on the north "
	                "base, the largest at the bottom. The hand can take only the top ring. A ring that the hand drops "
	                "near a base or a larger ring flickers out and stands on that stack. A ring dropped far from a stack "
	                "flickers back to its place after six turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 35.0f, 25.0f, k_Puzzle.y - 20.0f},
	                .look = {k_Puzzle.x, 3.0f, k_Puzzle.y - 20.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 180.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HanoiKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}},
	});
}

/// The tree puzzle of the second land, as a script makes it: not turned, at its normal size
void AddTreePuzzle(std::vector<Scenario>& all)
{
	// The puzzle's place, from the middle of the map. The spots of the grid are about ten metres apart.
	constexpr glm::vec2 k_Puzzle {0.0f, -40.0f};
	constexpr float k_PuzzleObjectType = 32.0f;
	constexpr float k_TreeKind = 1.0f;
	all.push_back({
	    .id = "story.tree_puzzle",
	    .name = "The trees of the second land's tree puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's tree puzzle south of the middle: nine trees on a grid of three "
	                   "by three spots, with a lantern at each corner, mist over the spots and bats over the middle.",
	    .expected = "Nine small trees, conifers and oaks, grow to full size in about two seconds. A lantern stands one "
	                "step out from each corner. Bats fly over the middle spot. When the hand pulls a tree, that tree and "
	                "the trees of the spots next to it change kind and grow again from small.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 45.0f, 30.0f, k_Puzzle.y - 25.0f},
	                .look = {k_Puzzle.x + 10.0f, 3.0f, k_Puzzle.y + 10.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 0.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_TreeKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}},
	});
}

void AddStory(std::vector<Scenario>& all)
{
	// The player's temple west of the middle, turned as Land 1's
	constexpr glm::vec2 k_Temple {-300.0f, 100.0f};
	constexpr float k_TempleTurn = 36.0f;
	all.push_back({
	    .id = "story.game_over",
	    .name = "The game's end as the player's temple falls",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle; two seconds in it starts being destroyed, as when "
	                   "its heart loses the last of its life, and the game's GameOver script plays.",
	    .expected = "The temple is destroyed and the script takes the camera round and up from it while the advisors leave. "
	                "The screen fades to black over 12 seconds, and \"Game Over\" fades in white over 3 seconds, centred in a "
	                "box from a quarter to three quarters across the screen and 45% to 55% down, its line 7.5% of the "
	                "screen high. Once the advisor's line is read the game's time stops, the words fade out over 5 "
	                "seconds, and the player is taken into the temple's save game room, kept there: leaving it ends the "
	                "game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 2.0f, .value = 0}},
	});
	all.push_back({
	    .id = "temple.heart_fade",
	    .name = "The temple's heart glows and fades out in its destruction",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, near the camera. Two seconds in, its destruction "
	                   "starts, as when its heart loses the last of its life.",
	    .expected = "From 5.5 seconds the temple glows in red, the player's colour, and the glow beats with the heartbeat. "
	                "From 16 seconds the temple fades out over four seconds. A little into the fade it writes no depth: "
	                "the land and the things behind it show through it, and its cut-out parts go first. From 16 seconds a "
	                "faint shell of light in red covers the heart, reaching out with the heartbeat and growing at the "
	                "end. Where the shell touches the heart it adds a faint grey. It is not drawn after 23.8 seconds, "
	                "just before it goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {25.0f}, k_Temple + glm::vec2 {25.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 2.0f, .value = 0}},
	});
	{
		// The scripts' delete with explode and with temple explode, as the later lands' scripts use them
		using Argument = ScriptArgument;
		const auto remove = [](float delay, Argument thing, float mode) {
			return Command {.kind = Kind::CallNative,
			                .delaySeconds = delay,
			                .script = "OBJECT_DELETE",
			                .scriptArguments = {thing, {.kind = Argument::Kind::Integer, .number = mode}}};
		};
		const Argument hut {.kind = Argument::Kind::Object, .index = 0};
		const Argument temple {.kind = Argument::Kind::Temple, .index = 0};
		all.push_back({
		    .id = "temple.script_breaks_up",
		    .name = "A script breaks up a hut and a rival's temple",
		    .facet = Facet::Story,
		    .description = "A rival's temple stands west of the middle with a hut in front of it. One second in, a script "
		                   "deletes the temple with temple explode. Two seconds later it deletes the hut with explode. 20 "
		                   "seconds after the first delete, it deletes the temple with temple explode again, as the last "
		                   "land's fight does.",
		    .expected = "The temple starts its destruction: the loop sound, the beams across it and the glow. The hut "
		                "breaks into pieces of its model that fly out from where it stood, and the hut is gone. At the second "
		                "delete, the model of the temple breaks into many pieces that fly out fast, while the temple "
		                "itself stays. Two seconds later the temple explodes and goes, as its destruction ends.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview,
		                .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}},
		                .distance = 1.0f},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_Temple + glm::vec2 {0.0f, -45.0f}}},
		    .temples = {{.offset = k_Temple, .owner = PlayerNames::PLAYER_TWO, .angle = k_TempleTurn}},
		    .commands = {remove(1.0f, temple, 3.0f), remove(2.0f, hut, 2.0f), remove(18.0f, temple, 3.0f)},
		});
	}
	all.push_back({
	    .id = "temple.intro_movement",
	    .name = "Looking about the temple while the advisors introduce it",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle; after a second the player goes into it for the "
	                   "first time, and the advisors' introduction of its main room plays. Turn and tilt the room with the "
	                   "camera's keys, move the hand about, and click and drag while they talk.",
	    .expected = "The camera flies in from the entrance to the main room by itself while the good and evil advisors "
	                "come out and talk through their nine lines. Throughout, the hand follows the cursor over the room "
	                "and the camera's keys turn and tilt the room as they do after the scene. Clicks and drags do nothing "
	                "until the scene is over: the flight isn't cut short and the room isn't dragged round.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    // Room 3 is the main room, the one the entrance leads to
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3}},
	});
	all.push_back({
	    .id = "temple.entrance_pick",
	    .name = "The hand over the player's temple entrance",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, fully built. The pointer rests on the land beside "
	                   "it, then over the temple's walls, then over its entrance for a few seconds, and then the right "
	                   "mouse button is clicked there.",
	    .expected = "Over the land and the walls the hand offers nothing about the temple but its health. Over the "
	                "entrance it picks the entrance (not the temple behind it) and offers \"Enter Temple\" with the "
	                "right mouse button. The click takes the player inside, the camera flying in from the entrance.",
	    .environment = {.dispenserGrid = false},
	    // From the west, where its entrance faces, a little above the land
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Temple.x - 90.0f, 40.0f, k_Temple.y},
	                .look = {k_Temple.x, 10.0f, k_Temple.y}},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.2f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.36f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.447f, 0.604f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	all.push_back({
	    .id = "temple.damage_beam",
	    .name = "A hurt temple's heart beaming across itself",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle with its heart at 30% of its life and no town to "
	                   "pass blows on to.",
	    .expected = "Every 1.2 seconds (12 turns) a plasma beam leaps between two points of the heart's model, leaving "
	                "and arriving along the model's surface, lasting 1.18 seconds and fairly faint (alpha 76 of 255). "
	                "Each beam sounds one of the five heart beam sounds in turn, from the heart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn, .life = 0.3f}},
	});
	all.push_back({
	    .id = "audio.temple_heartbeat",
	    .name = "The heartbeat heard from the player's temple",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, fully built, with no danger to the player's "
	                   "people. The camera looks at it from less than 140 m.",
	    .expected = "From the first turn a heartbeat sounds from the temple without a stop, heard to 500 m and fully to a "
	                "third of that. It plays at 30% of its rate, as the heart is calm, and the temple's column of light "
	                "throbs with the same beat. It starts only once: it is not heard twice over. It starts only with the "
	                "camera within 140 m of the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {25.0f}, k_Temple + glm::vec2 {25.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	});
	// The player's temple of the second land with its Norse town, an Indian town that the player won and the Indian town
	// nearest the temple, at the same distances and directions from the temple as on the second land
	constexpr std::string_view k_SecondLandSites = R"(CREATE_TOWN(950, "-129.73,154.60", "PLAYER_ONE", 7, "NORSE")
SET_TOWN_BELIEF(950, "PLAYER_ONE", 1.0)
CREATE_ABODE(950, "-139.73,164.60", "NORSE_ABODE_B", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-135.73,160.60", "-139.73,164.60", "NORSE_FORESTER", 20)
CREATE_VILLAGER_POS("-135.73,160.60", "-139.73,164.60", "NORSE_HOUSEWIFE", 20)
CREATE_TOWN(951, "-599.77,-138.91", "PLAYER_ONE", 4, "INDIAN")
SET_TOWN_BELIEF(951, "PLAYER_ONE", 1.0)
CREATE_ABODE(951, "-609.77,-128.91", "INDIAN_ABODE_A", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-605.77,-132.91", "-609.77,-128.91", "INDIAN_FORESTER", 20)
CREATE_VILLAGER_POS("-605.77,-132.91", "-609.77,-128.91", "INDIAN_HOUSEWIFE", 20)
CREATE_TOWN(952, "-498.68,206.42", "NEUTRAL", 4, "INDIAN")
CREATE_ABODE(952, "-508.68,216.42", "INDIAN_ABODE_A", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-504.68,212.42", "-508.68,216.42", "INDIAN_FORESTER", 20)
CREATE_CITADEL("0.0,0.0", 0, "PLAYER_ONE", 4254, 1000)
)";
	all.push_back({
	    .id = "worship.site_places_second_land",
	    .name = "The places of the player's worship sites around the temple of the second land",
	    .facet = Facet::Story,
	    .description = "Lines of a land put the player's temple, turned as on the second land, in the middle. The player "
	                   "has a Norse town to the north-west and an Indian town far to the west. A neutral Indian town is "
	                   "the Indian town nearest the temple. Each town of the player gets its site as the land is laid "
	                   "out, and the sites are built at once.",
	    .expected = "The Norse site takes the first place around the temple, with its altar about 28 m west of the "
	                "temple. The Indian site takes the next place to the south, with its altar 18 m west and 22 m south "
	                "of the temple: there the scripts of the second land look for the altar of the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {glm::vec2 {-60.0f, -50.0f}, glm::vec2 {30.0f, 40.0f}},
	                .distance = 1.0f},
	    .landScript = k_SecondLandSites,
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 0.5f}},
	});
	AddBreederOffers(all);
	AddPiedPiperRound(all);
	AddOgreLair(all);
	AddGuideStorm(all);
	AddRivalCreatureLoads(all);
	AddSingingStones(all);
	AddBeachTemplePuzzle(all);
	AddTreePuzzle(all);
}

void AddExplorersBoat(std::vector<Scenario>& all)
{
	// The dry dock on the testbed lake's east bank, so that the boat is pushed west into the water
	constexpr glm::vec2 k_Dock {58.0f, 220.0f};
	all.push_back({
	    .id = "story.explorers_boat",
	    .name = "The explorers' boat pushed into the sea and sailing away",
	    .facet = Facet::Story,
	    .description = "The film the Land 1 silver scroll of the explorers ends with, played on the testbed after a second: "
	                   "its dry dock stands on the lake's east bank and its sailing starts in the lake's north-east corner "
	                   "(on Land 1 the boat sails out to sea, 425 metres west of the beach).",
	    .expected = "Five sailors push the boat for three seconds, then it slides west down the bank into the lake with a "
	                "creak, a splash and a big splash, throwing up brown sand and then white water; the sailors stop "
	                "pushing as it goes, three worn out and two cheering. About eighteen seconds in it is at sea: two cows "
	                "eat beside a pile of grain, a man and a woman stand arms out at the bow, two sailors sit swinging "
	                "their legs, one scrubs the deck, five wake sprites run back from it, and it sails south west at five "
	                "metres a second for a minute, then is gone. Out of the lake it sails under the testbed's flat land.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Dock - glm::vec2 {110.0f, 70.0f}, k_Dock + glm::vec2 {10.0f, 60.0f}},
	                .distance = 1.0f},
	    .commands = {{.kind = Kind::PlayBoatFilm, .delaySeconds = 1.0f, .point = k_Dock}},
	});
	all.push_back({
	    .id = "story.explorers_boat_deck_light",
	    .name = "The deck of the explorers' boat in the light of the boat's place",
	    .facet = Facet::Story,
	    .description = "The same film as story.explorers_boat, framed on the lake that the boat crosses at sea. Each thing "
	                   "on the deck takes the land's light of the place of the boat, and no colour of the land.",
	    .expected = "About eighteen seconds in, the boat sails across the lake. The cows, the grain and the six people "
	                "on its deck show the same light as the boat at its middle, also where they stand over the bank. "
	                "No colour of the land under a part shows on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Dock - glm::vec2 {170.0f, 150.0f}, k_Dock - glm::vec2 {40.0f, 20.0f}},
	                .distance = 1.0f},
	    .commands = {{.kind = Kind::PlayBoatFilm, .delaySeconds = 1.0f, .point = k_Dock}},
	});
}

void AddAppearance(std::vector<Scenario>& all)
{
	constexpr std::array<CreatureType, 3> k_Species {CreatureType::GiantApe, CreatureType::Mandrill, CreatureType::Horse};
	constexpr std::array<float, 3> k_Alignments {-1.0f, 0.0f, 1.0f};
	constexpr std::array<std::string_view, 9> k_Labels {"evil ape",      "neutral ape",      "good ape",
	                                                    "evil mandrill", "neutral mandrill", "good mandrill",
	                                                    "evil horse",    "neutral horse",    "good horse"};
	std::vector<CreatureSetup> skins;
	for (size_t row = 0; row < k_Species.size(); ++row)
	{
		for (size_t column = 0; column < k_Alignments.size(); ++column)
		{
			auto creature = Content(k_Species.at(row), {RowX(column, 3, 45.0f), static_cast<float>(row) * 45.0f}, 0.0f,
			                        k_Labels.at((row * 3) + column));
			creature.alignment = k_Alignments.at(column);
			creature.pauseMind = true;
			skins.push_back(creature);
		}
	}
	all.push_back({
	    .id = "appearance.alignment_skins",
	    .name = "Alignment skins and hair",
	    .facet = Facet::Appearance,
	    .description = "Giant apes, mandrills and horses, a row of each from front to back, evil, neutral and good from "
	                   "left to right.",
	    .expected = "Each skin blends towards its evil or good skin with the alignment, the bodies follow it, and the "
	                "hair of the hairy species is drawn over them (the spawner's Show hair turns it off).",
	    .framing = {.shot = Shot::Overview},
	    .creatures = skins,
	});

	auto goodTiger = Content(CreatureType::Tiger, {-12.0f, 0.0f}, 0.0f, "good");
	goodTiger.alignment = 1.0f;
	goodTiger.pauseMind = true;
	auto evilTiger = Content(CreatureType::Tiger, {12.0f, 0.0f}, 0.0f, "evil");
	evilTiger.alignment = -1.0f;
	evilTiger.pauseMind = true;
	all.push_back({
	    .id = "appearance.alignment_motes",
	    .name = "Good motes and evil smoke",
	    .facet = Facet::Appearance,
	    .description = "Close up, a good tiger on the left and an evil tiger on the right stand side by side.",
	    .expected = "Pale motes of six colours twinkle all over the good tiger, three new ones each game turn; they "
	                "pulse, flicker and fade in two seconds. Dark smoke rises off the evil tiger and fades in two "
	                "seconds. A paused game makes no new motes or smoke.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -15.0f}, {30.0f, 15.0f}}, .distance = 1.0f},
	    .creatures = {goodTiger, evilTiger},
	});

	// A tiger as strong as it can be, whose strength only fades. A weak tiger beside it shows the weak look.
	auto fading = Content(CreatureType::Tiger, {-12.0f, 0.0f}, 0.0f, "strength fades");
	fading.strength = 1.0f;
	fading.pauseMind = true;
	auto weak = Content(CreatureType::Tiger, {12.0f, 0.0f}, 0.0f, "weak");
	weak.strength = 0.0f;
	weak.pauseMind = true;
	all.push_back({
	    .id = "appearance.strength_fades",
	    .name = "The body follows a strength that fades",
	    .facet = Facet::Appearance,
	    .description = "Close up, a tiger with the most strength stands on the left, and a tiger with no strength stands "
	                   "on the right. Nothing changes their strength by an amount.",
	    .expected = "The strength of the left tiger fades a little each game turn, and at the end of each turn its body "
	                "takes the look of that strength. Run the game fast: the left tiger becomes less muscular. Its "
	                "strong look does not stay.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -15.0f}, {30.0f, 15.0f}}, .distance = 1.0f},
	    .creatures = {fading, weak},
	});

	all.push_back({
	    .id = "appearance.falling_spell_film",
	    .name = "The falling spell's film",
	    .facet = Facet::Appearance,
	    .description = "The player's giant ape stands on the land. After a second, the falling spell's film plays as the "
	                   "Land 5 script starts it.",
	    .expected = "The world goes and the film plays. A copy of the ape stands in the middle of the film and plays its "
	                "fall action once, on the right side, as the film's camera moves round it. It glows white at first, "
	                "then orange over a dark red that flickers. Its hands glow white, and the glows become larger at about "
	                "16 s and 17 s. Orange stars fall and fade from 13.45 s. It goes from 19.55 s to 27.35 s, then comes "
	                "back white. From 37.75 s a burst of coloured rays turns round it and grows. The white fade at the end "
	                "covers it, and the copy goes with the film.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {Command {.kind = Kind::PlayFallingSpellFilm, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "interface.film_surface",
	    .name = "The 16-bit colours of a film",
	    .facet = Facet::Interface,
	    .description = "After a second, the intro film plays full screen. Each picture goes through the 16-bit surface "
	                   "that the game uses on the display cards of today. The Videos window changes the surface.",
	    .expected = "The sky and the sea of the film show bands of colour, as the game shows them. Each channel "
	                "keeps 32 levels. The 565 surface keeps 64 levels of green. The None surface shows smooth colours.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {Command {.kind = Kind::PlayIntroFilm, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "appearance.creature_hand_glows",
	    .name = "The glows on a creature's hands",
	    .facet = Facet::Appearance,
	    .description = "The player's giant ape stands on the land. After a second, it gets the four glows that the creed "
	                   "powers use: one on each hand, one on its belly and one above its body.",
	    .expected = "A white glowing shell shows round each hand and the belly, and one turns slowly above the head. Each "
	                "texture slides. The glows move with the ape's pose.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {Command {.kind = Kind::GiveHandGlows, .delaySeconds = 1.0f, .value = 2}},
	});

	all.push_back({
	    .id = "appearance.creed_glow",
	    .name = "A creature gets a creed power",
	    .facet = Facet::Appearance,
	    .description = "The player's giant ape stands on the land. After a second, a script gives it a weak creed power "
	                   "above its body. Two seconds later, the script gives it the full power, as the story does when "
	                   "the creature gets a part of the creed.",
	    .expected = "A small white glowing shell turns slowly above the head of the ape, and the ape starts to hum. Then the "
	                "shell becomes larger. The hum continues.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {Command {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 0.1f},
	                 Command {.kind = Kind::GiveCreedPower, .delaySeconds = 2.0f, .value = 3, .amount = 1.0f}},
	});

	all.push_back({
	    .id = "audio.creed_hum_with_script_sound",
	    .name = "A creature with a script's sound gets a creed power",
	    .facet = Facet::Audio,
	    .description = "The player's giant ape stands on the land. After a second, a script attaches the village bell "
	                   "sound to the ape. A second later, a script gives the ape the full creed power above its body. "
	                   "The ape keeps up the two sounds at the same time, each from its own tag.",
	    .expected = "The bell rings from the ape again and again. Then a white glowing shell turns above the head of the "
	                "ape, and the ape starts to hum. The bell continues while the ape hums.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {Command {.kind = Kind::AttachScriptSound,
	                          .delaySeconds = 1.0f,
	                          .value = static_cast<size_t>(audio::SoundId::G_VillageBell)},
	                 Command {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f}},
	});

	// Sounds on objects follow the objects: a creature that hums as it walks, a burning villager, and a ball with a sound
	// in the hand
	all.push_back({
	    .id = "audio.sounds_follow_objects",
	    .name = "Sounds on objects follow the objects",
	    .facet = Facet::Audio,
	    .description = "The player's giant ape gets the full creed power and walks from the west to the east. A villager "
	                   "burns and runs. A script attaches the village bell sound to a ball. The hand takes the ball and "
	                   "moves it from the left of the screen to the right. Then the hand puts the ball down.",
	    .expected = "The hum and the footsteps of the ape move from the left speaker to the right speaker as the ape "
	                "walks. The fire moves with the villager. The bell moves with the hand, also when the hand holds the "
	                "ball. Each sound changes its place one time each game turn, ten times each second.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = 0.55f},
	    .creatures = {Posed(CreatureType::GiantApe, {-40.0f, 10.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -10.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {20.0f, -30.0f}, .alight = true}},
	    .commands = {Command {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f},
	                 Go(Kind::WalkTo, 0, {40.0f, 10.0f}, 1.0f, false),
	                 Command {.kind = Kind::AttachObjectScriptSound,
	                          .delaySeconds = 8.0f,
	                          .value = static_cast<size_t>(audio::SoundId::G_VillageBell)},
	                 Command {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 Command {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.1f, 0.5f}},
	                 Command {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.9f, 0.5f}},
	                 Command {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.6f}},
	                 Command {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	});

	// A script's spoken line from a point on the land, the same kind of line heard everywhere, and an advisor's line
	all.push_back({
	    .id = "audio.say_lines_and_advisor",
	    .name = "Spoken lines of scripts and advisors",
	    .facet = Facet::Audio,
	    .description = "After a second, a script says a villager's line from a point on the land to the east of the "
	                   "camera. Six seconds later, a script says the next line with no point. Six seconds later, a "
	                   "script shows a help text that the good advisor says. No bank sample of the game locks an axis "
	                   "of its position, so no sound here does.",
	    .expected = "The first line comes from the right speaker and stays at its point. The second line is centred and "
	                "has the same volume wherever the camera is. The good advisor's line is centred and does not move "
	                "with the advisor.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}},
	    .commands = {{.kind = Kind::SayLine, .delaySeconds = 1.0f, .point = {40.0f, 0.0f}, .value = 1250, .ctrl = true},
	                 {.kind = Kind::SayLine, .delaySeconds = 6.0f, .value = 1251},
	                 {.kind = Kind::RunText, .delaySeconds = 6.0f, .value = 1009}},
	});

	// Guidance voices whose callers give a volume and a pitch, with bank headers that do and do not set them
	constexpr size_t k_AttackVoice = 130;
	constexpr size_t k_DeathLine = 1;
	constexpr size_t k_DiscipleLine = 6;
	all.push_back({
	    .id = "audio.header_wins_over_callers",
	    .name = "Bank headers win over the volume and pitch of callers",
	    .facet = Facet::Audio,
	    .description = "Every five seconds, a guidance voice plays with volume 127 and pitch 100, as the guidance's voices "
	                   "do: a villager's attack voice (its bank header gives volume 50 and 10% pitch change), a death "
	                   "line (only the 10% pitch change), a disciple line (neither), then the attack voice twice from a "
	                   "point 40 metres to the east, heard to 200 metres. The audio log gives the volume, the pitch and "
	                   "the distances of each sound.",
	    .expected = "The attack voice starts at volume 50, the other two lines at volume 127. The attack voice and the "
	                "death line start at a pitch from 90% to 110%, which changes on each play; the disciple line starts "
	                "at 100%. The placed attack voice comes from the right speaker, has volume 50, and has the distances "
	                "66.6 and 200 metres that its caller gives, not the 15 and 150 metres of its bank header.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PlayGuidanceVoice, .delaySeconds = 1.0f, .value = k_AttackVoice},
	                 {.kind = Kind::PlayGuidanceVoice, .delaySeconds = 5.0f, .value = k_DeathLine},
	                 {.kind = Kind::PlayGuidanceVoice, .delaySeconds = 5.0f, .value = k_DiscipleLine},
	                 {.kind = Kind::PlayGuidanceVoice,
	                  .delaySeconds = 5.0f,
	                  .point = {40.0f, 0.0f},
	                  .value = k_AttackVoice,
	                  .radius = 200.0f,
	                  .ctrl = true},
	                 {.kind = Kind::PlayGuidanceVoice,
	                  .delaySeconds = 5.0f,
	                  .point = {40.0f, 0.0f},
	                  .value = k_AttackVoice,
	                  .radius = 200.0f,
	                  .ctrl = true}},
	});

	// The hair close up while the head moves: the tiger's whiskers, and the wolf's tufts, which use the same material
	std::vector<Command> headMoves;
	for (size_t i = 0; i < animations::k_GestureCount; ++i)
	{
		headMoves.push_back(Play(Kind::PlayGesture, 0, animations::k_FirstGesture + i, 3.0f, false));
	}
	// The light on the bodies of creatures by day and at night, while they move
	const auto bodyLight = [&all, &headMoves](std::string_view id, std::string_view name, float hour,
	                                          std::string_view description, std::string_view expected) {
		auto ape = Content(CreatureType::GiantApe, {-12.0f, 0.0f}, 0.0f, "ape");
		ape.pauseMind = true;
		auto cow = Content(CreatureType::Cow, {12.0f, 0.0f}, 0.0f, "cow");
		cow.pauseMind = true;
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Appearance,
		    .description = description,
		    .expected = expected,
		    .environment = {.hour = hour, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-18.0f, -4.0f}, {18.0f, 4.0f}}, .distance = 0.6f},
		    .creatures = {ape, cow},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	};
	bodyLight("appearance.creature_body_light_day", "Creature body light by day", 12.0f,
	          "At noon, the clock stopped, a giant ape and a cow stand side by side. They nod, shake their heads and do "
	          "their other gestures.",
	          "The sun lights each body as the game lights the bodies it poses. The side away from the sun keeps two fifths "
	          "of the light. The side that faces the sun is fully lit, also where it faces the sun only a little.");
	bodyLight("appearance.creature_body_light_night", "Creature body light at night", 23.0f,
	          "At eleven at night, the clock stopped, a giant ape and a cow stand side by side. They nod, shake their heads "
	          "and do their other gestures.",
	          "The light that the hand carries at night lights each body as the game lights the bodies it poses. The side "
	          "away from the hand keeps two fifths of the light. The eyes keep the usual light of the models.");
	// The player's creature mirrored in the lake, beside a creature of no player that the game does not mirror
	auto mirroredApe = Posed(CreatureType::GiantApe, {k_Lake.x - 15.0f, k_FarShallows}, 0.0f, "player's ape");
	auto unmirroredCow = Posed(CreatureType::Cow, {k_Lake.x + 15.0f, k_FarShallows}, 0.0f, "cow of no player");
	unmirroredCow.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "appearance.creature_sea_reflection",
	    .name = "Creature reflection in the sea",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the player's giant ape and a cow of no player stand in the far "
	                   "shallows of the testbed's lake, seen across the water from the south. The ape nods, shakes its "
	                   "head and does its other gestures.",
	    .expected = "The ape shows upside down in the water in front of it and moves as it moves. Its reflection is one "
	                "flat blue grey colour over its skin, with no light or shade, and without its eyes or hair. The cow "
	                "shows no reflection.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {mirroredApe, unmirroredCow},
	    .commands = headMoves,
	    .repeatFrom = 0,
	});
	// The player's hand mirrored in the lake with the villager that it holds, beside the player's creature
	all.push_back({
	    .id = "appearance.hand_sea_reflection",
	    .name = "Hand reflection in the sea",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the camera looks down over the lake of the testbed near its far "
	                   "shallows, with the player's hand over the water. After a second the hand takes a villager. The "
	                   "player's giant ape stands in the shallows near the hand.",
	    .expected = "The hand shows upside down in the water under it, in one flat grey with no light, shade or glow, and "
	                "only its outside. The villager that it holds shows in the water with it. The reflections of the hand "
	                "and the ape show over the reflection of the land.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.2f},
	    .creatures = {Posed(CreatureType::GiantApe, {k_Lake.x - 8.0f, k_FarShallows}, 0.0f, "player's ape")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {k_Lake.x + 10.0f, k_FarShallows + 10.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	});
	// The player's creature frozen in the lake: the game mirrors its body without the ice
	all.push_back({
	    .id = "appearance.frozen_creature_sea_reflection",
	    .name = "Frozen creature reflection in the sea",
	    .facet = Facet::Appearance,
	    .description =
	        "At noon, the clock stopped, the player's giant ape stands in the far shallows of the testbed's "
	        "lake, seen from close by across the water from the south. After a second the freeze spell is cast on it.",
	    .expected = "The ape darkens to an icy blue with a sheen of ice. Its reflection in the water stays one flat blue "
	                "grey colour, with no icy tint and no sheen of ice, before, during and after the freeze.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 20.0f}}, .distance = 0.35f},
	    .creatures = {Posed(CreatureType::GiantApe, {k_Lake.x, k_FarShallows}, 0.0f, "player's ape")},
	    .miracles = {{.type = MagicType::CreatureSpellFreeze,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 0,
	                  .delaySeconds = 1.0f}},
	});
	// The land, a hut, trees and the sky as the lowest detail levels draw them, with textures of half the size
	all.push_back({
	    .id = "appearance.low_resolution_textures",
	    .name = "Low resolution textures",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the camera looks north over a hut and two oaks to the far land of the "
	                   "testbed, with the sky above it. Start the game with --detail-level 1 (Low) or 0 (Minimum) to see "
	                   "the low resolution textures, and with 4 to compare.",
	    .expected = "At Low the land, its footprints and the shadows of the hut and the oaks are softer, painted at half "
	                "the texels. The shadows are half as dark. The hut, the oaks and the sea have softer textures. The "
	                "sky is the day's sky of the alignment and does not change with the time of day. The far land "
	                "fades into its coarse texture as at Maximum.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -90.0f}, .look = {0.0f, 0.0f, 60.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-15.0f, -40.0f}},
	                {.type = TreeInfo::Oak, .offset = {15.0f, -45.0f}},
	                {.type = TreeInfo::Oak, .offset = {25.0f, -30.0f}}},
	});
	// The player's lion mirrored in the lake: its one chroma part keeps the alpha of its texture
	all.push_back({
	    .id = "appearance.lion_sea_reflection",
	    .name = "Lion reflection in the sea",
	    .facet = Facet::Appearance,
	    .description =
	        "At noon, the clock stopped, the player's lion stands in the far shallows of the testbed's lake, "
	        "seen from close by across the water from the south. The lion nods, shakes its head and does its other gestures.",
	    .expected = "The lion shows upside down in the water in one flat blue grey colour. Its part with the cut-out "
	                "edge shows the same cut-out edge in the reflection as on the lion.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 20.0f}}, .distance = 0.35f},
	    .creatures = {Posed(CreatureType::Lion, {k_Lake.x, k_FarShallows}, 0.0f, "player's lion")},
	    .commands = headMoves,
	    .repeatFrom = 0,
	});
	// The thing that the hand holds, mirrored in the lake with the hand: flat in the light that it took in the hand
	all.push_back({
	    .id = "appearance.held_thing_sea_reflection",
	    .name = "Held thing reflection in the sea",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down over the lake of the testbed "
	                   "near its far shallows, with the player's hand over the water. After a second the hand takes a "
	                   "barrel.",
	    .expected = "The barrel shows upside down in the water under the hand. The low sun shades the barrel in the hand, "
	                "but its reflection is flat, in the light of the land under it, with no shade.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.2f},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {k_Lake.x + 10.0f, k_FarShallows + 10.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	});
	// A tree in the hand, mirrored in the lake with the hand: in the light of its place on the map, where the hand took it
	all.push_back({
	    .id = "appearance.held_tree_sea_reflection",
	    .name = "Held tree reflection in the sea",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down over the lake of the testbed near "
	                   "its far shallows, with the player's hand over the water. After a second the hand takes a young oak "
	                   "from the bank.",
	    .expected = "The oak shows upside down in the water under the hand, with the hand's reflection. Its reflection is "
	                "flat, in the light of the bank where the hand took it, and not in the light of the land under the "
	                "hand.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.3f},
	    .objects = {{.type = TreeInfo::Oak, .offset = {k_Lake.x + 10.0f, k_FarShallows + 14.0f}, .fullSize = 0.4f}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	});
	// A burning barrel, then a pot of poisoned food, in the hand over the lake: the reflection carries the colours of the
	// fire and of the poison
	all.push_back({
	    .id = "appearance.held_burning_and_poisoned_sea_reflection",
	    .name = "Held burning and poisoned things in the sea reflection",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down over the lake of the testbed near "
	                   "its far shallows, with the player's hand over the water. After a second the hand takes a burning "
	                   "barrel. Six seconds later it lets the barrel go and takes a pot of poisoned food.",
	    .expected = "The reflection of the burning barrel is darker by its charring and flickers with the red glow of the "
	                "fire, as the barrel in the hand does. The reflection of the poisoned pot is a little green.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.2f},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel,
	                 .offset = {k_Lake.x + 10.0f, k_FarShallows + 10.0f},
	                 .alight = true},
	                {.type = PotInfo::FoodPot, .offset = {k_Lake.x - 10.0f, k_FarShallows + 10.0f}, .poisoned = true}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::HandRelease, .delaySeconds = 6.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 1}},
	});
	// A pot of poisoned food beside a clean one on the bank, then the poisoned pot in the hand
	all.push_back({
	    .id = "appearance.poisoned_pot_tint",
	    .name = "Poisoned pot tint on the ground and in the hand",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down at the bank of the lake of the "
	                   "testbed. A pot of poisoned food stands beside a clean pot of food. After three seconds the hand "
	                   "takes the poisoned pot.",
	    .expected = "The poisoned pot is a little green beside the clean pot, on the ground and in the hand.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows + 6.0f}}, .distance = 0.15f},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {k_Lake.x - 4.0f, k_FarShallows + 10.0f}, .poisoned = true},
	                {.type = PotInfo::FoodPot, .offset = {k_Lake.x + 4.0f, k_FarShallows + 10.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	});
	// A burning pot in the hand over the lake: it glows as a burning barrel does
	all.push_back({
	    .id = "appearance.burning_pot_in_hand",
	    .name = "Burning pot in the hand",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down over the lake of the testbed "
	                   "near its far shallows. After a second a pot of wood on the bank catches fire. A second later the "
	                   "hand takes it. A pot burns out fast, so it is set alight again in the hand every two seconds.",
	    .expected = "The burning pot flickers with the red glow of the fire and goes darker as it chars, on the ground "
	                "and in the hand, as a burning barrel does.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x + 6.0f, k_FarShallows + 40.0f}}, .distance = 0.15f},
	    .objects = {{.type = PotInfo::WoodPot, .offset = {k_Lake.x + 6.0f, k_FarShallows + 44.0f}}},
	    .commands = {{.kind = Kind::SetAlight, .delaySeconds = 1.0f, .value = 2, .object = 0},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 1.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0}},
	});
	// Two burning trees: one stays on the bank, the hand holds the other over the lake
	all.push_back({
	    .id = "appearance.held_burning_tree_sea_reflection",
	    .name = "Held burning tree in the sea reflection",
	    .facet = Facet::Appearance,
	    .description = "At four in the afternoon, the clock stopped, the camera looks down over the lake of the testbed "
	                   "near its far shallows, with the player's hand over the water. After a second two young oaks on the "
	                   "bank catch fire. Four seconds later the hand takes one of them, which is set alight again every "
	                   "two seconds.",
	    .expected = "In the hand, the burning oak is grey and its leaves become thin as it burns. Its reflection in the "
	                "water is grey too, but the reflection keeps all of its leaves and its full width.",
	    .environment = {.hour = 16.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.4f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 20.0f}}, .distance = 0.3f},
	    .objects = {{.type = TreeInfo::Oak, .offset = {k_Lake.x + 10.0f, k_FarShallows + 26.0f}, .fullSize = 0.4f},
	                {.type = TreeInfo::Oak, .offset = {k_Lake.x - 10.0f, k_FarShallows + 26.0f}, .fullSize = 0.4f}},
	    .commands = {{.kind = Kind::SetAlight, .delaySeconds = 1.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 0.0f, .value = 2, .object = 1},
	                 {.kind = Kind::HandTake, .delaySeconds = 4.0f, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 1.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0},
	                 {.kind = Kind::SetAlight, .delaySeconds = 2.0f, .value = 2, .object = 0}},
	});
	// Sharks under the lake near the far shore, with the player's creature and the hand mirrored in the water over them
	all.push_back({
	    .id = "appearance.sharks_under_sea_reflection",
	    .name = "Sharks under the sea reflection",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the camera looks across the lake of the testbed to its far shallows. "
	                   "The player's giant ape stands in the shallows and the hand holds a villager over the water. Three "
	                   "sharks lie under the water: one under the ape's reflection, one under the hand's reflection and one "
	                   "near the bank, under the reflection of the land.",
	    .expected = "Each shark shows dark blue under the water. The shark near the bank shows over the reflection of the "
	                "land. The reflections of the ape and the hand hide the parts of the sharks that are behind them.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.45f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.3f},
	    .creatures = {Posed(CreatureType::GiantApe, {k_Lake.x - 8.0f, k_FarShallows}, 0.0f, "player's ape")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {k_Lake.x + 10.0f, k_FarShallows + 10.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0}},
	    .sharks = {{k_Lake.x - 8.0f, k_FarShallows - 14.0f},
	               {k_Lake.x + 4.0f, k_FarShallows - 22.0f},
	               {k_Lake.x + 22.0f, k_FarShallows - 2.0f}},
	});
	// A villager drawn in high detail swims in the shallows, as the boy swims in the opening
	all.push_back({
	    .id = "appearance.swimmer_under_sea",
	    .name = "A swimmer in high detail under the sea",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the camera looks across the lake of the testbed to its far shallows. "
	                   "The cinema bars come in. A villager in the shallows changes to high detail and plays the clip of "
	                   "the swimmer.",
	    .expected = "The part of the villager that is under the water shows dark blue through the water. The part above "
	                "the water shows as usual. No rings come on the water near the villager.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 4.0f}}, .distance = 0.08f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {k_Lake.x, k_FarShallows - 4.0f}}},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "SET_HIGH_GRAPHICS_DETAIL",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "OVERRIDE_STATE_ANIMATION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 390.0f}}}},
	});
	// The eyes close up by day and at night, with the glass environment map added over each eyeball
	const auto eyesClose = [&all, &headMoves](std::string_view id, std::string_view name, float hour,
	                                          std::string_view description, std::string_view expected) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Appearance,
		    .description = description,
		    .expected = expected,
		    .environment = {.hour = hour, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Head, .distance = 0.6f},
		    .creatures = {Posed(CreatureType::GiantApe, {0.0f, 0.0f})},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	};
	eyesClose("appearance.creature_eyes_glass_day", "Creature eyes close up by day", 12.0f,
	          "At noon, the clock stopped, a giant ape with a paused mind faces the camera close up. It nods, shakes its "
	          "head and does its other gestures.",
	          "Each eyeball shines with the glass environment map added over its lit texture. The shine moves over the "
	          "eyeball as the head turns, as if the eye reflects the sky.");
	eyesClose("appearance.creature_eyes_glass_night", "Creature eyes close up at night", 23.0f,
	          "At eleven at night, the clock stopped, a giant ape with a paused mind faces the camera close up. It nods, "
	          "shakes its head and does its other gestures.",
	          "Each eyeball shines with the glass environment map. The night does not dim the shine, because it is "
	          "added after the light, so the eyes look bright in the dark face.");
	const auto hairClose = [&all, &headMoves](std::string_view id, std::string_view name, CreatureType species,
	                                          std::string_view description, std::string_view expected, float distance) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Appearance,
		    .description = description,
		    .expected = expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Head, .distance = distance},
		    .creatures = {CreatureSetup {.species = species, .needs = Content(), .hold = true, .pauseMind = true}},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	};
	hairClose("appearance.tiger_whiskers", "Tiger whiskers close up", CreatureType::Tiger,
	          "Close up, a neutral tiger with a paused mind faces the camera. It nods, shakes its head and does its "
	          "other gestures.",
	          "The whiskers are thin white lines with sharp edges and no pale blur. A neutral tiger's whiskers do not "
	          "swing: they only follow the head. The face does not show through them.",
	          0.6f);
	hairClose("appearance.ape_hair", "Giant ape hair close up", CreatureType::GiantApe,
	          "A neutral giant ape with a paused mind faces the camera. It nods, shakes its head and does its other "
	          "gestures.",
	          "The ten strands of hair have sharp edges and no pale blur. They swing when the ape moves.", 1.5f);
	hairClose("appearance.chimp_hair", "Chimp hair close up", CreatureType::Chimp,
	          "A neutral chimp with a paused mind faces the camera. It nods, shakes its head and does its other "
	          "gestures.",
	          "The two tufts of hair (ten strands and two strands) have sharp edges and no pale blur. They swing when "
	          "the chimp moves.",
	          1.5f);
	hairClose("appearance.horse_hair", "Horse hair close up", CreatureType::Horse,
	          "A neutral horse with a paused mind faces the camera. It nods, shakes its head and does its other "
	          "gestures.",
	          "The two tufts of hair (four strands and thirteen strands) have sharp edges and no pale blur. They "
	          "swing when the horse moves.",
	          1.5f);
	hairClose("appearance.mandrill_hair", "Mandrill hair close up", CreatureType::Mandrill,
	          "A neutral mandrill with a paused mind faces the camera. It nods, shakes its head and does its other "
	          "gestures.",
	          "The three tufts of hair (eight, four and six strands) have sharp edges and no pale blur. They swing "
	          "when the mandrill moves.",
	          1.5f);
	hairClose("appearance.wolf_hair", "Wolf hair close up", CreatureType::Wolf,
	          "A neutral wolf with a paused mind faces the camera. It nods, shakes its head and does its other gestures.",
	          "The tufts of hair at the hips have sharp edges and no pale blur. They swing when the wolf moves.", 1.5f);

	// A young lion close up: the long dark strands of its mane are part of the body mesh, not hair. The leopard has
	// no such strands.
	const auto maneClose = [&all, &headMoves](std::string_view id, std::string_view name, CreatureType species,
	                                          std::string_view expected) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Appearance,
		    .description = "Close up, a young neutral creature (size 0.4) with a paused mind faces the camera. It nods, "
		                   "shakes its head and does its other gestures.",
		    .expected = expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Head, .distance = 2.5f},
		    .creatures = {CreatureSetup {
		        .species = species, .size = 0.4f, .needs = Content(), .hold = true, .pauseMind = true}},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	};
	maneClose("appearance.lion_mane", "Young lion mane close up", CreatureType::Lion,
	          "Long dark strands of the mane hang down at the two sides of the face and neck. The land shows between "
	          "the strands, and a little through their edges. They do not swing: they move only with the head and "
	          "the body.");
	maneClose("appearance.leopard_mane", "Young leopard head close up", CreatureType::Leopard,
	          "The leopard has no mane strands, no whiskers and no tufts of hair.");

	auto tattooed = Posed(CreatureType::Tiger, {0.0f, 0.0f}, 30.0f, "tattooed");
	constexpr std::array<glm::u8vec3, 8> k_Colours {{
	    {220, 40, 40},
	    {40, 200, 60},
	    {50, 90, 230},
	    {240, 210, 40},
	    {200, 60, 220},
	    {40, 210, 220},
	    {250, 140, 30},
	    {245, 245, 245},
	}};
	for (size_t site = 0; site < creature_tattoo::k_SlotCount; ++site)
	{
		tattooed.tattoos.push_back({.design = static_cast<uint8_t>(((site * 5) + 1) % creature_tattoo::k_DesignCount),
		                            .site = static_cast<uint8_t>(site),
		                            .colour = k_Colours.at(site)});
	}
	tattooed.wounds = {
	    {.u = 60, .v = 90, .skin = 0, .type = 3, .column = 1},  {.u = 150, .v = 120, .skin = 0, .type = 4, .column = 3},
	    {.u = 200, .v = 70, .skin = 1, .type = 5, .column = 5}, {.u = 90, .v = 180, .skin = 1, .type = 1, .column = 2},
	    {.u = 120, .v = 40, .skin = 2, .type = 3, .column = 6},
	};
	for (uint8_t v = 100; v < 112; ++v)
	{
		tattooed.blood.push_back({.u = 128, .v = v, .skin = 0});
	}
	auto plain = Posed(CreatureType::Tiger, {25.0f, 0.0f}, 330.0f, "unmarked");
	// A second creature of the player would show the tattoos of the player's first creature, as in the game
	plain.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "appearance.tattoos_wounds",
	    .name = "Tattoos and wounds showcase",
	    .facet = Facet::Appearance,
	    .description = "A tiger with a tattoo in every slot, each of a different design and colour, five wounds and "
	                   "burns and a trail of blood, beside an unmarked tiger to compare.",
	    .expected = "The designs sit on the species' tattoo sites in their colours; the wounds start fresh and red and "
	                "heal over time to old scars, the blood running down the skin and fading.",
	    .framing = {.shot = Shot::Follow, .distance = 0.9f},
	    .creatures = {tattooed, plain},
	});

	std::vector<Command> passing {Act(Kind::SitDown, 0, 0.5f)};
	const std::array<glm::vec2, 2> across {glm::vec2 {-45.0f, -30.0f}, glm::vec2 {45.0f, -30.0f}};
	WalkRound(passing, 1, across);
	// Sitting down again once it has got up by itself, so it stays to be looked at
	passing.push_back(Act(Kind::SitDown, 0, 0.0f));
	all.push_back({
	    .id = "appearance.eyes",
	    .name = "Eyes and blinking close-up",
	    .facet = Facet::Appearance,
	    .description = "A tiger, close up, sits facing the camera while a cow walks back and forth in front of it, "
	                   "just behind the camera.",
	    .expected = "It blinks every few seconds; its eyes and head turn to follow the cow from side to side, the eyes "
	                "leading the head.",
	    .framing = {.shot = Shot::Head, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "watcher"),
	                  Posed(CreatureType::Cow, across.at(1), 90.0f, "walker")},
	    .commands = passing,
	    .repeatFrom = 1,
	});

	// A good tiger that only wants to eat, with villagers near it. An evil tiger that only wants to be nice, by a hut of
	// its town that a fireball sets alight. The minds run, and the desires stay as given.
	auto eater = Content(CreatureType::Tiger, {-60.0f, 0.0f}, 0.0f, "good, hungry");
	eater.alignment = 0.6f;
	eater.needs.energy = 0.2f;
	eater.desires = OnlyDesire(Desire::Hunger);
	auto helper = Content(CreatureType::Tiger, {73.0f, 49.0f}, 270.0f, "evil, compassionate");
	helper.alignment = -0.6f;
	helper.desires = OnlyDesire(Desire::Compassion);
	all.push_back({
	    .id = "appearance.deed_alignment",
	    .name = "Deeds move the alignment",
	    .facet = Facet::Appearance,
	    .description = "On the left, a good tiger that only wants to eat stands by six villagers. On the right, an evil "
	                   "tiger that only wants to be nice knows the water miracle and stands by a hut of its town. A "
	                   "fireball sets the hut alight.",
	    .expected = "Each turn that the left tiger eats a villager, its alignment moves to evil. Each turn that the right "
	                "tiger puts out the fire, its alignment moves to good. The change is slow: run the game fast "
	                "and watch the alignment in the spawner. The bodies and skins follow it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {eater, helper},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-48.0f, -12.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-72.0f, -12.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-60.0f, -25.0f}},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {-90.0f, 10.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-30.0f, 10.0f}},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {-60.0f, 30.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {66.0f, 44.0f}},
	                {.type = TreeInfo::Birch, .offset = {53.0f, 43.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {63.0f, 37.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {56.0f, 42.0f}, .joinTown = true}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = {66.0f, 44.0f},
	                  .handOffset = {66.0f, 34.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 4.0f}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 1, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)}},
	});

	// A tiger that only wants to look at things, by a small town whose desire flags stand near its store
	auto curious = Content(CreatureType::Tiger, {0.0f, -30.0f}, 0.0f, "curious");
	curious.phase = 13;
	curious.desires = OnlyDesire(Desire::Curiosity);
	all.push_back({
	    .id = "mind.curious_about_desire_flag",
	    .name = "A curious creature can look at a town's desire flag",
	    .facet = Facet::Mind,
	    .description = "A grown tiger that only wants to look at things stands 30 m south of a small town: a Celtic storage "
	                   "pit with two villagers of the town. The town's desire flags stand over the pit.",
	    .expected = "The tiger comes across the flags as it comes across the pit. The data of a flag lets a creature act "
	                "on it and look it over, so a flag can be the thing of a curiosity plan, as the pit and the villagers "
	                "can. In the first two minutes the inspector's creature plan names a flag as the object of a look "
	                "(ExamineByLooking). A flag is never the thing "
	                "of a plan to attack, to play, to sleep by or to pick up.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -40.0f}, {30.0f, 30.0f}}},
	    .creatures = {curious},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 10.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, 4.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {12.0f, 4.0f}, .joinTown = true}},
	    .townBeliefInPlayer = 1000.0f,
	});
}

void AddLight(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "light.shadows",
	    .name = "Shadows through the day",
	    .facet = Facet::Light,
	    .description = "A tiger, a cow and a giant ape stand still while the hour jumps from morning to midday, the "
	                   "afternoon and dusk, the clock otherwise standing still.",
	    .expected = "Their shadows lie long to one side in the morning, short under them at midday and long to the "
	                "other side in the afternoon, fading as the sun goes down.",
	    .environment = {.hour = 9.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {40.0f, 0.0f}), Posed(CreatureType::Cow, {0.0f, 0.0f}),
	                  Posed(CreatureType::GiantApe, {-45.0f, 0.0f})},
	    .commands = {AtHour(9.0f, 0.0f), AtHour(12.0f, 5.0f), AtHour(15.0f, 5.0f), AtHour(17.5f, 5.0f), AtHour(20.0f, 5.0f),
	                 AtHour(9.0f, 5.0f)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "light.boost",
	    .name = "A light boost brightens the light of the land",
	    .facet = Facet::Light,
	    .description = "Start the game with --profile-folder on a folder that holds a Profiles.txt and a settings.json "
	                   "with {\"detail_level\": 5, \"custom_detail\": {\"light_boost\": 1}}. A tiger, a cow and a giant "
	                   "ape stand still at midday. Start again with \"light_boost\": 0, or with no light_boost, to compare.",
	    .expected = "With the boost the land, the sky's haze and the creatures are lit brighter than without it, the "
	                "brightest parts most; at a boost of 0 the light is the same as at every other detail level.",
	    .environment = {.hour = 12.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {40.0f, 0.0f}), Posed(CreatureType::Cow, {0.0f, 0.0f}),
	                  Posed(CreatureType::GiantApe, {-45.0f, 0.0f})},
	});

	all.push_back({
	    .id = "light.moving_shadows",
	    .name = "Shadows of things in flight",
	    .facet = Facet::Light,
	    .description = "A dead pine, a pot of food, a rock and a toy ball fly up high from a row on the land, one after "
	                   "the other, and fly up again every eight seconds.",
	    .expected = "While each thing flies, a shadow lies on the land straight below it and turns as the thing turns. "
	                "The dead pine's and the pot's shadows are soft and light, through their textures. The rock's and "
	                "the ball's shadows are solid and darker. Each shadow goes when its thing comes to rest.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-24.0f, -10.0f}, {24.0f, 10.0f}}},
	    .objects = {{.type = TreeInfo::Pine, .offset = {-18.0f, 0.0f}, .dead = true},
	                {.type = PotInfo::FoodPot, .offset = {-6.0f, 0.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {6.0f, 0.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {18.0f, 0.0f}}},
	    .throws = {{.object = 0,
	                .from = glm::vec2(-18.0f, 0.0f),
	                .height = 2.0f,
	                .velocity = {0.0f, 25.0f, 1.0f},
	                .delaySeconds = 1.0f,
	                .repeatSeconds = 8.0f},
	               {.object = 1,
	                .from = glm::vec2(-6.0f, 0.0f),
	                .height = 2.0f,
	                .velocity = {0.0f, 25.0f, 1.0f},
	                .delaySeconds = 1.5f,
	                .repeatSeconds = 8.0f},
	               {.object = 2,
	                .from = glm::vec2(6.0f, 0.0f),
	                .height = 2.0f,
	                .velocity = {0.0f, 25.0f, 1.0f},
	                .delaySeconds = 2.0f,
	                .repeatSeconds = 8.0f},
	               {.object = 3,
	                .from = glm::vec2(18.0f, 0.0f),
	                .height = 2.0f,
	                .velocity = {0.0f, 25.0f, 1.0f},
	                .delaySeconds = 2.5f,
	                .repeatSeconds = 8.0f}},
	});

	// A row of animals that live and walk about, with a villager at the end to compare their blobs with his
	all.push_back({
	    .id = "light.animal_blobs",
	    .name = "Blobs under the feet of animals",
	    .facet = Facet::Light,
	    .description = "A cow, a horse, a sheep, a tortoise and a wolf live and walk about in a row at midday. A farmer "
	                   "stands at the end of the row.",
	    .expected = "Under each foot of each animal, a short dark blob lies on the land and falls away to the south "
	                "east. The blobs move with the feet as the animals walk. The farmer has a blob under each foot too.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-14.0f, -6.0f}, {14.0f, 6.0f}}, .distance = 0.5f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {-12.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Horse, .offset = {-6.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Sheep, .offset = {0.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Tortoise, .offset = {6.0f, 0.0f}, .living = true},
	                {.type = AnimalInfo::Wolf, .offset = {12.0f, 0.0f}, .living = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {14.0f, 4.0f}}},
	});

	all.push_back({
	    .id = "light.reflections",
	    .name = "Reflections in the lake",
	    .facet = Facet::Light,
	    .description = "A tiger, a horse and a giant ape stand in the far shallows of the testbed's lake, seen across "
	                   "the water from the south.",
	    .expected = "Only the tiger, the player's first creature, shows upside down in the water in front of it, moving "
	                "as it moves. Its reflection is a flat blue grey without light or shade. The horse and the giant ape "
	                "show no reflection.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x + 30.0f, k_FarShallows}),
	                  Posed(CreatureType::Horse, {k_Lake.x, k_FarShallows}),
	                  Posed(CreatureType::GiantApe, {k_Lake.x - 30.0f, k_FarShallows})},
	});

	all.push_back({
	    .id = "light.rain_splashes",
	    .name = "Rain splashes on the land and the lake",
	    .facet = Facet::Light,
	    .description = "A heavy rain falls over the whole island at noon. The camera looks low over the grass, a birch, "
	                   "the shallows and the open water of the testbed's lake.",
	    .expected = "Small rings come and go on the grass and on the water where the rain falls. Each ring grows and "
	                "fades in less than a second. The rings are white by day.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .weather = Weather::Rain, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
	                .look = {k_Lake.x, 0.0f, k_NearShallows + 10.0f}},
	    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
	});

	all.push_back({
	    .id = "light.rain_splashes_night",
	    .name = "Rain splashes at night",
	    .facet = Facet::Light,
	    .description = "A heavy rain falls over the whole island at eleven at night. The camera looks low over the "
	                   "grass, a birch, the shallows and the open water of the testbed's lake.",
	    .expected = "Small rings come and go on the grass and on the water where the rain falls. The rings take the "
	                "dark colour of the land's light at night, so they show less than by day.",
	    .environment = {.hour = 23.0f, .clockRuns = false, .weather = Weather::Rain, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
	                .look = {k_Lake.x, 0.0f, k_NearShallows + 10.0f}},
	    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
	});

	all.push_back({
	    .id = "light.sky_dawn",
	    .name = "The sky at dawn",
	    .facet = Facet::Light,
	    .description = "Six in the morning, and the clock does not move. The camera looks low over the testbed's lake to "
	                   "the sky, past a birch on the shore.",
	    .expected = "The dome blends the night sky into the sky of dawn. The land and the water take the dim light of "
	                "dawn, and the moon or the sun shows where the hour puts it.",
	    .environment = {.hour = 6.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
	                .look = {k_Lake.x, 12.0f, k_NearShallows + 10.0f}},
	    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
	});

	all.push_back({
	    .id = "light.snow_close",
	    .name = "Heavy snow near the camera",
	    .facet = Facet::Light,
	    .description =
	        "A blizzard falls over the whole island at noon. The snow on the land is not deep, so its edges "
	        "are ragged. A tree stands in the snow. The camera is low and near the land, so the flakes fall close to it.",
	    .expected = "The edges of the snow on the land are soft, as the texture of the land is. You see no square "
	                "texels. The flakes are small white crystals that turn as they fall.",
	    .environment =
	        {.hour = 12.0f, .clockRuns = false, .weather = Weather::Blizzard, .dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -25.0f}, .look = {0.0f, 0.0f, 5.0f}},
	    .objects = {{.type = TreeInfo::Conifer, .offset = {8.0f, 14.0f}, .scale = 1.5f}},
	});

	all.push_back({
	    .id = "light.snow_on_things",
	    .name = "Snow settles on new things",
	    .facet = Facet::Light,
	    .description = "Deep snow lies over the whole island and more snow falls. A hut, a field, trees, a rock, a cow, "
	                   "two villagers and a tiger are new on the land in front of the camera.",
	    .expected = "The things start with no snow. In about two seconds the snow shows on the hut, the trees, the rock, "
	                "the cow and the villagers, and it increases for about two seconds more. The field shows only a "
	                "little snow. The tiger shows no snow. Turn the camera away and back: the things keep their snow.",
	    .environment = {.weather = Weather::Snow, .dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 2.0f, 12.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {28.0f, 18.0f})},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-12.0f, 18.0f}},
	                {.type = FieldTypeInfo::Wheat, .offset = {-26.0f, 0.0f}},
	                {.type = TreeInfo::Conifer, .offset = {8.0f, 22.0f}, .scale = 1.5f},
	                {.type = TreeInfo::Birch, .offset = {14.0f, 10.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {4.0f, 0.0f}},
	                {.type = AnimalInfo::Cow, .offset = {14.0f, -2.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 4.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-8.0f, 6.0f}, .joinTown = true}},
	});

	all.push_back({
	    .id = "light.snow_on_carried_things",
	    .name = "Snow on villagers, not on what they carry",
	    .facet = Facet::Light,
	    .description = "Deep snow lies over the whole island. Two villagers carry wood and one villager carries food. "
	                   "They walk to and fro in front of the camera.",
	    .expected = "The snow shows on the villagers in about four seconds. The wood and the food that they carry show "
	                "no snow.",
	    .environment = {.dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -10.0f}, .look = {0.0f, 1.5f, 8.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {-4.0f, 6.0f},
	                 .walkTo = glm::vec2 {4.0f, 6.0f},
	                 .walkRepeatSeconds = 6.0f,
	                 .walkFinal = VillagerStates::ArrivesAtStoragePitForDropOff,
	                 .woodHeld = 250},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = {4.0f, 10.0f},
	                 .walkTo = glm::vec2 {-4.0f, 10.0f},
	                 .walkRepeatSeconds = 6.0f,
	                 .walkFinal = VillagerStates::ArrivesAtStoragePitForDropOff,
	                 .woodHeld = 250},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-3.0f, 3.0f},
	                 .walkTo = glm::vec2 {3.0f, 3.0f},
	                 .walkRepeatSeconds = 6.0f,
	                 .walkFinal = VillagerStates::HousewifeReturnHomeWithFood,
	                 .foodHeld = 150}},
	});

	all.push_back({
	    .id = "light.snow_high_detail_villager",
	    .name = "No snow on a villager in high detail",
	    .facet = Facet::Light,
	    .description = "Deep snow lies over the whole island. Two villagers stand in front of the camera. After six "
	                   "seconds, the cinema bars come in and the villager on the left changes to high detail.",
	    .expected = "The snow shows on the two villagers in about four seconds. When the villager on the left changes to "
	                "high detail, it shows no snow. The villager on the right keeps its snow.",
	    .environment = {.dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -8.0f}, .look = {0.0f, 1.5f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-1.5f, 6.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {1.5f, 6.0f}}},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 6.0f, .value = 1},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "SET_HIGH_GRAPHICS_DETAIL",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});

	// The same things in snow of three depths. A thing that takes all the snow gets the level (depth - 20) x 255 / 256.
	struct SnowCoverStep
	{
		std::string_view id;
		std::string_view name;
		std::string_view description;
		float depth;
	};
	for (const auto& step : std::array {
	         SnowCoverStep {.id = "light.snow_cover_low",
	                        .name = "Object snow at a low level",
	                        .description = "Snow lies 100 deep over the whole island at noon. A hut, two trees, a rock, "
	                                       "a cow and a villager are new on the land. Their snow increases to the level 79.",
	                        .depth = 100.0f},
	         SnowCoverStep {.id = "light.snow_cover_mid",
	                        .name = "Object snow at a middle level",
	                        .description = "Snow lies 160 deep over the whole island at noon. A hut, two trees, a rock, "
	                                       "a cow and a villager are new on the land. Their snow increases to the level 139.",
	                        .depth = 160.0f},
	         SnowCoverStep {.id = "light.snow_cover_deep",
	                        .name = "Object snow at a high level",
	                        .description = "Snow lies 255 deep over the whole island at noon. A hut, two trees, a rock, "
	                                       "a cow and a villager are new on the land. Their snow increases to the level "
	                                       "234.",
	                        .depth = 255.0f}})
	{
		all.push_back({
		    .id = step.id,
		    .name = step.name,
		    .facet = Facet::Light,
		    .description = step.description,
		    .expected = "The things start with no snow. The snow shows first on the parts that face up, and then spreads "
		                "as it increases. Each texel is snow or the colour of the thing, with no mix of the two. A "
		                "deeper snow covers more of each thing.",
		    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .snowDepth = step.depth},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 7.0f, -9.0f}, .look = {0.0f, 2.0f, 12.0f}},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, 16.0f}},
		                {.type = TreeInfo::Conifer, .offset = {8.0f, 20.0f}, .scale = 1.5f},
		                {.type = TreeInfo::Birch, .offset = {12.0f, 8.0f}},
		                {.type = MobileStaticInfo::Rock, .offset = {3.0f, 2.0f}},
		                {.type = AnimalInfo::Cow, .offset = {-6.0f, 2.0f}},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {-1.0f, 4.0f}}},
		});
	}
}

void AddMovement(std::vector<Scenario>& all)
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

void AddFootprints(std::vector<Scenario>& all)
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

void AddAudio(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "audio.surfaces",
	    .name = "Footsteps on grass and in the shallows",
	    .facet = Facet::Audio,
	    .description = "A tiger walks from the grass down the bank into the shallows of the testbed's lake and back. "
	                   "Select it in the spawner to see its sound log.",
	    .expected = "Its footsteps sound of grass on the land and of splashing in the water; the log's keys show the "
	                "surface changing.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x, 80.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {k_Lake.x, k_NearShallows}, 1.0f), Go(Kind::WalkTo, 0, {k_Lake.x, 80.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	// Two tracks in the custom music folder: the first is liked after strokes, the second disliked after slaps
	const auto music = [](size_t track, float delay) {
		return Command {.kind = Kind::PlayCustomMusic, .delaySeconds = delay, .value = track};
	};
	all.push_back({
	    .id = "audio.custom_music_moods",
	    .name = "The creature's moods for custom music",
	    .facet = Facet::Audio,
	    .description = "Put two MP3 files in the custom_music folder of the profile folder (Profiles/custom_music in the "
	                   "working folder of the game, Profiles/developer/custom_music on a developer run). The first track plays "
	                   "and the tiger is stroked three times, then the second track plays and the tiger is slapped three "
	                   "times. Then the first track plays again, and after 45 seconds the second track again. The log "
	                   "gives the tiger's mood for each track as it starts.",
	    .expected = "Each new track makes the tiger curious: it turns to the camera, points at it, talks, yawns and sits "
	                "still. The first track again (mood above 0) makes it happy: it smiles, waves and dances. The second "
	                "track again (mood below 0) makes it sad, frightened or angry, and the advisor tells you that it "
	                "does not like the music.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f}, 180.0f)},
	    .commands = {music(0, 0.5f), Act(Kind::Stroke, 0, 3.0f), Act(Kind::Stroke, 0, 0.5f), Act(Kind::Stroke, 0, 0.5f),
	                 music(1, 2.0f), Act(Kind::Slap, 0, 3.0f), Act(Kind::Slap, 0, 0.5f), Act(Kind::Slap, 0, 0.5f),
	                 music(0, 2.0f), music(1, 45.0f), music(k_StopCustomMusic, 45.0f)},
	});

	// The controls of the custom music player, open with the play list, while the first track plays
	all.push_back({
	    .id = "audio.custom_music_controls",
	    .name = "The custom music player's controls",
	    .facet = Facet::Audio,
	    .description = "Put two MP3 files in the custom_music folder of the profile folder (Profiles/custom_music in the "
	                   "working folder of the game, Profiles/developer/custom_music on a developer run). The first track "
	                   "plays, and the controls at the top right of the screen open with the play list. Click the "
	                   "buttons, move the sliders and click a track of the list.",
	    .expected = "The music button is at the top right, left of the corner. Below it are play, pause, stop, the last "
	                "and next track, rewind and fast forward, then the volume slider with shuffle and repeat, then the "
	                "position slider with the time above its knob and the play list button. The song name is to the left "
	                "of the music button, and the play list below the controls. Each button clicks. Shuffle and repeat "
	                "stay lit while they are on. Rewind and fast forward do nothing.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {music(0, 0.5f), Command {.kind = Kind::ShowCustomMusicControls, .delaySeconds = 0.5f, .value = 2}},
	});

	all.push_back({
	    .id = "audio.snow",
	    .name = "Footsteps in snow",
	    .facet = Facet::Audio,
	    .description = "Snow falls over the whole island while a tiger walks back and forth. Select it in the spawner to "
	                   "see its sound log.",
	    .expected = "Once the snow lies deep enough, its footsteps change from grass to snow.",
	    .environment = {.weather = Weather::Snow},
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {-50.0f, 0.0f}, 270.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {50.0f, 0.0f}, 1.0f), Go(Kind::WalkTo, 0, {-50.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	constexpr std::array<size_t, 5> k_Voiced {animations::k_Angry, animations::k_Happy, animations::k_Hungry,
	                                          animations::k_Taunt, animations::k_Frightened};
	std::vector<Command> voices;
	for (const auto action : k_Voiced)
	{
		for (size_t creature = 0; creature < 3; ++creature)
		{
			voices.push_back(Play(Kind::PlayAction, creature, action, 1.0f, true));
		}
	}
	auto small = Content(CreatureType::Tiger, {45.0f, 0.0f}, 0.0f, "small tiger");
	small.size = 0.5f;
	small.pauseMind = true;
	auto big = Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "big tiger");
	big.size = 2.0f;
	big.pauseMind = true;
	auto cow = Content(CreatureType::Cow, {-50.0f, 0.0f}, 0.0f, "cow");
	cow.pauseMind = true;
	all.push_back({
	    .id = "audio.voices",
	    .name = "Voices by species and size",
	    .facet = Facet::Audio,
	    .description = "A small tiger, a big tiger and a cow take turns to be angry, happy, hungry, taunting and "
	                   "frightened. Select one in the spawner to see its sound log.",
	    .expected = "Each plays its voice for the action from its own bank, the small tiger higher than the big one and "
	                "the cow its own; the log's size key differs between the tigers.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {small, big, cow},
	    .commands = voices,
	    .repeatFrom = 0,
	});

	// Twenty creatures call out at the same time, more voices than the sound player has
	{
		constexpr size_t k_Crowd = 20;
		std::vector<CreatureSetup> crowd;
		std::vector<Command> calls;
		for (size_t i = 0; i < k_Crowd; ++i)
		{
			const auto offset = glm::vec2(static_cast<float>(i % 5) * 14.0f - 28.0f, static_cast<float>(i / 5) * 14.0f - 21.0f);
			auto setup = Content(i % 2 == 0 ? CreatureType::Tiger : CreatureType::Cow, offset, 180.0f);
			setup.pauseMind = true;
			crowd.push_back(std::move(setup));
		}
		for (const auto action : {animations::k_Angry, animations::k_Taunt})
		{
			for (size_t creature = 0; creature < k_Crowd; ++creature)
			{
				calls.push_back(Play(Kind::PlayAction, creature, action, creature == 0 ? 3.0f : 0.0f, false));
			}
		}
		all.push_back({
		    .id = "audio.voice_limits",
		    .name = "More voices than the sound player has",
		    .facet = Facet::Audio,
		    .description = "Ten tigers and ten cows are angry at the same time, then taunt at the same time, every "
		                   "three seconds. The audio log gives each voice that a sound takes and each sound that gets "
		                   "no voice.",
		    .expected = "At most sixteen sounds play at once. A new sound takes a free voice, else the voice of a sound "
		                "of lower priority; a sound with no such voice stays silent.",
		    .framing = {.shot = Shot::Overview, .distance = 0.5f},
		    .creatures = std::move(crowd),
		    .commands = std::move(calls),
		    .repeatFrom = 0,
		});
	}

	// A tiger to the east roars, then a cow to the west moos, the camera looking north between them
	auto east = Content(CreatureType::Tiger, {40.0f, 0.0f}, 180.0f, "tiger, east");
	east.pauseMind = true;
	auto west = Content(CreatureType::Cow, {-40.0f, 0.0f}, 180.0f, "cow, west");
	west.pauseMind = true;
	all.push_back({
	    .id = "audio.stereo",
	    .name = "Voices from either side of the screen",
	    .facet = Facet::Audio,
	    .description = "The camera looks north between a tiger on the right of the screen and a cow on the left, who take "
	                   "turns to be angry. The audio log gives the side each sound is heard from.",
	    .expected = "The tiger is heard from the right and the cow from the left; turning the camera round swaps them.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -70.0f}, .look = {0.0f, 5.0f, 0.0f}},
	    .creatures = {east, west},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Angry, 1.0f, true),
	                 Play(Kind::PlayAction, 1, animations::k_Angry, 1.0f, true)},
	    .repeatFrom = 0,
	});

	// A tiger roars again and again while a script's cinema bars come in and go out
	auto roaring = Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f, "tiger");
	roaring.pauseMind = true;
	all.push_back({
	    .id = "audio.creature_cut_scene",
	    .name = "A creature heard in a cut scene",
	    .facet = Facet::Audio,
	    .description = "A tiger is angry twice, then twice more while a script's cinema bars are up, and so on again "
	                   "and again. Select it in the spawner to see its sound log.",
	    .expected = "The tiger's growls and footsteps are heard all the time, also while the bars are up: no creature "
	                "sound is kept quiet in a cut scene.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {roaring},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Angry, 1.0f, true),
	                 Play(Kind::PlayAction, 0, animations::k_Angry, 1.0f, true),
	                 {.kind = Kind::WideScreen, .delaySeconds = 1.0f, .value = 1},
	                 Play(Kind::PlayAction, 0, animations::k_Angry, 1.0f, true),
	                 Play(Kind::PlayAction, 0, animations::k_Angry, 1.0f, true),
	                 {.kind = Kind::WideScreen, .delaySeconds = 1.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "audio.alignment_music",
	    .name = "The land's music following the alignment",
	    .facet = Facet::Audio,
	    .description = "The camera stays in the player's influence while their alignment goes from good to evil, to "
	                   "neutral and back to good, twelve seconds apart. The audio log gives the music mix as it fades.",
	    .expected = "The land's music crossfades to the version of each alignment, carrying on in time: the new version "
	                "fades in over about two and a half seconds while the old one fades out over about three.",
	    .environment = {.playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = 0.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = 1.0f}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "audio.cinema_music",
	    .name = "The land's music waiting for the cinema bars",
	    .facet = Facet::Audio,
	    .description = "The camera stays in the player's influence while the land's music plays. After fifteen seconds a "
	                   "script's cinema bars slide in, and twelve seconds later out again. The audio log gives when the "
	                   "land's music waits and when it may play.",
	    .expected = "The land's music fades out as the bars start to slide in and stays silent while they are up and "
	                "while they slide out; once they are gone it comes back, carrying on from where it stopped.",
	    .environment = {.playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 15.0f, .value = 1},
	                 {.kind = Kind::WideScreen, .delaySeconds = 12.0f, .value = 0}},
	});

	// The music type number that the creature choice script gives for the third epic music
	constexpr size_t k_ThirdEpicMusic = 63;
	all.push_back({
	    .id = "audio.script_music_once",
	    .name = "A script's music playing once in a cinema",
	    .facet = Facet::Audio,
	    .description = "As in the creature choice on Land 1, a script's cinema bars slide in and the script starts the "
	                   "third epic music. Forty seconds later the bars slide out. The audio log gives when the music "
	                   "starts and stops.",
	    .expected = "The epic music plays once from its start and stops after about 32 seconds. Then there is no "
	                "music while the bars are up. When the bars are gone, the land's music comes back.",
	    .environment = {.playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 2.0f, .value = 1},
	                 {.kind = Kind::StartScriptMusic, .delaySeconds = 1.0f, .value = k_ThirdEpicMusic},
	                 {.kind = Kind::WideScreen, .delaySeconds = 40.0f, .value = 0}},
	});

	// The music type number of the missionaries' first sung verse
	constexpr size_t k_FirstVerseMusic = 51;
	all.push_back({
	    .id = "audio.sing_along_glow",
	    .name = "The glow under the words of a sung verse",
	    .facet = Facet::Audio,
	    .description = "A script starts the missionaries' first sung verse and shows a dialogue text at each of its "
	                   "first four line cues. Help texts stand in for the verse's words. The audio log gives the cues.",
	    .expected = "A glow box in the colour of the words shows under the word that the song has reached. It glides "
	                "from word to word in about a fifth of a second as the word cues come, and goes back to the first "
	                "word of the newest text at each line cue. The older texts get no glow. When the verse ends, the "
	                "glow goes.",
	    .environment = {.playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::StartScriptMusic, .delaySeconds = 1.0f, .value = k_FirstVerseMusic},
	                 {.kind = Kind::RunText, .delaySeconds = 0.05f, .value = 1009},
	                 {.kind = Kind::RunText, .delaySeconds = 3.1f, .value = 1250},
	                 {.kind = Kind::RunText, .delaySeconds = 2.27f, .value = 1717},
	                 {.kind = Kind::RunText, .delaySeconds = 5.05f, .value = 1240}},
	});

	// The Land 1 script that shows the words of the first sung verse, each at its line of the music
	all.push_back({
	    .id = "story.missionaries_verse",
	    .name = "The words of the missionaries' first sung verse at the lines of the music",
	    .facet = Facet::Story,
	    .description = "At the start, the missionaries' first sung verse starts and the Land 1 script that shows its "
	                   "twelve lines runs, as the explorers' scroll starts them. The script waits for each line of the "
	                   "music, or for 10 seconds, and then shows the words of that line.",
	    .expected = "The verse plays for about 40 seconds. The words of each of the twelve lines show at the start of "
	                "that line of the song, and the glow under the words moves from word to word with the song. The "
	                "audio log gives each line cue.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::StartScriptMusic, .delaySeconds = 0.0f, .value = k_FirstVerseMusic},
	                 {.kind = Kind::RunScript, .delaySeconds = 0.0f, .script = "TheMissionariesVerseOneText"}},
	});

	all.push_back({
	    .id = "audio.temple_music",
	    .name = "The temple's music following the player's alignment",
	    .facet = Facet::Audio,
	    .description = "The temple opens on the creature's room, and the player's alignment goes from good to evil, to "
	                   "neutral and back to good, twelve seconds apart. The audio log gives the music mix as it fades.",
	    .expected = "The temple's music plays in the version of the player's own alignment, crossfading as it changes "
	                "and carrying on in time.",
	    .environment = {.playerAlignment = 1.0f},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 40.0f}, 180.0f, "yours")},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = 0.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 12.0f, .alignment = 1.0f}},
	});

	// Four challenges recorded with their pictures, each looking at a different place of the land
	const std::vector<Command> challengeRecords {{.kind = Kind::RecordChallenge,
	                                              .delaySeconds = 1.0f,
	                                              .point = {0.0f, 0.0f},
	                                              .value = 1,
	                                              .amount = 0.25f,
	                                              .title = "HELP_TEXT_TITLE_01"},
	                                             {.kind = Kind::RecordChallenge,
	                                              .delaySeconds = 0.5f,
	                                              .point = {200.0f, -150.0f},
	                                              .value = 3,
	                                              .amount = 0.0f,
	                                              .title = "HELP_TEXT_TITLE_03"},
	                                             {.kind = Kind::RecordChallenge,
	                                              .delaySeconds = 0.5f,
	                                              .point = {-250.0f, 200.0f},
	                                              .value = 15,
	                                              .amount = 1.0f,
	                                              .title = "HELP_TEXT_TITLE_15"},
	                                             {.kind = Kind::RecordChallenge,
	                                              .delaySeconds = 0.5f,
	                                              .point = {150.0f, 250.0f},
	                                              .value = 16,
	                                              .amount = 0.5f,
	                                              .ctrl = true,
	                                              .title = "HELP_TEXT_TITLE_16"}};
	all.push_back({
	    .id = "temple.challenge_room",
	    .name = "The temple's challenge room with four challenges",
	    .facet = Facet::Story,
	    .description = "Four challenges are recorded as their scripts record them, each with its picture of a different "
	                   "view of the land: The Sea and The Ogre under way, The Lost Brother done, and The Riddles a quest "
	                   "half done. Then the player is put in the temple's main room, and from there in its challenge room.",
	    .expected = "The scroll lists the four titles (four discovered, one completed). The first four pictures on the "
	                "walls show the four views in low detail, in silver frames but The Riddles' gold one; every picture "
	                "but The Lost Brother's has its replay button. Clicking a picture looks at it closely in high detail, "
	                "with its title and its time and percentage at the top of the screen; clicking it again (or a button) "
	                "fades to white with a woosh and leaves the temple to the challenge's view.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        [&challengeRecords] {
		        auto commands = challengeRecords;
		        // The other rooms open after the temple draws the main room. Thus the player enters through it.
		        commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3});
		        commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 0});
		        return commands;
	        }(),
	});

	all.push_back({
	    .id = "story.alignment_history",
	    .name = "The totals of the alignment changes that challenges make",
	    .facet = Facet::Story,
	    .description = "The Lost Brother challenge is recorded three times, one second apart, with a good alignment. "
	                   "The Alignment History debug window opens as the scenario starts.",
	    .expected = "The first record does not change the alignment. The second and third records each make the "
	                "player's alignment better. Each one adds its change to the Script line of the window, and its "
	                "count goes to 2. The Current and Change lines show the alignment now and how far it moved. The "
	                "other lines stay at 0.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::RecordChallenge,
	                  .delaySeconds = 1.0f,
	                  .value = 19,
	                  .amount = 0.0f,
	                  .alignment = 1.0f,
	                  .title = "HELP_TEXT_TITLE_19"},
	                 {.kind = Kind::RecordChallenge,
	                  .delaySeconds = 1.0f,
	                  .value = 19,
	                  .amount = 0.5f,
	                  .alignment = 1.0f,
	                  .title = "HELP_TEXT_TITLE_19"},
	                 {.kind = Kind::RecordChallenge,
	                  .delaySeconds = 1.0f,
	                  .value = 19,
	                  .amount = 1.0f,
	                  .alignment = 1.0f,
	                  .title = "HELP_TEXT_TITLE_19"}},
	    .debugWindow = "Alignment History",
	});

	all.push_back({
	    .id = "temple.challenge_markers",
	    .name = "The temple's map marking the challenges not done",
	    .facet = Facet::Story,
	    .description = "The same four challenges as the challenge room scenario are recorded, then the player is put in "
	                   "the temple's main room.",
	    .expected = "The map in the pool marks the three challenges not done (not The Lost Brother) with the white "
	                "challenge marker, turning, at the places their pictures looked at; with no glow under them, as the "
	                "game draws their glow at the challenge's place in the world instead. The challenges' button by "
	                "the pool hides them.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        [&challengeRecords] {
		        auto commands = challengeRecords;
		        commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3});
		        return commands;
	        }(),
	});

	all.push_back({
	    .id = "temple.room_not_available",
	    .name = "The temple's creature room is closed to a player with no creature",
	    .facet = Facet::Story,
	    .description = "The player has no creature and is put in the temple's main room. Click the door of the creature "
	                   "room (the door marked Creature), then the door of the challenge room.",
	    .expected = "At the creature room's door the camera stays where it is. The help running stops, and the help "
	                "script CitadelWorldRoomNotAvailable starts and tells the player. The creature room's key does "
	                "nothing. The challenge room's door takes the camera through to it as usual.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.save_game_room",
	    .name = "The temple's save game room with a saved game",
	    .facet = Facet::Story,
	    .description = "The game is saved into slot 2 with the name Testbed. Then the player goes through the temple's "
	                   "main room into the save game room. Click a picture, then the picture again or a button under it.",
	    .expected = "The scroll lists the saved games, the time played and the counts of saves and loads. The empty "
	                "pictures show half of the empty picture. A click on a picture takes the camera close to it, and "
	                "the save, load and delete buttons show. A click on an empty picture asks for a name; OK saves, the "
	                "save count goes up and the help on loading starts. Load and delete ask Yes or No first.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 1.0f, .value = 2},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6}},
	});

	all.push_back({
	    .id = "temple.save_room_picture_after_save",
	    .name = "The temple's save game room shows a new save's picture at once",
	    .facet = Facet::Story,
	    .description = "The player goes through the temple's main room into the save game room. Click an empty "
	                   "picture, click it again, then click OK in the box that asks for a name.",
	    .expected = "After the save, the picture in front of the camera shows the land as it was when the player went "
	                "into the temple. It does not show a white picture, and the player does not have to leave the "
	                "room first.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6}},
	});

	all.push_back({
	    .id = "temple.save_room_close_look_words",
	    .name = "The temple's save game room writes a save's date and name over its picture",
	    .facet = Facet::Story,
	    .description = "The game is saved into slots 0 and 1 with the name Testbed. Then the player goes through the "
	                   "temple's main room into the save game room. Click the picture of slot 0, then the picture of an "
	                   "empty land slot, for example Quick Save.",
	    .expected = "While the camera comes close to the picture of slot 0, the date and the time of the save show at the "
	                "top of the screen, and Testbed shows below them, both in yellow over a black shadow. The words get "
	                "stronger as the camera comes closer. An empty land slot shows only its label, such as Quick Save or "
	                "Auto Save Land: and the land's number, a line below the top. An empty slot of the player shows no "
	                "words.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::SaveGame, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6}},
	});

	all.push_back({
	    .id = "temple.future_room_turn",
	    .name = "The temple's future room turns by itself under its words",
	    .facet = Facet::Story,
	    .description = "The player goes through the temple's main room into the future room. Press the arrow keys once "
	                   "the words show.",
	    .expected = "As the camera comes to the end of its path, \"The future is still uncertain...\" fades in to half "
	                "over two seconds. From then on the room turns slowly by itself, speeding up over a second, and the "
	                "arrow keys do not turn it.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 4}},
	});

	all.push_back({
	    .id = "temple.pictures_unlit",
	    .name = "The temple's pictures drawn in one colour with no lightmap",
	    .facet = Facet::Story,
	    .description = "The four challenges of the challenge room scenario are recorded with their pictures. Then the "
	                   "player goes through the temple's main room into the challenge room.",
	    .expected = "Each picture on the walls is drawn evenly in the temple's light, with no shadows of the room's "
	                "lightmap on it, unlike the frames and the walls around it. A picture further away is fogged evenly "
	                "over all of it.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        [&challengeRecords] {
		        auto commands = challengeRecords;
		        commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3});
		        commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 0});
		        return commands;
	        }(),
	});

	all.push_back({
	    .id = "interface.skirmish_box",
	    .name = "The skirmish box",
	    .facet = Facet::Interface,
	    .description = "After a second the skirmish box shows over the game, as Start Skirmish Game of the menu shows it. "
	                   "Click a land of the list, then Start Game to start a skirmish on it, or Back to continue the story.",
	    .expected = "The screen goes black, then the front end's swirls come up with a box of 780 by 580. \"Choose a map for "
	                "skirmish game.\" is across its top on a light grey band. The list holds each .txt file of the "
	                "playground folder in the order of the folder: construct, Death Comes To Those That Wait, Firestorm, "
	                "\"Defeat three gods to control this realm.\", Island Wars, \"Defeat two gods to control this realm.\" "
	                "and \"Defeat another god to control this realm.\". Back is at the bottom left. Start Game shows at "
	                "the bottom right only after a click on a land.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 0}},
	});

	all.push_back({
	    .id = "interface.skirmish_box_picked",
	    .name = "The skirmish box with a land picked",
	    .facet = Facet::Interface,
	    .description = "After a second the skirmish box shows over the game with its second land picked, as after a click "
	                   "on it.",
	    .expected = "Death Comes To Those That Wait is on a grey bar, and Start Game shows at the bottom right of the box "
	                "with its arrow. Start Game starts a skirmish on that land.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 2}},
	});

	all.push_back({
	    .id = "interface.skirmish_menu",
	    .name = "The main menu of a skirmish",
	    .facet = Facet::Interface,
	    .description = "After a second the main menu opens as it shows in a skirmish.",
	    .expected = "The main menu has four buttons: Continue Game, Leave Skirmish Game, Options and Quit Black & White, "
	                "80 high and 100 apart from the top at 150, with no Join Online Game.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 1}},
	});

	all.push_back({
	    .id = "interface.land_info_box",
	    .name = "The land information box of a skirmish",
	    .facet = Facet::Interface,
	    .description = "After a second the land information box shows, as the script of Two Gods shows it while the land "
	                   "loads. The game goes on under the box. Click Go! or press Escape to close it. Back ends a skirmish.",
	    .expected = "A box of 780 by 580 fades in. \"Defeat another god to control this realm.\" is across its top on a light "
	                "grey band. The list under it has one line, \"Defeat another god to control this realm.\" again, and "
	                "shows no selection. Back is an arrow at the bottom left, and Go! a button in the middle of the bottom. "
	                "The hand does not show while the box is up.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 3}},
	});

	{
		const auto key = [](Kind kind, SDL_Scancode scancode) {
			return Command {.kind = kind, .delaySeconds = 2.0f, .value = static_cast<size_t>(scancode)};
		};
		all.push_back({
		    .id = "interface.box_held_key",
		    .name = "A key held while a box opens and closes",
		    .facet = Facet::Interface,
		    .description = "After 2 seconds the scenario puts the up arrow key (Move Forwards) down and holds it. Every 2 "
		                   "seconds after that: the land information box shows, the keyboard repeats the held key, "
		                   "Escape closes the box, the keyboard repeats the held key again, and the key comes up.",
		    .expected = "The camera moves forwards from the first press. It does not stop when the box shows, because no "
		                "key went down. It stops at the repeat, because the box now has the keyboard. It stays stopped "
		                "after Escape closes the box, until the second repeat. Then it moves forwards again until the key "
		                "comes up. input.state's box_keyboard shows when the box has the keyboard.",
		    .framing = {.shot = Shot::Overview},
		    .commands = {key(Kind::PlainKeyDown, SDL_SCANCODE_UP),
		                 {.kind = Kind::SkirmishBox, .delaySeconds = 2.0f, .value = 3},
		                 key(Kind::PlainKeyRepeat, SDL_SCANCODE_UP),
		                 key(Kind::PressPlainKey, SDL_SCANCODE_ESCAPE),
		                 key(Kind::PlainKeyRepeat, SDL_SCANCODE_UP),
		                 key(Kind::PlainKeyUp, SDL_SCANCODE_UP)},
		});
	}

	all.push_back({
	    .id = "interface.leave_skirmish_question",
	    .name = "The question of Leave Skirmish Game",
	    .facet = Facet::Interface,
	    .description = "After a second the main menu of a skirmish opens, and Leave Skirmish Game is pressed. Yes ends the "
	                   "skirmish, and the skirmish box shows again. No goes back to the menu.",
	    .expected = "A box of 500 by 200 over the menu asks \"This will leave the current Skirmish game.\" and \"Do you "
	                "wish to do this?\" on two lines, with a Yes arrow and a No arrow.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 4}},
	});

	all.push_back({
	    .id = "multiplayer.skirmish_profile_creature",
	    .name = "The player's creature comes to the temple as a skirmish starts",
	    .facet = Facet::Multiplayer,
	    .description = "A temple of the player stands to the north. After a second, the god of the player brings the "
	                   "creature that the profile keeps, as in the first turn of a skirmish. The mind file LethysCreature "
	                   "of the game takes the place of the creature file of the profile.",
	    .expected = "A grown ape named Matey comes at the creature home of the player's temple, at once and with no "
	                "sparkle. It belongs to the player, it knows every leash, and its mind and body come from the file.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-30.0f, 18.0f, 60.0f}, .look = {-14.0f, 6.0f, 88.0f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 5, .mindFile = "LethysCreature"}},
	});

	all.push_back({
	    .id = "multiplayer.skirmish_load_one_creature",
	    .name = "A loaded skirmish keeps one creature for the player",
	    .facet = Facet::Multiplayer,
	    .description =
	        "A temple of the player stands to the north, and the testbed plays as a skirmish. After a second, "
	        "the god of the player brings the creature that the profile keeps, and the profile keeps it in its file. "
	        "The game saves at 3 seconds into slot 0 and loads the slot at 5 seconds. The mind file "
	        "LethysCreature of the game takes the place of the creature file of the profile.",
	    .expected = "A grown ape named Matey comes at the creature home of the temple. After the load, the ape stays where "
	                "the save kept it, and it is the creature of the player. No second ape comes. The log tells that the "
	                "god brings no creature, with the reason 6.",
	    .environment = {.dispenserGrid = false, .skirmish = true},
	    .framing = {.shot = Shot::Placed, .eye = {-30.0f, 18.0f, 60.0f}, .look = {-14.0f, 6.0f, 88.0f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 6, .mindFile = "LethysCreature"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 5.0f, .value = 0}},
	});

	all.push_back({
	    .id = "interface.land_info_box_new_land",
	    .name = "A new land closes the land information box",
	    .facet = Facet::Interface,
	    .description = "The game saves into slot 0 after half a second. After a second the land information box of Two Gods "
	                   "shows. At 3 seconds the game loads the slot, which loads the land again.",
	    .expected = "The land information box fades in. As the land loads again, the box closes, and it does not show "
	                "again after the load. The hand shows again.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 0.5f, .value = 0},
	                 {.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "interface.pause_text",
	    .name = "The words of the pause",
	    .facet = Facet::Interface,
	    .description = "After a second the game pauses, as the pause key does. Press P to continue the game, and P "
	                   "again to pause it.",
	    .expected = "Game Paused shows in white at the top middle of the screen. The words fade in as they move away, "
	                "and each letter turns a little after the letter before it. When the game continues, the words fade "
	                "out as they come nearer. The menu (Escape) hides the words at once.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::PauseGame, .delaySeconds = 1.0f, .value = 1}},
	});

	// The message of the real weather in the list of incoming messages, then a second one after it
	all.push_back({
	    .id = "interface.real_weather_message",
	    .name = "The message of the real weather",
	    .facet = Facet::Interface,
	    .description = "After a second the message of the real weather comes in, as the real weather gives it at the "
	                   "start of a game: a storm with rain over Guildford. A second message, snow over Oslo, comes in "
	                   "after it. Click the button, or wait.",
	    .expected = "At the bottom left of the screen, a 48 pixel button shows a cloud with rain, and to its right the "
	                "line \"Weather Information from: Guildford (Surrey,Great Britain)\" in white with a black shadow. "
	                "The picture does not flash. After 10 seconds the second message shows its own picture and "
	                "\"Weather Information from: Oslo (Norway)\" for 10 seconds, then the corner is clear. A left click "
	                "on the button clicks, removes the message and locks the camera onto the player's creature. A right "
	                "click only removes the message.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        {{.kind = Kind::AddIncomingMessage, .delaySeconds = 1.0f, .value = 4, .action = "Guildford (Surrey,Great Britain)"},
	         {.kind = Kind::AddIncomingMessage, .delaySeconds = 0.5f, .value = 6, .action = "Oslo (Norway)"}},
	});

	// The nine weather pictures one after the other
	all.push_back({
	    .id = "interface.weather_pictures",
	    .name = "The nine weather pictures",
	    .facet = Facet::Interface,
	    .description = "After a second nine messages of the real weather come in, one for each weather picture, in the "
	                   "order of the pictures. Each one shows for 10 seconds. Click the button to go to the next one.",
	    .expected = "The pictures show in turn: sun, sun with a cloud, a dark cloud, sun with a cloud and some rain, a "
	                "cloud with rain, sun with a cloud and some snow, a cloud with snow, a cloud with snow and rain, and a "
	                "thunder storm. The line of text names the number of the picture.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        [] {
		        std::vector<Command> commands;
		        static constexpr std::array<std::string_view, incoming_messages::k_WeatherPictures> k_Places {
		            "Picture 0 (Sun)",       "Picture 1 (Sun,Cloud)", "Picture 2 (Cloud)",
		            "Picture 3 (Some rain)", "Picture 4 (Rain)",      "Picture 5 (Some snow)",
		            "Picture 6 (Snow)",      "Picture 7 (Snow,Rain)", "Picture 8 (Thunder storm)"};
		        for (size_t picture = 0; picture < k_Places.size(); ++picture)
		        {
			        commands.push_back({.kind = Kind::AddIncomingMessage,
			                            .delaySeconds = picture == 0 ? 1.0f : 0.1f,
			                            .value = picture,
			                            .action = k_Places.at(picture)});
		        }
		        return commands;
	        }(),
	});

	// A chat line and an e-mail, whose buttons flash
	all.push_back({
	    .id = "interface.incoming_chat_and_mail",
	    .name = "Incoming chat and e-mail",
	    .facet = Facet::Interface,
	    .description = "After a second a chat line and an e-mail come into the list of incoming messages, as other "
	                   "players and the e-mail announcement add them. Single player gets no chat or e-mail; this stages "
	                   "the looks of their buttons.",
	    .expected = "The chat line shows first: a speech bubble with an arrow that flashes to a speech bubble without "
	                "the arrow for a tenth of a second in each four tenths. The bubble lights up under the pointer. "
	                "After 10 seconds the e-mail shows an envelope that flashes in the same way.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::AddIncomingMessage,
	                  .delaySeconds = 1.0f,
	                  .value = k_IncomingChat,
	                  .action = "Alice: Hello from the other island"},
	                 {.kind = Kind::AddIncomingMessage,
	                  .delaySeconds = 0.5f,
	                  .value = k_IncomingMail,
	                  .action = "Bob has sent you an e-mail about the harvest"}},
	});

	// The chat of a network game: the speech bubbles over the creatures, the incoming lines and the open chat
	all.push_back({
	    .id = "interface.chat_lines",
	    .name = "Chat lines and speech bubbles",
	    .facet = Facet::Interface,
	    .description = "Bob, the god of the second tiger, says a line in the chat. Then Alice, the god of this computer, "
	                   "says a line. Then the chat opens with a line in it, and its list of players shows Bob.",
	    .expected = "A bubble with the line comes over each tiger. The bubble over Bob's tiger has his line in his green, "
	                "on a box of half of his colour. The bubble of Alice's tiger shows her line without her name. The "
	                "lines of Bob come into the incoming messages. The speech button at the top right becomes the line "
	                "of the chat, the quick chat and the send buttons, and the list of players under them. The bubbles "
	                "fade after some seconds.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -60.0f}, .look = {0.0f, 10.0f, 0.0f}},
	    .creatures = {{.label = "Alice's tiger",
	                   .species = CreatureType::Tiger,
	                   .offset = {-12.0f, 0.0f},
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .size = 0.5f,
	                   .pauseMind = true},
	                  {.label = "Bob's tiger",
	                   .species = CreatureType::Tiger,
	                   .offset = {12.0f, 0.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 0.5f,
	                   .pauseMind = true}},
	    .commands = {{.kind = Kind::Chat,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .value = k_ChatSays,
	                  .action = "Hello from the other island"},
	                 {.kind = Kind::Chat,
	                  .player = PlayerNames::PLAYER_ONE,
	                  .delaySeconds = 1.5f,
	                  .value = k_ChatSays,
	                  .action = "Hello Bob"},
	                 {.kind = Kind::Chat, .delaySeconds = 1.5f, .value = k_ChatTypes, .action = "Shall we trade"}},
	});

	all.push_back({
	    .id = "interface.controls_rebind",
	    .name = "The Controls page asks for a new key",
	    .facet = Facet::Interface,
	    .description = "After a second the menu opens on its Controls page. The page picks the Help row and double "
	                   "clicks it. Press a key, or click a mouse button away from the box's buttons, then click OK. Load "
	                   "Defaults asks first, then puts every binding back.",
	    .expected = "The box reads \"Define a key for  Help:\" between a Cancel and an OK arrow. A key shows under the "
	                "words, with Ctrl, Shift or Alt before it. Escape, Tab, P, F1, F2 and the Enter key of the number pad "
	                "show nothing. OK binds the input, and the action that had it before loses it. The keys have the "
	                "names that the system gives them, such as Ctrl and Space.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::OpenControlsBox, .delaySeconds = 1.0f, .value = 0}},
	});

	all.push_back({
	    .id = "interface.dialog_keys",
	    .name = "Tab, the arrows and Return in the menu",
	    .facet = Facet::Interface,
	    .description = "After a second the menu opens on its Options page. Each second Tab moves the focus to the next "
	                   "control, nine times. Then the right arrow moves the music slider, Shift and Tab moves the focus "
	                   "back, and Escape goes back to the main menu.",
	    .expected = "The control with the focus has white words in place of grey ones. The arrows of the detail "
	                "selector, which have no words, light up while they have the focus. After the controls, the focus "
	                "goes to the five tabs and shows nothing there. The right arrow moves the knob a tenth to the right.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::OpenMenuPage, .delaySeconds = 1.0f, .value = 1},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 0x4000004F},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9, .ctrl = true},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 27}},
	});

	all.push_back({
	    .id = "interface.read_speed_slider",
	    .name = "The help text speed slider of a game with no sound",
	    .facet = Facet::Interface,
	    .description = "After a second the menu opens on its Advanced page as if the game had no sound. A second later "
	                   "Tab moves the focus ten times, to the slider under the three selectors. Then the right arrow "
	                   "moves the slider twice.",
	    .expected = "A \"Help Text Speed\" slider shows at the middle of the page, under the Tooltips selector and over "
	                "the check boxes. Its knob starts at the middle and moves a tenth to the right for each arrow.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::OpenMenuPage, .delaySeconds = 1.0f, .value = 3, .ctrl = true},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 1.0f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.1f, .value = 9},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.5f, .value = 0x4000004F},
	                 {.kind = Kind::PressMenuKey, .delaySeconds = 0.5f, .value = 0x4000004F}},
	});

	all.push_back({
	    .id = "interface.speed_text",
	    .name = "The words of the game's speed",
	    .facet = Facet::Interface,
	    .description = "After a second the game goes at half speed, as Alt and 1 does. Six seconds later it goes at "
	                   "twice the speed, as Alt and 2 does. Six seconds later it goes at the normal speed again.",
	    .expected = "Slow Down Time shows in white at the top middle of the screen, as Game Paused does. Then it fades "
	                "out, and Speed Up Time fades in. At the normal speed the words fade out and nothing shows.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SetGameSpeed, .delaySeconds = 1.0f, .amount = 0.5f},
	                 {.kind = Kind::SetGameSpeed, .delaySeconds = 3.0f, .amount = 2.0f},
	                 {.kind = Kind::SetGameSpeed, .delaySeconds = 12.0f, .amount = 1.0f}},
	});

	all.push_back({
	    .id = "temple.save_room_pictures",
	    .name = "The save game room shows the picture of a quick save",
	    .facet = Facet::Story,
	    .description = "The game quick saves, as the quick save key does. Then the player goes into the temple's main room, "
	                   "and the game takes a picture of the land. The game saves into slot 0, as the save game room does. "
	                   "Then the player goes into the save game room.",
	    .expected = "The pictures of the Quick Save slot and of slot 0 show the land, not a white picture: the quick save "
	                "its own picture, slot 0 the picture from the entry into the temple. Slot 0 is low on the wall in "
	                "front. The scroll lists the saves as Quick Save and Testbed.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::QuickSave, .delaySeconds = 1.0f},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::SaveGame, .delaySeconds = 0.5f, .value = 0},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6}},
	});

	all.push_back({
	    .id = "temple.scroll_control_words",
	    .name = "The library's did you know scroll with control words",
	    .facet = Facet::Story,
	    .description = "The player reads three did you know tips on signs, one of them ending in the words of a control. "
	                   "Then the player goes through the temple's main room into the library. After four seconds the "
	                   "camera looks at the did you know scroll.",
	    .expected = "The last tip ends \"field. (RMB )\" on its own line, whole, as the game breaks it. Its words are "
	                "the brackets, the name of the mouse input and a space inside them. The line is not cut at the "
	                "right. The temple's hand at the middle of the screen hides the end of the line. Move the pointer away "
	                "and the whole line shows.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::ReadTip, .delaySeconds = 0.5f, .value = 4131, .amount = 4.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4138, .amount = 4.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4153, .amount = 4.0f},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 2},
	                 {.kind = Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 5}},
	});

	all.push_back({
	    .id = "temple.library_history",
	    .name = "The temple's library with tips read and a story history",
	    .facet = Facet::Story,
	    .description = "Scripts show seven help texts of different speakers: the big voice, a man, Khazar, a woman and "
	                   "the two advisors. The player reads four tips on signs: two of village life, one of the creature "
	                   "and one of did you know. Then the player goes through the temple's main room into the library. "
	                   "After four seconds the camera looks at the last scroll (Story History), as a press on it does.",
	    .expected = "The village life scroll lists its two tips in the order read, the creature scroll and the did you "
	                "know scroll one tip each, the other help scrolls only their titles. Close to the Story History "
	                "scroll, its text shows the five newest lines, the newest first: who said each line, then the line. "
	                "Each line after the first starts with how many lines ago it was. The buttons Previous Line (left "
	                "arrow, bottom left), Latest Line (bottom middle) and Next Line (right arrow, bottom right) fade "
	                "in. Previous Line shows one older line first, Next Line one newer line, and Latest Line the newest "
	                "line. The temple keeps the menu's click silent. The buttons fade out as the camera goes back.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::RunText, .delaySeconds = 0.5f, .value = 4411},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 1250},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 1717},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 1240},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 6675},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 6676},
	                 {.kind = Kind::RunText, .delaySeconds = 0.2f, .value = 4411},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4119, .amount = 2.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4120, .amount = 2.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4121, .amount = 1.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4122, .amount = 4.0f},
	                 // The other rooms open after the temple draws the main room. Thus the player enters through it.
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 2},
	                 {.kind = Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 6}},
	});

	all.push_back({
	    .id = "temple.scroll_pictures",
	    .name = "The pictures of a gesture and of controls on the library's did you know scroll",
	    .facet = Facet::Story,
	    .description = "The player reads three did you know tips on signs: one with the picture of a gesture, two with the "
	                   "picture of a control. Then the player goes through the temple's main room into the library. After "
	                   "four seconds the camera looks at the did you know scroll, as a press on it does.",
	    .expected = "Far from the scroll, the lines of the pictures are blank. Close to the scroll, its text is in front of "
	                "it, and under each tip is its picture, two lines high: the gesture in white over a black shadow, "
	                "and the mouse button or the key of each control. The pictures move with the scroll when it turns. "
	                "A gesture is cut at the top and the bottom of the scroll. A control shows only while all of it is "
	                "on the scroll.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::ReadTip, .delaySeconds = 0.5f, .value = 4131, .amount = 4.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4138, .amount = 4.0f},
	                 {.kind = Kind::ReadTip, .delaySeconds = 0.2f, .value = 4153, .amount = 4.0f},
	                 // The other rooms open after the temple draws the main room. Thus the player enters through it.
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 2},
	                 {.kind = Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 5}},
	});

	all.push_back({
	    .id = "temple.map_markers",
	    .name = "The temple's map showing the miracles cast and the influence",
	    .facet = Facet::Story,
	    .description = "The player has a temple west of the middle. A script casts a shield, a storm and a forest there, "
	                   "each to last ten minutes. Then the player is put in the temple's main room.",
	    .expected = "Over the map in the pool, each miracle shows its seed at a tenth of its size, half a unit above the "
	                "map. The seeds do not turn, because the world stops in the temple. The player's influence circle "
	                "stands round the temple on the map, at full strength, and its texture does not move. The miracles' "
	                "button hides the seeds. The influence button hides the circle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .miracles = {{.type = MagicType::Shield,
	                  .point = {-260.0f, 60.0f},
	                  .handOffset = {-260.0f, 60.0f},
	                  .delaySeconds = 0.5f,
	                  .script = ScriptSpell {.radius = 15.0f, .seconds = 600.0f, .curl = 0.0f}},
	                 {.type = MagicType::StormWindRain,
	                  .point = {-340.0f, 160.0f},
	                  .handOffset = {-340.0f, 160.0f},
	                  .delaySeconds = 0.5f,
	                  .script = ScriptSpell {.radius = 20.0f, .seconds = 600.0f, .curl = 0.0f}},
	                 {.type = MagicType::Forest,
	                  .point = {-220.0f, 160.0f},
	                  .handOffset = {-220.0f, 160.0f},
	                  .delaySeconds = 0.5f,
	                  .script = ScriptSpell {.radius = 15.0f, .seconds = 600.0f, .curl = 0.0f}}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 3.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.held_dialogue",
	    .name = "The world's dialogue kept while the player is in the temple",
	    .facet = Facet::Story,
	    .description = "A script holds the dialogue with the cinema bars and shows two texts of the first lesson. Then the "
	                   "player is put in the temple's main room. Push Escape to leave the temple.",
	    .expected = "Going in, the help stops, the advisors fly home and the dialogue box closes. The bars stay, so the "
	                "dialogue is still held when the player leaves. Thus the game gives back no kept texts: the box "
	                "shows its last texts again, the temple's help texts among them.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::RunText, .delaySeconds = 0.5f, .value = 5128},
	                 {.kind = Kind::RunText, .delaySeconds = 1.0f, .value = 5129},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 5.0f, .value = 3}},
	});

	// The camera looks out across the north edge of the player's influence: the bottom of the screen is inside it,
	// the middle and the top beyond it
	all.push_back({
	    .id = "audio.virtual_influence",
	    .name = "The hand's hum past the influence border",
	    .facet = Facet::Audio,
	    .description = "The hand rests inside the player's influence near its border, goes out past the border for twenty "
	                   "seconds, and comes back in. The audio log gives the hum's pitch as it changes.",
	    .expected = "Outside, a hum is heard, not from any place, at full pitch at first, falling as the hand stays out and "
	                "goes further; it stops when the hand comes back. With no temple standing the border isn't shown, so "
	                "crossing it makes no sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 330.0f}, .look = {0.0f, 0.0f, 420.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.95f}},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 3.0f, .point = {0.0f, -0.5f}, .amount = 2.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 10.0f, .point = {0.0f, -0.1f}, .amount = 2.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 10.0f, .point = {0.0f, 0.6f}, .amount = 2.0f}},
	    .repeatFrom = 0,
	});

	// As above, but with the player's temple standing in the middle of their influence, so their border shows
	all.push_back({
	    .id = "audio.border_crossing",
	    .name = "The hand crossing the influence border",
	    .facet = Facet::Audio,
	    .description = "The player's temple stands in the middle of their influence. The hand goes out over the north edge "
	                   "of the influence and back in, every five seconds. The audio log gives where each sound is heard.",
	    .expected = "Each time the hand goes over the border, out or in, the crossing sound plays once, from the hand, "
	                "and smoke ripples along the border in the player's colour.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f)},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 330.0f}, .look = {0.0f, 0.0f, 420.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.95f}},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 3.0f, .point = {0.0f, -0.5f}, .amount = 2.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 5.0f, .point = {0.0f, 0.5f}, .amount = 2.0f}},
	    .repeatFrom = 1,
	});

	// Two teleport stones the villagers jump between, the first jumps heard from the land, the later ones while the
	// camera is in the temple
	constexpr glm::vec2 k_StoneA {-40.0f, 20.0f};
	constexpr glm::vec2 k_StoneB {40.0f, 70.0f};
	all.push_back({
	    .id = "audio.temple_quiet",
	    .name = "The land's sound effects while the camera is in the temple",
	    .facet = Facet::Audio,
	    .description = "Two teleport stones; a villager beside the first walks to beside the second, through the stones, "
	                   "and another is put down on the first. Then the temple opens on the creature's room, and the "
	                   "villager walks back. The audio log gives the sounds started and those not heard.",
	    .expected = "The first jumps sound where they leave and arrive. In the temple the creature's room's water and "
	                "fire are heard, but the jumps on the land are not.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB}, .distance = 0.8f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -40.0f}, 180.0f, "yours")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(6.0f, 4.0f),
	                 .walkTo = k_StoneB + glm::vec2(6.0f, 4.0f),
	                 .walkAfterSeconds = 5.0f,
	                 .walkRepeatSeconds = 15.0f,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(-8.0f, -4.0f),
	                 .dropOnStoneSeconds = 16.0f}},
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
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 11.5f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f}},
	});

	// A tiger throws a rock at a hut of a town with people, again and again
	constexpr glm::vec2 k_CryHut {40.0f, 0.0f};
	all.push_back({
	    .id = "audio.town_attack_cry",
	    .name = "A town cries out when a thrown rock harms it",
	    .facet = Facet::Audio,
	    .description = "A grown tiger picks up a rock and throws it at a hut of a town with three people, every few "
	                   "seconds. The guidance waits end first. The audio log gives each cry of the town and its sample.",
	    .expected = "When the rock harms the hut or a villager, the town sometimes cries out from the town centre: a "
	                "villager voice of the guidance bank, about an attack or about rocks. The cries come at least four "
	                "seconds apart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, k_CryHut + glm::vec2(10.0f, 10.0f)}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {4.0f, -6.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = k_CryHut},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_CryHut + glm::vec2(-6.0f, 6.0f), .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_CryHut + glm::vec2(-6.0f, -6.0f),
	                 .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_CryHut + glm::vec2(6.0f, 8.0f), .joinTown = true}},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Go(Kind::ThrowAt, 0, k_CryHut, 0.5f),
	                 Go(Kind::WalkTo, 0, k_CryHut - glm::vec2(25.0f, 0.0f), 2.0f),
	                 OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Go(Kind::ThrowAt, 0, k_CryHut, 0.5f)},
	    .repeatFrom = 3,
	});

	// A town that believes more in the neutral player sees the player's heal power-up again and again
	std::vector<ObjectSetup> aweTown;
	constexpr std::array k_AweVillagers {VillagerInfo::CelticFarmerMale, VillagerInfo::CelticForesterMale,
	                                     VillagerInfo::CelticHousewifeFemale};
	for (size_t i = 0; i < 9; ++i)
	{
		aweTown.push_back({.type = k_AweVillagers.at(i % 3),
		                   .offset = {-16.0f + 4.0f * static_cast<float>(i), 30.0f + 3.0f * static_cast<float>(i % 3)},
		                   .joinTown = true});
	}
	all.push_back({
	    .id = "audio.belief_voice",
	    .name = "A town calls out in awe at the belief that it gains",
	    .facet = Facet::Audio,
	    .description = "Nine villagers of a town that believes 0.2 in the player and 0.3 in the neutral player. The hand "
	                   "casts food in front of them for four seconds, every ten seconds. The guidance waits end first. The "
	                   "audio log gives each voice of awe, its sample and its distance from the cursor.",
	    .expected = "A symbol of belief rises from each impressed villager. While the neutral player leads the town, a good "
	                "villager voice of awe sometimes plays at a villager, at most once in 30 turns. The voice stops once "
	                "the town believes most in the player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 50.0f}}, .distance = 0.7f},
	    .objects = aweTown,
	    .miracles = {{.type = MagicType::Food,
	                  .point = {0.0f, 45.0f},
	                  .handOffset = {0.0f, 45.0f},
	                  .handHeight = 14.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 10.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f}},
	    .townBeliefInPlayer = 0.2f,
	});

	// The hand strikes three people of a town with a lightning bolt, far from their hut. The hut does not burn, so its
	// fire does not take the cry first.
	all.push_back({
	    .id = "audio.town_lightning_cry",
	    .name = "A town cries out about lightning when a lightning bolt harms it",
	    .facet = Facet::Audio,
	    .description = "The hand casts a lightning bolt for 30 seconds at the three people of a town, 30 metres from "
	                   "their hut. The guidance waits end first. The audio log gives each cry of the town and its sample.",
	    .expected = "When the bolt harms a villager, the town cries out from the town centre: a villager voice of the "
	                "guidance bank, about an attack, a fire or lightning (samples 190 to 199). With seed 1 the first cry "
	                "is about lightning. The cries come at least four seconds apart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, k_CryHut + glm::vec2(10.0f, 10.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_CryHut},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_CryHut + glm::vec2(-30.0f, 2.0f), .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_CryHut + glm::vec2(-30.0f, -2.0f),
	                 .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_CryHut + glm::vec2(-28.0f, 0.0f), .joinTown = true}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = k_CryHut + glm::vec2(-30.0f, 0.0f),
	                  .handOffset = k_CryHut + glm::vec2(-30.0f, 0.0f),
	                  .handHeight = 15.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 30.0f,
	                  .repeatSeconds = 32.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f}},
	});
}

void AddObjects(std::vector<Scenario>& all)
{
	// Balls all round a tiger facing the camera, south: it reaches for each in turn, looks it over and puts it down,
	// then goes back to the middle and faces south again
	constexpr float k_Reach = 10.0f;
	constexpr float k_Diagonal = 7.0f;
	const std::array<glm::vec2, 6> k_Round {glm::vec2 {0.0f, -k_Reach},          glm::vec2 {-k_Diagonal, -k_Diagonal},
	                                        glm::vec2 {k_Diagonal, -k_Diagonal}, glm::vec2 {-k_Reach, 0.0f},
	                                        glm::vec2 {k_Reach, 0.0f},           glm::vec2 {0.0f, k_Reach}};
	std::vector<ObjectSetup> balls;
	std::vector<Command> reaching;
	for (size_t i = 0; i < k_Round.size(); ++i)
	{
		balls.push_back({.type = MobileObjectInfo::Ball, .offset = k_Round.at(i)});
		reaching.push_back(OnObject(Kind::PickUp, 0, i, 1.0f));
		reaching.push_back(Play(Kind::Examine, 0, creature_object_actions::k_KeepAnimationCount - 1, 0.5f, true));
		reaching.push_back(Act(Kind::PutDown, 0, 0.5f, true));
		reaching.push_back(Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f));
		reaching.push_back(Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 0.0f));
	}
	all.push_back({
	    .id = "objects.reach",
	    .name = "Picking up, looking over, putting down",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera with balls in front of it, to its front left and right, either side "
	                   "and behind it. It picks each up in turn, examines it and puts it down, then goes back to the "
	                   "middle.",
	    .expected = "It reaches for each ball the way it lies, in front, to the side or behind, picking it up with its "
	                "hand; holds it up to look at it; and puts it down gently.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = balls,
	    .commands = reaching,
	    .repeatFrom = 0,
	});

	std::vector<Command> examining {OnObject(Kind::PickUp, 0, 0, 1.0f)};
	for (size_t way = 0; way < creature_object_actions::k_KeepAnimationCount; ++way)
	{
		examining.push_back(Play(Kind::Examine, 0, way, 1.0f, true));
	}
	examining.push_back(Act(Kind::PutDown, 0, 1.0f, true));
	all.push_back({
	    .id = "objects.examine",
	    .name = "Looking a thing over four ways",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a pot in front of it and strokes, shakes, smells and examines it in turn, then "
	                   "puts it down.",
	    .expected = "Each way of looking the pot over plays its own animation with the pot held in the hand.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::EgyptPotA, .offset = {0.0f, -k_Reach}}},
	    .commands = examining,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.throw_put_down_toss",
	    .name = "Throwing at a target, putting down and tossing away",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball and throws it at a pillar of rock 60 units east, fetches it and puts it "
	                   "down, fetches it again and tosses it away.",
	    .expected = "The throw flies flat and hard towards the pillar. The put-down lets go of the ball at rest at the "
	                "finger ends. The toss drops it aside. Each comes to rest where it lands.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 30.0f}, {-30.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 0.0f}, 270.0f)},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-20.0f, -k_Reach}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 0.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {60.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::Discard, 0, 0.5f, true), Go(Kind::WalkTo, 0, {-20.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.toss_close_up",
	    .name = "Tossing a ball away, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball in front of it and tosses it away, again and again, seen close up.",
	    .expected = "The toss lets go of the ball as the hand swings out; the ball leaves at six tenths of the speed of "
	                "the middle of the hand's finger ends at that moment, sideways and a little up, and drops beside the "
	                "tiger. Every toss of the tiger leaves at the same speed in the tiger's own frame.",
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::Discard, 0, 0.5f, true),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.held_in_fingers",
	    .name = "A ball held in the fingers, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball in front of it, looks it over four ways and puts it down, again and "
	                   "again, seen close up.",
	    .expected = "The ball hangs below the middle of the finger ends and turns with the hand. The put-down lets go "
	                "of the ball at rest where the finger ends are.",
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = examining,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.pickup_hold",
	    .name = "Holding a villager, a tree and a ball after the pick-up, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger with, in front of it from left to right, a villager, a birch and a ball. It picks up the "
	                   "villager, stands, walks a few steps and puts her down. Then it does the same with the birch and "
	                   "with the ball, again and again, seen close up.",
	    .expected = "When the pick-up ends, the arm stays up in the hold pose and the held thing stays in the hand. It does "
	                "not jump away from the hand when the creature stands, walks or starts to put it down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.8f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = {-6.0f, -k_Reach},
	                 .indestructible = true,
	                 .inScript = true},
	                {.type = TreeInfo::Birch, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::Ball, .offset = {6.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.5f), Go(Kind::WalkTo, 0, {-6.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 1, 1.5f), Go(Kind::WalkTo, 0, {0.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 2, 1.5f), Go(Kind::WalkTo, 0, {6.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.pickup_head_look",
	    .name = "The head look after the hold of a pick-up, close up",
	    .facet = Facet::Objects,
	    .description = "A content tiger with a ball in front of it. A script tells it to hold the ball. Then it puts the "
	                   "ball down. This repeats, seen close up.",
	    .expected = "As it reaches, the head and the eyes look at the ball. From the moment that the hand takes hold of "
	                "the ball to the end of the pick-up, the head and the eyes look at no point: they turn to the "
	                "front. After the pick-up, they look at the things near the creature again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 Act(Kind::PutDown, 0, 6.0f, true)},
	    .repeatFrom = 0,
	});

	// A tiger a tenth of the size beside it, for whom a villager is too heavy
	auto small = Posed(CreatureType::Tiger, {30.0f, 0.0f});
	small.size = 0.1f;
	all.push_back({
	    .id = "objects.pickup_limits",
	    .name = "What is too big or heavy to lift, and putting down what can't be moved",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, with a ball eight times the usual size to its front right and a villager to its "
	                   "front left whom a script has made immovable, though she may be picked up. A tiger a tenth of the "
	                   "size stands 30 m east with a villager in front of it. The small tiger tries to pick up its "
	                   "villager; the big one tries to pick up the giant ball, then picks up the villager and puts her "
	                   "down, then picks her up again and throws her to the west.",
	    .expected = "The small tiger gives up at once: the villager weighs more than 0.8 of its weight. The big tiger "
	                "gives up on the ball at once: its size and the ball's together are beyond its reach. It picks up "
	                "the villager; each time it lets her go, put down or thrown, she goes straight down onto the land "
	                "under its hand and stands there, never left in the air.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -15.0f}, {35.0f, -15.0f}, {-40.0f, 5.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}), small},
	    .objects =
	        {{.type = VillagerInfo::NorseHousewifeFemale,
	          .offset = {-6.0f, -k_Reach},
	          .indestructible = true,
	          .inScript = true,
	          .immovable = true},
	         {.type = MobileObjectInfo::Ball, .offset = {12.0f, -k_Reach}, .scale = 8.0f},
	         {.type = VillagerInfo::NorseHousewifeFemale, .offset = {30.0f, -3.0f}, .indestructible = true, .inScript = true}},
	    .commands = {OnObject(Kind::PickUp, 1, 2, 1.0f), OnObject(Kind::PickUp, 0, 1, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::PutDown, 0, 1.0f, true), OnObject(Kind::PickUp, 0, 0, 2.0f),
	                 Go(Kind::ThrowAt, 0, {-40.0f, -10.0f}, 1.0f), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 2.0f),
	                 Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 0.0f)},
	});

	all.push_back({
	    .id = "villagers.load_in_creature_hand",
	    .name = "A villager with a load in a creature's hand",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a man in front of it who carries a load of wood. The tiger picks him up, walks "
	                   "a few steps with him, holds him for three seconds and puts him down, and does it again.",
	    .expected = "Before the tiger picks him up, the man holds a log of wood in his hand. In the tiger's hand he holds "
	                "nothing: no log shows. When he is put down he still has the wood, and the log shows in his hand "
	                "again as he goes about.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {15.0f, 15.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -k_Reach}, .woodHeld = 300}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.5f), Go(Kind::WalkTo, 0, {0.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 3.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 2.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.pickup_kinds",
	    .name = "The kinds of thing a creature picks up",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with, in front of it from left to right, a toy ball, a rock, an idol, a birch and a "
	                   "magic mushroom. It picks up and throws the toy ball and then the rock to the west, tries to pick up "
	                   "the idol, pulls up the birch and puts it down, then picks up the mushroom and puts it down.",
	    .expected = "The toy ball and the rock are lifted and thrown. The idol is refused at once: of the statics only "
	                "toys are picked up. The birch creaks as it comes out of the ground and is planted again where it is "
	                "put down. The mushroom is lifted and put down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {25.0f, -15.0f}, {-60.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-12.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-6.0f, -k_Reach}},
	                {.type = MobileStaticInfo::Idol, .offset = {0.0f, -k_Reach}},
	                {.type = TreeInfo::Birch, .offset = {6.0f, -k_Reach}},
	                {.type = FeatureInfo::MagicMushroom, .offset = {12.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, -10.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 1, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, 10.0f}, 4.0f),
	                 OnObject(Kind::PickUp, 0, 2, 1.0f), OnObject(Kind::PickUp, 0, 3, 1.0f), Act(Kind::PutDown, 0, 1.0f, true),
	                 OnObject(Kind::PickUp, 0, 4, 1.0f), Act(Kind::PutDown, 0, 1.0f, true)},
	});

	all.push_back({
	    .id = "objects.pile_kinds",
	    .name = "The kinds of food and wood piles",
	    .facet = Facet::Objects,
	    .description = "From west to east: a store's food pile, the food miracle's pile, a small pile of food such as the "
	                   "hand drops, a pot of food, a pile of wood, the wood miracle's pile, a small pile of wood such as "
	                   "the hand drops and a pot of wood.",
	    .expected = "Each one is drawn where it stands, on the ground, and rises out of the ground for what it holds. The "
	                "store's food pile half full is a low heap of grain; the food miracle's pile full is a wide heap.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, 25.0f}, {50.0f, 60.0f}}},
	    .objects = {{.type = PotInfo::StoragePitFoodPile, .offset = {-40.0f, 45.0f}, .amount = 7500},
	                {.type = PotInfo::MagicFood, .offset = {-10.0f, 40.0f}, .amount = 1000},
	                {.type = PotInfo::HandFood, .offset = {12.0f, 40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {18.0f, 40.0f}},
	                {.type = PotInfo::WoodPile_1, .offset = {26.0f, 40.0f}, .amount = 2500},
	                {.type = PotInfo::MagicWood, .offset = {34.0f, 40.0f}, .amount = 1000},
	                {.type = PotInfo::HandWood, .offset = {40.0f, 40.0f}},
	                {.type = PotInfo::WoodPot, .offset = {46.0f, 40.0f}}},
	});

	all.push_back({
	    .id = "objects.give_wood",
	    .name = "Giving a tree to a store as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's empty storage pit off to the east. A script tells "
	                   "it to give wood from the birch to the pit, as the creature does by itself out of compassion "
	                   "for a town short of wood.",
	    .expected = "It walks to the birch and pulls it up (the tree creaks), carries it towards the pit until it is "
	                "near enough to throw, turns to face the pit with a look of compassion and throws the tree in. The "
	                "pit's wood goes up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {90.0f, 0.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {100.0f, -12.0f}, .joinTown = true}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "GiveWoodFromTreeToStoragePit", .with = 0}},
	});

	all.push_back({
	    .id = "objects.give_wood_site",
	    .name = "Giving a tree to a damaged building as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
	                   "life so that its town wants it mended. A script tells it to give wood from the birch to the "
	                   "building's site.",
	    .expected = "It walks to the birch and pulls it up, carries it towards the hut until it is near enough to throw, "
	                "turns to face it with a look of compassion and throws the tree in: the tree is gone and the hut's "
	                "site has the wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
	    .commands = {{.kind = Kind::DoAction,
	                  .delaySeconds = 1.0f,
	                  .object = 1,
	                  .action = "GiveWoodFromTreeToBuildingSite",
	                  .with = 0}},
	});

	all.push_back({
	    .id = "objects.give_wood_workshop",
	    .name = "Giving a tree to a workshop as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's workshop off to the east, with no wood. A script "
	                   "tells it to give wood from the birch to the workshop, as its copy of the player's gift of wood "
	                   "to a workshop does.",
	    .expected = "It walks to the birch and pulls it up, carries it towards the workshop until it is near enough to "
	                "throw, turns to face the workshop with a look of compassion and throws the tree in. The tree is gone "
	                "and the workshop's pile of logs shows the wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = {90.0f, 0.0f}, .storedFood = 0, .storedWood = 0}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "GiveWoodFromTreeToWorkshop", .with = 0}},
	});

	all.push_back({
	    .id = "objects.steal_scaffold_to_town",
	    .name = "Creature steals a scaffold and takes it to a town",
	    .facet = Facet::Objects,
	    .description = "A tiger with a scaffold of no workshop in front of it, and the storage pit of the player's town "
	                   "behind it. A script tells it to steal the scaffold and take it to the town.",
	    .expected = "It picks the scaffold up, looking at it. It goes near the storage pit of the town, to within three "
	                "times its height, looking at the pit, and puts the scaffold down there. The scaffold makes no "
	                "building site: only a scaffold that a hand puts down starts a building.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 90.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = ScaffoldInfo::Normal, .offset = {0.0f, -20.0f}, .scale = 0.7f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "StealObjectAndTakeToTown"}},
	    .townBeliefInPlayer = 1000.0f,
	});

	all.push_back({
	    .id = "objects.repair_house",
	    .name = "Helping repair a house",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
	                   "life. A script tells it to help repair the hut with the birch.",
	    .expected = "It walks to the birch and picks it up, walks right up to the hut, turns to it and puts the tree "
	                "down by it; the hut gets a fifth of its life back (to eight tenths) and the tree is gone.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "HelpRepairHouse", .with = 0}},
	});

	all.push_back({
	    .id = "objects.plant_tree",
	    .name = "Planting a tree by something",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch in front of it and a rock off to the north-east. A script tells it to plant "
	                   "the birch by the rock, as the creature does by itself out of compassion for a town.",
	    .expected = "It pulls up the birch (the tree creaks), walks right up to the rock, turns to face it and puts the "
	                "birch down beside it, where it stands and takes root.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {50.0f, 50.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {50.0f, 50.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "PlantTree", .with = 0}},
	});

	all.push_back({
	    .id = "objects.kind_throws",
	    .name = "Throwing the kinds of thing about",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager, a rock and a toy ball in front of it, a hut off to the north and a "
	                   "birch to the north-east. A script tells it in turn to throw the villager about, to hurl the rock "
	                   "at the hut, and to throw the toy ball at the birch.",
	    .expected = "It picks up the villager and throws her off to one side. It picks up the rock, goes to where it can "
	                "throw at the hut and hurls the rock at it, angry. Half the time it jumps for joy, then picks up the "
	                "toy ball and throws it at the birch, angry, sometimes jumping for joy again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -15.0f}, {0.0f, -90.0f}, {50.0f, -70.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-12.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {12.0f, -k_Reach}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, -90.0f}},
	                {.type = TreeInfo::Birch, .offset = {50.0f, -70.0f}}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowAround"},
	         {.kind = Kind::DoAction, .delaySeconds = 6.0f, .waitUntilFree = true, .object = 3, .action = "Hurl", .with = 1},
	         {.kind = Kind::DoAction,
	          .delaySeconds = 6.0f,
	          .waitUntilFree = true,
	          .object = 4,
	          .action = "ThrowBallAtObject",
	          .with = 2}},
	});

	all.push_back({
	    .id = "objects.kick_ball",
	    .name = "Kicking a ball about",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a toy ball in front of it. A script tells it to kick the ball about.",
	    .expected = "It walks up to the ball, turns to it and kicks it: the ball flies off ahead of it, half upwards, and "
	                "bounces away. A quarter of the time it then points at the ball a second, else it waits up to a "
	                "second. It goes after the ball and kicks it again, two to four times in all, sometimes jumping for "
	                "joy at the end. The kicks make no sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 10.0f}, {20.0f, -150.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "KickBallAround"}},
	});

	all.push_back({
	    .id = "objects.break_rock",
	    .name = "Breaking a rock",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it. A script tells it to break the rock.",
	    .expected = "It picks the rock up and throws it off to one side; as the rock flies it breaks into two halves, "
	                "which fly on as it flew and come down apart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {80.0f, 80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BreakRock"}},
	});

	all.push_back({
	    .id = "objects.script_takes_rock_wood",
	    .name = "A script takes wood from a rock and gives wood to it",
	    .facet = Facet::Objects,
	    .description = "A rock of scale 1 holds 200 wood. After one second, REMOVE_RESOURCE takes 100 wood from it. After "
	                   "three seconds, ADD_RESOURCE gives it 300 wood. After five seconds, REMOVE_RESOURCE takes 1000 "
	                   "wood from it.",
	    .expected = "The rock becomes half its size and its life becomes half. Then it becomes twice its first size, or "
	                "its largest size if that is smaller. Then the rock gives all its wood and goes from the land.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, {10.0f, 10.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "REMOVE_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 100.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "ADD_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 300.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "REMOVE_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1000.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});

	// A land's script moves its challenge scroll to another place, as The Sacrifice moves its scroll to the altar
	all.push_back({
	    .id = "scripts.scroll_moved_to_another_cell",
	    .name = "A script moves a challenge scroll to a place in another map cell",
	    .facet = Facet::Objects,
	    .description = "After one second, CREATE_HIGHLIGHT makes a silver challenge scroll on the land. After two more "
	                   "seconds, SET_POSITION puts the scroll 30 to the side, in another map cell.",
	    .expected = "The scroll shows where it was made, then it shows and turns at its new place, 30 to the side. "
	                "The cursor on the scroll at its new place picks it. Nothing stays at the old place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {15.0f, 20.0f, -25.0f}, .look = {15.0f, 3.0f, 0.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE_HIGHLIGHT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 2.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {0.0f, 0.0f}},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 28.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {30.0f, 0.0f}}}}},
	});

	// A land's script aims a magic beam from one villager at another, as the healer of Land 2 calls the child
	all.push_back({
	    .id = "scripts.spot_beam_target",
	    .name = "A script aims a magic beam at a villager that walks",
	    .facet = Facet::Objects,
	    .description =
	        "A villager that a script holds stands still. A second villager walks to and fro 16 to the side. "
	        "After one second, SPECIAL_EFFECT_OBJECT starts a magic beam on the first villager for 60 "
	        "seconds, and ADD_SPOT_VISUAL_TARGET_OBJECT gives the beam the second villager as its target. "
	        "After two more seconds, SPECIAL_EFFECT_POSITION starts a magic beam 10 to the side of the first villager, "
	        "and ADD_SPOT_VISUAL_TARGET_POS gives it a point 12 above the land.",
	    .expected = "Wiggling ribbons of light go from the place of the first villager to the middle of the second "
	                "villager. The ribbons follow the second villager while it walks. The second beam goes from its "
	                "place up to the point above the land and stays there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5.0f, -10.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-8.0f, 12.0f},
	                 .walkTo = glm::vec2 {8.0f, 12.0f},
	                 .walkAfterSeconds = 0.5f,
	                 .walkRepeatSeconds = 8.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SPECIAL_EFFECT_OBJECT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 20.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 60.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "ADD_SPOT_VISUAL_TARGET_OBJECT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SPECIAL_EFFECT_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 20.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-10.0f, 4.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 60.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "ADD_SPOT_VISUAL_TARGET_POS",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-10.0f, 4.0f}, .height = 12.0f}}}},
	});

	all.push_back({
	    .id = "objects.smash_stone",
	    .name = "Smashing a stone in half",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it. A script tells it to smash the stone in half.",
	    .expected = "Angry, it reaches for the rock with a blow and stamps on it: the rock breaks into two halves with a "
	                "smashing sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SmashStoneInHalf"}},
	});

	all.push_back({
	    .id = "objects.stomp_villager",
	    .name = "Stomping on a villager",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager in front of it. A script tells it to stomp on the villager.",
	    .expected = "Sometimes angry first, it reaches for the villager with a blow of its foot and an angry face. The "
	                "villager dies under the foot.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Stomp"}},
	});

	all.push_back({
	    .id = "objects.stomp_lost_villager",
	    .name = "Stomping on a villager that the hand takes away",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager in front of it. A script tells it to stomp on the villager. As "
	                   "the foot starts to come down, the player's hand takes the villager.",
	    .expected = "The foot stops and goes back up along the same path, to where the stomp started. Then the tiger "
	                "stands, and the stomp fails. The villager stays alive in the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Stomp"},
	                 {.kind = Kind::HandTake, .delaySeconds = 3.7f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.stomp_and_eat",
	    .name = "Stomping on a sheep and eating it",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a sheep in front of it. A script tells it to stomp on the sheep and eat it.",
	    .expected = "With an angry face it stomps on the sheep, which dies. Then it picks the dead sheep up and eats it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {0.0f, -k_Reach}, .living = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StompAndEat"}},
	});

	all.push_back({
	    .id = "objects.stone_and_eat",
	    .name = "Stoning a sheep and eating it",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it and a sheep further away. A script tells it to stone the "
	                   "sheep with the rock and eat it.",
	    .expected = "It picks up the rock and, with an angry face, walks to a place to throw from. As in the game, it "
	                "aims at the rock in its own hand, so the throw fails: it stops with the rock still in its hand and does "
	                "not eat the sheep.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, -80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {0.0f, -60.0f}, .living = true},
	                {.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StoneAndEat", .with = 1}},
	});

	all.push_back({
	    .id = "objects.sleep_at_home_without_temple",
	    .name = "A script tells a creature with no temple to sleep at home",
	    .facet = Facet::Objects,
	    .description = "A grown tiger and no temple. The tiger walks 100 south of the place where it was made. Then a "
	                   "script tells it to sleep at home, as the guide's scripts on Land 1 do.",
	    .expected = "It walks back to the place where it was made, faces down the slope and sleeps there until it is "
	                "rested.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, -100.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {40.0f, -50.0f}}},
	    .commands =
	        {Go(Kind::WalkTo, 0, {0.0f, -100.0f}, 0.5f, true),
	         {.kind = Kind::DoAction, .delaySeconds = 1.0f, .waitUntilFree = true, .object = 0, .action = "SleepAtHome"}},
	});

	all.push_back({
	    .id = "objects.kick_hut",
	    .name = "Kicking a hut",
	    .facet = Facet::Objects,
	    .description = "A grown tiger near a hut. A script tells it to kick the hut.",
	    .expected = "It walks up to the hut, faces it and kicks it: the kick makes a sound and breaks two pieces off the "
	                "hut, which fly off.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 10.0f})},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, -25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Kick"}},
	});

	all.push_back({
	    .id = "objects.throw_in_the_sea",
	    .name = "Throwing a ball into the sea",
	    .facet = Facet::Objects,
	    .description = "A grown tiger south of the lake with a ball beside it. A script tells it to throw the ball into "
	                   "the sea.",
	    .expected = "It picks up the ball, walks to the nearest coast with a playful face, stops two of its heights "
	                "short of it and throws the ball out into the nearest water.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 120.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {10.0f, 110.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowInTheSea"}},
	});

	all.push_back({
	    .id = "objects.kick_tree",
	    .name = "Kicking a tree",
	    .facet = Facet::Objects,
	    .description = "A grown tiger near a birch. A script tells it to kick the tree.",
	    .expected = "It walks up to the tree, faces it and kicks it: the tree creaks and flies off ahead of the tiger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 10.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "KickTree"}},
	});

	{
		// Villagers, sheep and carts close round a tiger that lies down to sleep
		std::vector<ObjectSetup> round;
		for (int i = 0; i < 8; ++i)
		{
			const auto angle = static_cast<float>(i) * std::numbers::pi_v<float> / 4.0f;
			const glm::vec2 at {portable_math::Sin(angle) * 6.0f, portable_math::Cos(angle) * 6.0f};
			if (i % 3 == 0)
			{
				round.push_back({.type = MobileObjectInfo::Cart, .offset = at});
			}
			else if (i % 3 == 1)
			{
				round.push_back({.type = VillagerInfo::CelticFarmerMale, .offset = at});
			}
			else
			{
				round.push_back({.type = AnimalInfo::Sheep, .offset = at, .living = true});
			}
		}
		all.push_back({
		    .id = "objects.trample_lying_down",
		    .name = "Knocking things about as it lies down",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with villagers, sheep and carts close round it. A script tells it to sleep "
		                   "where it stands.",
		    .expected = "As it lies down, its hands, feet, head and body knock the villagers, sheep and carts that they "
		                "pass near through the air. Nothing is knocked while it sleeps. As it gets up, it knocks things "
		                "again.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = round,
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SleepOnTheSpot"}},
		});
	}

	all.push_back({
	    .id = "objects.trample_huts",
	    .name = "Knocking pieces off huts as it lies down",
	    .facet = Facet::Objects,
	    .description = "A grown tired tiger with four huts close round it. It sleeps where it stands.",
	    .expected = "As it lies down, its hands, feet, head and body knock pieces off the huts that they touch. The "
	                "speed of each piece is about four times the distance that the part of the body moved since its "
	                "last check: a part that moves slowly pushes a piece off, a part that moves quickly throws it far.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, 0.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {8.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-8.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, 8.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, -8.0f}}},
	    .commands = {Act(Kind::Sleep, 0, 1.0f, false)},
	});

	all.push_back({
	    .id = "objects.stroke_toy",
	    .name = "Stroking a cuddly toy",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a cuddly toy in front of it. A script tells it to stroke the toy.",
	    .expected = "With a look of compassion it picks the toy up and strokes it in its hand, looking at it, then keeps "
	                "holding it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StrokToy"}},
	});

	all.push_back({
	    .id = "objects.throw_die",
	    .name = "Throwing a die",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a die in front of it. A script tells it to throw the die.",
	    .expected = "It picks the die up and throws it a little way off to one side, watches it fly, amazed, until it "
	                "lands (three to four seconds at most), then jumps for joy two times in three, else looks sad.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyDie, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowDie"}},
	});

	{
		constexpr glm::vec2 k_ToyTemple {-300.0f, 100.0f};
		constexpr float k_ToyTempleTurn = 36.0f;
		all.push_back({
		    .id = "objects.take_toy_home",
		    .name = "Taking a toy home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a toy ball in front "
		                   "of it. A script tells it to take the toy home.",
		    .expected = "With a look of compassion it picks the ball up, walks to its home, the pen before the temple, "
		                "and sets the ball down within five of it; now and then it tosses it away instead.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "TakeToyHome"}},
		});
		all.push_back({
		    .id = "objects.kick_ball_in_pen",
		    .name = "Kicking a ball about in the pen",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's stands in its pen before the temple, shown at a newborn's size. A "
		                   "toy ball lies nine metres from it in the pen. A script tells it to kick the ball about.",
		    .expected = "It walks towards the ball and stops some metres short of it, because its walks keep clear of "
		                "things by its own size. It turns to the ball and kicks it: the ball flies off a short way, as a "
		                "small creature kicks. It goes after the ball and kicks it again, two to four times in all.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {-320.0f, 113.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-320.0f, 122.0f}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "KickBallAround"}},
		});
		all.push_back({
		    .id = "objects.take_wheat_home",
		    .name = "Taking wheat from a field home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a ripe wheat field in "
		                   "front of it. A script tells it to take food from the field to its home.",
		    .expected = "It walks to the field and faces it, pulls a handful of wheat from it, then walks home with the "
		                "wheat in its hand and looks ahead. At its home, the pen before the temple, it puts the wheat down.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -40.0f}, .ripeCrop = true}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "TakeFoodFromFieldToHome"}},
		});
		all.push_back({
		    .id = "objects.bring_thing_home",
		    .name = "Bringing a thing home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a rock in front of it. "
		                   "A script tells it to bring the rock home.",
		    .expected = "It picks the rock up, walks home and looks ahead, and puts the rock down within five metres of "
		                "its home, the pen before the temple.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringHome"}},
		});
		all.push_back({
		    .id = "objects.steal_toy",
		    .name = "Stealing a toy",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a cuddly toy in front "
		                   "of it, out of the influence of the player. A script tells it to steal the toy and take it to the "
		                   "temple.",
		    .expected = "It picks the toy up and looks at it, walks home and looks ahead, and puts the toy down within two "
		                "times its height of its home, the pen before the temple.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StealObjectAndTakeToCitadel"}},
		});
		// The six practice globes of the guide's food lesson, made on the land in one place by the script
		std::vector<DispenserSetup> lessonGlobes(6,
		                                         {.type = MagicType::Food, .offset = {0.0f, -k_Reach}, .bubbleHeight = 0.0f});
		auto lessonTiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		lessonTiger.pauseMind = false;
		lessonTiger.phase = 9;
		all.push_back({
		    .id = "story.food_lesson_globes",
		    .name = "The globes of the guide's food lesson near the creature",
		    .facet = Facet::Story,
		    .description =
		        "A grown tiger of the player's, at the stage of growing up that the guide's second food lesson "
		        "gives it, stands near six food globes on the land, as the lesson's script makes them. Its "
		        "temple stands far to the west, and the globes are out of the influence of the player. Its "
		        "body wants nothing, and its mind runs. Tap the globes one at a time, and tap one while the hand still holds "
		        "food.",
		    .expected = "The globes stay where they are until the player takes them: the creature leaves them alone. A "
		                "tap takes a globe only when the hand holds nothing: while the hand holds a miracle, the globe "
		                "stays.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {12.0f, 14.0f, -32.0f}, .look = {0.0f, 1.0f, -k_Reach}},
		    .creatures = {lessonTiger},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .dispensers = lessonGlobes,
		});
		all.push_back({
		    .id = "objects.bring_villager_home",
		    .name = "Bringing a villager home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a villager in front of "
		                   "it. A script tells it to bring the villager home.",
		    .expected = "It picks the villager up, walks home and puts the villager down near the pen. Then the villager "
		                "falls dead at once, becomes a skeleton, and goes when the turns that its state still had to count "
		                "are over. No death is counted, and the villager keeps its life.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringHome"}},
		});
		all.push_back({
		    .id = "objects.bring_live_food_home",
		    .name = "Bringing a live pig home as food",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a live pig in front "
		                   "of it. A script tells it to bring the pig home as food.",
		    .expected = "It picks the pig up. In its hand the pig falls dead at once, and its body goes in a puff of smoke "
		                "when the turns that its state still had to count are over: for a pig that grazed nothing, on "
		                "the first turn that it lies dead. No death is counted.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -k_Reach}, .living = true}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringFoodHome"}},
		});
	}

	all.push_back({
	    .id = "objects.throw_to_player",
	    .name = "Throwing a toy to the player",
	    .facet = Facet::Objects,
	    .description = "A grown tiger of the player's with a toy ball in front of it. A script tells it to play a throwing "
	                   "game with the player; keep the hand in view.",
	    .expected = "It picks the ball up and throws it at the player's hand, where the hand was as it set about it, "
	                "looking at it; then it jumps for joy.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "PlayThrowingGameWithPlayer", .with = 0}},
	});

	// The actions with which a creature gets its player's attention, or tells the player what it thinks of the player
	all.push_back({
	    .id = "mind.go_to_hand",
	    .name = "Going to the player's hand",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands to one side of the land in view. Keep the hand still on the land far from "
	                   "it, more than an eighth of the view across. A script tells it to go to the hand after two seconds.",
	    .expected = "It walks to the hand, looking at it, and stops four metres past its own radius from the hand. It "
	                "follows the hand when the hand moves, for thirty seconds at most. One time in two it then jumps for "
	                "joy. With the hand near it the action can not start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "GoToHand"}},
	});
	all.push_back({
	    .id = "mind.point_at_hand",
	    .name = "Pointing at the player's hand",
	    .facet = Facet::Mind,
	    .description =
	        "A player's tiger stands side on to the camera. The cursor puts the hand on the land beside it. A script tells "
	        "it to point at the hand after two seconds and again after eight seconds.",
	    .expected = "Each time it turns to the camera, looking at it, then points for a second at where the hand was as it "
	                "began, looking at that place, amazed.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.7f, 0.75f)},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "PointAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 6.0f, .action = "PointAtHand"}},
	});
	all.push_back({
	    .id = "mind.follow_player",
	    .name = "Following the player when the camera looks away",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands in the middle of the map. The camera stands to its north and looks further "
	                   "north, away from it. A script tells it to follow the player after two seconds.",
	    .expected = "It walks to where the camera looks, looking at the camera, until it is near that place or thirty "
	                "seconds go by. It turns to the camera and plays the clip that asks the player to look at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "FollowPlayer"}},
	});
	all.push_back({
	    .id = "mind.go_to_middle_of_screen",
	    .name = "Going to the middle of the screen",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands near the edge of the view, the camera looking at the land beside it. A "
	                   "script tells it to go to the middle of the screen after two seconds.",
	    .expected = "It walks to where the camera looks, looking at the camera, then turns to the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -120.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .creatures = {Content(CreatureType::Tiger, {35.0f, 10.0f}, 0.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "GoToMiddleOfScreen"}},
	});
	all.push_back({
	    .id = "mind.be_cross",
	    .name = "Being cross with the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger is slapped four times, so that its attitude to the player falls below minus a "
	                   "tenth. A script then tells it to be cross with the player.",
	    .expected = "It walks to where the camera looks, looking at the camera, turns to the camera, then one time in two "
	                "makes a rude sign, else taunts the player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::Slap, .delaySeconds = 0.5f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "BeCrossWithPlayer"}},
	});
	all.push_back({
	    .id = "mind.hold_keeps_plan",
	    .name = "A creature keeps its plan during a hold",
	    .facet = Facet::Mind,
	    .description = "The hand holds a player's tiger, strokes its head and lets go. Soon after, the hand holds it, "
	                   "strokes it and lets go again.",
	    .expected = "During each hold the log shows no new plan (\"sets about\"). On the first turn after each release, "
	                "the tiger starts the plan that it had during the hold again (\"starts N again after the hand let "
	                "go\"). A plan to show the player how it feels (23) also starts again, and the tiger turns to the "
	                "camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {{.kind = Kind::HandStroke,
	                  .delaySeconds = 0.5f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.5f),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 0.4f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.5f)},
	});
	all.push_back({
	    .id = "mind.restart_after_hold",
	    .name = "A plan starts again after a hold",
	    .facet = Facet::Mind,
	    .description = "The hand holds a player's tiger, strokes its head and lets go.",
	    .expected = "On the first turn after the release, the tiger starts the plan that it had during the hold again "
	                "from the first step. On the next turn, the feedback of the stroke arrives and stops that plan. The "
	                "log shows the restart one time only.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true)},
	});
	all.push_back({
	    .id = "mind.stage_zero_body_desires",
	    .name = "The desires of the body before the first stage",
	    .facet = Facet::Mind,
	    .description = "A player's tiger goes back to the first stage of its growth, stage 0.",
	    .expected = "Its desires of fear, tiredness and rest stay at 0 or almost 0, and it does not act on them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {{.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 0.5f, .value = 0}},
	});
	all.push_back({
	    .id = "mind.show_how_nice",
	    .name = "Showing the player how nice it thinks the player is",
	    .facet = Facet::Mind,
	    .description = "A player's tiger is stroked, and a script tells it to show the player how nice it thinks the player "
	                   "is. Then it is slapped four times and told again.",
	    .expected = "The first time it turns to the camera and waves. The second time it turns to the camera and plays the "
	                "sad clip. It looks at the camera all the time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::Stroke, .delaySeconds = 0.5f},
	                 {.kind = Kind::DoAction, .delaySeconds = 1.5f, .action = "ShowPlayerHowNiceYouThinkHeIs"},
	                 {.kind = Kind::Slap, .delaySeconds = 8.0f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "ShowPlayerHowNiceYouThinkHeIs"}},
	});
	all.push_back({
	    .id = "mind.pray_to_player",
	    .name = "Praying to the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera. A script tells it to pray to the player after "
	                   "two seconds.",
	    .expected = "It turns to the camera, looking at it, then kneels and prays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "PrayToPlayer"}},
	});
	all.push_back({
	    .id = "mind.be_silly",
	    .name = "Being silly with the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera. A script tells it to be silly with the player "
	                   "after two seconds.",
	    .expected = "Four times: it turns to the camera and talks, fluttering its eyelids; three times in four it shows that "
	                "it feels playful; one time in two it laughs and talks again; then it pulls a silly face for five "
	                "seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "BeSillyWithPlayer"}},
	});
	all.push_back({
	    .id = "mind.throw_at_camera",
	    .name = "Throwing a thing at the camera",
	    .facet = Facet::Mind,
	    .description = "A player's tiger with a toy ball in front of it. A script tells it to throw the ball at the camera.",
	    .expected = "It picks the ball up, looking at it, then throws it at the camera, looking at the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "ThrowAtCamera", .with = 0}},
	});
	all.push_back({
	    .id = "mind.show_player_object",
	    .name = "Showing the player a thing",
	    .facet = Facet::Mind,
	    .description = "A player's tiger with a toy ball in front of it, the camera looking away from it. A script tells it "
	                   "to show the player the ball.",
	    .expected = "It picks the ball up, walks to where the camera looks, looking at the camera, turns to the camera and "
	                "puts the ball down. Its face shows its attitude to the player as it turns and puts the ball down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "ShowPlayerAnObject", .with = 0}},
	});

	auto stoner = Content(CreatureType::Tiger, {0.0f, 0.0f});
	stoner.desires = {{.desire = creature_desires::Desire::GetHigh, .fraction = 1.0f}};
	all.push_back({
	    .id = "objects.magic_mushroom",
	    .name = "Eating a magic mushroom",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a magic mushroom in front of it, wanting to get high as much as it can. It "
	                   "picks the mushroom up and eats it by itself.",
	    .expected = "Once it has eaten it, it becomes high: amazed, it turns to the camera and talks; its eyes go wide and "
	                "dart about, and smiling it stares at the camera, plays dazed and confused actions, half the time "
	                "jumps for joy, and half the time walks to a clear place, sits there sadly ten to fifteen seconds and "
	                "starts up frightened. At last it turns to the camera amazed, talks and plays a dazed action again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {stoner},
	    .objects = {{.type = FeatureInfo::MagicMushroom, .offset = {0.0f, -k_Reach}}},
	});

	auto hungry = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	hungry.needs = {.energy = 0.3f};
	hungry.hold = false;
	all.push_back({
	    .id = "objects.eat_held",
	    .name = "Eating by picking food up",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger with three piles of food about it picks each up and eats it from its hand.",
	    .expected = "It walks up to each, picks it up, brings it to its mouth and eats it; the food is gone and its "
	                "energy rises in the spawner.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -5.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -5.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 2, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "objects.eat_poor_food",
	    .name = "Eating a handful of food, then one of wood",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger picks up a handful of food and eats it, then picks up a handful of wood and eats it.",
	    .expected = "It keeps the food down and its energy rises. The wood has no food in it: the tiger is sick after it "
	                "eats the wood, is not hungry for twenty seconds, and does not eat a pot again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = PotInfo::HandFood, .offset = {0.0f, -k_Reach}, .amount = 300},
	                {.type = PotInfo::HandWood, .offset = {-15.0f, -5.0f}, .amount = 300}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "objects.bring_back_up",
	    .name = "Bringing up what can't be eaten",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger picks up a villager a script has made indestructible, as Sable is for her lessons, "
	                   "and tries to eat her.",
	    .expected = "It brings her to its mouth but doesn't swallow her: she falls from its hand unhurt, and it retches "
	                "and is sick, then isn't hungry for twenty seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}, .indestructible = true}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	// Giving from the player's hand: the hand takes the thing, held in front of the creature, and gives it
	const std::vector<Command> giving {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                                   {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}};
	const HandHold offering {.offset = {0.0f, -k_Reach}, .height = 12.0f};
	// Its mind runs, to take the thing and choose what to do with it
	auto hungryThinking = hungry;
	hungryThinking.pauseMind = false;
	all.push_back({
	    .id = "creature.give_food",
	    .name = "Giving food from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a pile of food and, held in front of a hungry tiger, gives it to it.",
	    .expected = "The hand turns its palm over to offer the food; the tiger turns to the camera looking curious, reaches "
	                "out, and the hand meets its hand as it takes the food; hungry, it then eats it.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// The hand gives the food, then moves far away while the creature reaches for it
	constexpr float k_CallOffSweepDelay = 0.3f;
	constexpr float k_CallOffSweepSeconds = 0.2f;
	auto callingOff = giving;
	callingOff.push_back({.kind = Kind::PointerSweep,
	                      .delaySeconds = k_CallOffSweepDelay,
	                      .point = {0.45f, 0.0f},
	                      .amount = k_CallOffSweepSeconds});
	all.push_back({
	    .id = "creature.give_called_off",
	    .name = "Calling off a gift by moving the hand away",
	    .facet = Facet::Objects,
	    .description = "The player's hand gives a pile of food to a hungry tiger. As the tiger reaches out for it, the "
	                   "pointer moves fast to the side, and the hand moves with it.",
	    .expected = "The tiger stops the gift: its hand closes on nothing, and the food stays in the player's hand.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = callingOff,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.give_animal_kept_at_home",
	    .name = "Giving an animal from afar to a creature kept at home",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger is kept within ten metres of its home, as in the first land's lessons. The player's "
	                   "hand takes a pig from forty metres away and gives it to the tiger.",
	    .expected = "The tiger takes the pig from the hand and eats it at once. It does not look the pig over first: the pig "
	                "in the hand is where the hand holds it, not where the hand took it.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.05f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -40.0f}, .living = true}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 10.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}},
	    .hand = offering,
	});

	auto curious = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	curious.hold = false;
	curious.pauseMind = false;
	// A creature that moves far from the hand while the hand takes the villager cannot take her. Its mind starts a short
	// time before the hand gives her.
	constexpr float k_ThinksBeforeTheGive = 2.0f;
	auto curiousAtTheGive = curious;
	curiousAtTheGive.pauseMind = true;
	curiousAtTheGive.thinksAfterSeconds = k_ThinksBeforeTheGive;
	all.push_back({
	    .id = "creature.give_villager",
	    .name = "Giving a villager from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a tiger that isn't hungry, gives her to it.",
	    .expected = "The hand turns the villager a quarter round to offer her; the tiger turns to the camera, reaches out and "
	                "takes her from the hand, then does with her what its desires say: looks her over the first time if it "
	                "wants nothing much, eats her if hungry, or throws her at the camera if it wants to play.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {curiousAtTheGive},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// It wants to play with the player and nothing else, so it throws what it is given at the camera
	auto playful = curiousAtTheGive;
	playful.desires = {{.desire = Desire::PlayWithPlayer, .fraction = 1.0f}};
	playful.hold = true;
	all.push_back({
	    .id = "creature.throws_villager",
	    .name = "A creature throws a villager it is given",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a tiger that wants to play with the "
	                   "player, gives her to it.",
	    .expected = "The tiger takes her with its take clip and throws her at the camera with its throw clip, looking at "
	                "the camera. She leaves its hand at the moment of the throw and flies with her thrown clip, then "
	                "lands and gets up.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {playful},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.picks_up_villager",
	    .name = "A creature picks up, puts down and throws a villager",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up the villager in front of it and puts her down. Then it picks her up again and "
	                   "throws her to the west.",
	    .expected = "The tiger reaches down with its pick-up clips and takes her at the moment of the pick-up; in its "
	                "hand she plays her scared stiff clip. It puts her down with its put-down clip. Then it picks her up "
	                "and throws her with its throw clip: she leaves its hand at the moment of the throw, flies with her "
	                "thrown clip, lands and gets up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 1.5f, true), OnObject(Kind::PickUp, 0, 0, 1.5f),
	                 Go(Kind::ThrowAt, 0, {-40.0f, -10.0f}, 1.5f)},
	});

	// A leash order stops the plan while the tiger puts the villager down. The put-down continues and lets her go at its
	// moment.
	all.push_back({
	    .id = "creature.put_down_through_an_order",
	    .name = "A put-down that continues through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash picks up the villager in front of it and starts to put her down. "
	                   "A fifth of a second later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The order does not cut the put-down, and it does not hurry it, because the tiger holds her. The "
	                "tiger lets her go at the moment of the put-down, with no speed, and she stands on the land. When the "
	                "put-down ends, the tiger walks to the point. It does not carry her there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 1.5f, true),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 0.2f, false)},
	});

	// The hand strokes the tiger and lets it go while the tiger puts the villager down. The feedback stops the plan, but
	// the put-down continues and lets her go at its moment.
	all.push_back({
	    .id = "creature.put_down_through_a_stroke",
	    .name = "A put-down that continues through a stroke",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up the villager in front of it and starts to put her down. A tenth of a second later, "
	                   "the hand takes hold of the tiger to stroke its head. The put-down is too new for the stroke, so only "
	                   "the hold counts. A tenth of a second after that, the hand lets go.",
	    .expected = "The feedback of the hand stops what the tiger does, but not the put-down. The tiger lets the villager "
	                "go at the moment of the put-down, with no speed, and she stands on the land. The tiger does not keep "
	                "her in its hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::PutDown, 0, 1.5f, true),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 0.1f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.1f)},
	});

	// A script takes the tiger while it puts the villager down. The put-down continues, and the action of the script
	// waits.
	{
		constexpr uint32_t k_HappyAnimation = 55;
		all.push_back({
		    .id = "creature.put_down_through_a_script",
		    .name = "A put-down that continues when a script takes the creature",
		    .facet = Facet::Objects,
		    .description = "A tiger picks up the villager in front of it and starts to put her down. A fifth of a second "
		                   "later, a script takes the tiger and tells it to play the happy animation two times.",
		    .expected = "The script does not cut the put-down. The tiger lets the villager go at the moment of the "
		                "put-down, with no speed, and she stands on the land. When the put-down ends, the tiger plays the "
		                "happy animation.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
		    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f),
		                 Act(Kind::PutDown, 0, 1.5f, true),
		                 {.kind = Kind::ScriptPlay, .delaySeconds = 0.2f, .value = k_HappyAnimation, .amount = 2.0f}},
		});
	}

	// A leash order stops the plan while the tiger reaches for the villager. The pick-up continues and takes her at its
	// moment.
	all.push_back({
	    .id = "creature.pick_up_through_an_order",
	    .name = "A pick-up that continues through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash starts to pick up the villager in front of it. A fifth of a second "
	                   "later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The order does not cut the pick-up. The tiger plays its reach to the end and takes the villager at "
	                "the moment of the pick-up; in its hand she plays her scared stiff clip. When the pick-up ends, the "
	                "tiger walks to the point.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 0.2f, false)},
	});

	// A leash order stops the plan while the tiger points. The arm does not hold its point: it plays the end of the point
	// at once.
	all.push_back({
	    .id = "creature.point_through_an_order",
	    .name = "A point that ends through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash points at the land in front of it, for five seconds. One second "
	                   "later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The arm does not stop at once and does not hold its point for the five seconds. At the order, the arm "
	                "plays the end of its point animations. When the point ends, the tiger walks to the point.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::PointAt, 0, {0.0f, -40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 1.0f, false)},
	});

	auto hungryAtTheGive = hungryThinking;
	hungryAtTheGive.pauseMind = true;
	hungryAtTheGive.thinksAfterSeconds = k_ThinksBeforeTheGive;
	all.push_back({
	    .id = "creature.give_villager_eaten",
	    .name = "Giving a villager to a hungry creature",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a hungry tiger, gives her to it.",
	    .expected = "The tiger reaches out with its take clip and takes her; in its hand she plays her scared stiff clip. "
	                "Its hunger is its strongest desire, so it plays its eat clip at once: at the moment of "
	                "the bite she is gone from its hand, and it swallows her as the clip ends. Then she dies and its "
	                "hunger falls.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryAtTheGive},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.give_villager_by_hand",
	    .name = "Giving a villager by hand",
	    .facet = Facet::Objects,
	    .description = "A tiger that isn't hungry stands with a villager in front of it. The player picks her up with the "
	                   "hand, holds her in front of the tiger and holds the Action button down to give her.",
	    .expected = "In the hand she plays her scared stiff clip. The hand turns her a quarter round to offer her; the "
	                "tiger turns to the camera, reaches out with its take clip and takes her at the moment of the take; "
	                "in its hand she keeps her scared stiff clip while it looks her over or does what its desires say.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {curious},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	});

	// The pictures for the creature's web page: the camera looks at the tiger, and each action played once has its
	// moment halfway through. The first picture can come once more than ten seconds of the game went by.
	all.push_back({
	    .id = "creature.web_page",
	    .name = "The creature's web page",
	    .facet = Facet::Objects,
	    .description = "The camera looks at a tiger that plays the impress action after twelve seconds; then the game "
	                   "writes the tiger's web page from Scripts/creature_html.tmpl into the profile folder.",
	    .expected = "Halfway through the impress action the game takes a picture of the screen: creatureshot_0.jpg, 400 "
	                "by 300, in the player's profile folder. Then html/<the tiger's name>.html shows its values, the "
	                "picture and the decorations from the game folder, and creature.lhp holds the values and the "
	                "picture.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Impress, 12.0f, true),
	                 Command {.kind = Kind::WriteWebPage, .delaySeconds = 4.0f, .waitUntilFree = true}},
	});

	auto ogre = hungryThinking;
	ogre.species = CreatureType::Ogre;
	all.push_back({
	    .id = "creature.give_to_ogre",
	    .name = "Giving food to an ogre",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a pile of food and, held in front of a hungry ogre, tries to give it to it.",
	    .expected = "An ogre takes nothing from the hand: the hand doesn't offer the food, the ogre doesn't reach for it, "
	                "and the hand lets the food go as it would anywhere else.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {ogre},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// A hungry grown-up tiger that thinks for itself, with villagers about it
	auto eater = Content(CreatureType::Tiger, {0.0f, 0.0f});
	eater.phase = 13;
	eater.hold = false;
	// It becomes hungry as the hand gives, so that it eats what it is given and not the villagers about it
	const Command hungryNow {.kind = Kind::SetDesire,
	                         .creature = 0,
	                         .delaySeconds = 1.4f,
	                         .value = static_cast<size_t>(Desire::Hunger),
	                         .amount = 0.9f};
	auto giveNow = giving[1];
	giveNow.delaySeconds = 0.1f;
	// The hand strokes it as soon as the meal lets it: a stroke too early in the meal does nothing
	std::vector<Command> eatAgain {giving[0], hungryNow, giveNow};
	for (int i = 0; i < 12; ++i)
	{
		auto stroke = Stroke(0, creature_feedback::BodyPart::Head, i == 0 ? 2.5f : 0.3f);
		stroke.waitUntilFree = false;
		eatAgain.push_back(stroke);
	}
	eatAgain.push_back(Act(Kind::HandLetGo, 0, 0.5f, false));
	all.push_back({
	    .id = "mind.stroked_eat_again",
	    .name = "Stroked while it eats from the hand",
	    .facet = Facet::Mind,
	    .description = "The player's hand gives a villager to a hungry grown-up tiger. As the tiger starts to eat her, the "
	                   "hand strokes it. Two more villagers of the same kind stand near.",
	    .expected = "The hand strokes the tiger while it eats. When the hand lets go, the tiger learns to eat what it is "
	                "given and eats the villager. When the meal is done, it tries to do the lesson again on a different "
	                "villager. The log shows the try. The villager that it ate is gone, so it does what it wants next.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {eater},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {30.0f, 25.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-35.0f, 30.0f}}},
	    .commands = eatAgain,
	    .hand = offering,
	});

	all.push_back({
	    .id = "objects.knock_down_trees",
	    .name = "Knocking down trees",
	    .facet = Facet::Objects,
	    .description = "A tiger walks up to each of three trees in a row and knocks it down.",
	    .expected = "It strikes each tree, which falls; its town's view of it shows in the spawner.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = TreeInfo::Beech, .offset = {0.0f, 10.0f}},
	                {.type = TreeInfo::Conifer, .offset = {40.0f, 10.0f}}},
	    .commands = {OnObject(Kind::KnockDown, 0, 0, 1.0f), OnObject(Kind::KnockDown, 0, 1, 1.0f),
	                 OnObject(Kind::KnockDown, 0, 2, 1.0f)},
	});

	all.push_back({
	    .id = "objects.point",
	    .name = "Pointing",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera points in front of it, to either side, far off and behind it.",
	    .expected = "It turns as needed and points its arm at each point in turn.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Go(Kind::PointAt, 0, {0.0f, -40.0f}, 1.0f), Go(Kind::PointAt, 0, {-40.0f, 0.0f}, 1.0f),
	                 Go(Kind::PointAt, 0, {40.0f, 0.0f}, 1.0f), Go(Kind::PointAt, 0, {0.0f, 200.0f}, 1.0f),
	                 Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	// Left to the mind: one desire as strong as it gets, with things about to act on
	struct Mood
	{
		std::string_view id;
		std::string_view name;
		Desire desire;
		std::string_view description;
		std::string_view expected;
	};
	const std::array<Mood, 3> k_Moods {{
	    {"objects.curious", "Curious about things", Desire::Curiosity,
	     "A tiger as curious as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It walks up to something nearby, picks it up and looks it over, then mostly puts it down gently and sometimes "
	     "tosses it away, and goes on to the next."},
	    {"objects.playful", "Playing with things", Desire::Play,
	     "A tiger as playful as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It picks something up and throws it about, a good way off in a random direction, and goes after the next."},
	    {"objects.angry", "Angry with things", Desire::Anger,
	     "A tiger as angry as it gets, wanting nothing else, among a ball, a pot and a barrel, with trees nearby.",
	     "It picks something up, now and then shows its anger first, and hurls it at the nearest tree."},
	}};
	for (const auto& mood : k_Moods)
	{
		auto moody = Content(CreatureType::Tiger, {0.0f, 0.0f});
		moody.desires = OnlyDesire(mood.desire);
		all.push_back({
		    .id = mood.id,
		    .name = mood.name,
		    .facet = Facet::Objects,
		    .description = mood.description,
		    .expected = mood.expected,
		    .framing = {.shot = Shot::Overview, .distance = 1.2f},
		    .creatures = {moody},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptPotA, .offset = {15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptBarrel, .offset = {0.0f, 15.0f}},
		                {.type = TreeInfo::Oak, .offset = {-50.0f, 30.0f}},
		                {.type = TreeInfo::Birch, .offset = {50.0f, 30.0f}}},
		});
	}
}

/// The hand of the player and the creature of a rival god: the hand holds it only after a script makes the two gods
/// allies
void AddAlliedCreatureHold(std::vector<Scenario>& all)
{
	auto rival = Content(CreatureType::Tiger, {15.0f, 0.0f}, 270.0f, "ally's");
	rival.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "hand.allied_creature",
	    .name = "The hand holds the creature of an allied god",
	    .facet = Facet::Hand,
	    .description = "The player's cow and the tiger of a rival god stand 30 apart. The hand tries to hold the tiger "
	                   "and stroke its head. Then a script makes the two gods allies, and the hand holds the tiger and "
	                   "strokes its head again.",
	    .expected = "At the first try the hand does not hold the tiger, and the log shows that the hand can not hold "
	                "it. After the alliance the hand holds the tiger, the camera turns to it, and the hand strokes its "
	                "head. Then the hand lets go and the camera goes back.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Overview, .distance = 0.5f},
	    .creatures = {Content(CreatureType::Cow, {-15.0f, 0.0f}, 90.0f, "player's"), rival},
	    .commands = {Stroke(1, creature_feedback::BodyPart::Head, 1.0f),
	                 Act(Kind::HandLetGo, 1, 1.0f),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_PLAYER_ALLY",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Number, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 2.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1.0f}}},
	                 Stroke(1, creature_feedback::BodyPart::Head, 1.0f),
	                 Act(Kind::HandLetGo, 1, 3.0f)},
	});
}

void AddHand(std::vector<Scenario>& all)
{
	using creature_feedback::BodyPart;
	AddAlliedCreatureHold(all);
	// Every part of the body, the head first
	std::vector<Command> strokes;
	for (size_t part = 0; part < creature_feedback::k_BodyPartCount; ++part)
	{
		strokes.push_back(Stroke(0, static_cast<BodyPart>(part), 1.0f));
	}
	strokes.push_back(Act(Kind::HandLetGo, 0, 1.0f, true));
	all.push_back({
	    .id = "hand.stroke",
	    .name = "Stroking each part of the body",
	    .facet = Facet::Hand,
	    .description = "The hand rests on a tiger facing the camera and strokes its head, armpits, belly, groin, feet and "
	                   "hands in turn, then lets go.",
	    .expected = "Each stroke plays the pleased animation for its part, mirrored for the left side, and its face; the "
	                "panel's reward climbs a tenth a stroke to Good Boy! 90%, and letting go warms it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = strokes,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "hand.slap",
	    .name = "Slapping gently and hard, high and low",
	    .facet = Facet::Hand,
	    .description = "The hand slaps a tiger facing the camera on the head, the waist and the feet, gently and then "
	                   "hard, from either side, then lets go.",
	    .expected = "Each slap plays its reeling animation for its height, the gentle ones softer, the sweeps to the "
	                "right mirrored; the panel's reward falls to Bad Boy! 100%, and letting go cools it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Slap(0, 0.85f, true, false, 1.0f), Slap(0, 0.55f, true, true, 1.0f), Slap(0, 0.2f, true, false, 1.0f),
	                 Slap(0, 0.85f, false, true, 1.0f), Slap(0, 0.55f, false, false, 1.0f), Slap(0, 0.2f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	    .repeatFrom = 0,
	});

	// The real hand on the body: the pointer swept fast across the middle of the screen, at one height, four times
	std::vector<Command> sweeps {{.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                             {.kind = Kind::PointerSweep, .delaySeconds = 1.0f, .point = {-0.1f, 0.0f}, .amount = 0.3f}};
	for (int sweep = 0; sweep < 4; ++sweep)
	{
		const float across = sweep % 2 == 0 ? 0.2f : -0.2f;
		sweeps.push_back({.kind = Kind::PointerSweep, .delaySeconds = 2.5f, .point = {across, 0.0f}, .amount = 0.01f});
	}
	sweeps.push_back({.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3});
	// Its life is not held, so that the hundredth a bruise takes stays off
	auto slapped = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	slapped.needs.life.reset();
	all.push_back({
	    .id = "hand.bruise",
	    .name = "Hard slaps bruise",
	    .facet = Facet::Hand,
	    .description = "The hand holds the player's tiger, which faces the camera, by its middle, and sweeps fast across "
	                   "the middle of the screen four times at the same height, then lets go.",
	    .expected = "Each sweep is a hard slap at the waist. The third hard slap in a row leaves a dark bruise on the "
	                "skin where the line of sight through the hand meets the body, and the tiger's life falls by a "
	                "hundredth. The fourth slap leaves no bruise.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {slapped},
	    .commands = sweeps,
	});

	// The creature that the recorded input of the hand strokes and slaps for the dedicated server's check
	// (tools/sim/hand_inputs/strokes.json). The creature help is on: in a network game every computer counts the time
	// that the reported hand stays over the creature, and the turn sets the look of the held creature.
	all.push_back({
	    .id = "hand.recorded_strokes",
	    .name = "A tiger for the recorded strokes and slaps",
	    .facet = Facet::Hand,
	    .description = "The player's tiger faces the camera in the middle of the screen. The "
	                   "dedicated server and the game both play this scenario, and the game makes the recorded input of "
	                   "tools/sim/hand_inputs/strokes.json: the hand holds the tiger, strokes it, slaps it and lets go.",
	    .expected = "The strokes play the pleased animations and the slaps the reeling ones. In a network game each "
	                "reaction plays at the next turn, on every computer at the same turn.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	// The carried leash round the wrist while the hand strokes and slaps: slow sweeps stroke, fast sweeps slap
	std::vector<Command> collarHand {Leash(0, LeashType::Rope, 0.5f),
	                                 Leash(0, LeashType::Evil, 0.5f),
	                                 Leash(0, LeashType::Good, 0.5f),
	                                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 1},
	                                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3}};
	for (int stroke = 0; stroke < 4; ++stroke)
	{
		const float down = stroke % 2 == 0 ? 0.08f : -0.08f;
		collarHand.push_back({.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, down}, .amount = 1.2f});
	}
	for (int sweep = 0; sweep < 4; ++sweep)
	{
		const float across = sweep % 2 == 0 ? 0.2f : -0.2f;
		collarHand.push_back({.kind = Kind::PointerSweep, .delaySeconds = 2.0f, .point = {across, 0.0f}, .amount = 0.01f});
	}
	collarHand.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3});
	all.push_back({
	    .id = "hand.carried_leash_touch",
	    .name = "The carried leash on the stroking and slapping hand",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera and knows all three leashes. The learning leash is picked at the "
	                   "temple, so the hand carries it. The hand holds the tiger by its middle, strokes it up and down "
	                   "slowly four times, then sweeps fast across the middle of the screen four times, then lets go.",
	    .expected = "The twisted rope collar stays round the wrist of the hand all the time. It moves and turns with the "
	                "wrist as the hand rests on the body, strokes and slaps, and it stays in scale with the hand.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .temples = {{.offset = {-60.0f, 60.0f}, .angle = 36.0f}},
	    .commands = collarHand,
	});

	// The real hand on the body: a stroke on one part plays a long pleased animation, then the hand strokes another part
	// before that animation is far enough through
	std::vector<Command> refusedStroke {
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {-0.1f, 0.0f}, .amount = 0.3f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.6f, .point = {0.2f, 0.0f}, .amount = 0.6f}};
	for (int wiggle = 0; wiggle < 16; ++wiggle)
	{
		const float across = wiggle % 2 == 0 ? 0.02f : -0.02f;
		refusedStroke.push_back({.kind = Kind::PointerSweep, .delaySeconds = 0.35f, .point = {across, 0.0f}, .amount = 0.3f});
	}
	refusedStroke.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3});
	all.push_back({
	    .id = "hand.refused_stroke_face",
	    .name = "A stroke that the creature refuses pulls only the face",
	    .facet = Facet::Hand,
	    .description = "The hand holds the player's tiger, which faces the camera, by its middle. It strokes one part of "
	                   "the body, then moves to another part and strokes it while the first pleased animation plays.",
	    .expected =
	        "The first stroke plays a pleased animation and its face. A stroke on the other part that comes before "
	        "that animation is far enough through pulls only its face, at once: the tiger plays no animation for it and "
	        "the hand sends nothing. The log shows \"only this computer shows its face\". In a network game "
	        "the other computers do not show the face of a stroke.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = refusedStroke,
	});

	all.push_back({
	    .id = "hand.slap_after_stroke",
	    .name = "A slap soon after a stroke",
	    .facet = Facet::Hand,
	    .description = "A tiger faces the camera and the hand is free. Hold the action button on the tiger, stroke it once, "
	                   "then sweep the hand fast across the middle of the screen.",
	    .expected = "The stroke plays the pleased animation. The fast sweep plays a reeling animation and the reward falls, "
	                "with no wait after the stroke, once the stroke animation is far enough through.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	all.push_back({
	    .id = "hand.mesh_contact",
	    .name = "The hand on the body",
	    .facet = Facet::Hand,
	    .description = "A tiger faces the camera and the hand is free. Hold the action button on the tiger. Move the hand "
	                   "slowly over its head, its arms and its belly, then off the body and back.",
	    .expected = "The hand stays under the cursor and rests just in front of the skin, also on thin parts. It eases "
	                "back to the skin in a short time and never goes into the body. Off the body, it stays in front of "
	                "the tiger and stands upright.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	auto worn = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	worn.needs = {.energy = 0.595f, .exhaustion = 0.325f, .life = 0.745f};
	all.push_back({
	    .id = "hand.status_panel",
	    .name = "The creature's status panel",
	    .facet = Facet::Hand,
	    .description = "The hand on a tiger whose needs are held at 25% damage, 40% hunger and 32% tiredness strokes it "
	                   "three times, slaps it once gently and once hard, and lets go.",
	    .expected = "While the hand is on it the panel at the left shows Damage 25%, Hunger 40% and Tiredness 32% in "
	                "yellow bars, and the reward going from No Reward 0% to Good Boy! 30% in green, then down past the "
	                "middle to Bad Boy! in red. Hovering the hand over it shows the panel with the last reward.",
	    .framing = {.shot = Shot::Follow, .distance = 1.0f},
	    .creatures = {worn},
	    .commands = {Stroke(0, BodyPart::Head, 1.0f), Stroke(0, BodyPart::Belly, 1.0f), Stroke(0, BodyPart::RightHand, 1.0f),
	                 Slap(0, 0.85f, true, false, 2.0f), Slap(0, 0.55f, false, true, 2.0f), Act(Kind::HandLetGo, 0, 3.0f, true)},
	    .repeatFrom = 0,
	});
	// The camera looks down on the north edge of the player's influence, 400 m from the middle: the bottom of the
	// screen is inside it, the rest beyond
	all.push_back({
	    .id = "hand.virtual_influence",
	    .name = "The hand acting past the influence border",
	    .facet = Facet::Hand,
	    .description = "The hand picks up a ball inside the player's influence, carries it past the border and lets it "
	                   "go, picks up a second ball just outside and drops it, then waits further out and tries a third "
	                   "ball once the share it keeps has run out, and comes back in.",
	    .expected = "Just past the border the hand still takes and lets go of the balls, keeping its inside look, with "
	                "the hum playing; the game log gives the share it keeps falling a tenth at a time. Once the log says "
	                "it has none, the third ball can't be taken and the hand changes to its outside look.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, 375.0f}, .look = {0.0f, 0.0f, 415.0f}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, 393.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {5.0f, 408.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {-5.0f, 430.0f}}},
	    .commands =
	        {// Inside, the hand takes the first ball
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.96f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         // Carries it past the border and lets it go there
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.0f, -0.25f}, .amount = 1.0f},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Takes the second ball, just outside, and lets it go
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.582f, 0.6f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Waits further out until nothing is left, then tries the third
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.44f, 0.02f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 10.0f, .point = {0.441f, 0.296f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         // And comes back in
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.3f, 0.97f}}},
	});
	// The camera looks down on a temple of player two, 700 m north of the middle and far past the player's influence.
	// The two players are allies.
	all.push_back({
	    .id = "hand.ally_influence",
	    .name = "The hand acting in the influence of an ally",
	    .facet = Facet::Hand,
	    .description = "Player two is an ally of the player, and a temple of player two stands far past the player's own "
	                   "influence. The hand goes over a ball near that temple, takes it, carries it a short distance and "
	                   "lets it go.",
	    .expected = "In the influence of the ally the hand has its inside look. It takes the ball and lets it go as it "
	                "does in the player's own influence. The game log gives no message that the hand is outside.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 620.0f}, .look = {0.0f, 0.0f, 680.0f}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, 670.0f}}},
	    .temples = {{.offset = {0.0f, 700.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .allies = {{.player = PlayerNames::PLAYER_ONE, .other = PlayerNames::PLAYER_TWO, .strength = 1.0f}},
	    .commands = {{.kind = Kind::PointerOnObject, .delaySeconds = 1.0f, .object = 0, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.1f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3}},
	});
	// The camera looks down on the north edge of the player's influence, 400 m from the middle: a wood pile and another
	// player's tiger just inside, a wood pile, another player's tiger and the player's own tiger just outside, all clear
	// of the testbed's window. The hand rests on the land between visits, so that each tooltip is submitted afresh. A
	// birch stands just inside, so tall that the land under the cursor on its crown lies outside.
	all.push_back({
	    .id = "hand.tooltips_influence",
	    .name = "The hand's tooltips and grabs either side of the influence border",
	    .facet = Facet::Hand,
	    .description = "The hand starts over the wood pile outside the player's influence, then goes over the other "
	                   "player's tiger outside and the player's own tiger outside. It comes in over the wood pile and the "
	                   "other player's tiger inside, then goes back out over the outside wood pile, rests out on the land "
	                   "for twenty seconds and tries the pile again. Then it presses on the crown of the birch standing "
	                   "just inside, against the land outside, drops what it took on the land inside, and holds the "
	                   "button down on the other player's tiger inside and then on the player's own tiger outside.",
	    .expected = "Outside, the wood pile shows no amount and the other player's tiger no \"Interact\"; the player's "
	                "own tiger shows \"Interact\" anywhere. Inside, the wood pile shows its amount and the other player's "
	                "tiger \"Interact\". Back out at once, the outside pile shows its amount, as the hand keeps some of "
	                "the player's influence past the border; once that has run out it shows nothing again. The birch is "
	                "pulled up and held, as it stands inside though the land under the cursor is outside. The other "
	                "player's tiger is never taken hold of, even inside (the log says so); the player's own tiger is, "
	                "outside, and the camera turns to it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 45.0f, 355.0f}, .look = {0.0f, 0.0f, 410.0f}},
	    .creatures = {{.label = "own outside",
	                   .species = CreatureType::Tiger,
	                   .offset = {48.0f, 430.0f},
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .size = 0.5f,
	                   .pauseMind = true},
	                  {.label = "other inside",
	                   .species = CreatureType::Tiger,
	                   .offset = {18.0f, 386.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 0.5f,
	                   .pauseMind = true},
	                  {.label = "other outside",
	                   .species = CreatureType::Tiger,
	                   .offset = {35.0f, 430.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 0.5f,
	                   .pauseMind = true}},
	    .objects = {{.type = PotInfo::WoodPile_1, .offset = {0.0f, 390.0f}, .amount = 300},
	                {.type = PotInfo::WoodPile_1, .offset = {15.0f, 428.0f}, .amount = 300},
	                {.type = TreeInfo::Birch, .offset = {30.0f, 396.0f}}},
	    .commands =
	        {// Outside: the wood pile, the other player's tiger, the land, the player's own tiger, the land
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.626f, 0.379f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.798f, 0.327f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.909f, 0.327f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         // Inside: the wood pile, the land, the other player's tiger, the land
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.702f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.62f, 0.9f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.759f, 0.71f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.62f, 0.9f}},
	         // Straight back out over the outside pile, out on the land until nothing is kept, and the pile again
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.626f, 0.379f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 20.0f, .point = {0.626f, 0.379f}},
	         // With nothing kept, the birch standing just inside, its top against the land outside: pulled up and held
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.887f, 0.547f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	         // Dropped on the land inside
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.62f, 0.9f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // The other player's tiger inside, held down on: never taken hold of
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.759f, 0.71f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	         // The player's own tiger outside, held down on: taken hold of
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.909f, 0.327f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3}},
	});
	// The camera stands as in the tutorial's throwing demonstration, 9 m over the land and 35 m behind the rock, which
	// sits in the middle of the screen; the flat land beyond leaves the throw room to land
	all.push_back({
	    .id = "hand.throw",
	    .name = "Throwing a rock",
	    .facet = Facet::Hand,
	    .description = "The hand picks up a rock, makes ready to throw low on the screen and flings it with a quick "
	                   "upward sweep of the pointer, in real time.",
	    .expected = "The rock leaves the hand at the hand's speed (the inspector's HandGrab lastThrow), the same at any "
	                "frame rate, and flies on the order of a hundred metres or more before it lands.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands =
	        {// Takes the rock under the middle of the screen
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	         // Makes ready low on the screen and sweeps up, letting go as the sweep ends
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The camera looks at a rock from 35 m. The hand takes the rock and throws it. The take and the throw are commands,
	// which the world does at the start of the next turn.
	all.push_back({
	    .id = "hand.commands_next_turn",
	    .name = "The hand's take and throw at the next turn",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on a rock until it takes it, then makes ready to throw and flings "
	                   "it with an upward sweep of the pointer. Step the frames with the inspector (game.step frames 1) "
	                   "and watch players.commands and the hand's HandGrab state.",
	    .expected = "After the wait the hand sends the take: its state is Taking, one command waits, and the rock is still on "
	                "the land, gripped by the hand. At the start of the next turn the rock is in the hand. After the "
	                "release the state is LettingGo and the rock stays in the hand until the next turn, then flies from "
	                "where the hand is, at the speed that the hand had at the release.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The reports of the hand come 300 ms late, as in a network game. The hand takes a rock and throws it.
	all.push_back({
	    .id = "hand.throw_lead",
	    .name = "The lead of a throw for a late report",
	    .facet = Facet::Hand,
	    .description = "The reports of the player's hand get a fixed latency of 300 ms. The hand holds the Action button on "
	                   "a rock until it takes it, then makes ready to throw and throws it with an upward sweep of the "
	                   "pointer.",
	    .expected = "At the release the rock stops in the hand's move and starts its flight at once. The hand does not "
	                "carry it. At the next turn the rock jumps forward along its flight, to where it is three turns "
	                "after the release, and flies on from there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::SetReportLatency, .delaySeconds = 0.0f, .value = 300},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The same throw with no latency of the reports, to compare with hand.throw_lead
	all.push_back({
	    .id = "hand.throw_no_lead",
	    .name = "A throw with no lead",
	    .facet = Facet::Hand,
	    .description = "The reports of the player's hand have no latency. The hand holds the Action button on "
	                   "a rock until it takes it, then makes ready to throw and throws it with an upward sweep of the "
	                   "pointer.",
	    .expected = "At the release the rock starts its flight from the hand. It flies on with no jump, as in a game alone "
	                "with no latency.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::SetReportLatency, .delaySeconds = 0.0f, .value = 0},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The camera looks at a cuddly toy and a toy ball on flat land from 35 m. The hand takes each toy in turn.
	all.push_back({
	    .id = "hand.hold_toys",
	    .name = "Holding toys",
	    .facet = Facet::Hand,
	    .description = "The hand takes the cuddly toy, holds it for three seconds and puts it down. Then it takes the "
	                   "toy ball and holds it.",
	    .expected = "The hand holds the cuddly toy from the side, as it holds a skittle. It holds the toy ball from "
	                "above on its palm, as it holds a bowling ball or a die.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, 0.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {0.0f, 6.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.75f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.41f, 0.48f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3}},
	});

	// The camera looks at a wood pile on flat land from 35 m. The hand scoops from it for four seconds.
	all.push_back({
	    .id = "hand.scoop_pile",
	    .name = "Scooping from a pile",
	    .facet = Facet::Hand,
	    .description = "The hand takes wood from a wood pile and keeps the button down for four seconds. Then it lets "
	                   "go of the button and moves away with the handful.",
	    .expected = "While it scoops, the hand stays above the pile with the side hold of the handful, as it holds the "
	                "handful after the scoop. It does not tip down. It stays as high above the pile as the handful "
	                "hangs below it, and at least 1.9 m.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = PotInfo::WoodPile_1, .offset = {0.0f, 0.0f}, .amount = 2000}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 4.0f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.7f}}},
	});

	// The camera looks at a rock on flat land from 35 m. The hand does one thing after another, with a pause between them.
	all.push_back({
	    .id = "hand.state_blend",
	    .name = "The hand blends when it changes what it does",
	    .facet = Facet::Hand,
	    .description = "The hand moves over the bare land, then over the rock. It takes the rock and puts it down. It "
	                   "grips the land and pulls the camera, then turns the camera with the middle button. Last, it "
	                   "gets a food seed. Two seconds between the steps.",
	    .expected = "At each change, every bone of the drawn hand moves in a straight line for 0.13 s from how it looked "
	                "before. A grip of the bare land blends from the pose that the hand kept at the change before. A "
	                "change of the camera animation shows almost no blend.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.35f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.65f, 0.75f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.35f, 0.8f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 1},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.1f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 2},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.15f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 2},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Food)}},
	});

	// The camera looks north over the land, a rock and a tiger to its east, all in the player's influence; the pointer stays
	// east of the middle, clear of the testbed's window
	all.push_back({
	    .id = "hand.cast_tooltips",
	    .name = "What the hand offers with a miracle in it",
	    .facet = Facet::Hand,
	    .description = "A food seed is put in the hand and the pointer goes to the bare land, then to a rock; then a storm "
	                   "seed over the land; then the creature spell that makes a creature big over the land and over a "
	                   "tiger. Six seconds each.",
	    .expected = "Food over the land: \"Cast\" with the right mouse button. Over the rock it offers nothing. The storm "
	                "over the land: \"Cast Circle\". The big spell over the land offers nothing; over the tiger, "
	                "\"Cast\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {Content(CreatureType::Tiger, {40.0f, 40.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {20.0f, 40.0f}}},
	    .commands = {{.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.75f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.66f, 0.45f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 6.0f, .value = static_cast<size_t>(SpellSeedType::Storm)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.75f, 0.8f}},
	                 {.kind = Kind::HoldSeed,
	                  .delaySeconds = 6.0f,
	                  .value = static_cast<size_t>(SpellSeedType::CreatureSpellBig)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.83f, 0.45f}}},
	});

	// The camera looks down on a row of things, and the pointer goes from one to the next
	all.push_back({
	    .id = "hand.tooltips_rest",
	    .name = "What the hand offers over the land, over things and with a thing in it",
	    .facet = Facet::Hand,
	    .description =
	        "The pointer rests on the bare land, then on the right edge of the screen and on its bottom edge. "
	        "Then it goes over a toy ball, a big rock, a hut that stands four tenths built and a whole hut. The hand then "
	        "takes the ball and the pointer rests on the land. Last, the hand puts the ball down, takes the "
	        "birch and the pointer rests on the land. Six seconds each.",
	    .expected =
	        "Over the land: \"Move\" with the left button and four arrows. At the right edge: \"Rotate\". At the "
	        "bottom edge: \"Pitch\" with the up and down arrows. Over the ball: \"Pick Up\" with the right "
	        "button. Over the big rock: \"Pick Up\" and \"Tap To Break\" in turns, or \"Tap To Break\" alone "
	        "when the hand cannot take it. Over the hut that is not built: \"Wood Required\" and \"Building Completed: 40%\" "
	        "in turns. Over the whole hut: \"Tap\". With the ball: \"Drop\" and \"Throw\" in turns, \"Throw\" "
	        "with four arrows. With the birch: \"Plant\" and \"Throw\" in turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-20.0f, 40.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {0.0f, 40.0f}, .scale = 2.0f},
	                {.type = AbodeInfo::CelticHut, .offset = {20.0f, 40.0f}, .built = 0.4f},
	                {.type = AbodeInfo::CelticHut, .offset = {40.0f, 40.0f}},
	                {.type = TreeInfo::Birch, .offset = {-40.0f, 40.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.995f, 0.5f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.995f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.483f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.656f, 0.485f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.813f, 0.488f}},
	                 {.kind = Kind::HandTake, .delaySeconds = 6.0f, .object = 0},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::HandRelease, .delaySeconds = 12.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 4},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.5f, 0.85f}}},
	    // The huts' town stays the player's: the hand shows the wood that a building wants only for the player's own
	    .townBeliefInPlayer = 1000.0f,
	});

	// The camera looks down on a row of huts that are not whole, and the pointer goes from one to the next
	all.push_back({
	    .id = "hand.pick_unbuilt",
	    .name = "The hand over buildings that are not built or not mended",
	    .facet = Facet::Hand,
	    .description =
	        "Four huts of the player's town stand in a row: one four tenths built, one a tenth built, one hurt to half its "
	        "life and one with nothing built. The pointer rests on each, six seconds each, a little above the ground where "
	        "the whole hut would stand. Then the hand taps the hut that is four tenths built. Last, the hand takes a pot "
	        "of wood, the pointer goes back on that hut and the hand gives the wood to it.",
	    .expected = "Over the first three huts the hand finds the hut: \"Wood Required\" and \"Building Completed\" take "
	                "turns. The hut a tenth built is found also where only its scaffold shows. Over the hut with nothing "
	                "built the hand finds nothing and shows \"Move\". The tap knocks on the hut. While the hand holds the "
	                "wood over the hut it shows \"Supply\". The wood goes onto the hut's building site and \"Wood "
	                "Required\" goes down by the wood given.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-20.0f, 40.0f}, .built = 0.4f},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, 40.0f}, .built = 0.1f},
	                {.type = AbodeInfo::CelticHut, .offset = {20.0f, 40.0f}, .life = 0.5f},
	                {.type = AbodeInfo::CelticHut, .offset = {40.0f, 40.0f}, .built = 0.0f},
	                {.type = PotInfo::WoodPot, .offset = {0.0f, 25.0f}, .amount = 100}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.656f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.813f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::HandTake, .delaySeconds = 3.0f, .object = 4},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 6.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	    .townBeliefInPlayer = 1000.0f,
	});

	// The camera looks down at the player's creature, and the pointer grips the land and then rests on the creature
	all.push_back({
	    .id = "hand.grip_and_creature_tips",
	    .name = "What the hand offers as it grips the land, and over the player's creature",
	    .facet = Facet::Hand,
	    .description = "The pointer goes on the land in front of the player's tiger and holds the Move button. A second "
	                   "later it also holds the Action button, for six seconds. Then it lets go of both buttons and rests "
	                   "on the tiger for six seconds.",
	    .expected = "While both buttons are held the hand shows \"Zoom\" with the mouse of both buttons and the up and "
	                "down arrows. Over the tiger, in the player's influence, the hand shows \"Interact\" with the mouse "
	                "of the Action button.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -40.0f}, .look = {0.0f, 0.0f, 15.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 15.0f})},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 6.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.45f}}},
	});

	// The camera looks along the land just ahead, so the hand is seen up close in the sun
	all.push_back({
	    .id = "hand.lighting",
	    .name = "The hand in the light",
	    .facet = Facet::Hand,
	    .description = "The bare hand sits still near the camera while the player turns evil, then good, ten seconds each.",
	    .expected = "The hand is lit as the game lights the models it poses: the side facing the light at full strength, the "
	                "side away from it never darker than two fifths, in the land's light half as bright again, whatever its "
	                "evil or good shape and skin.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 1.0f, 4.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.7f}},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 8.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 10.0f, .alignment = 1.0f}},
	});

	// The camera looks along the land ahead and nothing moves the pointer, so the hand shows where it starts
	all.push_back({
	    .id = "hand.spawn_point",
	    .name = "The hand before the mouse moves",
	    .facet = Facet::Hand,
	    .description = "A food store in the middle of the view is looked at from a little way up and the pointer is left "
	                   "alone. Run it as the game starts (ob-build run --scenario hand.spawn_point) without touching the "
	                   "mouse.",
	    .expected = "From the first frame the hand is under the cursor when the cursor is over the window. When the "
	                "cursor has never been over the window the pointer is the window's top left corner, as in the "
	                "original, so the hand is off along the line through that corner until the cursor comes over the "
	                "window. An agent holding the input puts it under the middle of the window, over the food store.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, -40.0f}, .look = {0.0f, 0.0f, 10.0f}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, 10.0f}}},
	});

	// The camera looks down at the land just ahead, so the hand is seen up close
	all.push_back({
	    .id = "hand.miracle_glow",
	    .name = "The glow over a hand holding a miracle",
	    .facet = Facet::Hand,
	    .description = "A food seed is put in the hand, which sits still near the camera. Then the player turns evil, "
	                   "then good, ten seconds each.",
	    .expected = "The flowing sheet runs over the hand in the player's colour, shaded as the hand is (brighter on the "
	                "side facing the light), added over it at four fifths and lying exactly on its skin whatever its "
	                "evil or good shape.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 1.0f, 4.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.7f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 8.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 10.0f, .alignment = 1.0f}},
	});

	// The camera looks north over the land from high up; the pointer starts east of the middle, clear of the testbed's
	// window
	all.push_back({
	    .id = "hand.held_miracle_draw",
	    .name = "Miracles that follow the hand as it moves",
	    .facet = Facet::Hand,
	    .description = "The hand gets a water seed and casts it with the right mouse button held. The pointer moves left "
	                   "and right, then to the far edge of the testbed and back, then lets go. Then the hand casts "
	                   "food, wood, lightning and fire, and the pointer moves left and right.",
	    .expected = "The rain cloud and its rain cone stay exactly on the moving hand, with no lag. The food grains "
	                "and the logs fall from where they come out and do not move with the hand. The start of the "
	                "lightning and the fireball in the hand stay on the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -30.0f}, .look = {0.0f, 10.0f, 30.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.6f, 0.8f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Water)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, -0.78f}, .amount = 1.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 2.0f, .point = {0.0f, 0.78f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Wood)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::LightningBolt)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Fire)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3}},
	});

	// The camera looks down on an oak from about 150 m, as in play, the pointer on its crown
	all.push_back({
	    .id = "hand.tree_tug",
	    .name = "Pulling a tree out of the ground",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m up, the hand grips a large oak by its crown, pulls it up the screen until "
	                   "it comes out of the ground, carries it a little, then puts it down.",
	    .expected = "The hand hovers on the crown, drops to the trunk as it takes hold, and the oak leans and stretches "
	                "towards the pull. When it comes out, leaving its roots hole, the hand and the oak move together to "
	                "under the pointer; the hand never leaves the tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 110.0f, -80.0f}, .look = {0.0f, 4.0f, 20.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 20.0f}, .scale = 1.6f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.47f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.25f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	});

	// The camera looks down at 45 degrees on a large oak from about 150 m, the pointer on its crown. The oak is heavy, so
	// the hand must pull it for a time before it comes out.
	all.push_back({
	    .id = "hand.tree_tug_heavy",
	    .name = "Pulling a heavy tree out of the ground",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m away at 45 degrees, the hand grips a very large oak, pulls it slowly up the "
	                   "screen until it comes out of the ground, then carries it.",
	    .expected = "The oak leans and stretches towards the pull. When it comes out, the hand and the oak move together "
	                "to under the pointer. On no frame is the hand drawn away from the oak.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 106.0f, -66.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 40.0f}, .scale = 2.5f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.42f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.3f}, .amount = 3.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 3.5f, .value = 3}},
	});

	// The camera looks down at 45 degrees on a very large oak from about 150 m. The hand pulls it for a tenth of a second
	// and lets go before it comes out, so the oak stays drawn in the lean of the pull.
	all.push_back({
	    .id = "hand.tree_tug_let_go",
	    .name = "Letting go of a tree before it comes out",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m away at 45 degrees, the hand grips a very large oak, pulls it up the screen "
	                   "for a tenth of a second, then lets go before the oak comes out of the ground.",
	    .expected = "The oak leans and stretches a little towards the pull. After the hand lets go, the oak stays in "
	                "that lean in a game alone. In a network game, it stands straight again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 106.0f, -66.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 40.0f}, .scale = 2.5f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.42f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.8f, 0.8f}}},
	});

	// The camera looks at a conifer from 25 m, 20 degrees down, the pointer on its crown. A press of a fifth of a second
	// is shorter than a tap, but the pull starts after the 0.13 s fade and the hand is far from its grip on the trunk.
	// Thus the conifer comes free on the first frame of the pull and the hand takes it. The same press does the same in
	// a network game.
	all.push_back({
	    .id = "hand.tree_short_press",
	    .name = "A short press on a conifer",
	    .facet = Facet::Hand,
	    .description = "Seen from 25 m, 20 degrees down, the hand presses on the crown of a conifer for a fifth of a "
	                   "second and lets go.",
	    .expected = "The conifer comes out of the ground on the first frame of the pull and leaves its roots hole. The "
	                "hand holds it after the button comes up. The conifer does not stay leaning in the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.55f, -3.49f}, .look = {0.0f, 4.0f, 20.0f}},
	    .objects = {{.type = TreeInfo::ConiferA, .offset = {0.0f, 20.0f}, .scale = 1.03f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.35f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	});

	// The camera looks down on a few people and animals from about 25 m, as in play: three people stand still at different
	// distances, two more walk to and fro, two stand by an oak, and a cow, a sheep, a horse and a pig graze to the left
	all.push_back({
	    .id = "hand.pick_living",
	    .name = "Picking up people and animals",
	    .facet = Facet::Hand,
	    .description = "Three people stand still near, middling and far from the camera, two walk to and fro across the "
	                   "view, two stand by an oak, and a cow, a sheep, a horse and a pig graze. Move the hand near "
	                   "each and press to pick them up.",
	    .expected = "A person is taken whenever the pointer is inside the circle their bounding ball makes on the screen, "
	                "not only on their body: a little beside, above or below a person still picks them. Of two people "
	                "under the pointer the nearer to the camera is taken; by the oak the person is taken rather than "
	                "the tree unless the pointer is on the tree nearer the camera. Animals are taken only with the "
	                "pointer on their bodies.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -25.0f}, .look = {0.0f, 0.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 0.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {3.0f, 8.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, 22.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {-12.0f, 12.0f},
	                 .walkTo = glm::vec2(12.0f, 12.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 12.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {12.0f, 16.0f},
	                 .walkTo = glm::vec2(-12.0f, 16.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 12.0f},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 3.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {8.5f, 2.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {11.5f, 2.0f}, .inScript = true},
	                {.type = AnimalInfo::Cow, .offset = {-14.0f, 2.0f}, .living = true},
	                {.type = AnimalInfo::Sheep, .offset = {-9.0f, -3.0f}, .living = true},
	                {.type = AnimalInfo::Horse, .offset = {-12.0f, 8.0f}, .living = true},
	                {.type = AnimalInfo::Pig, .offset = {-6.0f, 5.0f}, .living = true}},
	});

	// One of each kind of thing the cursor picks by its own shape, in a row on bare land
	all.push_back({
	    .id = "hand.pick_kinds",
	    .name = "What the cursor picks, kind by kind",
	    .facet = Facet::Hand,
	    .description = "In a row on bare land, left to right: a person, a pig, an oak, a \"did you know\" sign, a silver "
	                   "scroll, a fire dispenser floating its globe, and a lightning globe on its own; behind them, a "
	                   "vortex between the lands. Move the pointer round each and press the Action button on it.",
	    .expected = "A person is picked anywhere in the circle their bounding ball makes, at that ball's near side; the "
	                "pig, the oak and the sign only on their bodies. The scroll is picked on its model or a little "
	                "round it, whichever is nearer. The dispenser is picked on its building and in a wide ball round "
	                "its globe, and a press there knocks on it; on the globe itself the globe is picked, being pushed "
	                "towards the camera, and a quick press puts its miracle in the hand. The loose globe is picked on "
	                "its model or within a ball about its middle 1.2 times its size (moved by the middle of the miracle's "
	                "model it shows). The vortex is picked anywhere within a ball at its middle seven tenths as wide as "
	                "its cylinder, though the land in front of it wins.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -28.0f}, .look = {0.0f, 3.0f, 8.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 6.0f}, .inScript = true},
	                {.type = AnimalInfo::Pig, .offset = {-13.0f, 6.0f}},
	                {.type = TreeInfo::Oak, .offset = {-7.0f, 10.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {0.0f, 45.0f}, .delaySeconds = 0.5f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {11.0f, 10.0f}},
	                   {.type = MagicType::LightningBolt, .offset = {19.0f, 6.0f}, .bubbleHeight = 3.0f}},
	    .highlights = {{.kind = 1, .offset = {-1.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}},
	                   {.kind = 2, .offset = {4.0f, 6.0f}}},
	});
	// A healing dispenser floats its globe in the middle of the screen, and a second healing globe floats on its own to
	// its left, both in the player's influence on flat land
	all.push_back({
	    .id = "hand.globe_carry",
	    .name = "Carrying and throwing one-shot globes",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on the dispenser's globe until it takes it, carries it to the "
	                   "right and lets it go gently. Then it takes the globe floating on its own and throws it with a "
	                   "quick upward sweep of the pointer.",
	    .expected = "Held past a tap, each globe is lifted from where it floats and carried hanging from the side of the "
	                "hand, still turned to the camera with its miracle spinning inside; the dispenser floats a new globe "
	                "once its time has passed. Let go gently, the globe falls to the land and settles there, whole. Thrown, it "
	                "flies off "
	                "like any loose thing and lands, whole: a globe never bursts or casts by landing. A quick press on a "
	                "globe still puts its miracle in the hand instead.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}},
	                   {.type = MagicType::Heal, .offset = {0.0f, 6.0f}, .bubbleHeight = 3.0f}},
	    .commands =
	        {// Takes the dispenser's globe, holding the button past a tap
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.31f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         // Carries it to the right and lets it go there gently
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.2f, 0.1f}, .amount = 1.5f},
	         {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Takes the lone globe and throws it with a quick upward sweep
	         {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.26f, 0.37f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// A healing dispenser floats its globe in the middle of the screen, in the player's influence on flat land. The hand
	// takes the globe and throws it at once.
	all.push_back({
	    .id = "hand.globe_throw",
	    .name = "Throwing a one-shot globe",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on the dispenser's globe until it takes it. Then it makes ready "
	                   "low on the screen and throws the globe with a quick upward sweep of the pointer.",
	    .expected = "The globe leaves the hand turned as it was last drawn, with its dome to the camera, and from the same "
	                "place. It does not jump or turn as it leaves the hand. It flies off and lands, whole.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}}},
	    .commands =
	        {// Takes the dispenser's globe, holding the button past a tap
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.31f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         // Makes ready low on the screen and sweeps up, letting go as the sweep ends
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});
	// A dispenser floats a heal globe in front of the camera, in the player's influence, and the pointer rests on it
	all.push_back({
	    .id = "hand.globe_tooltip",
	    .name = "The name of the miracle over a one-shot globe",
	    .facet = Facet::Hand,
	    .description = "The pointer rests on the land for three seconds, then on the dispenser's globe for six seconds.",
	    .expected = "Over the land: \"Move\". Over the globe: the name of its miracle (the heal miracle) at once, with no "
	                "mouse button, and no \"Pick Up\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.31f}}},
	});
	// A "did you know" sign stands in front of the camera. The hand taps it, and its tip bubble comes up.
	all.push_back({
	    .id = "hand.bubble_scroll_tooltip",
	    .name = "The tooltip over the tip bubble of a sign",
	    .facet = Facet::Hand,
	    .description = "The pointer goes to the sign and taps it with a quick press of the Action button. Then move the "
	                   "pointer onto the tip bubble, and hold the Action button on it.",
	    .expected = "Over the bubble, and while the hand grips it: \"Scroll\" with the Action button and the up and down "
	                "arrows. Over the land: \"Move\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -20.0f}, .look = {0.0f, 3.0f, 6.0f}},
	    .highlights = {{.kind = 1, .offset = {0.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	// A tree stands between the camera and the tip bubble of a sign
	all.push_back({
	    .id = "hand.bubble_behind_tree",
	    .name = "A tree in front of a tip bubble hides it",
	    .facet = Facet::Hand,
	    .description = "The pointer goes to a \"did you know\" sign and taps it. An oak stands to its right, between the "
	                   "camera and the bubble that comes up over the sign.",
	    .expected = "The oak hides the part of the bubble's box and words that is behind its trunk and leaves. Between "
	                "the leaves the bubble shows. The blinking arrows and the pictures of the controls show over the "
	                "tree. Turn the camera to look down: more of the bubble then shows, because the bubble comes "
	                "nearer to the camera as it looks down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 3.0f, -14.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {5.0f, -2.0f}}},
	    .highlights = {{.kind = 1, .offset = {0.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	// The hand pulls a conifer out of the ground: the tree leaves a pile of roots in its hole, which fades away
	all.push_back({
	    .id = "hand.pull_tree_roots_hole",
	    .name = "Tree pulled free leaves a hole of roots",
	    .facet = Facet::Hand,
	    .description =
	        "A conifer stands at the middle of the screen. After a second the pointer goes onto its trunk, "
	        "presses the Action button and moves up, so the hand pulls the tree. Two seconds later the button comes up.",
	    .expected = "The tree leans to the hand, comes free and goes into the hand with a puff of dust. A pile of roots "
	                "and earth stays in the hole where the tree stood, on the shape of the land. After 14 seconds of "
	                "game time the pile fades, and one second later it is gone. Only this computer draws the pile. The "
	                "inspector query magic.state shows it under roots_piles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 5.0f, 10.0f}},
	    .objects = {{.type = TreeInfo::Conifer, .offset = {0.0f, 10.0f}}},
	    .commands = {{.kind = Kind::PointerOnObject, .delaySeconds = 1.0f, .object = 0, .amount = 6.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.2f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.25f}, .amount = 0.6f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3}},
	});
}

void AddLeash(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "leash.types",
	    .name = "Led on each leash",
	    .facet = Facet::Leash,
	    .description = "A tiger is put on the learning leash, then the aggression leash, then the compassion leash, a "
	                   "while each, and the leash is taken off. Move the hand about to lead it.",
	    .expected = "A rope runs from the hand to its collar, coloured for each leash; on the aggression leash anger is "
	                "what it wants most, on the compassion leash kindness and making friends, and on the learning leash "
	                "it watches the player. Taken off the compassion leash, that mood ends at once and its other wants "
	                "come back; taken off the learning leash, nothing it wants changes.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Leash(0, LeashType::Evil, 10.0f), Leash(0, LeashType::Good, 10.0f),
	                 Act(Kind::TakeOffLeash, 0, 10.0f), Act(Kind::Stop, 0, 3.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.orders",
	    .name = "Orders given with the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is told with the Action button to go to a point to the east, told "
	                   "again before it gets there, then to act on a rock, a cow, and the lake.",
	    .expected = "Each order is answered with the acknowledge sound, a nod and the tiger's voice, and marked: six pale "
	                "yellow sparkles over the place, and a throbbing yellow ring with a paw print inside, which follows "
	                "the cow. Told again it runs and then waits a few seconds; the rock it looks at, the cow it picks "
	                "up and holds, at the lake it drinks or looks at its reflection. One marker at a time.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {-40.0f, 30.0f}},
	                {.type = AnimalInfo::Cow, .offset = {30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 2.0f, false), OnObject(Kind::LeashOrderOn, 0, 0, 10.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 1, 12.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 15.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 15.0f)},
	    .repeatFrom = 0,
	});

	// A man held by a script out in the lake, as the drowning men of Land 1's silver scroll, and a tiger that shrinks
	// in its temple's pen first, so that it grows again on its way out to him
	constexpr glm::vec2 k_RescueTemple {-150.0f, 150.0f};
	constexpr glm::vec2 k_DrowningMan {-30.0f, k_NearShallows + 8.0f};
	constexpr glm::vec2 k_Bank {-30.0f, k_NearShallows - 20.0f};
	all.push_back({
	    .id = "leash.rescue_from_water",
	    .name = "A man carried out of the water on the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 0.5 walks home into its temple's pen, where it shrinks, and then goes on the "
	                   "learning leash. The Action button of the leash orders it onto a man that a script holds still out "
	                   "in the lake, as the drowning men of Land 1, and some time later onto the bank.",
	    .expected = "It grows back to its own size as it leaves the pen, wades out, picks the man up and holds him, "
	                "facing the player. Told onto the bank, it carries him there and puts him down gently on his feet, "
	                "with no toss: he stays where its hand sets him.",
	    .framing = {.shot = Shot::Overview, .include = {k_RescueTemple, k_DrowningMan, k_Bank}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, k_RescueTemple + glm::vec2(60.0f, -40.0f));
		    tiger.size = 0.5f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_DrowningMan, .inScript = true}},
	    .temples = {{.offset = k_RescueTemple, .angle = 36.0f}},
	    .commands = {Act(Kind::WalkHome, 0, 1.0f), Leash(0, LeashType::Rope, 25.0f),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 1.0f, false), Go(Kind::LeashOrderAt, 0, k_Bank, 45.0f, false)},
	});

	// A full size tiger stops its walk to a man just inside the reach of its hand, which is less straight ahead, so it
	// turns on the spot before it reaches
	constexpr glm::vec2 k_StillMan {40.0f, -40.0f};
	all.push_back({
	    .id = "leash.pick_up_turns_into_reach",
	    .name = "A full size creature turns to get a man in reach",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 1 goes on the learning leash. The Action button of the leash orders it onto a "
	                   "man that a script holds still on the land in front of it.",
	    .expected = "It walks up to the man and stops a hand's reach from him. It turns a little on the spot, so that "
	                "the man is in front of its hand, then picks him up and holds him. Before, it reached straight "
	                "ahead with nothing in its hand.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 20.0f}, k_StillMan}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, {0.0f, 20.0f});
		    tiger.size = 1.0f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_StillMan, .inScript = true}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false)},
	});

	// Worn out and wanting to sleep more than anything, far from its player's temple
	CreatureSetup tired {.species = CreatureType::Tiger,
	                     .offset = {0.0f, 20.0f},
	                     .needs = {.energy = 1.0f, .exhaustion = 0.95f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                     .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}};
	all.push_back({
	    .id = "leash.tired_orders",
	    .name = "Orders given with the leash to a tired creature",
	    .facet = Facet::Leash,
	    .description = "A worn out tiger (exhaustion 0.95) that wants to sleep is on the learning leash, far from its "
	                   "player's temple. After eight seconds it is told to go to a point to the east, and after twenty "
	                   "seconds to a point to the west.",
	    .expected = "It sets off home to sleep. Each order stops that: it nods and walks to the place it is told. Once "
	                "there and the order is done, it sets off home to sleep again.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {tired},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {-60.0f, 40.0f}, 12.0f, false)},
	});

	// Wholly exhausted, and as young as a creature on the first land, too young to faint
	auto exhausted = tired;
	exhausted.needs.exhaustion = 1.0f;
	exhausted.phase = 4;
	// It stays exhausted though it sleeps
	exhausted.hold = true;
	all.push_back({
	    .id = "leash.exhausted_orders",
	    .name = "Orders given with the leash to a wholly exhausted creature",
	    .facet = Facet::Leash,
	    .description = "A young, wholly exhausted tiger (exhaustion 1) is on the learning leash, far from its player's "
	                   "temple. It is too young to faint, and stays exhausted. After eight seconds it is told to go to a "
	                   "point to the east.",
	    .expected = "It takes no order: no nod, no marker, and it does not go east. The leash still keeps it near: if "
	                "it strays too far on its way to sleep, it walks back. Then it sleeps. An older creature this "
	                "exhausted faints and lies out cold.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {exhausted},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false)},
	});

	all.push_back({
	    .id = "leash.order_hurries",
	    .name = "An order with the leash hurries what the body does",
	    .facet = Facet::Leash,
	    .description = "Two tigers sit side by side. The left tiger is on the learning leash. After eight seconds, the "
	                   "left tiger is told to go to a point to the east. At the same time, the right tiger is told to "
	                   "stand up, with no order.",
	    .expected = "The left tiger gets up at two times the usual speed, nods, and walks east. The right tiger gets up "
	                "at the usual speed. A kiss, a cast and a throw keep their usual speed when an order comes.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 20.0f}, {60.0f, 40.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 20.0f}), Content(CreatureType::Tiger, {15.0f, 20.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Act(Kind::SitDown, 0, 1.0f), Act(Kind::SitDown, 1, 0.0f),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 8.0f, false), Act(Kind::StandUp, 1, 0.0f)},
	});

	all.push_back({
	    .id = "leash.marker_flat_and_slope",
	    .name = "The marker of a leash order on flat land and on a slope",
	    .facet = Facet::Leash,
	    .description = "A tiger far away on the learning leash is told with the Action button to go to a point on the "
	                   "flat land. Twenty seconds later it is told to go to a point halfway down the steep bank of the "
	                   "lake.",
	    .expected = "Each time a yellow ring hangs two metres above the point, with a paw print inside. The ring and the "
	                "print always face the camera and do not lie along the land, also on the bank. The ring throbs, its "
	                "width and its height a fifth of a second apart. The print stands on its side, with its top to the "
	                "left, and it is narrower than the ring.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, -40.0f}, {0.0f, k_NearShallows - 28.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-150.0f, -100.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {40.0f, -40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows - 28.0f}, 20.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 30.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.step_looks_reflection",
	    .name = "Looks at its reflection with the faces of each step",
	    .facet = Facet::Mind,
	    .description = "A tiger that is not thirsty, on the learning leash, away from the lake, is told with the Action "
	                   "button to go to the lake.",
	    .expected = "It walks to the water of the nearest region of sea on its map of the land, its head on the place. "
	                "Then it looks down at the water four metres in front of its feet, puzzled, plays its dazed clip "
	                "with an amazed face, and looks down again, puzzled. It walks to the nearest coast with an amazed "
	                "face, looks down with a curious face, plays its dazed clip, walks back to the water amazed, and "
	                "looks down, first curious and then amazed. The coast and the water are near each other, so these "
	                "two walks are short or stop at once.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -60.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, -60.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 2.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 45.0f)},
	});

	all.push_back({
	    .id = "mind.step_looks_set_fire",
	    .name = "Sets fire to a tree with the looks and faces of each step",
	    .facet = Facet::Mind,
	    .description = "A script tells a player's lion to set fire to an oak with a burning villager beside it.",
	    .expected = "It walks to the villager, its head on the villager, and picks him up. It walks to the oak, its head "
	                "on the oak. It turns to the oak with an angry face and flutters its eyelids at it, throws the villager "
	                "at it with a smile, watches him until he lands, then holds still a second with an angry face.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-20.0f, 10.0f}, .alight = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SetFireToObject", .with = 1}},
	});

	all.push_back({
	    .id = "senses.look_at_mover",
	    .name = "Looks at a thing that moves as it walks",
	    .facet = Facet::Senses,
	    .description = "A tiger on the learning leash is told to go to a point far to the north. Behind it, two "
	                   "villagers walk away to the west and to the east.",
	    .expected = "On its way the head looks at the point for two seconds. For the next two seconds it turns back to "
	                "a villager that walks, at the place where the tiger last saw the villager move. Then it looks from "
	                "side to side for two seconds. Near the point it looks only at the point.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 110.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticForesterMale,
	                 .offset = {-30.0f, -5.0f},
	                 .walkTo = glm::vec2(-90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {30.0f, -5.0f},
	                 .walkTo = glm::vec2(90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, 110.0f}, 2.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 0.0f}, 30.0f, false)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "senses.hang_around_at_home",
	    .name = "Hangs around at home",
	    .facet = Facet::Senses,
	    .description = "A tiger kept within 40 of its home at the temple is told to hang around at home, again and again. "
	                   "A birch stands to the west as the thing that the order names.",
	    .expected = "It walks home and waits for two to five seconds while it looks about. Then, one time in ten, it yawns. "
	                "Seven times in ten, it walks 32 out from home away from the temple, faces away from the temple and "
	                "sits or points. Then it turns to the temple and sits for ten to fifteen seconds. Two times in ten, it "
	                "only turns to the temple and sits.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-60.0f, 0.0f}}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HangAroundAtHome"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 0, .action = "HangAroundAtHome"}},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "leash.temple_leashes",
	    .name = "The leashes hanging on the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The temple's three leashes are hung in a row in front of "
	                   "the camera; then it is taught the aggression and compassion leashes, the aggression leash is "
	                   "tapped, and tapped again.",
	    .expected = "At first only the learning leash hangs, a twisted rope collar in a white smoky glow, tumbling slowly "
	                "with its texture running round it. Taught the others, the spiked blade (aggression) and the rainbow "
	                "fur (compassion) hang beside it. Tapped with the click sound, the aggression leash is carried in the "
	                "hand and its glow at the temple turns orange; tapped again it hangs back in place, silently. The "
	                "hand over each names it: Leash Of Aggression, Leash Of Learning, Leash Of Compassion.",
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 6.0f, 26.0f}, .look = {9.0f, 5.0f, 40.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 70.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 Go(Kind::HangLeashPosts, 0, {14.0f, 40.0f}, 0.5f, false),
	                 Leash(0, LeashType::Evil, 4.0f),
	                 Leash(0, LeashType::Good, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0}},
	});

	const auto templeGlows = [](std::string_view id, std::string_view name, float hour) {
		return Scenario {
		    .id = id,
		    .name = name,
		    .facet = Facet::Leash,
		    .description = "Your tiger knows all three leashes and your temple stands in front of the camera, its leash posts "
		                   "in view. The learning leash is picked at its post, so the hand carries it.",
		    .expected = "The aggression leash (the spiked blade) and the compassion leash (the rainbow fur) tumble slowly on "
		                "their posts, each in a pale smoky glow about four metres across, half seen by day and fading at "
		                "night. By the empty learning post the glow is orange and added to what is behind it. The learning "
		                "leash, a twisted rope collar, hangs round the hand's wrist, turned with the hand, its texture "
		                "running round it. Tapping the orange glow puts the leash back on its post, silently; with the hand "
		                "put away the carried leash and its orange glow are gone.",
		    .environment = {.hour = hour, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
		    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
		    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
		    .commands = {Leash(0, LeashType::Rope, 0.5f),
		                 Leash(0, LeashType::Evil, 0.5f),
		                 Leash(0, LeashType::Good, 0.5f),
		                 Act(Kind::TakeOffLeash, 0, 0.5f),
		                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
		};
	};
	all.push_back(templeGlows("leash.temple_glows", "The glows round the temple's leashes by day", 12.0f));
	all.push_back(templeGlows("leash.temple_glows_night", "The glows round the temple's leashes at night", 23.0f));

	all.push_back({
	    .id = "leash.l_key_picks_post",
	    .name = "The leash key picks the leash at the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes and your temple stands in front of the camera. The compassion "
	                   "leash is tapped at its post, then tapped again, so no leash is picked. Then the leash key puts "
	                   "the compassion leash on. Last, the leash key takes it off.",
	    .expected = "After the second tap, no glow is orange and the hand carries no leash. When the leash key puts the "
	                "leash on, the compassion leash (the rainbow fur) goes round the hand's wrist again and its glow "
	                "turns orange. When the leash comes off, the leash stays in the hand and its glow stays orange.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 2},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 2.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "leash.pull_to_hand",
	    .name = "Pulled to the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger far off to the north is put on the learning leash. Put the hand on the land near the "
	                   "camera, well away from it.",
	    .expected = "When the tiger is further from the hand than the full length of the leash, it stops what it does "
	                "and walks back. It is faster when it is further away. It walks to the point below the hand. When it "
	                "goes out of the area again, it walks back again.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 150.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f)},
	});

	all.push_back({
	    .id = "leash.walk_back_on_the_way",
	    .name = "Walked back while on the way",
	    .facet = Facet::Leash,
	    .description = "A tiger starts in the middle of the testbed. It is kept within 400 units of where it stands, as "
	                   "a young creature is kept near its home. After one second its player orders it to look at its "
	                   "reflection, and it walks north to the shore of the lake. After six seconds, on its way, the "
	                   "area that holds it becomes 10 units wide.",
	    .expected = "When the area becomes small, the tiger is out of it. It turns at once and walks back to where it "
	                "started. It does not walk on to the lake first.",
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -20.0f}, {60.0f, 160.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	    }},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 400.0f},
	                 {.kind = Kind::ForceAction, .delaySeconds = 1.0f, .action = "LookAtReflection"},
	                 {.kind = Kind::ConfineToHome, .delaySeconds = 6.0f, .radius = 10.0f}},
	});

	all.push_back({
	    .id = "leash.tied",
	    .name = "Tied to a tree and a rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and told to walk far off, untied back to the "
	                   "hand, then tied to a pillar of rock and told to walk far off again.",
	    .expected = "The rope runs from what it is tied to, and it is kept within the rope's length of it however far "
	                "it is told to walk; untied, the rope goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 60.0f}, {120.0f, 60.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Go(Kind::WalkTo, 0, {-120.0f, 60.0f}, 1.0f, false), Act(Kind::UntieLeash, 0, 8.0f),
	                 OnObject(Kind::TieLeash, 0, 1, 2.0f, false), Go(Kind::WalkTo, 0, {120.0f, 60.0f}, 1.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.hand_attach_sounds",
	    .name = "Attach sounds of the leash in the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and untied back to the hand two times, every "
	                   "four seconds.",
	    .expected = "Each tie and each return to the hand plays a leash-attach sound. The two sounds play in turn. The "
	                "first return to the hand plays the first sound, and the second return plays the second sound.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f), OnObject(Kind::TieLeash, 0, 0, 4.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.tied_thrown",
	    .name = "Tied to a thrown rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a rock of nobody's, which is then thrown off to the "
	                   "east, every eight seconds.",
	    .expected = "The leash stays tied to the rock as it flies and after it lands: the rope runs from the rock "
	                "throughout and never goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 10.0f}, {60.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	    .throws = {{.object = 0, .height = 4.0f, .velocity = {12.0f, 8.0f, 0.0f}, .delaySeconds = 3.0f, .repeatSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "leash.tie_by_hand",
	    .name = "Ordering and tying with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, a tree and a pillar of rock stand ahead. With the hand: "
	                   "right click the tree once, then double click it, then double click it again; right click the "
	                   "rock, and double click the rock.",
	    .expected = "Over the tree with the leash in the hand the tip says Attach Leash. A single click sends the tiger to "
	                "act on the tree (marked); the double click ties the leash to it, the rope running from the tree, and "
	                "over the tree the tip now says Detach Leash; double clicked again it comes back to the hand. The "
	                "same for the rock. The hand never pulls the tree up on a quick click.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {40.0f, 60.0f}, {0.0f, 20.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 60.0f}}},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.snap_rope",
	    .name = "Snap the tied leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger's leash is tied to a tree. Put the hand on the rope (the hand opens its fingers), hold "
	                   "the right button to take hold of it, drag it out over the land and let go.",
	    .expected = "Over the rope the hand opens its fingers to pinch it and no tip shows. Holding it, the fingers pinch "
	                "it and the rope runs as two, one from the collar and one from the tree, both to the hand. Let go, the "
	                "tiger is sent to the place under the hand (marked) and is kept within the distance from the tree to "
	                "where the hand let go.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {60.0f, 20.0f}, {0.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	});

	// The hand that holds the rope of the tied leash stands straight up, also when the pointer leaves the rope
	all.push_back({
	    .id = "hand.rope_hold_stand",
	    .name = "The hand that holds the rope of the tied leash",
	    .facet = Facet::Hand,
	    .description = "Your tiger's leash is tied to a tree, and the tiger lies down to sleep. The pointer goes onto the "
	                   "rope and the Action button takes hold of it. The pointer then moves away from the rope, across "
	                   "the lake onto its far bank, and the button comes up. Then the pointer moves a little to the "
	                   "right.",
	    .expected = "Holding the rope, the hand pinches it and stands straight up, not turned to the slope of the bank, "
	                "also away from the rope. It does not offer to turn the camera. Let go, it turns to the slope when the "
	                "pointer moves across the screen.",
	    .framing = {.shot = Shot::Placed, .eye = {-20.0f, 40.0f, 20.0f}, .look = {-20.0f, 0.0f, 120.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 75.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 100.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::Sleep, 0, 0.5f),
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 5.0f},
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.12f, -0.3f}, .amount = 4.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 5.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.03f, 0.0f}, .amount = 0.5f}},
	});

	all.push_back({
	    .id = "leash.hand",
	    .name = "Leash your creature with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger stands to the north. The hand clicks it with the right button, putting the learning "
	                   "leash on. Move the hand about to lead it; press L or shake the hand to take the leash off "
	                   "again, or right click it yourself to put it back on.",
	    .expected = "A rope runs from the hand to its collar; pulled taut, the tiger walks to the hand. The readout says "
	                "\"on\".",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "audio.leash_tie_placed",
	    .name = "The tie sound of another player's leash",
	    .facet = Facet::Audio,
	    .description = "Your tiger on the left is tied to a tree. Then player two's lion on the right is tied to a "
	                   "pillar of rock.",
	    .expected = "The tie of your tiger's leash is heard in the middle, with no place. The tie of the lion's leash "
	                "is heard from the rock on the right.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 70.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 70.0f}}},
	    .commands = {OnObject(Kind::TieLeash, 0, 0, 1.0f, false), OnObject(Kind::TieLeash, 1, 1, 3.0f, false)},
	});

	all.push_back({
	    .id = "leash.someone_elses",
	    .name = "Another player's creature refuses the leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left and player two's lion on the right. Your hand taps the lion, then your "
	                   "tiger.",
	    .expected = "The lion is refused, as it belongs to another player, and the log says so; then your tiger is "
	                "leashed. Right clicking the lion yourself is refused the same way.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.only_leashable",
	    .name = "Only the leashable one of two",
	    .facet = Facet::Leash,
	    .description = "You have two tigers: the left one is the one you can lead, the right one isn't. The hand taps "
	                   "the right one, then the left one.",
	    .expected = "The right tiger is refused as it isn't the one you can lead; the left one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.switch",
	    .name = "Switch the leashable creature",
	    .facet = Facet::Leash,
	    .description = "Your left tiger is on the leash. The right tiger is made the one you can lead, then the hand "
	                   "taps the left one and the right one.",
	    .expected = "Made leashable, the right tiger takes over: the left one's leash comes off and tapping it is "
	                "refused, and the right one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::MakeLeashable, .creature = 1, .delaySeconds = 4.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.keys",
	    .name = "The leash shortcuts",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes. The keys are pressed in turn: L puts the learning leash "
	                   "on, B steps down to the aggression leash, V steps back up to the learning leash and on up to "
	                   "the compassion leash, and L takes it off, however many leashes it knows.",
	    .expected = "The rope goes on, changes colour three times, and comes off; the readout names each leash. "
	                "Pressing L, V and B yourself does the same.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::LeashKey, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_next_turn",
	    .name = "The leash key acts at the start of the next turn",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The scenario presses the leash key through the game's "
	                   "controls, as a player does, and three seconds later presses it again. The key makes a command "
	                   "of the player, and the world does the command at the start of the next game turn.",
	    .expected = "The rope goes on one or two game turns after the press (a tenth to a fifth of a second), not on the "
	                "frame of the press, and comes off again after the second press. The players.commands query shows "
	                "the command that waits.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)},
	                 {.kind = Kind::PressKey,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)}},
	});

	all.push_back({
	    .id = "leash.click_or_hold",
	    .name = "Hold to stroke, right click to leash",
	    .facet = Facet::Leash,
	    .description = "The hand holds on to your tiger with the right button, stroking its head and belly, and lets go; "
	                   "then it clicks the tiger, which puts the leash on. Try it yourself: holding the right button on "
	                   "it for a second strokes it, sweeping the held hand across it slaps it, and a quick right click "
	                   "leashes it.",
	    .expected = "The hold strokes it without leashing it; the click then puts the rope on without stroking it.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 2.5f),
	                 Act(Kind::HandLetGo, 0, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.shake",
	    .name = "Shake the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, and a scribble is then drawn with the empty hand, as "
	                   "the game shakes a leash off. Draw it yourself (move the mouse quickly side to side a few times, "
	                   "no button held) to try it.",
	    .expected = "The rope goes on, and comes off once the scribble is recognised; the log says so.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashShake, .creature = 0, .delaySeconds = 4.0f}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_off",
	    .name = "L takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, then L is pressed.",
	    .expected = "The rope goes on, then comes off with L.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashKey, .delaySeconds = 4.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.other_gods_creatures",
	    .name = "Other gods' creatures can't be slapped",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left, a tortoise of player three (as Khazar's is) in the middle and a wolf of "
	                   "player four (as Lethys's is) on the right. The hand tries to slap the tortoise and the wolf, "
	                   "then to leash the tortoise.",
	    .expected = "Neither is slapped: the hand never takes hold of another god's creature, inside the player's "
	                "influence or out, and the log says so. Leashing the tortoise is refused too, as it isn't yours.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-40.0f, 60.0f}),
	                  [] {
		                  auto tortoise = Content(CreatureType::Tortoise, {0.0f, 60.0f});
		                  tortoise.owner = PlayerNames::PLAYER_THREE;
		                  return tortoise;
	                  }(),
	                  [] {
		                  auto wolf = Content(CreatureType::Wolf, {40.0f, 60.0f});
		                  wolf.owner = PlayerNames::PLAYER_FOUR;
		                  return wolf;
	                  }()},
	    .commands = {Slap(1, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 1, 2.0f, true),
	                 Slap(2, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 2, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.home",
	    .name = "Kept at home",
	    .facet = Facet::Leash,
	    .description = "A tiger is kept within 40 units of where it stands, as a young creature is kept near its home, "
	                   "and told to walk 120 units away.",
	    .expected = "Once it strays past the radius it turns back and walks home again.",
	    .framing = {.shot = Shot::Overview, .include = {{120.0f, 0.0f}, {-40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 Go(Kind::WalkTo, 0, {120.0f, 0.0f}, 1.0f),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 6.0f)},
	    .repeatFrom = 1,
	});
}

Command Fight(Kind kind, size_t creature, size_t value, float delay, float chargeMs = 0.0f)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .value = value, .chargeMs = chargeMs};
}

/// A content tiger for a fight, its owner, strength and alignment given
CreatureSetup Fighter(glm::vec2 offset, float facing, std::string_view label, PlayerNames owner, float strength,
                      float alignment)
{
	auto creature = Content(CreatureType::Tiger, offset, facing, label);
	creature.owner = owner;
	creature.strength = strength;
	creature.alignment = alignment;
	return creature;
}

void AddCombat(std::vector<Scenario>& all)
{
	constexpr size_t k_High = 0;
	constexpr size_t k_Mid = 1;
	constexpr size_t k_Low = 2;
	constexpr size_t k_Forward = 0;
	constexpr size_t k_Back = 1;
	const glm::vec2 left {-25.0f, 40.0f};
	const glm::vec2 right {25.0f, 40.0f};
	const auto red = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(left, 90.0f, "red", owner, strength, alignment);
	};
	const auto blue = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(right, 270.0f, "blue", owner, strength, alignment);
	};
	const Framing ringside {.shot = Shot::Overview, .include = {{-45.0f, 40.0f}, {45.0f, 40.0f}}, .distance = 0.8f};

	all.push_back({
	    .id = "combat.ai_duel",
	    .name = "Two tigers fight by themselves",
	    .facet = Facet::Combat,
	    .description = "Two somewhat evil tigers, so aggressive fighters, the red one stronger, are told to fight, and both "
	                   "fight by themselves.",
	    .expected = "They walk to their places either side of the arena, face each other and maybe taunt, then duel: "
	                "blows high, in the middle and low with their hit wobbles and reeling, blocks and steps. The panel at "
	                "the top left shows both fight healths falling. The loser faints, and the winner finishes and shows "
	                "off.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.strength_gain_at_stop",
	    .name = "A knock-out makes both tigers stronger",
	    .facet = Facet::Combat,
	    .description = "Two tigers, the red one of strength 0.6 and the blue one of strength 0.4, fight by themselves.",
	    .expected = "At the knock-out the strength of each tiger increases two times by the strength gain of the fight "
	                "action, 0.012 each time. The fight transcript has a record strength_gain with two gains for each "
	                "tiger.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.4f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	{
		// Each tiger holds a rock as the fight starts. It lets go of the rock before its taunt.
		auto start = Fight(Kind::StartFight, 0, 1, 1.0f);
		start.waitUntilFree = true;
		// The minds wait, so that each tiger keeps its rock until the fight
		auto holder = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto other = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		holder.pauseMind = true;
		other.pauseMind = true;
		all.push_back({
		    .id = "combat.let_go_before_taunt",
		    .name = "Two tigers let go of their rocks before they taunt",
		    .facet = Facet::Combat,
		    .description = "The blue tiger picks up a rock, and a fifth of a second later the red tiger picks up a rock. "
		                   "A second after the red tiger holds its rock, it asks the blue tiger to fight. Both fight by "
		                   "themselves.",
		    .expected = "Each tiger keeps its rock while it turns to face the other tiger. Then each tiger throws its rock "
		                "away or puts it down with its let-go clip, and after that it taunts. No rock falls from a hand at "
		                "the start of the fight.",
		    .framing = ringside,
		    .creatures = {holder, other},
		    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {-21.0f, 34.0f}},
		                {.type = MobileStaticInfo::RockChalk, .offset = {21.0f, 34.0f}}},
		    .commands = {OnObject(Kind::PickUp, 1, 1, 1.0f), OnObject(Kind::PickUp, 0, 0, 0.2f), start,
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The other tiger plays an action as the fight starts. The action plays on, and its turn to face waits for it.
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.face_off_waits_for_body",
		    .name = "A tiger finishes its action before it turns to fight",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts its impress action. Six seconds later, near the end of the action, the "
		                   "red tiger asks the blue tiger to fight. Both fight by themselves.",
		    .expected = "The red tiger turns to face the blue tiger. The blue tiger plays its action to the end, and only "
		                "then turns to face the red tiger. Then both taunt.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Play(Kind::PlayAction, 1, animations::k_Impress, 1.0f, false), Fight(Kind::StartFight, 0, 1, 6.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The other tiger walks as the fight starts. It continues until the first tiger tells it to obey, two turns later.
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.asked_walks_on",
		    .name = "A tiger walks on for a moment when it is asked to fight",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts to walk away from the red tiger. One second later, the red tiger asks the "
		                   "blue tiger to fight. Both fight by themselves.",
		    .expected = "The red tiger stops at once and turns to face the blue tiger. The blue tiger walks on for two "
		                "turns, then slows to a stand and turns to face the red tiger. Then both taunt. In the arena, "
		                "each tiger turns on the spot to face the other. The red tiger does not start its duel before the "
		                "blue tiger.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Go(Kind::WalkTo, 1, {25.0f, 0.0f}, 1.0f, false), Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The first tiger stops the fight on the turn after the request, before it tells the other tiger to obey
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.asked_stops_on_early_end",
		    .name = "An early end of a fight stops the tiger that was asked",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts to walk away from the red tiger. One second later, the red tiger asks the "
		                   "blue tiger to fight. One turn after that, the fight of the red tiger stops.",
		    .expected = "The red tiger stops at once. On the turn that the fight stops, the blue tiger also stops its walk "
		                "and slows to a stand. No arena appears.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Go(Kind::WalkTo, 1, {25.0f, 0.0f}, 1.0f, false), Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAbort, 0, 1, 0.1f)},
		});
	}

	{
		// The needs of the blue tiger are not set again each frame, so the script can take its life
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		asked.hold = false;
		all.push_back({
		    .id = "combat.asked_stops_after_duel",
		    .name = "The end of the first tiger stops the asked tiger after the duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. The creatures do not "
		                   "faint. When the duel is on, a script sets the life of the blue tiger to one half. Two seconds "
		                   "later the fight of the red tiger stops.",
		    .expected = "The duel stops, and both tigers finish their moves and play their responses. The red tiger has "
		                "more life, and its response ends first. Two turns later its fight ends. The blue tiger then "
		                "stops at once with no fear, and it does not obey the red tiger again.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), asked},
		    .commands = {Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 {.kind = Kind::CallNative,
		                  .delaySeconds = 12.0f,
		                  .script = "SET_PROPERTY",
		                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
		                                       .number = static_cast<float>(script::ObjectPropertyType::Health)},
		                                      {.kind = ScriptArgument::Kind::Creature, .index = 1},
		                                      {.kind = ScriptArgument::Kind::Number, .number = 0.5f}}},
		                 Fight(Kind::FightAbort, 0, 0, 2.0f)},
		});
	}

	all.push_back({
	    .id = "saves.arena_ring_back",
	    .name = "Save and load: the ring of the arena shows again",
	    .facet = Facet::Combat,
	    .description = "Two tigers are told to fight, and both fight by themselves. 5 seconds later the game saves into "
	                   "slot 0. 3 seconds after the save the game loads slot 0.",
	    .expected = "Before the save the ring of light stands round the arena. After the load the ring stands round the "
	                "arena again while the fight continues, as before the save.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::SaveGame, .delaySeconds = 5.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	{
		// A lion against a tiger, so that two species show their heads
		auto lion = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		lion.species = CreatureType::Lion;
		all.push_back({
		    .id = "combat.head_look",
		    .name = "Heads in a fight",
		    .facet = Facet::Combat,
		    .description = "The player's red lion and a blue tiger start to look at the camera. A second later they fight "
		                   "by themselves, so each head has a point to look at from before the fight. The camera looks at "
		                   "the arena from the side.",
		    .expected = "Before the duel each creature looks at the other as it walks to its place. In the fight stance "
		                "each head follows the head of the other creature. During a blow, a block, a cast or a fall the "
		                "head turns back to straight ahead. No head looks off to the left or the right of the opponent.",
		    .framing = ringside,
		    .creatures = {lion, blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 0.5f, .object = 0, .action = "LookAtCamera"},
		                 {.kind = Kind::DoAction, .delaySeconds = 0.0f, .object = 1, .action = "LookAtCamera"},
		                 Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The two tigers start back to back, so that each one must turn to face the other
		auto away = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		away.facingDegrees = 90.0f;
		auto otherAway = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		otherAway.facingDegrees = 90.0f;
		all.push_back({
		    .id = "combat.lead_in",
		    .name = "The steps before a duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger and the blue tiger stand back to back. The red tiger starts a fight with the blue "
		                   "tiger, and both fight by themselves.",
		    .expected = "Both tigers stop and turn to face each other. Each one taunts or makes the rude gesture. Then "
		                "the ring of the arena appears with its sound, and the red tiger points at the arena. Both walk to "
		                "their places and look at each other with angry faces. They turn to face each other again, and "
		                "the duel starts.",
		    .framing = ringside,
		    .creatures = {away, otherAway},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	all.push_back({
	    .id = "combat.fight_view",
	    .name = "Watching a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger are told to fight, both by themselves, while the camera "
	                   "looks at the arena from within it. Play it with the hand: the Move button (left) on the blue "
	                   "tiger strikes at once, the Action button (right) queues the move, on the red tiger it blocks and "
	                   "on the ground it steps; hold either to charge a blow, and draw a star for the special move.",
	    .expected = "After a second of looking, the camera flies to the side of the arena and then follows the duel "
	                "side on, easing round as the tigers move, and the fight panel shows at the top left. Zooming far "
	                "out leaves the fight; once it is over the view stays three seconds and gives the camera back.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The fight that the recorded input of the hand directs for the dedicated server's check (tools/sim/hand_inputs).
	// The creature help is on: in a network game every computer counts the time that the reported hand stays over the
	// creature.
	all.push_back({
	    .id = "combat.recorded_presses",
	    .name = "A fight for the recorded presses of the hand",
	    .facet = Facet::Combat,
	    .description = "As Watching a fight: the player's red tiger and a blue tiger fight by "
	                   "themselves, and the hand's presses direct the red one. The dedicated server and the game both "
	                   "play this scenario, and the game makes the recorded input of tools/sim/hand_inputs/fight.json.",
	    .expected = "The camera flies to the side of the arena. Each press on the blue tiger, on the red tiger or on the "
	                "ground puts its sparkle up at once, and the red tiger makes the move at the next turn.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.gesture_view_moves",
	    .name = "Drawing a gesture while the fight view moves",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves, while the camera looks at the arena "
	                   "from within it. When the fight view follows the duel and moves each frame, the hand draws the "
	                   "star of the special move across the middle of the screen.",
	    .expected = "The camera flies to the side of the arena and follows the duel. The star drawn while the view "
	                "moves is recognised: the red tiger does its special move. Another camera that moves would wipe "
	                "the path, but the fight view lets the hand draw on.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::DrawGesture, .delaySeconds = 8.0f, .value = static_cast<size_t>(GestureType::Star)}},
	});

	{
		// The player's tiger knows the five miracles that a creature casts in a fight
		const auto knows = [](MagicType magic) {
			return Command {
			    .kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.1f, .value = static_cast<size_t>(magic)};
		};
		all.push_back({
		    .id = "interface.hint_fight_seeds",
		    .name = "The seeds over the hints of the fight miracles",
		    .facet = Facet::Interface,
		    .description = "The player's red tiger knows the fireball, the lightning bolt, the heal, and the weak and the "
		                   "strong creature spells. It fights a blue tiger, and the two fight by themselves.",
		    .expected = "When the duel starts, the hints along the bottom right of the screen show the star of the "
		                "special move and the gesture of each of the five miracles. Over each gesture of a miracle, the "
		                "seed of the miracle turns slowly, with the effect of its holder over it. The seeds are small, in "
		                "white light, and they do not go away while the duel continues.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {knows(MagicType::Fireball), knows(MagicType::LightningBolt), knows(MagicType::Heal),
		                 knows(MagicType::CreatureSpellWeak), knows(MagicType::CreatureSpellStrong),
		                 Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// Two creatures that are not the player's fight by themselves, and the player's own lion stands near the arena
		// The Action button is the right button of the mouse
		constexpr size_t k_RightButton = 3;
		constexpr size_t k_Own = 2;
		auto own = Content(CreatureType::Lion, {12.0f, 15.0f}, 0.0f, "own");
		own.owner = PlayerNames::PLAYER_ONE;
		all.push_back({
		    .id = "combat.view_eases_back",
		    .name = "The fight view eases back after the hand lets go",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue tiger that are not the player's fight by themselves. The player's own "
		                   "lion stands near the arena. A double click into the arena makes the camera show the fight. "
		                   "A second later the hand holds the lion with the Action button for four seconds, and then "
		                   "lets go.",
		    .expected = "The camera flies to the side of the arena and then follows the duel. While the hand holds the "
		                "lion, the camera looks at the lion. When the hand lets go, the camera flies back to where the "
		                "fight view was, and then eases on to the fight slowly: first over 5 seconds, then faster, over "
		                "2 seconds after the first 2 seconds. It does not jump.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_TWO, 0.6f, -0.2f), blue(PlayerNames::NEUTRAL, 0.5f, -0.2f), own},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 7.0f),
		                 {.kind = Kind::PointerOnCreature, .creature = k_Own, .delaySeconds = 1.2f},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.0f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 4.0f, .value = k_RightButton}},
		});
	}

	{
		// The Move button is the left button of the mouse
		constexpr size_t k_LeftButton = 1;
		all.push_back({
		    .id = "combat.view_flight_woosh",
		    .name = "The fight view flies in from far away with a woosh",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger and a blue tiger fight by themselves. The camera looks down on the "
		                   "arena from high above it. The camera shows the fight when the fighters have made their arena.",
		    .expected = "The camera flies down to the side of the arena through a point half way, and a woosh is heard "
		                "once as the flight starts. Then the camera follows the duel.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 320.0f, 30.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f), Fight(Kind::FightView, 0, 0, 1.0f)},
		});
		all.push_back({
		    .id = "combat.view_flight_stopped",
		    .name = "A turn of the wheel stops the flight of the fight view",
		    .facet = Facet::Combat,
		    .description = "The same fight, seen from high above. The camera shows the fight when the fighters have made "
		                   "their arena, and a third of a second later the wheel turns one notch.",
		    .expected = "The flight stops at once: the camera does not go on through the point half way. It goes "
		                "straight to the fight on its slow curves and then follows the duel.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 320.0f, 30.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::WheelTurn, .delaySeconds = 5.75f, .value = 1}},
		});
		all.push_back({
		    .id = "combat.view_drag_away",
		    .name = "Dragging the land in the fight view, and away from the fight",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue tiger that are not the player's fight by themselves. A double click "
		                   "into the arena makes the camera show the fight. Seven seconds later the hand grips the land "
		                   "with the Move button and drags it a little, and lets go. Four seconds later it grips the land "
		                   "again and drags it far.",
		    .expected = "While the hand grips the land, the camera stops following the fight and moves with the land. "
		                "When the hand lets go, the camera eases back to the fight with no jump. In the long drag, the "
		                "fight view ends when the camera is far from the arena, and the land moves on with the hand.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_TWO, 0.6f, -0.2f), blue(PlayerNames::NEUTRAL, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::PointerTo, .delaySeconds = 7.0f, .point = {0.5f, 0.75f}},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_LeftButton},
		                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.2f}, .amount = 0.5f},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = k_LeftButton},
		                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.5f, 0.8f}},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_LeftButton},
		                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.7f}, .amount = 2.0f},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = k_LeftButton}},
		});
		// The temple key is space
		constexpr auto k_TempleKey = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_TEMPLE);
		all.push_back({
		    .id = "combat.view_temple_key",
		    .name = "A double tap of the temple key while the camera shows a fight",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger and a blue tiger fight by themselves. The player's temple stands 150 "
		                   "metres south of the arena. The camera shows the fight when the fighters have made their arena. "
		                   "Eight seconds later the scenario presses the temple key twice, 0.2 seconds apart.",
		    .expected = "The camera starts to fly towards the temple and rises high on the way. After about three "
		                "quarters of a second it turns back, and the view of the fight eases the camera back to the "
		                "duel. A single tap alone does nothing while the camera shows the fight.",
		    .environment = {.dispenserGrid = false, .temple = glm::vec2 {0.0f, -150.0f}},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::PressKey, .delaySeconds = 8.0f, .value = k_TempleKey},
		                 {.kind = Kind::PressKey, .delaySeconds = 0.2f, .value = k_TempleKey}},
		});
	}

	{
		// The fight lesson of the guide: no one chooses the moves of either creature, and the guide belongs to no player
		// The Action button is the right button of the mouse
		constexpr size_t k_RightButton = 3;
		// The double click comes after the arena is made, nearly six seconds into the fight, so it finds the arena. The
		// presses come after the duel starts, nearly fourteen seconds into the fight.
		constexpr float k_ViewAfterArena = 6.5f;
		constexpr float k_PressAfterDuel = 8.0f;
		auto guide = Fighter(right, 270.0f, "guide", PlayerNames::NEUTRAL, 0.5f, 0.0f);
		guide.species = CreatureType::Lion;
		all.push_back({
		    .id = "combat.lesson_press",
		    .name = "The guide's fight lesson: a press on the guide",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. No one chooses the moves of either creature. A double click into the arena makes "
		                   "the camera show the fight. Then every four seconds the hand presses the Action button on the "
		                   "middle of the lion's body.",
		    .expected = "The camera shows the fight side on at once. Each press puts a sparkle on the lion's body, and the "
		                "red tiger strikes at the middle. The lion reels from each blow that lands. Neither creature makes "
		                "a move by itself.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f),
		                 Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
		                 Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel),
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_RightButton},
		                 Fight(Kind::PointerOnOpponent, 0, 0, 3.8f)},
		    .repeatFrom = 5,
		});
		// While a press on the opponent's body is held, the hand goes to the sparkle that the press put there
		all.push_back({
		    .id = "hand.fight_sparkle",
		    .name = "The hand on the sparkle of a held press in a fight",
		    .facet = Facet::Hand,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. A double click into the arena makes the camera show the fight. Then every five "
		                   "seconds the hand presses the Action button on the middle of the lion's body and holds it for "
		                   "two seconds.",
		    .expected = "While the button is down, the hand sits on the sparkle on the lion's body and moves with the "
		                "body. When the button comes up, or a blow takes the sparkle away, the hand goes back under the "
		                "pointer.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f),
		                 Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
		                 Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel),
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = k_RightButton},
		                 Fight(Kind::PointerOnOpponent, 0, 0, 2.9f)},
		    .repeatFrom = 5,
		});
		{
			// Eighteen quick presses fill the queue of twelve moves, also when the fight plays some of the moves; the fight
			// refuses the presses after that
			constexpr size_t k_QuickPresses = 18;
			std::vector<Command> commands {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 0, 0.2f),
			                               Fight(Kind::FightAuto, 1, 0, 0.2f), Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
			                               Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel)};
			for (size_t press = 0; press < k_QuickPresses; ++press)
			{
				commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = k_RightButton});
				commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.05f, .value = k_RightButton});
			}
			all.push_back({
			    .id = "combat.refused_press_does_nothing",
			    .name = "A press that the fight refuses does nothing",
			    .facet = Facet::Combat,
			    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight "
			                   "lesson starts it. No one chooses the moves of either creature. A double click into the "
			                   "arena makes the camera show the fight. Then the hand presses the Action button on the lion "
			                   "eighteen times quickly.",
			    .expected = "The presses put sparkles on the lion until twelve show. The fight refuses the presses after "
			                "that (the fight transcript shows refused_full), and nothing else takes them: the log shows no "
			                "line that the hand can't hold the lion, and the leash does not answer.",
			    .framing = ringside,
			    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
			    .commands = std::move(commands),
			});
		}
		// The fight autopilot plays the stages of the lesson with the real mouse, then checks them in the transcript
		constexpr size_t k_LessonPolicy = 0;
		constexpr float k_LessonCheckAfterSeconds = 45.0f;
		all.push_back({
		    .id = "combat.autopilot_lesson",
		    .name = "The fight autopilot plays the guide's fight lesson",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. No one chooses the moves of either creature. The fight autopilot fights for the "
		                   "player with the real mouse. It double clicks into the arena, then does each stage of the "
		                   "lesson: one blow, three quick blows, a block and a step. Then it hits until the lion falls. "
		                   "After 45 seconds the fight transcript is checked for the four stages in their order. Start the "
		                   "game with the inspector (--inspect-port 0).",
		    .expected = "The camera shows the fight after the double click. The red tiger strikes, the lion reels, three "
		                "sparkles queue three blows, the tiger blocks, then steps. The log line of the fight lesson check "
		                "starts with \"passed\". The tiger wins.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f), Fight(Kind::FightAutopilot, 0, k_LessonPolicy, 0.5f),
		                 Fight(Kind::FightLessonCheck, 0, 0, k_LessonCheckAfterSeconds)},
		});
	}

	all.push_back({
	    .id = "combat.player",
	    .name = "Directing the player's creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger fights a blue tiger that fights by itself. The red one is given blows "
	                   "high, in the middle and low, a step forward, a block and a fully charged high blow in turn, as "
	                   "clicking the blue tiger's head, middle and legs, the ground ahead, and the red tiger itself would. "
	                   "Click them yourself to direct it too.",
	    .expected = "Each order plays at once: the blow chosen lands at the height asked for, stepping in first when "
	                "out of reach; the block holds until the next order. Left alone for 15 seconds, it fights by "
	                "itself.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_High, 8.0f, 400.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 2.5f, 400.0f), Fight(Kind::FightBlow, 0, k_Low, 2.5f, 400.0f),
	                 Fight(Kind::FightStep, 0, k_Forward, 2.5f), Fight(Kind::FightBlock, 0, 0, 2.0f),
	                 Fight(Kind::FightBlow, 0, k_High, 3.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "combat.blocking",
	    .name = "Blocking an aggressive opponent",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger blocks while an evil blue tiger, as aggressive as fighters get, attacks "
	                   "it; every twelve seconds the red one steps back out of its block and blocks again.",
	    .expected = "The red tiger raises its guard and holds it. Blows on it make it reel back in its block and take "
	                "a tenth of the damage, leaving no wounds; the blue tiger attacks again and again.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.8f, -0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlock, 0, 0, 7.0f),
	                 Fight(Kind::FightStep, 0, k_Back, 12.0f), Fight(Kind::FightBlock, 0, 0, 1.5f)},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "combat.charged",
	    .name = "Charged and quick blows",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger strikes a good, defensive blue tiger with blows in the middle, in turn "
	                   "let go at once and held the full 1.2 seconds.",
	    .expected = "A quick click's blow plays at half speed and does little; a fully charged blow plays half as fast "
	                "again as normal and takes three times as much off the blue tiger's fight health.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 8.0f, 0.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 4.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	auto worn = Fighter({0.0f, 0.0f}, 0.0f, "worn", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	worn.hold = false;
	worn.pauseMind = true;
	worn.needs.life = 0.6f;
	worn.needs.exhaustion = 0.7f;
	// The winner of a knock-out is made to do something to the loser, by its mind
	{
		constexpr float k_DuelStartsSeconds = 12.0f;
		constexpr float k_EvilAlignment = -0.8f;
		constexpr float k_GoodAlignment = 0.8f;
		all.push_back({
		    .id = "combat.winner_poos",
		    .name = "An evil winner has a poo on the loser",
		    .facet = Facet::Combat,
		    .description = "An evil red tiger (alignment -0.8) fights a blue tiger. When the duel has started, the blue tiger "
		                   "is knocked out as a fight's loser is.",
		    .expected = "The red tiger finishes its move and leaves the fight. Then it walks to the blue tiger and has a poo "
		                "on it. The blue tiger lies out cold.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_EvilAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds)},
		});
		auto passer = Fighter({60.0f, -40.0f}, 270.0f, "passer", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
		all.push_back({
		    .id = "combat.after_fight_block",
		    .name = "After a knock-out, the winner takes no notice until its poo is done",
		    .facet = Facet::Combat,
		    .description = "An evil red tiger (alignment -0.8) fights a blue tiger, and the blue tiger is knocked out as "
		                   "in combat.winner_poos. A third tiger then walks past the red tiger.",
		    .expected = "The red tiger walks to the blue tiger and has a poo on it. It does not stop to look over the "
		                "tiger that walks past. Once the poo is done, it takes up reactions again.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_EvilAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f), passer},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds),
		                 Go(Kind::WalkTo, 2, {-60.0f, -40.0f}, 1.0f, false)},
		});
		all.push_back({
		    .id = "combat.winner_shows_off",
		    .name = "A winner that is not evil shows off at the loser",
		    .facet = Facet::Combat,
		    .description = "A good red tiger (alignment 0.8) fights a blue tiger. When the duel has started, the blue tiger is "
		                   "knocked out as a fight's loser is.",
		    .expected = "The red tiger finishes its move and leaves the fight. Then it shows off at the blue tiger, which "
		                "lies out cold.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_GoodAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds)},
		});
	}

	all.push_back({
	    .id = "combat.faint_recovery",
	    .name = "Knocked out, taken home and back up",
	    .facet = Facet::Combat,
	    .description = "A worn tiger, life 0.6 and exhaustion 0.7, is given a home where it stands, walks 90 units away "
	                   "and is knocked out there as a fight's loser is.",
	    .expected = "It faints and lies out cold with its eyes closed for 12 seconds, is taken home, lies there a few "
	                "seconds more, then rests until it is no more exhausted than 0.3, and gets up.",
	    .framing = {.shot = Shot::Overview, .include = {{90.0f, 30.0f}, {-20.0f, 0.0f}}, .distance = 0.6f},
	    .creatures = {worn},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
	                 Go(Kind::WalkTo, 0, {90.0f, 30.0f}, 0.5f, false),
	                 Act(Kind::KnockOut, 0, 1.0f, true)},
	});
	all.push_back({
	    .id = "combat.script_moves_knocked_out",
	    .name = "A script moves a creature that lies knocked out",
	    .facet = Facet::Combat,
	    .description = "A worn tiger, life 0.6 and exhaustion 0.7, is given a home where it stands and is knocked out "
	                   "there as a fight's loser is. Six seconds later, while it lies still, a script puts it 40 units "
	                   "to the east with SET_POSITION.",
	    .expected = "It shows at the new place at once, and its body stands there, as the game's move stands it. It "
	                "stays knocked out for the same time as in combat.faint_recovery, counted from when it first lay "
	                "still. Then it is taken home, waits there, rests and gets up. It does not stay knocked out for ever.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, 0.0f}, {-20.0f, 0.0f}}, .distance = 0.6f},
	    .creatures = {worn},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
	                 Act(Kind::KnockOut, 0, 1.0f, false),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 6.0f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Creature, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {40.0f, 0.0f}}}}},
	});

	{
		constexpr float k_HurtLife = 0.3f;
		auto hurt = Fighter({0.0f, 0.0f}, 0.0f, "hurt", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
		hurt.hold = false;
		hurt.needs.life = k_HurtLife;
		all.push_back({
		    .id = "combat.knocked_out_no_emergency",
		    .name = "Knocked out, a hurt creature has no emergency until it gets up",
		    .facet = Facet::Combat,
		    .description = "A hurt tiger, life 0.3, is knocked out at once, as a fight's loser is. A life below 0.4 is an "
		                   "emergency for a creature that is awake.",
		    .expected = "It lies out cold, is taken home and rests. The emergency that its low life started stops when it "
		                "faints. While it lies there, the mind starts no new emergency (inspector: emergencyRunning of "
		                "CreatureMindState stays false). It can have an emergency again only after it gets up.",
		    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}}, .distance = 0.4f},
		    .creatures = {hurt},
		    .commands = {Act(Kind::KnockOut, 0, 0.0f, true)},
		});
	}

	all.push_back({
	    .id = "combat.leash",
	    .name = "Leashed onto another creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger is put on the aggression leash, which is tied to the blue tiger.",
	    .expected = "Tied to another creature, it fights it: both walk to the arena and duel, the red one fighting by "
	                "itself once left alone for three seconds.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Evil, 1.0f), Fight(Kind::TieLeashToCreature, 0, 1, 2.0f)},
	});

	{
		// A ring of villagers of the player's town round the place of the arena, which see the knock-out. The fight
		// autopilot fights for the player with the aggressive policy.
		constexpr size_t k_AggressivePolicy = 1;
		std::vector<ObjectSetup> crowd;
		constexpr int k_Villagers = 8;
		constexpr float k_RingRadius = 44.0f;
		for (int i = 0; i < k_Villagers; ++i)
		{
			const auto angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Villagers);
			crowd.push_back({.type = i % 2 == 0 ? VillagerInfo::CelticFarmerMale : VillagerInfo::CelticHousewifeFemale,
			                 .offset = glm::vec2 {20.0f, 35.0f} +
			                           (k_RingRadius * glm::vec2 {portable_math::Cos(angle), portable_math::Sin(angle)}),
			                 .joinTown = true});
		}
		auto strong = red(PlayerNames::PLAYER_ONE, 0.9f, -0.6f);
		strong.facingDegrees = 90.0f;
		// The weak tiger has little life, thus little fight health, and soon faints
		auto weak = blue(PlayerNames::PLAYER_TWO, 0.2f, 0.0f);
		weak.needs.life = 0.15f;
		all.push_back({
		    .id = "combat.leash_lead_in",
		    .name = "A fight on the leash, from the face-off to the faint",
		    .facet = Facet::Combat,
		    .description = "The player's strong red tiger stands with its back to a weak blue tiger. The aggression leash "
		                   "on the red tiger is tied to the blue tiger. Eight villagers stand round the place of the arena.",
		    .expected = "Both tigers stop and turn to face each other, and each one taunts or makes the rude gesture. The "
		                "arena appears with its sound, the red tiger points at it, and both walk to their places. They "
		                "turn to face each other again and duel. A fighter near the edge steps back to the middle. The "
		                "blue tiger faints, and the villagers near it react as to a death.",
		    .framing = {.shot = Shot::Placed, .eye = {20.0f, 45.0f, -25.0f}, .look = {20.0f, 0.0f, 35.0f}},
		    .creatures = {strong, weak},
		    .objects = crowd,
		    .commands = {Leash(0, LeashType::Evil, 1.0f), Fight(Kind::TieLeashToCreature, 0, 1, 2.0f),
		                 Fight(Kind::FightAutopilot, 0, k_AggressivePolicy, 0.5f)},
		});
	}

	{
		// Two tigers with the same life start a fight. The debug tools stop the duel, so that no one is knocked out.
		auto even = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		auto other = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		other.needs.life = 0.5f;
		even.needs.life = 0.5f;
		auto stronger = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		stronger.needs.life = 0.9f;
		constexpr float k_StopAfterSeconds = 25.0f;
		const std::vector<Command> stopped {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                                    Fight(Kind::FightAbort, 0, 0, k_StopAfterSeconds)};
		all.push_back({
		    .id = "combat.stopped_tie",
		    .name = "A fight stopped with no knock-out, the two tigers with the same life",
		    .facet = Facet::Combat,
		    .description = "Two tigers fight by themselves. The scenario keeps the life of both at 0.5, thus they have the "
		                   "same life after the duel. After 25 seconds the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. Both tigers are sad. The first tiger to finish fears the other "
		                "and runs from it, and the other tiger stops its response and is free again.",
		    .framing = ringside,
		    .creatures = {even, other},
		    .commands = stopped,
		});
		auto weaker = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		weaker.needs.life = 0.3f;
		auto healthier = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		healthier.needs.life = 0.9f;
		all.push_back({
		    .id = "combat.stopped_starter_weaker",
		    .name = "A fight stopped with no knock-out, the starter with less life",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts the fight but has less life than the blue tiger. Both fight by themselves. "
		                   "After 25 seconds the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. The red tiger is sad and the blue tiger is happy. The red tiger then "
		                "fears the blue tiger and runs from it. The blue tiger is free again: it does not stay stopped.",
		    .framing = ringside,
		    .creatures = {weaker, healthier},
		    .commands = stopped,
		});
		all.push_back({
		    .id = "combat.stopped_happy_sad",
		    .name = "A fight stopped with no knock-out, one tiger with more life",
		    .facet = Facet::Combat,
		    .description = "The red tiger has much more life than the blue tiger. Both fight by themselves. After 25 seconds "
		                   "the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. The red tiger is happy. The blue tiger is sad and then runs from the "
		                "red tiger in fear.",
		    .framing = ringside,
		    .creatures = {stronger, other},
		    .commands = stopped,
		});
	}

	auto angry = red(PlayerNames::PLAYER_ONE, 0.6f, -0.3f);
	angry.desires = OnlyDesire(Desire::Anger);
	all.push_back({
	    .id = "combat.anger",
	    .name = "An angry creature picks a fight",
	    .facet = Facet::Combat,
	    .description = "A red tiger as angry as it gets, wanting nothing else, stands near a blue tiger.",
	    .expected = "Its anger makes a plan to fight the blue tiger. It asks the blue tiger to fight, and they duel. The "
	                "blue tiger obeys the red tiger until the fight is over.",
	    .framing = ringside,
	    .creatures = {angry, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	});

	auto unpractised = red(PlayerNames::PLAYER_THREE, 0.6f, -0.3f);
	unpractised.desires = OnlyDesire(Desire::Anger);
	all.push_back({
	    .id = "combat.lean_until_completed",
	    .name = "A creature that did not complete a fight leans against its alignment again",
	    .facet = Facet::Combat,
	    .description = "A slightly evil red tiger (alignment -0.30) as angry as it gets stands near a blue tiger. Both "
	                   "fight by themselves. The scenario gives the red tiger an aggressive lean of +0.70, but the red "
	                   "tiger did not complete a fight.",
	    .expected = "Its anger makes a plan to fight the blue tiger. When the fight starts, the lean of the red tiger "
	                "changes to +0.30, against its alignment (the inspector shows CreatureFightRecord tendency). That "
	                "lean is below the aggressive tier, thus the red tiger fights as a defensive fighter and blocks and "
	                "steps more than it strikes. The blue tiger has not started a fight, thus it chooses its moves with a "
	                "lean of 0.",
	    .framing = ringside,
	    .creatures = {unpractised, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Command {.kind = Kind::SetFightLean, .creature = 0, .delaySeconds = 0.0f, .amount = 0.7f}},
	});

	auto lion = Fighter(left, 90.0f, "evil lion", PlayerNames::PLAYER_TWO, 1.0f, -0.9f);
	lion.species = CreatureType::Lion;
	lion.size = 1.3f;
	auto small = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_ONE, 0.1f, 0.8f);
	small.size = 0.8f;
	small.needs.life = 0.3f;
	// Left to heal as it rests after being knocked out
	small.hold = false;
	all.push_back({
	    .id = "combat.evil_winner",
	    .name = "An evil winner has a poo on the loser",
	    .facet = Facet::Combat,
	    .description = "A big, strong, evil lion fights a small, weak, good tiger with little life left, so starting "
	                   "with little fight health, both by themselves.",
	    .expected = "The tiger soon faints. The lion finishes, walks up to it and has a poo on it, rather than showing "
	                "off as a good winner does.",
	    .framing = ringside,
	    .creatures = {lion, small},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 1, 1, 0.2f)},
	});

	all.push_back({
	    .id = "leash.pulled_out_of_fight",
	    .name = "The leash pulls a creature out of its fight",
	    .facet = Facet::Leash,
	    .description = "The player's red tiger wears the learning leash held in the hand and fights a blue tiger, both "
	                   "by themselves. Hold the hand near the red tiger, then move it further away than the full length of the "
	                   "leash.",
	    .expected = "With the hand near, the rope hangs slack and the duel goes on. With the hand further away than the "
	                "full length of the leash, the red tiger is taken out of the fight: the fight is called off with "
	                "no winner and the red tiger walks back to the hand. The help says nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Rope, 0.5f), Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// Big against small: the game never resizes a creature for a fight; it only refuses one outside half to twice its
	// height
	auto big = Fighter(left, 90.0f, "big tiger", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	big.size = 1.5f;
	auto smaller = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	smaller.size = 0.8f;
	all.push_back({
	    .id = "combat.big_vs_small",
	    .name = "A big creature fights a small one",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) fights a small one (size 0.8), just under twice its height, both by "
	                   "themselves.",
	    .expected = "The fight starts and both keep their sizes throughout: the arena is sized for the big one, its "
	                "blows reach further and hit harder, and the small one has to step in to land its own.",
	    .framing = ringside,
	    .creatures = {big, smaller},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightAuto, 1, 1, 0.0f)},
	});
	auto tiny = Fighter(right, 270.0f, "tiny tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	tiny.size = 0.6f;
	all.push_back({
	    .id = "combat.too_small",
	    .name = "A creature too small to fight",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) is told to fight a tiny one (size 0.6), less than half its height.",
	    .expected = "The tiny tiger refuses: no arena appears and neither moves to fight (the command reports "
	                "\"refused\").",
	    .framing = ringside,
	    .creatures = {big, tiny},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f)},
	});

	// Miracles in a fight: the player's hurt tiger heals itself, then strikes its opponent with lightning
	auto hurt = red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	hurt.needs.life = 0.3f;
	all.push_back({
	    .id = "combat.casting",
	    .name = "Casting miracles in a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger, hurt (life 0.3, so fight health 0.65), knows Heal and Lightning. In "
	                   "the fight it is given Heal, as drawing Heal's gesture does, then Lightning, then Heal again; the "
	                   "blue tiger fights by itself.",
	    .expected = "Each miracle is cast from the fight stance: the creature raises its arms (start of the cast), "
	                "casts as the start ends, holds the casting pose only a moment and lowers its arms. Heal is cast on "
	                "itself and its fight health bar rises; Lightning strikes the blue tiger, which reels. Each cast "
	                "takes from the red tiger's stamina (the blue bar); without enough stamina a cast does nothing.",
	    .framing = ringside,
	    .creatures = {hurt, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Heal)},
	         {.kind = Kind::KnowMiracle,
	          .creature = 0,
	          .delaySeconds = 0.0f,
	          .value = static_cast<size_t>(MagicType::LightningBolt)},
	         Fight(Kind::StartFight, 0, 1, 0.5f),
	         Fight(Kind::FightAuto, 1, 1, 0.2f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 8.0f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::LightningBolt), 6.0f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 6.0f)},
	});

	// Forfeiting: there is no forfeit, but taking the creature away ends the fight with no winner
	all.push_back({
	    .id = "combat.forfeit",
	    .name = "Taking a creature out of a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger wears the leash held in the hand and fights a blue tiger, both by "
	                   "themselves. Ten seconds into the duel the player gives a leash order to go to a point, as the "
	                   "Action button outside the fight view does. Try it yourself too: pull the leash taut by moving "
	                   "the hand well away from the red tiger.",
	    .expected = "The fight is called off with no winner: both tigers finish the move they are making, play their "
	                "finish and stand, without celebrating or sulking; the ring of light goes. The red tiger then walks "
	                "to the point. Pulled further than the full length of the leash instead, the red tiger leaves the "
	                "fight and walks back to the hand, and the help says nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Rope, 0.5f), Fight(Kind::StartFight, 0, 1, 0.5f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, -20.0f}, 16.0f, false)},
	});

	// A fight next to a village: its people come to watch
	all.push_back({
	    .id = "combat.villagers_watch",
	    .name = "Villagers watching a fight",
	    .facet = Facet::Combat,
	    .description = "A hut of the player's town south of an arena with six of its people about it. The player's red "
	                   "tiger and a blue tiger fight there, both by themselves.",
	    .expected = "Once the fight is on, the villagers within the arena's reach take up its reaction and walk to places "
	                "spread round a ring 1.4 times the arena's radius, the newest first at the east, the next opposite, "
	                "then the quarters. There they face the arena: for fifteen seconds after a blow lands they cheer "
	                "when the tiger they don't support took it last and despair when theirs did, otherwise they chat. "
	                "They go back to what they were doing when the reaction's time is up or the arena goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -25.0f}, {60.0f, 95.0f}}, .distance = 0.8f},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, -20.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, -12.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, -6.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {2.0f, -8.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, -14.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {6.0f, -10.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, -8.0f}, .joinTown = true}},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The fight music of the player's creature, heard near its arena
	all.push_back({
	    .id = "combat.fight_music",
	    .name = "The fight music",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves. The camera looks at the arena from "
	                   "nearer than 100 metres to its middle.",
	    .expected = "When the arena appears, the land's music fades out and the creature fight music fades in, centred "
	                "on the listener. It plays until the fight is over, then the land's music comes back. Moving the "
	                "camera more than 100 metres from the middle of the arena stops it, unless the hand directs the "
	                "fight in the fight view.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 50.0f, -10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// A double click into the arena from far away: the fight view does not start, the advisors speak instead
	all.push_back({
	    .id = "combat.far_fight_advisors",
	    .name = "The advisors speak of a fight too far to show",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves. The camera is far away from the "
	                   "arena. When the duel is on, a double click goes into the arena.",
	    .expected = "The camera does not show the fight, because it was too far away. An advisor says one of the lines "
	                "about a creature fight, such as \"There's a humdinger of a fight going on.\"",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 120.0f, -900.0f}, .look = {0.0f, 0.0f, -800.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightView, 0, 0, 14.0f)},
	});

	{
		// A horse's blows are of the wound type that bleeds, so its opponent bleeds where they land
		auto horse = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		horse.species = CreatureType::Horse;
		all.push_back({
		    .id = "combat.blow_blood",
		    .name = "Blood and dust where blows land",
		    .facet = Facet::Combat,
		    .description = "A strong red horse and a weak blue tiger fight by themselves. The camera is near the middle "
		                   "of the arena.",
		    .expected = "Faint white puffs follow the striking hand or foot as each blow lands. Where a blow meets the "
		                "body, a wound shows on the skin there, and puffs of sand-coloured dust fly along the blow. When "
		                "the blow is not blocked, red and brown drops of blood fly along the blow, fall to the land and "
		                "slide a little on it, then go. On the tiger, blood from many of the horse's wounds runs down the "
		                "skin for some seconds. The body reels to the side, the top or the bottom by the swing of the "
		                "blow, or straight back when the blow swings ahead.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 16.0f, 18.0f}, .look = {0.0f, 8.0f, 40.0f}},
		    .creatures = {horse, blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The needs of the blue tiger are not set again each frame, so the script can take its life
		auto loser = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		loser.hold = false;
		all.push_back({
		    .id = "combat.opponent_out_of_life",
		    .name = "An opponent with no life left loses the duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger of the player and a blue tiger fight by themselves, and the creatures do not faint. "
		                   "When the duel is on, a script sets the life of the blue tiger to zero. Its fight health stays.",
		    .expected = "On its next turn, the red tiger wins. It finishes its move, stands and plays the happy response. "
		                "An advisor says one of the lines for good news, because the place of the red tiger is on screen. "
		                "The blue tiger does not faint. It stays in its stance and faces the red tiger. When the red tiger "
		                "finishes its response, the blue tiger stops with no response. The fight view ends a few seconds "
		                "after the win. The fight transcript shows a win with by_blow false.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), loser},
		    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 {.kind = Kind::CallNative,
		                  .delaySeconds = 12.0f,
		                  .script = "SET_PROPERTY",
		                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
		                                       .number = static_cast<float>(script::ObjectPropertyType::Health)},
		                                      {.kind = ScriptArgument::Kind::Creature, .index = 1},
		                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}}},
		});
	}

	all.push_back({
	    .id = "combat.held_in_duel",
	    .name = "The hand holds a creature in its duel",
	    .facet = Facet::Combat,
	    .description = "The red tiger of the player and a blue tiger fight by themselves. When the duel is on, the hand "
	                   "holds the red tiger and strokes its head. Two seconds later the hand lets go.",
	    .expected = "When the hand takes hold, the red tiger leaves the duel. It finishes its move and stands. The blue tiger "
	                "stays in its stance and faces the red tiger. When the hand lets go, both tigers stop with no "
	                "response. The fight transcript shows held_by_hand, let_go_by_hand and partner_stopped.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 12.0f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 2.0f)},
	});

	{
		// Blows strike the shapes round the bones of the body: a tiger against a horse, which has a body of a different
		// shape
		auto horse = blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f);
		horse.species = CreatureType::Horse;
		all.push_back({
		    .id = "combat.hit_shapes",
		    .name = "Blows strike the shapes of the body",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue horse fight by themselves. The camera is near the middle of the arena.",
		    .expected = "Each creature steps until a blow reaches into the other's body, then strikes. A blow lands where its "
		                "swing meets the body: on the long head and neck of the horse, on its back or on its legs. A blow "
		                "that swings past the body does not land.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 16.0f, 18.0f}, .look = {0.0f, 8.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), horse},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.bodies_apart",
		    .name = "Fighters keep their bodies apart",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "On the walk to their places, the two tigers go through each other, because the red tiger takes the "
		                "east place. The original game does the same: a creature that walks does not keep clear of another "
		                "creature that walks. In the duel, the bodies do not stay in each other. When a blow reaches into "
		                "the other body, both tigers move apart at once. In the stance and in each step, each tiger turns "
		                "straight to the other, and the two bodies move apart when they touch, also when the other tiger "
		                "blocks or casts after a blow.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The first fight on the first land is the guide's lesson. The guide starts the fight and takes the east place, so
		// the tiger to its east walks past it to the west place.
		auto guide = Fighter(left, 90.0f, "guide", PlayerNames::NEUTRAL, 0.5f, 0.0f);
		guide.species = CreatureType::Lion;
		auto tiger = Fighter(right, 270.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
		all.push_back({
		    .id = "combat.lesson_bodies",
		    .name = "The guide's fight: bodies on the walk and in the duel",
		    .facet = Facet::Combat,
		    .description = "The guide lion starts a fight with the player's red tiger to its east, and both fight by "
		                   "themselves. The camera looks at the arena from the side.",
		    .expected = "On the walk to their places, the lion and the tiger go through each other, because the lion takes "
		                "the east place. The original game does the same. In the duel, a body can reach into the other "
		                "body for a moment during a blow or a step, because the fighters measure their bodies as they were "
		                "at the last game turn. Then both move apart.",
		    .framing = ringside,
		    .creatures = {guide, tiger},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.start_turn",
		    .name = "Fighters turn on the spot as the duel starts",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "As the duel starts, each tiger first turns on the spot to the other with its turn animation. A very "
		                "small turn also plays this animation. Then each tiger plays its start animation, and it faces the "
		                "other at once on each game turn. As a tiger starts or ends a block, it does not turn.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.turn_blend",
		    .name = "Fighters move smoothly between game turns",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "Each body takes a pose once a game turn, and each frame shows the body between its last two poses. "
		                "A lunge or a step moves the body smoothly forward. The body does not move ahead between two turns "
		                "and then jump back. A push apart also shows over one turn.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
		all.push_back({
		    .id = "combat.queued_blows",
		    .name = "Queued blows follow each other at once",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a blue tiger that fights by itself. The red tiger gets three "
		                   "blows in the middle quickly, so the second blow and the third blow wait in its queue.",
		    .expected = "Each queued blow starts in the game turn in which the blow before it ends, and it moves in that "
		                "turn. The last part of each blow, less than one turn, does not play. The original game does the "
		                "same.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 14.0f, 0.0f),
		                 Fight(Kind::FightBlow, 0, k_Mid, 0.1f, 0.0f), Fight(Kind::FightBlow, 0, k_Mid, 0.1f, 0.0f)},
		    .repeatFrom = 1,
		});
		all.push_back({
		    .id = "combat.own_end_tests",
		    .name = "The next fight state plays in the turn that the last one ends",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger knows Heal and fights a blue tiger that fights by itself. The red tiger "
		                   "blocks, then gets a blow in the middle, then Heal. Then the debug tools stop the fight.",
		    .expected =
		        "The start of the fight plays by the span of the turn and ends when the span gets to its end. "
		        "When the start, the start or end of the block, or the start, hold or end of the cast ends, the "
		        "next state moves on in the same turn. The next move starts in the turn that the block ends. The finish "
		        "ends when the span gets to its end, a little before its last frame. The original game does the "
		        "same.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
		    .commands = {{.kind = Kind::KnowMiracle,
		                  .creature = 0,
		                  .delaySeconds = 0.5f,
		                  .value = static_cast<size_t>(MagicType::Heal)},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightBlock, 0, 0, 14.0f),
		                 Fight(Kind::FightBlow, 0, k_Mid, 3.0f, 0.0f),
		                 Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 5.0f),
		                 Fight(Kind::FightAbort, 0, 0, 6.0f)},
		});
		all.push_back({
		    .id = "appearance.burning_body_flames",
		    .name = "Flames stay on the limbs of a burning creature",
		    .facet = Facet::Appearance,
		    .description = "A tiger of no player stands north of the middle, short of the testbed's lake, and catches "
		                   "fire. The camera follows it closely.",
		    .expected = "It runs to the lake. Each flame stays on one part of the body: a flame on a leg moves with "
		                "the leg in each frame. The flames do not stay behind at the place where the limb was.",
		    .framing = {.shot = Shot::Follow, .distance = 1.4f},
		    .creatures = {CreatureSetup {
		        .species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .owner = PlayerNames::NEUTRAL}},
		    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
		});
		all.push_back({
		    .id = "leash.rope_starts_at_collar",
		    .name = "A new leash rope starts at the collar",
		    .facet = Facet::Leash,
		    .description = "A tiger walks to the west. While it walks, it goes on the learning leash, then the leash "
		                   "comes off. The leash goes on again three times.",
		    .expected = "Each time the leash goes on, the rope starts straight from the collar to the hand. The rope "
		                "then follows the collar in each frame. It does not start at a point away from the body.",
		    .framing = {.shot = Shot::Follow, .distance = 1.4f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
		    .commands = {Go(Kind::WalkTo, 0, {-80.0f, 40.0f}, 1.0f, false), Leash(0, LeashType::Rope, 1.0f),
		                 Act(Kind::TakeOffLeash, 0, 2.0f), Leash(0, LeashType::Rope, 1.0f), Act(Kind::TakeOffLeash, 0, 2.0f),
		                 Leash(0, LeashType::Rope, 1.0f), Act(Kind::TakeOffLeash, 0, 2.0f), Leash(0, LeashType::Rope, 1.0f)},
		    .repeatFrom = 0,
		});
	}
}

void AddMind(std::vector<Scenario>& all)
{
	// The game's belief type for villagers, the deed of playing with a toy, and the first skill of the game's table
	constexpr size_t k_VillagerBelief = 6;
	constexpr size_t k_PlayWithToy = 41;
	constexpr size_t k_FirstSkill = 0;
	const auto setDesire = [](Desire desire, float amount, float delay) {
		return Command {.kind = Kind::SetDesire,
		                .creature = 0,
		                .delaySeconds = delay,
		                .value = static_cast<size_t>(desire),
		                .amount = amount};
	};

	// Facing away from the camera, their minds free, while a script's cinema bars come and go
	const auto awayOwned = Content(CreatureType::Tiger, {-20.0f, 0.0f}, 180.0f, "player's");
	auto awayNeutral = Content(CreatureType::Cow, {20.0f, 0.0f}, 180.0f, "no player's");
	awayNeutral.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "mind.cut_scene_look",
	    .name = "Creatures looking at the camera in a cut scene",
	    .facet = Facet::Mind,
	    .description = "A player's tiger and a cow with no player stand facing away from the camera, their minds free. "
	                   "A script's cinema bars slide in at once (before any help script can take the dialogue, whose "
	                   "release would take the bars away), and twenty-five seconds later out again.",
	    .expected = "As the bars come in both stop what they were doing. The tiger turns round to the camera and looks at "
	                "it, curious; now and then it first points at the camera, waves, talks, or looks confused and talks, "
	                "and keeps on looking until the bars go. The cow doesn't turn but may point, wave and talk. Once the "
	                "bars are out each finishes the look it is in and goes back to its own life.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {awayOwned, awayNeutral},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.0f, .value = 1},
	                 {.kind = Kind::WideScreen, .delaySeconds = 25.0f, .value = 0}},
	});

	// The camera looks away from the creature, so the player does not watch it
	all.push_back({
	    .id = "mind.lonely_unwatched",
	    .name = "Lonely when the player does not watch it",
	    .facet = Facet::Mind,
	    .description = "A player's cow stands in the middle of the map, its mind free. The camera stands to its north and "
	                   "looks further north, away from it.",
	    .expected = "The player does not watch it, so it gets lonely: its loneliness source goes up by a sixtieth each "
	                "second and is full after one minute, and its desire to attract the player's attention grows. Turn "
	                "the camera to it and its loneliness source goes to zero at once. Its desire to get high stays at "
	                "zero, because it has not eaten a hundred magic mushrooms.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Cow, {0.0f, 0.0f}, 0.0f)},
	});

	all.push_back({
	    .id = "mind.look_at_camera_still_hand",
	    .name = "Looking at the camera stops when the hand stays still",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. A script tells it to look "
	                   "at the camera after one second, and again after twelve seconds.",
	    .expected = "Each time it turns round to the camera and watches it, curious. If the hand moves all the time, each "
	                "look lasts six seconds. If the hand stays in one place, the first look stops five seconds after "
	                "the hand last moved, and the second look stops at once.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtCamera"},
	                 {.kind = Kind::DoAction, .delaySeconds = 12.0f, .object = 0, .action = "LookAtCamera"}},
	});

	all.push_back({
	    .id = "mind.head_look_hand",
	    .name = "An idle lion watches the hand",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands side on to the camera, a birch far from it. A script tells it to look at "
	                   "the hand every six seconds. Move the hand from side to side over the land in front of it.",
	    .expected = "Its head turns to follow the hand, quickly at first and then easing to a stop on it. The head does "
	                "not turn further than half a turn to either side or a quarter turn up or down.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    // The script needs a thing to tell the creature about. The look itself is at the hand.
	    .objects = {{.type = TreeInfo::Birch, .offset = {-100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 6.0f, .object = 0, .action = "LookAtHand"}},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "creatures.reaction_cut_short",
	    .name = "A forced action stops a reaction",
	    .facet = Facet::Mind,
	    .description = "A creature stands facing south, towards the camera. A cart 20 metres in front of it makes the "
	                   "reaction of a thing crushed by the player. After four seconds, a script tells the creature to "
	                   "look at the camera.",
	    .expected = "The creature goes to look at the cart. When the script tells it to look at the camera, it stops the "
	                "look at the cart at once and turns its face to the camera. It does not go back to the cart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {35.0f, 12.0f, -10.0f}, .look = {0.0f, 6.0f, -10.0f}},
	    .creatures = {{.offset = {0.0f, 0.0f}, .facingDegrees = 0.0f}},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToObjectCrushed}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .object = 0, .action = "LookAtCamera"}},
	});

	all.push_back({
	    .id = "mind.look_at_land",
	    .name = "An idle lion looks at the hills and the sea",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands alone, with only a birch far from it, facing the nearest hill. A script "
	                   "tells it to be idle every fifteen seconds.",
	    .expected = "While it waits, nothing else catches its eye, so its head turns to the nearest hill or sea that it "
	                "did not go to yet, at the ground there. When that place is no longer in front of it, it looks ahead "
	                "again until the hill or the sea comes back into view.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "mind.woken_eyes",
	    .name = "Yawning droops the lids until the lion stops",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands facing the camera. A script tells it to be idle every fifteen seconds: it "
	                   "waits and yawns two times.",
	    .expected = "With the first yawn its lids droop and it blinks slowly. The lids stay down through the wait and the "
	                "second yawn. When it stops being idle, its lids open by how rested it is: nearly wide when it is "
	                "fresh.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 0.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 1,
	});

	// The emotes: a script tells a player's lion to do one action again and again, with a villager beside it as the thing
	// that the action is done to
	const auto emote = [&all](std::string_view id, std::string_view name, std::string_view description,
	                          std::string_view expected, std::string_view action, float every, Environment environment,
	                          std::optional<float> villagerLife) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Mind,
		    .description = description,
		    .expected = expected,
		    .environment = environment,
		    .framing = {.shot = Shot::Testbed},
		    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-40.0f, 10.0f}, .life = villagerLife}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = action},
		                 {.kind = Kind::DoAction, .delaySeconds = every, .object = 0, .action = action}},
		    .repeatFrom = 1,
		});
	};
	emote("emote.howl", "The creature howls at the player",
	      "A script tells a player's lion to howl at the player every eight seconds.",
	      "It turns to face the camera, its head on the camera, then plays its howl with its sound. It pulls no face.",
	      "HowlAtPlayer", 8.0f, {}, std::nullopt);
	emote("emote.sad", "The creature is sad", "A script tells a player's lion to be sad every ten seconds.",
	      "It looks down at its feet with a sad face for two to five seconds, then it plays its sad clip or it cries, in "
	      "equal shares.",
	      "BeSad", 10.0f, {}, std::nullopt);
	emote("emote.silly_faces", "The creature pulls silly faces at a villager",
	      "A script tells a player's lion to pull silly faces at a villager beside it every twelve seconds.",
	      "It goes near the villager and turns to it. Two times in three it plays its playful clip. Then it pulls three "
	      "random faces in a row, one each turn, and holds the last for five seconds, its head on the villager.",
	      "PullSillyFaces", 12.0f, {}, std::nullopt);
	emote("emote.pathetic", "The creature is pathetic to the player",
	      "A script tells a player's lion to be pathetic to the player every fifteen seconds. Move the camera's focus "
	      "away from it.",
	      "It walks to the point that the camera looks at, for up to thirty seconds, its head on the camera. Then it "
	      "turns to the camera and plays its pick-me clip.",
	      "BePatheticToPlayer", 15.0f, {}, std::nullopt);
	emote("emote.stroke_heal", "The creature strokes a hurt villager and heals it",
	      "A villager at three tenths of its life stands near a player's lion. A script tells the lion to stroke it "
	      "every twenty seconds.",
	      "The lion picks the villager up, strokes it with a face of compassion, heals it to full life, then puts it "
	      "down. The villager then goes home.",
	      "Stroke", 20.0f, {}, 0.3f);
	emote("emote.look_at_sun", "The creature looks at the sun",
	      "At noon, a script tells a player's lion to look at the sun every six seconds.",
	      "It turns to face the far south-west of the land, its head on that point. One time in two it points there for "
	      "three seconds.",
	      "LookAtSun", 6.0f, {}, std::nullopt);
	emote("emote.look_at_moon", "The creature looks at the moon",
	      "At midnight, a script tells a player's lion to look at the moon every six seconds.",
	      "It turns to face the moon's place, its head on it. One time in two it points there for three seconds.", "LookAtMoon",
	      6.0f, {.hour = 0.0f}, std::nullopt);
	emote("emote.sit_down", "The creature sits down by a villager",
	      "A script tells a player's lion to sit down by a villager every twenty-five seconds.",
	      "It walks to the villager, turns to face it, faces down the slope, then sits for ten to fifteen seconds and "
	      "looks about.",
	      "SitDown", 25.0f, {}, std::nullopt);
	emote("emote.sleep_on_the_spot", "The creature sleeps at a clear place near it",
	      "A script tells a player's lion to sleep on the spot every forty seconds.",
	      "One time in two it yawns. It walks to a clear place near it, sleeps until it is rested, then gets up dazed "
	      "with a grimace.",
	      "SleepOnTheSpot", 40.0f, {}, std::nullopt);

	all.push_back({
	    .id = "mind.look_at_hand_turn",
	    .name = "Looking at the hand turns round once and then stops",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. A script tells it to look "
	                   "at the hand after one second, and again after twelve seconds. Keep the hand over the land in "
	                   "front of the camera.",
	    .expected = "Each time it turns round to the hand in one smooth turn, then watches it, curious, and stops after "
	                "about two seconds. It does not freeze at the start of its turn.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 12.0f, .object = 0, .action = "LookAtHand"}},
	});

	all.push_back({
	    .id = "mind.reward_villagers",
	    .name = "Learning: reward eating villagers, punish eating food",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger with a villager right by it, lots of magic food close by and more villagers further "
	                   "off. Every 18 seconds its "
	                   "hunger is set to 40% of its most, enough for one meal. Whatever it eats, as soon as it has eaten, "
	                   "it is stroked if it was a villager and slapped if it was anything else.",
	    .expected = "First it eats the villager beside it and is stroked: with one example, its hunger tree finds "
	                "everything good. Then it eats the nearest food and is slapped: now the tree tells them apart. From then "
	                "on it walks past the food to eat villagers, the "
	                "planner's goal usefulness for villagers near 0.8 and for food near 0. The spawner's Mind panel shows "
	                "the tree and the thoughts.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {12.0f, -12.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-25.0f, -25.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {25.0f, -30.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -40.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-35.0f, 15.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {35.0f, 20.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {80.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-80.0f, 40.0f}},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {0.0f, 85.0f}},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {60.0f, 75.0f}}},
	    .commands = {{.kind = Kind::RewardIf, .creature = 0, .delaySeconds = 1.0f, .value = k_VillagerBelief},
	                 setDesire(Desire::Hunger, 0.4f, 1.0f),
	                 Act(Kind::Stop, 0, 17.0f)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "mind.community",
	    .name = "Load a chosen creature mind",
	    .facet = Facet::Mind,
	    .description = "A wolf that takes up the mind file last opened with the spawner's Open mind file button (a "
	                   "community-made creature, say), or else the game's own Khazar creature mind, with villagers, "
	                   "food, trees and a ball about it.",
	    .expected = "Its name, desires, opinions and the examples of its decision trees come from the file (the spawner's "
	                "Mind panel lists them, and its thoughts). The planner weighs what it has learnt: it goes for the "
	                "things its trees rate well. If neither file can be read the wolf keeps a fresh mind and the log "
	                "says why.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Wolf, .mindFile = "chosen:KhazarCreature"}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}},
	                {.type = TreeInfo::Beech, .offset = {-50.0f, 40.0f}},
	                {.type = TreeInfo::Beech, .offset = {55.0f, 45.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {60.0f, -50.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-60.0f, -55.0f}}},
	});
	all.push_back({
	    .id = "mind.made_from_file_by_land",
	    .name = "A land makes a creature from a mind file",
	    .facet = Facet::Mind,
	    .description = "The land's script line CREATE_CREATURE_FROM_FILE makes the second player's creature from the "
	                   "game's mind file ComputerControlledCreature, 20 m east of the middle, as the later lands do. Save "
	                   "the game and read the creature's mind file.",
	    .expected = "The creature takes up the file. Then it knows all leashes and it is fully grown (stage 13, its time "
	                "at the stage 0). In the saved mind file, the four leash words are 1 and the stage is 13.",
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-20.0f, 0.0f}}},
	    .landScript = R"(CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 6, "ComputerControlledCreature", "20.0,0.0"))",
	});

	std::vector<Command> phases;
	for (size_t phase = 1; phase <= k_LastPhase; ++phase)
	{
		phases.push_back({.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 8.0f, .value = phase});
	}
	all.push_back({
	    .id = "mind.phases",
	    .name = "Stages of growing up unlock desires",
	    .facet = Facet::Mind,
	    .description = "A cow that starts at the first stage of growing up and moves on a stage every eight seconds, "
	                   "through all fourteen.",
	    .expected = "At first only fear, tiredness, attention, showing how it is and the like are active; hunger, "
	                "curiosity and play come at the second stage, water at the fourth, anger at the eleventh, compassion "
	                "at the twelfth. The Mind panel's desires list grows, and the planner starts weighing what each new "
	                "desire can do.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Cow, .phase = 0}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}}},
	    .commands = phases,
	});

	all.push_back({
	    .id = "mind.slap_reaction",
	    .name = "What a slap makes it show",
	    .facet = Facet::Mind,
	    .description = "The hand slaps a grown-up tiger facing the camera hard on the head and lets go. The scenario "
	                   "repeats, so each run can show another reaction.",
	    .expected = "After the slap's reeling animation, the tiger shows one of four reactions, each one time in four: "
	                "frightened when it feels no more anger than kindness, else angry or a rude gesture; sad; it cries; "
	                "or it looks at the player for one to two seconds with darting eyes and a sad face.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Slap(0, 0.85f, false, false, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true), Act(Kind::Stop, 0, 6.0f)},
	    .repeatFrom = 0,
	});

	auto young = Content(CreatureType::Tiger, {0.0f, 0.0f});
	young.phase = 1;
	all.push_back({
	    .id = "mind.young_feedback",
	    .name = "A young creature stroked and slapped",
	    .facet = Facet::Mind,
	    .description = "A tiger at the second stage of growing up, too young to learn from the hand, is stroked and let "
	                   "go, then slapped hard and let go. The scenario repeats.",
	    .expected = "One stroke in two, it then plays its happy animation. One slap in two, it is then frightened of the "
	                "player: it runs away one time in three, else it cowers where it stands. Each other slap shows one of "
	                "the four reactions of a grown-up creature. The advisor says nothing: the help needs a grown-up "
	                "creature.",
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {young},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Act(Kind::Stop, 0, 6.0f), Slap(0, 0.85f, false, false, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Act(Kind::Stop, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	auto grown = Content(CreatureType::Tiger, {0.0f, 20.0f});
	grown.phase = 13;
	all.push_back({
	    .id = "mind.lost_thing",
	    .name = "The creature loses what it goes to",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told to act on a cow. When the advisor has said what "
	                   "the tiger wants, and before the tiger is done with the cow, the cow is removed from the world.",
	    .expected = "The tiger stops what it does and decides again. The good advisor says: \"Your Creature lost the "
	                "object he was looking for.\"",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {grown},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {30.0f, 60.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 OnObject(Kind::RemoveObject, 0, 0, 9.0f, false), Act(Kind::TakeOffLeash, 0, 8.0f)},
	});

	// The help explains why an order fails, says what the creature wants, and tells what a stroke taught it
	all.push_back({
	    .id = "mind.help_explanations",
	    .name = "The help explains what the creature does and learns",
	    .facet = Facet::Mind,
	    .description = "The pointer goes high in the sky, away from the tiger. A grown-up tiger on the learning leash is "
	                   "told with the Action button to go to a place off the land, then to act on a cow. The leash comes "
	                   "off, and the player strokes it twice.",
	    .expected = "The order off the land gets no acknowledge sound and no marker, and the help says: \"Your "
	                "Creature wasn't able to get to his destination because it is inaccessible.\" Told to act on the "
	                "cow, the help says what the tiger wants: \"Your Creature is feeling curious.\" After the strokes, "
	                "the good advisor comes out, looks at the tiger and says what the stroke taught it about the source of "
	                "what it did last, such as \"From now on, your Creature will want to look around more.\" or \"From "
	                "now on, your Creature will pay more attention to you.\" Each line has its voice.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {grown},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {30.0f, 60.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.9f, 0.1f}},
	                 Leash(0, LeashType::Rope, 0.5f),
	                 Go(Kind::LeashOrderAt, 0, {4800.0f, 4800.0f}, 2.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 10.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f),
	                 Act(Kind::Stroke, 0, 9.5f),
	                 Act(Kind::Stroke, 0, 4.0f)},
	});

	auto curious = Content(CreatureType::Tiger, {0.0f, 20.0f});
	curious.phase = 13;
	all.push_back({
	    .id = "learning.forgotten_order",
	    .name = "The creature forgets the oldest of its orders",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told with the Action button to act on six cows, one "
	                   "after the other, 90 seconds apart.",
	    .expected = "The tiger picks up and holds each cow for about 44 seconds. The inspector shows each remembered action "
	                "marked as ordered, and as done when the hold ends. At the sixth order the log shows that the tiger "
	                "forgets the first order. The hold serves its curiosity, which learns nothing from orders, so the "
	                "log shows no approval.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {curious},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {20.0f, 40.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 40.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 60.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 60.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 80.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 80.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 1, 90.0f, false), OnObject(Kind::LeashOrderOn, 0, 2, 90.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 3, 90.0f, false), OnObject(Kind::LeashOrderOn, 0, 4, 90.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 5, 90.0f, false), Act(Kind::TakeOffLeash, 0, 90.0f)},
	});

	auto watched = Content(CreatureType::Tiger, {0.0f, 20.0f});
	watched.phase = 13;
	all.push_back({
	    .id = "learning.seen_action",
	    .name = "The player sees the action that it orders",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told with the Action button to act on a cow. The camera "
	                   "follows the tiger the whole time.",
	    .expected = "The tiger picks up and holds the cow. The inspector shows the newest remembered action marked as "
	                "seen once the main step of its agenda moves on, and as done when the hold ends. Feedback then goes "
	                "to it, less and less until its learning window closes.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {watched},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {20.0f, 40.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 60.0f)},
	});

	auto iconWatcher = Content(CreatureType::Tiger, {0.0f, 0.0f});
	iconWatcher.phase = 13;
	auto healed = Content(CreatureType::Cow, {0.0f, 30.0f}, 180.0f, "hurt");
	healed.needs.life = 0.3f;
	all.push_back({
	    .id = "learning.icon_miracle_without_seed",
	    .name = "A miracle that an icon made, and its seed",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger stands near a hurt cow. A heal seed from the player's worship (made at an icon) "
	                   "is pressed on the cow. The seed leaves the hand but stays linked to the heal until the heal goes.",
	    .expected = "If the tiger takes up its reaction at the moment of the cast, the log tells that it ignores the "
	                "miracle: the seed is not linked yet. When it takes up the reaction later, while the heal lasts, it "
	                "is not ignored (no such log line) and the heal counts towards learning the miracle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 30.0f}}, .distance = 0.6f},
	    .creatures = {iconWatcher, healed},
	    .miracles = {{.type = MagicType::Heal,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 1,
	                  .handOffset = {0.0f, 30.0f},
	                  .delaySeconds = 2.0f,
	                  .byHand = true,
	                  .fromWorship = true}},
	});

	{
		// A ring of huts shuts the tiger in, with a cow outside it
		auto shutIn = Content(CreatureType::Tiger, {0.0f, 0.0f});
		shutIn.phase = 13;
		std::vector<ObjectSetup> objects {{.type = AnimalInfo::Cow, .offset = {0.0f, 90.0f}}};
		constexpr int k_Huts = 24;
		constexpr float k_RingRadius = 24.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back({.type = AbodeInfo::CelticHut,
			                   .offset = {k_RingRadius * portable_math::Sin(angle), k_RingRadius * portable_math::Cos(angle)},
			                   .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.clear_way",
		    .name = "The creature clears its way out",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash stands in a ring of huts. It is told to act on a cow "
		                   "outside the ring.",
		    .expected = "The tiger finds no way out. It kicks one of the huts that the search met, or throws it when it "
		                "can pick it up. Then it walks on to where it went. A tiger with no way and nothing to clear "
		                "makes a sound of refusal and gives up.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {shutIn},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A tight ring of huts shuts a rock in, so that the tiger finds no way to it. A second rock lies in the open.
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -60.0f}},
		                                  {.type = MobileStaticInfo::Rock, .offset = {40.0f, 30.0f}}};
		constexpr int k_Huts = 12;
		constexpr float k_RingRadius = 9.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back(
			    {.type = AbodeInfo::CelticHut,
			     .offset = {k_RingRadius * portable_math::Sin(angle), -60.0f + k_RingRadius * portable_math::Cos(angle)},
			     .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.pick_up_no_way",
		    .name = "The creature stops when it finds no way to a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash. A ring of huts shuts a rock in, and a second rock lies "
		                   "in the open. The player tells it to act on the shut-in rock. Then the scenario removes the leash.",
		    .expected = "The tiger walks to pick up the shut-in rock. When the search finds no route, it stops at once and "
		                "makes a sound of refusal. Then it thinks for itself and does something different. For ten seconds "
		                "it does not choose the shut-in rock again.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A tight ring of huts shuts a rock in, so that the tiger finds no way to it to stomp on it
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -60.0f}}};
		constexpr int k_Huts = 12;
		constexpr float k_RingRadius = 9.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back(
			    {.type = AbodeInfo::CelticHut,
			     .offset = {k_RingRadius * portable_math::Sin(angle), -60.0f + k_RingRadius * portable_math::Cos(angle)},
			     .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.stomp_no_way",
		    .name = "The creature stops when it finds no way to a thing to stomp on",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger. A ring of huts shuts a rock in. The script tells the tiger to smash the rock.",
		    .expected = "The tiger walks to stomp on the rock. When the search finds no route, it stops at once and makes "
		                "a sound of refusal. For ten seconds it does not choose the rock again.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = std::move(objects),
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "SmashStoneInHalf"}},
		});
	}

	{
		// A ring of huts shuts the tiger in, with a rock outside it
		auto shutIn = Content(CreatureType::Tiger, {0.0f, 0.0f});
		shutIn.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 60.0f}}};
		constexpr int k_Huts = 24;
		constexpr float k_RingRadius = 24.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back({.type = AbodeInfo::CelticHut,
			                   .offset = {k_RingRadius * portable_math::Sin(angle), k_RingRadius * portable_math::Cos(angle)},
			                   .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.pick_up_shut_in",
		    .name = "The creature clears its way out to pick up a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash stands in a ring of huts. The player tells it to act on "
		                   "a rock outside the ring.",
		    .expected = "The tiger walks to pick up the rock and finds no way out. It stops the pick-up. It kicks one of "
		                "the huts that the search met, or throws it when it can pick it up. Then it walks on to where it "
		                "went.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {shutIn},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A far rock to pick up, and a near ball to look at
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		all.push_back({
		    .id = "mind.script_action_stops_walk",
		    .name = "A script action stops a walk to a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger. The script tells it to throw a rock 150 m away. After four seconds, while "
		                   "the tiger walks to the rock, the script tells it to look at a ball near it.",
		    .expected = "The tiger walks to the rock. When the second order comes, it stops at once and turns to look at "
		                "the ball. It does not walk on to the rock.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -150.0f}},
		                {.type = MobileStaticInfo::ToyBall, .offset = {25.0f, 10.0f}}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "ThrowAround"},
		                 {.kind = Kind::DoAction, .delaySeconds = 4.0f, .object = 1, .action = "LookButDontApproach"}},
		});
	}

	{
		// A small rock between a fence and a hut in a village, 20 m from a young tiger of the size it has on the first land
		auto smallTiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		smallTiger.size = 0.26f;
		all.push_back({
		    .id = "mind.pick_up_small_rock_by_hut",
		    .name = "The creature picks up a small rock between a fence and a hut",
		    .facet = Facet::Mind,
		    .description = "A young tiger. A small rock lies 20 m from it, between a short fence, a second rock and a hut. "
		                   "The script tells the tiger to pick up the small rock and look at it.",
		    .expected = "The tiger walks round to the small rock and stops at the length of its arm from it. It picks the "
		                "rock up at once. It does not walk again and again beside the rock.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {smallTiger},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {20.0f, 0.0f}, .scale = 0.267f},
		                {.type = MobileStaticInfo::CeltFenceShort, .offset = {22.5f, 3.0f}, .yawDegrees = 45.0f},
		                {.type = MobileStaticInfo::Rock, .offset = {23.3f, -6.3f}, .scale = 0.267f},
		                {.type = AbodeInfo::CelticShackX, .offset = {28.0f, 2.7f}}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "ExamineByPickingUp"}},
		});
	}

	all.push_back({
	    .id = "mind.pick_up_running_sheep",
	    .name = "The creature walks to a sheep that runs and picks it up",
	    .facet = Facet::Mind,
	    .description = "A tiger and a sheep 15 m in front of it. After one second the sheep runs away from the tiger. "
	                   "Then the script tells the tiger to pick up the sheep and look at it.",
	    .expected = "On each turn the tiger aims at the place where the sheep will be when it gets there, not at the "
	                "place where the sheep is. When the sheep stops, the tiger walks up to it, reaches to where the "
	                "sheep will be at the moment of the hold, and picks it up.",
	    .framing = {.shot = Shot::Follow, .distance = 3.0f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {15.0f, 0.0f}, .living = true}},
	    .commands = {{.kind = Kind::AnimalFlees, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::DoAction, .delaySeconds = 0.5f, .object = 0, .action = "ExamineByPickingUp"}},
	});

	all.push_back({
	    .id = "mind.pick_up_walking_villager",
	    .name = "The creature walks to a villager that walks past and picks it up",
	    .facet = Facet::Mind,
	    .description = "A tiger. A villager 30 m in front of it and 20 m to the side walks across in front of the tiger, "
	                   "to 40 m on the other side. The script tells the tiger to pick up the villager and look at it.",
	    .expected = "The tiger walks to a place ahead of the villager on the villager's way, and aims again on each "
	                "turn. It reaches to where the villager will be at the moment of the hold, and picks it up.",
	    .framing = {.shot = Shot::Follow, .distance = 3.0f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {30.0f, 20.0f},
	                 .walkTo = glm::vec2 {30.0f, -40.0f},
	                 .walkAfterSeconds = 0.5f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ExamineByPickingUp"}},
	});

	all.push_back({
	    .id = "mind.mimic",
	    .name = "Mimic the player",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with two toy balls about it. Every 20 seconds the player plays with a toy near it, "
	                   "the one deed creatures copy without the learning leash.",
	    .expected = "Every time, unless it is still copying the action of the last time, it picks up a ball and throws "
	                "it about, copying the player; then for a while it wants to play. The deed is on no thing, so the "
	                "creature does not show that it saw it. The Mind panel shows the stage of copying.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    // A loose ball from the mobile object table is not a toy: the toys are statics
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {25.0f, -25.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {-30.0f, -20.0f}}},
	    .commands =
	        {{.kind = Kind::PlayerDid, .creature = 0, .delaySeconds = 3.0f, .point = {30.0f, -30.0f}, .value = k_PlayWithToy},
	         Act(Kind::Stop, 0, 20.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.mimic_same_kind",
	    .name = "Mimic the player on a thing of the same kind",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger. The player plays with a loose ball that is not a toy. A toy die lies near the "
	                   "tiger and a cuddly toy lies far from it. Every 20 seconds the player plays again.",
	    .expected = "The tiger cannot play with the player's ball, so it plays with the nearest thing of the same kind: "
	                "the toy die. The log shows the pass and the thing for each copy.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {30.0f, -30.0f}},
	                {.type = MobileStaticInfo::ToyDie, .offset = {12.0f, 8.0f}},
	                {.type = MobileStaticInfo::ToyCuddly, .offset = {-35.0f, 25.0f}}},
	    .commands = {{.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 3.0f, .value = k_PlayWithToy, .object = 0},
	                 Act(Kind::Stop, 0, 20.0f)},
	    .repeatFrom = 0,
	});

	// The deeds of the player that the five notices follow, from the least likely to be copied to the most likely: a
	// creature that copies a deed only starts to copy a deed that is as likely or more likely
	constexpr size_t k_PlantTree = 11;
	constexpr size_t k_PutFoodInWorshipSite = 0;
	constexpr size_t k_DamageByThrowing = 15;
	constexpr size_t k_StealObjectToTown = 35;
	const auto deedOn = [](size_t deed, float delay) {
		return Command {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = delay, .value = deed, .object = 0};
	};
	all.push_back({
	    .id = "mind.notice_deed",
	    .name = "Show that it saw a deed",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash, near a rock. Every 20 seconds the player does a deed on "
	                   "the rock: plant a tree, put food in a worship site, damage by a throw, steal a thing, play with "
	                   "a toy.",
	    .expected = "After each deed the tiger points at the rock, amazed, turns to the camera and watches it. Then it "
	                "plays an action: aha after the tree, happy or aha after the food, angry (and one time in three a "
	                "laugh first) after the damage, a scratch after the theft, happy after the toy. The aggressive "
	                "notice points for a shorter time and watches the camera one time, angry.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {25.0f, -25.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), deedOn(k_PlantTree, 2.0f), deedOn(k_PutFoodInWorshipSite, 20.0f),
	                 deedOn(k_DamageByThrowing, 20.0f), deedOn(k_StealObjectToTown, 20.0f), deedOn(k_PlayWithToy, 20.0f)},
	});

	constexpr size_t k_PutWoodInWorkshop = 9;
	all.push_back({
	    .id = "mind.copy_wood_to_workshop",
	    .name = "Copy the player's gift of wood to a workshop",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash, with its town's workshop near it and in its view, and a birch "
	                   "behind it. Every 30 seconds the player gives wood to the workshop, four times.",
	    .expected = "After a gift the tiger points at the workshop and watches the camera. When it copies the gift, it "
	                "walks to the birch, pulls it up, carries it to the workshop and throws it in: the workshop's pile of "
	                "logs shows the wood. The log shows the copy of the gift as the action GiveWoodFromTreeToWorkshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -45.0f}, {40.0f, 25.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = AbodeInfo::CelticWorkshop, .offset = {30.0f, -35.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = TreeInfo::Birch, .offset = {-25.0f, 20.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), deedOn(k_PutWoodInWorkshop, 2.0f), deedOn(k_PutWoodInWorkshop, 30.0f),
	                 deedOn(k_PutWoodInWorkshop, 30.0f), deedOn(k_PutWoodInWorkshop, 30.0f)},
	});

	all.push_back({
	    .id = "mind.deed_sight",
	    .name = "Copy only a deed it can see",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger faces a rock 40 m ahead. Another rock is 40 m behind it "
	                   "and a third 400 m ahead. The player plays with a toy at the rock behind it, then at the far "
	                   "rock, then at the near rock ahead.",
	    .expected = "The tiger does not copy the deeds behind it and far away. It copies only the deed at the near rock: "
	                "the log shows \"I saw you\" one time. The Desires panel shows the first source of the desire to "
	                "play 0.1 higher and the wish to follow the player 0.3 higher after that deed.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 40.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, -400.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, -40.0f}}},
	    .commands = {{.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 2.0f, .value = k_PlayWithToy, .object = 0},
	                 {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 4.0f, .value = k_PlayWithToy, .object = 1},
	                 {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 4.0f, .value = k_PlayWithToy, .object = 2}},
	});

	all.push_back({
	    .id = "mind.copy_broken_rock",
	    .name = "Copy a broken rock",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash faces a chalk rock 30 m ahead. Two more chalk rocks are to "
	                   "its left and right. The hand taps the rock ahead and breaks it. Then the leash comes off.",
	    .expected = "The tiger sees the deed: the log shows \"I saw you: Break rocks\" and it learns that rocks are good to "
	                "play with. The broken rock is gone, so it does not walk to it. When its wish to follow the player wins, "
	                "or after 3 minutes, it copies the deed: it picks up a rock of the same kind and breaks it. The log "
	                "shows \"I'm copying you: BreakRock\".",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -30.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-15.0f, -20.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {15.0f, -20.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 {.kind = Kind::HandTapObject, .delaySeconds = 2.0f, .object = 0},
	                 Act(Kind::TakeOffLeash, 0, 2.0f)},
	});

	// The creature's happy action, played once as a script asks
	constexpr uint32_t k_HappyAction = 55;
	all.push_back({
	    .id = "mind.hand_free_action",
	    .name = "Let go of a thing before an action",
	    .facet = Facet::Mind,
	    .description = "A content tiger. A script tells it to hold a rock. 8 seconds later, while it holds the rock, a "
	                   "script tells it to play its happy action two times. This repeats every 20 seconds.",
	    .expected = "Before the first happy action, the tiger lets go of the rock. It puts the rock down when it finds a "
	                "plan for the rock with Impress, Compassion or Curiosity. Else it tosses the rock away. Then it plays "
	                "the action two times. The log shows how it let go.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {15.0f, -15.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::ScriptPlay, .delaySeconds = 8.0f, .value = k_HappyAction, .amount = 2.0f},
	                 Act(Kind::Stop, 0, 11.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "saves.creature_resumes",
	    .name = "Save and load: the creature does its plan again",
	    .facet = Facet::Mind,
	    .description = "A content tiger of the second player, so a load gives it the mind of the saved game's mind "
	                   "file. A script tells it to hold a rock 250 metres away, so the tiger walks to the rock. 4 "
	                   "seconds later the game saves into slot 0. 6 seconds after that, before the tiger gets to the "
	                   "rock, the game loads slot 0.",
	    .expected = "At the load the tiger is back where it was at the save, facing the same way. A script controls "
	                "it, so the save kept its plan: it walks to the rock again from the start of its step and holds "
	                "the rock. The log shows the save and the load.",
	    .framing = {.shot = Shot::Overview, .include = {{100.0f, -15.0f}, {260.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    return tiger;
	    }()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 4.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 6.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.creed_glow",
	    .name = "Save and load: the load restores the creed glows",
	    .facet = Facet::Mind,
	    .description = "A tiger of the second player stands on the land. After a second, a script gives it a full creed "
	                   "power above its body and a half power on its left hand. 2 seconds later the game saves into "
	                   "slot 0. 1 second after that, the script removes the two powers. 3 seconds later the game loads "
	                   "slot 0.",
	    .expected = "The tiger gets a glowing shell above its head and one on its left hand, and it starts to hum. The "
	                "shells go and the hum stops. At the load the two shells appear again with the same size, and the "
	                "tiger hums again. The log shows the save and the load.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{80.0f, -15.0f}, {120.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    tiger.hold = true;
		    tiger.pauseMind = true;
		    return tiger;
	    }()},
	    .commands = {{.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 0.0f, .value = 0, .amount = 0.5f},
	                 {.kind = Kind::SaveGame, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 0.0f},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 0.0f, .value = 0, .amount = 0.0f},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.world_back",
	    .name = "Save and load: the whole world comes back",
	    .facet = Facet::Mind,
	    .description = "A content tiger of the second player. A script tells it to hold a rock 250 metres away, so the "
	                   "tiger walks to the rock. 3 seconds later the game saves into slot 0. The tiger walks on. 5 "
	                   "seconds after the save the game loads slot 0: the testbed's flat land is built again and the "
	                   "saved world takes its place.",
	    .expected = "At the load the whole testbed is as it was at the save: the tiger, the rock, the dispensers and "
	                "the camera are where they were, and the clock shows the turn of the save. The tiger walks to the "
	                "rock again. The log shows the saved world with its count of entities, then the load of the whole "
	                "world of slot 0.",
	    .framing = {.shot = Shot::Overview, .include = {{100.0f, -15.0f}, {260.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    return tiger;
	    }()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 5.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.fire_and_walk_back",
	    .name = "Save and load: the fires burn on and the walk continues",
	    .facet = Facet::Mind,
	    .description = "Three oaks burn. A content tiger walks to a rock 250 metres away. 3 seconds later the game saves "
	                   "into slot 0. 5 seconds after the save the game loads slot 0.",
	    .expected = "At the load the oaks burn as they did at the save, with the same flames, and their fire continues "
	                "to grow and spread. The tiger continues on the same route to the rock. The log shows the running "
	                "state of the systems in the save and no section that does not read.",
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -40.0f}, {260.0f, 15.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {100.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, -30.0f}, .alight = true},
	                {.type = TreeInfo::Oak, .offset = {-50.0f, -20.0f}, .alight = true},
	                {.type = TreeInfo::Oak, .offset = {-30.0f, -40.0f}, .alight = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 5.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.miracles_back",
	    .name = "Save and load: a shield and a fireball in flight come back",
	    .facet = Facet::Miracles,
	    .description = "A shield over a content tiger. 4 seconds after the start a hand throws a fireball at the shield. "
	                   "4.6 seconds after the start the game saves into slot 0. 3 seconds after the save the game loads "
	                   "slot 0.",
	    .expected = "At the save the box shows \"Saving...\" on a grey band across the middle of a darker screen. At the "
	                "load the box shows \"Loading...\". After the load the shield stands with its vapour. The fireball "
	                "continues its flight from where it was at the save, bounces off the shield with a spark and burns "
	                "out. The log shows no section that does not read and no value that the save does not keep.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -5.0f}, {0.0f, 70.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .miracles = {{.type = MagicType::Shield, .point = {0.0f, 40.0f}, .delaySeconds = 0.5f},
	                 {.type = MagicType::Fireball,
	                  .point = {0.0f, 40.0f},
	                  .handOffset = {0.0f, -5.0f},
	                  .handHeight = 10.0f,
	                  .throwVelocity = {0.0f, 4.0f, 40.0f},
	                  .delaySeconds = 4.0f}},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 4.6f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	// A physical shield stands in the physics as a solid dome from the turn that it goes up
	all.push_back({
	    .id = "miracles.physical_shield_stops_boulders",
	    .name = "A physical shield stops thrown boulders",
	    .facet = Facet::Miracles,
	    .description = "A hand casts a physical shield over a point 40 metres north. 3 seconds after the start a hand "
	                   "throws a boulder north at the shield from 50 metres south of it. The throw occurs again "
	                   "every 6 seconds.",
	    .expected = "The dome rises and spins. Each boulder hits the side of the dome and bounces back or rolls off it. "
	                "No boulder goes through the dome or lands in it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -10.0f}, {0.0f, 70.0f}}},
	    .objects = {{.type = MobileStaticInfo::Boulder1Chalk, .offset = {0.0f, -10.0f}}},
	    .miracles = {{.type = MagicType::PhysicalShield, .point = {0.0f, 40.0f}, .delaySeconds = 0.5f}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {0.0f, -10.0f},
	                .height = 6.0f,
	                .velocity = {0.0f, 6.0f, 25.0f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 6.0f}},
	});

	// Two rocks fall fast on a small physical shield, as the rocks of Khazar's Shield Challenge fall on the shield over
	// his hut: the dome is light and soft, so it holds the smaller rock and lets the larger rock through
	all.push_back({
	    .id = "miracles.small_shield_and_falling_rocks",
	    .name = "A small physical shield and two falling rocks",
	    .facet = Facet::Miracles,
	    .description = "A script casts a physical shield of radius 15.6 over a point 40 metres north. 3 seconds after "
	                   "the start a rock of scale 0.6 falls from 50 metres above a point 22 metres east and 21 metres "
	                   "south of the shield's middle. It falls toward the middle and arrives there after 2 seconds, as "
	                   "the challenge's rocks do. 6 seconds later a rock of scale 0.7 falls in the same way. The falls "
	                   "occur again every 12 seconds.",
	    .expected = "The dome rises and spins. Each rock hits the dome at about 30 metres a second. The rock of scale "
	                "0.6 sinks a little into the dome, then rolls off it to the land outside. The rock of scale 0.7 "
	                "sinks into the dome, goes through it and lands near its middle, as in the original game: the "
	                "small dome is too soft for a rock of more than 5 tonnes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {30.0f, 60.0f}}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {22.0f, 19.0f}, .scale = 0.6f},
	                {.type = MobileStaticInfo::Rock, .offset = {28.0f, 19.0f}, .scale = 0.7f}},
	    .miracles = {{.type = MagicType::PhysicalShield,
	                  .point = {0.0f, 40.0f},
	                  .delaySeconds = 0.5f,
	                  .script = ScriptSpell {.radius = 15.6f, .seconds = 600.0f, .curl = 0.0f}}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {22.0f, 19.0f},
	                .height = 50.0f,
	                .velocity = {-11.0f, -15.19f, 10.5f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 12.0f},
	               {.object = 1,
	                .from = glm::vec2 {22.0f, 19.0f},
	                .height = 50.0f,
	                .velocity = {-11.0f, -15.19f, 10.5f},
	                .delaySeconds = 9.0f,
	                .repeatSeconds = 12.0f}},
	});

	// A save while sounds, script music and an advisor's line play: a load brings back only what the game keeps
	all.push_back({
	    .id = "saves.sounds_music_help_back",
	    .name = "Save and load: the sounds, the music and the help text",
	    .facet = Facet::Audio,
	    .description = "The player's giant ape gets the full creed power and hums. A script attaches the village bell "
	                   "sound to a ball. A script starts the music of the chosen creature. A script shows a help text "
	                   "that the good advisor says. While the advisor talks, the game saves into slot 0. 3 seconds "
	                   "after the save the game loads slot 0.",
	    .expected = "After the load no sound that played at the save continues. The hum of the ape starts again. The "
	                "bell of the ball does not start again. The help text shows again with its full reading time, and "
	                "the advisor does not say it again. No music plays: the script's music counts as playing, as in "
	                "the game, until a script stops it or starts other music.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::GiantApe, {-20.0f, 10.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {10.0f, -10.0f}}},
	    .commands = {Command {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f},
	                 Command {.kind = Kind::AttachObjectScriptSound,
	                          .delaySeconds = 1.0f,
	                          .value = static_cast<size_t>(audio::SoundId::G_VillageBell)},
	                 Command {.kind = Kind::StartScriptMusic,
	                          .delaySeconds = 0.5f,
	                          .value = static_cast<size_t>(audio::MusicType::ScriptCreatureChosen)},
	                 Command {.kind = Kind::RunText, .delaySeconds = 1.0f, .value = 1009},
	                 Command {.kind = Kind::SaveGame, .delaySeconds = 1.5f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	// A save while a vortex out of the land writes the crossing file: the load puts the file back as it was at the save
	all.push_back({
	    .id = "saves.vortex_crossing_back",
	    .name = "Save and load: a vortex that leads away",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens among six oaks. It takes the oaks one by one and "
	                   "writes each into the crossing file. 4 seconds after the start the game saves into slot 0. 6 "
	                   "seconds after the save the game loads slot 0.",
	    .expected = "At the save the vortex has taken the six oaks. After the load the vortex is open at the same "
	                "place, and the crossing file (vortex.txt in the player's saved games) holds the six lines that "
	                "it held at the save. An oak moved into the vortex after the load adds its line after them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -30.0f}, {30.0f, 30.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {6.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {-8.0f, 3.0f}},
	                {.type = TreeInfo::Oak, .offset = {2.0f, -11.0f}},
	                {.type = TreeInfo::Oak, .offset = {14.0f, 9.0f}},
	                {.type = TreeInfo::Oak, .offset = {-15.0f, -12.0f}},
	                {.type = TreeInfo::Oak, .offset = {20.0f, -18.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {0.0f, 0.0f}, .delaySeconds = 0.5f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 4.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 6.0f, .value = 0}},
	});

	// Both kinds of land vortex, open at a save: after the load each looks as it did before the save
	all.push_back({
	    .id = "saves.vortex_drawn_after_load",
	    .name = "Save and load: two open vortices",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens on the left. A vortex that leads into the land "
	                   "opens on the right. 6 seconds after the start the game saves into slot 0. 4 seconds after the "
	                   "save the game loads slot 0.",
	    .expected = "Before and after the load, each vortex shows its swirl in the hole in the land, with the stars "
	                "that turn over it. The land under each vortex stays level. After the load nothing of the land "
	                "or the sea shows in the holes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -30.0f}, {60.0f, 30.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {-35.0f, 0.0f}, .delaySeconds = 0.5f},
	                 {.type = VortexType::Out, .offset = {35.0f, 0.0f}, .delaySeconds = 0.5f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 6.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 4.0f, .value = 0}},
	});

	// A vortex levels the pond and then goes. The saved game keeps the levelled land, and the load gives it back.
	all.push_back({
	    .id = "saves.levelled_land_back",
	    .name = "Save and load: the land that a vortex levelled",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens over the pond north-west of the middle. It levels "
	                   "the pond and its banks. 4 seconds later it starts to fade out, and 7 seconds after that it is "
	                   "gone. 14 seconds after the start the game saves into slot 0. 4 seconds after the save the game "
	                   "loads slot 0.",
	    .expected = "When the vortex has gone, a level square of land is where the pond and its banks were. After the "
	                "load the square is level, as it was at the save: the pond and its banks do not come back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {flat_land::k_PondCentre - flat_land::k_MapMiddle - glm::vec2(70.0f),
	                            flat_land::k_PondCentre - flat_land::k_MapMiddle + glm::vec2(70.0f)}},
	    .vortices = {{.type = VortexType::Out,
	                  .offset = flat_land::k_PondCentre - flat_land::k_MapMiddle,
	                  .delaySeconds = 0.5f,
	                  .fadeOutAfterSeconds = 4.0f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 14.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 4.0f, .value = 0}},
	});

	all.push_back({
	    .id = "frontend.continue_with_menu",
	    .name = "Start-up: continue from the save on exit",
	    .facet = Facet::Story,
	    .description = "Two oaks stand on the testbed. 2 seconds after the start the game saves into the slot of the "
	                   "save on exit (slot 20). 1 second later the game continues as a plain start of the program "
	                   "does.",
	    .expected = "The tips screen shows while slot 20 loads. Then the menu fades in over the testbed as it was "
	                "saved, with the profile's name above its buttons, and the game is paused under it. The hand is "
	                "hidden and the menu's pointer shows. Continue Game closes the menu, and the game runs again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {6.0f, 0.0f}}, {.type = TreeInfo::Oak, .offset = {-8.0f, 3.0f}}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 2.0f, .value = 20},
	                 Command {.kind = Kind::ContinueGame, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "frontend.logo_effect",
	    .name = "Start-up: the logo effect",
	    .facet = Facet::Story,
	    .description = "One second after the start the logo effect of the first start plays, with its sound, as before "
	                   "the first game of the program.",
	    .expected = "The screen fades from black to a sky sphere. Dots float at random depths and the camera turns round "
	                "them. The camera comes back, and the dots make the Lionhead logo, with TM at its top right. The "
	                "dots leave trails. Then the camera flies through the logo and the screen goes black after about 33 "
	                "seconds. A key ends it at once. On a processor of family 15 with SSE2 (all recent AMD processors) "
	                "the sponsor's logo shows at the bottom left.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .commands = {Command {.kind = Kind::PlayLogoEffect, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "mind.hand_free_pick_up",
	    .name = "Put down a thing before a pick-up",
	    .facet = Facet::Mind,
	    .description = "A content tiger. A script tells it to hold the first rock. 8 seconds later a script tells it to "
	                   "examine the second rock. 20 seconds later a script tells it to hold the second rock, and 8 "
	                   "seconds after that to examine the second rock again.",
	    .expected = "For the first order to examine, the tiger puts the first rock down (it does not toss it). Then it "
	                "picks up the second rock and examines it. For the second order to examine, it does not pick the "
	                "rock up again: it examines the rock that it holds at once.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {15.0f, -15.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {-15.0f, -15.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::DoAction, .delaySeconds = 8.0f, .object = 1, .action = "ExamineByPickingUp"},
	                 {.kind = Kind::DoAction, .delaySeconds = 20.0f, .object = 1, .action = "HoldObject"},
	                 {.kind = Kind::DoAction, .delaySeconds = 8.0f, .object = 1, .action = "ExamineByPickingUp"}},
	});

	// An angry tiger with two tigers of another player about it: its friend, and one that a fight knocked out
	auto furious = Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "angry tiger");
	furious.desires = OnlyDesire(Desire::Anger);
	const auto otherTiger = [](glm::vec2 offset, float facing, std::string_view label) {
		auto tiger = Content(CreatureType::Tiger, offset, facing, label);
		tiger.owner = PlayerNames::PLAYER_TWO;
		return tiger;
	};
	all.push_back({
	    .id = "mind.friends_and_dead",
	    .name = "Not angry with friends or dead creatures",
	    .facet = Facet::Mind,
	    .description = "An angry tiger stands between two tigers of another player. The tiger to its left becomes its "
	                   "friend, as a script makes friends. A fight knocks out the tiger to its right.",
	    .expected = "The angry tiger makes no plan of anger about its friend, and none about the tiger that lies "
	                "knocked out. Both tigers have no worth for its anger. The inspector shows the friends of each "
	                "tiger and the knock-out. When the knocked out tiger is up again, the angry tiger can plan a fight "
	                "with it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {furious, otherTiger({-40.0f, 0.0f}, 90.0f, "friend"), otherTiger({40.0f, 0.0f}, 270.0f, "knocked out")},
	    // The friends and the knock-out are made before the angry tiger can plan anything
	    .commands = {{.kind = Kind::MakeFriends, .creature = 0, .delaySeconds = 0.0f, .value = 1},
	                 Fight(Kind::KnockOut, 2, 0, 0.0f)},
	});
	all.push_back({
	    .id = "combat.anger_picks_foe",
	    .name = "An angry creature fights neither a friend nor a dead creature",
	    .facet = Facet::Combat,
	    .description = "An angry tiger stands between two tigers of another player, as in mind.friends_and_dead: its "
	                   "friend to its left and a knocked out tiger to its right. A third tiger of that player stands "
	                   "further away, in front of it.",
	    .expected = "The angry tiger does not fight its friend or the knocked out tiger. Its anger makes a plan to fight "
	                "the tiger in front of it, and the two duel. The fight transcript gives the reason anger.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {furious, otherTiger({-40.0f, 0.0f}, 90.0f, "friend"), otherTiger({40.0f, 0.0f}, 270.0f, "knocked out"),
	                  otherTiger({0.0f, 70.0f}, 180.0f, "foe")},
	    // The friends and the knock-out are made before the angry tiger can plan anything
	    .commands = {{.kind = Kind::MakeFriends, .creature = 0, .delaySeconds = 0.0f, .value = 1},
	                 Fight(Kind::KnockOut, 2, 0, 0.0f)},
	});

	// A tiger of the player and a lion of no player, as the guide is on the first land
	const auto guideLion = [](glm::vec2 offset, float facing) {
		auto lion = Content(CreatureType::Lion, offset, facing, "guide");
		lion.owner = PlayerNames::NEUTRAL;
		return lion;
	};
	auto befriender = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "befriends");
	befriender.desires = OnlyDesire(Desire::BeFriends);
	all.push_back({
	    .id = "friends.wave_with_guide",
	    .name = "Make friends with the guide",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. As a script of the first land "
	                   "does, both want only to be friends.",
	    .expected = "The tiger smiles, howls or waves at the lion. To wave, it asks the lion to play first; the lion agrees "
	                "when it wants to be friends with the tiger more than it fears it or is angry with it. Then the lion "
	                "stands still in its mind, turns to the tiger and waves back as the tiger waves, from where they "
	                "stand. The inspector shows the partners while they wave.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, guideLion({25.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	// The lion of no player asks the player's tiger, which wants only to sleep, as the guide does after the fight lesson
	auto askingLion = guideLion({25.0f, 0.0f}, 270.0f);
	askingLion.desires = OnlyDesire(Desire::BeFriends);
	auto sleepyTiger = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "refuses");
	sleepyTiger.desires = OnlyDesire(Desire::Tiredness);
	all.push_back({
	    .id = "friends.refused",
	    .name = "Another creature refuses to be friends",
	    .facet = Facet::Mind,
	    .description = "A lion of no player and your tiger stand 50 apart and face each other. The lion wants only to be "
	                   "friends. Your tiger wants only to sleep. No script has made a creature do an action.",
	    .expected = "The lion asks your tiger to play or to be friends. Your tiger wants neither more than it fears the "
	                "lion or is angry with it, so it refuses. The lion stops that action, and your tiger tells you that "
	                "it refused to play or to be friends. The log says that the lion stops because the tiger refused. "
	                "When your tiger looks the lion over, its wish to be friends can grow. Then it can ask the lion, which "
	                "agrees.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {sleepyTiger, askingLion},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::Tiredness),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	all.push_back({
	    .id = "friends.side_by_side",
	    .name = "Do things side by side with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. Both want only to be friends.",
	    .expected = "Some of the time the tiger asks the lion to be its friend and does a joint action with it. To be happy "
	                "together, both go to places side by side, the tiger waits until the lion stops, both turn to face "
	                "each other and both play the happy action. To sit together, both go to the two sides of a clear "
	                "place, turn to face each other and sit for 10 to 15 seconds; then the tiger plays the happy action. "
	                "The inspector shows the partners while they do it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, guideLion({25.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	// One joint action with the lion, which a script tells the tiger to do. The lion first smiles at the tiger by a
	// script too, so that a script controls both and the lion agrees.
	struct JointScene
	{
		std::string_view id;
		std::string_view name;
		std::string_view action;
		bool town {false};
		std::optional<Desire> desire;
		std::string_view expected;
	};
	for (const auto& scene : {
	         JointScene {.id = "friends.happy_together",
	                     .name = "Be happy with a friend",
	                     .action = "BeHappyWithFriend",
	                     .expected = "Both go to places side by side and the tiger waits until the lion stops. Both turn "
	                                 "to face each other and play the happy action."},
	         JointScene {.id = "friends.sit_together",
	                     .name = "Sit with a friend",
	                     .action = "SitWithFriend",
	                     .expected = "Both go to the two sides of a clear place, turn to face each other and sit for 10 "
	                                 "to 15 seconds. Then the tiger plays the happy action."},
	         JointScene {.id = "friends.sleep_together",
	                     .name = "Sleep with a friend",
	                     .action = "SleepWithFriend",
	                     .desire = Desire::Tiredness,
	                     .expected = "Both go to the two sides of a clear place, turn to face each other and sleep for 17 "
	                                 "seconds. Then the tiger wakes with a confused look."},
	         JointScene {.id = "friends.drink_together",
	                     .name = "Drink with a friend",
	                     .action = "DrinkWithFriend",
	                     .desire = Desire::Water,
	                     .expected = "Both go to the nearest coast, side by side, turn to face the sea and drink. Then "
	                                 "the tiger turns to the lion with a smile."},
	         JointScene {.id = "friends.poo_together",
	                     .name = "Have a poo with a friend",
	                     .action = "PooWithFriend",
	                     .desire = Desire::Poo,
	                     .expected = "Both go to places side by side, squat for four seconds and have a poo. Then the "
	                                 "tiger turns to the lion with a smile and plays the happy action."},
	         JointScene {.id = "friends.help_town_together",
	                     .name = "Help a town with a friend",
	                     .action = "HelpTownWithFriend",
	                     .town = true,
	                     .expected = "The tiger turns to the lion, plays the happy action and points at the town. Both go "
	                                 "near the storage pit and turn to face each other. Then the first of the two to take "
	                                 "up the wish to help the town does so, and the other one stops."},
	         JointScene {.id = "friends.attack_town_together",
	                     .name = "Attack a town with a friend",
	                     .action = "AttackWithFriend",
	                     .town = true,
	                     .desire = Desire::Anger,
	                     .expected = "The script's action fails: a creature that has learnt nothing finds the town more "
	                                 "useful to its compassion than to its anger, so it cannot attack the town. A creature "
	                                 "taught to be angry with towns attacks it as helping a town with a friend."},
	     })
	{
		auto tiger = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "asks");
		if (scene.desire.has_value())
		{
			tiger.desires = {{.desire = *scene.desire, .fraction = 0.6f}};
			tiger.hold = true;
		}
		Scenario scenario {
		    .id = scene.id,
		    .name = scene.name,
		    .facet = Facet::Mind,
		    .description = "A tiger and a lion of no player stand 50 apart and face each other. A script tells the lion "
		                   "to smile at the tiger, then tells the tiger to do a joint action with the lion.",
		    .expected = scene.expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview},
		    .creatures = {tiger, guideLion({25.0f, 0.0f}, 270.0f)},
		    .commands = {{.kind = Kind::DoAction,
		                  .creature = 1,
		                  .delaySeconds = 0.5f,
		                  .action = "SmileAtFriend",
		                  .atCreature = size_t {0}},
		                 {.kind = Kind::DoAction,
		                  .creature = 0,
		                  .delaySeconds = 4.0f,
		                  .action = scene.action,
		                  .atCreature = size_t {1}}},
		};
		if (scene.town)
		{
			scenario.objects = {
			    {.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -70.0f}, .storedFood = 2000, .storedWood = 0}};
		}
		all.push_back(std::move(scenario));
	}
	auto missing = Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f, "misses its friend");
	missing.desires = OnlyDesire(Desire::MissFriend);
	all.push_back({
	    .id = "friends.miss_friend",
	    .name = "Miss a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger that wants only to see a friend it misses stands 120 from a lion of no player.",
	    .expected = "The tiger pines for the lion: with a sad face it turns to it and points at it, then plays its sad "
	                "action. Or it looks for the lion: it points at it and plays its sad action, then walks near it, to "
	                "three times its own height.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {missing, guideLion({60.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::MissFriend),
	                  .amount = 600.0f}},
	});

	all.push_back({
	    .id = "mind.watch_skill",
	    .name = "Learn a skill by watching",
	    .facet = Facet::Mind,
	    .description = "A grown-up lion watches villagers build: it sees the skill now, again 4 seconds later and again "
	                   "4 seconds after that.",
	    .expected = "Once more than the skill takes to learn has gone by since it first saw it (7 seconds for building, by "
	                "the game's table) it "
	                "knows it: its thoughts say it has learnt to build, and the Mind panel marks the skill known.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Lion, .needs = Content()}},
	    .commands = {{.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 1.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill}},
	});

	// A content tiger: it only lies down to sleep when told to, and gets up again soon after, being rested
	const auto rested = Content(CreatureType::Tiger, {0.0f, 0.0f});
	all.push_back({
	    .id = "mind.replan_mid_action",
	    .name = "Changing its plan in the middle of an action",
	    .facet = Facet::Mind,
	    .description = "A tiger starts showing it is hungry (an action played once); half a second in it is told to "
	                   "sleep, which changes its plan.",
	    .expected = "It shows it is hungry to the end of the animation, then lies down to sleep. It doesn't snap out of "
	                "the action the moment its plan changes.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {rested},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Hungry, 1.0f, true), Act(Kind::Sleep, 0, 0.5f, false)},
	});

	all.push_back({
	    .id = "mind.replan_from_sitting",
	    .name = "Changing its plan while sitting",
	    .facet = Facet::Mind,
	    .description = "A tiger sits down; four seconds later, sitting, it is told to sleep, which changes its plan.",
	    .expected = "It gets up through the end of sitting, then lies down to sleep. It doesn't jump from sitting "
	                "straight into lying down.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {rested},
	    .commands = {Act(Kind::SitDown, 0, 1.0f, true), Act(Kind::Sleep, 0, 4.0f, false)},
	});

	// Worn out, held so, but not yet wanting to sleep when the hand takes hold
	auto worn = Content(CreatureType::Tiger, {0.0f, 0.0f});
	worn.needs.exhaustion = 0.75f;
	worn.desires = {{.desire = Desire::Tiredness, .fraction = 0.0f}};
	all.push_back({
	    .id = "mind.held_while_tired",
	    .name = "Tired while held by the hand",
	    .facet = Facet::Mind,
	    .description = "The hand takes hold of a worn out tiger by stroking it; once held, its desire to sleep is set as "
	                   "strong as it gets. The hand strokes it twice more over twelve seconds, then lets go.",
	    .expected = "While the hand holds it, its plan never changes and it never lies down to sleep: a held creature's "
	                "mind stands still, choosing nothing (the Mind panel's activity stays as it was when taken hold of). "
	                "Once let go it stops what it was doing and chooses afresh.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {worn},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), setDesire(Desire::Tiredness, 1.0f, 0.5f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 6.0f), Stroke(0, creature_feedback::BodyPart::Head, 6.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	});

	// The player's tiger looks north over two small forests and two flocks of birds; its player's temple stands to
	// the west and another god's far off to the north east, out of its sight
	all.push_back({
	    .id = "mind.object_beliefs",
	    .name = "Beliefs about the temples, flocks and forests it has met",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, stands looking north at two forests of three trees and two "
	                   "flocks of doves. Its player's temple is to the west; another player's is far to the north east. "
	                   "After six seconds the Creature Cave opens on its Mind scroll.",
	    .expected = "Its eyes sweep the cells about it in a couple of seconds. The Mind scroll then says it knows of 1 "
	                "temple (its player's, the nearest; the far one it never sees), 2 flocks and 2 forests.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours")},
	    .objects = {{.type = TreeInfo::Oak, .offset = {30.0f, 25.0f}, .forest = 1},
	                {.type = TreeInfo::Oak, .offset = {38.0f, 32.0f}, .forest = 1},
	                {.type = TreeInfo::Oak, .offset = {26.0f, 36.0f}, .forest = 1},
	                {.type = TreeInfo::Conifer, .offset = {-30.0f, 25.0f}, .forest = 2},
	                {.type = TreeInfo::Conifer, .offset = {-38.0f, 32.0f}, .forest = 2},
	                {.type = TreeInfo::Conifer, .offset = {-26.0f, 36.0f}, .forest = 2}},
	    .birdFlocks = {{.kind = AnimalInfo::Dove, .offset = {-10.0f, 30.0f}, .count = 5, .reach = 10.0f},
	                   {.kind = AnimalInfo::Dove, .offset = {10.0f, 40.0f}, .count = 5, .reach = 10.0f}},
	    .temples = {{.offset = {-90.0f, 0.0f}, .angle = 0.0f},
	                {.offset = {300.0f, 300.0f}, .owner = PlayerNames::PLAYER_TWO, .angle = 0.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 5.5f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_cave::Page::Mind)}},
	});
	// The player's tiger looks north at another god's ogre, a hut and a villager, to read its beliefs in the inspector
	auto rival = Posed(CreatureType::Ogre, {0.0f, 25.0f}, 0.0f, "rival");
	rival.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "mind.beliefs_rest",
	    .name = "What it believes about another creature, a building and a villager",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, looks north at another player's ogre, a hut and a villager. "
	                   "Read its beliefs with the inspector query creature.beliefs.",
	    .expected = "After its eyes sweep about it, it has a belief about each of them. The ogre's belief shows the desire "
	                "of the ogre's last action, how many miracles the ogre knows, and that the ogre carries nothing (29). "
	                "The hut shows high life and that it is not being built (1). The villager shows high life.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours"), rival},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-20.0f, 20.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {15.0f, 15.0f}}},
	});
	// The player's tiger looks north at a villager and a hut, which both catch fire after three seconds
	all.push_back({
	    .id = "mind.belief_attributes",
	    .name = "A belief keeps what the creature took in, and a building keeps its first sight",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, looks north at a villager and a hut. After three seconds both "
	                   "catch fire. Read its beliefs with the inspector query creature.beliefs.",
	    .expected = "Before the fire, both beliefs show OnFire 1 (not on fire). After the fire, the villager's belief shows "
	                "OnFire 0 (on fire) once the eyes sweep over it again. The hut's belief still shows OnFire 1: a belief "
	                "about a building keeps the attributes of the first sight.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours")},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {15.0f, 15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-20.0f, 20.0f}}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 3.0f, .value = 1, .object = 0},
	                 {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 0.0f, .value = 1, .object = 1}},
	});
	// A hungry tiger, its mind running, looks north at a villager some way off; a nearer one stands behind it, out of sight
	auto peckish = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours");
	peckish.needs = {.energy = 0.3f};
	peckish.hold = false;
	peckish.pauseMind = false;
	all.push_back({
	    .id = "mind.goal_it_knows",
	    .name = "Choosing only among the things it has seen",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger looks north at a villager 45 m away. Another stands 14 m behind it, where it can't "
	                   "see. Its mind runs; after two seconds it is made as hungry as can be.",
	    .expected = "It goes for the villager ahead, the one it has seen, though the other is nearer: a creature weighs only "
	                "the things it knows of, each by what it has learnt of acting on such things and by how near it is.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {peckish},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 25.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -34.0f}}},
	    .commands = {setDesire(Desire::Hunger, 1.0f, 2.0f)},
	});
	// A tiger with its mind on, alone, that wants to show its state and is very hungry
	auto communicative = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 90.0f, "yours");
	communicative.hold = false;
	communicative.pauseMind = false;
	all.push_back({
	    .id = "mind.communicate_state",
	    .name = "Showing its state to the player",
	    .facet = Facet::Mind,
	    .description = "A tiger stands alone with its mind on, side on to the camera. At once it wants to show its "
	                   "state as much as possible, and it is very hungry.",
	    .expected = "It turns to face the camera, with its head on the camera, then plays its happy animation once: its mind "
	                "is less than ten seconds old, which counts as feedback from the player. Then it does not want to show "
	                "its state for a minute. After that minute it shows its state again with its hungry animation. When no "
	                "plan is ready, it stands and does nothing.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {communicative},
	    // The desires are set as soon as it has them, so that its first communication shows them
	    .commands = {setDesire(Desire::ManifestState, 1.0f, 0.1f), setDesire(Desire::Hunger, 1.0f, 0.1f)},
	});
	// A tiger of another player looks north at a village of the player: a hut and three of its villagers
	auto stranger = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "another player's");
	stranger.owner = PlayerNames::PLAYER_TWO;
	stranger.hold = false;
	stranger.pauseMind = false;
	const std::vector<ObjectSetup> village {
	    {.type = AbodeInfo::CelticHut, .offset = {0.0f, 40.0f}},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 25.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 22.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {8.0f, 25.0f}, .joinTown = true},
	};
	// A rock by the tiger, which it can hurl
	auto angryVillage = village;
	angryVillage.push_back({.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -14.0f}});
	all.push_back({
	    .id = "mind.angry_at_village",
	    .name = "Anger at a village acts on one of its people",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a village of the player: a hut and three villagers. A rock "
	                   "lies by it. Its mind runs; after two seconds it is made as angry as can be.",
	    .expected = "Its anger is about the village (the Mind panel's plan shows the town as what the plan is about), and it "
	                "acts on one of the villagers that it has seen, not on the town itself.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = angryVillage,
	    .commands = {setDesire(Desire::Anger, 1.0f, 2.0f)},
	});
	all.push_back({
	    .id = "mind.angry_at_temple",
	    .name = "Anger at a temple acts on its heart",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a temple of the player. A rock lies by the tiger. Its mind "
	                   "runs; after two seconds it is made as angry as can be.",
	    .expected = "Its anger is about the temple, and it acts on the heart of the temple: the temple itself, as the "
	                "temple here is one thing. It does not act on a worship site.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -14.0f}}},
	    .temples = {{.offset = {0.0f, 70.0f}}},
	    .commands = {setDesire(Desire::Anger, 1.0f, 2.0f)},
	});
	all.push_back({
	    .id = "mind.impress_village",
	    .name = "To impress a village it acts on one of its people",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a village of the player: a hut and three villagers. Its "
	                   "mind runs; after two seconds its desire to impress is made as strong as can be.",
	    .expected = "Its desire to impress is about the village, and it acts on one of the villagers that it has seen and "
	                "that does not dance, never on the hut.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = village,
	    .commands = {setDesire(Desire::Impress, 1.0f, 2.0f)},
	});
	// The player's tiger looks north at a village of the player, on the learning leash
	auto ownTiger = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "player's");
	ownTiger.hold = false;
	ownTiger.pauseMind = false;
	all.push_back({
	    .id = "leash.shown_hut",
	    .name = "A hut shown on the leash",
	    .facet = Facet::Leash,
	    .description = "The player's tiger on the learning leash looks north at a village of the player: a hut and three "
	                   "villagers. Its compassion is made as strong as can be. Then the Action button of the leash "
	                   "orders it onto the hut.",
	    .expected = "First it tries the desire it wants most for the village of the hut: compassion. When compassion "
	                "has no plan, it tries the next desire, up to 40 times. The log shows the desire that it uses and "
	                "how many desires it tried.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {ownTiger},
	    .objects = village,
	    .commands = {Leash(0, LeashType::Rope, 1.0f), setDesire(Desire::Compassion, 1.0f, 1.0f),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 0.5f, false), Act(Kind::TakeOffLeash, 0, 20.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "leash.aggression_lesson_saved",
	    .name = "A hut shown on the aggression leash, then saved",
	    .facet = Facet::Leash,
	    .description = "The player's tiger on the aggression leash looks north at a village of the player: a hut and three "
	                   "villagers. The Action button of the leash orders it onto the hut, then the leash comes off. Save "
	                   "the game and read the creature's mind file.",
	    .expected = "The tiger learns anger towards the village of the hut and less compassion for it. In the saved mind "
	                "file, the activity trees of those desires (and of the desires that depend on them) hold a new example "
	                "of lesson kind 1: the village, with the place of the village, action 0 and the desire of its tree.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {ownTiger},
	    .objects = village,
	    .commands = {Leash(0, LeashType::Evil, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 0.5f, false),
	                 Act(Kind::TakeOffLeash, 0, 5.0f)},
	});
	// The player's evil, big tiger swaps minds with a small, neutral sheep that belongs to nobody, as The Lost Flock's
	// sheep offers; then back again
	auto swapper = Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "player's tiger");
	swapper.alignment = -0.9f;
	swapper.size = 1.5f;
	swapper.tattoos.push_back({.design = 3, .site = 0, .colour = {220, 40, 40}});
	swapper.tattoos.push_back({.design = 7, .site = 2, .colour = {40, 90, 230}});
	auto sheep = Content(CreatureType::Sheep, {15.0f, 0.0f}, 270.0f, "sheep");
	sheep.owner = PlayerNames::NEUTRAL;
	sheep.alignment = 0.0f;
	sheep.size = 0.6f;
	all.push_back({
	    .id = "mind.creature_swap",
	    .name = "Swapping creatures",
	    .facet = Facet::Mind,
	    .description = "The player's evil tiger (size 1.5, two tattoos) and a neutral sheep of nobody's (size 0.6) face "
	                   "each other. Three seconds in, a script swaps their minds, as The Lost Flock's sheep lets the "
	                   "player; twenty seconds later it swaps them back.",
	    .expected = "At the swap both play their creation action and the beam starts; the tattoos move to the sheep at once "
	                "while each keeps its name. Over about ten seconds the sheep grows to the tiger's size and turns "
	                "evil, while the tiger shrinks to the sheep's size and turns neutral. The sheep is now the player's "
	                "creature, which the leash and the spawner show. Swapped back, it all goes the other way.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {swapper, sheep},
	    .commands = {{.kind = Kind::SwapMinds, .creature = 0, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::SwapMinds, .creature = 1, .delaySeconds = 20.0f, .value = 0}},
	});
}

void AddParticles(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "particles.smoke_and_fire",
	    .name = "Smoke, steam and a bonfire",
	    .facet = Facet::Particles,
	    .description = "The spot visuals' smoke, steam, evil smoke and a bonfire, side by side on the plain.",
	    .expected = "Grey smoke and white steam puffs rise, spin and fade; the evil smoke is dark; the bonfire's flames "
	                "flicker in its additive sprites with smoke rising above them.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-30.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Steam, .offset = {-10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::EvilSmoke, .offset = {10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Bonfire, .offset = {30.0f, 0.0f}, .magnitude = 5.0f}},
	});

	all.push_back({
	    .id = "particles.sparkles",
	    .name = "Sparkles and magic",
	    .facet = Facet::Particles,
	    .description = "Short effects started again every few seconds: the failed cast, the hand gripping the land, a "
	                   "magic object made, an object appearing in the second player's colour, and a forest made.",
	    .expected = "Each plays out in sprites from the sprite sheets and starts again: the cross of the failed cast, "
	                "a ring of sparks, a spinning cloud of sparks, sparks fountaining up in green, and falling leaves.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .particles =
	        {{.type = ParticleType::SpellFail, .offset = {-30.0f, 0.0f}, .height = 5.0f, .restartSeconds = 4.0f},
	         {.type = ParticleType::GripLandscape, .offset = {-15.0f, 0.0f}, .restartSeconds = 3.0f},
	         {.type = ParticleType::MagicObjectCreated,
	          .offset = {0.0f, 0.0f},
	          .height = 3.0f,
	          .magnitude = 3.0f,
	          .restartSeconds = 5.0f},
	         {.type = ParticleType::ObjectAppear,
	          .offset = {15.0f, 0.0f},
	          .magnitude = 3.0f,
	          .player = 1,
	          .restartSeconds = 4.0f},
	         {.type = ParticleType::ForestCreated, .offset = {30.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.models",
	    .name = "Models among the sparks",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles are models: the vortex rising where a magic object is made, the glowing "
	                   "cone of the beam marking a place, the paper streamers of the ticker tape and the fireball's flash.",
	    .expected = "A spinning additive vortex model rises and fades; a glowing cone turns to face the camera wherever it "
	                "looks from; coloured streamers fall from a disk; the flash's model plays through its texture's frames.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_MagicObjectCreated2", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 5.0f},
	                  {.file = "SF_SeeThisBeam", .offset = {-10.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_TickerTape", .offset = {10.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Flash", .offset = {30.0f, 0.0f}, .height = 3.0f, .magnitude = 2.0f, .restartSeconds = 3.0f}},
	});

	// The beam that marks a place for the player, as the first land marks where its vortex opens
	all.push_back({
	    .id = "particles.see_this_beam",
	    .name = "The beam that marks a place",
	    .facet = Facet::Particles,
	    .description = "The yellow beam that a script puts on a place to show it to the player, as the first land shows "
	                   "where its vortex will open. It starts again every ten seconds.",
	    .expected = "A yellow cone of light stands on the land and turns to face the camera. Its brightness beats with "
	                "the scrolls' highlights, about once every 1.3 seconds, from a dim glow to full and back.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, -6.0f}, {6.0f, 6.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SeeThisBeam", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.tumble",
	    .name = "Tumbling streamers and wood",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles tumble as they move: the paper streamers of the ticker tape and the planks of "
	                   "wood that a store gives out. Each turns about its forward axis when it moves more east to west, else "
	                   "about its east to west axis.",
	    .expected = "Streamers fall from a disk and planks fly out, each turning as it goes, faster the faster it moves.",
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, 0.0f}, {12.0f, 0.0f}}, .distance = 0.25f},
	    .particles = {{.file = "SF_TickerTape", .offset = {-8.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Wood", .offset = {8.0f, 0.0f}, .height = 2.0f, .magnitude = 1.0f, .restartSeconds = 4.0f}},
	});

	all.push_back({
	    .id = "particles.light_maps",
	    .name = "Light on the land",
	    .facet = Facet::Particles,
	    .description = "Effects that light the ground under them, as fire and lightning do, at night so the light shows: "
	                   "the ground effect's ring and the light a landscape vortex leaves, each many cells across.",
	    .expected = "A ring of light spreads over the ground and fades, again and again; a steady pool of coloured light "
	                "lies on the ground beside it, lighting what stands there too.",
	    .environment = {.hour = 23.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 0.0f}, {120.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_GroundEffect", .offset = {-70.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 4.0f},
	                  {.file = "SF_LandscapeVortexLightMap", .offset = {70.0f, 0.0f}}},
	});

	// The singing stones' heal as the funeral march's raising starts it: the script makes it 13.5 times its size, in the
	// colour of this computer's player
	all.push_back({
	    .id = "particles.singing_stones_heal_rings",
	    .name = "The rings of light of the singing stones' heal",
	    .facet = Facet::Particles,
	    .description = "The singing stones' heal effect at 13.5 times its size, in the first player's colour, at night. "
	                   "Its rule stands three rings of light on the land round the place where the effect starts, each "
	                   "about 15 m across from the centre, under the glow that the effect's light map puts on the land.",
	    .expected = "Three rings of light stand one on the other round the light on the land, 10 high, rising and "
	                "falling in waves that roll round them, their stars sliding up. They are a dim red: the player's red "
	                "times the alpha of 30 of the effect's points. They follow the land and stay where they were made.",
	    .environment = {.hour = 23.0f, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SingingStonesHeal", .offset = {0.0f, 0.0f}, .magnitude = 13.5f, .player = 0}},
	});

	all.push_back({
	    .id = "particles.mist_shadows",
	    .name = "Shadows of a storm's mist",
	    .facet = Facet::Particles,
	    .description = "The lightning storm effect over the land at noon. Each puff of its cloud stamps a grey light map on "
	                   "the land that darkens the land's luminosity, as the sky's clouds do, for the frame after it is drawn.",
	    .expected = "Dark patches move on the ground under the storm's clouds and fade with them. The ground goes darker, "
	                "not a different colour, and the things that stand on it go darker with it.",
	    .environment = {.hour = 12.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, -120.0f}, {120.0f, 120.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_LightningStorm", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 20.0f}},
	});

	all.push_back({
	    .id = "particles.vortex_and_flies_sounds",
	    .name = "The landscape vortex's and the flies' sounds",
	    .facet = Facet::Particles,
	    .description = "The landscape vortex as it opens and as it closes, and a swarm of flies, side by side and close up, "
	                   "every eight seconds. Their sounds are named in the effect files by sound actions the game's header "
	                   "numbers by counting on from the one before.",
	    .expected = "The vortices play the vortex's hum and the flies their buzz as they start; the debug log names an "
	                "emitter for each.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 30.0f}, {15.0f, 50.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_LandscapeVortexInBefore", .offset = {-8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_LandscapeVortexOutAfter", .offset = {8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.flies_face_their_way",
	    .name = "Flies turned to the way they fly",
	    .facet = Facet::Particles,
	    .description = "A swarm of flies close up, every eight seconds. Each fly is a sprite that the flock rule turns on "
	                   "the screen.",
	    .expected = "Each fly's picture turns to point the way that the fly moves across the screen, as the game turns "
	                "it. A fly that flies to the right on the screen points to the right.",
	    .framing = {.shot = Shot::Overview, .include = {{-3.0f, 33.0f}, {3.0f, 39.0f}}, .distance = 0.15f},
	    .particles = {{.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.ribbon_slide",
	    .name = "A ribbon's corners and its texture sliding along it",
	    .facet = Facet::Particles,
	    .description = "The simple beam, three ribbons from a point ten metres up to a creature twenty metres away, "
	                   "started again every six seconds and seen close from the side.",
	    .expected = "Each beam is a ribbon that faces the camera from every side, its corners meeting half way where two "
	                "segments join. Its texture slides along it at twice the speed of its data, as the game adds the "
	                "slide to the texture twice. The slide moves on only by the frames drawn, and stops while the game "
	                "pauses.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {25.0f, 5.0f}}, .distance = 0.35f},
	    .creatures = {{.offset = {20.0f, 0.0f}, .pauseMind = true}},
	    .particles = {{.file = "SF_SimpleBeam",
	                   .targetsCreatures = true,
	                   .offset = {-10.0f, 0.0f},
	                   .height = 10.0f,
	                   .magnitude = 1.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.mist",
	    .name = "Mist over the holders",
	    .facet = Facet::Particles,
	    .description = "The water and storm miracles' mists, as they sit over a holder, and the water miracle in the hand.",
	    .expected = "Puffs of mist, shaped wider than tall, turn to face the camera, shrink seen edge on and grow seen from "
	                "above, and play through their frames of the smoke texture.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.file = "SF_WaterOnHolder", .offset = {-15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_LightningStormOnHolder", .offset = {0.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_WaterInHand", .offset = {15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f}},
	});

	all.push_back({
	    .id = "particles.symbols_and_heal",
	    .name = "Player symbols and the heal chakra",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain throwing up the casting player's symbol in each player's colour, and the "
	                   "heal miracle's chakra over two creatures.",
	    .expected = "Symbols between two glows of the player's colour, one turning, arc up out of each fountain and fade; "
	                "a chakra appears over each creature and follows it, a burst of sparks under it rising and fading.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .creatures = {{.offset = {15.0f, 0.0f}}, {.offset = {30.0f, 5.0f}}},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_HealChakra",
	                   .targetsCreatures = true,
	                   .offset = {22.0f, 0.0f},
	                   .magnitude = 2.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.profile_symbol",
	    .name = "The symbol of the player's profile in the world",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain of the first player and of the second player, side by side. The world "
	                   "shows the tenth symbol for the first player, as when the player's profile has that symbol.",
	    .expected = "The fountain on the left throws up the tenth symbol of the players' symbols page in the first "
	                "player's colour. The fountain on the right throws up the second player's symbol as the file of the "
	                "players' symbols has it.",
	    .environment = {.profileSymbol = 9u},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-10.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {10.0f, 0.0f}, .magnitude = 2.0f, .player = 1}},
	});

	all.push_back({
	    .id = "particles.gods_symbols",
	    .name = "The computer gods' pictures in the world",
	    .facet = Facet::Particles,
	    .description = "As on the second land, a land script switches on the computer minds of the second and third "
	                   "players. The sheet of the players' symbols is then made again, as at the start of a land. The "
	                   "player icon fountains of the first three players stand side by side.",
	    .expected = "The fountain on the left throws up the symbol of the player's profile. The fountain in the middle "
	                "throws up the picture of Lethys for the second player. The fountain on the right throws up the "
	                "picture of Khazar for the third player. Each picture is in the colour of its player.",
	    .environment = {.symbolsAfterLandScript = true, .landNumber = 2},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {0.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_PlayerIconFountain", .offset = {15.0f, 0.0f}, .magnitude = 2.0f, .player = 2}},
	    .landScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
)",
	});

	all.push_back({
	    .id = "particles.draw_paths",
	    .name = "Sorted and queued sprites",
	    .facet = Facet::Particles,
	    .description = "The same smoke drawn three ways side by side: each puff in its own place among what blends, the "
	                   "whole effect at its origin as some spot visuals are, and as the miracle in the hand is.",
	    .expected = "The sorted smoke's puffs blend far to near from any side; the queued smoke draws newest first as one, "
	                "so seen from above older puffs cover newer ones; the third matches the queued without a hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-20.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Queued,
	                   .offset = {0.0f, 0.0f},
	                   .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Immediate,
	                   .offset = {20.0f, 0.0f},
	                   .magnitude = 8.0f}},
	});

	all.push_back({
	    .id = "particles.fireball_rows",
	    .name = "The fireball's heat by its power-up",
	    .facet = Facet::Particles,
	    .description = "Three fireballs are thrown at three oaks every twelve seconds: the plain fireball on the left, "
	                   "the first power-up in the middle and the second power-up on the right.",
	    .expected = "The first power-up uses the second row of the fireball table, so it starts hotter and sets its oak "
	                "on fire sooner. The plain fireball and the second power-up use the first row.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 30.0f}}, .distance = 0.45f},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-30.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {30.0f, 25.0f}}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = {-30.0f, 25.0f},
	                  .handOffset = {-30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpOne,
	                  .point = {0.0f, 25.0f},
	                  .handOffset = {0.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpTwo,
	                  .point = {30.0f, 25.0f},
	                  .handOffset = {30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f}},
	});

	all.push_back({
	    .id = "particles.lightning_forks_arcs",
	    .name = "The lightning bolt's forks and electric arcs",
	    .facet = Facet::Particles,
	    .description = "A lightning bolt held over five villagers and an oak for six seconds, again every ten seconds.",
	    .expected = "The bolt keeps two forks for each target that it finds, and no more. Electric arcs crawl over the "
	                "things that it strikes, from one point of each model to another.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 20.0f}}, .distance = 0.35f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 10.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-3.0f, 14.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {3.0f, 9.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {7.0f, 13.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {0.0f, 17.0f}, .inScript = true},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 18.0f}}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = {0.0f, 12.0f},
	                  .handOffset = {0.0f, 0.0f},
	                  .handHeight = 15.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 6.0f,
	                  .repeatSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.globe_alpha",
	    .name = "The one-shot globe's see-through bubble and rings",
	    .facet = Facet::Particles,
	    .description = "Two one-shot globes close up: a plain fireball and the fireball's first power-up, whose miracle "
	                   "shows one ring.",
	    .expected = "Each bubble is added over what is behind it at an alpha of 149, the globe's 150 times the land's "
	                "fully opaque light. The ring round the powered-up miracle shows at an alpha of 34.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -18.0f}, .look = {0.0f, 4.0f, 8.0f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {-5.0f, 8.0f}},
	                   {.type = MagicType::FireballPowerUpOne, .offset = {5.0f, 8.0f}}},
	});
}

void AddEditor(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "editor.one_of_each",
	    .name = "One of each kind to pick",
	    .facet = Facet::Editor,
	    .description = "A tiger, a villager, an oak, a pillar of rock and a ball, spread out in view, for trying the "
	                   "editor (F2) on: the outliner lists each under its kind, a click on the land or in the list "
	                   "picks one, and the inspector shows it.",
	    .expected = "Each picks with a box drawn round it. The Move (E) and Rotate (R) tools drag the picked thing over "
	                "the land and turn it; Ctrl+D copies it beside itself and Delete removes it. The tiger's inspector "
	                "has every creature section.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, -10.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 20.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {20.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.orbit_walker",
	    .name = "A walking creature to orbit and follow",
	    .facet = Facet::Editor,
	    .description = "A tiger walks round a square 80 units across, again and again, for trying the editor's cameras "
	                   "on it: pick it, then Orbit (O) or Follow (Shift+O).",
	    .expected = "Orbiting, the camera keeps round the tiger as it walks, turned by dragging with the right button "
	                "and drawn in and out by the wheel. Following, it eases along behind the tiger as it turns each "
	                "corner. Free, or Escape, hands the camera back where it is.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-40.0f, -40.0f})},
	    .commands = {Go(Kind::WalkTo, 0, {40.0f, -40.0f}, 0.5f), Go(Kind::WalkTo, 0, {40.0f, 40.0f}, 0.5f),
	                 Go(Kind::WalkTo, 0, {-40.0f, 40.0f}, 0.5f), Go(Kind::WalkTo, 0, {-40.0f, -40.0f}, 0.5f)},
	    .repeatFrom = 0,
	});
}

/// A benchmark of a crowd of the size, to compare runs of several sizes and see which costs grow faster than the crowd
/// does
void AddCrowd(std::vector<Scenario>& all, Crowd::Kind kind, size_t count, std::string_view id, std::string_view name)
{
	const bool creatures = kind == Crowd::Kind::Creatures;
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Benchmark,
	    .description = creatures ? "Creatures of every species and of four players, spread evenly over the land round the "
	                               "middle and clear of the lake, the same way every run. Nothing holds them: their minds, "
	                               "needs, walking and route finding all run as in the game."
	                             : "Villagers of every tribe and role in towns of fifty, each a ring of huts about a storage "
	                               "pit, spread evenly over the land round the middle and clear of the lake, the same way "
	                               "every run, and left to go about their lives.",
	    .expected = "They spawn a batch a frame, then the frames are measured once they have settled: the mean, 95th "
	                "percentile and slowest frame and the costliest profiler stages show as it runs, and Save results "
	                "writes them out to compare with the other sizes.",
	    // The crowd is spread over the middle, where the miracle dispensers would stand
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = kind, .count = count, .seed = k_BenchmarkSeed, .perFrame = creatures ? 100u : 250u},
	});
}

void AddBenchmark(std::vector<Scenario>& all)
{
	// Steps close enough together that a cost growing faster than the crowd shows between them
	AddCrowd(all, Crowd::Kind::Creatures, 10, "benchmark.creatures_10", "10 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 25, "benchmark.creatures_25", "25 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 50, "benchmark.creatures_50", "50 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 100, "benchmark.creatures_100", "100 active creatures");
	AddCrowd(all, Crowd::Kind::Villagers, 100, "benchmark.villagers_100", "100 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 250, "benchmark.villagers_250", "250 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 500, "benchmark.villagers_500", "500 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 1'000, "benchmark.villagers_1000", "1,000 active villagers");
}

/// A creature rests to get better, heals itself, goes or runs home, and goes into, prays at or looks at its temple
void AddRestAndHome(std::vector<Scenario>& all)
{
	// The player's temple west of the middle, turned as Land 1's, and the place east of its pen where the tiger starts
	constexpr glm::vec2 k_Temple {-300.0f, 100.0f};
	constexpr float k_TempleTurn = 36.0f;
	constexpr glm::vec2 k_EastOfHome {-100.0f, 40.0f};
	// A hurt and tired tiger whose body is not held, so that the rest heals it
	const auto hurt = [](glm::vec2 offset, float facing = 90.0f) {
		auto tiger = Content(CreatureType::Tiger, offset, facing);
		tiger.needs.life = 0.3f;
		tiger.needs.exhaustion = 0.6f;
		tiger.hold = false;
		return tiger;
	};
	// A script tells the tiger to do the action, to the birch or to the temple
	const auto told = [](std::string_view action, bool onTemple = false) {
		return Command {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = action, .onTemple = onTemple};
	};
	const auto birchBy = [](glm::vec2 offset) {
		return ObjectSetup {.type = TreeInfo::Birch, .offset = offset + glm::vec2(0.0f, -25.0f)};
	};

	all.push_back({
	    .id = "needs.rest_to_get_better",
	    .name = "Hurt creature rests to get better",
	    .facet = Facet::Needs,
	    .description = "A hurt and tired tiger (life 0.3, tiredness 0.6) stands by a birch. After one second a script "
	                   "tells it to rest to get better.",
	    .expected = "It turns to face down the slope and lies down to sleep, with its eyes closed. Each turn its life and "
	                "its tiredness get better by its sleep rates. It gets up only when its life is full and it is not "
	                "tired at all, and never before three seconds. Then it looks about, dazed, with a grimace.",
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt({0.0f, 0.0f})},
	    .objects = {birchBy({0.0f, 0.0f})},
	    .commands = {told("RestToGetBetter")},
	});

	all.push_back({
	    .id = "needs.go_home_to_recover",
	    .name = "Hurt creature goes home to recover",
	    .facet = Facet::Needs,
	    .description = "A hurt and tired tiger stands east of the pen of its player's temple. After one second a script "
	                   "tells it to go home to recover.",
	    .expected = "It walks home to the pen and stops within its height (no more than 5) of it. Then it faces down the "
	                "slope, lies down and rests until its life is full and it is not tired at all, and gets up dazed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt(k_EastOfHome)},
	    .objects = {birchBy(k_EastOfHome)},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {told("GoHomeToRecover")},
	});

	all.push_back({
	    .id = "needs.restore_health",
	    .name = "Hurt creature chooses how to get better",
	    .facet = Facet::Needs,
	    .description = "A badly hurt tiger (life 0.2) stands east of the pen of its player's temple. Its only desire is to "
	                   "restore its health, as strong as it gets. Nothing frightening is near it.",
	    .expected = "It chooses an action of its list to restore its health: it rests where it is, or goes home to "
	                "recover, or heals itself with the heal miracle when it knows it. It does not stand idle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = hurt(k_EastOfHome);
		    tiger.needs.life = 0.2f;
		    tiger.needs.exhaustion = 0.0f;
		    tiger.desires = OnlyDesire(Desire::RestoreHealth);
		    return tiger;
	    }()},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	});

	all.push_back({
	    .id = "needs.heal_itself",
	    .name = "Creature heals itself with a miracle",
	    .facet = Facet::Needs,
	    .description = "A hurt tiger that knows the heal miracle stands by a birch. A second later a script tells it to "
	                   "heal itself.",
	    .expected = "It casts the heal miracle at the place where it stands, with a grimace, and holds the miracle for "
	                "three seconds. Then its life is full at once.",
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt({0.0f, 0.0f})},
	    .objects = {birchBy({0.0f, 0.0f})},
	    .commands = {{.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Heal)},
	                 told("HealHimself")},
	});

	all.push_back({
	    .id = "mind.go_home",
	    .name = "Creature far from home goes home",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple, more than 20 from it. Its only desire is to "
	                   "stay near home, as strong as it gets.",
	    .expected = "It walks home to the pen and stops within its height (no more than 5) of it. Nearer than 20 to its "
	                "home it does not choose to go home.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, k_EastOfHome, 90.0f);
		    tiger.desires = OnlyDesire(Desire::StayNearHome);
		    return tiger;
	    }()},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	});

	all.push_back({
	    .id = "mind.run_home",
	    .name = "Frightened creature runs home",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to run "
	                   "home.",
	    .expected = "Half the time it first shows that it is frightened, with its eyes wide open. Then it runs home and "
	                "stops within two times its height of the pen.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {told("RunHome")},
	});

	all.push_back({
	    .id = "mind.enter_temple",
	    .name = "Creature goes into its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to go "
	                   "into its temple.",
	    .expected = "It walks home to the pen and stops within its height. It turns to face the temple with a smile. Then "
	                "it walks towards the temple and stops short of it by 1.3 times a third of the temple's radius.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {told("EnterCitadel")},
	});

	all.push_back({
	    .id = "mind.pray_at_temple",
	    .name = "Creature prays at its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to pray "
	                   "at its temple.",
	    .expected = "It walks home to the pen and stops within its height. Then it prays (the pray animation, with its "
	                "sounds).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {told("PrayAtCitadel")},
	});

	all.push_back({
	    .id = "mind.look_at_temple",
	    .name = "Creature looks at its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple, facing away from it. After one second a "
	                   "script tells it to look at the temple.",
	    .expected = "It turns to face the temple (two seconds) and looks at its top for three seconds. One time in three "
	                "it then points at the temple for two seconds, amazed. Then it looks at the foot of the temple for "
	                "three seconds and at its top for two seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {told("LookAtCitadel", true)},
	});

	all.push_back({
	    .id = "mind.odd_lesson_and_confused",
	    .name = "Creature shows a lesson and looks confused",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. After one second a script "
	                   "tells it to show a lesson that it learnt. After eight seconds a script tells it to look confused.",
	    .expected = "First it scratches its head once and smiles. Then it turns round to the camera, scratches its head "
	                "and looks confused. It looks puzzled and keeps its eyes on the camera.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ShowLearntLesson"},
	                 {.kind = Kind::DoAction, .delaySeconds = 8.0f, .object = 0, .action = "LookConfused"}},
	});

	all.push_back({
	    .id = "mind.odd_behave_strangely",
	    .name = "Creature behaves strangely",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing the camera, a birch beside it. After one second a script tells it "
	                   "to behave strangely, and after twenty seconds a script tells it again.",
	    .expected = "Each time it stares at the camera for five seconds with wide eyes that move about, then it is happy. "
	                "One time in two it then faints, lies still for five to ten seconds and gets up. Villagers near it "
	                "react as to a death. At the end it looks confused two times with a grimace, first with sleepy eyes.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BehaveStrangely"},
	                 {.kind = Kind::DoAction, .delaySeconds = 20.0f, .object = 0, .action = "BehaveStrangely"}},
	});

	all.push_back({
	    .id = "mind.examine_kind_looked_over",
	    .name = "Examine a thing and know its kind",
	    .facet = Facet::Mind,
	    .description = "A content tiger and a rock. After one second a script tells the tiger to examine the rock by "
	                   "picking it up.",
	    .expected = "The tiger picks up the rock, looks at it with a curious face and releases it. Then the inspector "
	                "shows the kind of a rock (12) in the observedKinds of its mind. After that, for its curiosity a "
	                "rock is a hundredth as useful as before.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {15.0f, -15.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ExamineByPickingUp"}},
	});
}

/// A task of a land's script that stops gives back the objects that its variables hold. The objects stay in a script
/// until the scripts' turn ends, and then they are in no script
void AddScriptTaskRelease(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	const auto stop = [](float delaySeconds) {
		return Command {.kind = Kind::StopScript, .delaySeconds = delaySeconds, .script = "CheckForCommentOnFoodItems"};
	};
	all.push_back({
	    .id = "story.stopped_task_lets_go",
	    .name = "A stopped script task lets go of the object that it holds",
	    .facet = Facet::Story,
	    .description = "The game's CheckForCommentOnFoodItems script runs with a food pot as its parameter, which it "
	                   "keeps in a variable. One second later the scenario stops the script, and one second after that "
	                   "it stops the script again.",
	    .expected = "The log tells that the first stop stopped one task and that the pot is still in a script for "
	                "the rest of that turn. The second stop stopped no task, and the pot is in no script.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "CheckForCommentOnFoodItems",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0}}},
	                 stop(1.0f),
	                 stop(1.0f)},
	});

	all.push_back({
	    .id = "temple.room_help_without_sound",
	    .name = "The temple's room help when no sound can play",
	    .facet = Facet::Story,
	    .description = "Start the game with no sound device (for example with the environment ALSOFT_DRIVERS=none). "
	                   "The player has a temple, and is put in its main room for the first time.",
	    .expected = "The advisors come out and the help shows its lines one at a time, with no voice. Each line stays for "
	                "its reading time: 5 game turns for each word and 8 more. The help shows all nine lines, then the "
	                "advisors go home. With a sound device, each line stays until the advisor stops talking.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.main_room_lean",
	    .name = "The temple's main room leans at the screen's edge and as the floor is dragged",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. Move the pointer near the top or the bottom "
	                   "of the screen, then drag the floor up and down.",
	    .expected = "The camera starts to lean when the pointer is more than 0.8 of the way from the middle of the "
	                "screen to the top or bottom edge, and leans faster nearer the edge. Dragging the floor leans the "
	                "camera by 0.06 for each pixel and second, less the nearer the pointer is to the sides.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 5.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_rooms",
	    .name = "At the lowest detail the temple's rooms have no light maps",
	    .facet = Facet::Story,
	    .description = "Start the game with --detail-level 0. The player has a temple and is put in its main room. Walk "
	                   "through the doors into the other rooms.",
	    .expected = "Each room, its floor and its pools are the rooms' low models: plainer walls with no light from "
	                "light maps, each wall hazed as one piece. The domes are as at other levels. The doors, the "
	                "scrolls and the clicks on them work as at other levels. With no --detail-level, or above 0, the "
	                "rooms have their light maps.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_scroll_text",
	    .name = "At the lowest detail levels the scrolls have textures of half the size",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player has a temple and is put in "
	                   "its main room. Look at the scrolls on the walls from a distance, then start again with "
	                   "--detail-level 4 to compare.",
	    .expected = "At Low and Minimum the text and the parchment of each scroll are softer, with half the texels "
	                "along each side. Each texel is the mean of four texels of the full text. When the camera goes "
	                "close to a scroll, the text in front of it is as sharp as at other levels.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.way_out_glow",
	    .name = "A white light grows at the temple's way out as the player leaves",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. After ten seconds the camera walks to the "
	                   "door out of the temple and through it, as a click on that door does.",
	    .expected = "The camera turns to the door out. 2.2 seconds after the walk starts, a white sheet of light grows "
	                "on the floor from the door towards the middle of the room, added over all of the room, its texture "
	                "slowly drifting. It is brightest and widest at the door and fades to nothing towards the middle. "
	                "Then the player leaves the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 10.0f, .value = static_cast<size_t>(TempleRoom::Unknown)}},
	});
	// The second visit uses the draws and the scroll textures of the first visit again
	all.push_back({
	    .id = "temple.visit_again",
	    .name = "The player goes into the temple, leaves it and goes in again",
	    .facet = Facet::Story,
	    .description = "The player has a temple and goes into its main room after two seconds. After six seconds the "
	                   "camera walks out through the door. After sixteen seconds the player goes into the main room "
	                   "again.",
	    .expected = "Each time, the camera comes into the main room along its path and the room, its scrolls and the "
	                "map of the land show as on the first visit. Outside, the land and its things show as before the "
	                "visit. No frame of a visit, of the walk out or of the land after it takes 10 ms or more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 6.0f, .value = static_cast<size_t>(TempleRoom::Unknown)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 16.0f, .value = static_cast<size_t>(TempleRoom::Main)}},
	});
}

/// Land 1's script natives and land script commands that the lane land1-script-stubs completed
void AddLandOneScriptStubScenarios(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// A script's dance about a person, as the Pied Piper's dance of Land 1, which wants its camera at once
	all.push_back({
	    .id = "story.dance_camera",
	    .name = "The camera of a dance holds the view until a grip of the land",
	    .facet = Facet::Story,
	    .description = "A leader stands in the middle with six villagers round him. After one second, a script makes the "
	                   "dance about a person (the dance of the Pied Piper on Land 1) about the leader, and the six "
	                   "villagers join it. The scenario says that the hand was near the dance for more than ten seconds, "
	                   "so the dance wants its camera.",
	    .expected = "The villagers walk to their places and dance. At the first key frame of the dance that asks for "
	                "the camera, the camera holds its view. The keys, the wheel and the mouse do not move it. A grip of "
	                "the land with the left button gives the camera back in the next game turn, and the drag of the land "
	                "starts. The dance does not take the camera again. The log tells the number of dancers.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {15.0f, 15.0f}}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 6.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -6.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 4.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-4.0f, -4.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0,
	                  .ctrl = true}},
	});

	all.push_back({
	    .id = "script.set_target_throw",
	    .name = "A script throws a villager to get to a place in a time",
	    .facet = Facet::Story,
	    .description = "A villager stands in the middle. After one second, SET_TARGET throws the villager to a point "
	                   "20 metres east and 10 metres north, 5 metres above the land, in 3 seconds. The magic mushroom "
	                   "challenge of Land 1 throws its actor at the camera in this way.",
	    .expected = "The villager tumbles up in an arc with no air drag and gets to the point after 3 seconds. Then it "
	                "falls to the land. The log tells that SET_TARGET ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, {30.0f, 20.0f}}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_TARGET",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0},
	                                      {.kind = Argument::Kind::Position, .point = {20.0f, 10.0f}, .height = 5.0f},
	                                      {.kind = Argument::Kind::Number, .number = 3.0f}}}},
	});

	// A script gives a clip to a flock of villagers, as Idol Pyre does to its extra worshippers
	all.push_back({
	    .id = "script.flock_plays_clip",
	    .name = "A script gives a flock of villagers a clip to play",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row. After one second, a script makes a flock of them. SET_SCRIPT_ULONG "
	                   "gives the flock the prayer clip once, and SET_SCRIPT_STATE tells the flock to play it. Idol Pyre "
	                   "on Land 2 gives its extra worshippers the clip in this way.",
	    .expected = "Each villager of the flock plays the prayer clip one time. The script then controls each villager. "
	                "The log tells that the two natives ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -12.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_ULONG",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 343.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 1.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_STATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 200.0f}}}},
	});

	// The leans of the three dead trees of Land 1, in degrees about x and z, and one tree with a large lean
	all.push_back({
	    .id = "nature.dead_tree_lean",
	    .name = "Dead trees from a land script lean about x and z",
	    .facet = Facet::Nature,
	    .description = "Four dead trees in a row from the west. The first three have the kinds and the leans of the three "
	                   "dead trees on Land 1. The last tree leans 20 degrees about x and 10 degrees about z, so that the "
	                   "direction of the lean is clear.",
	    .expected = "The first three trees lean by a few degrees, each in its own direction. The last tree leans "
	                "clearly. The turn about the vertical comes first, then the lean about x, then the lean about z.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = TreeInfo::Pine, .offset = {-18.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4544f, -3.1648f}},
	                {.type = TreeInfo::ConiferA, .offset = {-6.0f, 0.0f}, .dead = true, .leanDegrees = {2.2700f, 0.0694f}},
	                {.type = TreeInfo::ConiferA, .offset = {6.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4841f, 1.8931f}},
	                {.type = TreeInfo::ConiferA, .offset = {18.0f, 0.0f}, .dead = true, .leanDegrees = {20.0f, 10.0f}}},
	});

	// Bonfires from a land script: the turn is the third number, and the second number is not used
	all.push_back({
	    .id = "nature.bonfire_land_script_turn",
	    .name = "Bonfires from a land script take their turn from the third number",
	    .facet = Facet::Nature,
	    .description = "Two bonfires in a row from the west, made by land script lines. The first has the numbers of the "
	                   "two bonfires on Land 1: 24.7, then 0. The second has 0, then a quarter turn.",
	    .expected = "The first bonfire stands with no turn, as on Land 1, because the second number is not used. The "
	                "second bonfire is turned by a quarter turn about the vertical.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -6.0f}, {12.0f, 6.0f}}},
	    .landScript = "CREATE_BONFIRE(\"-6.0,0.0\", 24.700001, 0.000000, 1.000000)\n"
	                  "CREATE_BONFIRE(\"6.0,0.0\", 0.000000, 1.570796, 1.000000)\n",
	});

	// Mobile statics that a land script turns and leans: the lean follows the way each one faces
	all.push_back({
	    .id = "nature.mobile_static_lean",
	    .name = "Mobile statics from a land script lean and turn in the game's order",
	    .facet = Facet::Nature,
	    .description = "Four mobile statics in a row from the west, each turned 45 degrees about the vertical. The first "
	                   "fence leans 30 degrees about x. The second fence leans 30 degrees about z. The long rock and the "
	                   "totem lean about x and z.",
	    .expected = "Each thing leans in the direction that it faces after its turn, not along the axes of the map. The "
	                "first fence tips forward along its facing. The second fence tips to its side.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {30.0f, 0.0f}},
	                {.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {0.0f, 30.0f}},
	                {.type = MobileStaticInfo::LongrockSandstone,
	                 .offset = {6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {20.0f, 20.0f}},
	                {.type = MobileStaticInfo::GateTotemApe,
	                 .offset = {18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {25.0f, -15.0f}}},
	});

	// Two rows of small pines along the path of a low throw. The camera stands as for the hand's throw.
	std::vector<ObjectSetup> bendTrees {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}};
	for (int row = 0; row < 10; ++row)
	{
		const float along = 6.0f + static_cast<float>(row) * 3.0f;
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, -2.0f}, .scale = 0.5f});
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, 2.0f}, .scale = 0.5f});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_a_thrown_rock",
	    .name = "Trees bend away from a rock that flies between them",
	    .facet = Facet::Nature,
	    .description = "The hand takes a rock and throws it low between two rows of small pines. Then the rock comes to "
	                   "rest.",
	    .expected = "While the rock flies, the pines near it lean away from it, more when the rock is near. A pine that "
	                "the rock passes stands straight again when the rock is out of its reach. When the rock comes to "
	                "rest among the pines, the pines in its reach stay bent away from it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = bendTrees,
	    .commands =
	        {// Takes the rock under the middle of the screen
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	         // Makes ready low on the screen and sweeps a little up, so that the throw stays low
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.7f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.15f}, .amount = 0.3f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// Two rows of pines with a lane between them. The player's creature walks along the lane and back.
	std::vector<ObjectSetup> lanePines;
	for (int row = 0; row < 10; ++row)
	{
		const float along = static_cast<float>(row) * 3.0f;
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, -4.5f}});
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, 4.5f}});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_the_creature",
	    .name = "Trees bend away from the player's creature",
	    .facet = Facet::Nature,
	    .description = "The player's tiger walks along a lane between two rows of pines, then walks back.",
	    .expected = "The pines near the tiger lean away from it, more when the tiger is near, and crash about. When the "
	                "tiger has gone by, they stand straight again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -12.0f}, {40.0f, 12.0f}}},
	    .creatures = {CreatureSetup {
	        .label = "yours", .species = CreatureType::Tiger, .offset = {-10.0f, 0.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .objects = lanePines,
	    .commands = {{.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = {40.0f, 0.0f}},
	                 {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 8.0f, .point = {-10.0f, 0.0f}}},
	});
}

void AddVillagerSizeAndReachScenarios(std::vector<Scenario>& all)
{
	// Children of each age from 1 to 12 in a row, then a grown up, all held still by a script
	std::vector<ObjectSetup> ages;
	constexpr uint32_t k_OldestChild = 12;
	for (uint32_t age = 1; age <= k_OldestChild + 1; ++age)
	{
		ages.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                .offset = {static_cast<float>(age) * 4.0f - 28.0f, 0.0f},
		                .villagerAge = age <= k_OldestChild ? age : 30u,
		                .inScript = true});
	}
	all.push_back({
	    .id = "growth.villager_size_by_age",
	    .name = "Villagers of each age side by side",
	    .facet = Facet::Growth,
	    .description = "Twelve children of the ages 1 to 12 and a grown up stand in a row, from the west, held still by a "
	                   "script.",
	    .expected = "Each child is a little larger than the one to its west. A child starts at the size of the year before "
	                "its age and grows towards the size of the year after it. The grown up is between 0.95 and 1.05 of "
	                "full size.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -6.0f}, {30.0f, 6.0f}}},
	    .objects = ages,
	});

	// Villagers of the player's town walking there and back
	std::vector<ObjectSetup> walkers;
	for (int i = 0; i < 3; ++i)
	{
		const float y = static_cast<float>(i) * 6.0f - 6.0f;
		walkers.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                   .offset = {-20.0f, y},
		                   .walkTo = glm::vec2(20.0f, y),
		                   .walkAfterSeconds = 1.0f,
		                   .walkRepeatSeconds = 20.0f,
		                   .joinTown = true});
	}
	all.push_back({
	    .id = "movement.villager_speed_town_belief",
	    .name = "The walk speed of villagers in a player's town",
	    .facet = Facet::Movement,
	    .description = "Three villagers of the player's town walk 40 metres east and back every twenty seconds. Their speed "
	                   "comes from the player's most Indian tribal power and from the town's belief in the player against "
	                   "the town's own neutral belief.",
	    .expected = "The villagers walk at the speed of their state, scaled by the town's belief in the player beyond the "
	                "town's own neutral belief. The inspector shows the speed of each walker's WallHug.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-24.0f, -10.0f}, {24.0f, 10.0f}}},
	    .objects = walkers,
	});
	// A villager that sits at the end of each walk stands up before the next walk starts
	all.push_back({
	    .id = "movement.villager_stands_up_then_walks",
	    .name = "A villager stands up before it walks",
	    .facet = Facet::Movement,
	    .description = "A villager walks 20 metres east and sits down there. Every twenty seconds it walks to the other "
	                   "point and sits down again.",
	    .expected = "At the start of each walk, the villager plays its clip to stand up where it sits. It does not move "
	                "until the clip ends. Then it walks. The villager walks in its own turn, and it does not walk while "
	                "a clip into or out of a state plays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {10.0f, 2.5f, -7.0f}, .look = {9.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-10.0f, 0.0f},
	                 .walkTo = glm::vec2(10.0f, 0.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f,
	                 .walkFinal = VillagerStates::SitAndChillout}},
	});
	// Dancers that walk to their places in a dance, each step at its point in the dance's state
	all.push_back({
	    .id = "movement.dancers_step_in_their_dance",
	    .name = "Dancers take their steps in the dance",
	    .facet = Facet::Movement,
	    .description = "A leader stands in the middle with four villagers round him, 8 metres away. After one second, a "
	                   "script makes the dance about a person about the leader, and the four villagers join it.",
	    .expected = "Each dancer does the move of the dance first. In the same turn, it starts its walk to its place and "
	                "takes the first step. When the group moves, each dancer takes its step to the new place in the "
	                "turn that the group moves. A dancer on its place takes no step.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -18.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 8.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -8.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0}},
	});
	// Two creatures, each with its own turn in the list of the living
	all.push_back({
	    .id = "mind.two_creatures_own_turns",
	    .name = "Two creatures each take their turn at their own place",
	    .facet = Facet::Mind,
	    .description = "A tiger and a cow stand 30 metres apart with nothing to do, and a villager walks between them. "
	                   "With the game's log at the debug level, each creature writes the synced random numbers of its "
	                   "turn.",
	    .expected = "Each creature thinks, walks and acts as it does alone. In the log, the numbers that each creature "
	                "draws come together in its own turn, the newer creature first. No numbers of the other creature come "
	                "between them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "older"),
	                  Content(CreatureType::Cow, {15.0f, 0.0f}, 270.0f, "newer")},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {0.0f, -10.0f},
	                 .walkTo = glm::vec2(0.0f, 10.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f}},
	});

	// The tutorial's interface levels, as a lesson's script steps through them
	std::vector<Command> levels;
	for (const size_t level : {1, 2, 1, 5, 1, 11, 1, 15, 8})
	{
		levels.push_back({.kind = Kind::SetInterfaceLevel, .delaySeconds = 5.0f, .value = level});
	}
	all.push_back({
	    .id = "tutorial.interface_level_reach",
	    .name = "The hand's reach at each interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level changes every five seconds, as a lesson's script changes it: level 1, then 2, 5, "
	                   "11 and 15, each after level 1 again, and at the end level 8. The scenario log gives the hand's reach "
	                   "at each level.",
	    .expected = "Level 1 gives the hand a short reach of 75 metres. Levels 2, 5, 11 and 15 give the full reach of "
	                "1800 metres. Level 8 keeps the reach that level 15 gave.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = levels,
	});
	// A lesson's interface level limits the Action button, and the normal level removes the limits
	all.push_back({
	    .id = "tutorial.interface_level_hand_limits",
	    .name = "The limits of the Action button at a lesson's interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level becomes 1, the level of the opening's lesson. After 40 seconds it becomes "
	                   "the normal level 0. A villager and a scroll stand in front of the camera.",
	    .expected = "At level 1 the Action button does not pick up the villager, and a press on the land does nothing. "
	                "A click on the scroll still reaches the scripts. At level 0 the hand picks up the villager again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 6.0f}}},
	    .highlights = {{.kind = 1, .offset = {4.0f, 6.0f}, .challenge = 0}},
	    .commands = {{.kind = Kind::SetInterfaceLevel, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::SetInterfaceLevel, .delaySeconds = 40.0f, .value = 0}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_flight",
	    .name = "A long flight to a bookmark",
	    .facet = Facet::Tutorial,
	    .description = "A bookmark is put on the land 900 metres from the middle of the map, and the camera flies to it, as "
	                   "the number key 1 does. Five seconds later a second bookmark is put back at the middle, and the "
	                   "camera flies back to it.",
	    .expected = "Each flight goes by a high point half way. The high point is 0.3 metres higher for each metre that the "
	                "flight goes across, so these flights go up by about 270 metres. The camera makes a woosh at the start "
	                "of each flight.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 2.0f, .point = {900.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 5.0f, .point = {0.0f, 0.0f}, .value = 1}},
	});
	all.push_back({
	    .id = "tutorial.tab_town_flights",
	    .name = "The Tab key flies to the player's towns",
	    .facet = Facet::Tutorial,
	    .description = "Lines of a land script make three towns of the player in this order: a town with a storage pit to "
	                   "the west, a town with a village centre to the north, and a town with no building to the east. A "
	                   "neutral town with a storage pit stands to the south. The player presses the Tab key four times, "
	                   "eight seconds apart.",
	    .expected = "The first press flies the camera to the village centre to the north. The second press flies it to the "
	                "storage pit to the west. The town to the east has no place to fly to, so the third press flies to "
	                "the north again, and the fourth to the west. The camera never goes to the neutral town. Each flight "
	                "ends 24 metres west, 28 metres up and 47 metres north of the place, looking at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .landScript = R"(CREATE_TOWN(60, "-70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(60, "PLAYER_ONE", 10.0)
CREATE_ABODE(60, "-70.0,10.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
CREATE_TOWN(61, "0.0,80.0", "PLAYER_ONE", 0, "NORSE")
SET_TOWN_BELIEF(61, "PLAYER_ONE", 10.0)
CREATE_TOWN_CENTRE(61, "0.0,90.0", "NORSE_ABODE_TOWN_CENTRE", 0, 990, 0)
CREATE_TOWN(62, "70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(62, "PLAYER_ONE", 10.0)
CREATE_TOWN(63, "0.0,-80.0", "NEUTRAL", 0, "CELTIC")
CREATE_ABODE(63, "0.0,-70.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
)",
	    .commands = {{.kind = Kind::TabToTown, .delaySeconds = 3.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_markers_follow",
	    .name = "Bookmark markers and a followed creature",
	    .facet = Facet::Tutorial,
	    .description = "Three bookmarks go on the land near the middle of the map, as Ctrl and the keys 1, 2 and 3 do. Then "
	                   "the player keeps the tiger in bookmark 4, as Ctrl and the key 4 do with the hand over it, and the "
	                   "camera follows it, as Shift and the key 4 do. The tiger walks away to the north.",
	    .expected = "Each bookmark shows its yellow number in a ring, two metres above its place. The marker of bookmark 4 "
	                "stays two metres above the tiger and moves with it. All the markers throb together: the width and "
	                "the height each swing once a second, the height a fifth of a second behind. The camera flies after "
	                "the tiger each turn, until the player moves the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 0.0f)},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {25.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {-25.0f, 0.0f}, .value = 1},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {0.0f, 25.0f}, .value = 2},
	                 {.kind = Kind::BookmarkCreature, .creature = 0, .delaySeconds = 1.0f, .value = 3},
	                 Go(Kind::WalkTo, 0, {0.0f, 160.0f}, 0.5f, false)},
	});
}

/// A blow that sways the creature, and a wolf that frightens it
void AddSwayAndFrightScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.sway_from_a_thrown_rock",
	    .name = "A thrown rock sways the creature",
	    .facet = Facet::Objects,
	    .description = "A tiger stands still, and a rock flies at its legs from the west every five seconds.",
	    .expected = "Each rock that hits the tiger sways its body. The body swings back on its spring once a game "
	                "turn and settles. A body that turns on the spot or sits does not sway.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 0.0f}, {10.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-25.0f, 0.0f}}},
	    .throws =
	        {{.object = 0, .height = 14.0f, .velocity = {14.0f, 4.0f, 0.0f}, .delaySeconds = 2.0f, .repeatSeconds = 5.0f}},
	});

	// Rocks that a script makes go into the physics
	const auto create = [](float delay, int32_t type, glm::vec2 point, float height) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = "CREATE",
		    .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(type)},
		                        {.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(MobileStaticInfo::Rock)},
		                        {.kind = ScriptArgument::Kind::Position, .point = point, .height = height}}};
	};
	all.push_back({
	    .id = "objects.script_rock_falls",
	    .name = "A rock that a script makes falls onto the land",
	    .facet = Facet::Objects,
	    .description = "After one second, CREATE makes a rock as a mobile static 10 metres above the land, 10 metres "
	                   "west of the middle. Two seconds later, CREATE makes a rock by the rock type at the same height, "
	                   "10 metres east of the middle.",
	    .expected = "The first rock falls from the height that the script gave and comes to rest on the land. The "
	                "second rock shows on the land at once, turned to the slope, and stays there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}},
	    .commands = {create(1.0f, 8, {-10.0f, 0.0f}, 10.0f), create(2.0f, 33, {10.0f, 0.0f}, 10.0f)},
	});

	all.push_back({
	    .id = "senses.creature_runs_from_a_wolf",
	    .name = "A wolf frightens the creature",
	    .facet = Facet::Senses,
	    .description = "A tiger with its fear as strong as it gets, and a wolf fifteen metres east of it.",
	    .expected = "The tiger runs away from the wolf. Tigers, leopards, wolves and the wolves of a spell frighten a "
	                "creature as lions do.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {20.0f, 0.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	        .needs = Content(),
	        .desires = {{.desire = Desire::Fear, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Wolf, .offset = {15.0f, 0.0f}, .living = true}},
	});
}

void AddStoreScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	    {.type = AbodeInfo::CelticStoragePit, .offset = {-30.0f, 30.0f}, .life = 0.1f, .storedFood = 2000, .storedWood = 1000},
	};
	for (size_t i = 0; i < 3; ++i)
	{
		const float x = (static_cast<float>(i) - 1.5f) * 3.0f;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "objects.store_stops_working",
	    .name = "A broken storage pit calls people to its piles",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood. The "
	                   "storage pit has damage down to a tenth of its life. Six villagers of the town stand near it.",
	    .expected = "The storage pit stops working. Its food pile and each wood pile with wood in it call the people "
	                "near them, as a pile on its own does (the inspector shows the reactions of the piles). A repair "
	                "of the pit above its limit stops the calls. A pile that becomes empty stops its call and keeps its "
	                "place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 30.0f}, {0.0f, 50.0f}, {0.0f, 17.0f}}},
	    .objects = std::move(objects),
	});

	// A toadstool, as Land 1's script puts nine on the land, let go above a storage pit
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.toadstool_poisons_store",
	    .name = "A toadstool poisons the food of a storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood, and "
	                   "a toadstool beside them. After one second the toadstool is lifted and let go above the "
	                   "storage pit, as the hand drops one there.",
	    .expected = "The storage pit takes the toadstool as 150 food. Its food pile becomes poisoned and shows a pale "
	                "green tint. Its wood piles keep their usual colour. Villagers who take food or wood from the "
	                "storage pit become poisoned.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = MobileObjectInfo::Toadstool, .offset = k_Pit + glm::vec2 {15.0f, -15.0f}}},
	    .throws = {{.object = 2, .from = k_Pit, .height = 6.0f, .velocity = {0.0f, -1.0f, 0.0f}, .delaySeconds = 1.0f}},
	});
}

void AddPoisonScenarios(std::vector<Scenario>& all)
{
	// What the Plague's script does first: it enables the poison of the town and then of its storage pit, and later cures
	// the town. The scenario runs those calls.
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	using Argument = ScriptArgument;
	const auto poison = [](float delay, Argument thing, int flag) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = delay,
		                .script = "SET_POISONED",
		                .scriptArguments = {{.kind = Argument::Kind::Integer, .number = static_cast<float>(flag)}, thing}};
	};
	all.push_back({
	    .id = "objects.script_poisons_town_and_store",
	    .name = "A script poisons a town and its storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit with 2000 food and 1000 wood, "
	                   "and four villagers of the town. After one second a script poisons the town. After three seconds it "
	                   "poisons the storage pit. After ten seconds it cures the town.",
	    .expected = "At one second each of the four villagers is poisoned, as the inspector shows. At three seconds the "
	                "food pile of the storage pit is poisoned and shows a pale green tint. Its wood piles keep their usual "
	                "colour. At ten seconds the villagers are cured, but the food pile stays poisoned. Villagers who take "
	                "food from the pit become poisoned again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-6.0f, -8.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {6.0f, -8.0f}, .joinTown = true}},
	    .commands = {poison(1.0f, {.kind = Argument::Kind::Town}, 1),
	                 poison(2.0f, {.kind = Argument::Kind::Object, .index = 1}, 1),
	                 poison(7.0f, {.kind = Argument::Kind::Town}, 0)},
	});
}

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
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddBirdScenarios(all);
	AddTeleportScenarios(all);
	AddVortexScenarios(all);
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
	AddWorldSoundScenarios(all);
	AddTownMiracleScenarios(all);
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
	AddWorshipSiteScenarios(all);
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
	const auto fits = [&scenario, earlierCalls](const ScriptArgument& argument) {
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
	return kind == Kind::SetHour || kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed ||
	       kind == Kind::PressKey || kind == Kind::HandTakeFireBall || kind == Kind::SetAlignment || kind == Kind::WideScreen ||
	       kind == Kind::SetInterfaceLevel || kind == Kind::FlyToBookmark || kind == Kind::TabToTown ||
	       kind == Kind::SetOpenClose || kind == Kind::LayGateStone || kind == Kind::HandTapObject || kind == Kind::Advisor ||
	       kind == Kind::RunText || kind == Kind::PlayHandDemo || kind == Kind::Reframe || kind == Kind::DestroyTemple ||
	       kind == Kind::RunHelpMessage || kind == Kind::EndGuidanceWaits || kind == Kind::SetTownBeliefInRival ||
	       kind == Kind::RecordChallenge || kind == Kind::EnterTemple || kind == Kind::HandTake || kind == Kind::RunScript ||
	       kind == Kind::PlayBoatFilm || kind == Kind::HandPour || kind == Kind::HandRelease ||
	       kind == Kind::SetReportLatency || kind == Kind::RemoveObject || kind == Kind::ReadTip ||
	       kind == Kind::LookAtTempleScroll || kind == Kind::HandGripTotem || kind == Kind::ProfileFolderCheck ||
	       kind == Kind::PlayCustomMusic || kind == Kind::ShowCustomMusicControls || kind == Kind::OwnTattooPicture ||
	       kind == Kind::OpenTattooMenu || kind == Kind::OpenControlsBox || kind == Kind::OpenMenuPage ||
	       kind == Kind::PressMenuKey || kind == Kind::StopScript || kind == Kind::StartScriptMusic || kind == Kind::SaveGame ||
	       kind == Kind::LoadGame || kind == Kind::QuickSave || kind == Kind::ContinueGame || kind == Kind::PlayLogoEffect ||
	       kind == Kind::PlayFallingSpellFilm || kind == Kind::PlayIntroFilm || kind == Kind::SayLine ||
	       kind == Kind::PlayGuidanceVoice || kind == Kind::ScriptRelease || kind == Kind::FillSiteIcons ||
	       kind == Kind::ChargeIconWithSeedHeld || kind == Kind::FinishBuilding || kind == Kind::FillBuiltAmount ||
	       kind == Kind::ScaffoldStartsSite || kind == Kind::DestroyScaffoldSite || kind == Kind::CallNative ||
	       kind == Kind::PauseGame || kind == Kind::SetGameSpeed || kind == Kind::RemoveTeleportStone ||
	       kind == Kind::MakeDance || kind == Kind::OpenMultiplayerBox || kind == Kind::BuildWorshipSites ||
	       kind == Kind::PressPlainKey || kind == Kind::PlainKeyDown || kind == Kind::PlainKeyRepeat ||
	       kind == Kind::PlainKeyUp || kind == Kind::OtherGodHand || kind == Kind::OwnGod || kind == Kind::JoinOnline ||
	       kind == Kind::EndBox || kind == Kind::BringCreature || kind == Kind::TattooEditorAsNetworkGame ||
	       kind == Kind::ShowDemoPing || kind == Kind::AddIncomingMessage || kind == Kind::Chat ||
	       kind == Kind::ComputerPersonality || kind == Kind::ComputerUntaughtSends || kind == Kind::SkirmishBox ||
	       IsPointerCommand(kind);
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
	constexpr std::array<std::string_view, 202> k_Names {
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
		if (throwing.object >= scenario.objects.size() || (throwing.from.has_value() && !ValidOffset(*throwing.from)) ||
		    throwing.height < 0.0f)
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
