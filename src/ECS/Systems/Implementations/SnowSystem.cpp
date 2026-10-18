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

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"

using namespace openblack::ecs::systems;
using openblack::ecs::components::Storm;

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
	reflection::Save(out, _objectPace);
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
	const auto gameTime = water_rings::Step(_objectPace, frameRealTime, frameGameTime);
	const auto step = snow_cover::LevelStep(static_cast<int32_t>(gameTime.count()));
	if (step <= 0 || !drawList.GetView().has_value())
	{
		return;
	}
	_drawn.clear();
	std::ranges::copy_if(drawList.GetList(), std::back_inserter(_drawn),
	                     [&registry, &drawList](entt::entity entity) { return drawList.Drawn(registry, entity); });
	snow_levels::Step(registry, _drawn, [this](glm::vec2 xz) { return GetDepth(xz); }, step);
}
