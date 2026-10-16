/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of Creature Mode, the camera locked onto a creature with its status panel, and of the
// Creature Cave

#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureCave.h"
#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The cursor keys, by the camera keys command's value
constexpr size_t k_Left = 0;
constexpr size_t k_Up = 2;
constexpr size_t k_Down = 3;

CreatureSetup Player(glm::vec2 offset, std::string_view label)
{
	return {.label = label,
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .owner = PlayerNames::PLAYER_ONE,
	        .needs = {.energy = 0.8f, .exhaustion = 0.3f, .dehydration = 0.0f, .poo = 0.0f, .life = 0.9f}};
}

Command Walk(glm::vec2 point, float delay)
{
	return {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = delay, .point = point};
}

Command Key(Kind kind, float delay, size_t creature = 0)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay};
}

Command CameraKeys(size_t direction, float seconds, bool ctrl, float delay)
{
	return {
	    .kind = Kind::CameraKeys, .creature = 0, .delaySeconds = delay, .value = direction, .amount = seconds, .ctrl = ctrl};
}

void AddCreatureMode(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "creature_mode.enter",
	    .name = "Creature Mode with C",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger walks round a square; C is pressed after a second, and again after twenty.",
	    .expected = "The camera flies to the tiger over two seconds, keeping the way it was turned, about eight of the "
	                "tiger's heights away and looking at its middle, then follows it round the square, easing after it "
	                "a second behind. Near the top left of the screen the panel shows its damage (10%), hunger (20%) and "
	                "tiredness (30%), without the reward. The second C gives the camera back where it is.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {Key(Kind::CreatureKey, 1.0f), Walk({40.0f, 40.0f}, 1.0f), Walk({40.0f, 100.0f}, 5.0f),
	                 Walk({0.0f, 100.0f}, 5.0f), Walk({0.0f, 40.0f}, 5.0f), Key(Kind::CreatureKey, 5.0f)},
	});

	all.push_back({
	    .id = "creature_mode.other_god",
	    .name = "Double click another god's creature",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger and another god's lion. The lion is double clicked; later C is pressed.",
	    .expected = "The double click locks the camera onto the other god's lion, any god's creature can be followed, "
	                "and the panel shows the lion's status. C then moves the camera over to the player's own tiger, as "
	                "C only ever locks onto your creature; C again gives the camera back.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({-30.0f, 50.0f}, "yours"),
	                  CreatureSetup {.label = "another god's",
	                                 .species = CreatureType::Lion,
	                                 .offset = {30.0f, 50.0f},
	                                 .owner = PlayerNames::PLAYER_TWO,
	                                 .needs = {.energy = 0.5f, .exhaustion = 0.5f, .life = 0.5f}}},
	    .commands = {Key(Kind::DoubleClick, 1.0f, 1), Key(Kind::CreatureKey, 8.0f), Key(Kind::CreatureKey, 8.0f)},
	});

	all.push_back({
	    .id = "creature_mode.orbit",
	    .name = "Orbit with Shift and the cursor keys",
	    .facet = Facet::CreatureMode,
	    .description = "Locked onto the player's tiger, Shift and left are held for four seconds, Shift and up for one, "
	                   "Shift and down for one, then Ctrl and up for one and Ctrl and down for two.",
	    .expected = "Shift and left swing the camera round the tiger, half a turn in about four seconds on a screen 2560 "
	                "wide (400 pixels a second, a screen's width a half turn); Shift and up tilt it to look down more "
	                "steeply, and down back again, no lower than about 14 degrees; Ctrl and up draw it in, Ctrl and down "
	                "out. The cursor keys alone would give the camera back.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {Key(Kind::CreatureKey, 1.0f), CameraKeys(k_Left, 4.0f, false, 3.0f), CameraKeys(k_Up, 1.0f, false, 5.0f),
	                 CameraKeys(k_Down, 1.0f, false, 2.0f), CameraKeys(k_Up, 1.0f, true, 2.0f),
	                 CameraKeys(k_Down, 2.0f, true, 2.0f)},
	});

	// A wall of trees between the camera and the tiger
	std::vector<ObjectSetup> trees;
	for (int i = -3; i <= 3; ++i)
	{
		trees.push_back({.type = TreeInfo::Conifer, .offset = {static_cast<float>(i) * 8.0f, 25.0f}, .scale = 1.5f});
	}
	all.push_back({
	    .id = "creature_mode.obstruction",
	    .name = "Ctrl and Shift clear the view",
	    .facet = Facet::CreatureMode,
	    .description = "Locked onto the player's tiger behind a wall of conifers, Ctrl and Shift are pressed together.",
	    .expected = "The camera swings round to the heading, of 32 round the tiger, along which the land falls away "
	                "furthest below its middle, favouring the heading it has, and tilts to a little over 22 degrees plus "
	                "a fifth of its pitch, between 22.5 and 60. As in the game it reads only the land's heights: on the "
	                "testbed's level plane it keeps its heading and only tilts, and the trees aren't looked at.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .objects = trees,
	    .commands = {Key(Kind::CreatureKey, 1.0f), Key(Kind::ClearCameraView, 4.0f)},
	});

	all.push_back({
	    .id = "creature_mode.pass_out",
	    .name = "Passes out at 100% and comes round in its pen",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger, its tiredness at 100%, away from the middle of the testbed with the camera "
	                   "locked onto it.",
	    .expected = "With the panel's tiredness at 100% the tiger passes out where it stands: it lies out cold for "
	                "four times its size and eight seconds, fizzes out of sight through static over two seconds with the "
	                "teleport's energise sound and back in over two at its pen, which on the testbed, with no temple, is the "
	                "middle of the land; the camera follows it there. It lies a few seconds, rests until its life is 40% "
	                "and its tiredness 30%, and gets up.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.label = "yours",
	                                 .species = CreatureType::Tiger,
	                                 .offset = {-60.0f, 60.0f},
	                                 .facingDegrees = 270.0f,
	                                 .owner = PlayerNames::PLAYER_ONE,
	                                 .needs = {.energy = 1.0f, .exhaustion = 1.0f, .dehydration = 0.0f, .poo = 0.0f}}},
	    .commands = {Key(Kind::CreatureKey, 0.5f)},
	});

	all.push_back({
	    .id = "creature_mode.fizz_static_off_screen",
	    .name = "A fizzing creature's static holds still while off the screen",
	    .facet = Facet::CreatureMode,
	    .description = "Two tigers are knocked out at the same time. The camera looks north at the one in front of it. The "
	                   "other lies behind the camera.",
	    .expected = "Each tiger lies out cold, then fizzes out of sight through static over two seconds and back in at "
	                "its home. In the inspector, the staticScroll of the tiger in view slides while it fizzes. The "
	                "staticScroll of the tiger behind the camera stays where it was, as the game slides the static only "
	                "on the frames that draw the body on the screen.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, 0.0f}, .look = {0.0f, 2.0f, 40.0f}},
	    .creatures = {CreatureSetup {.label = "in view",
	                                 .species = CreatureType::Tiger,
	                                 .offset = {0.0f, 40.0f},
	                                 .facingDegrees = 270.0f,
	                                 .owner = PlayerNames::PLAYER_ONE},
	                  CreatureSetup {.label = "behind",
	                                 .species = CreatureType::Tiger,
	                                 .offset = {0.0f, -60.0f},
	                                 .facingDegrees = 90.0f,
	                                 .owner = PlayerNames::PLAYER_TWO}},
	    .commands = {Command {.kind = Kind::KnockOut, .creature = 0, .delaySeconds = 1.0f},
	                 Command {.kind = Kind::KnockOut, .creature = 1, .delaySeconds = 0.0f}},
	});
}

