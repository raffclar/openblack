/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicFireBall.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicObjectsSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Particles/PSysManager.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MagicFireBall;
using ecs::components::Transform;

namespace
{
/// The fireballs, newest first (Locator::magicObjectsSystem)
std::vector<entt::entity>& FireBallList()
{
	if (!Locator::magicObjectsSystem::has_value())
	{
		std::fputs("magic::fireball: no magic objects in the locator (Locator::magicObjectsSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicObjectsSystem::value().FireBalls();
}

const GMagicFireBallInfo& InfoOf(const MagicFireBall& ball)
{
	return Locator::infoConstants::value().magicFireBall.at(static_cast<size_t>(ball.infoRow));
}

void Destroy(entt::entity fireball)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::erase(FireBallList(), fireball);
	if (auto* fire = ecs::fire::Find(fireball))
	{
		ecs::fire::ToBeDeleted(*fire);
	}
	if (registry.Valid(fireball))
	{
		registry.Destroy(fireball);
	}
}
} // namespace

entt::entity fireball::Create(const glm::vec3& position, int infoRow, uint32_t effect, uint32_t atomKey,
                              std::optional<PlayerNames> player, bool scriptCast, entt::entity source)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the object (the scale comes with the first FollowAtom), the head of the fireball list
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& ball = registry.Assign<MagicFireBall>(entity);
	ball.infoRow = std::clamp(infoRow, 0, 2); // openblack's guard: GMagicFireBallInfo has 3 rows
	ball.effect = effect;
	ball.atomKey = atomKey;
	ball.player = player;
	ball.seen = true;
	auto& balls = FireBallList();
	balls.insert(balls.begin(), entity);
	// the temperature is the effect's strength x info.initialTemperature: the new fire takes the ball's player (its
	// effect's). (inferred) The fire is made here rather than through fire::SetTemperature, whose player lookup does
	// not know the ball; unlike it the fire is also made when t is not above the object's temperature (then it keeps
	// that temperature)
	const float temperature = Strength(entity) * InfoOf(ball).initialTemperature;
	if (auto* fire = ecs::fire::Create(entity, player, source); fire != nullptr && fire->temperature < temperature)
	{
		fire->temperature = temperature;
	}
	// rained on unless a script cast it
	registry.Get<MagicFireBall>(entity).affectedByRain = !scriptCast;
	if (ecs::fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireball: ball {} (row {}) at ({:.1f}, {:.1f}, {:.1f}) T {:.0f}",
		                   static_cast<int>(entity), infoRow, position.x, position.y, position.z,
		                   ecs::fire::GetTemperature(entity));
	}
	return entity;
}

bool fireball::FollowAtom(entt::entity fireball, const glm::vec3& position, float scale, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.TryGet<Transform>(fireball);
	auto* ball = registry.TryGet<MagicFireBall>(fireball);
	if (transform == nullptr || ball == nullptr)
	{
		return false;
	}
	// the atom's MapCoords and its height above the land (the fire reads both through the transform)
	transform->position = position;
	transform->scale = glm::vec3(scale);
	static_cast<void>(turn);
	ball->seen = true;
	return ecs::fire::GetTemperature(fireball) < InfoOf(*ball).deletionTemperature;
}

float fireball::Strength(entt::entity fireball)
{
	// (inferred) 1 without the ball or its effect: in the original the effect is always there
	const auto* ball = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(fireball);
	if (ball == nullptr)
	{
		return 1.0f;
	}
	const auto* effect = psys::manager::Find(ball->effect);
	return effect != nullptr ? effect->GetProcessInfo().power : 1.0f;
}

void fireball::ToBeDeleted(entt::entity fireball)
{
	Destroy(fireball);
}

bool fireball::ValidForPlaceInHand(entt::entity fireball, PlayerNames player)
{
	const auto* ball = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(fireball);
	return ball == nullptr || ball->player != player;
}

entt::entity fireball::Catch(entt::entity fireball, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value() || !registry.Valid(fireball))
	{
		return entt::null;
	}
	auto& hand = Locator::handSystem::value();
	// the hand is ready for an object (inferred: nothing held) and ValidForPlaceInHand
	if (hand.GetHeldObject().has_value() || !ValidForPlaceInHand(fireball, player))
	{
		return entt::null;
	}
	const auto position = registry.Get<const Transform>(fireball).position;
	const auto entity = seed::Create(position, SpellSeedType::Fire, player, -1, 1.0f);
	if (entity == entt::null)
	{
		return entt::null;
	}
	hand.PlaceObjectInMagicHand(entity);
	seed::InterfaceSetInMagicHand(entity);
	auto& component = registry.Get<ecs::components::SpellSeed>(entity);
	seed::SetInactive(component, false);
	seed::AddToChantStore(component, seed::GetChantNeeded(component, component.powerUp));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireball: ball {} caught -> seed {} with {:.0f} chants",
	                   static_cast<int>(fireball), static_cast<int>(entity), component.chantStore);
	Destroy(fireball);
	return entity;
}

void fireball::DeleteAndPutIntoSpellSeed(entt::entity fireball, entt::entity seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* ball = registry.TryGet<const MagicFireBall>(fireball);
	auto* component = registry.TryGet<ecs::components::SpellSeed>(seed);
	if (ball == nullptr || component == nullptr)
	{
		return;
	}
	component->psysPower *= 1.0f + InfoOf(*ball).catchIncreaseFactor;
	Destroy(fireball);
}

const std::vector<entt::entity>& fireball::All()
{
	return FireBallList();
}

void fireball::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto balls = FireBallList();
	for (const auto fireball : balls)
	{
		const auto* ball = registry.TryGet<const MagicFireBall>(fireball);
		// its atom (or the whole effect) is gone when the rule stopped refreshing it
		if (ball == nullptr || psys::manager::Find(ball->effect) == nullptr || !ball->seen)
		{
			Destroy(fireball);
			continue;
		}
		registry.Get<MagicFireBall>(fireball).seen = false;
	}
}

void fireball::Clear()
{
	FireBallList().clear();
}
