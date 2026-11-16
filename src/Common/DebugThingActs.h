/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Enums.h"

/// The acts of the Physics, Particles, Temple villagers and Path finding debug windows on the things of the world. In a
/// network game each act goes as a command of the turns, and each computer does it at the turn. In a game of this
/// computer alone the act occurs at once.
namespace openblack::debug_thing_acts
{

/// The players that an effect of the Particles tab can have
inline constexpr uint8_t k_EffectPlayers = 8;
/// The largest magnitude of an effect of the Particles tab
inline constexpr float k_MostMagnitude = 10.0f;
/// The most creatures and villagers that an effect of the Particles tab gets to act on
inline constexpr uint8_t k_MostTargets = 10;
/// The ways that an effect draws
inline constexpr uint8_t k_DrawPaths = 6;
/// The particle types
inline constexpr uint8_t k_ParticleTypes = 150;
/// The youngest and the oldest age of a new villager, and the most new villagers of one act, as the window gives them
inline constexpr uint8_t k_YoungestAge = 1;
inline constexpr uint8_t k_OldestAge = 80;
inline constexpr uint8_t k_MostMade = 32;
/// The new villagers stand round the home of their town, this far from it, in metres
inline constexpr float k_MadeRing = 6.0f;
/// The first number of an effect of this computer only, and the first number that the simulation gives. The effects
/// with the shared random numbers have the numbers below the first, and the simulation gives the numbers from the second.
inline constexpr uint32_t k_FirstLocalEffect = 0x4000'0001u;
inline constexpr uint32_t k_FirstSimulationEffect = 0x8000'0001u;

[[nodiscard]] std::string_view NameOf(player_commands::DebugParticle::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(player_commands::DebugTempleVillager::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(player_commands::DebugVillagerPath::Act act) noexcept;

/// Whether each computer has the effect of the number: an effect with the shared random numbers, or an effect that the
/// simulation asked for
[[nodiscard]] constexpr bool IsSharedEffect(uint32_t effect) noexcept
{
	return effect != 0 && (effect < k_FirstLocalEffect || effect >= k_FirstSimulationEffect);
}

/// Why no computer does the act, none when each computer does it. The game server refuses an act with a refusal, and each
/// computer checks the act again at the turn.
[[nodiscard]] std::optional<std::string_view> PhysicsRefusal(const player_commands::DebugPhysics& act) noexcept;
[[nodiscard]] std::optional<std::string_view> ParticleRefusal(const player_commands::DebugParticle& act) noexcept;
[[nodiscard]] std::optional<std::string_view> TempleVillagerRefusal(const player_commands::DebugTempleVillager& act) noexcept;
[[nodiscard]] std::optional<std::string_view> VillagerPathRefusal(const player_commands::DebugVillagerPath& act) noexcept;
/// The refusal of a command of these windows, none for every other command
[[nodiscard]] std::optional<std::string_view> RefusalOf(const player_commands::Action& action) noexcept;

/// The places on the land, x and z, where the new villagers of a Make act stand: round the home, one after the other
[[nodiscard]] std::vector<glm::vec2> MadePlaces(glm::vec2 home, uint8_t count);

/// The parts of the world that the acts change. The game gives its systems, and the tests give a fake.
class ThingTarget
{
public:
	virtual ~ThingTarget() = default;

	/// Throws the thing, or a new static thing when the thing is not in the world. Returns false when nothing moves.
	virtual bool Throw(const player_commands::DebugPhysics& act, PlayerNames player) = 0;

	/// Starts an effect with the shared random numbers. Returns its number, 0 when there is no such effect.
	virtual uint32_t StartEffect(const player_commands::DebugParticle& act) = 0;
	virtual void SetEffectsPaused(bool paused) = 0;
	virtual void CloseAllEffects() = 0;
	virtual void DeleteAllEffects() = 0;
	virtual void CloseEffect(uint32_t effect) = 0;
	virtual void DeleteEffect(uint32_t effect) = 0;

	/// Returns false when the town is not in the world
	virtual bool SetTownShare(entt::entity town, float share) = 0;
	virtual bool AdjustWorshippers(entt::entity town, int32_t count) = 0;
	/// Makes a villager of the town at the place on the land. Returns none when the villager has no type.
	virtual std::optional<entt::entity> MakeVillager(const player_commands::DebugTempleVillager& act, glm::vec2 place) = 0;
	/// Returns false when the villager is not in the world
	virtual bool SendToWorship(entt::entity villager) = 0;
	virtual bool TakeAway(entt::entity villager) = 0;

	/// Returns false when the villager is not in the world
	virtual bool MoveVillager(const player_commands::DebugVillagerPath& act) = 0;
};

/// The result of an act: whether a part of the world changed, and the villagers that a Make act made
struct ActResult
{
	bool done {false};
	std::vector<entt::entity> made;
};

/// Does an act. An act with a refusal changes nothing.
ActResult Apply(ThingTarget& target, const player_commands::DebugPhysics& act, PlayerNames player);
ActResult Apply(ThingTarget& target, const player_commands::DebugParticle& act);
ActResult Apply(ThingTarget& target, const player_commands::DebugTempleVillager& act);
ActResult Apply(ThingTarget& target, const player_commands::DebugVillagerPath& act);

/// Divides the villagers into acts that take away at most k_MostTakenVillagers villagers each
[[nodiscard]] std::vector<player_commands::DebugTempleVillager> TakeAwayActs(std::vector<entt::entity> villagers);

} // namespace openblack::debug_thing_acts
