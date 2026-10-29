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

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Particles/ManaPathMaths.h"
#include "Particles/ParticleSpellLink.h"
#include "Particles/PlasmaCommand.h"

namespace openblack::particles
{
struct BeliefSprite;
class LightSheet;
struct ShieldSphere;
namespace draw
{
struct FightMark;
struct PlayerSymbol;
} // namespace draw
} // namespace openblack::particles

namespace openblack::ecs::systems
{

/// The number by which the simulation keeps a particle effect that it asked for. The simulation gives the numbers in
/// the same order on every computer. The particles of this computer use the same number for the effect.
using EffectHandle = uint32_t;
/// No effect
inline constexpr EffectHandle k_NoEffectHandle = 0;
/// The first number that the simulation gives. The particles give their own effects numbers below it, so that the two
/// sets of numbers never meet.
inline constexpr EffectHandle k_FirstEffectHandle = 0x8000'0001u;

/// The part of the game that steps an effect
enum class EffectStep : uint8_t
{
	/// The particles step it once a game turn
	Turn,
	/// The frames that draw its thing step it
	Frame,
};

/// A particle effect that a game turn or a script asks for
struct EffectRequest
{
	/// A particle type, or a spot visual of the info table, which gives the particle type, the life and the draw
	std::variant<ParticleType, SpotVisualType> type {ParticleType::None};
	glm::vec3 position {0.0f};
	float magnitude {1.0f};
	/// The random numbers of the effect come from the seed that every computer shares, else from this computer's own
	bool synced {true};
	EffectStep step {EffectStep::Turn};
	/// A spot visual's life in turns: its own life when none, for ever when negative
	std::optional<int> turns;
	/// The object that a spot visual follows and ends with
	entt::entity owner {entt::null};
	/// The player whose colour the effect shows
	std::optional<int> player;
	/// The direction the effect goes in
	std::optional<glm::vec3> direction;
	/// The state of the miracle that the effect shows
	std::optional<particles::ProcessInfo> processInfo;
	/// The miracle that gets the events of the effect. Its owner then steps the effect with Step, and the turn does not.
	particles::SpellSink* sink {nullptr};
	/// The turn does not step the effect: its owner does, with Step, also when no miracle gets its events
	bool ownerSteps {false};
	/// The effect draws immediately after the hand, and does not sort with the other effects
	bool drawnWithHand {false};
	/// The effect starts hidden
	bool hidden {false};
};

/// The side that shows the effects that the simulation asks for: the particles of this computer. A computer that shows
/// nothing has none.
class EffectServerInterface
{
public:
	virtual ~EffectServerInterface() = default;

	/// Starts the effect under the number that the simulation gave it, or under a number of its own for none. Gives the
	/// number of the effect, or none when no effect started.
	virtual EffectHandle Serve(const EffectRequest& request, EffectHandle handle) = 0;
	/// Puts the effect at another point
	virtual void Move(EffectHandle handle, glm::vec3 position) = 0;
	/// The effect stops making particles and fades out as its file has it
	virtual void Stop(EffectHandle handle) = 0;
	/// The effect goes at once
	virtual void Remove(EffectHandle handle) = 0;
	[[nodiscard]] virtual bool IsRunning(EffectHandle handle) const = 0;
	/// The player whose colour the effect shows
	virtual void SetPlayer(EffectHandle handle, int player) = 0;
	/// The strength of the miracle that the rules of the effect read
	virtual void SetPower(EffectHandle handle, float power) = 0;
	virtual void SetMagnitude(EffectHandle handle, float magnitude) = 0;
	/// The alpha that the effect multiplies all its particles by
	virtual void SetAlpha(EffectHandle handle, float alpha) = 0;
	/// The state of the miracle that the effect shows
	virtual void SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info) = 0;
	/// While false, the rules of the effect that ask whether the miracle is still cast fail
	virtual void SetEnabled(EffectHandle handle, bool enabled) = 0;
	/// An object for the rules of the effect to act on
	virtual void AddTarget(EffectHandle handle, entt::entity target) = 0;
	/// A point for the rules of the effect to act on
	virtual void AddTargetPosition(EffectHandle handle, glm::vec3 position) = 0;
	/// A plasma beam for the effect to fire
	virtual void AddPlasma(EffectHandle handle, const particles::PlasmaCommand& command) = 0;
	/// A spark for the effect of the mana path to carry from one point to another
	virtual void AddManaPathSpark(EffectHandle handle, const particles::mana_path::Spark& spark) = 0;

	/// One step of an effect that its owner steps. Gives false when the effect ended, and then the effect is gone.
	virtual bool ProcessForSpell(EffectHandle handle, const particles::ProcessInfo& info, float seconds) = 0;
	/// Gives the effect its miracle again after a load, with no event to the miracle
	virtual void RestoreSink(EffectHandle handle, particles::SpellSink* sink) = 0;
	/// A hidden effect continues to step but does not draw
	virtual void SetHidden(EffectHandle handle, bool hidden) = 0;
	/// The draw moves all the effect by this value from the point where its last step put it
	virtual void SetDrawOffset(EffectHandle handle, glm::vec3 offset) = 0;
	/// The point where this computer draws its hand in this frame. The draw moves the atoms that follow the hand to it.
	virtual void SetLiveHand(EffectHandle handle, glm::vec3 hand) = 0;
	/// The live shield that holds a point, with its sphere larger by a margin, if a shield holds the point
	[[nodiscard]] virtual std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 point, float margin) const = 0;

	/// A symbol of belief rises in the effect that shows all the symbols
	virtual void AddBeliefSprite(const particles::BeliefSprite& sprite) = 0;
	/// A spark of the mana path of the hand, in the effect that shows all these sparks
	virtual void AddHandManaPathSpark(const particles::mana_path::Spark& spark) = 0;
	/// A forked bolt of a storm comes down from a point under its cloud, in the effect of all the bolts
	virtual void AddStormBolt(glm::vec3 cloudPoint) = 0;
	/// The sparkles that mark the fight moves of the player. Each list replaces the last list.
	virtual void SetFightMarks(std::vector<particles::draw::FightMark> marks) = 0;
	/// A sheet of light on the land, which shows while its owner keeps it and does not hide it
	virtual void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) = 0;
	/// The symbols of the players that the artefacts show. Each list replaces the last list.
	virtual void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols) = 0;
};