/// A tiger that fizzes half out of sight in front of houses and trees, which stand further from the camera. The camera
/// looks over the tiger at them.
void AddFizzInFront(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> behind;
	for (int i = -2; i <= 2; ++i)
	{
		behind.push_back({.type = AbodeInfo::CelticHut, .offset = {static_cast<float>(i) * 14.0f, 30.0f}});
		behind.push_back({.type = TreeInfo::Conifer, .offset = {(static_cast<float>(i) * 14.0f) + 7.0f, 40.0f}, .scale = 1.5f});
	}
	all.push_back({
	    .id = "creature_mode.fizz_in_front",
	    .name = "A fizzing creature in front of houses and trees",
	    .facet = Facet::CreatureMode,
	    .description =
	        "The player's tiger stands in front of a row of houses and conifers. It fizzes a third of the way out of sight "
	        "over two seconds, and stays there.",
	    .expected = "Where the static shows the tiger, the houses and trees behind it do not show through its body. The "
	                "land behind the tiger shows through. The houses and trees show where the static has holes. The "
	                "static moves over the tiger.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.label = "yours",
	                                 .species = CreatureType::Tiger,
	                                 .offset = {0.0f, 10.0f},
	                                 .facingDegrees = 90.0f,
	                                 .owner = PlayerNames::PLAYER_ONE,
	                                 .needs = {.energy = 0.8f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f}}},
	    .objects = behind,
	    .commands = {{.kind = Kind::SetFizz, .creature = 0, .delaySeconds = 1.0f, .value = 2000, .amount = 0.3f}},
	});
}

