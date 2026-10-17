/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the things that move in the experiment's water and what they write into its field

#define LOCATOR_IMPLEMENTATIONS

#include "WaterWakeSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Graphics/Underwater.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
namespace wake = openblack::graphics::underwater::wake;

namespace
{
/// How far from the field's centre a thing writes into the field: half the field's side, and a little more so that a
/// thing that comes in from its edge already has its wake
constexpr float k_Reach = wake::k_FieldSide * wake::k_FieldSpacing * 0.5f + 8.0f;
/// How far the field's centre is from the eye at most, across the ground
constexpr float k_CentreReach = 40.0f;
/// The time in seconds over which a thing's place and speed are eased
constexpr float k_Ease = 0.1f;
/// A thing that moves farther than this in one frame jumped, and its track starts again
constexpr float k_Jump = 10.0f;

/// The smallest and largest corner of a box after a model matrix, and its middle
struct WorldBox
{
	glm::vec3 low {1e9f};
	glm::vec3 high {-1e9f};
};

WorldBox BoxIn(const glm::mat4& model, glm::vec3 low, glm::vec3 high)
{
	WorldBox box;
	for (int i = 0; i < 8; ++i)
	{
		const glm::vec3 corner {(i & 1) != 0 ? high.x : low.x, (i & 2) != 0 ? high.y : low.y, (i & 4) != 0 ? high.z : low.z};
		const glm::vec3 at = model * glm::vec4(corner, 1.0f);
		box.low = glm::min(box.low, at);
		box.high = glm::max(box.high, at);
	}
	return box;
}
/// The mass of a thing that goes into the water: its body's mass in the physics, else a mass from the size of its box,
/// at half the density of water
std::vector<std::pair<entt::entity, glm::vec3>> NearThings(glm::vec3 focus, float reach)
{
	std::vector<std::pair<entt::entity, glm::vec3>> near;
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto take = [&](entt::entity entity, const openblack::ecs::components::Transform& transform) {
		if (glm::distance(glm::vec2(transform.position.x, transform.position.z), glm::vec2(focus.x, focus.z)) < reach)
		{
			near.emplace_back(entity, transform.position);
		}
	};
	registry.Each<const openblack::ecs::components::MobileObject, const openblack::ecs::components::Transform>(
	    [&](entt::entity entity, const openblack::ecs::components::MobileObject& /*mobile*/,
	        const openblack::ecs::components::Transform& transform) { take(entity, transform); });
	registry.Each<const openblack::ecs::components::MobileStatic, const openblack::ecs::components::Transform>(
	    [&](entt::entity entity, const openblack::ecs::components::MobileStatic& /*mobile*/,
	        const openblack::ecs::components::Transform& transform) { take(entity, transform); });
	registry.Each<const openblack::ecs::components::Villager, const openblack::ecs::components::Transform>(
	    [&](entt::entity entity, const openblack::ecs::components::Villager& /*villager*/,
	        const openblack::ecs::components::Transform& transform) { take(entity, transform); });
	registry.Each<const openblack::ecs::components::Animal, const openblack::ecs::components::Transform>(
	    [&](entt::entity entity, const openblack::ecs::components::Animal& /*animal*/,
	        const openblack::ecs::components::Transform& transform) { take(entity, transform); });
	return near;
}

float MassOf(entt::entity entity, glm::vec3 low, glm::vec3 high)
{
	if (openblack::Locator::dynamicsSystem::has_value())
	{
		if (const auto* entry = openblack::Locator::dynamicsSystem::value().Find(entity);
		    entry != nullptr && entry->body != nullptr)
		{
			return entry->body->Mass();
		}
	}
	constexpr float k_HalfWaterDensity = 500.0f;
	const glm::vec3 metres = (high - low) / wake::k_UnitsPerMetre;
	return metres.x * metres.y * metres.z * k_HalfWaterDensity / wake::k_KilogramsPerMass;
}
} // namespace

