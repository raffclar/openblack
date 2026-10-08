/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Utility.h"

#include <cmath>

#include <array>
#include <string>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "PSysManager.h"
#include "ParticleTypes.h"
#include "Particles/PSysManagerState.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
namespace gestures = magic::gestures;

using utility::UtilityEffects;

/// The particle system's state, where the utility effects and the pending gestures live
manager::State& ManagerState()
{
	return Locator::particleSystem::value().GetState();
}

/// The effect of `type` made once, at the origin with scale 1. The trail and the recognised sparkles then get the
/// local player (openblack: PLAYER_ONE, inferred), so SF_GestureChain's ParticleChainCreator0 (UsePlayerColor 1) is
/// drawn in its colour; the selection gets no player
uint32_t CreateOnce(uint32_t& slot, ParticleType type, bool localPlayer, bool perFrame = true,
                    game_random::psys::NetGameType net = game_random::psys::NetGameType::Local)
{
	if (slot != 0 && manager::Find(slot) == nullptr)
	{
		slot = 0;
	}
	if (slot == 0)
	{
		const auto file = ParticleTypeFile(type);
		if (!file.empty())
		{
			slot = manager::StartForSpell(std::string(file), glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, nullptr, net);
			if (perFrame)
			{
				manager::SetPerFrame(slot);
			}
			if (auto* effect = manager::Find(slot); effect != nullptr && localPlayer)
			{
				effect->SetPlayer(static_cast<int>(PlayerNames::PLAYER_ONE));
			}
		}
	}
	return slot;
}

/// The trail's magnitude table: camera distance {0, 50, 500, 1500} -> {0.2, 1, 1, 1.5}
float TrailScale(float distance)
{
	constexpr std::array<float, 4> k_Distances {0.0f, 50.0f, 500.0f, 1500.0f};
	constexpr std::array<float, 4> k_Values {0.2f, 1.0f, 1.0f, 1.5f};
	if (distance <= k_Distances[0])
	{
		return k_Values[0];
	}
	if (distance >= k_Distances[3])
	{
		return k_Values[3];
	}
	for (size_t i = 0; i + 1 < k_Distances.size(); ++i)
	{
		if (distance < k_Distances[i + 1])
		{
			const float t = (distance - k_Distances[i]) / (k_Distances[i + 1] - k_Distances[i]);
			return k_Values[i] + (k_Values[i + 1] - k_Values[i]) * t;
		}
	}
	return k_Values[3];
}

/// The gesture trail's condition: the game expects a gesture
bool TrailWanted()
{
	const auto& hand = gestures::GetHandStatus();
	const auto& state = gestures::State();
	// a button bit with no known setter and an unknown flag are not ported
	if (gestures::HoldingChargingSeed())
	{
		return true;
	}
	// a seed in the hand with the held gesture trail shown
	if (hand.heldSeed != entt::null && state.showHeldGestureTrail)
	{
		return true;
	}
	// the selection open with the hand ready
	if (state.selection.open && hand.handReady)
	{
		return true;
	}
	// a circle-sized seed in the hand
	if (hand.heldSeed != entt::null && Locator::entitiesRegistry::value().Valid(hand.heldSeed))
	{
		const auto* seed = Locator::entitiesRegistry::value().TryGet<const ecs::components::SpellSeed>(hand.heldSeed);
		if (seed != nullptr && magic::seed::InfoOf(*seed).sizingGesture == GestureType::Circle)
		{
			return true;
		}
	}
	return false;
}

void Step(uint32_t id, const glm::vec3& handPosition, bool enabled, float magnitude, float seconds)
{
	auto* effect = manager::Find(id);
	if (effect == nullptr)
	{
		return;
	}
	ProcessInfo info {
	    .handPos = handPosition,
	    .power = 1.0f,
	    .enabled = enabled,
	};
	effect->SetMagnitude(magnitude);
	manager::ProcessForSpell(id, info, seconds); // with the frame's game time
}
/// The slot made when missing, processed with the turn's time and every field 0 but power 1.0 and enabled; when the
/// effect ends it is deleted and the slot cleared, made again on the next turn
void StepTurn(uint32_t& slot, ParticleType type, bool localPlayer, game_random::psys::NetGameType net)
{
	if (CreateOnce(slot, type, localPlayer, false, net) == 0)
	{
		return;
	}
	ProcessInfo info {
	    .power = 1.0f,
	    .enabled = true,
	};
	if (!manager::ProcessForSpell(slot, info, static_cast<float>(game_clock::MsPerTurn()) * 0.001f))
	{
		slot = 0;
	}
}
} // namespace

