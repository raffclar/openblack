/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature that draws a gesture in the air. It points at the start of the gesture's shape, on an upright square in
// front of its head. Each turn, the point moves along the shape, and a glowing chain follows its hand. At the end of the
// shape, the arm holds still for some turns, then the point ends. A step of an agenda or a move of a dance starts it.

#define LOCATOR_IMPLEMENTATIONS

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "Creature/CreatureDance.h"
#include "Creature/CreatureGesture.h"
#include "CreatureBodyBusy.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureGestureDrawing.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Particles/DrawnEffectSteps.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
using Stage = CreatureGestureDrawing::Stage;
using Outcome = CreatureGestureDrawing::Outcome;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t GameTurn()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}

/// Where the creature points for the share of the shape that it has drawn
glm::vec3 TargetOf(const ecs::Registry& registry, entt::entity creature, const CreatureGestureDrawing& drawing)
{
	const auto& transform = registry.Get<const Transform>(creature);
	// Its heading, or the way its rotation faces when it does not walk. The meshes look back along their z axis.
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	const auto ahead = glm::xz(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	const auto heading = walk != nullptr ? walk->heading : GameHeading::Of(ahead.x, ahead.y);
	const auto point = creature_gesture::SamplePath(drawing.shape, drawing.drawing.drawn);
	return creature_gesture::HandTarget(mind_detail::HeadPoint(registry, creature), heading,
	                                    object_measures::TwoDRadius(registry, creature), point);
}

/// The chain follows the hand while the arm points; else it waits at the creature's feet, and makes nothing. Its size
/// follows the size that the creature has grown to.
void FeedChain(const CreatureGestureDrawing& drawing, const Creature& body, const std::optional<glm::vec3>& hand,
               glm::vec3 feet)
{
	if (Locator::effectRequestSystem::has_value())
	{
		auto& effects = Locator::effectRequestSystem::value();
		effects.SetMagnitude(drawing.chain, creature_gesture::ChainMagnitude(body.size));
		effects.SetProcessInfo(drawing.chain, {.handPosition = hand.value_or(feet), .enabled = hand.has_value()});
	}
}

void StopChain(CreatureGestureDrawing& drawing)
{
	if (!Locator::effectRequestSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::effectRequestSystem::value();
	effects.SetEnabled(drawing.chain, false);
	effects.Stop(drawing.chain);
}

/// The chain starts at the start of the shape as the arm starts to point, in its player's colour. Its size is the size
/// that the creature has grown to. A chain that the last drawing left goes first.
void StartChain(const ecs::Registry& registry, entt::entity creature, CreatureGestureDrawing& drawing, glm::vec3 start)
{
	if (!Locator::effectRequestSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::effectRequestSystem::value();
	effects.Remove(drawing.chain);
	const auto& body = registry.Get<const Creature>(creature);
	drawing.chain = effects.Start({
	    .type = ParticleType::CreatureGesture,
	    .position = start,
	    .magnitude = creature_gesture::ChainMagnitude(body.size),
	    .synced = false,
	    .step = EffectStep::Frame,
	    .player = static_cast<int>(body.owner),
	    .processInfo = particles::ProcessInfo {.handPosition = glm::vec3(0.0f), .enabled = false},
	});
}
} // namespace

void CreatureMindSystem::WatchGestureChainOwners()
{
	auto& registry = Entities();
	if (_gestureChainOwners == &registry)
	{
		return;
	}
	_gestureChainOwners = &registry;
	_gestureChainOwnersGo = registry.OnDestroying().connect<&CreatureMindSystem::GestureChainOwnerGoes>(*this);
}

void CreatureMindSystem::GestureChainOwnerGoes(entt::entity creature)
{
	const auto* drawing = Entities().TryGet<const CreatureGestureDrawing>(creature);
	if (drawing != nullptr && drawing->chain != k_NoEffectHandle && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Remove(drawing->chain);
	}
}

void CreatureMindSystem::UpdateDrawnGestureChains(const ObjectDrawListSystemInterface& drawList,
                                                  std::chrono::milliseconds frameRealTime,
                                                  std::chrono::milliseconds frameGameTime, bool landDrawn)
{
	WatchGestureChainOwners();
	const auto seconds = particles::drawn_steps::StepSeconds(_gestureChainPace, frameRealTime, frameGameTime, landDrawn);
	if (!Locator::particleRuleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleRuleSystem::value();
	auto& registry = Entities();
	std::vector<entt::entity> creatures;
	registry.Each<const CreatureGestureDrawing>([&creatures](entt::entity entity, const CreatureGestureDrawing& drawing) {
		if (drawing.chain != k_NoEffectHandle)
		{
			creatures.push_back(entity);
		}
	});
	const auto order = particles::drawn_steps::DrawOrder(
	    drawList.GetList(), creatures, drawList.GetView().has_value(),
	    [&registry, &drawList](entt::entity entity) { return drawList.Decides(registry, entity); });
	const auto stepChain = [&](entt::entity creature, bool drawn) {
		auto& drawing = registry.Get<CreatureGestureDrawing>(creature);
		const auto frame = creature_gesture::ChainFrameStep(drawn, seconds);
		particles.SetShown(drawing.chain, frame.shown);
		if (!frame.seconds.has_value())
		{
			return;
		}
		// The chain shows all of its last step, with no blend from the step before
		particles.SetDrawnAtLastStep(drawing.chain);
		// A chain that faded out is gone
		if (!particles.ProcessByFrame(drawing.chain, *frame.seconds))
		{
			drawing.chain = k_NoEffectHandle;
		}
	};
	for (const auto creature : order)
	{
		stepChain(creature, drawList.Drawn(registry, creature));
	}
	// A creature that is not on the land's list is not drawn, so its chain does not show
	for (const auto creature : particles::drawn_steps::LeftOut(creatures, order))
	{
		stepChain(creature, false);
	}
}

bool CreatureMindSystem::DrawGesture(entt::entity creature, uint32_t gesture)
{
	return StartGesture(creature, gesture, false);
}

bool CreatureMindSystem::StartGesture(entt::entity creature, uint32_t gesture, bool inDance)
{
	auto& registry = Entities();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature) ||
	    !Locator::creatureObjectActionSystem::has_value())
	{
		return false;
	}
	auto* drawing = registry.TryGet<CreatureGestureDrawing>(creature);
	if (drawing == nullptr)
	{
		// The counter counts every turn of the creature's life, so it is well past the start's wait
		drawing = &registry.Assign<CreatureGestureDrawing>(
		    creature, CreatureGestureDrawing {.drawing = {.drawn = 0.0f, .turns = creature_gesture::k_HoldTurns},
		                                      .countedTurn = GameTurn()});
	}
	WatchGestureChainOwners();
	if (drawing->stage != Stage::None)
	{
		return false;
	}
	const auto type = static_cast<GestureType>(gesture);
	const auto symbol = Locator::gestureSystem::has_value() ? Locator::gestureSystem::value().GetSymbol(type) : std::nullopt;
	const auto reference =
	    Locator::gestureSystem::has_value()
	        ? Locator::gestureSystem::value().GetSymbol(static_cast<GestureType>(creature_gesture::k_ReferenceGesture))
	        : std::nullopt;
	drawing->gesture = gesture;
	drawing->inDance = inDance;
	drawing->outcome = Outcome::Running;
	drawing->shape = symbol.has_value() ? symbol->points : std::vector<glm::vec3> {};
	drawing->sharePerTurn = symbol.has_value() && reference.has_value() && reference->length > 0.0f
	                            ? creature_gesture::SharePerTurn(symbol->length, reference->length)
	                            : 1.0f / creature_gesture::k_ReferenceTurns;
	drawing->stage = Stage::Start;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} starts to draw gesture {}{}", entt::to_integral(creature), gesture,
	                    inDance ? " in its dance" : "");
	StepGesture(creature);
	return true;
}

