/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ExplosionSystem.h"

#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleBlast.h"
#include "Particles/ParticleDrawFrame.h"
#include "Physics/ObjectRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The dust puff's size
constexpr float k_DustSize = 1.0f;
constexpr entt::hashed_string k_SmokeSheet = entt::hashed_string("raw/smoke");
constexpr entt::hashed_string k_SmokeSheetAlpha = entt::hashed_string("raw/smokea");

float LocalRandom(float a, float b)
{
	return Locator::gameRandom::has_value() ? a + Locator::gameRandom::value().LocalFloatRand(b - a) : (a + b) * 0.5f;
}

/// The dust's sprites: the smoke sheet, eight cells a row, blended over what is behind
const particles::Creator& DustCreator()
{
	static const particles::Creator creator = [] {
		particles::Creator dust;
		dust.kind = particles::Creator::Kind::Sprite;
		dust.className = "Dust";
		dust.texture = "smoke";
		dust.spritesPerRow = 8;
		dust.numFrames = 64;
		dust.additive = false;
		return dust;
	}();
	return creator;
}
/// Half the size of a model's box, in its own units
glm::vec3 MeshHalfSize(MeshId mesh)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = resources::HashIdentifier(mesh);
	return meshes.Contains(id) ? meshes.Handle(id)->GetBoundingBox().Size() * 0.5f : glm::vec3(0.0f);
}

/// A dead tree just fallen drops its roots: as it lies, a share of its model's half width in size, falling from where it
/// lies to just above the land
ExplosionSystemInterface::GroundMark DroppedRoots(const ecs::Registry& registry, entt::entity deadTree)
{
	namespace objects = physics::objects;
	const auto& transform = registry.Get<const Transform>(deadTree);
	const auto type = registry.Get<const DeadTree>(deadTree).type;
	const auto treeMesh = Locator::infoConstants::value().tree.at(static_cast<size_t>(type)).normal;
	const float scale = transform.scale.x * objects::k_RootsScale * MeshHalfSize(treeMesh).x;
	const auto rootsHalf = MeshHalfSize(MeshId::TreeRoots);
	const float land = Locator::terrainSystem::value().GetHeightAt({transform.position.x, transform.position.z});
	return {
	    .mesh = resources::HashIdentifier(MeshId::TreeRoots),
	    .transform = {.position = transform.position, .rotation = transform.rotation, .scale = glm::vec3(scale)},
	    .millisecondsLeft = 0.0f,
	    .alpha = std::nullopt,
	    .morphWithTerrain = false,
	    .falling =
	        ExplosionSystemInterface::GroundMark::Falling {
	            .seconds = 0.0f,
	            .startHeight = transform.position.y,
	            .restHeight = land + objects::k_RootsRestShare * std::max(rootsHalf.x, rootsHalf.z) * scale,
	        },
	};
}
} // namespace

void ExplosionSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _shakes);
	reflection::Save(out, _puffs);
}

bool ExplosionSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _shakes);
	reflection::Load(in, _puffs);
	// A load starts with no piles of roots, as a new land does
	_piles.clear();
	return !in.Failed();
}

void ExplosionSystem::AddRubble(const glm::vec3& centre, const glm::mat3& rotation, float scale)
{
	_piles.push_back({.position = centre, .rotation = rotation, .scale = scale});
	_puffs.push_back(dust_puff::Make(centre, k_DustSize, LocalRandom));
	// The draw lists make room for it
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().SetDirty();
	}
}

void ExplosionSystem::AgeRootsPiles(std::chrono::milliseconds gameTime)
{
	const auto milliseconds = static_cast<int32_t>(gameTime.count());
	std::erase_if(_piles, [milliseconds](roots_pile::Pile& pile) { return !roots_pile::Age(pile, milliseconds); });
}

void ExplosionSystem::AddSmoke(const glm::vec3& centre, float size, dust_puff::Kind kind, std::optional<uint32_t> colour)
{
	_puffs.push_back(dust_puff::Make(centre, size, LocalRandom, kind, colour));
}

void ExplosionSystem::AddShake(const glm::vec3& position, float radius, float strength, int32_t milliseconds, bool verticalOnly)
{
	_shakes.push_back({.position = position,
	                   .radius = radius,
	                   .strength = strength,
	                   .milliseconds = milliseconds,
	                   .millisecondsLeft = milliseconds,
	                   .verticalOnly = verticalOnly});
}

