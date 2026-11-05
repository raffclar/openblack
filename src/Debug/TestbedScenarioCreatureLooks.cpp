/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the looks of the creature: growth, appearance and light.

#include <array>

#include "Creature/CreatureCave.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Sound/SoundId.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddGrowth(std::vector<Scenario>& all)
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

void testbed_scenarios::AddAppearance(std::vector<Scenario>& all)
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
	// A creature whose mind file gives its body the environment map, beside one whose mind file does not
	{
		auto plain = Content(CreatureType::GiantApe, {-12.0f, 0.0f}, 0.0f, "plain ape");
		plain.pauseMind = true;
		auto mapped = Content(CreatureType::GiantApe, {12.0f, 0.0f}, 0.0f, "environment-mapped ape");
		mapped.pauseMind = true;
		mapped.environmentMap = 1;
		all.push_back({
		    .id = "appearance.creature_environment_map",
		    .name = "Creature body with the environment map",
		    .facet = Facet::Appearance,
		    .description = "At noon, the clock stopped, two giant apes stand side by side. The mind file of the ape on the "
		                   "right gives its body the environment map. They nod, shake their heads and do their other "
		                   "gestures.",
		    .expected = "The ape on the right shows the environment map added over every part of its body, so it looks "
		                "brighter and shiny. The shine moves over the body as it turns, as if it reflects the sky. The "
		                "light on the body is the same as on the ape on the left. The eyes of the two apes look the same.",
		    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-18.0f, -4.0f}, {18.0f, 4.0f}}, .distance = 0.6f},
		    .creatures = {plain, mapped},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	}
	// The hair of a body with the environment map takes the light of the last usual hair that the game drew. At night
	// the hand lights the plain ape, and the ape with the map stands in the dark.
	{
		auto plain = Content(CreatureType::GiantApe, {-30.0f, 0.0f}, 0.0f, "plain ape");
		plain.pauseMind = true;
		auto mapped = Content(CreatureType::GiantApe, {30.0f, 0.0f}, 0.0f, "environment-mapped ape");
		mapped.pauseMind = true;
		mapped.environmentMap = 1;
		all.push_back({
		    .id = "appearance.creature_environment_hair_light",
		    .name = "Hair light of a creature with the environment map",
		    .facet = Facet::Appearance,
		    .description = "At eleven at night, the clock stopped, two giant apes stand apart. The hand is over the ape on "
		                   "the left. The mind file of the ape on the right gives its body the environment map. They nod, "
		                   "shake their heads and do their other gestures.",
		    .expected = "The hand lights the land under the ape on the left, so its hair is bright. The ape on the right "
		                "stands in the dark, but its hair is as bright as the hair of the ape on the left. Its hair takes "
		                "the light of the last usual hair that the game drew, not the light where it stands.",
		    .environment = {.hour = 23.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.3f, 0.5f)},
		    .framing = {.shot = Shot::Overview, .include = {{-36.0f, -4.0f}, {36.0f, 4.0f}}, .distance = 0.6f},
		    .creatures = {plain, mapped},
		    .commands = headMoves,
		    .repeatFrom = 0,
		});
	}
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
	// A frozen creature that fizzes: the ice goes over all of its body after the fizz, also where the body is gone
	all.push_back({
	    .id = "appearance.frozen_creature_fizzing",
	    .name = "Frozen creature fizzing out of sight",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, a tiger stands on open land, seen from close by. After a second the "
	                   "freeze spell is cast on it. When it is wholly frozen, a script fade fizzes it six tenths of the "
	                   "way out of sight over three seconds.",
	    .expected = "The tiger freezes to an icy blue with a sheen of ice. Then it fizzes through static. The sheen of ice "
	                "stays over all of its body, also where the static has taken the body away: there the land behind "
	                "shows through with the ice added over it.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.4f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -90.0f}, 180.0f, "frozen tiger")},
	    .miracles = {{.type = MagicType::CreatureSpellFreeze,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 0,
	                  .delaySeconds = 1.0f}},
	    .commands = {{.kind = Kind::SetFizz, .creature = 0, .delaySeconds = 5.5f, .value = 3000, .amount = 0.6f}},
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
	// The same view at the lowest detail levels, with the graphics device made again after two seconds: the textures
	// at half their size give their texels again
	for (const bool lost : {false, true})
	{
		all.push_back({
		    .id =
		        lost ? "appearance.low_resolution_textures_device_lost" : "appearance.low_resolution_textures_device_restored",
		    .name = lost ? "Low resolution textures after a lost graphics device"
		                 : "Low resolution textures after the graphics device is made again",
		    .facet = Facet::Appearance,
		    .description = lost ? "The view of the low resolution textures scenario. After two seconds the graphics device "
		                          "is lost, and the renderer makes it again. Start the game with --detail-level 1 (Low) "
		                          "or 0 (Minimum)."
		                        : "The view of the low resolution textures scenario. After two seconds the renderer makes "
		                          "the graphics device again, as after a lost device. Start the game with --detail-level 1 "
		                          "(Low) or 0 (Minimum).",
		    .expected = "The picture does not change when the device is made again. The textures keep their half size: "
		                "the land, the hut, the oaks, the sea and the sky are as in the low resolution textures "
		                "scenario. No texture is black, white or missing.",
		    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -90.0f}, .look = {0.0f, 0.0f, 60.0f}},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-15.0f, -40.0f}},
		                {.type = TreeInfo::Oak, .offset = {15.0f, -45.0f}},
		                {.type = TreeInfo::Oak, .offset = {25.0f, -30.0f}}},
		    .commands = {{.kind = Kind::RestoreDevice, .delaySeconds = 2.0f, .value = lost ? 1U : 0U}},
		});
	}
	// A boulder breaks a hut: the broken model and its pieces are models that the game makes. With the restore, the
	// graphics device is made again after the hit, and the made models give their vertices again.
	constexpr glm::vec2 k_BrokenHut {0.0f, 20.0f};
	constexpr std::array<std::string_view, 3> k_BrokenHutDescriptions {
	    "At noon, the clock stopped, a hand throws a boulder north at a hut from 30 metres south of it, one second "
	    "after the start.",
	    "At noon, the clock stopped, a hand throws a boulder north at a hut from 30 metres south of it, one second "
	    "after the start. Four seconds after the start the renderer makes the graphics device again, as after a lost "
	    "device.",
	    "At noon, the clock stopped, a hand throws a boulder north at a hut from 30 metres south of it, one second "
	    "after the start. Four seconds after the start the graphics device is lost, and the renderer makes it again.",
	};
	for (const auto restore : {0U, 1U, 2U})
	{
		const bool lost = restore == 2U;
		auto& broken = all.emplace_back(Scenario {
		    .id = restore == 0U ? "physics.boulder_breaks_hut"
		          : lost        ? "physics.boulder_breaks_hut_device_lost"
		                        : "physics.boulder_breaks_hut_device_restored",
		    .name = restore == 0U ? "A boulder breaks a hut"
		            : lost        ? "A broken hut after a lost graphics device"
		                          : "A broken hut after the graphics device is made again",
		    .facet = Facet::Physics,
		    .description = k_BrokenHutDescriptions.at(restore),
		    .expected = restore == 0U ? "The boulder hits the hut and breaks it. The hut shows the hole of the hit, and "
		                                "pieces fall away from it and lie on the land."
		                              : "The boulder hits the hut and breaks it. The hut shows the hole of the hit, and "
		                                "pieces fall away from it and lie on the land. The broken hut and its pieces do "
		                                "not change when the device is made again: no part goes black or missing.",
		    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {-20.0f, 14.0f, -4.0f}, .look = {k_BrokenHut.x, 2.0f, k_BrokenHut.y}},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_BrokenHut},
		                {.type = MobileStaticInfo::Boulder1Chalk, .offset = k_BrokenHut - glm::vec2(0.0f, 30.0f)}},
		    .throws = {{.object = 1,
		                .from = k_BrokenHut - glm::vec2(0.0f, 30.0f),
		                .height = 4.0f,
		                .velocity = {0.0f, 4.0f, 25.0f},
		                .delaySeconds = 1.0f}},
		});
		if (restore != 0U)
		{
			broken.commands = {{.kind = Kind::RestoreDevice, .delaySeconds = 4.0f, .value = lost ? 1U : 0U}};
		}
	}
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
	all.push_back({
	    .id = "appearance.low_detail_tattoos_wounds",
	    .name = "Tattoos and wounds on half-size skins",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The same tiger as in "
	                   "appearance.tattoos_wounds, with a tattoo in every slot, five wounds and burns and a trail of "
	                   "blood, beside an unmarked tiger. Start it again with --detail-level 4 to compare.",
	    .expected = "At Low and Minimum the skins are at half size, so they look softer. The tattoos, the wounds and the "
	                "blood are at the same places on the body and have the same size as at the higher levels, with "
	                "fewer texels. Nothing is missing and nothing moves to another place.",
	    .framing = {.shot = Shot::Follow, .distance = 0.9f},
	    .creatures = {tattooed, plain},
	});

	auto evilFace = Posed(CreatureType::Tiger, {-4.0f, 0.0f}, 0.0f, "evil");
	evilFace.alignment = -1.0f;
	auto goodFace = Posed(CreatureType::Tiger, {4.0f, 0.0f}, 0.0f, "good");
	goodFace.alignment = 1.0f;
	goodFace.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "appearance.eyelid_skin_colour",
	    .name = "Eyelids in the colour of the painted skin",
	    .facet = Facet::Appearance,
	    .description = "An evil tiger and a good tiger stand side by side and face the camera, close up. Start it again "
	                   "with --detail-level 1 to see the half-size skins.",
	    .expected = "Each tiger's eyelids have the colour of its own painted skin under them: the evil skin on the evil "
	                "tiger and the good skin on the good tiger. Both lids of a tiger have the same colour. A white part "
	                "of the skin gives a lid that is a little darker than full white.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Head, .distance = 0.8f},
	    .creatures = {evilFace, goodFace},
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