void AddCave(std::vector<Scenario>& all)
{
	auto tattoo = [](Kind kind, size_t design, size_t site, float delay) {
		return Command {.kind = kind, .creature = 0, .delaySeconds = delay, .value = design, .bodyPart = site};
	};
	all.push_back({
	    .id = "creature_mode.cave_tattoo",
	    .name = "The Creature Cave and its tattoos",
	    .facet = Facet::CreatureMode,
	    .description = "F5 is pressed for the player's tiger, on the tattoo page; a symbol is tattooed on its head, "
	                   "another on its chest, and the head's taken off again.",
	    .expected = "The temple opens on the creature's room, with its four scrolls telling of the tiger: its attributes, "
	                "the actions it has learnt, its mind and its miracles. The cave's screen stands at the right with "
	                "the same pages, what it has learnt to do and not to do, and its tattoos: the symbols appear on the "
	                "tiger's head and chest, and the head's goes again (seen on the tiger once out of the temple).",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_cave::Page::Tattoos)},
	                 tattoo(Kind::ApplyTattoo, 4, 1, 3.0f),
	                 tattoo(Kind::ApplyTattoo, 9, 2, 2.0f),
	                 tattoo(Kind::RemoveTattoo, 0, 1, 4.0f)},
	});
	all.push_back({
	    .id = "creature_mode.tattoo_editor",
	    .name = "The tattoo editor",
	    .facet = Facet::CreatureMode,
	    .description = "The tattoo editor is opened on the player's tiger, as clicking it in the Creature Cave does; a "
	                   "colour is picked from the palette and darkened on the brightness bar (the pointer's points are "
	                   "for a screen of 1600 by 900).",
	    .expected = "Over the scene: the line of help at the top, the sixteen symbols in two columns down the sides, the "
	                "palette at the far left and the brightness bar at the far right, OK at the bottom left, Cancel at "
	                "the bottom right and the round rotation button between them. Picking from the palette colours "
	                "the bar's middle, and the symbols take the bar's colour at its arrow.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::OpenTattooEditor, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {431.0f / 1600.0f, 270.0f / 900.0f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.2f, .value = 1},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 1},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.3f, .point = {1130.0f / 1600.0f, 530.0f / 900.0f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.2f, .value = 1},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 1}},
	});
}
/// The player puts a tattoo on the creature in the Creature Cave and takes it off again, as the player does with the
/// pointer: the points after the click on the creature are for a screen of 1600 by 900
void AddCaveTattooOnBody(std::vector<Scenario>& all)
{
	const auto at = [](float x, float y, float delay) {
		return Command {.kind = Kind::PointerTo, .delaySeconds = delay, .point = {x / 1600.0f, y / 900.0f}};
	};
	const auto press = [](float delay) { return Command {.kind = Kind::PointerPress, .delaySeconds = delay, .value = 1}; };
	const auto release = [](float delay) { return Command {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = 1}; };
	all.push_back({
	    .id = "creature_mode.cave_tattoo_on_body",
	    .name = "A tattoo dragged onto the creature in the cave",
	    .facet = Facet::CreatureMode,
	    .description = "The player goes into the temple and its Creature Cave and clicks the tiger by the fire. The "
	                   "player picks a blue, drags the heart onto the tiger's chest, then lifts it off and drops it away.",
	    .expected = "The camera zooms onto the tiger and the tattoo editor opens; the tiger gets up and turns to the "
	                "camera. While the heart is over the chest it flashes, and a small black square marks the chest "
	                "while the button is down. Let go there, the blue heart goes into the skin of the chest and the "
	                "tiger smiles for 1.8 seconds. Pressed on the chest, the heart comes off with the tiger sad for 1.8 "
	                "seconds, and dropped away from the body it stays off.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerOnCaveCreature, .delaySeconds = 10.0f},
	                 press(0.5f),
	                 release(0.2f),
	                 // The palette's blue, then the heart to the chest
	                 at(246.0f, 330.0f, 5.0f),
	                 press(0.2f),
	                 release(0.2f),
	                 at(368.0f, 780.0f, 0.5f),
	                 press(0.2f),
	                 at(600.0f, 600.0f, 0.2f),
	                 at(799.0f, 404.0f, 0.2f),
	                 release(1.0f),
	                 // The heart off the chest and away from the body
	                 at(799.0f, 404.0f, 3.0f),
	                 press(0.2f),
	                 at(700.0f, 350.0f, 0.2f),
	                 at(560.0f, 300.0f, 0.2f),
	                 release(1.0f)},
	});
}

