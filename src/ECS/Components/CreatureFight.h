/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureTurnToFace.h"

namespace openblack::ecs::components
{

/// A creature in a fight, from the face-off with its opponent until it has finished showing off or walked off. Its mind
/// leaves its body alone meanwhile. The creature that started the fight by its own plan can still choose a plan that it
/// wants much more. The other creature obeys it and does not think for itself until the fight is over.
///
/// The creature that starts the fight leads the steps before the duel, and tells the other creature when to do each one:
/// 1. The first creature asks the other creature. On the next turn, it tells the mind of the other creature to stand
///    still. On the turn after, it tells the other creature to stop and obey it. Until then, the other creature
///    continues with what it did.
/// 2. Both turn to face each other.
/// 3. Both taunt or make the rude gesture.
/// 4. The arena appears and the first creature points at it.
/// 5. Both walk to their places, and the first creature waits for the other.
/// 6. Both turn to face each other again. Then the other creature starts its duel, and the first creature starts its
///    duel on the next turn.
struct CreatureFighting
{
	enum class Stage : uint8_t
	{
		/// Turning to face the opponent
		FaceOff,
		/// Taunting the opponent or making the rude gesture at it
		Taunt,
		/// The creature that started the fight points at the arena that appeared
		Point,
		/// Walking to its place in the arena
		Approach,
		/// Waiting for the other creature
		Ready,
		/// Turning to face the opponent again in the arena
		Square,
		Duel,
		/// Having won: it finishes its move, then leaves the fight. Its mind then does what the win made it do to the loser.
		Celebrate,
		/// After a fight that ended otherwise: finishing, then happy or sad
		Respond,
		/// Its fight stopped with nothing after it: it finishes its move and leaves the fight with no response
		Stop,
	};
	Stage stage {Stage::FaceOff};
	/// How far the creature takes part in the fight. The first creature takes part from the start.
	enum class Part : uint8_t
	{
		/// The other creature is asked. Its mind continues with what it did, and it can still choose a new plan, which
		/// stops the fight.
		Asked,
		/// The mind of the other creature stands still: its desires do not change and it chooses nothing. It continues
		/// with what it did.
		StandsStill,
		/// The creature stopped what it did, and the steps of the fight move its body
		Obeys,
	};
	Part part {Part::Obeys};
	entt::entity opponent {entt::null};
	creature_fight::Arena arena {};
	/// Whether this creature started the fight, takes the arena and takes its place to the east of the middle; the arena
	/// that it took, from the moment the arena appears
	bool madeArena {false};
	entt::entity arenaEntity {entt::null};
	/// Whether this creature started the fight by a plan of its own, and not by the leash, a script or the debug tools
	bool planned {false};
	creature_fight::Fighter fighter {};
	/// The sparkles that mark the moves that the player queued, in step with the queue. Only the computer of the player
	/// who made the moves adds them, as it draws them.
	creature_fight::marks::MarkList marks;
	/// How far each of its blows reaches and where it lands, measured as the fight starts, and when in each blow it
	/// lands, by the blow's place from the first
	std::vector<creature_fight::Reach> reaches;
	std::array<float, creature_fight::animations::k_AttackCount> hitTimesMs {};
	bool measured {false};
	/// Seconds in the stage, and until it next shows its anger in its face
	float stageSeconds {0.0f};
	float faceSeconds {0.0f};
	/// Whether it has been told what to do in this stage yet, and has taunted its opponent
	bool played {false};
	bool taunted {false};
	/// Its turn to face the opponent in the face-off or in the arena, and the game turns of that stage so far
	creature_turn_to_face::Turn faceTurn {};
	uint32_t faceOffTurns {0};
	/// Whether its taunt before the fight is the rude gesture, chosen as the fight starts
	bool rude {false};
	/// Whether it started to let go of the thing that it held before its taunt
	bool handsChecked {false};
	/// Whether its life has paid for the fight and its mind learnt from it
	bool ended {false};
	/// Whether the hand took hold of it in the fight. Its fight steps wait while the hand holds it. When the hand lets go
	/// of it, its fight stops.
	bool held {false};
	/// Why its opponent left the duel before it, if the opponent did. It then stays in its duel against the opponent until
	/// the fight of the opponent ends. Then it stops with no response.
	enum class PartnerLeft : uint8_t
	{
		None,
		/// The opponent won because this creature had no life left
		Won,
		/// The hand took hold of the opponent
		Held,
	};
	PartnerLeft partnerLeft {PartnerLeft::None};
	/// Whether the creature ended its response with no fear, and the game turns after that. The plan of the first
	/// creature then ends after a set number of turns. The other creature waits until the fight of the first creature
	/// ends.
	bool responded {false};
	uint32_t turnsAfterResponse {0};
	/// Whether the creature started its response after the duel. A fight that was called off has no response.
	bool showedResponse {false};
	/// Whether the first creature, on the turn after its response, freed the mind of the other creature again. The other
	/// creature then thinks and can choose a new plan, which stops the fight for both.
	bool mindFree {false};
	/// Whether the body of the first creature came to rest after a duel that it won by the life of the other creature,
	/// while the other creature stays in its duel. On the next turn, the first creature orders the other creature to
	/// respond.
	bool restedAfterDuel {false};
	/// Whether the first creature ended its plan with its order to respond, so that it does not stop this creature when
	/// it leaves. After its response, this creature waits until its mind chooses a new plan.
	bool released {false};
	/// Whether the mind of the creature still has its opponent as its partner from the request for the fight. A stop of
	/// either creature makes the mind forget the partner.
	bool partnerInMind {true};
};
/// Whether a fighter is in one of the steps before the duel
[[nodiscard]] constexpr bool IsLeadIn(CreatureFighting::Stage stage)
{
	return stage < CreatureFighting::Stage::Duel;
}

/// What a creature has learnt of fighting, kept between fights
struct CreatureFightRecord
{
	/// How it leans in fights, from -1 (defensive) to 1 (aggressive), which the temple's belts show. Its moves change it
	/// as they are made.
	float tendency {0.0f};
	/// The lean that the computer chooses its moves by in its fights: 0 for a new creature, then the learnt lean as it
	/// was when the creature last started a fight. A fight that another creature started keeps it as it is.
	float choosingTendency {0.0f};
	uint32_t fights {0};
	uint32_t wins {0};
	/// Who chose its moves when its last fight ended, or as a script last set it; nobody before its first fight. Each
	/// fight starts it afresh.
	creature_fight::Control control {creature_fight::Control::None};
	/// The game turn a blow in a fight last landed on it, blocked or not; the start of the game before any. Villagers
	/// watching its fights cheer or despair by it.
	uint32_t struckTurn {0};
	/// Its fight health, from 0 to 1, as its last fight left it or a script last set it outside a fight; full before its
	/// first fight. Each fight starts it afresh from the creature's life.
	float health {1.0f};
};

/// A creature that won or lost a fight by a knock-out. It takes up no reaction until what the knock-out made it do is
/// over: the winner's plan for the loser, or the loser's time out cold. The mark goes when the creature lies out cold no
/// more and it has finished the agenda that it had at the knock-out.
struct CreatureAfterFight
{
	/// The number of the creature's agenda at the knock-out
	uint32_t agenda {0};
};

/// A creature knocked out: lying where it fell, then taken home, resting there until better, and getting up again
struct CreatureKnockedOut
{
	enum class Stage : uint8_t
	{
		Lying,
		/// Fading out where it lies and back in at home
		FadingOut,
		FadingIn,
		/// Lying at home a few seconds
		Waiting,
		/// Lying until the spells on it have worn off
		WaitingForSpells,
		Resting,
		GettingUp,
	};
	Stage stage {Stage::Lying};
	float seconds {0.0f};
	/// It lay still in its faint at least one time, so the time that it lies counts
	bool layStill {false};
	/// The faint is still to start on the body. It starts on the next turn that the knock-out is looked after: then the
	/// side of the faint is chosen, and those near it react as to a death.
	bool fainting {true};
	/// The creature started to let go of what it held before the faint. The faint starts on the next turn without a new
	/// check of the hands.
	bool handsChecked {false};
	/// Decided as it faints: whether it rests to get better before it gets up
	bool rest {false};
	/// Only a script kills a creature for good: it never gets up
	bool permanent {false};
	/// Where it is taken, if it is taken home
	std::optional<glm::vec3> home;
	/// Game turns left of fizzing out of sight, or back in at home
	uint16_t fizzTurns {0};
};

} // namespace openblack::ecs::components