void WaterWakeSystem::Update(std::chrono::milliseconds frameRealTime)
{
	const float frameSeconds = std::chrono::duration<float>(frameRealTime).count();
	_movers.clear();
	_rings.clear();
	_hulls.clear();
	_height = 0.0f;
	_foam = 0.0f;
	const bool enabled = Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsEnabled() &&
	                     Locator::entitiesRegistry::has_value() && Locator::camera::has_value() &&
	                     Locator::resources::has_value();
	if (!enabled)
	{
		_tracks.clear();
		_splashes.clear();
		_plan = _clock.Plan(frameSeconds, _clock.Centre(), 0.0f, 0.0f);
		return;
	}
	const auto& underwater = Locator::underwaterSystem::value();
	const auto& camera = Locator::camera::value();
	_centre = wake::FieldCentre(camera.GetOrigin(), camera.GetFocus(), k_CentreReach);
	const float seaLevel = underwater.GetSeaLevel();
	const float seconds = underwater.GetSeconds();
	const bool off = !underwater.IsEffectOn(graphics::underwater::Effect::WakeField);

	// The things that can be in the water near the field: the bodies in the physics, the creatures, the villagers and
	// the explorers' boat
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<Candidate> candidates;
	const auto near = [this](glm::vec3 at) { return glm::distance(glm::vec2(at.x, at.z), _centre) < k_Reach; };
	const auto add = [&](entt::entity entity, const glm::mat4& model, entt::id_type mesh, bool floats, bool boat) {
		if (!meshes.Contains(mesh))
		{
			return;
		}
		const auto handle = meshes.Handle(mesh);
		const auto box = handle->GetBoundingBox();
		candidates.push_back({entity, model, box.minima, box.maxima, floats, boat, &*handle});
	};
	registry.Each<const components::PhysicsDrawPose, const components::Mesh>(
	    [&](entt::entity entity, const components::PhysicsDrawPose& pose, const components::Mesh& mesh) {
		    if (!pose.underSea && near(pose.origin))
		    {
			    const auto model = glm::translate(glm::mat4(1.0f), pose.origin) * glm::mat4(pose.axes);
			    add(entity, underwater.FloatOnWaves(model), mesh.id, true, false);
		    }
	    });
	registry.Each<const components::Creature, const components::Transform, const components::Mesh>(
	    [&](entt::entity entity, const components::Creature& /*creature*/, const components::Transform& transform,
	        const components::Mesh& mesh) {
		    if (near(transform.position))
		    {
			    add(entity, components::PlacedMatrix(transform.position, transform.rotation, transform.scale), mesh.id, false,
			        false);
		    }
	    });
	registry.Each<const components::Villager, const components::Transform, const components::Mesh>(
	    [&](entt::entity entity, const components::Villager& /*villager*/, const components::Transform& transform,
	        const components::Mesh& mesh) {
		    if (near(transform.position))
		    {
			    add(entity, components::PlacedMatrix(transform.position, transform.rotation, transform.scale), mesh.id, false,
			        false);
		    }
	    });
	registry.Each<const components::BoatFilmPart, const components::DrawnMatrix, const components::Mesh>(
	    [&](entt::entity entity, const components::BoatFilmPart& part, const components::DrawnMatrix& drawn,
	        const components::Mesh& mesh) {
		    if (part.role == components::BoatFilmPart::Role::Boat && near(drawn.model[3]))
		    {
			    add(entity, underwater.FloatOnWaves(drawn.model), mesh.id, true, true);
		    }
	    });

	for (auto& [entity, track] : _tracks)
	{
		track.seen = false;
	}
	for (const auto& candidate : candidates)
	{
		Follow(candidate, frameSeconds, seaLevel, seconds);
	}
	std::erase_if(_tracks, [](const auto& item) { return !item.second.seen; });
	std::erase_if(_splashes, [seconds](const ActiveSplash& active) {
		const float age = seconds - active.start;
		return age > active.splash.seconds || age < 0.0f;
	});

	// The nearest movers, rings and hulls first, up to the most of each
	const auto byDistance = [this](const auto& a, const auto& b) {
		return glm::distance(a.position, _centre) < glm::distance(b.position, _centre);
	};
	std::ranges::sort(_movers, byDistance);
	std::ranges::sort(_rings, byDistance);
	if (_movers.size() > wake::k_MostMovers)
	{
		_movers.resize(wake::k_MostMovers);
	}
	if (_rings.size() > wake::k_MostImpulses)
	{
		_rings.resize(wake::k_MostImpulses);
	}
	std::ranges::sort(_hulls, [this](const wake::Hull& a, const wake::Hull& b) {
		return glm::distance(a.centre, _centre) < glm::distance(b.centre, _centre);
	});
	if (_hulls.size() > wake::k_MostHulls)
	{
		_hulls.resize(wake::k_MostHulls);
	}
	if (off)
	{
		_movers.clear();
		_rings.clear();
		_height = 0.0f;
		_foam = 0.0f;
	}
	for (const auto& write : _movers)
	{
		_height = std::max(_height, write.depth);
		_foam = std::max(_foam, write.foam);
	}
	for (const auto& ring : _rings)
	{
		_height = std::max(_height, ring.depth);
	}
	_plan = _clock.Plan(frameSeconds, wake::FieldCell(_centre), _height, _foam);
	if (_plan.steps == 0)
	{
		// Rings start at the first step: with no step this frame, none start
		_rings.clear();
	}
}

