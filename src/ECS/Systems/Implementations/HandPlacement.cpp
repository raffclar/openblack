/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>
#include <utility>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "3D/ScreenFade.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Creature/CreatureFeedback.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/HandPickReject.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerHome.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "HandGrain.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/Citadel.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// Hand_Boned_*2.l3d: 0 = palm (root), 8..19 = four fingers (3 phalanges), 10/13/16/19 = fingertips, 20/21 = thumb.
constexpr std::array<uint32_t, 1> k_PalmBones = {0};
constexpr std::array<uint32_t, 4> k_TipBones = {10, 13, 16, 19};
/// The temple entrance's mesh, Data\Citadel\OutsideMeshes\Entrance.l3d (Game.cpp loads that folder as "temple/<file>")
constexpr auto k_EntranceMesh = entt::hashed_string("temple/Entrance_l3d");

/// What the placement traces keep between calls, in the debug hooks' store (Locator::debugHooks)
struct HandPlacementDebugHooksState
{
	int gripTraceFrame {0};  // OPENBLACK_HAND_TRACE: frames of a land grip, one line every 30
	int hoverTraceFrame {0}; // OPENBLACK_HAND_TRACE: searches for the object under the hand, one line every 60
};

HandPlacementDebugHooksState& HandPlacementDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandPlacementDebugHooksState>();
}
} // namespace

glm::vec3 HandSystem::ModelPosition(size_t vertex, const std::vector<glm::mat4>& bones) const noexcept
{
	return glm::vec3(bones[_vertexBones[vertex]] * glm::vec4(_vertices[vertex], 1.0f));
}

void HandSystem::LoadGeometry() noexcept
{
	auto logger = spdlog::get("game");
	auto& fileSystem = Locator::filesystem::value();
	const auto& mesh = Locator::resources::value().GetMeshes().Handle(Hand::k_MeshId);
	l3d::L3DFile file;
	const auto path = fileSystem.GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d";
	const auto open = [&]() {
		try
		{
			// the found path, as the hand's morph loads it: the same blob, read once
			return file.Open(resources::LoadBlob(Locator::resources::value().GetBlobs(), fileSystem.FindPath(path))) ==
			       l3d::L3DResult::Success;
		}
		catch (const std::exception&)
		{
			return false;
		}
	};
	if (!open() || file.GetSubmeshHeaders().empty())
	{
		SPDLOG_LOGGER_WARN(logger, "Hand: cannot read {} for placement", path.string());
		return;
	}
	_vertices.clear();
	_vertexBones.clear();
	const auto& vertices = file.GetVertexSpan(0);
	size_t offset = 0;
	for (const auto& group : file.GetVertexGroupSpan(0))
	{
		for (uint32_t i = 0; i < group.vertexCount && offset < vertices.size(); ++i, ++offset)
		{
			_vertices.emplace_back(vertices[offset].position.x, vertices[offset].position.y, vertices[offset].position.z);
			_vertexBones.push_back(group.boneIndex);
		}
	}

	const auto& bind = mesh->GetBoneMatrices();
	const auto& parents = mesh->GetBoneParents();
	const auto root = static_cast<size_t>(
	    std::distance(parents.begin(), std::find(parents.begin(), parents.end(), std::numeric_limits<uint32_t>::max())));
	const auto centroid = [&](const auto& boneSet, const std::vector<glm::mat4>& bones) {
		glm::vec3 c(0.0f);
		int n = 0;
		for (size_t v = 0; v < _vertices.size(); ++v)
		{
			if (std::find(boneSet.begin(), boneSet.end(), _vertexBones[v]) != boneSet.end())
			{
				c += ModelPosition(v, bones);
				++n;
			}
		}
		return n > 0 ? c / static_cast<float>(n) : c;
	};

	const auto palm = centroid(k_PalmBones, bind);
	_palmCenter = palm;
	SPDLOG_LOGGER_INFO(logger, "Hand: {} vertices", _vertices.size());
	// Debug: OPENBLACK_HAND_GRIP_PROBE=1 logs where the fist of the hold poses is relative to the model origin.
	static const debug_env::Variable k_HandGripProbe("OPENBLACK_HAND_GRIP_PROBE");
	if (k_HandGripProbe.Get() != nullptr)
	{
		for (const auto& clip : _animator->ListClips())
		{
			if (clip.name != "Chold_side" && clip.name != "Chold_above" && clip.name != "Cwiggle")
			{
				continue;
			}
			const auto n = std::max<uint32_t>(1, clip.frameCount);
			for (const float f :
			     {0.0f, 0.25f * static_cast<float>(n - 1), 0.5f * static_cast<float>(n - 1), static_cast<float>(n - 1)})
			{
				const float t = n > 1 ? f * static_cast<float>(clip.durationMs) / static_cast<float>(n - 1) : 0.0f;
				const auto free = _animator->Evaluate(clip.name, t);
				auto locked = free;
				if (locked.size() == bind.size())
				{
					const auto correction = bind[root] * glm::affineInverse(locked[root]);
					for (auto& m : locked)
					{
						m = correction * m;
					}
				}
				const std::array<std::pair<const char*, const std::vector<glm::mat4>*>, 2> variants {
				    std::pair {"free", &free}, std::pair {"locked", &locked}};
				for (const auto& [label, bones] : variants)
				{
					const auto p = centroid(k_PalmBones, *bones);
					const auto tips = centroid(k_TipBones, *bones);
					const auto rootT = glm::vec3((*bones)[root][3]);
					SPDLOG_LOGGER_INFO(logger, "Grip probe {} f={:.1f}/{} {}: palm {} tips {} root {}", clip.name, f, n, label,
					                   fmt::format("({:.0f},{:.0f},{:.0f})", p.x, p.y, p.z),
					                   fmt::format("({:.0f},{:.0f},{:.0f})", tips.x, tips.y, tips.z),
					                   fmt::format("({:.0f},{:.0f},{:.0f})", rootT.x, rootT.y, rootT.z));
				}
			}
		}
	}
}

