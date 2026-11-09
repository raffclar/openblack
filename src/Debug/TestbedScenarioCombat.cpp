/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the fights of the creatures.

#include <SDL_scancode.h>
#include <glm/gtc/constants.hpp>

#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Input/BindableActions.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
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
} // namespace

void testbed_scenarios::AddCombat(std::vector<Scenario>& all)
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
	                "blows high, in the middle and low with their reels, blocks and steps. The panel at the top left "
	                " shows both fight healths falling. The loser faints, and the winner finishes and shows "
	                "off.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.blow_no_wobble",
	    .name = "A struck tiger reels with no wobble on top",
	    .facet = Facet::Combat,
	    .description = "Two tigers duel by themselves, the red one stronger, so that blows land on both bodies.",
	    .expected = "Each blow that lands makes the struck body reel high, in the middle or low. No second clip shakes "
	                "the body on top of the reel. The animation query shows only the body and blend layers, and maybe a "
	                "face or a gesture.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The first land's Ogre, and creatures on later lands, heal by a script after they win a fight: the script gives the
	// victor a hundredth of life every six seconds, until it is full or it fights again
	const auto healScript = [](float delay) {
		return Command {.kind = Kind::RunScript,
		                .delaySeconds = delay,
		                .script = "HealFightVictorOverTime",
		                .scriptArguments = {{.kind = ScriptArgument::Kind::Creature, .index = 0}}};
	};
	const auto victor = [&](PlayerNames owner) {
		auto creature = red(owner, 0.6f, -0.45f);
		creature.label = "victor";
		creature.needs.life = 0.9f;
		creature.hold = false;
		return creature;
	};
	all.push_back({
	    .id = "combat.victor_heals_over_time",
	    .name = "A fight victor heals slowly after the fight",
	    .facet = Facet::Combat,
	    .description = "A tiger with nine tenths of its life. Half a second in, the game's script that heals a creature "
	                   "after it wins a fight starts on it, as the first land's Ogre quest starts it when the Ogre wins.",
	    .expected = "The life of the tiger increases by 0.01 at once and then by 0.01 every six seconds: 0.91, 0.92 after "
	                "six seconds, and so on. When the life is 1 or more, the script stops.",
	    .framing = ringside,
	    .creatures = {victor(PlayerNames::PLAYER_ONE)},
	    .commands = {healScript(0.5f)},
	    .checks =
	        {{.label = "the first hundredth at once",
	          .quantity = checks::Quantity::CreatureLife,
	          .afterTurns = 20,
	          .atLeast = 0.905,
	          .atMost = 0.915},
	         {.label = "a hundredth more after six seconds",
	          .quantity = checks::Quantity::CreatureLife,
	          .afterTurns = 80,
	          .atLeast = 0.915,
	          .atMost = 0.925},
	         {.label = "full after a minute", .quantity = checks::Quantity::CreatureLife, .afterTurns = 620, .atLeast = 0.999}},
	});
	all.push_back({
	    .id = "combat.victor_heal_stops_in_fight",
	    .name = "A fight victor stops healing when it fights again",
	    .facet = Facet::Combat,
	    .description = "As combat.victor_heals_over_time, with a second tiger. Nine seconds in, the healed tiger asks the "
	                   "other tiger to fight, and both fight by themselves.",
	    .expected = "The life of the red tiger increases by 0.01 every six seconds while the tigers walk to the arena "
	                "and take their places, because for the scripts a creature fights only while it duels. At the first "
	                "check of the script in the duel, the script stops: the life gets no more hundredths from it. The "
	                "inspector's script.tasks query no longer lists the script.",
	    .framing = ringside,
	    .creatures = {victor(PlayerNames::PLAYER_ONE), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {healScript(0.5f), Fight(Kind::StartFight, 0, 1, 8.5f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
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

	all.push_back({
	    .id = "combat.life_pays_at_each_stop",
	    .name = "A leash order in the duel costs life at each stop",
	    .facet = Facet::Combat,
	    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. In the duel the red "
	                   "tiger gets the rope leash, and a second later the player tells it with the leash to go to a point "
	                   "to the south.",
	    .expected = "The order calls the fight off with no winner. The fight transcript has a record fight_end for each "
	                "tiger: when its body is in its stance, a move, a block or a cast, the life of the red tiger pays two "
	                "times (life_payments 2) and the life of the blue tiger three times. A tiger that finishes pays "
	                "nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f), Leash(0, LeashType::Rope, 18.0f),
	                 Go(Kind::LeashOrderAt, 0, {-25.0f, 0.0f}, 1.0f, false)},
	});

	all.push_back({
	    .id = "combat.life_pays_after_the_fight",
	    .name = "A stroke just after a call-off costs life again",
	    .facet = Facet::Combat,
	    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. In the duel the red "
	                   "tiger gets the rope leash, and a second later the player tells it with the leash to go to a point "
	                   "to the south. At once after the order, the player strokes the blue tiger.",
	    .expected = "The order calls the fight off with no winner. The stroke stops the blue tiger again. While its body "
	                "still plays its stance, a move, a block or a cast, its life pays one time more: the fight transcript "
	                "has a level record for its life with why stopped_again and life_payments 1. A body that finishes "
	                "pays nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f), Leash(0, LeashType::Rope, 18.0f),
	                 Go(Kind::LeashOrderAt, 0, {-25.0f, 0.0f}, 1.0f, false), Act(Kind::Stroke, 1, 0.0f)},
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

	all.push_back({
	    .id = "saves.loaded_arena_ring",
	    .name = "The arena of a loaded saved game of the original game makes a white ring",
	    .facet = Facet::Combat,
	    .description = "An arena of radius 30 m, as an arena after the load of a saved game of the original game, is put "
	                   "midway between two tigers. It has no ring. 1 second later the two tigers are told to fight, and "
	                   "both fight by themselves.",
	    .expected = "Before the fight no ring stands round the arena. When the fight takes the arena, a ring of light "
	                "appears round it with its sound. The ring is white, not the grey of the ring of an arena that the land "
	                "or a fight makes. It has the same shape, height and stars.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {{.kind = Kind::PutLoadedArena, .creature = 0, .delaySeconds = 0.5f, .value = 1, .radius = 30.0f},
	                 Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
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

	all.push_back({
	    .id = "combat.fight_view_follows_bookmark",
	    .name = "A followed bookmark in the fight view",
	    .facet = Facet::Combat,
	    .description = "As Watching a fight. Three seconds after the tigers are told to fight, the player keeps the blue "
	                   "tiger in bookmark 4, as Ctrl and the key 4 do with the hand over it, and the camera follows it, as "
	                   "Shift and the key 4 do. The fight view is a part of the world camera.",
	    .expected = "The follow stays on in the fight view: each turn the camera flies again towards the view of bookmark "
	                "4 on the blue tiger, until the player moves the camera. A key that turns, tilts or zooms the camera, "
	                "or a drag of more than two pixels with the land gripped, stops the follow.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::BookmarkCreature, .creature = 1, .delaySeconds = 3.0f, .value = 3}},
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
	    .id = "combat.fight_view_keys_held",
	    .name = "A fight starts while the left arrow key moves the camera",
	    .facet = Facet::Combat,
	    .description = "As Watching a fight, but the left arrow key (Move Left) goes down half a second before the tigers "
	                   "are told to fight, and stays down for three seconds. In a fight the arrow keys turn the camera.",
	    .expected = "Before the fight the camera moves to the left. In the first step of the fight the keys of the last "
	                "step still move the camera to the left, and the view of the fight does not follow the fighters in "
	                "that step. Then the arrow key turns the view round the fighters, and the view follows them.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Command {
	                     .kind = Kind::PlainKeyDown, .delaySeconds = 0.5f, .value = static_cast<size_t>(SDL_SCANCODE_LEFT)},
	                 Fight(Kind::StartFight, 0, 1, 0.5f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Command {.kind = Kind::PlainKeyUp, .delaySeconds = 2.3f, .value = static_cast<size_t>(SDL_SCANCODE_LEFT)}},
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
		    .id = "combat.view_start_distance",
		    .name = "The fight view starts as far from the arena as the follow camera's clear view",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger and a blue tiger fight by themselves. The camera looks down on the "
		                   "arena from high above it. The camera shows the fight when the fighters have made their arena.",
		    .expected = "The camera flies to the side of the arena from the best heading. The flight ends between 40 "
		                "and 50 from the middle of a small arena: most of the way out to 50. Then the camera follows "
		                "the duel.",
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
		    .expected = "On its next turn, the red tiger wins. It finishes its move and stands. An advisor says one of the "
		                "lines for good news, because the place of the red tiger is on screen. The blue tiger does not "
		                "faint. It stays in its stance and faces the red tiger. Two turns after the red tiger stands, the "
		                "red tiger orders the blue tiger to respond and plays no response. The blue tiger finishes its "
		                "move, plays the sad response and fears the red tiger. The fight view ends a few seconds after the "
		                "win. The fight transcript shows a win with by_blow false and order_respond.",
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

	{
		// The same win by the life of the opponent, but after the win the script gives the blue tiger its life back and
		// halves the life of the red tiger, so the blue tiger has more life when it responds
		auto winner = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		winner.hold = false;
		auto loser = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		loser.hold = false;
		const auto setLife = [](size_t creature, float life, float delay) {
			return Command {.kind = Kind::CallNative,
			                .delaySeconds = delay,
			                .script = "SET_PROPERTY",
			                .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
			                                     .number = static_cast<float>(script::ObjectPropertyType::Health)},
			                                    {.kind = ScriptArgument::Kind::Creature, .index = creature},
			                                    {.kind = ScriptArgument::Kind::Number, .number = life}}};
		};
		all.push_back({
		    .id = "combat.loser_waits_free_mind",
		    .name = "The loser waits after its response until its own mind takes a plan",
		    .facet = Facet::Combat,
		    .description = "The red tiger of the player and a blue tiger fight by themselves, and the creatures do not faint. "
		                   "When the duel is on, a script sets the life of the blue tiger to zero. A moment after the red "
		                   "tiger wins, the script gives the blue tiger full life and sets the life of the red tiger to 0.5.",
		    .expected =
		        "The red tiger wins, finishes its move, orders the blue tiger to respond and plays no response. "
		        "The blue tiger finishes its move, plays the happy response and does not fear the red tiger. Then "
		        "it waits with its mind free. Its own plans do not take the place of the order, because each is "
		        "worth less than two times the order. The red tiger goes home to get better. An order of another "
		        "creature, such as to share a smile, can still take its place. The fight transcript shows order_respond.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {winner, loser},
		    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 setLife(1, 0.0f, 18.0f),
		                 setLife(1, 1.0f, 0.3f),
		                 setLife(0, 0.5f, 0.0f)},
		});
	}

	all.push_back({
	    .id = "combat.held_in_duel",
	    .name = "The hand holds a creature in its duel",
	    .facet = Facet::Combat,
	    .description = "The red tiger of the player and a blue tiger fight by themselves. When the duel is on, the hand "
	                   "holds the red tiger and strokes its head. Four seconds later the hand lets go.",
	    .expected = "When the hand takes hold, the red tiger leaves the duel. It finishes its move while the hand holds "
	                "it. Then it turns to the camera and shows what it wants. The blue tiger stays in its stance and "
	                "faces the red tiger. When the hand lets go, both tigers stop with no response. The fight transcript "
	                "shows held_by_hand, let_go_by_hand and partner_stopped.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Act(Kind::HandLock, 0, 18.0f),
	                 {.kind = Kind::HandStroke, .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 4.0f)},
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
		    .id = "combat.blow_speed",
		    .name = "A charge makes a blow stronger but not faster",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a blue tiger that fights by itself. The red tiger gets a blow "
		                   "in the middle with no charge. Four seconds later it gets a blow in the middle with the full "
		                   "charge.",
		    .expected = "The two blows of the red tiger play at the same speed. The blow with the full charge takes "
		                "more fight health from the blue tiger. The blows of the blue tiger also play at this speed. "
		                "The original game does the same.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 14.0f, 0.0f),
		                 Fight(Kind::FightBlow, 0, k_Mid, 4.0f, 1200.0f)},
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