void WaterWakeSystem::Follow(const Candidate& candidate, float frameSeconds, float seaLevel, float seconds)
{
	namespace underwater = graphics::underwater;
	const auto box = BoxIn(candidate.model, candidate.boxLow, candidate.boxHigh);
	const glm::vec3 middle = (box.low + box.high) * 0.5f;
	const glm::vec2 across {middle.x, middle.z};
	const glm::vec2 size {box.high.x - box.low.x, box.high.z - box.low.z};
	const float radius = 0.5f * std::min(size.x, size.y);
	// The water's height here: the sea's level and its waves, which are smaller where the water is shallow
	const auto& system = Locator::underwaterSystem::value();
	const float depth = system.GetSeabed().DepthAt(across) + seaLevel;
	const float shallow = std::clamp((depth - 0.2f) / 3.8f, 0.0f, 1.0f);
	const float water = seaLevel + underwater::WavesAt(across, seconds, system.GetWaves()).height * shallow;

	auto found = _tracks.find(candidate.entity);
	const bool fresh = found == _tracks.end() || glm::distance(found->second.position, middle) > k_Jump;
	if (fresh)
	{
		found =
		    _tracks.insert_or_assign(candidate.entity, Track {.position = middle, .bottom = box.low.y, .water = water}).first;
	}
	auto& track = found->second;
	track.seen = true;
	const float before = track.bottom;
	const float wasWater = track.water;
	track.bottom = box.low.y;
	track.water = water;
	if (fresh || frameSeconds <= 0.0f)
	{
		return;
	}
	const float ease = 1.0f - std::exp(-frameSeconds / k_Ease);
	const glm::vec3 last = track.position;
	track.position = glm::mix(track.position, middle, ease);
	track.velocity = glm::mix(track.velocity, (track.position - last) / frameSeconds, ease);

	// A thing that goes down through the surface this frame splashes by its speed, or makes rings when it is slow
	const float fall = (before - box.low.y) / frameSeconds;
	if (before > wasWater && box.low.y <= water && fall > 0.0f)
	{
		const glm::vec2 at {track.position.x, track.position.z};
		const wake::Impact impact {.downSpeed = fall,
		                           .acrossSpeed = glm::length(glm::vec2(track.velocity.x, track.velocity.z)),
		                           .radius = radius,
		                           .mass = MassOf(candidate.entity, box.low, box.high)};
		const float ringRadius = std::max(radius, 0.1f);
		if (const auto splash = wake::SplashFor(impact); splash.has_value())
		{
			if (_splashes.size() < wake::k_MostSplashes)
			{
				_splashes.push_back({.position = {at.x, water, at.y}, .start = seconds, .splash = *splash});
			}
			_rings.push_back({.position = at, .radius = ringRadius, .depth = splash->ringDepth, .foam = splash->foam});
		}
		else
		{
			_rings.push_back({.position = at, .radius = ringRadius, .depth = wake::RingDepthFor(impact)});
		}
	}

	const float submerged = std::clamp(water - box.low.y, 0.0f, box.high.y - box.low.y);
	if (submerged <= 0.0f)
	{
		return;
	}
	const glm::vec2 velocity {track.velocity.x, track.velocity.z};
	std::optional<wake::Hull> hull;
	if (candidate.floats)
	{
		// The mask is the hull's shape at its water line, which is narrower than its top
		auto low = candidate.boxLow;
		auto high = candidate.boxHigh;
		const float waterline = (glm::inverse(candidate.model) * glm::vec4(middle.x, water, middle.z, 1.0f)).y;
		const float band = 0.1f * (high.y - low.y);
		std::optional<wake::WaterlineBox> line;
		for (const auto& surface : candidate.mesh->GetSurfaces())
		{
			if (const auto part = wake::WaterlineOf(surface.positions, waterline, band); part.has_value())
			{
				line = line.has_value() ? wake::WaterlineBox {.low = glm::min(line->low, part->low),
				                                              .high = glm::max(line->high, part->high)}
				                        : *part;
			}
		}
		if (line.has_value())
		{
			low = {line->low.x, low.y, line->low.y};
			high = {line->high.x, high.y, line->high.y};
		}
		hull = wake::HullFor(candidate.model, low, high, water, candidate.boat);
		if (hull.has_value())
		{
			hull->speed = glm::dot(velocity, hull->forward);
			_hulls.push_back(*hull);
		}
	}
	const auto addMover = [this, &velocity, &track](glm::vec2 at, float moverRadius, float inWater) {
		const auto write = wake::WriteFor({.position = at,
		                                   .velocity = velocity,
		                                   .verticalSpeed = track.velocity.y,
		                                   .radius = moverRadius,
		                                   .submerged = inWater});
		if (write.depth > 0.0f || write.foam > 0.0f)
		{
			_movers.push_back({.position = at,
			                   .velocity = velocity,
			                   .radius = std::max(moverRadius, 0.3f),
			                   .depth = write.depth,
			                   .foam = write.foam});
		}
	};
	if (candidate.boat && hull.has_value())
	{
		// A boat makes its waves at its two ends, the bow and the stern, so that each end has its own V of waves
		const float endRadius = hull->half.y * 0.4f;
		for (const float end : {0.8f, -0.8f})
		{
			addMover(hull->centre + hull->forward * hull->half.x * end, endRadius, hull->draft);
		}
		return;
	}
	addMover({track.position.x, track.position.z}, radius, submerged);
}

