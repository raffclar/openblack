/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSoul.h"

#include <string>
#include <vector>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::villager_soul
{
using namespace components;

namespace
{
constexpr int32_t k_Dead1GotoHeaven = 244; ///< P_DEAD1_GOTO_HEAVEN
constexpr int32_t k_Dead1GotoHell = 245;   ///< P_DEAD1_GOTO_HELL
constexpr int32_t k_Dead2GotoHeaven = 247; ///< P_DEAD2_GOTO_HEAVEN
constexpr int32_t k_Dead2GotoHell = 248;   ///< P_DEAD2_GOTO_HELL
constexpr uint8_t k_Alpha = 105;           ///< The soul's alpha before the fade

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The source's current clip name is "M_P_DEAD1"
bool SourceClipIsDead1(entt::entity source)
{
	const auto* animation = Entities().TryGet<const SkeletalAnimation>(source);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return false;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return false;
	}
	return animations.Handle(animation->clip)->GetName() == "M_P_DEAD1";
}

uint32_t ClipDuration(entt::id_type clip)
{
	if (!Locator::resources::has_value())
	{
		return 0;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(clip))
	{
		return 0;
	}
	return static_cast<uint32_t>(animations.Handle(clip)->GetDurationMs());
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

int32_t SoulClip(bool sourceIsDead1Name, bool heavenForced, float roll)
{
	// The pair by the name, heaven when roll < 50
	const bool heaven = roll < 50.0f;
	int32_t clip =
	    sourceIsDead1Name ? (heaven ? k_Dead2GotoHeaven : k_Dead2GotoHell) : (heaven ? k_Dead1GotoHeaven : k_Dead1GotoHell);
	// heavenForced -> the heaven clip after all
	if (heavenForced)
	{
		clip = sourceIsDead1Name ? k_Dead2GotoHeaven : k_Dead1GotoHeaven;
	}
	return clip;
}

uint8_t SoulAlpha(uint32_t elapsed, uint32_t duration)
{
	// The fade starts 500 ms before the end (signed comparison)
	const auto start = static_cast<int32_t>(duration) - 500;
	const auto now = static_cast<int32_t>(elapsed);
	if (now <= start)
	{
		return k_Alpha;
	}
	// Linear from 105 to 0 over the 500 ms, each step rounded to float as in the original, then truncated
	const float x = (1.0f - static_cast<float>(now - start) * 0.002f) * 105.0f;
	return static_cast<uint8_t>(static_cast<int32_t>(x));
}

bool SoulExpired(uint32_t elapsed, uint32_t duration)
{
	// Freed 110 ms before the clip ends (signed comparison)
	return static_cast<int32_t>(elapsed) + 110 > static_cast<int32_t>(duration);
}

// ---- the souls ---------------------------------------------------------------------------------------------------

int32_t Create(entt::entity source, MeshId mesh, bool heavenForced)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(source);
	if (transform == nullptr)
	{
		return -1;
	}
	// The soul's own entity
	const auto soul = registry.Create();
	// The source's position and orientation where it is drawn (DrawPosition), else its Transform. (inferred) Scale 1:
	// the source's scale is not copied
	const auto* draw = registry.TryGet<const DrawPosition>(source);
	const glm::vec3 position = draw != nullptr && draw->started ? draw->position : transform->position;
	const glm::mat3 rotation = draw != nullptr && draw->started ? draw->rotation : transform->rotation;
	registry.Assign<Transform>(soul, position, rotation, glm::vec3(1.0f));
	// mesh = MeshPack[mesh] (0 out of range)
	registry.Assign<Mesh>(soul, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// The clip; Random(0, 100) (the CRT stream) drawn always, before the name test is used
	const bool dead1 = SourceClipIsDead1(source);
	const float roll = game_random::crt::Random(0.0f, 100.0f);
	const int32_t clip = SoulClip(dead1, heavenForced, roll);
	auto& animation = registry.Assign<SkeletalAnimation>(soul);
	animation.clip = ClipId(static_cast<uint32_t>(clip));
	animation.clipIndex = clip;
	animation.hasClip = true;
	animation.time = 0.0f;
	// White, alpha 105 / 255 in the translucent pass. (inferred) the original puts the alpha in the object colour; here
	// it goes through components::Alpha (render_modes' GlobalAlpha, the shared translucent path), ObjectColour stays white
	registry.Assign<ObjectColour>(soul);
	registry.Assign<Alpha>(soul, static_cast<float>(k_Alpha) / 255.0f);
	registry.Assign<VillagerSoul>(soul, 0u, ClipDuration(animation.clip));
	return clip;
}

void Update(uint32_t milliseconds)
{
	auto& registry = Entities();
	std::vector<entt::entity> gone;
	registry.Each<VillagerSoul, SkeletalAnimation, Alpha>(
	    [&gone, milliseconds](entt::entity soul, VillagerSoul& record, SkeletalAnimation& animation, Alpha& alpha) {
		    // Advance by the elapsed time
		    record.elapsedMs += milliseconds;
		    // elapsed + 110 > the clip's duration -> freed
		    if (SoulExpired(record.elapsedMs, record.durationMs))
		    {
			    gone.push_back(soul);
			    return;
		    }
		    // The clip's time = elapsed
		    animation.time = static_cast<float>(record.elapsedMs);
		    // The alpha (the fade)
		    alpha.value = static_cast<float>(SoulAlpha(record.elapsedMs, record.durationMs)) / 255.0f;
	    });
	for (const auto soul : gone)
	{
		registry.Destroy(soul);
	}
	if (!gone.empty())
	{
		registry.SetDirty();
	}
}
} // namespace openblack::ecs::villager_soul
