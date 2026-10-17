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
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"

/// How creatures get on with each other: the feelings each keeps for the other creatures, and the rules of a joint
/// action. A creature asks another creature to be its partner before the two do something together. While they are
/// partners, the partner does what the asker tells it to do, and neither creature is free for another joint action.
namespace openblack::creature_friendship
{

/// What a creature asks another creature to do with it. The kind sets the rule by which the other creature answers.
enum class RequestKind : uint8_t
{
	Fight,
	Play,
	/// To be shown something: a miracle, its home or its citadel
	BeShown,
	BeFriends,
};

/// What the asked creature is free for, before the kind of request counts
enum class Availability : uint8_t
{
	/// It is dead, it is under a script's control when the asker is not (or the opposite), or it has another partner
	Refuses,
	/// It is the asker's partner already
	AlreadyPartner,
	/// It has no partner
	Free,
};

/// The facts about the asked creature that its availability comes from
struct AvailabilityFacts
{
	/// It is dead, or it lies dead for good
	bool dead {false};
	bool scriptControlled {false};
	bool askerScriptControlled {false};
	/// Its partner now, none when it has no partner
	std::optional<uint32_t> partner;
	uint32_t asker {0};
};
[[nodiscard]] Availability AvailabilityOf(const AvailabilityFacts& facts);

/// The answer of the asked creature
struct Answer
{
	bool agrees {false};
	/// The explanation that the asked creature gives its player when it refuses, as a failure explanation, none when it
	/// gives none
	std::optional<uint32_t> explanation;
};
/// The failure explanations of a refusal: it refused to play, or it refused to be friends or to be shown something
constexpr uint32_t k_RefusedToPlay = 5;
constexpr uint32_t k_RefusedToBeFriends = 6;

/// A fight is agreed to only when the height of the asked creature is from half to two times the height of the asker
constexpr float k_LeastFightHeightRatio = 0.5f;
constexpr float k_MostFightHeightRatio = 2.0f;

/// What the answer to a request comes from
struct AnswerFacts
{
	RequestKind kind {RequestKind::Play};
	Availability availability {Availability::Refuses};
	/// After any action that a script or the hand forced on a creature, every free creature agrees to play, to be
	/// friends and to be shown something, until the land changes
	bool alwaysAgree {false};
	/// The height of the asked creature divided by the height of the asker
	float heightRatio {1.0f};
	/// How much the asked creature wants a desire about the asker, from 0 to 1
	std::function<float(creature_desires::Desire)> worthAbout;
};
/// The answer of the asked creature. A creature that is not free refuses. To fight, it agrees only for a height near
/// its own. Asked to play, it agrees when it wants to play or to be friends with the asker more than it fears it or is
/// angry with it; asked to be friends or to be shown something, when it wants that more than it fears the asker or is
/// angry with it. A desire counts only when it is worth more than the desires before it in that order and more than
/// nothing.
[[nodiscard]] Answer AnswerOf(const AnswerFacts& facts);

/// When a creature sees another creature that does something with it in mind, it finds that it gets that creature's
/// attention: the attention goes up by this much, to at most 1
constexpr float k_AttentionStep = 0.02f;
/// Whether the attention of the other creature goes up as the creature sees it. The creature must not be under a
/// script's control, and the other creature must be a different creature whose current agenda uses the thing it
/// believes the creature is.
[[nodiscard]] bool AttentionRises(bool scriptControlled, bool sameCreature, bool otherAgendaUsesIt);

/// The things that a creature's current plan and the rest of its agenda use, each none where nothing is used
struct AgendaUse
{
	/// The plan's whole thing, the thing it acts on and the thing it uses
	std::array<std::optional<uint32_t>, 3> plan {};
	/// The things of the steps that are left, from the current step to the last
	std::vector<std::optional<uint32_t>> steps;
	/// The plan is current and has steps left
	bool running {false};
};
/// Whether the agenda uses a thing. With no belief about the thing (none), it is "used" whenever a part of the plan or
/// a step that is left uses nothing, as in the game.
[[nodiscard]] bool AgendaUses(const AgendaUse& use, std::optional<uint32_t> thing);

/// What a turn does to a joint action, from the agenda of one of the two creatures
enum class JointTurn : uint8_t
{
	Continues,
	/// The agenda came to an end with an order: the joint action stops on the next turn
	EndsNextTurn,
	Ends,
};
/// The facts about the agenda of one of the two creatures in a joint action on a turn
struct JointFacts
{
	/// The creature took up another agenda
	bool otherAgenda;
	/// The agenda had steps left when the joint action noted it, and has steps left now
	bool hadStepsLeft;
	bool hasStepsLeft;
	/// The last step of the agenda gave an order to the partner, and the agenda did not stop before that step
	bool endedWithOrder;
	/// The creature saw on an earlier turn that its agenda came to an end
	bool endSeen;
};
/// The joint action stops when the creature takes up another agenda, or when its agenda comes to an end. The asker gives
/// an order on the turn that the order step starts, but it sees the end of its agenda only on the next turn. Thus the
/// partner starts the last order before the joint action stops.
[[nodiscard]] JointTurn JointTurnOf(const JointFacts& facts);

/// Each turn that a creature thinks, how impressive it finds the other creatures fades a little, and the attention
/// that they gave it fades faster
constexpr float k_ImpressiveFade = 0.999999f;
constexpr float k_AttentionFade = 0.9999f;
void Fade(std::span<creature_learning::CreatureAttitude> attitudes);
/// The attention that the other creature gives the creature goes up or down by an amount, kept from 0 to 1
void ChangeAttention(creature_learning::CreatureAttitude& attitude, float change);

/// A creature misses the creatures it finds nicer than this that give it less attention than this
constexpr float k_MissedNiceness = 0.4f;
constexpr float k_MissedAttention = 0.4f;
/// How much the creature misses its friends: the sum, for each creature it finds nice enough and that gives it too
/// little attention, of one less that attention; kept from 0 to 1. Its own feeling for itself counts as the others do.
[[nodiscard]] float MissedFriends(std::span<const creature_learning::CreatureAttitude> attitudes);

} // namespace openblack::creature_friendship