void testbed_scenarios::AddLight(std::vector<Scenario>& all)
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

	// The same dawn with no restore of the device: the picture that the restore scenarios must give
	all.push_back({
	    .id = "light.sky_dawn_clock_runs",
	    .name = "The sky at dawn as the clock runs",
	    .facet = Facet::Light,
	    .description = "Six in the morning, and the clock runs. The camera looks low over the testbed's lake to the sky, "
	                   "past a birch on the shore.",
	    .expected = "The dome blends the sky of dawn into the next light, a band of rows at a time from the top down.",
	    .environment = {.hour = 6.0f, .clockRuns = true, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
	                .look = {k_Lake.x, 12.0f, k_NearShallows + 10.0f}},
	    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
	});

	for (const bool lost : {false, true})
	{
		all.push_back({
		    .id = lost ? "light.sky_dome_device_lost" : "light.sky_dome_device_restored",
		    .name = lost ? "The sky dome after a lost graphics device" : "The sky dome after the graphics device is made again",
		    .facet = Facet::Light,
		    .description = lost ? "Six in the morning, and the clock runs. After two seconds, at the next frame at which "
		                          "the dome is part way through a change to the new light, the graphics device is lost. "
		                          "The renderer finds the loss and makes the device again."
		                        : "Six in the morning, and the clock runs. After two seconds, at the next frame at which "
		                          "the dome is part way through a change to the new light, the renderer makes the "
		                          "graphics device again, as after a lost device.",
		    .expected = "The sky does not jump. The top of the dome keeps the new light and the bottom keeps the old "
		                "light, and the change continues down the dome as before. The log gives the rows of the dome "
		                "that had the new light.",
		    .environment = {.hour = 6.0f, .clockRuns = true, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
		                .look = {k_Lake.x, 12.0f, k_NearShallows + 10.0f}},
		    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
		    .commands = {{.kind = Kind::RestoreDeviceInDomeFollow, .delaySeconds = 2.0f, .value = lost ? 1U : 0U}},
		});
	}

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
	    .id = "light.snow_by_face",
	    .name = "Snow on the faces of trees and fences",
	    .facet = Facet::Light,
	    .description = "Deep snow lies over the whole island. Two trees and two fences stand on the land near the camera. "
	                   "A rock stands between them.",
	    .expected = "In about four seconds the snow shows on the trees and the fences. Each face of a tree or a fence "
	                "shows its snow as one flat area, with a sharp edge to the next face. The faces that point up show "
	                "the most snow. The rock shows its snow with soft changes across its faces.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 8.0f, -14.0f}, .look = {0.0f, 2.0f, 8.0f}},
	    .objects = {{.type = TreeInfo::Conifer, .offset = {-6.0f, 12.0f}, .scale = 1.5f},
	                {.type = TreeInfo::Oak, .offset = {7.0f, 14.0f}},
	                {.type = MobileStaticInfo::CeltFenceShort, .offset = {-3.0f, 4.0f}},
	                {.type = MobileStaticInfo::CeltFenceTall, .offset = {3.0f, 5.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, 8.0f}}},
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

	all.push_back({
	    .id = "light.haze_high_detail_villager",
	    .name = "No distance haze on a villager in high detail",
	    .facet = Facet::Light,
	    .description = "Two villagers stand in front of the camera. After six seconds, the cinema bars come in and the "
	                   "villager on the left changes to high detail. Use --detail-level 3 or more, which has the haze.",
	    .expected = "The villager on the left changes to its high-detail model. It keeps the light and the colour of the "
	                "land where it stands, and it shows no distance haze. At this distance the haze is small, so its "
	                "colours change only a little. The villager on the right does not change.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 3.0f, 40.0f}, .look = {0.0f, 1.5f, 60.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-3.0f, 60.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {3.0f, 60.0f}}},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 6.0f, .value = 1},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "SET_HIGH_GRAPHICS_DETAIL",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});

	// A villager of the north changes to its own detailed model on dry land, and one of another kind keeps its model
	all.push_back({
	    .id = "appearance.high_detail_villager_switch",
	    .name = "Villagers change to high detail where they stand",
	    .facet = Facet::Appearance,
	    .description = "A man of the north and a farmer stand on dry land in front of the camera. After six seconds, the "
	                   "cinema bars come in and the two villagers change to high detail.",
	    .expected = "Each villager shows its whole body where it stands and continues to stand or walk. The man of the "
	                "north changes to his detailed model, which has eyes. The farmer keeps his model.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 2.5f, 16.0f}, .look = {0.0f, 1.2f, 6.0f}},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = {-1.5f, 6.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {1.5f, 6.0f}}},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 6.0f, .value = 1},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "SET_HIGH_GRAPHICS_DETAIL",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_HIGH_GRAPHICS_DETAIL",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}}},
	});

	// A villager that the game makes on the water starts to drown at once, with the clip of its first state
	all.push_back({
	    .id = "appearance.villager_made_on_water",
	    .name = "A villager made on the water drowns from the start",
	    .facet = Facet::Appearance,
	    .description = "At noon, the clock stopped, the camera looks from the far shore of the lake of the testbed to "
	                   "its shallows. One farmer is made in the shallows, and one farmer is made on the dry land in "
	                   "front of the camera. Only the head of the farmer in the shallows is above the water.",
	    .expected = "The farmer in the shallows plays the drowning clip from the start, until his time in the water "
	                "ends. Then he dies and lies drowned. The farmer on the dry land plays the clip of a new villager "
	                "until he first decides what to do. No farmer stands still in the pose of a model with no clip.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cursor = glm::vec2(0.95f, 0.1f)},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lake.x + 6.0f, 4.5f, k_FarShallows + 20.0f},
	                .look = {k_Lake.x - 1.0f, 0.0f, k_FarShallows + 7.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {k_Lake.x - 2.0f, k_FarShallows + 4.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {k_Lake.x + 2.0f, k_FarShallows + 14.0f}}},
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