/// The main menu's tattoo editor over the menu, with the creature it shows. The points are for a screen of 1600 by 900.
void AddMenuTattooEditor(std::vector<Scenario>& all)
{
	const auto at = [](float x, float y, float delay) {
		return Command {.kind = Kind::PointerTo, .delaySeconds = delay, .point = {x / 1600.0f, y / 900.0f}};
	};
	const auto press = [](float delay) { return Command {.kind = Kind::PointerPress, .delaySeconds = delay, .value = 1}; };
	const auto release = [](float delay) { return Command {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = 1}; };
	all.push_back({
	    .id = "creature_mode.menu_tattoo_editor",
	    .name = "The tattoo editor from the main menu",
	    .facet = Facet::CreatureMode,
	    .description = "The player opens the game's menu, goes to the Players page and clicks Edit Tattoo. The player "
	                   "picks a blue and drags the heart onto the creature's chest, then turns the view with the round "
	                   "button.",
	    .expected = "The menu fades out and the tattoo editor fades in with a white frame of its own. The profile's "
	                "creature, or a Giant Ape when the profile keeps no creature, stands in the middle over the world, "
	                "seen from in front and above. Its hair hangs from it and swings as it moves. It fidgets now and "
	                "then, looks at the pointer for a second and a half after a press, and the blue heart goes into the skin "
	                "of its chest. The view turns round it. "
	                "Cancel takes the tattoo off again and the menu comes back.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::OpenTattooMenu, .delaySeconds = 1.0f},
	                 at(246.0f, 330.0f, 3.0f),
	                 press(0.2f),
	                 release(0.2f),
	                 at(368.0f, 780.0f, 0.5f),
	                 press(0.2f),
	                 at(600.0f, 600.0f, 0.2f),
	                 at(816.0f, 470.0f, 0.2f),
	                 release(1.0f),
	                 at(800.0f, 760.0f, 2.5f),
	                 press(0.2f),
	                 at(900.0f, 760.0f, 1.5f),
	                 release(0.5f)},
	});
}