/// The particle effects that the game turns and the scripts ask for. The simulation keeps each effect by a number of
/// its own, and the particles of this computer, if any, show the effect.
class EffectRequestSystemInterface: public SavedStateInterface
{
public:
	/// Starts an effect and gives the number that keeps it. Every request takes a number, also when no particles show it.
	/// Call it on every computer in the same order, as the turns and the scripts do.
	virtual EffectHandle Start(const EffectRequest& request) = 0;
	/// Starts an effect that plays out by itself, with no number for the simulation
	virtual void Show(const EffectRequest& request) = 0;
	/// Starts an effect that only this computer shows, such as an effect of the hand of this computer. The particles of
	/// this computer give the number. Gives none when no effect started. The turns of the other computers never see the
	/// number.
	virtual EffectHandle StartHere(const EffectRequest& request) = 0;
	virtual void Move(EffectHandle handle, glm::vec3 position) = 0;
	/// The effect stops making particles and fades out
	virtual void Stop(EffectHandle handle) = 0;
	/// The effect goes at once
	virtual void Remove(EffectHandle handle) = 0;
	/// Whether the particles of this computer show the effect. It is false on a computer that shows nothing.
	[[nodiscard]] virtual bool IsRunning(EffectHandle handle) const = 0;
	/// The player whose colour the effect shows
	virtual void SetPlayer(EffectHandle handle, int player) = 0;
	/// The strength of the miracle that the rules of the effect read
	virtual void SetPower(EffectHandle handle, float power) = 0;
	virtual void SetMagnitude(EffectHandle handle, float magnitude) = 0;
	/// The alpha that the effect multiplies all its particles by
	virtual void SetAlpha(EffectHandle handle, float alpha) = 0;
	/// The state of the miracle that the effect shows
	virtual void SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info) = 0;
	/// While false, the rules of the effect that ask whether the miracle is still cast fail
	virtual void SetEnabled(EffectHandle handle, bool enabled) = 0;
	/// An object for the rules of the effect to act on
	virtual void AddTarget(EffectHandle handle, entt::entity target) = 0;
	/// A point for the rules of the effect to act on
	virtual void AddTargetPosition(EffectHandle handle, glm::vec3 position) = 0;
	/// A plasma beam for the effect to fire
	virtual void AddPlasma(EffectHandle handle, const particles::PlasmaCommand& command) = 0;
	/// A spark for the effect of the mana path to carry from one point to another
	virtual void AddManaPathSpark(EffectHandle handle, const particles::mana_path::Spark& spark) = 0;

	/// Whether the particles of this computer show the effects. It is false on a computer that shows nothing.
	[[nodiscard]] virtual bool ShowsEffects() const = 0;
	/// One step of an effect that its owner steps, such as the effect of a miracle. Gives false when the effect ended,
	/// and then the effect is gone. It is false on a computer that shows nothing.
	virtual bool Step(EffectHandle handle, const particles::ProcessInfo& info, float seconds) = 0;
	/// Gives the effect its miracle again after a load, with no event to the miracle
	virtual void RestoreSink(EffectHandle handle, particles::SpellSink* sink) = 0;
	/// A hidden effect continues to step but does not draw
	virtual void SetHidden(EffectHandle handle, bool hidden) = 0;
	/// The draw moves all the effect by this value from the point where its last step put it
	virtual void SetDrawOffset(EffectHandle handle, glm::vec3 offset) = 0;
	/// The point where this computer draws its hand in this frame. The draw moves the atoms that follow the hand to it.
	virtual void SetLiveHand(EffectHandle handle, glm::vec3 hand) = 0;
	/// The live shield that holds a point, with its sphere larger by a margin. None on a computer that shows nothing.
	[[nodiscard]] virtual std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 point, float margin) const = 0;

	/// A symbol of belief rises in the effect that shows all the symbols
	virtual void AddBeliefSprite(const particles::BeliefSprite& sprite) = 0;
	/// A spark of the mana path of the hand, in the effect that shows all these sparks
	virtual void AddHandManaPathSpark(const particles::mana_path::Spark& spark) = 0;
	/// A forked bolt of a storm comes down from a point under its cloud, in the effect of all the bolts
	virtual void AddStormBolt(glm::vec3 cloudPoint) = 0;
	/// The sparkles that mark the fight moves of the player. Each list replaces the last list.
	virtual void SetFightMarks(std::vector<particles::draw::FightMark> marks) = 0;
	/// A sheet of light on the land, which shows while its owner keeps it and does not hide it
	virtual void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) = 0;
	/// The symbols of the players that the artefacts show. Each list replaces the last list.
	virtual void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols) = 0;

	/// A new land: the numbers start from the first again
	virtual void Reset() = 0;
	/// The number that the next effect gets
	[[nodiscard]] virtual EffectHandle NextHandle() const = 0;
};

} // namespace openblack::ecs::systems
