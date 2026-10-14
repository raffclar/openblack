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
#include <vector>

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

/// The volume and the pitch of each guidance voice
constexpr uint32_t k_VoiceVolume = 127;
constexpr uint32_t k_VoicePitch = 100;
/// The share of the full distance at which the player hears a voice at its full volume
constexpr float k_VoiceNearShare = 0.333f;
/// A town answers a gift that falls this near to it, and the player hears the answer this far
constexpr float k_GiftTownReach = 100.0f;
constexpr float k_GiftAnswerReach = 200.0f;

} // namespace openblack::guidance