void ExplosionSystem::Update(float milliseconds)
{
	const float seconds = milliseconds / 1000.0f;
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		bool changed = false;
		// A dead tree drops its roots the first time that a frame draws it. The mark of the dead tree is only drawn.
		if (Locator::terrainSystem::has_value() && Locator::infoConstants::has_value() && Locator::resources::has_value())
		{
			std::vector<entt::entity> fallen;
			registry.Each<const DropsRoots, const DeadTree, const Transform>(
			    [&fallen](entt::entity entity, const DeadTree&, const Transform&) { fallen.push_back(entity); });
			for (const auto deadTree : fallen)
			{
				registry.Remove<DropsRoots>(deadTree);
				_marks.push_back(DroppedRoots(registry, deadTree));
				changed = true;
			}
		}
		const auto before = _marks.size();
		// The falling roots drop, settle, fade and go by the game's time
		std::erase_if(_marks, [&changed, seconds](GroundMark& mark) {
			if (!mark.falling.has_value())
			{
				return true;
			}
			namespace objects = physics::objects;
			mark.falling->seconds += seconds;
			if (objects::RootsGone(mark.falling->seconds))
			{
				return true;
			}
			mark.transform.position.y = objects::RootsHeight(mark.falling->startHeight, mark.falling->restHeight,
			                                                 mark.falling->seconds);
			mark.alpha = objects::RootsAlpha(mark.falling->seconds);
			changed = true;
			return false;
		});
		if (changed || _marks.size() != before)
		{
			registry.SetDirty();
		}
	}
	// A destroyed building's ghost runs down by the frames' time once drawn, and goes as it reaches nothing
	std::erase_if(_ghosts, [milliseconds](DestructionGhost& ghost) {
		if (ghost.shown)
		{
			ghost.millisecondsLeft = std::max(ghost.millisecondsLeft - static_cast<int>(milliseconds), 0);
			if (ghost.millisecondsLeft == 0)
			{
				return true;
			}
		}
		ghost.shown = true;
		return false;
	});
	std::erase_if(_puffs, [seconds](dust_puff::Puff& puff) { return !dust_puff::Advance(puff, seconds); });
	// The shakes count down by the real time of the frame, also when the game pauses
	const auto realMilliseconds = Locator::time::has_value() ? Locator::time::value().GetFrameRealTime().count() : 0;
	camera_shake::Advance(_shakes, static_cast<int32_t>(realMilliseconds));
	if (Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		// The distance to a shake is from the eye that the game drew, with its shake
		const auto eye = camera.GetOrigin(Camera::Interpolation::Drawn);
		const auto* nearest = camera_shake::Nearest(_shakes, eye);
		const auto offsets = camera_shake::Jitter(camera_shake::Amplitude(_shakes, eye),
		                                          nearest == nullptr || nearest->verticalOnly, LocalRandom);
		camera.SetShake(offsets.eye, offsets.focus);
	}
}

void ExplosionSystem::CollectDrawFrame(particles::draw::Frame& frame) const
{
	if (_puffs.empty())
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
	for (const auto& puff : _puffs)
	{
		const auto rgb =
		    std::array<uint8_t, 3> {static_cast<uint8_t>(puff.colour >> 16u), static_cast<uint8_t>((puff.colour >> 8u) & 0xFFu),
		                            static_cast<uint8_t>(puff.colour & 0xFFu)};
		walk.Clear();
		for (const auto& sprite : dust_puff::Look(puff))
		{
			walk.steps.push_back({.chain = false, .index = static_cast<uint32_t>(walk.atoms.size())});
			walk.atoms.push_back({
			    .creator = &DustCreator(),
			    .position = sprite.position,
			    // The sprite's roll is read from the rotation about the vertical
			    .rotation = glm::mat3(glm::eulerAngleY(-sprite.angle)),
			    .scale = sprite.halfWidth,
			    .stretch = 1.0f,
			    .alpha = sprite.alpha,
			    .frame = static_cast<float>(sprite.cell),
			    .rgb = rgb,
			});
		}
		particles::draw::AddEffect(frame, walk, particles::draw::DrawPath::Sorted, puff.positions.front(),
		                           particles::draw::k_NeutralPlayer, sources);
	}
}

void ExplosionSystem::Reset()
{
	_shakes.clear();
	_puffs.clear();
	_ghosts.clear();
	_piles.clear();
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetShake(glm::vec3(0.0f), glm::vec3(0.0f));
	}
	_marks.clear();
}
