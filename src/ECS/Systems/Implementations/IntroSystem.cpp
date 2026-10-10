/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "IntroSystem.h"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "Animals/AnimalAnimation.h"
#include "Common/GameRandom.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/IntroHand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::intro_rules;
namespace light = openblack::ecs::intro_rules::light;

namespace
{
constexpr std::string_view k_HandModel = "hand_intro.l3d";
/// The hand lifting the boy out of the sea, played once
constexpr std::string_view k_LiftClip = "hand_intro2.anm";
/// The hand setting him down, which comes round at its end, when the hand goes
constexpr std::string_view k_SetDownClip = "hand_intro.anm";

auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

float CrtRandom(float a, float b)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().CrtRandom(a, b) : a;
}

/// A model or clip of the misc folder, loaded into its cache the first time
template <typename Loader>
std::optional<entt::id_type> LoadMisc(resources::ResourceManager<Loader>& cache, std::string_view file)
{
	if (!Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto id = resources::HashIdentifier("misc/" + std::string(file));
	if (cache.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / file;
	try
	{
		cache.Load(id, typename Loader::FromDiskTag {}, path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The opening's hand: cannot load {}: {}", path.generic_string(), e.what());
		return std::nullopt;
	}
	return id;
}

const L3DAnim* ClipOf(entt::id_type id)
{
	const auto& animations = Locator::resources::value().GetAnimations();
	return id != 0 && animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}
} // namespace

void IntroSystem::Play(int32_t special)
{
	switch (special)
	{
	case 0:
		// A new light falls; one still falling is forgotten, as the game forgets it
		_stage = Stage::Falling;
		_light = light::Make(rules::k_BoySpot, rules::k_LightWay, CrtRandom);
		_holdingBoy = false;
		return;
	case 1:
		if (_stage != Stage::Falling)
		{
			return;
		}
		// The camera chases the light from its start, looking at the boy
		_cameraChasing = true;
		if (_light.has_value())
		{
			_light->head = light::HeadPosition(*_light);
			_cameraOrigin = _light->head;
		}
		return;
	case 2:
		if (_stage == Stage::Falling)
		{
			_cameraChasing = false;
		}
		return;
	case 4:
		// The hand that lifted the boy, or a new one, stands ready at the beach with the setting-down clip
		MakeHand(k_SetDownClip);
		PlaceHand(rules::k_PutDownSpot, rules::PutDownYaw());
		_stage = Stage::SettingDown;
		return;
	case 5:
		if (_stage != Stage::SettingDown)
		{
			return;
		}
		if (_handPlaying)
		{
			// Again: the hand lets the boy go and jumps on to the end of its clip
			if (auto* hand = Entities().TryGet<IntroHand>(_hand); hand != nullptr)
			{
				hand->time = rules::k_LetGoMs;
			}
			_holdingBoy = false;
			return;
		}
		_handPlaying = true;
		_holdingBoy = true;
		return;
	default:
		return;
	}
}

void IntroSystem::ReleaseAll()
{
	_light.reset();
	FreeHand();
	_handPlaying = false;
	_cameraChasing = false;
	_stage = Stage::None;
	_lightDrawn = false;
}

void IntroSystem::MakeHand(std::string_view clipFile)
{
	auto& registry = Entities();
	const auto model = LoadMisc(Locator::resources::value().GetMeshes(), k_HandModel);
	const auto clip = LoadMisc(Locator::resources::value().GetAnimations(), clipFile);
	if (!model.has_value())
	{
		return;
	}
	if (_hand == entt::null || !registry.Valid(_hand))
	{
		_hand = registry.Create();
		registry.Assign<Transform>(_hand, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(_hand, *model, static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<AnimatedStaticPose>(_hand);
		registry.Assign<IntroHand>(_hand);
	}
	auto& hand = registry.Get<IntroHand>(_hand);
	hand.clip = clip.value_or(0);
	hand.time = 0;
	registry.SetDirty();
}

void IntroSystem::PlaceHand(const glm::vec3& position, float yaw)
{
	auto& registry = Entities();
	auto* hand = _hand != entt::null && registry.Valid(_hand) ? registry.TryGet<IntroHand>(_hand) : nullptr;
	if (hand == nullptr)
	{
		return;
	}
	hand->yaw = yaw;
	hand->scale = rules::k_HandScale;
	auto& transform = registry.Get<Transform>(_hand);
	transform.position = position;
	transform.rotation = glm::mat3(glm::eulerAngleY(-yaw));
	transform.scale = glm::vec3(rules::k_HandScale);
	registry.SetDirty();
}

void IntroSystem::FreeHand()
{
	if (_hand != entt::null && Locator::entitiesRegistry::has_value() && Entities().Valid(_hand) &&
	    Entities().AllOf<IntroHand>(_hand))
	{
		Entities().Destroy(_hand);
		Entities().SetDirty();
	}
	_hand = entt::null;
}

bool IntroSystem::AdvanceHand(uint32_t milliseconds)
{
	auto* hand = Entities().TryGet<IntroHand>(_hand);
	const auto* clip = hand != nullptr ? ClipOf(hand->clip) : nullptr;
	if (clip == nullptr)
	{
		return false;
	}
	bool cameRound = false;
	const auto time =
	    rules::AdvanceClip(hand->time, milliseconds, static_cast<int32_t>(clip->GetPlayTime()), clip->IsLooping(), cameRound);
	if (!cameRound)
	{
		hand->time = time;
	}
	return cameRound;
}

void IntroSystem::PoseHand()
{
	auto& registry = Entities();
	if (_hand == entt::null || !registry.Valid(_hand))
	{
		return;
	}
	const auto& hand = registry.Get<const IntroHand>(_hand);
	auto& pose = registry.Get<AnimatedStaticPose>(_hand);
	const auto* clip = ClipOf(hand.clip);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = registry.Get<const Mesh>(_hand).id;
	if (clip == nullptr || !meshes.Contains(meshId))
	{
		pose.bones.clear();
		return;
	}
	const auto model = meshes.Handle(meshId);
	const auto& frames = clip->GetFrames();
	const auto& parents = model->GetBoneParents();
	const auto span =
	    animals::SpanAt({.playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping()},
	                    static_cast<uint32_t>(std::max(hand.time, 0)));
	if (!model->IsBoned() || frames.empty() || span.to >= frames.size() || frames[span.from].bones.size() != parents.size() ||
	    frames[span.to].bones.size() != parents.size())
	{
		pose.bones.clear();
		return;
	}
	pose.bones.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, pose.bones);
	registry.SetDirty();
}

void IntroSystem::UpdateGrip()
{
	auto& registry = Entities();
	if (_hand == entt::null || !registry.Valid(_hand) || !_handPlaying)
	{
		return;
	}
	const auto& hand = registry.Get<const IntroHand>(_hand);
	const auto& pose = registry.Get<const AnimatedStaticPose>(_hand);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = registry.Get<const Mesh>(_hand).id;
	if (!meshes.Contains(meshId))
	{
		return;
	}
	const auto& point = meshes.Handle(meshId)->GetFirstBonePoint();
	if (!point.has_value())
	{
		return;
	}
	const auto& transform = registry.Get<const Transform>(_hand);
	const auto model = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
	                   glm::scale(glm::mat4(1.0f), transform.scale);
	// Past the time the hand lets go it holds no one, and the last place it held the boy is kept
	if (const auto grip = rules::GripPoint(hand.time, pose.bones, point->bone, point->point, model); grip.has_value())
	{
		_grip = grip;
	}
}

void IntroSystem::PlaceFollowers()
{
	auto& registry = Entities();
	const auto heldAt = _holdingBoy ? _grip : std::nullopt;
	registry.Each<HighDetail>([&registry, &heldAt](entt::entity /*entity*/, HighDetail& detail) {
		const auto drawnAt = detail.orders.followIntroHand ? heldAt : std::nullopt;
		if (detail.heldAt != drawnAt)
		{
			detail.heldAt = drawnAt;
			registry.SetDirty();
		}
	});
}

void IntroSystem::Update(uint32_t milliseconds)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	// Those following the hand are drawn where it held them as the last frame left it
	UpdateGrip();
	PlaceFollowers();
	_lightDrawn = false;
	const auto stepLight = [this, milliseconds]() {
		if (!_light.has_value())
		{
			return;
		}
		if (_light->state == light::State::Gone)
		{
			_light.reset();
			return;
		}
		light::Step(*_light, milliseconds, CrtRandom, _lightFrame);
		_lightDrawn = true;
	};
	switch (_stage)
	{
	case Stage::Falling:
	{
		if (!_light.has_value())
		{
			break;
		}
		if (_light->state == light::State::Gone)
		{
			_light.reset();
			break;
		}
		// Landed on the last frame: the hand comes to lift the boy
		const bool landed = _light->state == light::State::Holding;
		stepLight();
		if (_cameraChasing && _lightFrame.cameraAt.has_value())
		{
			_cameraOrigin = *_lightFrame.cameraAt;
		}
		if (landed)
		{
			_stage = Stage::Lifting;
		}
		break;
	}
	case Stage::Lifting:
	{
		if (_hand == entt::null || !Entities().Valid(_hand))
		{
			_liftGo = 0;
			MakeHand(k_LiftClip);
		}
		// Its clip waits until the light has gone, then plays once and holds its end
		if (_light.has_value() && _light->state == light::State::Gone)
		{
			_light.reset();
			_liftGo = rules::k_LiftGo;
		}
		else
		{
			stepLight();
		}
		if (_liftGo != 0 && AdvanceHand(milliseconds))
		{
			_liftFinished = true;
		}
		PlaceHand(rules::k_PickUpSpot, rules::k_PickUpYaw);
		break;
	}
	case Stage::SettingDown:
		// Drawn where its clip stands, then played on while it sets the boy down; it goes when its clip comes round
		if (_handPlaying && AdvanceHand(milliseconds))
		{
			FreeHand();
			_handPlaying = false;
			_stage = Stage::None;
		}
		break;
	case Stage::None:
		break;
	}
	PoseHand();
}

std::optional<IntroSystemInterface::CameraView> IntroSystem::GetCameraView() const
{
	if (!_cameraChasing)
	{
		return std::nullopt;
	}
	return CameraView {.origin = _cameraOrigin, .focus = rules::k_BoySpot};
}

const light::Frame* IntroSystem::GetLightFrame() const
{
	return _lightDrawn ? &_lightFrame : nullptr;
}

glm::vec3 IntroSystem::GetLightSortPoint() const
{
	return _light.has_value() ? _light->head : rules::k_BoySpot;
}

IntroSystemInterface::State IntroSystem::GetState() const
{
	State state {.stage = static_cast<int32_t>(_stage),
	             .cameraChasing = _cameraChasing,
	             .hand = _hand,
	             .handPlaying = _handPlaying,
	             .holdingBoy = _holdingBoy,
	             .grip = _grip,
	             .liftFinished = _liftFinished};
	if (_light.has_value())
	{
		state.light = _light->state;
		state.lightHead = _light->head;
		state.lightElapsed = _light->elapsed;
	}
	if (_hand != entt::null && Locator::entitiesRegistry::has_value() && Entities().Valid(_hand))
	{
		state.handClipTime = Entities().Get<const IntroHand>(_hand).time;
	}
	return state;
}
