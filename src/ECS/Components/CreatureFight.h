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
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"

namespace openblack::ecs::components
{

/// A creature in a fight, from the face-off with its opponent until it has finished showing off or walked off. Its mind
/// leaves its body alone meanwhile. The creature that started the fight by its own plan can still choose a plan that it
/// wants much more. The other creature obeys it and does not think for itself until the fight is over.
///
/// The creature that starts the fight leads the steps before the duel, and tells the other creature when to do each one:
/// 1. Both stop and turn to face each other.
/// 2. Both taunt or make the rude gesture.
/// 3. The arena appears and the first creature points at it.
/// 4. Both walk to their places, and the first creature waits for the other.
/// 5. Both turn to face each other again, then the duel starts.
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
	};
	Stage stage {Stage::FaceOff};
	entt::entity opponent {entt::null};
	creature_fight::Arena arena {};
	/// Whether this creature started the fight, takes the arena and takes its place to the east of the middle; the arena
	/// that it took, from the moment the arena appears
	bool madeArena {false};
	entt::entity arenaEntity {entt::null};
	/// Whether this creature started the fight by a plan of its own, and not by the leash, a script or the debug tools
	bool planned {false};
	creature_fight::Fighter fighter {};
	/// How far each of its blows reaches and where it lands, measured as the fight starts, and when in each blow it
	/// lands, by the blow's place from the first
	std::vector<creature_fight::Reach> reaches;
	std::array<float, creature_fight::animations::k_AttackCount> hitTimesMs {};
	bool measured {false};
	/// Where the action's animation was at the last game turn, which this turn's tests of a blow start from, and which
	/// animation that was
	float blowTestFromMs {0.0f};
	size_t blowTestAnimation {0};
	/// How far the action's own movement had carried it, as a share of the action
	float movedShare {0.0f};
	/// Seconds in the stage, and until it next shows its anger in its face
	float stageSeconds {0.0f};
	float faceSeconds {0.0f};
	/// Whether it has been told what to do in this stage yet, and has taunted its opponent
	bool played {false};
	bool taunted {false};
	/// Whether its life has paid for the fight and its mind learnt from it
	bool ended {false};
};
/// Whether a fighter is in one of the steps before the duel
[[nodiscard]] constexpr bool IsLeadIn(CreatureFighting::Stage stage)
{
	return stage < CreatureFighting::Stage::Duel;
}

/// What a creature has learnt of fighting, kept between fights
struct CreatureFightRecord
{
	/// How it leans in fights, from -1 (defensive) to 1 (aggressive), which the temple's belts show
	float tendency {0.0f};
	bool foughtBefore {false};
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
