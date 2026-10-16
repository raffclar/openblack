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
#include <string_view>
#include <variant>
#include <vector>

#include <IFRFile.h>
#include <glm/vec2.hpp>

#include "Enums.h"

/// The rules of the force-feedback effects: which effect each game event plays, which detail level lets it play, how
/// long it plays and how strong it feels at each moment. The effects themselves come from the game's effect files.
namespace openblack::force_feedback
{

/// The detail level that a profile has before the player changes it
constexpr int k_DefaultLevel = 1;
/// The levels go from 0 (only the most important effects) to 2 (all effects)
constexpr int k_HighestLevel = 2;

/// An effect that the game plays: the name of its object in the effect file, and the lowest detail level that lets it
/// play
struct EffectEntry
{
	ImmersionEffectType type;
	std::string_view name;
	int level;
};

// clang-format off
constexpr std::array<EffectEntry, 48> k_Effects {{
    {ImmersionEffectType::Fur, "Fur", 1},
    {ImmersionEffectType::Slap, "CreatureSlap", 1},
    {ImmersionEffectType::Hit, "CreatureHit", 1},
    {ImmersionEffectType::GestureSuccess, "GestureSuccess", 1},
    {ImmersionEffectType::GripForce, "GripForce", 1},
    {ImmersionEffectType::Uproot, "MiscUpRoot", 1},
    {ImmersionEffectType::HandHitInfluence, "HitInfluence", 1},
    {ImmersionEffectType::LeashPull, "LeashPull", 1},
    {ImmersionEffectType::GestureTrail, "GestureTrail", 1},
    {ImmersionEffectType::SpellCrackle, "SpellCrackle", 1},
    {ImmersionEffectType::MiscHeart, "Heart", 1},
    {ImmersionEffectType::SprinkleFood, "SprinkleFood", 1},
    {ImmersionEffectType::SprinkleWood, "SprinkleWood", 1},
    {ImmersionEffectType::Burning, "MovingHandBurning", 1},
    {ImmersionEffectType::OneShotSpellPop, "OneShotSpellPop", 1},
    {ImmersionEffectType::Heavy, "InHandHeavy", 1},
    {ImmersionEffectType::Trademark, "TradeMark", 0},
    {ImmersionEffectType::MushroomChallenge, "Challenge1", 0},
    {ImmersionEffectType::FireballL1, "SpellFireballSmall", 1},
    {ImmersionEffectType::FireballL2, "SpellFireballMedium", 1},
    {ImmersionEffectType::FireballL3, "SpellFireballLarge", 1},
    {ImmersionEffectType::LightningBolt, "SpellLightningBolt", 1},
    {ImmersionEffectType::AnimatingInHand, "AnimatingInHand", 1},
    {ImmersionEffectType::GeneralSpellCast, "SpellGeneral", 1},
    {ImmersionEffectType::Shield, "SpellShield", 1},
    {ImmersionEffectType::SprinkleWater, "SprinkleWater", 1},
    {ImmersionEffectType::FlockFlying, "SpellReleaseBirds", 1},
    {ImmersionEffectType::FlockOnGround, "SpellCreateFlockOnGround", 1},
    {ImmersionEffectType::CreatureSpell, "SpellCastOnCreature", 1},
    {ImmersionEffectType::HandOverSpell, "MovingHandSpell", 1},
    {ImmersionEffectType::HandOverGeneral, "MovingHandGeneric", 1},
    {ImmersionEffectType::HandOverStraw, "MovingHandStraw", 2},
    {ImmersionEffectType::HandOverSmooth, "MovingHandSmooth", 2},
    {ImmersionEffectType::HandOverWood, "MovingHandWood", 2},
    {ImmersionEffectType::HandOverCanvas, "MovingHandCanvas", 2},
    {ImmersionEffectType::HandOverChallengeScroll, "SilverScroll", 1},
    {ImmersionEffectType::HandOverStoryScroll, "GoldScroll", 1},
    {ImmersionEffectType::HandOverFoliage, "MovingHandTrees", 2},
    {ImmersionEffectType::HandOverCreed, "Creed", 1},
    {ImmersionEffectType::HandOverSingingStones, "MovingHandSingingStone", 2},
    {ImmersionEffectType::HandOverVortex, "Vortex", 1},
    {ImmersionEffectType::HandOverCitadel, "MovingHandCitadel", 2},
    {ImmersionEffectType::HitExclusion, "HitInfluenceBorder", 1},
    {ImmersionEffectType::MovingFish, "MovingFish", 2},
    {ImmersionEffectType::CommandSuccess, "MiscCommandSuccess", 2},
    {ImmersionEffectType::Hanoi, "Hanoi", 2},
    {ImmersionEffectType::PlantObject, "MiscPlantObject", 1},
    {ImmersionEffectType::HandOverTeleport, "SpellTeleport", 1},
}};
// clang-format on

/// The entry of an effect type, none for the types that have no effect (none, and the types past the table)
[[nodiscard]] const EffectEntry* EntryFor(ImmersionEffectType type);
/// True when the player's detail level lets the effect play, start, stop or answer that it plays
[[nodiscard]] bool LevelAllows(ImmersionEffectType type, int level);

/// The kinds of device, which pick the effect file
enum class DeviceKind : uint8_t
{
	/// A device that pushes the mouse with forces
	FullForce,
	/// A device that only vibrates
	Tactile,
};

/// The file of Data/Immersion that has the effects for this kind of device
[[nodiscard]] std::string_view FileFor(DeviceKind kind);

/// One effect of an effect object
using Part = std::variant<ifr::Periodic, ifr::Texture, ifr::Condition>;

/// An effect as the device plays it: its parts play together
struct Effect
{
	std::vector<Part> parts;
	/// The period in milliseconds and the duration as a number of periods of the first part, when the first part is a
	/// periodic effect: the beat that a quicker beat changes
	std::optional<uint32_t> periodMs;
	std::optional<float> durationRatio;
};

/// The effect of an object of the effect file. Each part keeps the values of the file.
[[nodiscard]] Effect EffectFrom(const ifr::IFRFile& file, const ifr::Object& object);

/// The time a part plays for, from its start, in seconds. None when it plays until something stops it. The given
/// number of times replaces the number of times of the file.
[[nodiscard]] std::optional<double> PlaySeconds(const Part& part, std::optional<uint32_t> iterations);
/// True while some part of the effect plays, the given seconds after its start
[[nodiscard]] bool Plays(const Effect& effect, double seconds, std::optional<uint32_t> iterations);

/// The first part of an effect with its beat made quicker by a fraction from 0 to 1. The period becomes the period
/// less that fraction, at least 3 milliseconds. The duration keeps the same number of periods. Without a periodic first
/// part or a duration ratio above 0 the effect does not change.
void QuickenBeat(Effect& effect, float fraction);
/// The first part of an effect, when it is periodic, gets a new period in milliseconds and a new strength
void SetPeriodAndMagnitude(Effect& effect, uint32_t periodMs, uint32_t magnitude);

/// The effect of what the hand holds or touches, which the game keeps up once a frame
struct HandTextureState
{
	std::optional<ImmersionEffectType> current;
	/// True when the effect already played when the hand took it, so that the hand does not stop it
	bool startedElsewhere {false};
};

/// What a frame of the hand's texture does: an effect to stop and an effect to start, in that order
struct HandTextureStep
{
	std::optional<ImmersionEffectType> stop;
	std::optional<ImmersionEffectType> start;
};

/// The next frame of the hand's texture. The same effect starts again when it was playing already when the hand took
/// it and it has stopped since. A new effect stops the old one, unless the old one played before the hand took it.
[[nodiscard]] HandTextureStep NextHandTexture(HandTextureState& state, std::optional<ImmersionEffectType> wanted,
                                              const std::function<bool(ImmersionEffectType)>& isPlaying);

/// The touch on a creature's body: while the hand touches the body, a fur effect that does not play starts and one
/// that plays stops; off the body, a fur effect that plays stops. True starts the effect, false stops it, and none does
/// nothing.
[[nodiscard]] std::optional<bool> FurStep(bool touchesBody, bool furPlays);

/// The vibration of a device that can only vibrate, from 0 to 1, for a periodic part the given seconds after its start
/// and the given number of times. A wave quicker than the frames of a vibration motor shows as its strength, a slower
/// wave as its value at that moment.
[[nodiscard]] float PeriodicStrength(const ifr::Periodic& periodic, double seconds, std::optional<uint32_t> iterations);
/// The vibration, from 0 to 1, of a texture as the cursor moves from one place to the next, in pixels of the game's
/// screen of reference
[[nodiscard]] float TextureStrength(const ifr::Texture& texture, glm::vec2 from, glm::vec2 to);

/// The period, in seconds, below which a wave shows as its strength on a vibration motor
constexpr double k_QuickWaveSeconds = 0.05;
/// The size of the game's screen of reference, which the effect files measure textures on
constexpr glm::vec2 k_ReferenceScreen {800.0f, 600.0f};

} // namespace openblack::force_feedback
