/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <random>
#include <span>
#include <unordered_map>

#include "3D/SkeletalAnimation.h"
#include "Common/FramePace.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureRig.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureAnimationSystem final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override;
	void BreathTurn(entt::entity creature) override;
	void BodyTurn(entt::entity creature) override;
	void MotesTurn(entt::entity creature) override;
	void SwayTurn(entt::entity creature) override;
	void EndPoseTurn(entt::entity creature) override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void PoseTurn(entt::entity creature) override;
	void PoseTurns() override;
	void HoldTurnPose(entt::entity creature) override;
	void PoseCopy(Registry& copies, entt::entity copy, float milliseconds) override;
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs, uint32_t bone,
	                                                       bool mirrored) override;
	[[nodiscard]] std::optional<glm::vec3> MiddleOfBonesInAnimation(entt::entity creature, size_t animation, float timeMs,
	                                                                std::span<const uint32_t> bones) override;
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<creature_kiss::KissPoints> KissPointsOf(entt::entity creature) override;
	void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) override;
	void Hurry(entt::entity creature) override;
	void PlaceHandGlows(entt::entity creature, int32_t frameMs, int32_t gameMs) override;
	void SetCreedPower(entt::entity creature, creature_hand_glow::Place place, float scale, float power) override;
	void LoadCreedPowers(entt::entity creature,
	                     std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount> powers) override;
	void PlayCreedEvent(entt::entity creature, int32_t event) override;
	[[nodiscard]] std::optional<glm::vec3> MiddleOfGroinAndLips(entt::entity creature) override;
	void AddAlignmentMotesOf(entt::entity copy) override;

	/// A species' animation adjusted to the stand of a variant mesh that has no animation of its own, and the
	/// animations that it comes from
	struct AdjustedAnimation
	{
		const skeletal_animation::Animation* base;
		const skeletal_animation::Animation* baseStand;
		const skeletal_animation::Animation* stand;
		skeletal_animation::Animation animation;
	};
	/// Which species, variant mesh and animation an adjusted animation is of
	struct AdjustedKey
	{
		const creature::CreatureRig* rig;
		creature::CreatureRig::Mesh mesh;
		size_t index;
		[[nodiscard]] bool operator==(const AdjustedKey&) const = default;
	};
	struct AdjustedKeyHash
	{
		[[nodiscard]] size_t operator()(const AdjustedKey& key) const noexcept
		{
			return std::hash<const void*> {}(key.rig) ^ (std::hash<size_t> {}(key.index) * 31u) ^
			       (static_cast<size_t>(key.mesh) << 20u);
		}
	};
	using AdjustedAnimations = std::unordered_map<AdjustedKey, std::optional<AdjustedAnimation>, AdjustedKeyHash>;

private:
	/// The creature's animations move on by some milliseconds, and the body takes a new pose where the creature is. The
	/// pose of the last turn becomes the older pose.
	void PoseForTurn(entt::entity entity, float milliseconds);
	/// The breath of a creature in a game turn of this many milliseconds
	void BreatheForTurn(entt::entity creature, float milliseconds);
	/// The creature starts or stops its creed hum after a change to its glows
	static void ApplyHum(entt::entity creature, creature_hand_glow::Hum hum);
	/// While the body of a creature plays the loop of the puke animation, twelve drops of sick come out of its lips
	static void AddPukeDrops(entt::entity creature);

	/// The game's random numbers: the blinks and the eye movements draw on the local seed, the dance moves on the synced seed
	GameRandomSource _random;
	/// The animations adjusted to the variants' stands, made the first time that a creature of the species wants them
	AdjustedAnimations _adjusted;
};

} // namespace openblack::ecs::systems