void HandSystem::Place(std::optional<glm::vec3> groundPoint, [[maybe_unused]] glm::vec3 cameraForward, bool gripping,
                       std::chrono::microseconds dt) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (!_animator || _vertices.empty())
	{
		return;
	}
	_lastDt = static_cast<float>(dt.count()) / 1e6f;
	if (_handHidden)
	{
		return; // the draw preparation returns early while hidden (HandSystem::Update): no pose this frame
	}
	// the land behind the hand, from this frame's bones (the animator ran in Update)
	UpdatePointBehindHand();
	if (_renderHandState == 7 && _creaturePose)
	{
		// (approximate) the CREATURE state places the hand where the creature hand put it, on or beside the body
		groundPoint = _creaturePose->position;
	}
	// The hand's scale from its distance to the camera (normal state: the required position; camera state: the grip
	// point), clamped to [2, the hand's reach], and the model scale k = 3.2 * handScale / 555.294 (the bind joint Y extent
	// of Hand_Boned_Base2): the hand is 3.2 units long.
	const auto scalePoint = gripping && !_held && _gripPoint ? _gripPoint : groundPoint;
	if (scalePoint && Locator::camera::has_value())
	{
		const float d = glm::clamp(glm::distance(Locator::camera::value().GetOrigin(), *scalePoint), 2.0f, _handReach);
		_handScale = d < 10.0f ? std::pow(d / 10.0f, 0.8f) : 1.0f;
		if (d > 150.0f)
		{
			_handScale *= (d / 150.0f) * (1.0f - 0.3f * (d - 150.0f) / (1800.0f - 150.0f));
		}
		transform.scale = glm::vec3(3.2f * _handScale / 555.294f);
	}

	if (gripping && !_held)
	{
		// the camera state (Grip Landscape): the hand stays at the grabbed land point
		_inNormalState = false;
		if (!_gripPoint)
		{
			if (!_interactionPoint && !groundPoint)
			{
				return;
			}
			// the change to Cgrip: the hand's position = the land point under the mouse (while gripping ResolveCursorPoint
			// gives the land itself). (not ported) with an object, a bubble or the leash in the interface the original keeps
			// the Normal position instead
			_gripPoint = groundPoint ? groundPoint : _interactionPoint;
			// Starting a land grip: one branch, chosen by the water bit of the cell (InBounds && IsLand; off the map or
			// without a block counts as water): land -> dust (the grip packet, SF_GripLandscape) and G_HandGrabLand; water ->
			// the ring, G_HandInWater and the fish scare.
			// No dust nor sound while the help system has its widescreen on for a script (videos and playback do not count):
			// openblack's only widescreen is SET_WIDESCREEN. No splash while the game is paused, nor while the hand's 3D
			// object draws a held object (empty or a SpellSeed that is not drawn in the hand): here the hand grips the land
			// only while it holds nothing.
			if (IsLand(*_gripPoint))
			{
				if (!Locator::screenFade::has_value() || !Locator::screenFade::value().Fade().IsWideScreenOn())
				{
					// the grip packet makes spot visual 2 GRIP_LANDSCAPE (SF_GripLandscape, 100 turns, Z-sorted), stepped once
					// a turn and drawn with the turn fraction; the packet is applied at the next turn's start (HandTurn.cpp)
					game_packets::Push({game_packets::Type::SpotVisual, entt::null, *_gripPoint, 2});
					GripLandSound(*_gripPoint);
				}
			}
			else if (!Locator::time::has_value() || !game_clock::IsPaused())
			{
				SplashHand(*_gripPoint);
			}
		}
		static const debug_env::Variable k_HandTrace("OPENBLACK_HAND_TRACE");
		if (k_HandTrace.Get() != nullptr && groundPoint)
		{
			auto& frame = HandPlacementDebugHooksData().gripTraceFrame;
			if (++frame % 30 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Grip trace: grab ({:.1f},{:.1f},{:.1f}) under cursor ({:.1f},{:.1f},{:.1f}) gap={:.2f}",
				                   _gripPoint->x, _gripPoint->y, _gripPoint->z, groundPoint->x, groundPoint->y, groundPoint->z,
				                   glm::distance(*_gripPoint, *groundPoint));
			}
		}
		// it stays at the grabbed point (no offset); the hand matrix with the last Normal up (frozen) and the heading from
		// the camera -> mouse ray every frame
		transform.rotation = HandMatrixRotation(_normalUp);
		transform.position = *_gripPoint;
		_smoothedPosition = transform.position;
		_interactionPoint = _gripPoint;
		registry.SetDirty();
		return;
	}
	_gripPoint.reset();
	if (!groundPoint)
	{
		return;
	}

	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// the normal state: the model origin is the required position itself, and the matrix is the hand matrix with the
	// smoothed up; no lift over the land
	// the hand's state follows its own held object, from the press
	const auto renderHeld = RenderHeld();
	const bool normalState = !(renderHeld || _tug) || _holdType == HoldType::None;
	if (normalState && !_inNormalState)
	{
		// entering the normal state: the up Zoomers at (0, 1, 0)
		_up.SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
		_upMouseX = -1e30f;
	}
	_inNormalState = normalState;
	UpdateReleaseImpulse(); // the hand's required state, before the state's update
	auto rotation = HandMatrixRotation(UpdateNormalUp(seconds));
	auto position = *groundPoint;
	// the play-anim state: the hand pinned at the animation's point with the world up; the mouse does not move it
	if (_renderHandState == 9 && _playAnim)
	{
		rotation = HandMatrixRotation(glm::vec3(0.0f, 1.0f, 0.0f));
		position = _playAnimPoint;
	}
	// Hand state 8 (the held object is a spell seed) is the grain state (HandGrain.cpp)
	const bool seedHeld = renderHeld && ecs::IsAvailable(*renderHeld) && registry.AllOf<SpellSeed>(*renderHeld);
	hand_grain::SetHoldingSeed(seedHeld);
	_heldUp.reset();
	if ((renderHeld || _tug) && _holdType != HoldType::None)
	{
		// the holding state: the hand model origin IS the grip point; the hold animation (root bone animated) wraps
		// palm and fingers around it. Hand matrix: model X = side, Y = side x up, Z = -up, with d = back along the
		// camera-to-cursor ray (Heading) and side = d x up'.
		// Height above the ground by hold type: ABOVE 0.2, TREE/SIDE/VILLAGER max(lowering, 1.9), plus 0.1 * height for
		// rooted objects.
		const float lowering = _loweringMultiplier * _heldHeight; // GetHeight() * GetHoldLoweringMultiplier()
		// MAGIC (a spell seed before it is ready): the hand size (3.2) x the hand's scale
		const float base = _holdType == HoldType::Above   ? 0.2f
		                   : _holdType == HoldType::Magic ? 3.2f * _handScale
		                                                  : std::max(lowering, 1.9f);
		const float height = base + (_rooted ? 0.1f * _heldHeight : 0.0f);
		// the tug's grip follows the drawn lean (the tug state's object matrix)
		const auto* tugPose = _tug ? registry.TryGet<const HandDrawPose>(*_tug) : nullptr;
		auto grip = _tug ? registry.Get<Transform>(*_tug).position +
		                       (tugPose != nullptr ? tugPose->rotation : registry.Get<Transform>(*_tug).rotation) *
		                           glm::vec3(0.0f, lowering, 0.0f)
		                 : *groundPoint + glm::vec3(0.0f, height, 0.0f);
		if (_pickSource && ecs::IsAvailable(*_pickSource))
		{
			// holding while a pile, field or fish farm is locked in its interaction: the hand stays where it is (x, z of
			// its own position), at the ground there + the object's GetHeightForHandAboveInteractObject, then the hold
			// height on top
			const glm::vec2 at(transform.position.x, transform.position.z);
			grip = glm::vec3(at.x,
			                 Locator::terrainSystem::value().GetHeightAt(at) +
			                     ecs::object::GetHeightForHandAboveInteractObject(*_pickSource) + height,
			                 at.y);
		}
		if (!_tug)
		{
			// the required position (HOLDING and GRAIN): the cursor's ground point while a pile is locked, else the
			// required hand position before the state adjusts it.
			// (approximate) openblack's grip stands for the required hand position
			_requiredHandPosition = _pickSource && ecs::IsAvailable(*_pickSource) ? *groundPoint : grip;
			_heldGroundPoint = *groundPoint; // the cursor's ground point, from the state's update
		}
		if (seedHeld)
		{
			// holding a seed: the grain state clamps the position (with ClampHand the required position is the point the
			// raise started from), then adds the raise's height
			if (const auto clamped = hand_grain::ClampedPosition(); clamped)
			{
				grip = *clamped;
			}
			grip.y += hand_grain::Height();
		}
		// the holding state: the held object and the hand share the sway's up' (Grain's roll included); the land
		// normal plays no part while holding (docs/bw1-notes/hand-and-interface.md)
		const float tilt = seedHeld ? hand_grain::Tilt() : 0.0f;
		_heldUp = HeldSway(grip, tilt) * glm::vec3(0.0f, 1.0f, 0.0f);
		// the tug state keeps its own up: the land normal
		rotation = HandMatrixRotation(_tug ? _normalUp : *_heldUp);
		position = grip;
	}
	// the holding state: every holding frame adds the frame's game ms to the spring's clock, spring or not
	if (renderHeld && !_tug && _holdType != HoldType::None)
	{
		_springClockMs += game_clock::FrameGameMs();
	}
	// the holding state: the spring (not for a spell seed) is on from the second press (action state 12, IN
	// THROW); its velocity is also the throw's
	if (_held && _releaseArmed && _smoothedPosition && !seedHeld)
	{
		if (!_springActive)
		{
			_springActive = true;
			_springVelocity = glm::vec3(0.0f);
		}
		// 10 ms steps, at least one a frame, until they catch up with the clock; neither counter is ever reset, so the
		// first frame takes the steps missed while the spring was off
		do
		{
			_springStepMs += 10;
			const auto d = position - *_smoothedPosition;
			_springVelocity += (d * 260.0f - _springVelocity * 40.0f) * 0.01f;
			const float speed = glm::length(_springVelocity);
			if (speed > 124.0f)
			{
				_springVelocity *= 124.0f / speed;
			}
			*_smoothedPosition += _springVelocity * 0.01f;
		} while (_springStepMs < _springClockMs);
		_handVelocity = _springVelocity;
	}
	else
	{
		_springActive = false;
		_smoothedPosition = position;
	}
	const glm::vec3 oldHand = transform.position;
	transform.rotation = rotation;
	transform.position = *_smoothedPosition;
	_interactionPoint = _collidePoint ? _collidePoint : groundPoint; // the action collide point
	UpdateHeldObject();
	UpdateRenderHandHeldPose();
	BlendHandWorld(affine::Model(transform.position, transform.rotation, transform.scale));
	// While the state blend runs (StateBlendFraction): the hand's place = from + (to - from) t, and in HOLDING (state 4)
	// the held object's 3D object at that place + the hold offset: the held object is drawn from the blended place.
	// (not identified) one exception of the original is not ported. The hand itself: BlendHandWorld above
	if (const auto t = _animator->StateBlendFraction(); t && _renderHandState == 4)
	{
		if (const auto held = RenderHeld(); held && registry.Valid(*held))
		{
			const auto shift = (_stateBlendFromPosition - transform.position) * (1.0f - *t);
			if (auto* pose = registry.TryGet<HandDrawPose>(*held); pose != nullptr)
			{
				pose->position += shift;
			}
			else
			{
				// (approximate) without a HandDrawPose the logic Transform is moved every frame
				registry.Get<Transform>(*held).position += shift;
			}
		}
	}
	// the hand state's throw block after this frame's move (Grain's speed is the last write of the throw
	// velocity)
	UpdateThrowBlock(oldHand, transform.position);
	registry.SetDirty();
}

