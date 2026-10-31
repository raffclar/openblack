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

#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/GuidanceHeard.h"
#include "Enums.h"

namespace openblack::magic
{
struct EffectValues;
}

/// The voices of the villagers that the guidance plays: a town cries out when something harms it, and it answers food,
/// wood or rain that it gets. Each voice is a sample of the guidance sound bank. The guidance kind of each voice sets
/// how often the player hears it.
namespace openblack::guidance
{

/// The thing that harmed a town, for the cry of its people
enum class Attacker : uint8_t
{
	/// A thing with no cry of its own, or no thing
	Other,
	/// A rock, a bonfire, a dead or felled tree, or a piece of a building
	Rock,
	/// A lightning bolt miracle
	Lightning,
	/// A creature
	Creature,
};

/// The harm that a town got, for the cry of its people
struct TownAttackVoice
{
	/// Fire did the most harm: the people also cry about the fire
	bool fire {false};
	Attacker attacker {Attacker::Other};
};

/// Whether harm with these values makes a town cry out, and if fire did the most harm. The values that count are the
/// burn (a thousandth of it, at most 1), the crush, the hit and the throw. The largest value above 0 wins, and the first
/// of equal values wins. No value above 0 gives no cry.
[[nodiscard]] std::optional<bool> FireLeads(const magic::EffectValues& values);

/// The cries of an attacker: ten samples, picked with a draw of 0 to 9. The creature cries have one empty slot (0),
/// which the game plays as no sound.
[[nodiscard]] uint32_t AttackerCry(Attacker attacker, uint32_t draw);

/// The cries that a town can give, in the order that the game makes its list: the ten general cries, then the ten fire
/// cries when fire led, then the cry of the attacker ten times when it has one
[[nodiscard]] std::vector<uint32_t> AttackCries(bool fire, uint32_t attackerCry);

/// The cry that a draw of 0 to (the count - 1) picks. The game adds each cry to the front of its list, so a draw counts
/// from the last cry.
[[nodiscard]] uint32_t PickCry(const std::vector<uint32_t>& cries, uint32_t draw);

/// The full distance of a town's cry: 200 metres, longer by up to 200 metres when other players attacked it a lot.
/// The protection is the sum of the aggression of the other players.
[[nodiscard]] float AttackCryReach(float protection);

/// The resources and the rain that a town answers when it gets them
enum class Gift : uint8_t
{
	Food = 1,
	Wood = 2,
	Rain = 3,
};

/// The answer of a town to a gift, picked with a draw of 0 to 2. The raw desire of the town for the gift sets it:
/// - 0.5 or more: a pleased answer.
/// - less than 0.25: a displeased answer for food, and a pleased answer for wood and rain (as in the game).
/// - else: no answer.
[[nodiscard]] std::optional<uint32_t> GiftAnswer(Gift gift, float rawDesire, uint32_t draw);
/// Whether a town with this raw desire for a gift answers it. The game draws the answer only when the town answers.
[[nodiscard]] bool GiftAnswered(float rawDesire);

/// The town desire voices: every tenth turn, the people of the town near the camera call out what their town wants most.
/// The people at a worship site near the camera call out for food or for more worshippers.
constexpr uint32_t k_DesireVoiceTurns = 10;
/// The town (its storage pit) or the worship site must be nearer to the camera than this, in metres
constexpr float k_DesireVoiceRange = 200.0f;
/// A voice plays only when its score is more than this
constexpr float k_DesireVoiceLeast = 0.3f;
/// The rows of voices for a worship site, after the rows of the 17 town desires
constexpr uint32_t k_WorshipFoodRow = 17;
constexpr uint32_t k_WorshippersRow = 18;

/// The largest random amount that comes off the value of a desire before its voice is picked: a third of the value
[[nodiscard]] float DesireVoiceDrawLimit(float value);
/// The voice of a row of desire voices, for the value of the desire less a random amount (the draw). The difference
/// picks the voice:
/// - 0.85 or more: the first voice of the row.
/// - 0.65 or more: the second voice.
/// - 0.45 or more: the third voice.
/// - less: no voice (0). A row without voices also gives 0.
[[nodiscard]] uint32_t DesireVoice(uint32_t row, float value, float draw);

/// What the score of a desire voice comes from
struct DesireVoiceScoreInput
{
	/// The turns since the player last heard a town desire voice
	uint32_t turnsSinceHeard {0};
	/// The turns since the voices first looked at the town or the site (see TurnsSinceSeen)
	uint32_t turnsSinceSeen {0};
	/// The distance from the camera to the town's storage pit or to the site, in metres
	float distance {0.0f};
	/// How much the town or the site wants it
	float desire {0.0f};
	/// The voice is the voice that played last
	bool sameAsLast {false};
};
/// The score of a desire voice, from 0 to 2. A strong desire near the camera gives a high score. A long time since the
/// last voice also gives a high score. The score goes down while the voices look at the same thing for a long time. The
/// voice that played last needs a longer time since the last voice.
[[nodiscard]] float DesireVoiceScore(const DesireVoiceScoreInput& input);

/// The turns since the voices first looked at a thing. The first look adds the thing to the front of the list and gives
/// 0. The time starts again when more than 600 turns went by.
[[nodiscard]] uint32_t TurnsSinceSeen(std::vector<ecs::components::GuidanceHeard::SeenThing>& seen, entt::entity thing,
                                      uint32_t turn);

/// A voice and its score
struct DesireVoicePick
{
	uint32_t voice {0};
	float score {0.0f};
};
/// The voice of a worship site, from the voice for food and the voice for worshippers, if there is one. The food voice
/// wins only when its score is more than the score of the worshippers voice. But a worshippers voice with a score always
/// wins, as in the game. The town's voice does not count: the site's voice replaces it.
[[nodiscard]] std::optional<DesireVoicePick> WorshipSiteVoice(DesireVoicePick food, DesireVoicePick worshippers);

/// The largest random amount that comes off the best score for the distance of the voice: half the score
[[nodiscard]] float DesireVoiceReachDrawLimit(float best);
/// The full distance of a desire voice: 200 metres for each point of the best score less a random amount (the draw)
[[nodiscard]] float DesireVoiceReach(float best, float draw);

/// The voices of awe: when a town gains belief in the player at this computer, its people sometimes call out in awe.
/// The voice plays only when another player leads the belief of the town. The share is the belief of the town in the
/// player, against the most belief that it has in any player, from 0 to 1. A small amount added to the two values
/// lets a town with no belief give a share. No share when the player leads already.
[[nodiscard]] std::optional<float> BeliefVoiceShare(std::span<const float> beliefs, size_t player);
/// The largest random amount that comes off the share before the voice is picked: a third of the share
[[nodiscard]] float BeliefVoiceDrawLimit(float share);
/// The voice of awe for the share less the random amount (the left share), good or evil:
/// - 0.7 or more: the most awed voice.
/// - 0.4 or more: the second voice.
/// - more than 0.05: the third voice.
/// - else, or for an alignment that is not good or evil: no voice.
[[nodiscard]] std::optional<uint32_t> BeliefVoice(GuidanceAlignment alignment, float left);
/// How much the player wants to hear a voice of awe, from 0 to 1. The voice must be near the point under the cursor.
/// A long time since the last town desire voice also gives a high value. The voice that played last needs a longer
/// time since that voice.
[[nodiscard]] float BeliefVoiceWanted(uint32_t turnsSinceDesireVoice, float distance, bool sameAsLast);
/// A voice of awe plays only when the value that the player wants it is more than this
constexpr float k_BeliefVoiceLeast = 0.3f;
/// The full distance of a voice of awe, in metres
constexpr float k_BeliefVoiceReach = 200.0f;

/// The volume and the pitch of each guidance voice
constexpr uint32_t k_VoiceVolume = 127;
constexpr uint32_t k_VoicePitch = 100;
/// The share of the full distance at which the player hears a voice at its full volume
constexpr float k_VoiceNearShare = 0.333f;
/// A town answers a gift that falls this near to it, and the player hears the answer this far
constexpr float k_GiftTownReach = 100.0f;
/// A town has three answers of each kind to a gift
constexpr uint32_t k_GiftAnswers = 3;
constexpr float k_GiftAnswerReach = 200.0f;

} // namespace openblack::guidance
