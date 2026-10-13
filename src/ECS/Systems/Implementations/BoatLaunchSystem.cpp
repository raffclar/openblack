/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "BoatLaunchSystem.h"

#include <exception>
#include <string>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "Audio/ScriptSoundEffect.h"
#include "Blast/DustPuff.h"
#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::boat_launch;

namespace
{
constexpr entt::hashed_string k_SmokeSheet = entt::hashed_string("raw/smoke");
constexpr entt::hashed_string k_SmokeSheetAlpha = entt::hashed_string("raw/smokea");

auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// A clip of the misc folder, loaded into the animation cache the first time
std::optional<entt::id_type> LoadMiscClip(std::string_view file)
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& cache = Locator::resources::value().GetAnimations();
	const auto id = resources::HashIdentifier("misc/" + std::string(file));
	if (cache.Contains(id))
	{
		return id;
	}
	const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / file;
	try
	{
		cache.Load(id, resources::L3DAnimLoader::FromDiskTag {}, path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The explorers' boat: cannot load {}: {}", path.generic_string(), e.what());
		return std::nullopt;
	}
	return id;
}

const L3DAnim* ClipOf(entt::id_type id)
{
	const auto& animations = Locator::resources::value().GetAnimations();
	return id != 0 && animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}

entt::id_type ClipId(AnimId clip)
{
	return resources::HashIdentifier(clip);
}

/// The clip's root bone at a time of it, blended between the keyframes about it as the boat's clips are
std::optional<glm::mat4> RootAt(entt::id_type clipId, int32_t time)
{
	const auto* clip = ClipOf(clipId);
	if (clip == nullptr)
	{
		return std::nullopt;
	}
	const auto& frames = clip->GetFrames();
	const auto span =
	    animals::SpanAt({.playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping()},
	                    static_cast<uint32_t>(std::max(time, 0)));
	if (frames.empty() || span.to >= frames.size() || frames[span.from].bones.empty() || frames[span.to].bones.empty())
	{
		return std::nullopt;
	}
	return rules::RootBetween(frames[span.from].bones.front(), frames[span.to].bones.front(), span.t);
}

/// A thing of the film posed in its clip at a time, which comes round at the clip's length
void Pose(entt::entity entity, AnimId clipName, int32_t time)
{
	auto& registry = Entities();
	auto& pose = registry.Get<AnimatedStaticPose>(entity);
	auto& part = registry.Get<BoatFilmPart>(entity);
	const auto* clip = ClipOf(ClipId(clipName));
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = registry.Get<const Mesh>(entity).id;
	if (clip == nullptr || clip->GetPlayTime() == 0 || !meshes.Contains(meshId))
	{
		pose.bones.clear();
		return;
	}
	const auto model = meshes.Handle(meshId);
	const auto& frames = clip->GetFrames();
	const auto& parents = model->GetBoneParents();
	part.clipTime = std::max(time, 0) % static_cast<int32_t>(clip->GetPlayTime());
	const auto span =
	    animals::SpanAt({.playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping()},
	                    static_cast<uint32_t>(part.clipTime));
	if (!model->IsBoned() || frames.empty() || span.to >= frames.size() || frames[span.from].bones.size() != parents.size() ||
	    frames[span.to].bones.size() != parents.size())
	{
		pose.bones.clear();
		return;
	}
	pose.bones.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, pose.bones);
}

/// A thing of the film drawn by a matrix
void Place(entt::entity entity, const glm::mat4& model)
{
	auto& registry = Entities();
	registry.Get<DrawnMatrix>(entity).model = model;
	auto& transform = registry.Get<Transform>(entity);
	transform.position = glm::vec3(model[3]);
	transform.rotation = glm::mat3(model);
}

float CrtRandom(float a, float b)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().CrtRandom(a, b) : (a + b) * 0.5f;
}

float LandHeight(glm::vec2 xz)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

/// The wake's sprites: the smoke sheet, eight cells a row, lying on the sea, blended over what is behind
const particles::Creator& WakeCreator()
{
	static const particles::Creator creator = [] {
		particles::Creator wake;
		wake.kind = particles::Creator::Kind::Sprite;
		wake.className = "BoatWake";
		wake.texture = "smoke";
		wake.spritesPerRow = 8;
		wake.numFrames = 64;
		wake.horizontal = true;
		wake.additive = false;
		return wake;
	}();
	return creator;
}
} // namespace

