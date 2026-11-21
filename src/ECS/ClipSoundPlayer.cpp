/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClipSoundPlayer.h"

#include <cmath>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <SASFile.h>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/SoundGround.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/ClipSounds.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundRequestInterface.h"

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
		// The world finds the table one time, at the first clip that asks for it. The clips of a frame do not change it.
		if (!_table.has_value())
		{
			_table = FindTable();
		}
		return *_table != nullptr ? (*_table)->OfClip(static_cast<uint32_t>(clip)) : nullptr;
	}

	[[nodiscard]] std::optional<creature_audio::Ground> GroundAt(const glm::vec3& position) const override
	{
		return sound_ground::At(position);
	}

	[[nodiscard]] bool InsideTemple() const override
	{
		return Locator::temple::has_value() && Locator::temple::value().Active();
	}

	[[nodiscard]] bool ScriptCutScene() const override
	{
		if (!Locator::cinematicDirectorSystem::has_value())
		{
			return false;
		}
		const auto& director = Locator::cinematicDirectorSystem::value();
		return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}

	[[nodiscard]] float LifeOf(entt::entity object) const override { return world_objects::LifeOf(object); }

	void PlaySound(std::string_view bank, std::span<const int32_t> keys, entt::entity owner, const glm::vec3& position,
	               bool bySampleRules) override
	{
		if (bySampleRules && !_conditions.has_value())
		{
			_conditions = audio::CurrentSoundEffectConditions();
		}
		const auto conditions = bySampleRules ? _conditions : std::nullopt;
		auto& audio = Locator::sounds::value();
		// The sound stays with its owner, and it stops when the owner goes or dies
		const auto played = audio.PlayAnimEffect(std::string(bank), keys, owner, position, conditions);
		audio.SetEmitterFollows(played.emitter, audio::Follows::Owner);
	}

	void EndSound(std::string_view bank, std::span<const int32_t> keys, entt::entity owner, const glm::vec3& position,
	              audio::AnimEffectEnd end) override
	{
		Locator::sounds::value().EndAnimEffect(std::string(bank), keys, owner, position, end,
		                                       audio::AnimEffectMeasure::Straight);
	}

private:
	/// The table of the sounds placed on the clips, none when there is no table or no audio to play them
	[[nodiscard]] static const clip_sounds::ClipSoundTable* FindTable()
	{
		if (!Locator::resources::has_value() || !Locator::sounds::has_value())
		{
			return nullptr;
		}
		const auto& tables = Locator::resources::value().GetClipSounds();
		if (!tables.Contains(clip_sounds::k_TableId.value()))
		{
			return nullptr;
		}
		return tables.Find(clip_sounds::k_TableId.value());
	}

	/// The game's sound conditions, found at the first sound that the sample rules play
	std::optional<audio::SoundEffectConditions> _conditions;
	/// The table of the sounds placed on the clips, found at the first clip that asks for it
	mutable std::optional<const clip_sounds::ClipSoundTable*> _table;
};

} // namespace

bool clip_sound_player::RefreshPace::Step(float frameMilliseconds)
{
	sinceStep += frameMilliseconds;
	if (sinceStep < k_StepMilliseconds)
	{
		return false;
	}
	sinceStep = std::fmod(sinceStep - k_StepMilliseconds, k_StepMilliseconds);
	return true;
}

void clip_sound_player::Play(World& world, entt::entity entity, AnimId clipId, ClipTiming clip, uint32_t place, uint32_t played,
                             const glm::vec3& position, bool refresh)
{
	// Held at its end, a clip played once plays all its sounds again at each refresh, the clock moving or not
	const bool held = !clip.looping && place >= clip.duration;
	if (held ? !refresh : played == 0)
	{
		return;
	}
	const auto* sounds = world.SoundsOf(clipId);
	if (sounds == nullptr || sounds->sounds.empty())
	{
		return;
	}
	const auto passed = held ? clip_sounds::AllOf(sounds->sounds, clip.duration)
	                         : clip_sounds::Passed(sounds->sounds, place, played, clip.duration, clip.looping);
	if (std::ranges::all_of(passed, [](const auto& stretch) { return stretch.empty(); }))
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
	const bool inScript = registry.AllOf<InScript>(entity);
	const bool scriptCutScene = world.ScriptCutScene();
	// A sound that stops the rest of the clip stops only the rest of its stretch. The next stretch plays its sounds.
	for (const auto& stretch : passed)
	{
		for (const auto index : stretch)
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
			    .inScript = inScript,
			    .scriptCutScene = scriptCutScene,
			});
			if (route.outcome == clip_sounds::Outcome::Stop)
			{
				if (const auto logger = spdlog::get("audio"))
				{
					SPDLOG_LOGGER_DEBUG(logger, "Clip {} of entity {}: action {} stops the rest of its stretch",
					                    static_cast<uint32_t>(clipId), entt::to_integral(entity), sound.action);
				}
				break;
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
			if (route.end.has_value())
			{
				world.EndSound(bank, keys.ToArray(), owner, position, *route.end);
			}
			else
			{
				world.PlaySound(bank, keys.ToArray(), owner, position, route.bySampleRules);
			}
		}
	}
}

std::unique_ptr<clip_sound_player::World> clip_sound_player::MakeGameWorld()
{
	return std::make_unique<GameWorld>();
}

void clip_sound_player::Play(World& world, entt::entity entity, AnimId clipId, const L3DAnim& clip, uint32_t place,
                             uint32_t played, const glm::vec3& position, bool refresh)
{
	Play(world, entity, clipId, {.duration = clip.GetPlayTime(), .looping = clip.IsLooping()}, place, played, position,
	     refresh);
}

void clip_sound_player::Play(entt::entity entity, AnimId clipId, const L3DAnim& clip, uint32_t place, uint32_t played,
                             const glm::vec3& position, bool refresh)
{
	GameWorld world;
	Play(world, entity, clipId, clip, place, played, position, refresh);
}