/// The player has a picture of their own for the heart and puts the heart on the creature in the Creature Cave. The
/// points after the click on the creature are for a screen of 1600 by 900.
void AddCaveOwnTattooPicture(std::vector<Scenario>& all)
{
	const auto at = [](float x, float y, float delay) {
		return Command {.kind = Kind::PointerTo, .delaySeconds = delay, .point = {x / 1600.0f, y / 900.0f}};
	};
	const auto press = [](float delay) { return Command {.kind = Kind::PointerPress, .delaySeconds = delay, .value = 1}; };
	const auto release = [](float delay) { return Command {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = 1}; };
	all.push_back({
	    .id = "creature_mode.cave_own_tattoo_picture",
	    .name = "A tattoo of the player's own picture",
	    .facet = Facet::CreatureMode,
	    .description = "The player has a picture of their own for the eighth symbol, the heart, as from Symbol8.bmp in "
	                   "the folder of their profile: grey rings. The player goes into the Creature Cave, clicks the "
	                   "tiger, picks a blue and drags the heart onto the tiger's chest.",
	    .expected = "The symbol in the editor still shows the heart, because the game loads the symbols' picture "
	                "one time. The tattoo that goes into the skin of the chest is blue rings, not a heart.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::OwnTattooPicture, .delaySeconds = 0.5f, .value = 7},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerOnCaveCreature, .delaySeconds = 10.0f},
	                 press(0.5f),
	                 release(0.2f),
	                 at(246.0f, 330.0f, 5.0f),
	                 press(0.2f),
	                 release(0.2f),
	                 at(368.0f, 780.0f, 0.5f),
	                 press(0.2f),
	                 at(600.0f, 600.0f, 0.2f),
	                 at(799.0f, 404.0f, 0.2f),
	                 release(1.0f)},
	});
}

/// The player looks at a scroll of the Creature Cave and then leaves it. The points are for a screen of 1600 by 900.
void AddCaveScrollZoom(std::vector<Scenario>& all)
{
	const auto at = [](float x, float y, float delay) {
		return Command {.kind = Kind::PointerTo, .delaySeconds = delay, .point = {x / 1600.0f, y / 900.0f}};
	};
	const auto press = [](float delay) { return Command {.kind = Kind::PointerPress, .delaySeconds = delay, .value = 1}; };
	const auto release = [](float delay) { return Command {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = 1}; };
	all.push_back({
	    .id = "creature_mode.cave_scroll_zoom",
	    .name = "A scroll of the creature cave, looked at and left",
	    .facet = Facet::CreatureMode,
	    .description = "The player goes into the temple and its Creature Cave. The player clicks the Creature Mind scroll, "
	                   "waits, then presses the key that turns the camera to the left.",
	    .expected = "The camera moves along a curve to the front of the scroll in about 1.3 seconds. At half way the "
	                "text moves from the scroll to a plane in front of it, sharp. The help of the scroll starts. The "
	                "key stops the help, and the camera goes back in about 1.4 seconds. At half way the "
	                "text moves back onto the scroll.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f},
	                 at(940.0f, 300.0f, 10.0f),
	                 press(0.5f),
	                 release(0.2f),
	                 Command {.kind = Kind::PressKey,
	                          .delaySeconds = 6.0f,
	                          .value = static_cast<size_t>(input::BindableActionMap::MOVE_LEFT)}},
	});
}

/// The copy of the player's ape in the Creature Cave, which has hair of its own
void AddCaveCopyHair(std::vector<Scenario>& all)
{
	auto ape = Player({0.0f, 40.0f}, "yours");
	ape.species = CreatureType::GiantApe;
	all.push_back({
	    .id = "creature_mode.cave_copy_hair",
	    .name = "The hair of the creature in the cave",
	    .facet = Facet::CreatureMode,
	    .description = "The player has an ape. The player goes into the temple and its Creature Cave, and stays there.",
	    .expected = "The ape sits or sleeps by the fire with its hair on, as in the world. The hair starts straight out "
	                "from the roots, falls, and then swings as the ape moves, while the world stays paused.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {ape},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.5f}},
	});
}

