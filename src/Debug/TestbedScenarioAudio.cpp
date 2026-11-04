/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the sounds, the voices and the music.

#include <array>

#include <SDL_scancode.h>

#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "ECS/IncomingMessageRules.h"
#include "Input/PlatformKeys.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddAudio(std::vector<Scenario>& all)
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
	    .id = "temple.save_room_scroll_time",
	    .name = "The temple's save game room writes its scroll again in each frame while the camera is close",
	    .facet = Facet::Story,
	    .description = "The game is saved into slot 0 with the name Testbed. Then the player goes through the temple's "
	                   "main room into the save game room. After four seconds the camera looks at the picture of slot 0.",
	    .expected = "While the camera is more than half way to the picture, the text of the scroll shows in yellow over "
	                "a black shadow in front of the scroll, and the parchment under it has no text. The time played on "
	                "the scroll moves on each second. When the camera moves back past half way, the text goes "
	                "back onto the parchment.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6},
	                 {.kind = Kind::LookAtSaveGamePicture, .delaySeconds = 4.0f, .value = 0}},
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
	    .id = "multiplayer.chat_friends",
	    .name = "Friends and others in the list of people of the chat",
	    .facet = Facet::Interface,
	    .description = "The chat opens. Its list of people has Bob, the god of the second player, under Players. It has "
	                   "the made-up friends Carol and Dave under Friends, and Eve, a person of the staff, under Others. "
	                   "Carol was seen now.",
	    .expected = "Under the line of the chat, the list shows three headings: Players, Friends and Others, each with "
	                "its speech button. Bob is selected and has a bar. Carol has an envelope at the right of her row. "
	                "Dave has no envelope. Eve is yellow and has a bar. Under the list, the button \"Make Friends\" shows, "
	                "because Bob is selected. Click the button: Bob goes to the top of the friends, and the button changes "
	                "to \"Make Not Friends\".",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .commands = {{.kind = Kind::Chat,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .value = k_ChatSays,
	                  .action = "Hello"},
	                 {.kind = Kind::Chat, .delaySeconds = 0.5f, .value = k_ChatFriends}},
	});

	all.push_back({
	    .id = "multiplayer.staff_line",
	    .name = "A line of the staff and an announcement in the gathering box",
	    .facet = Facet::Interface,
	    .description = "A ping server of the testbed sends a line from the admin of the server, and then an "
	                   "announcement of two lines.",
	    .expected = "The chat opens by itself. The incoming message at the bottom left says \"Staff: Welcome to the "
	                "server\". The list of people shows the heading Others, with Staff and the two lines of the "
	                "announcement as yellow names with a bar, the last line at the top.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .commands =
	        {{.kind = Kind::Chat, .delaySeconds = 1.0f, .value = k_ChatStaffSays, .action = "Welcome to the server"},
	         {.kind = Kind::Chat, .delaySeconds = 0.5f, .value = k_ChatAnnouncement, .action = "New maps this week|Have fun"}},
	});

	all.push_back({
	    .id = "multiplayer.staff_question",
	    .name = "A question of the staff with a Yes and a No",
	    .facet = Facet::Interface,
	    .description = "A ping server of the testbed sends a question from the admin of the server, with the secret word "
	                   "at its start.",
	    .expected = "A framed box of 700 by 250 opens in the middle of the screen. Its top says \"Staff Do you like the "
	                "game?\". At the bottom, the button \"Yes.\" is at the right and the button \"NO\" at the left. The "
	                "box shows no line to type into. The question does not show as a line of the chat. Click a button: "
	                "the box goes at once.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .commands =
	        {{.kind = Kind::Chat, .delaySeconds = 1.0f, .value = k_ChatStaffSays, .action = "!SaMuElGRDo you like the game?"}},
	});

	all.push_back({
	    .id = "multiplayer.staff_line_question",
	    .name = "A question of the staff that asks for a line",
	    .facet = Facet::Interface,
	    .description = "A ping server of the testbed sends a question from the admin of the server. A '!' after the "
	                   "secret word asks for a line.",
	    .expected = "A framed box of 700 by 250 opens in the middle of the screen. Its top says \"Staff Where do you "
	                "play from?\". Under it is a line to type into, and the button \"OK\" is at the bottom right. The "
	                "box has no No button. Type a line and press Return two times: the box goes at once.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .commands = {{.kind = Kind::Chat,
	                  .delaySeconds = 1.0f,
	                  .value = k_ChatStaffSays,
	                  .action = "!SaMuElGR!Where do you play from?"}},
	});

	all.push_back({
	    .id = "multiplayer.staff_line_bubble",
	    .name = "A line of the staff in the bubble of the player's creature",
	    .facet = Facet::Interface,
	    .description = "A ping server of the testbed sends a line from the admin of the server. The god of this computer "
	                   "has a tiger.",
	    .expected = "The chat opens by itself, and the incoming message at the bottom left says \"Staff: Welcome to the "
	                "server\". A bubble comes over the tiger with the line \"Staff: Welcome to the server\" in white, on "
	                "a box of half of the colour of the god. The bubble fades after some seconds.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -60.0f}, .look = {0.0f, 10.0f, 0.0f}},
	    .creatures = {{.label = "Alice's tiger",
	                   .species = CreatureType::Tiger,
	                   .offset = {0.0f, 0.0f},
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .size = 0.5f,
	                   .pauseMind = true}},
	    .commands = {{.kind = Kind::Chat, .delaySeconds = 1.0f, .value = k_ChatStaffSays, .action = "Welcome to the server"}},
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
	    .id = "interface.editor_key",
	    .name = "The editor key and F2",
	    .facet = Facet::Interface,
	    .description = "After a second the scenario presses the editor key (F10, or End on the web). One second later it "
	                   "presses the editor key again, and one second after that it presses F2, as the player does.",
	    .expected = "The first press opens the editor and the second press closes it. F2 does not open the editor. F2 "
	                "pauses the game, and the white box asks \"Are you sure you want to go to the Gods' Playground?\". "
	                "Click No to close the box.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))},
	                 {.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))},
	                 {.kind = Kind::PressPlainKey, .delaySeconds = 1.0f, .value = static_cast<size_t>(SDL_SCANCODE_F2)}},
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
	    .id = "interface.list_press_under_items",
	    .name = "A press under the last item of a list",
	    .facet = Facet::Interface,
	    .description = "After a second the menu opens on its Players page, with the list of the profiles of this "
	                   "computer. Press on a profile, then press in the list under the last profile. Hold the button "
	                   "down and move the pointer over the profiles and under them.",
	    .expected = "A press on a profile picks it, and a press under the last profile removes the pick, so no line "
	                "shows as picked. While the button stays down, the pick follows the pointer, and goes under the "
	                "last profile. A press on the scroll bar of a long list removes the pick until the button comes "
	                "up.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::OpenMenuPage, .delaySeconds = 1.0f, .value = 2}},
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

	all.push_back({
	    .id = "temple.tap_entry",
	    .name = "The hand's tap on the temple's entrance stops the world in its turn",
	    .facet = Facet::Story,
	    .description = "A script holds the dialogue with the cinema bars and shows two texts of the first lesson. The "
	                   "player's temple stands in the middle of the land. Then the player's hand taps the temple's "
	                   "entrance. Push Escape to leave the temple.",
	    .expected = "The game turn of the tap stops the help, sends the advisors home, closes the dialogue box and stops "
	                "the world, so no more turns play. The player goes into the temple's main room after the next drawn "
	                "frame. When the player leaves, the box shows its last texts again.",
	    .environment = {.temple = glm::vec2(0.0f)},
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::RunText, .delaySeconds = 0.5f, .value = 5128},
	                 {.kind = Kind::RunText, .delaySeconds = 1.0f, .value = 5129},
	                 {.kind = Kind::TapTempleEntrance, .delaySeconds = 5.0f}},
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
