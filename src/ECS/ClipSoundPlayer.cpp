/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClipSoundPlayer.h"

#include <optional>
#include <string>
#include <string_view>

#include <SASFile.h>

#include "3D/L3DAnim.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/ClipSounds.h"
#include "Audio/GameSoundEffects.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/SoundGround.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace clip_sounds = openblack::audio::clip_sounds;

namespace
{
constexpr std::string_view k_EditorBank = "editor.sad";
constexpr std::string_view k_BanterBank = "VillagersBanter.sad";
/// The alignment key every clip's sound is played with
constexpr int32_t k_ClipSoundAlignment = 2;

/// The game's resources, audio, ground and temple
class GameWorld final: public clip_sound_player::World
{
public:
	[[nodiscard]] const Registry& Entities() const override { return Locator::entitiesRegistry::value(); }

	[[nodiscard]] const sas::ClipSounds* SoundsOf(AnimId clip) const override
	{
		if (!Locator::resources::has_value() || !Locator::audio::has_value())
		{
			return nullptr;
		}
		auto& tables = Locator::resources::value().GetClipSounds();
		if (!tables.Contains(clip_sounds::k_TableId.value()))
		{
			return nullptr;
		}
		return tables.Handle(clip_sounds::k_TableId.value())->OfClip(static_cast<uint32_t>(clip));
	}

	[[nodiscard]] std::optional<creature_audio::Ground> GroundAt(const glm::vec3& position) const override
	{
		return sound_ground::At(position);
	}

	[[nodiscard]] bool InsideTemple() const override
	{
		return Locator::temple::has_value() && Locator::temple::value().Active();
	}

	[[nodiscard]] float LifeOf(entt::entity object) const override { return world_objects::LifeOf(object); }

	void PlaySound(std::string_view bank, std::span<const int32_t> keys, entt::entity owner, const glm::vec3& position,
	               bool bySampleRules) override
	{
		const auto conditions = bySampleRules ? std::optional(audio::CurrentSoundEffectConditions()) : std::nullopt;
		Locator::audio::value().PlayAnimEffect(std::string(bank), keys, owner, position, conditions);
	}
};

} // namespace

void clip_sound_player::Play(World& world, entt::entity entity, AnimId clipId, ClipTiming clip, uint32_t place, uint32_t played,
                             const glm::vec3& position)
{
	if (played == 0)
	{
		return;
	}
	const auto* sounds = world.SoundsOf(clipId);
	if (sounds == nullptr || sounds->sounds.empty())
	{
		return;
	}
	if (!clip.looping && place >= clip.duration)
	{
		return;
	}
	const auto passed = clip_sounds::Passed(sounds->sounds, place, played, clip.duration, clip.looping);
	if (passed.empty())
	{
		return;
	}

	const auto& registry = world.Entities();
	const auto* villager = registry.TryGet<const Villager>(entity);
	const auto* action = registry.TryGet<const LivingAction>(entity);
	const auto size = clip_sounds::SizeOf(sounds->soundType, villager != nullptr,
	                                      villager != nullptr && villager->lifeStage == Villager::LifeStage::Child,
	                                      villager != nullptr && villager->sex == Villager::Sex::FEMALE);
	const auto surface = creature_audio::SurfaceKey(world.GroundAt(position));
	const bool insideTemple = world.InsideTemple();
	for (const auto index : passed)
	{
		const auto& sound = sounds->sounds[index];
		const auto route = clip_sounds::RouteOf({
		    .soundType = sounds->soundType,
		    .action = sound.action,
		    .mode = sound.mode,
		    .clip = static_cast<uint32_t>(clipId),
		    .isVillager = villager != nullptr,
		    .alive = world.LifeOf(entity) > 0.0f,
		    .turnsInState = action != nullptr ? action->turnsSinceStateChange : uint16_t {0},
		    .insideTemple = insideTemple,
		});
		if (route.outcome == clip_sounds::Outcome::Stop)
		{
			return;
		}
		if (route.outcome == clip_sounds::Outcome::Skip)
		{
			continue;
		}
		const audio::AnimEffectKeys keys {.size = size,
		                                  .alignment = k_ClipSoundAlignment,
		                                  .object = static_cast<audio::SoundObject>(sounds->soundType),
		                                  .surface = surface,
		                                  .action = static_cast<audio::SoundAction>(sound.action)};
		// The home's banter belongs to the home, which a homeless villager hasn't, and is heard as far off as the
		// villager is
		const auto owner = route.fromHome ? villager->abode : entity;
		const auto bank = route.bank == clip_sounds::Bank::Banter ? k_BanterBank : k_EditorBank;
		world.PlaySound(bank, keys.ToArray(), owner, position, route.bySampleRules);
	}
}

void clip_sound_player::Play(entt::entity entity, AnimId clipId, const L3DAnim& clip, uint32_t place, uint32_t played,
                             const glm::vec3& position)
{
	GameWorld world;
	Play(world, entity, clipId, {.duration = clip.GetPlayTime(), .looping = clip.IsLooping()}, place, played, position);
}