/// The Creature Cave's trophies for creatures of different histories: the belts on the attack dummies for how each leans
/// in its fights, the medals on the magic plinths and the seeds hovering by them for the miracles it has learnt
void AddCaveTrophies(std::vector<Scenario>& all)
{
	const auto lean = [](float amount, float delay) {
		return Command {.kind = Kind::SetFightLean, .creature = 0, .delaySeconds = delay, .amount = amount};
	};
	const auto seen = [](MagicType miracle, float times) {
		return Command {
		    .kind = Kind::SetMiracleSightings, .creature = 0, .value = static_cast<size_t>(miracle), .amount = times};
	};
	// The temple opens its rooms once the player was in its main room
	const auto main = [](float delay) {
		return Command {.kind = Kind::EnterTemple, .delaySeconds = delay, .value = static_cast<size_t>(TempleRoom::Main)};
	};
	const auto cave = [](float delay) {
		return Command {.kind = Kind::OpenCreatureCave,
		                .creature = 0,
		                .delaySeconds = delay,
		                .value = static_cast<size_t>(creature_cave::Page::Attributes)};
	};
	all.push_back({
	    .id = "creature_mode.cave_trophies_novice",
	    .name = "Cave trophies: a creature that never fought or learnt",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger has never fought and has seen no miracle; F5 is pressed.",
	    .expected = "The creature's room shows on each attack dummy the belts of an even balance: white, yellow and blue "
	                "at their fourth and green at its second, on both rows. No medal stands on the plinths and no seed "
	                "hovers by them.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {main(1.0f), cave(0.5f)},
	});
	all.push_back({
	    .id = "creature_mode.cave_trophies_aggressive",
	    .name = "Cave trophies: an aggressive fighter that knows its miracles",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger leans all the way to attack in its fights, has learnt the fireball, the "
	                   "heal and the water miracles in full and has seen food and the forest a few times; F5 is pressed.",
	    .expected = "One row of belts is full, white to black at its fourth, and the other row is empty. The plinths "
	                "show a medal for all the miracles together and one each for the four best learnt, gems for the "
	                "three learnt in full, shining; the seeds of the fireball, the heal, the water and the fourth best "
	                "hover by the plinths, spinning, with their holders' effects.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {lean(1.0f, 1.0f), seen(MagicType::Fireball, 1000.0f), seen(MagicType::Heal, 1000.0f),
	                 seen(MagicType::Water, 1000.0f), seen(MagicType::Food, 3.0f), seen(MagicType::Forest, 1.0f), main(0.5f),
	                 cave(0.5f)},
	});
	all.push_back({
	    .id = "creature_mode.cave_trophies_defensive",
	    .name = "Cave trophies: a defensive fighter with flocks and a phial",
	    .facet = Facet::CreatureMode,
	    .description = "The player's tiger leans well to defence (-0.6), and has learnt the flying flock and the "
	                   "freeze spell in full and the lightning half way; F5 is pressed.",
	    .expected = "The defence row has 27 levels: white to red at their fourth and purple at its second; the attack "
	                "row has six: white at its fourth and yellow at its first. The flock's seed hovers as a dove (or a "
	                "bat for an evil god), the freeze spell's phial pulses icy, and the lightning's seed hovers third.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f}, "yours")},
	    .commands = {lean(-0.6f, 1.0f), seen(MagicType::FlockFlying, 1000.0f), seen(MagicType::CreatureSpellFreeze, 1000.0f),
	                 seen(MagicType::LightningBolt, 5.0f), main(0.5f), cave(0.5f)},
	});
}
} // namespace

void testbed_scenarios::AddCreatureModeScenarios(std::vector<Scenario>& all)
{
	AddCreatureMode(all);
	AddCave(all);
	AddFizzInFront(all);
	AddCaveTattooOnBody(all);
	AddCaveOwnTattooPicture(all);
	AddMenuTattooEditor(all);
	AddCaveScrollZoom(all);
	AddCaveCopyHair(all);
	AddCaveTrophies(all);
}