void HandSystem::BlendHandWorld(const glm::mat4& handModel) noexcept
{
	// While the blend runs, every float of the hand's world matrices = prev + (cur - prev) t, prev being the matrices
	// the blend started from. openblack draws the hand as its model (the Transform) times model-space bone matrices, so
	// the world ones are model x bone, blended, and taken back to the model's space. (approximate) glm::inverse of the
	// model; the original keeps world matrices only
	auto& bones = _animator->BoneMatricesForBlend();
	_handWorld.resize(bones.size());
	for (size_t i = 0; i < bones.size(); ++i)
	{
		_handWorld[i] = handModel * bones[i];
	}
	const auto t = _animator->StateBlendFraction();
	if (!t || _stateBlendFromWorld.size() != _handWorld.size())
	{
		return;
	}
	const auto toModel = glm::inverse(handModel);
	for (size_t i = 0; i < _handWorld.size(); ++i)
	{
		for (int c = 0; c < 4; ++c)
		{
			for (int r = 0; r < 4; ++r)
			{
				const float prev = _stateBlendFromWorld[i][c][r];
				_handWorld[i][c][r] = (_handWorld[i][c][r] - prev) * *t + prev;
			}
		}
		bones[i] = toModel * _handWorld[i];
	}
}

std::optional<HandSystem::CursorHit> HandSystem::PickObjectAlongRay(const glm::vec3& origin,
                                                                    const glm::vec3& dir) const noexcept
{
	// Every drawn object is tested with an exact triangle collide and the nearest wins; objects in the hand are
	// skipped. Villagers (the 3D object's "human" flag, set only for villagers) have no triangle test: they count when
	// the mouse is inside the screen circle of their bounding sphere, at the distance |centre - camera| - (R + near
	// clip).
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& terrain = Locator::terrainSystem::value();
	const auto ground = land_morph::Altitude(terrain);
	glm::vec3 forward = dir;
	float nearClip = 0.3f;
	if (Locator::camera::has_value())
	{
		forward = glm::normalize(Locator::camera::value().GetForward());
		// the near clip: 0.3 + 0.16 x the camera's height over the land, 0.3..3.5
		const float h = origin.y - terrain.GetHeightAt(glm::vec2(origin.x, origin.z));
		nearClip = h <= 0.0f ? 0.3f : h > 20.0f ? 3.5f : 0.3f + 0.16f * h;
	}
	std::optional<CursorHit> best;
	float bestT = std::numeric_limits<float>::max();
	const auto skip = [&](entt::entity entity) {
		// a deleted object (Unavailable, freed later in the turn) is not under the cursor
		if (entity == _hands[0] || entity == _hands[1] || (_held && entity == *_held) || (_tug && entity == *_tug) ||
		    (_renderHandHeld && entity == *_renderHandHeld) || registry.AllOf<HandFxPart>(entity) || !ecs::IsAvailable(entity))
		{
			return true;
		}
		// Anything that moves with the hand would pull the hand towards the camera frame after frame: the pick-up
		// particles and the roots hanging under a tree out of the map.
		if (std::any_of(_roots.begin(), _roots.end(), [entity](const auto& pair) { return pair.second == entity; }))
		{
			return true;
		}
		return std::any_of(_pickupParticles.begin(), _pickupParticles.end(),
		                   [entity](const PickupParticle& particle) { return particle.entity == entity; });
	};
	// the ray's line seen from above, for the cheap reject below
	const glm::vec2 originXZ(origin.x, origin.z);
	const glm::vec2 dirXZ(dir.x, dir.z);
	registry.Each<const Transform, const Mesh>([&](entt::entity entity, const Transform& transform, const Mesh& mesh) {
		// one lookup of the mesh (an empty handle when the cache has none)
		const auto l3d = meshes.Handle(mesh.id);
		if (!l3d)
		{
			return;
		}
		const auto box = l3d->GetBoundingBox();
		if (box.minima.x > box.maxima.x)
		{
			return;
		}
		const auto offset = transform.rotation * (transform.scale * box.Center());
		const float radius = 0.5f * glm::length(transform.scale * box.Size());
		// the cheapest reject first (the centre's x and z do not depend on the ground the height-map objects are glued to)
		if (hand_pick::MissesFromAbove(originXZ, dirXZ,
		                               glm::vec2(transform.position.x + offset.x, transform.position.z + offset.z), radius))
		{
			return;
		}
		// a creature is tested by its posed body below, not by its mesh at rest
		if (registry.AllOf<Creature>(entity))
		{
			return;
		}
		if (skip(entity))
		{
			return;
		}
		// Height-map objects are picked glued to the landscape (plus a pile's sink offset), not at their stored y.
		// (approximate) The melting (land_morph, vs_object_hm_instanced) draws their origin at the stored y; this is the
		// older y = H(origin) + sink, the same wherever the stored y is that.
		glm::vec3 position = transform.position;
		if (registry.AllOf<MorphWithTerrain>(entity))
		{
			const auto* sink = registry.TryGet<const PileSink>(entity);
			position.y =
			    land_morph::OnGround(ground, glm::vec2(position.x, position.z), sink != nullptr ? sink->offset.value : 0.0f);
		}
		const auto centre = position + offset;
		if (registry.AllOf<Villager>(entity))
		{
			// the mouse ray at the view depth of the centre, against the sphere there (the projected circle)
			const float depth = glm::dot(centre - origin, forward);
			const float along = glm::dot(dir, forward);
			if (depth + radius < nearClip || along <= 0.0f)
			{
				return;
			}
			const auto onPlane = origin + dir * (depth / along);
			if (glm::distance(onPlane, centre) > radius && glm::distance(origin, centre) > radius)
			{
				return;
			}
			const glm::vec3 top(position.x, position.y + 0.5f * box.Size().y * transform.scale.y, position.z);
			const float d = glm::distance(top, origin) - (radius + nearClip);
			if (d < bestT)
			{
				bestT = d;
				const auto lo = position + transform.rotation * (transform.scale * box.minima);
				const auto hi = position + transform.rotation * (transform.scale * box.maxima);
				best = CursorHit {entity, d, glm::min(lo, hi), glm::max(lo, hi)};
			}
			return;
		}
		// Cheap reject: the ray against the bounding sphere.
		const auto oc = origin - centre;
		const float b = glm::dot(oc, dir);
		const float c = glm::dot(oc, oc) - radius * radius;
		if ((c > 0.0f && b > 0.0f) || b * b - c < 0.0f || -b - radius > bestT)
		{
			return;
		}
		// Exact test in mesh space: world = position + R * (S * local). The triangle test uses the matrix the object is
		// drawn with; a one-shot orb's is turned to the camera every frame, so its pick volume is the drawn dome:
		// world = position + facingOffset + facing * local
		glm::mat3 rotation = transform.rotation;
		glm::vec3 drawnAt = position;
		if (const auto* orb = registry.TryGet<const OneOffSpellSeed>(entity); orb != nullptr)
		{
			rotation = orb->facing;
			drawnAt += orb->facingOffset;
		}
		const auto toLocal = glm::inverse(rotation);
		const auto localOrigin = (toLocal * (origin - drawnAt)) / transform.scale;
		const auto localDir = (toLocal * dir) / transform.scale;
		glm::vec3 localNormal(0.0f, 1.0f, 0.0f);
		// A temple's heart is collided as it is drawn: its status sub-mesh is a closed shell round the whole temple that
		// the built temple does not draw, and it would stand in front of the doorway like an invisible wall
		const bool drawnOnly = registry.AllOf<CitadelHeart>(entity);
		const auto t = l3d->RayIntersect(localOrigin, localDir, &localNormal, drawnOnly);
		if (t && *t < bestT)
		{
			bestT = *t;
			const auto lo = position + transform.rotation * (transform.scale * box.minima);
			const auto hi = position + transform.rotation * (transform.scale * box.maxima);
			// the normal back to the world: rotation x (n / scale), the inverse transpose of R S
			const auto worldNormal = rotation * (localNormal / transform.scale);
			best = CursorHit {entity, *t, glm::min(lo, hi), glm::max(lo, hi),
			                  glm::length(worldNormal) > 0.0f ? glm::normalize(worldNormal) : glm::vec3(0.0f, 1.0f, 0.0f)};
		}
	});
	// A temple's entrance has a mesh of its own (Entrance.l3d at its heart's point, turned as the heart, scale 1) that
	// is never drawn: once the temple is fully built the heart's draw collides it with exact triangles, as any drawn
	// object, so a click on the doorway finds the entrance. Its 3D object stands on the land as the flattening under
	// the temple left it. Its triangles are the doorway's own triangles in the temple's mesh: the heart's draw
	// collides the entrance before the heart itself, and only a nearer object takes the cursor, so on a tie the
	// entrance keeps it
	if (const auto entranceMesh = meshes.Handle(k_EntranceMesh.value()); entranceMesh)
	{
		const auto box = entranceMesh->GetBoundingBox();
		registry.Each<const CitadelEntrance, const Transform>(
		    [&](entt::entity entrance, const CitadelEntrance&, const Transform& transform) {
			    if (!worship::citadel::EntranceCollides(entrance) || skip(entrance))
			    {
				    return;
			    }
			    glm::vec3 position = transform.position;
			    position.y = land_morph::OnGround(ground, glm::vec2(position.x, position.z), 0.0f);
			    const auto toLocal = glm::inverse(transform.rotation);
			    const auto localOrigin = (toLocal * (origin - position)) / transform.scale;
			    const auto localDir = (toLocal * dir) / transform.scale;
			    glm::vec3 localNormal(0.0f, 1.0f, 0.0f);
			    const auto t = entranceMesh->RayIntersect(localOrigin, localDir, &localNormal);
			    if (!t || *t > bestT)
			    {
				    return;
			    }
			    bestT = *t;
			    const auto lo = position + transform.rotation * (transform.scale * box.minima);
			    const auto hi = position + transform.rotation * (transform.scale * box.maxima);
			    const auto worldNormal = transform.rotation * (localNormal / transform.scale);
			    best = CursorHit {entrance, *t, glm::min(lo, hi), glm::max(lo, hi),
			                      glm::length(worldNormal) > 0.0f ? glm::normalize(worldNormal) : glm::vec3(0.0f, 1.0f, 0.0f)};
		    });
	}
	// A teleport stone has no mesh; while its spell still has a seed it sends an invisible draw collision at
	// (x, altitude + y, z) with radius 3.0 every frame: the centre is projected (nothing behind the near clip), and
	// the stone is hit when the mouse is inside the screen circle of a point 3 m off it: the mouse ray passes within
	// 3 m of the centre on the plane at the centre's view depth. (approximate) the distance it competes with is the
	// ray's length to that plane, like openblack's mesh hits, not the projected w itself.
	for (const auto stone : magic::teleport::HandCollisionStones())
	{
		if (skip(stone) || !registry.AllOf<Transform>(stone))
		{
			continue;
		}
		const auto centre = magic::ToWorld(magic::teleport::MapPositionOf(stone));
		const float radius = magic::teleport::k_HandCollisionRadius;
		const float depth = glm::dot(centre - origin, forward);
		const float along = glm::dot(dir, forward);
		if (depth < nearClip || along <= 0.0f)
		{
			continue;
		}
		const float t = depth / along;
		if (glm::distance(origin + dir * t, centre) >= radius || t >= bestT)
		{
			continue;
		}
		bestT = t;
		best = CursorHit {stone, t, centre - glm::vec3(radius), centre + glm::vec3(radius)};
	}
	// A creature by its posed body; the nearest object still wins. (approximate) the original collides the drawn, posed
	// triangles; capsules round the posed bones stand for them, and its box is its height round its feet
	if (Locator::creatureHandSystem::has_value())
	{
		if (const auto hit = Locator::creatureHandSystem::value().CreatureAlong(origin, dir);
		    hit.has_value() && !skip(hit->creature) && hit->distance < bestT)
		{
			const auto& creatures = std::as_const(registry);
			const auto* creature = creatures.TryGet<const Creature>(hit->creature);
			const auto* transform = creatures.TryGet<const Transform>(hit->creature);
			if (creature != nullptr && transform != nullptr)
			{
				const float height = creature_feedback::k_HeightAtSizeOne * creature->size;
				bestT = hit->distance;
				best = CursorHit {hit->creature, hit->distance, transform->position - glm::vec3(height, 0.0f, height),
				                  transform->position + glm::vec3(height, height, height)};
			}
		}
	}
	return best;
}

