/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ChimneySmokeSystem.h"

#include "3D/L3DMesh.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::Abode;
using openblack::ecs::components::ChimneySmoke;
using openblack::ecs::components::Transform;
using openblack::ecs::components::Workshop;

namespace
{
chimney_smoke::Random GameRandom()
{
	return [](float a, float b) { return Locator::gameRandom::value().CrtRandom(a, b); };
}

/// The hand of the player at this computer in the world, if it is in it, as the reports of the player put it. Only the
/// smoke that this computer draws moves in the air of this hand.
std::optional<glm::vec3> HandPosition()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::handSystem::value().GetOwnHandPositions()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}
} // namespace

void ChimneySmokeSystem::Attach(entt::entity abode, const graphics::L3DMesh& mesh, const Transform& transform, bool workshop)
{
	const auto& point = mesh.GetChimneyPos();
	if (!point.has_value())
	{
		return;
	}
	// The chimney's top through the home's turn and size, at its place
	const auto chimney = transform.position + (transform.rotation * (*point * transform.scale));
	Locator::entitiesRegistry::value().Assign<ChimneySmoke>(
	    abode,
	    chimney_smoke::Create(chimney, workshop ? chimney_smoke::k_WorkshopSmoke : chimney_smoke::k_HomeSmoke, GameRandom()));
}

void ChimneySmokeSystem::ProcessTurn()
{
	const auto position = HandPosition();
	if (!position)
	{
		return;
	}
	if (_lastHandPosition)
	{
		const auto millisecondsPerTurn = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration).count();
		_handVelocity = chimney_smoke::EaseHandVelocity(_handVelocity, *position - *_lastHandPosition, millisecondsPerTurn);
	}
	_lastHandPosition = position;
}

void ChimneySmokeSystem::Update(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime)
{
	const auto step = frame_pace::Step(_pace, frameRealTime, frameGameTime);
	if (step <= std::chrono::milliseconds {0})
	{
		return;
	}
	if (const auto position = HandPosition())
	{
		_handWind = chimney_smoke::WindOf(*position, _handVelocity);
	}
	const auto random = GameRandom();
	const auto& registry = Locator::entitiesRegistry::value();
	Locator::entitiesRegistry::value().Each<const Abode, ChimneySmoke>(
	    [&](entt::entity entity, const Abode& abode, ChimneySmoke& smoke) {
		    // Lit while someone is home, or while a workshop makes a scaffold
		    const auto* workshop = registry.TryGet<const Workshop>(entity);
		    const bool makes = workshop != nullptr && workshop->makingTurns != 0;
		    if (!chimney_smoke::UpdateState(smoke, chimney_smoke::IsLit(abode.presentAtHome > 0, makes)))
		    {
			    return;
		    }
		    chimney_smoke::Advance(smoke, step, chimney_smoke::Drift(smoke.chimney, _handWind, random));
	    });
	// A smoke that is not on a home (for example, the smoke of a creature fart) continues until something removes it
	Locator::entitiesRegistry::value().Each<ChimneySmoke>(
	    [&](ChimneySmoke& smoke) {
		    if (chimney_smoke::UpdateState(smoke, true))
		    {
			    chimney_smoke::Advance(smoke, step, chimney_smoke::Drift(smoke.chimney, _handWind, random));
		    }
	    },
	    entt::exclude<Abode>);
}
