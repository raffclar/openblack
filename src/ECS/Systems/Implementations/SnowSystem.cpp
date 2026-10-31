/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SnowSystem.h"

#include <algorithm>
#include <iterator>

#include "3D/L3DMesh.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Graphics/MeshDetail.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack::ecs::systems;
using openblack::ecs::components::BrokenModels;
using openblack::ecs::components::BuildingDamage;
using openblack::ecs::components::BuildingPiece;
using openblack::ecs::components::Mesh;
using openblack::ecs::components::SnowLevel;
using openblack::ecs::components::Storm;
using openblack::ecs::components::Transform;

namespace
{
/// A game turn, in seconds
constexpr float k_TurnSeconds = 0.1f;
} // namespace

SnowSystem::SnowSystem()
    : _depths(snow_cover::k_Cells, 0.0f)
{
}

void SnowSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _depths);
	reflection::Save(out, _melting);
	reflection::SavePresentation(out, _objectPace);
}

bool SnowSystem::LoadState(reflection::BinaryReader& in)
{
	std::vector<float> depths;
	reflection::Load(in, depths);
	reflection::Load(in, _melting);
	reflection::Load(in, _objectPace);
	if (in.Failed() || depths.size() != _depths.size())
	{
		return false;
	}
	_depths = std::move(depths);
	++_revision;
	return true;
}

void SnowSystem::Reset()
{
	std::ranges::fill(_depths, 0.0f);
	_melting = {};
	_objectPace = {};
	++_revision;
}

void SnowSystem::ProcessTurn(std::span<const Storm> storms)
{
	for (const auto& storm : storms)
	{
		const auto amount = snow_cover::StormSnowPerTurn(storm.effect.snow, storm.currentStrength);
		if (amount != 0.0f)
		{
			snow_cover::AddStorm(_depths, {storm.currentPosition.x, storm.currentPosition.z}, storm.currentInnerRadius,
			                     storm.outerRadius, amount);
		}
	}
	snow_cover::Melt(_depths, _melting, k_TurnSeconds);
	++_revision;
}

void SnowSystem::Cover(float depth)
{
	std::ranges::fill(_depths, std::clamp(depth, 0.0f, snow_cover::k_MaxDepth));
	++_revision;
}

void SnowSystem::StepObjects(Registry& registry, const ObjectDrawListSystemInterface& drawList,
                             std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime)
{
	// In each frame: a building that breaks gives its snow to its broken model, and a new piece takes the snow of the
	// broken model that it broke off. A tree that the frame draws out of the land carries less snow from then on.
	registry.Each<const BuildingDamage>([&registry](entt::entity building, const BuildingDamage&) {
		const auto* models = registry.TryGet<const BrokenModels>(building);
		snow_levels::FollowBreak(registry, building, true, models != nullptr ? models->made : 0);
	});
	registry.Each<SnowLevel>([&registry](entt::entity entity, SnowLevel& snow) {
		if (snow.broken.has_value() && !registry.AllOf<BuildingDamage>(entity))
		{
			snow.broken.reset();
		}
	});
	_newPieces.clear();
	registry.Each<const BuildingPiece>([this, &registry](entt::entity piece, const BuildingPiece& part) {
		if (!registry.AllOf<SnowLevel>(piece))
		{
			_newPieces.emplace_back(piece, part.parent);
		}
	});
	for (const auto& [piece, building] : _newPieces)
	{
		snow_levels::TakePieceSnow(registry, piece, building);
	}
	snow_levels::NoteTreesOutOfLand(registry);

	const auto gameTime = water_rings::Step(_objectPace, frameRealTime, frameGameTime);
	const auto step = snow_cover::LevelStep(static_cast<int32_t>(gameTime.count()));
	const auto depthAt = [this](glm::vec2 xz) { return GetDepth(xz); };
	// The phantom buildings of the scaffolds are not in the list: a site or a hand shows them
	snow_levels::StepScaffoldPhantoms(registry, depthAt, step);
	if (step <= 0 || !drawList.GetView().has_value())
	{
		return;
	}
	const auto* buildings = Locator::buildingDamageSystem::has_value() ? &Locator::buildingDamageSystem::value() : nullptr;
	_drawn.clear();
	_broken.clear();
	for (const auto entity : drawList.GetList())
	{
		if (!drawList.Drawn(registry, entity))
		{
			continue;
		}
		// A broken building keeps its own snow. Only the snow of its broken model changes, while the model shows on the
		// screen.
		if (registry.AllOf<BuildingDamage>(entity))
		{
			if (BrokenModelOnScreen(registry, *drawList.GetView(), entity))
			{
				_broken.push_back(entity);
			}
			continue;
		}
		// The site of a building draws it only as far up as it stands, and keeps its snow as it is
		if (buildings != nullptr && !buildings->DrawsWhole(entity))
		{
			continue;
		}
		_drawn.push_back(entity);
	}
	snow_levels::Step(registry, _drawn, depthAt, step);
	snow_levels::StepBroken(registry, _broken, depthAt, step);
}

bool SnowSystem::BrokenModelOnScreen(const Registry& registry, const object_draw_list::View& view, entt::entity building)
{
	// The sphere of the whole model of the building, as the building stands
	const auto* mesh = registry.TryGet<const Mesh>(building);
	const auto* transform = registry.TryGet<const Transform>(building);
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return true;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return true;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	const auto centre = transform->position + transform->rotation * (transform->scale * box.Center());
	const float radius = std::abs(transform->scale.x) * graphics::mesh_detail::HalfDiagonal(box.Size());
	return object_draw_list::SphereOnScreen(view, transform->position, centre, radius);
}