std::optional<glm::vec3> HandSystem::ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
                                                        std::optional<glm::vec3> land, bool gripping,
                                                        std::chrono::microseconds dt) noexcept
{
	_cursorObject.reset();
	_collidePoint.reset(); // the interface clears the collide point every frame
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	auto mouseDir = glm::normalize(direction);
	// OPENBLACK_TEST_CAST_PATH: during the test press the hand is dragged along a line (HandSpellSeed.cpp)
	if (const auto path = TestCastPathPoint(); path)
	{
		land = *path;
		mouseDir = glm::normalize(*path - origin);
	}
	_mouseRayOrigin = origin;
	_mouseRayDirection = mouseDir;
	const std::optional<float> landDistance = land ? std::optional(glm::distance(origin, *land)) : std::nullopt;
	if (gripping)
	{
		// The camera states place the hand themselves; entering the normal state resets the zoomer afterwards.
		_handDistanceValid = false;
		return land;
	}
	auto& registry = Locator::entitiesRegistry::value();

	// While holding, the object ray aims at the land point under the mouse raised by the aim height
	// (hold height * 0.6).
	auto dir = mouseDir;
	if ((RenderHeld() || _tug) && land)
	{
		const float lowering = _loweringMultiplier * _heldHeight;
		const float base = _holdType == HoldType::Above   ? 0.2f
		                   : _holdType == HoldType::Magic ? 3.2f * _handScale
		                                                  : std::max(lowering, 1.9f);
		const float h = base + (_rooted ? 0.1f * _heldHeight : 0.0f);
		dir = glm::normalize(*land - origin + glm::vec3(0.0f, h * 0.6f, 0.0f));
	}

	// The land pick counts 2.3 further than it is; when it is still
	// nearer than the object, the object is kept only if the land point lies inside its XZ bounding-box footprint.
	auto hit = PickObjectAlongRay(origin, dir);
	if (hit && landDistance && *landDistance + 2.3f < hit->t)
	{
		const auto& l = *land;
		if (l.x < hit->boxMin.x || l.x > hit->boxMax.x || l.z < hit->boxMin.z || l.z > hit->boxMax.z)
		{
			hit.reset();
		}
	}

	// The interface collide's pending distances (the object's and the land's), for the screen object's
	// arbitration (HandSystem::Update)
	_cursorObjectDistance = hit ? std::optional(hit->t) : std::nullopt;
	_cursorLandDistance = landDistance;
	static const debug_env::Variable k_HandTrace("OPENBLACK_HAND_TRACE");
	if (_tug && k_HandTrace.Get() != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Cursor trace: hit {} t {:.2f} land {:.2f} zoomer {:.2f}",
		                   hit ? static_cast<int>(hit->entity) : -1, hit ? hit->t : -1.0f, landDistance.value_or(-1.0f),
		                   _handDistance.value);
	}
	std::optional<glm::vec3> pos = land;
	_feelUp.reset();
	// A SpellIcon under the hand (openblack's ray hits its SpellSeedGraphic): the point is its MapCoords and altitude,
	// and the depth target is |camera - point| - 3.2 x the hand size; the empty hand's clip is Ccan_pickup (handclip),
	// and the up is the land normal: _feelUp stays reset
	float depthOffset = 0.0f;
	std::optional<entt::entity> icon;
	if (hit)
	{
		if (registry.AllOf<SpellIcon>(hit->entity))
		{
			icon = hit->entity;
		}
		else if (registry.AllOf<SpellSeedGraphic>(hit->entity))
		{
			registry.Each<const SpellIcon>([&](entt::entity owner, const SpellIcon& spellIcon) {
				if (spellIcon.graphic == hit->entity && ecs::IsAvailable(owner))
				{
					icon = owner;
				}
			});
		}
	}
	if (icon)
	{
		_cursorObject = *icon;
		pos = registry.Get<const Transform>(*icon).position;
		depthOffset = 3.2f * _handScale;
	}
	else if (hit)
	{
		_cursorObject = hit->entity;
		const auto& transform = registry.Get<const Transform>(hit->entity);
		if (registry.AnyOf<Villager, Animal>(hit->entity))
		{
			// Living (not a creature): hover just in front of it, at the distance of its centre minus its 2D radius,
			// then corrected for the height difference along the ray.
			const float radius2D = ecs::object::Get2DRadius(hit->entity);
			const auto& op = transform.position;
			const float d = glm::distance(op, origin) - radius2D;
			const auto p1 = origin + mouseDir * d;
			pos = origin + mouseDir * ((p1.y - op.y) * mouseDir.y + d);
		}
		else
		{
			// Mesh intersection: the surface point; while holding, the same depth on the mouse ray, pulled towards
			// the camera by half the held object's 2D radius so that it does not sink into the surface.
			const float s = hit->t;
			auto p = origin + (RenderHeld() || _tug ? mouseDir : dir) * s;
			if (s < 1.0f)
			{
				p = origin + dir;
			}
			if (const auto held = RenderHeld(); held && ecs::IsAvailable(*held))
			{
				// the held object's Get2DRadius x 0.5. (not ported) + the creature's push
				p += glm::normalize(origin - p) * (ecs::object::Get2DRadius(*held) * 0.5f);
			}
			pos = p;
			// the up target 0.25 d - n + (0, 0.5, 0), d = norm(camera - hit), when the hand feels the object's mesh: every
			// object but trees, dead trees, totem statues, landscape vortices, spell seeds and map shields
			if (!registry.AnyOf<Tree, DeadTree, TotemStatue, SpellSeed, MapShield>(hit->entity))
			{
				const auto toCamera = origin - p;
				const auto d = glm::length(toCamera) > 0.0f ? glm::normalize(toCamera) : -dir;
				_feelUp = d * 0.25f - hit->normal + glm::vec3(0.0f, 0.5f, 0.0f);
			}
		}
	}
	if (!pos)
	{
		return std::nullopt;
	}
	// the collide point: the collided object's position, else the land under the cursor
	_collidePoint = hit ? std::optional(registry.Get<const Transform>(hit->entity).position) : land;

	// _handDistance: towards the distance of the surface, in 0.1 s when coming closer and 0.28 s when moving away
	// (2.0 s only while a creature give or a CameraModeNew3 move is pending, which openblack does not have); at least 1.
	// With a screen object under the hand and no leash (openblack has no leash) the target is the screen object's
	// depth
	const float target =
	    _screenObject ? std::max(1.0f, _screenObjectDepth) : std::max(1.0f, glm::distance(origin, *pos) - depthOffset);
	if (!_handDistanceValid)
	{
		_handDistance.SetPosition(target);
		_handDistanceValid = true;
	}
	_handDistance.SetDestinationWithSpeedAndTime(target, 0.0f, target < _handDistance.value ? 0.1f : 0.28f);
	_handDistance.Update(seconds);
	if (_handDistance.value < 1.0f)
	{
		_handDistance.SetPosition(1.0f);
	}
	// Never beyond the land under the cursor (the hand's view distance). The empty hand (normal state, the hand size
	// subtracted) stops 3.2 x handScale x Size1 short of it, not over the sea (land y < 0.1); every branch is clamped
	// to [2, the hand's reach]. (inferred) Size1 is 1 for the hand. (not ported) with no land hit the original takes
	// |cam - hand position|
	std::optional<float> viewDistance = landDistance;
	if (landDistance && !RenderHeld() && !_tug && land->y >= 0.1f)
	{
		viewDistance = *landDistance - 3.2f * _handScale;
	}
	if (viewDistance)
	{
		viewDistance = std::clamp(*viewDistance, 2.0f, _handReach);
	}
	const float distance = viewDistance ? std::min(_handDistance.value, *viewDistance) : _handDistance.value;
	return origin + mouseDir * distance;
}

