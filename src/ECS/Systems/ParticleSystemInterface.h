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
#include <string>
#include <string_view>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Particles/ParticleSpellLink.h"
#include "Particles/ParticleSprites.h"

namespace openblack::particles
{
class Effect;
}

namespace openblack::ecs::systems
{

/// The running particle effects: those miracles own and step themselves, the spot visuals scripts and the game place for
/// a time, and any other effect, stepped once a game turn. Their sprites are drawn between the turns.
class ParticleSystemInterface
{
public:
	/// A running effect, by a number that is never reused
	using EffectId = uint32_t;
	static constexpr EffectId k_NoEffect = 0;

	/// The sprites of one effect that share a sheet and a way of blending, for one instanced draw
	struct SpriteBatch
	{
		entt::id_type texture;
		entt::id_type alphaTexture;
		graphics::render_modes::Mode mode;
		/// Where it takes its place among the things that blend: its effect's origin
		glm::vec3 sortPoint;
		/// Its sprites in the frame's list, the farthest from the camera first
		uint32_t first;
		uint32_t count;
	};
	/// Every sprite to draw this frame
	struct SpriteFrame
	{
		std::vector<particles::sprites::SpriteInstance> instances;
		std::vector<SpriteBatch> batches;
	};

	/// What the debug window shows of an effect
	struct EffectInfo
	{
		EffectId id;
		std::string file;
		glm::vec3 origin;
		float age;
		size_t atoms;
		size_t collections;
		bool closing;
		bool ownedBySpell;
		/// Seconds left of a spot visual's life, none for one that lasts until it is closed
		std::optional<float> secondsLeft;
		std::vector<std::string> unportedClasses;
	};

	virtual ~ParticleSystemInterface() = default;

	/// An effect from a particle file by its name (such as "SF_Smoke"), at a point and a magnitude; k_NoEffect if there
	/// is no such file. Its random numbers are the shared ones when synced.
	virtual EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced = false) = 0;
	/// The effect of a particle type; k_NoEffect for a type without a file
	virtual EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced = false) = 0;
	/// An effect a miracle owns and steps itself with ProcessForSpell; it is told the effect started
	virtual EffectId StartForSpell(ParticleType type, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                               particles::SpellSink& sink) = 0;
	/// One step of a miracle's effect; false once it has ended, and then it is gone
	virtual bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) = 0;
	/// A spot visual: the info table's particle type at a point for some turns (its own life when not given, for ever
	/// when negative), following an owner object and ending when the owner goes
	virtual EffectId StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                                 float magnitude = 1.0f) = 0;

	virtual void SetOrigin(EffectId id, glm::vec3 origin) = 0;
	virtual void SetPlayer(EffectId id, int player) = 0;
	/// The effect stops making particles and fades out as its file has it, or goes at once
	virtual void CloseDown(EffectId id) = 0;
	virtual void Delete(EffectId id) = 0;
	[[nodiscard]] virtual bool IsRunning(EffectId id) const = 0;
	[[nodiscard]] virtual particles::Effect* Find(EffectId id) = 0;

	/// Once a game turn: every effect not owned by a miracle steps, newest first, and the spot visuals count down
	virtual void ProcessTurn() = 0;
	/// A new land: every effect goes
	virtual void Reset() = 0;

	/// The sprites drawn this frame, t of the way through the game turn, sorted from the camera
	[[nodiscard]] virtual SpriteFrame CollectSprites(float turnFraction, const glm::vec3& camera) const = 0;
	[[nodiscard]] virtual std::vector<EffectInfo> GetEffects() const = 0;
	/// The particle files' names, for the debug window
	[[nodiscard]] virtual std::vector<std::string> GetFileNames() const = 0;
	/// Stops stepping effects, for looking at them in the debug window
	virtual void SetPaused(bool paused) = 0;
	[[nodiscard]] virtual bool IsPaused() const = 0;
};

} // namespace openblack::ecs::systems