void BoatLaunchSystem::Play()
{
	Play(Places {});
}

void BoatLaunchSystem::Play(const Places& places)
{
	// One plays at a time: a new film takes the place of one still playing
	Stop();
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	_launchClip = LoadMiscClip(rules::k_LaunchClip).value_or(0);
	_sailingClip = LoadMiscClip(rules::k_SailingClip).value_or(0);
	_places = places;
	_clocks = {};
	_playing = true;
	MakeParts();
	_boat = BoatMatrix();
	PlaceParts();
}

void BoatLaunchSystem::Stop()
{
	FreeParts();
	_playing = false;
	_boat.reset();
	_clocks = {};
}

void BoatLaunchSystem::MakeParts()
{
	auto& registry = Entities();
	const auto make = [&registry, this](MeshId mesh, BoatFilmPart::Role role, size_t index, bool animated) {
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<DrawnMatrix>(entity);
		// The boat casts its shadow only on the beach
		const bool castsShadow = role == BoatFilmPart::Role::Boat && _clocks.stage == rules::Stage::Launch;
		registry.Assign<BoatFilmPart>(entity, role, static_cast<uint8_t>(index), 0, castsShadow);
		if (animated)
		{
			registry.Assign<AnimatedStaticPose>(entity);
		}
		_parts.push_back(entity);
	};
	make(MeshId::ObjectArk, BoatFilmPart::Role::Boat, 0, false);
	if (_clocks.stage == rules::Stage::Launch)
	{
		for (size_t i = 0; i < rules::k_LaunchSailors; ++i)
		{
			make(MeshId::PersonNorseSailor, BoatFilmPart::Role::LaunchSailor, i, true);
		}
	}
	else
	{
		const auto& deck = rules::DeckParts();
		for (size_t i = 0; i < deck.size(); ++i)
		{
			make(deck.at(i).mesh, BoatFilmPart::Role::DeckPart, i, deck.at(i).clip.has_value());
		}
	}
	registry.SetDirty();
}

void BoatLaunchSystem::FreeParts()
{
	if (!_parts.empty() && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Entities();
		for (const auto entity : _parts)
		{
			if (registry.Valid(entity) && registry.AllOf<BoatFilmPart>(entity))
			{
				registry.Destroy(entity);
			}
		}
		registry.SetDirty();
	}
	_parts.clear();
}

std::optional<glm::mat4> BoatLaunchSystem::BoatMatrix() const
{
	if (_clocks.stage == rules::Stage::Launch)
	{
		const auto root = RootAt(_launchClip, _clocks.boatTime);
		if (!root.has_value())
		{
			return std::nullopt;
		}
		// The boat keeps to the land as it runs down the beach: lifted by the land under it less that under the dock
		const auto place = rules::LaunchPlace(*root, _places.dock);
		const float lift = LandHeight({place.x, place.z}) - LandHeight({_places.dock.x, _places.dock.z});
		return rules::LaunchMatrix(*root, _places.dock, lift);
	}
	const auto root = RootAt(_sailingClip, _clocks.boatTime);
	if (!root.has_value())
	{
		return std::nullopt;
	}
	return rules::SailingMatrix(*root, _places.sailingStart, _clocks.elapsed);
}

void BoatLaunchSystem::LaunchEffects()
{
	if (Locator::chlapi::has_value())
	{
		for (const auto sample : rules::SoundsPassed(_clocks.previousBoatTime, _clocks.boatTime))
		{
			Locator::chlapi::value().PlayBankSoundEffect(static_cast<int32_t>(audio::ScriptSoundBank::ScriptSfx), sample,
			                                             std::nullopt);
		}
	}
	if (!rules::TakeSpray(_clocks) || !_boat.has_value())
	{
		return;
	}
	const auto spray = rules::SprayAt(_clocks.boatTime);
	if (!spray.has_value() || !Locator::explosionSystem::has_value())
	{
		return;
	}
	const auto boat = glm::vec3((*_boat)[3]);
	for (int puff = 0; puff < 2; ++puff)
	{
		// Along the beach is drawn before across it
		const float along = CrtRandom(-rules::k_SpraySpreadZ, rules::k_SpraySpreadZ);
		const float across = CrtRandom(-rules::k_SpraySpreadX, rules::k_SpraySpreadX);
		const glm::vec3 place {(across + boat.x) + spray->offset.x, boat.y + spray->offset.y, boat.z + along};
		Locator::explosionSystem::value().AddSmoke(place, spray->size, dust_puff::Kind::Smoke, spray->colour);
	}
}