std::optional<entt::entity> HandSystem::FindObjectUnderHand() const noexcept
{
	if (!_interactionPoint)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const glm::vec2 point(_interactionPoint->x, _interactionPoint->z);
	// The interaction object is the one under the cursor (SendObjectDrawCollision: exact triangle pick) when it can be
	// picked up. FindObjectNearMapCoord (+-5 units) is only a fallback for a click that hit nothing, and it
	// only takes an object nearer than the clicked point (it is not a hover reach).
	std::optional<entt::entity> best;
	auto cursorObject = _cursorObject;
	if (cursorObject && ecs::IsAvailable(*cursorObject) && registry.AllOf<SpellSeedGraphic>(*cursorObject))
	{
		// the seed inside an orb or over a spell icon is drawn by its owner (a SpellSeedGraphic is not an object):
		// (inferred) a ray that hits it points at its owner
		const auto graphic = *cursorObject;
		cursorObject.reset();
		registry.Each<const OneOffSpellSeed>([&](entt::entity owner, const OneOffSpellSeed& orb) {
			if (orb.graphic == graphic && ecs::IsAvailable(owner))
			{
				cursorObject = owner;
			}
		});
		registry.Each<const SpellIcon>([&](entt::entity owner, const SpellIcon& icon) {
			if (icon.graphic == graphic && ecs::IsAvailable(owner))
			{
				cursorObject = owner;
			}
		});
	}
	// On the action press the object under the cursor is grabbed when it is valid to place in the hand or valid to tap.
	// A one-shot orb is both; a spell icon of the player only taps.
	if (cursorObject && ecs::IsAvailable(*cursorObject) &&
	    (registry.AnyOf<Mobile, Tree, DeadTree, Pot, Field, BigForest, OneOffSpellSeed>(*cursorObject) ||
	     worship::InterfaceValidToTap(*cursorObject, PlayerNames::PLAYER_ONE)) &&
	    _hands[0] != *cursorObject && _hands[1] != *cursorObject)
	{
		best = *cursorObject;
		// Rocks::ValidForPlaceInHand: boulders with a 2D radius over 3.6 cannot be lifted, but they stay the target of the
		// action button when they can be tapped (the grab goes straight to Tap).
		if (Rocks::IsRock(*best) && !Rocks::ValidForPlaceInHand(*best) && !Rocks::ValidToTap(*best))
		{
			best.reset();
		}
		// animal_ai::ValidForPlaceInHand: the species' playerCanPickUp
		else if (registry.AllOf<Animal>(*best) && !ecs::animal_ai::ValidForPlaceInHand(*best))
		{
			best.reset();
		}
		// a villager can be placed in the hand when IsReachable: not at home, not in a hand, not 236 GO_AND_HIDE; a
		// villager is never valid to tap, so an unreachable villager is not the target at all (a villager inside its abode)
		else if (registry.AllOf<Villager>(*best) && !ecs::villager::IsReachable(*best))
		{
			best.reset();
		}
	}
	// a spell seed out of the hand (drawn over its spell: a forest seed cast from an icon) or a teleport stone (its
	// invisible draw collision, valid through its seed) when valid to place in the hand
	if (!best && cursorObject && ecs::IsAvailable(*cursorObject) && SeedToPlaceInHand(*cursorObject) != entt::null &&
	    _hands[0] != *cursorObject && _hands[1] != *cursorObject)
	{
		best = *cursorObject;
	}
	static const debug_env::Variable k_HandTrace("OPENBLACK_HAND_TRACE");
	if (k_HandTrace.Get() != nullptr)
	{
		auto& frame = HandPlacementDebugHooksData().hoverTraceFrame;
		if (++frame % 60 == 0)
		{
			float nearest = std::numeric_limits<float>::max();
			int count = 0;
			registry.Each<const Transform, const Mobile>([&](entt::entity, const Transform& t, const Mobile&) {
				++count;
				nearest = std::min(nearest, glm::distance(point, glm::vec2(t.position.x, t.position.z)));
			});
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Hand trace: point ({:.1f},{:.1f}) mobiles={} nearest={:.2f} hovered={} held={}", point.x,
			                   point.y, count, nearest, best.has_value(), _held.has_value());
		}
	}
	return best;
}
