/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the joint actions of two friends, each staged close up: a kiss, a talk, a joke, a dance,
// an argument, a mope, a show of its home, a call for attention, an order to go away, a look at another creature, a sit
// on a hill, a meal at a field, a throwing game, a gift of a toy and a visit to the citadel

#include <optional>
#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The two friends stand this far apart, and the camera stays near them
constexpr float k_Apart = 30.0f;
constexpr float k_CloseUp = 0.5f;
/// The script tells the tiger to do the action this long after the scenario starts, time to put the camera near
constexpr float k_ActionDelay = 14.0f;

CreatureSetup Asker()
{
	return {.label = "asks",
	        .species = CreatureType::Tiger,
	        .offset = {-k_Apart * 0.5f, 0.0f},
	        .facingDegrees = 90.0f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

/// A lion of no player, as the guide is on the first land
CreatureSetup Partner(glm::vec2 offset = {k_Apart * 0.5f, 0.0f}, CreatureType species = CreatureType::Lion,
                      std::string_view label = "friend")
{
	return {.label = label,
	        .species = species,
	        .offset = offset,
	        .facingDegrees = 270.0f,
	        .owner = PlayerNames::NEUTRAL,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

/// A script tells a creature to do an action at another creature, as a challenge script does
Command Do(std::string_view action, size_t at, float delay = k_ActionDelay, std::optional<size_t> with = std::nullopt,
           size_t creature = 0)
{
	return {
	    .kind = Kind::DoAction, .creature = creature, .delaySeconds = delay, .action = action, .with = with, .atCreature = at};
}

/// A script tells a creature to smile at another first, so that a script controls both and the other agrees
Command Smile(size_t creature, size_t at)
{
	return Do("SmileAtFriend", at, 0.5f, std::nullopt, creature);
}

/// A scenario of two friends: a script tells the lion to smile at the tiger, then tells the tiger to do the action
Scenario TwoFriends(std::string_view id, std::string_view name, std::string_view action, std::string_view expected)
{
	return {.id = id,
	        .name = name,
	        .facet = Facet::Mind,
	        .description = "A tiger and a lion of no player stand 30 apart and face each other. A script tells the lion "
	                       "to smile at the tiger, then tells the tiger to do the action with the lion.",
	        .expected = expected,
	        .environment = {.dispenserGrid = false},
	        .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	        .creatures = {Asker(), Partner()},
	        .commands = {Smile(1, 0), Do(action, 1)}};
}
} // namespace

void testbed_scenarios::AddFriendActionScenarios(std::vector<Scenario>& all)
{
	all.push_back(TwoFriends("friends.wave_at_creature", "Wave at another creature", "WaveAtObject",
	                         "The tiger walks with a smile until it is within three and a half times its height of the "
	                         "lion. It faces the lion for 1.4 seconds and waves at it. Only a creature can be the thing of "
	                         "this wave when the tiger does the action again."));
	all.push_back(TwoFriends("friends.kiss", "Kiss a friend", "KissFriend",
	                         "The tiger asks the lion to be its friend. The two walk to the two sides of the point halfway "
	                         "between them and turn to face each other. Each kisses the other. The taller one bends down "
	                         "to the lips of the other. Half the time the tiger then smiles at the lion and plays its "
	                         "happy action."));
	all.push_back(TwoFriends("friends.talk", "Talk to a friend", "TalkToFriend",
	                         "The tiger walks near the lion with a smile and faces it for 4 to 8 seconds. Then it asks the "
	                         "lion to be its friend. The lion turns to it. The tiger talks, then the lion talks."));
	all.push_back(TwoFriends("friends.joke", "Tell a friend a joke", "TellFriendAJoke",
	                         "The tiger walks near the lion with a smile and faces it for 4 to 8 seconds. Then it asks the "
	                         "lion to be its friend. The lion turns to it. The tiger talks, then the lion laughs."));
	all.push_back(TwoFriends("friends.dance", "Dance with a friend", "DanceWithFriend",
	                         "The tiger walks near the lion, faces it and plays its happy action. The lion stands still in "
	                         "its mind and turns to the tiger. Then both dance alone side by side for 20 seconds."));
	all.push_back(TwoFriends("friends.argue", "Argue with a friend", "ArgueWithFriend",
	                         "The two walk near each other and face each other. The tiger becomes angry, points at the "
	                         "lion and plays its angry action, and the lion plays its angry action. Both wait, then the "
	                         "tiger smiles and plays its happy action."));
	all.push_back(TwoFriends("friends.mope", "Mope about with a friend", "MopeAboutWithFriend",
	                         "The two walk near each other with sad faces and face each other. Both play the sad action. "
	                         "Half the time the tiger then plays its happy action."));
	all.push_back(TwoFriends("friends.show_off", "Show off to a friend", "ShowOffToFriend",
	                         "The two walk near each other and face each other. The tiger points at its home and grimaces. "
	                         "The lion turns to face that home. The tiger points at the lion, and the lion plays its "
	                         "confused action. Then the tiger plays its happy action."));

	// The lion dances with a cow, so that it is not free, and the tiger calls for the attention of the lion
	all.push_back({
	    .id = "friends.attention",
	    .name = "Get the attention of a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger stands 30 from a lion of no player. A cow of no player stands next to the lion. A script "
	                   "tells the cow to smile at the lion and the lion to dance with the cow. Then a script tells the "
	                   "tiger to get the attention of the lion.",
	    .expected = "The tiger turns to the lion. Half the time it points at the lion with a sad face, half the time it "
	                "waves, and a third of the time it plays the action that asks to be looked at. Then it plays its sad "
	                "action.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner(), Partner({k_Apart * 0.5f, 25.0f}, CreatureType::Cow, "cow")},
	    .commands = {Smile(2, 1), Do("DanceWithFriend", 2, k_ActionDelay, std::nullopt, 1),
	                 Do("GetAttentionFromFriend", 1, 6.0f)},
	});
	all.push_back(TwoFriends("friends.go_away", "Tell a creature to go away", "TellCreatureToSodOff",
	                         "Half the time the tiger fights the lion. Else it asks the lion to fight, holds the mind of "
	                         "the lion still, smiles at it and plays its angry action. It points past the lion and "
	                         "grimaces. The lion plays its sad action and walks to that place, and the tiger plays its "
	                         "angry action again."));
	all.push_back({
	    .id = "friends.examine",
	    .name = "Examine another creature with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger stands 30 from a lion of no player. A cow of no player stands between them, 25 away. A "
	                   "script tells the lion to smile at the tiger, then tells the tiger to examine the cow with the lion.",
	    .expected = "The tiger asks the lion to play and holds the mind of the lion still. Both turn to the cow and point "
	                "at it. Half the time each plays the happy action.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner(), Partner({0.0f, 25.0f}, CreatureType::Cow, "examined")},
	    .commands = {Smile(1, 0),
	                 {.kind = Kind::DoAction,
	                  .delaySeconds = k_ActionDelay,
	                  .action = "ExamineOtherCreatureWithFriend",
	                  .atCreature = 1,
	                  .usesCreature = 2}},
	});
	all.push_back(TwoFriends("friends.hill", "Sit on top of a hill with a friend", "SitOnTopOfHillWithFriend",
	                         "The tiger asks the lion to be its friend. Both walk to a clear place on the hill nearest the "
	                         "tiger and stand on its two sides. Both face down the slope and sit for 10 to 15 seconds."));
	all.push_back({
	    .id = "friends.eat_field",
	    .name = "Eat from a field with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 30 apart, with a ripe wheat field between them, 30 to the "
	                   "south. A script tells the lion to smile at the tiger, then tells the tiger to eat from the field "
	                   "with the lion.",
	    .expected = "The two walk to the two sides of the field, 10 from it, face it, take a handful of wheat each and eat "
	                "it. Neither brings the wheat back up. When the first of the two finishes its eat, the joint action "
	                "stops, so the tiger does not play its happy action.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner()},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -30.0f}, .ripeCrop = true}},
	    .commands = {Smile(1, 0), Do("EatFromFieldWithFriend", 1)},
	});
	all.push_back({
	    .id = "friends.throwing_game",
	    .name = "Play a throwing game with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 30 apart. A toy ball lies at the feet of the tiger. A "
	                   "script tells the lion to smile at the tiger, then tells the tiger to play a throwing game with "
	                   "the lion, with the ball.",
	    .expected = "The tiger asks the lion to play and holds the mind of the lion still. It picks up the ball and throws "
	                "it at the lion. Three seconds later it tells the lion to pick the ball up, and waits while the "
	                "lion walks to the ball. As the lion starts to pick it up, the tiger tells it to throw the ball back. "
	                "The lion has nothing in its hand yet, so the game stops. The lion then holds the ball.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner()},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-k_Apart * 0.5f, -8.0f}}},
	    .commands = {Smile(1, 0), Do("PlayThrowingGameWithFriend", 1, k_ActionDelay, 0)},
	});
	all.push_back({
	    .id = "friends.give_toy",
	    .name = "Give a toy to a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 30 apart. A toy ball lies at the feet of the tiger. A "
	                   "script tells the lion to smile at the tiger, then tells the tiger to give the ball to the lion.",
	    .expected = "The lion points at the tiger. The tiger picks up the ball, walks up to the lion and puts the ball "
	                "down. The lion picks it up. When the lion is free, it strokes, shakes, smells or examines the ball.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner()},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-k_Apart * 0.5f, -8.0f}}},
	    .commands = {Smile(1, 0), Do("GiveFriendToy", 1, k_ActionDelay, 0)},
	});
	all.push_back({
	    .id = "friends.citadel",
	    .name = "Show the citadel to a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 30 apart, with the temple of the player 120 to the south. "
	                   "A script tells the lion to smile at the tiger, then tells the tiger to show its citadel to the "
	                   "lion.",
	    .expected = "The tiger asks the lion to be shown something. Both walk near the temple. The tiger points at it and "
	                "grimaces. The lion turns to the tiger, then to the temple, and waits. The tiger looks at its feet.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2 {0.0f, -120.0f}},
	    .framing = {.shot = Shot::Overview, .distance = k_CloseUp},
	    .creatures = {Asker(), Partner()},
	    .commands = {Smile(1, 0), Do("ShowFriendCitadel", 1)},
	});
}