void BoatLaunchSystem::PlaceParts()
{
	if (_parts.empty() || !_boat.has_value())
	{
		return;
	}
	auto& registry = Entities();
	Place(_parts.front(), *_boat);
	for (size_t i = 1; i < _parts.size(); ++i)
	{
		const auto entity = _parts.at(i);
		if (_clocks.stage == rules::Stage::Launch)
		{
			const auto sailor = rules::LaunchSailor(i - 1, _clocks, _places.dock);
			const glm::vec3 at {sailor.xz.x, LandHeight(sailor.xz), sailor.xz.y};
			Place(entity, glm::translate(glm::mat4(1.0f), at) * glm::eulerAngleY(-sailor.heading));
			Pose(entity, sailor.clip, sailor.clipTime);
			continue;
		}
		const auto& part = rules::DeckParts().at(i - 1);
		Place(entity, rules::OnDeck(part, *_boat));
		if (part.clip.has_value())
		{
			Pose(entity, *part.clip, _clocks.elapsed + part.clipOffset);
		}
	}
	registry.SetDirty();
}

void BoatLaunchSystem::Update(uint32_t milliseconds)
{
	if (!_playing || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto* launch = ClipOf(_launchClip);
	const auto* sailing = ClipOf(_sailingClip);
	const rules::ClipLengths lengths {
	    .launch = launch != nullptr ? static_cast<int32_t>(launch->GetPlayTime()) : 0,
	    .sailing = sailing != nullptr ? static_cast<int32_t>(sailing->GetPlayTime()) : 0,
	    .sailingLoops = sailing == nullptr || sailing->IsLooping(),
	};
	switch (rules::Advance(_clocks, static_cast<int32_t>(milliseconds), lengths))
	{
	case rules::Step::Ended:
		Stop();
		return;
	case rules::Step::SailingBegan:
		// The boat at sea and its deck are drawn from the next frame
		FreeParts();
		_boat.reset();
		return;
	case rules::Step::Continues:
		break;
	}
	if (_parts.empty())
	{
		MakeParts();
	}
	_boat = BoatMatrix();
	if (_clocks.stage == rules::Stage::Launch)
	{
		LaunchEffects();
	}
	PlaceParts();
}

void BoatLaunchSystem::CollectDrawFrame(particles::draw::Frame& frame) const
{
	if (!_playing || _clocks.stage != rules::Stage::Sailing || !_boat.has_value() || _parts.empty())
	{
		return;
	}
	const particles::draw::Sources sources {
	    .textures = [](std::string_view /*texture*/) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		    return std::pair {k_SmokeSheet.value(), k_SmokeSheetAlpha.value()};
	    },
	    .playerColour = {},
	    .random = {},
	};
	particles::Effect::DrawWalk walk;
	for (const auto phase : _clocks.wakes)
	{
		const auto wake = rules::WakeAt(phase, *_boat);
		if (!wake.shown)
		{
			continue;
		}
		// Each is sorted by itself among what blends
		walk.Clear();
		walk.steps.push_back({.chain = false, .index = 0});
		walk.atoms.push_back({
		    .creator = &WakeCreator(),
		    .position = wake.position,
		    // The sprite's turn is read from the rotation about the vertical
		    .rotation = glm::mat3(glm::eulerAngleY(-wake.angle)),
		    .scale = wake.halfWidth,
		    .stretch = wake.aspect,
		    .alpha = static_cast<float>(wake.alpha),
		    .frame = static_cast<float>(wake.cell),
		    .rgb = {0xFF, 0xFF, 0xFF},
		});
		particles::draw::AddEffect(frame, walk, particles::draw::DrawPath::Sorted, wake.position,
		                           particles::draw::k_NeutralPlayer, sources);
	}
}

BoatLaunchSystemInterface::State BoatLaunchSystem::GetState() const
{
	return {.playing = _playing,
	        .clocks = _clocks,
	        .places = _places,
	        .boat = _boat,
	        .parts = static_cast<uint32_t>(_parts.size())};
}
