/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the body of the creature: rest, faces, senses, needs, sleep and home.

#include <array>

#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/IncomingMessageRules.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddIdle(std::vector<Scenario>& all)
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

void testbed_scenarios::AddExpressions(std::vector<Scenario>& all)
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
	    .expected = "Smile, grimace, growl, scared, sad, amazed and puzzled, each held then easing out of the last. The "
	                "tiger has no laugh, ooh or aah, so its face stays plain for those three. Then a nod, a shake of the "
	                "head, a yawn, thirst, squirting water and talking.",
	    .framing = {.shot = Shot::Head, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = faces,
	    .repeatFrom = 0,
	});

	// A creature that freezes lives fewer milliseconds in each game turn. Its face, its gesture and its head move on by
	// these milliseconds, once a turn.
	all.push_back({
	    .id = "expressions.frozen_face_and_gesture",
	    .name = "Face and gesture of a creature that freezes",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused, close up, pulls a grimace. The player casts the freeze spell on it. "
	                   "While the ice grows, the tiger starts a yawn.",
	    .expected = "The yawn plays more slowly and stops part of the way through. The grimace relaxes more slowly and "
	                "stops. Nothing jumps: each part moves on by the time that the frozen body lives in the turn.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Head, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .miracles = {{.type = MagicType::CreatureSpellFreeze, .target = MiracleCast::Target::Creature, .delaySeconds = 2.0f}},
	    .commands = {Play(Kind::PullFace, 0, animations::k_FirstFace + 1, 1.0f, false),
	                 Play(Kind::PlayGesture, 0, animations::k_Yawn, 3.0f, false)},
	});

	// The tiger has no clip for the laugh. The tiger keeps the face that it wants, but the face shows nothing. The smile
	// after it starts at once.
	constexpr size_t k_TigerLaugh = animations::k_FirstFace + 7;
	all.push_back({
	    .id = "expressions.face_without_clip",
	    .name = "A face that the tiger has no clip for",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused, close up, pulls the laugh face, which the tiger has no clip for, "
	                   "then the smile, three seconds later.",
	    .expected = "For three seconds the face stays plain and does not move: the laugh shows nothing. Then the smile "
	                "starts at once from its first frame and holds for one second. When the laugh comes again, the smile "
	                "eases out and the face stays plain. This repeats. The clip log names the laugh as not found.",
	    .framing = {.shot = Shot::Head, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {Play(Kind::PullFace, 0, k_TigerLaugh, 1.0f, false),
	                 Play(Kind::PullFace, 0, animations::k_FirstFace, 3.0f, false)},
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

void testbed_scenarios::AddSenses(std::vector<Scenario>& all)
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

void testbed_scenarios::AddNeeds(std::vector<Scenario>& all)
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
	                "there. Its walk ends at once on each turn and starts again on the next turn. It does not give up the "
	                "toy and does not eat the pig. As in the game, it stays until it faints from hunger "
	                "(vanilla-bugs row 389).",
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

	// A script sends a flying pigeon to a point: the move to a place keeps its height above the land
	all.push_back({
	    .id = "animals.script_move_keeps_bird_height",
	    .name = "A bird that a script sends keeps its height",
	    .facet = Facet::Animals,
	    .description = "A pigeon flies at the usual height of its kind. After one second, MOVE_GAME_THING sends it 40 "
	                   "metres to the north.",
	    .expected = "The pigeon flies to its point at the same height above the land, as in the original game. It does "
	                "not go down to the land on the way.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-30.0f, 6.0f, 20.0f}, .look = {0.0f, 8.0f, 20.0f}},
	    .objects = {{.type = AnimalInfo::Pigeon, .offset = {0.0f, 0.0f}, .living = true}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {0.0f, 40.0f}},
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
	    .id = "needs.sleep_night_length",
	    .name = "Nearly rested creature sleeps out its night",
	    .facet = Facet::Needs,
	    .description = "Ten at night, the clock standing still: a tiger only a little tired, told to sleep. At night a "
	                   "nearly rested creature sleeps for its size, by its species' sleep length, by the seconds of a "
	                   "game day, by 5 turns.",
	    .expected = "It lies down and sleeps. It does not get up after a few seconds, when it is nearly rested: it "
	                "sleeps on for about a minute (the day is 1700 seconds long), then gets up.",
	    .environment = {.hour = 22.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.12f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}},
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

/// A creature rests to get better, heals itself, goes or runs home, and goes into, prays at or looks at its temple
void testbed_scenarios::AddRestAndHome(std::vector<Scenario>& all)
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