void CreatureMindSystem::StepGesture(entt::entity creature)
{
	auto& registry = Entities();
	auto* drawing = registry.Valid(creature) ? registry.TryGet<CreatureGestureDrawing>(creature) : nullptr;
	if (drawing == nullptr || drawing->stage == Stage::None || !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& objects = Locator::creatureObjectActionSystem::value();
	// The counter of the drawing counts each turn once
	const auto now = GameTurn();
	const auto elapsed = now - drawing->countedTurn;
	drawing->countedTurn = now;
	// The body is busy in each state but the stand: a move, an own action or an action on a thing
	const bool bodyBusy = creature_body_busy::PerformsBodyAction(registry, creature);
	switch (drawing->stage)
	{
	case Stage::Start:
	{
		drawing->drawing.turns += elapsed;
		// It starts once its body does nothing else
		if (bodyBusy)
		{
			return;
		}
		drawing->drawing.drawn = 0.0f;
		const auto start = TargetOf(registry, creature, *drawing);
		objects.PointUntilTold(creature, start);
		StartChain(registry, creature, *drawing, start);
		drawing->stage = Stage::Draw;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} points at the start of gesture {} on turn {}",
		                    entt::to_integral(creature), drawing->gesture, now);
		break;
	}
	case Stage::Draw:
	{
		// The step adds the turn itself
		drawing->drawing.turns += elapsed > 0 ? elapsed - 1 : 0;
		const auto hand = objects.GetPointingHand(creature);
		const auto outcome =
		    creature_gesture::Advance(drawing->drawing, drawing->gesture, hand.has_value(), drawing->sharePerTurn);
		if (outcome == creature_gesture::Outcome::Waiting)
		{
			return;
		}
		if (drawing->drawing.drawn > 0.0f && drawing->drawing.drawn <= drawing->sharePerTurn)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} draws the first share of gesture {} on turn {}",
			                    entt::to_integral(creature), drawing->gesture, now);
		}
		if (outcome == creature_gesture::Outcome::Failed)
		{
			drawing->stage = Stage::None;
			drawing->outcome = Outcome::Failed;
			return;
		}
		objects.MovePoint(creature, TargetOf(registry, creature, *drawing));
		if (outcome == creature_gesture::Outcome::Done)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} ends gesture {} on turn {}", entt::to_integral(creature),
			                    drawing->gesture, now);
			objects.EndPoint(creature);
			drawing->stage = Stage::End;
			return;
		}
		FeedChain(*drawing, registry.Get<const Creature>(creature), hand, registry.Get<const Transform>(creature).position);
		break;
	}
	case Stage::End:
	{
		drawing->drawing.turns += elapsed;
		StopChain(*drawing);
		if (drawing->inDance)
		{
			// In a dance, the move of the creature's group becomes a dance at once
			if (const auto* dancer = registry.TryGet<const Dancer>(creature);
			    dancer != nullptr && registry.Valid(dancer->dance) && registry.AllOf<Dance>(dancer->dance))
			{
				auto& groups = registry.Get<Dance>(dancer->dance).groups.all;
				if (dancer->group < groups.size())
				{
					groups[dancer->group].move.action = creature_dance::k_DanceMove;
				}
			}
			drawing->stage = Stage::None;
			drawing->outcome = Outcome::Done;
			return;
		}
		// As a step, it waits for the arm to come down
		if (!bodyBusy)
		{
			drawing->stage = Stage::None;
			drawing->outcome = Outcome::Done;
		}
		break;
	}
	case Stage::None:
		break;
	}
}

void CreatureMindSystem::EndDanceGesturePoint(entt::entity creature)
{
	const auto* drawing = Entities().TryGet<const CreatureGestureDrawing>(creature);
	if (drawing != nullptr && drawing->inDance && Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().EndPoint(creature);
	}
}
