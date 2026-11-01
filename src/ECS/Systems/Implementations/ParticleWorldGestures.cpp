/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the gesture effects ask of the world: the trails of recognised gestures, the sheets of light along them, the
// hand's glow as they flash, and the chain behind the gesturing hand stepped every frame

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/HandGlow.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"
#include "ParticleRuleSystem.h"
#include "Particles/GestureTrail.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::ecs::systems;

std::shared_ptr<particles::GestureTrail> GameParticleWorld::TakeGestureTrail()
{
	if (_gestureTrails.empty())
	{
		return nullptr;
	}
	auto trail = std::move(_gestureTrails.back());
	_gestureTrails.pop_back();
	return trail;
}

void GameParticleWorld::QueueGestureTrail(std::shared_ptr<particles::GestureTrail> trail)
{
	_gestureTrails.push_back(std::move(trail));
}

void GameParticleWorld::AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet)
{
	_lightSheets.push_back(sheet);
}

void GameParticleWorld::AddOwnedSheet(const std::shared_ptr<particles::LightSheet>& sheet)
{
	const bool listed = std::ranges::any_of(_ownedSheets, [&sheet](const auto& owned) { return owned.lock() == sheet; });
	if (sheet != nullptr && !listed)
	{
		_ownedSheets.push_back(sheet);
	}
}

std::vector<std::shared_ptr<particles::LightSheet>> GameParticleWorld::LightSheets() const
{
	std::vector<std::shared_ptr<particles::LightSheet>> sheets;
	for (auto* list : {&_lightSheets, &_ownedSheets})
	{
		std::erase_if(*list, [](const auto& sheet) { return sheet.expired(); });
		for (const auto& sheet : *list)
		{
			sheets.push_back(sheet.lock());
		}
	}
	return sheets;
}

void GameParticleWorld::SetHandGlow(uint32_t rgb)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::handSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// The glow of the gesture is on the hand of this computer, which draws the gesture
	const auto hand = Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	if (!registry.Valid(hand))
	{
		return;
	}
	if (auto* glow = registry.TryGet<ecs::components::HandGlow>(hand))
	{
		glow->rgb = rgb & 0xFFFFFFu;
	}
	else if (rgb != 0)
	{
		registry.Assign<ecs::components::HandGlow>(hand, rgb & 0xFFFFFFu);
	}
}

void ParticleRuleSystem::AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail)
{
	if (trail != nullptr)
	{
		_world.QueueGestureTrail(std::move(trail));
	}
}

void ParticleRuleSystem::KeepGestureTrails()
{
	if (IsRunning(_gestureTrails))
	{
		return;
	}
	// This computer's own: it draws on its own random numbers, in its player's colour
	_gestureTrails = Start(ParticleType::Gesture, glm::vec3(0.0f), 1.0f, false);
	SetPlayer(_gestureTrails, static_cast<int>(Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
	                                                                            : PlayerNames::PLAYER_ONE));
}

void ParticleRuleSystem::UpdateFrame(float gameSeconds, const HandFrame& hand)
{
	if (_paused)
	{
		return;
	}
	for (const auto& sheet : _world.LightSheets())
	{
		// A sheet with its own clock is moved on by its owner
		if (!sheet->Hidden() && !sheet->OwnClock())
		{
			sheet->Update(gameSeconds);
		}
	}
	StepShowers(gameSeconds);

	if (!IsRunning(_gestureChain))
	{
		_gestureChain = Start(ParticleType::GestureLocal, glm::vec3(0.0f), 1.0f, false);
		SetPlayer(_gestureChain, static_cast<int>(Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
		                                                                           : PlayerNames::PLAYER_ONE));
		if (const auto it = FindRunning(_gestureChain); it != _effects.end())
		{
			it->everyFrame = true;
		}
	}
	if (gameSeconds <= 0.0f)
	{
		return;
	}
	const auto it = FindRunning(_gestureChain);
	if (it == _effects.end())
	{
		return;
	}
	// While gesturing, the chain is sized by how far the hand is from the camera as well as by the hand
	const float distanceScale =
	    hand.gesturing ? particles::gesture_trail::ChainDistanceScale(glm::distance(hand.position, hand.cameraPosition)) : 1.0f;
	it->effect->SetMagnitude(hand.size * distanceScale);
	it->effect->SetProcessInfo({.handPosition = hand.position, .enabled = hand.gesturing});
	if (StepEffect(*it->effect, gameSeconds))
	{
		Forget(FindRunning(_gestureChain));
	}
	StepSelectionEffect(gameSeconds, hand);
}

void ParticleRuleSystem::StepSelectionEffect(float gameSeconds, const HandFrame& hand)
{
	// The effect of a selection runs all the time at the hand. It shows while the miracle selection or the leash
	// picker is open, with the strength of the size of the hand.
	if (!IsRunning(_selectionEffect))
	{
		_selectionEffect = Start(ParticleType::SpellSelection, glm::vec3(0.0f), 1.0f, false);
		SetPlayer(_selectionEffect,
		          static_cast<int>(Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
		                                                            : PlayerNames::PLAYER_ONE));
		if (const auto it = FindRunning(_selectionEffect); it != _effects.end())
		{
			it->everyFrame = true;
		}
	}
	const auto it = FindRunning(_selectionEffect);
	if (it == _effects.end())
	{
		return;
	}
	it->effect->SetProcessInfo({.handPosition = hand.position, .power = hand.size, .enabled = hand.selecting});
	if (StepEffect(*it->effect, gameSeconds))
	{
		Forget(FindRunning(_selectionEffect));
	}
}

void ParticleRuleSystem::ScrollChains(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime)
{
	const auto step = water_rings::Step(_chainScrollPace, frameRealTime, frameGameTime);
	if (_paused || step.count() == 0)
	{
		return;
	}
	for (auto& running : _effects)
	{
		running.effect->ScrollChains(static_cast<float>(step.count()));
	}
}

void ParticleRuleSystem::StepShowers(float gameSeconds)
{
	constexpr float k_MillisecondsPerSecond = 1000.0f;
	const auto localFloat = [](float range) {
		return Locator::gameRandom::has_value() ? Locator::gameRandom::value().LocalFloatRand(range) : 0.0f;
	};
	const auto local = [](uint32_t range) {
		return Locator::gameRandom::has_value()
		           ? static_cast<uint32_t>(Locator::gameRandom::value().LocalRand(static_cast<int32_t>(range)))
		           : 0u;
	};
	// The sparks showers over objects: made the first frame they are drawn, their sparks moving on by each drawn frame's
	// game time, only while their object is there to rise from. The visit is made one time for all the effects.
	const std::function<void(particles::ObjectShower&)> step = [&](particles::ObjectShower& shower) {
		if (!_world.ObjectPosition(shower.object).has_value())
		{
			return;
		}
		if (!shower.sparks.has_value())
		{
			shower.sparks = particles::rising_sparks::MakeSparks(localFloat, local);
		}
		particles::rising_sparks::StepSparks(*shower.sparks, shower.clock.Take(gameSeconds * k_MillisecondsPerSecond),
		                                     localFloat);
	};
	for (auto& running : _effects)
	{
		running.effect->ForEachDrawnShower(step);
	}
}