void utility::ProcessTurn()
{
	// the original's gates for these slots are always on; the slots' net game type: synced for SF_OnFire and
	// SF_LightningStrike, local for the others
	using game_random::psys::NetGameType;
	StepTurn(ManagerState().utility.onFire, ParticleType::OnFire, false, NetGameType::Synced);
	StepTurn(ManagerState().utility.manaPath, ParticleType::ManaPath, false, NetGameType::Local);
	// the belief sprite and the recognised sparkles get the local player (PLAYER_ONE)
	StepTurn(ManagerState().utility.belief, ParticleType::BeliefSprite, true, NetGameType::Local);
	StepTurn(ManagerState().utility.recognised, ParticleType::Gesture, true, NetGameType::Local);
	StepTurn(ManagerState().utility.lightningStrike, ParticleType::LightningStrike, false, NetGameType::Synced);
}

void utility::GestureRecognised(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result)
{
	// at least 2 samples and a gesture
	if (system.Count() < 2 || result.gesture == gestures::k_None)
	{
		return;
	}
	// the pixel box of the matched samples
	int first = 0;
	int last = 0;
	system.KeypointIndices(result.start, result.end, first, last);
	auto box = system.Box(first, last);
	// the land points of the whole buffer, those not at (0, 0, 0)
	std::vector<glm::vec3> stroke;
	for (int i = 0; i < system.Count(); ++i)
	{
		const auto& world = system.At(i).world;
		// fabs(x), fabs(y), fabs(z) against the float 1e-4f widened to double (so the float compare is exact)
		if (std::abs(world.x) > 1e-4f || std::abs(world.y) > 1e-4f || std::abs(world.z) > 1e-4f)
		{
			stroke.push_back(world);
		}
	}
	if (stroke.size() < 2)
	{
		return;
	}
	// the gesture's shape: the box keeps its centre, its half sizes divided by the shape's
	const auto& shape = gestures::ShapeOf(result.gesture);
	{
		const float cx = (box.minX + box.maxX) * 0.5f;
		const float hx = (box.maxX - box.minX) * 0.5f / (shape.maxX - shape.minX);
		const float cz = (box.maxZ + box.minZ) * 0.5f;
		const float hz = (box.maxZ - box.minZ) * 0.5f / (shape.maxZ - shape.minZ);
		box = {cx - hx, cz - hz, cx + hx, cz + hz};
	}
	// the camera's forward on the ground, (1, 0, 0) if it looks straight down; r is its right
	const auto projection = gestures::sampling::CurrentProjection();
	glm::vec3 forward = Locator::camera::has_value() ? Locator::camera::value().GetForward() : glm::vec3(1.0f, 0.0f, 0.0f);
	forward.y = 0.0f;
	if (forward.x * forward.x + forward.z * forward.z < 1e-4f)
	{
		forward = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	forward = glm::normalize(forward);
	const glm::vec2 right(forward.z, -forward.x);
	// a shape point in the box's pixels, then the land under it (its altitude), or 400 m along the ray
	auto onLand = [&](const glm::vec3& point) {
		const glm::vec2 pixel(static_cast<float>(static_cast<int>((box.maxX - box.minX + 1.0f) * point.x + box.minX)),
		                      static_cast<float>(static_cast<int>((box.maxZ - box.minZ + 1.0f) * point.z + box.minZ)));
		if (auto land = gestures::sampling::ScreenToLand(pixel); land)
		{
			if (Locator::terrainSystem::has_value())
			{
				land->y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(land->x, land->z));
			}
			return *land;
		}
		const glm::vec3 direction = projection.rayDirection ? projection.rayDirection(pixel) : glm::vec3(0.0f);
		return projection.cameraPosition + direction * 400.0f;
	};
	// the shape is squashed vertically on the screen (x 0.75 each time) until its depth on the land is no more than
	// twice its width, at most 15 times
	std::vector<glm::vec3> ideal;
	float squash = 1.0f;
	for (int iteration = 1;; ++iteration)
	{
		ideal.clear();
		for (const auto& p : shape.points)
		{
			ideal.push_back(onLand(glm::vec3(p.x, p.y, (p.z - 0.5f) * squash + 0.5f)));
		}
		float minA = 1e7f;
		float maxA = -1e7f;
		float minB = 1e7f;
		float maxB = -1e7f;
		for (const auto& p : ideal)
		{
			const float a = right.x * p.x + right.y * p.z;
			const float b = glm::dot(forward, p);
			minA = std::min(minA, a);
			maxA = std::max(maxA, a);
			minB = std::min(minB, b);
			maxB = std::max(maxB, b);
		}
		const float width = std::abs(maxA - minA);
		const float ratio = width > 0.0f ? std::abs(maxB - minB) / width : 1.0f;
		squash *= 0.75f;
		if (iteration >= 15 || !(ratio > 2.0f))
		{
			break;
		}
	}
	if (ideal.empty())
	{
		return;
	}
	// the record: the stroke, the ideal resampled to as many points, this interface's status
	gestures::RecognisedGesture record;
	for (const auto& p : stroke)
	{
		record.stroke.Add(p);
	}
	const size_t n = stroke.size();
	const size_t m = ideal.size();
	for (size_t k = 0; k < n; ++k)
	{
		const float u = static_cast<float>(k) * (1.0f / static_cast<float>(n - 1));
		if (u == 1.0f || m < 2)
		{
			record.ideal.Add(ideal.back());
			continue;
		}
		const float f = u * static_cast<float>(m - 1);
		const auto index = std::min(static_cast<size_t>(f), m - 2);
		record.ideal.Add(ideal[index] + (ideal[index + 1] - ideal[index]) * (f - static_cast<float>(index)));
	}
	// the interface's status and its hand position
	record.fromInterface = true;
	if (Locator::handSystem::has_value())
	{
		record.handPosition = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	ManagerState().pendingGestures.push_back(std::move(record));
	// SF_Gesture (35) is stepped once a turn (ProcessTurn) and drawn every frame with the turn's fraction; its rule
	// takes the record
}

std::vector<magic::gestures::RecognisedGesture>& utility::PendingRecognised()
{
	return ManagerState().pendingGestures;
}

void utility::Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance)
{
	if (seconds <= 0.0f)
	{
		return; // only while game time runs
	}
	// the trail (48): on while a gesture is expected (immersion 8 GESTURE_TRAIL: force feedback, not ported)
	if (const auto trail = CreateOnce(ManagerState().utility.trail, ParticleType::GestureLocal, true); trail != 0)
	{
		ManagerState().utility.trailActive = TrailWanted();
		Step(trail, handPosition, ManagerState().utility.trailActive, handScale * TrailScale(cameraDistance), seconds);
	}
	// the selection (28): while the selection (or the leash selection) is open (immersion 9). Not ported: the leash
	// selection case, only the normal selection is tested
	if (const auto selection = CreateOnce(ManagerState().utility.selection, ParticleType::SpellSelection, false);
	    selection != 0)
	{
		const auto& state = gestures::State();
		ManagerState().utility.selectionActive = state.selection.open && gestures::GetHandStatus().handReady;
		Step(selection, handPosition, ManagerState().utility.selectionActive, handScale, seconds);
	}
	// the recognised sparkles (35) are stepped once a turn (ProcessTurn) and drawn here.
	// (pending) the original draws them only while the interface has a hand
}

void utility::Reset()
{
	ManagerState().utility = UtilityEffects {};
	ManagerState().pendingGestures.clear();
}
