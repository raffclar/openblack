/*******************************************************************************
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
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{

/// The seed of a miracle that a player casts, which turns over the main room's map
struct TempleMapSpellSeed
{
	SpellSeedType seed;
	/// The point that the model stands its mesh height above, at the map's scale, in the world
	glm::vec3 middle;
	/// The turn about the up axis, in radians
	float spin;
	/// The pulse of a phial and the frame of its texture
	float phialPhase;
	float phialFrame;
	/// The alignment of the player who casts the miracle, which makes the seed of the flying flock a bat or a dove
	float ownerAlignment;
};

/// The seeds of the miracles that the players cast, over the map of the main room.
///
/// The map shows each miracle that has a seed as that seed, at a tenth of its size, half a unit over the map's point
/// above the miracle. The effect of the seed's holder plays above it at the same scale, in the colours of the caster.
/// The seeds turn and the phials pulse by the game's time. The world stops in the temple, so the seeds do not move
/// there.
class TempleMapSpells
{
public:
	/// The size of the seeds and their effects, and the height of the seeds over the map
	static constexpr float k_Scale = 0.1f;
	static constexpr float k_Lift = 0.5f;

	/// A miracle that a player casts, as the map shows it
	struct Cast
	{
		entt::entity spell;
		SpellSeedType seed;
		/// The point of the map above the place of the miracle, in the world
		glm::vec3 mapPoint;
		PlayerNames player;
		float ownerAlignment;
		/// The effect of the seed's holder and its height above the seed's middle at full size
		ParticleType holderParticle;
		float holderHeight;
		/// How a phial seed pulses, none for the other seeds
		std::optional<int> phialReceiveType;
	};

	/// The effects above the seeds
	class Effects
	{
	public:
		virtual ~Effects() = default;
		/// Starts an effect at a point and a scale, in the colours of a player. None when the effect does not start.
		virtual std::optional<uint32_t> Start(ParticleType type, glm::vec3 point, float scale, PlayerNames player) = 0;
		/// Moves the effect to a point and steps it by some seconds of the game's time
		virtual void Step(uint32_t effect, glm::vec3 point, float seconds) = 0;
		virtual void Stop(uint32_t effect) = 0;
	};

	/// Shows the seeds of these miracles this frame, and steps them by the game's time of the frame. The seed of a
	/// miracle that is new on the map starts unturned. The seeds of the miracles that stopped go, with their effects.
	void Update(std::span<const Cast> casts, float gameSeconds, Effects& effects);
	/// Removes all the seeds and their effects
	void Clear(Effects& effects);
	/// The seeds that the map shows this frame
	[[nodiscard]] const std::vector<TempleMapSpellSeed>& GetSeeds() const { return _seeds; }

	/// The middle of the seed above a point of the map
	[[nodiscard]] static glm::vec3 Middle(glm::vec3 mapPoint);

private:
	struct Shown
	{
		TempleMapSpellSeed seed;
		std::optional<uint32_t> effect;
	};
	std::unordered_map<entt::entity, Shown> _shown;
	std::vector<TempleMapSpellSeed> _seeds;
};

} // namespace openblack