WaterWakeSystemInterface::Frame WaterWakeSystem::GetFrame() const
{
	return {.plan = _plan,
	        .origin = wake::FieldOrigin(_clock.Centre()),
	        .movers = _movers,
	        .rings = _rings,
	        .hulls = _hulls,
	        .splashes = _splashes,
	        .flat = _clock.IsFlat()};
}

void WaterWakeSystem::DropDebugThings()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::camera::has_value() || !Locator::underwaterSystem::has_value())
	{
		return;
	}
	const auto focus = Locator::camera::value().GetFocus();
	const float seaLevel = Locator::underwaterSystem::value().GetSeaLevel();
	const auto near = NearThings(focus, 80.0f);
	// Each thing is thrown up and over to a point near the focus, so that it falls into the water there from high
	constexpr float k_Flight = 2.2f;
	constexpr float k_G = 9.8f;
	for (size_t i = 0; i < near.size(); ++i)
	{
		const float angle = static_cast<float>(i) * 2.39996f;
		const float spread = 3.0f + 2.0f * static_cast<float>(i % 4);
		const glm::vec3 target {focus.x + std::cos(angle) * spread, seaLevel, focus.z + std::sin(angle) * spread};
		const auto from = near[i].second;
		const glm::vec3 velocity {(target.x - from.x) / k_Flight,
		                          (target.y - from.y + 0.5f * k_G * k_Flight * k_Flight) / k_Flight,
		                          (target.z - from.z) / k_Flight};
		Locator::dynamicsSystem::value().InitialisePhysics(
		    near[i].first, ecs::systems::PhysicsStart {.velocity = velocity, .spin = {1.0f, 0.5f, 0.0f}});
	}
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: threw {} things into the water", near.size());
}

void WaterWakeSystem::DropDebugRow(float height)
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::camera::has_value() || !Locator::underwaterSystem::has_value())
	{
		return;
	}
	const auto focus = Locator::camera::value().GetFocus();
	const float seaLevel = Locator::underwaterSystem::value().GetSeaLevel();
	auto near = NearThings(focus, 40.0f);
	std::ranges::sort(near, {}, [](const auto& thing) { return thing.first; });
	// In a row along x, 6 units apart, with the focus in the middle
	constexpr float k_Spacing = 6.0f;
	auto& registry = Locator::entitiesRegistry::value();
	const float first = -0.5f * k_Spacing * static_cast<float>(near.empty() ? 0 : near.size() - 1);
	for (size_t i = 0; i < near.size(); ++i)
	{
		auto& transform = registry.Get<components::Transform>(near[i].first);
		transform.position = {focus.x + first + k_Spacing * static_cast<float>(i), seaLevel + height, focus.z};
		Locator::dynamicsSystem::value().InitialisePhysics(near[i].first, ecs::systems::PhysicsStart {});
	}
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: dropped {} things from {} units", near.size(), height);
}

void WaterWakeSystem::PlayDebugBoat()
{
	if (Locator::boatLaunchSystem::has_value())
	{
		Locator::boatLaunchSystem::value().Play();
	}
}
