/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileDrawing.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreaturePose.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/TeleportEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// What the draw trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct MobileDrawingDebugHooksState
{
	int traced {0}; // OPENBLACK_DRAW_TRACE: lines written so far (+1000 while a frame has written its line)
};

MobileDrawingDebugHooksState& MobileDrawingDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::mobile_drawing: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<MobileDrawingDebugHooksState>();
}

float Ground(const LandIslandInterface& island, float x, float z)
{
	return island.GetHeightAt(glm::vec2(x, z));
}

/// What Interpolates reads for every mobile of a pass: the registry, and the clips and the state table looked up the
/// first time one is needed (a pass with no animated mobile needs neither)
class InterpolationLookups
{
public:
	explicit InterpolationLookups(const Registry& registry)
	    : registry(registry)
	{
	}
	const Registry& registry;

	[[nodiscard]] const resources::AnimationManager& Animations()
	{
		if (_animations == nullptr)
		{
			_animations = &Locator::resources::value().GetAnimations();
		}
		return *_animations;
	}
	[[nodiscard]] const auto& StateTable()
	{
		if (_table == nullptr)
		{
			_table = &Locator::infoConstants::value().villagerStateTable;
		}
		return *_table;
	}

private:
	const resources::AnimationManager* _animations {nullptr};
	const decltype(InfoConstants::villagerStateTable)* _table {nullptr};
};

/// the villager moves for its animation (its state's info flag) and plays a distance-synced clip, or the animal moved
bool Interpolates(InterpolationLookups& lookups, entt::entity entity, bool villager, const Transform& transform,
                  const DrawPosition& draw)
{
	const auto& registry = lookups.registry;
	const auto* animation = registry.TryGet<const SkeletalAnimation>(entity);
	if (animation == nullptr || !animation->hasClip)
	{
		return false;
	}
	// one lookup of the clip (an empty handle when the cache has none)
	const auto clip = lookups.Animations().Handle(animation->clip);
	if (!clip || clip->GetCycleDistance() < 0.05f)
	{
		return false;
	}
	if (villager)
	{
		const auto* action = registry.TryGet<const LivingAction>(entity);
		const auto state = action != nullptr ? action->states[static_cast<size_t>(LivingAction::Index::Top)] : 0;
		const auto& table = lookups.StateTable();
		return state < table.size() && table[state].field0x14 != 0;
	}
	// moving: the position differs from the turn's start in x or z
	return draw.turnStart.x != transform.position.x || draw.turnStart.z != transform.position.z;
}
} // namespace

void BeginLivingTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Transform>(entity))
	{
		return;
	}
	const auto position = registry.Get<const Transform>(entity).position;
	auto* draw = registry.TryGet<DrawPosition>(entity);
	if (draw == nullptr)
	{
		draw = &registry.Assign<DrawPosition>(entity);
	}
	draw->turnStart = position;
	draw->started = true;
}

void SnapTurnStart(Registry& registry, entt::entity entity)
{
	// looked up through the const registry, which creates no storage for a component nobody has (the state hash
	// counts the storages)
	const Registry& lookup = registry;
	const auto* transform = lookup.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	if (lookup.TryGet<const DrawPosition>(entity) != nullptr)
	{
		registry.Get<DrawPosition>(entity).turnStart = transform->position;
	}
	if (lookup.TryGet<const Shark>(entity) != nullptr)
	{
		registry.Get<Shark>(entity).turnStart = transform->position;
	}
	// a creature is drawn between where it was at the start of the turn and where it is at its end: both become here
	if (lookup.TryGet<const CreatureLocomotion>(entity) != nullptr)
	{
		auto& locomotion = registry.Get<CreatureLocomotion>(entity);
		locomotion.fromPosition = transform->position;
		locomotion.toPosition = transform->position;
	}
	if (lookup.TryGet<const CreatureDrawPose>(entity) != nullptr)
	{
		auto& pose = registry.Get<CreatureDrawPose>(entity);
		pose.position = transform->position;
		pose.rotation = transform->rotation;
	}
}

void NotifyTeleported(entt::entity entity)
{
	SnapTurnStart(Locator::entitiesRegistry::value(), entity);
	events::Publish(events::Teleported {.thing = entity});
}

float StepYawFollow(float& followYaw, float target, float milliseconds, float radiansPerSecond, bool snap)
{
	const float aim = affine::WrapAngle(target);
	const float current = affine::WrapAngle(followYaw);
	followYaw = current;
	if (current == aim || snap)
	{
		followYaw = aim;
		return 0.0f;
	}
	const float difference = affine::WrapAngle(aim - current);
	const float step = (milliseconds * 0.001f) * radiansPerSecond;
	if (std::abs(difference) <= step)
	{
		followYaw = aim;
		return 0.0f;
	}
	followYaw = difference > 0.0f ? step + current : current - step;
	return followYaw - aim;
}

void UpdateMobileDrawing(float turnFraction, float milliseconds)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	const float t = std::clamp(turnFraction, 0.0f, 0.99f);
	bool any = false;
	InterpolationLookups lookups(registry);
	registry.Each<DrawPosition, const Transform>(
	    [&](entt::entity entity, DrawPosition& draw, const Transform& transform) {
		    any = true;
		    const bool villager = registry.AllOf<Villager>(entity);
		    // position: lerps the start and end of the last turn, each at its land height (+ its altitude)
		    if (draw.started && Interpolates(lookups, entity, villager, transform, draw))
		    {
			    const float y0 = draw.turnStart.y;
			    const float y1 = transform.position.y;
			    draw.position = glm::vec3(glm::mix(draw.turnStart.x, transform.position.x, t), glm::mix(y0, y1, t),
			                              glm::mix(draw.turnStart.z, transform.position.z, t));
		    }
		    else
		    {
			    draw.position = transform.position;
		    }
		    // yaw: a villager's drawn yaw turns towards the real yaw, 0.003 rad/ms, past 90 degrees 0.012 * |d| / pi rad/ms;
		    // animals turn once per turn (their SetTowardsAngle limits it instead)
		    draw.rotation = transform.rotation;
		    if (const auto* wallHug = registry.TryGet<const WallHug>(entity); wallHug != nullptr && villager)
		    {
			    const float yaw = wallHug->yAngle;
			    if (!draw.hasYaw)
			    {
				    draw.yaw = yaw;
				    draw.hasYaw = true;
			    }
			    // once into [-pi, pi]
			    const float d = affine::WrapAngle(affine::WrapAngle(yaw) - draw.yaw);
			    float rate = 0.003f;
			    if (std::abs(d) > glm::half_pi<float>())
			    {
				    rate = std::abs(d) * (2.0f / glm::pi<float>()) * 2.0f * 0.003f;
			    }
			    const float step = milliseconds * rate;
			    if (std::abs(d) < step)
			    {
				    draw.yaw = yaw;
			    }
			    else
			    {
				    draw.yaw += d > 0.0f ? step : -step;
			    }
			    draw.yaw = affine::WrapAngle(draw.yaw);
			    // the same rotation the pathfinding gives the transform (InitializeStep: AngleY(angle + 90 degrees))
			    draw.rotation = affine::AngleY(draw.yaw + glm::half_pi<float>());
			    // a SuperVillager (ECS/SuperVillager.h): its own turn over this one, in the object's yaw (this yaw + 90
			    // degrees, the angle of AngleY above), so that Wrap folds the same values. Only followDrawnTurn takes it: the
			    // original turns a local copy of the sheared matrix, so draw.rotation stays the object's for the shear below
			    // and every other reader (ecs::DrawnBodyModel)
			    if (draw.followRate > 0.0f)
			    {
				    const float objectYaw = draw.yaw + glm::half_pi<float>();
				    if (!draw.hasFollowYaw)
				    {
					    // (openblack) made before its first draw (no drawn yaw yet): ECS/SuperVillager's Create sets it
					    // otherwise (to the object's yaw)
					    draw.followYaw = objectYaw;
					    draw.hasFollowYaw = true;
				    }
				    draw.followDrawnTurn = 0.0f;
				    // only while its region is on screen
				    if (!draw.followFrozen)
				    {
					    const float turn =
					        StepYawFollow(draw.followYaw, objectYaw, milliseconds, draw.followRate, draw.followSnap);
					    if (draw.followTurn)
					    {
						    draw.followDrawnTurn = turn;
					    }
				    }
			    }
		    }
		    // a dove: the bank zoomer advances by the frame's game time and rolls the drawn matrix about its forward axis (rows
		    // 0 and 1 rotated by the bank)
		    if (auto* brain = registry.TryGet<AnimalBrain>(entity);
		        brain != nullptr && (brain->bank.value != 0.0f || brain->bank.IsMoving()))
		    {
			    brain->bank.Update(milliseconds * 0.001f);
			    const float c = std::cos(brain->bank.value);
			    const float s = std::sin(brain->bank.value);
			    const glm::vec3 x = draw.rotation[0];
			    const glm::vec3 y = draw.rotation[1];
			    draw.rotation[0] = c * x - s * y;
			    draw.rotation[1] = s * x + c * y;
		    }
		    // slope: shears the object on the land (altitude <= 0.2): the rise one unit along its x and z axes, each clamped to
		    // +-0.3
		    draw.shearX = 0.0f;
		    draw.shearZ = 0.0f;
		    const float ground = Ground(island, draw.position.x, draw.position.z);
		    const auto* animal = registry.TryGet<const Animal>(entity);
		    const bool bird = animal != nullptr && ecs::animal_ai::IsFlyingSpecies(animal->type);
		    if (draw.position.y - ground <= 0.2f && !bird)
		    {
			    // the object matrix's rows, with its scale
			    const glm::vec3 x = draw.rotation[0] * transform.scale.x;
			    const glm::vec3 z = draw.rotation[2] * transform.scale.z;
			    draw.shearX =
			        std::clamp(Ground(island, draw.position.x + x.x, draw.position.z + x.z) - draw.position.y, -0.3f, 0.3f);
			    draw.shearZ =
			        std::clamp(Ground(island, draw.position.x + z.x, draw.position.z + z.z) - draw.position.y, -0.3f, 0.3f);
		    }
	    },
	    entt::exclude<Unavailable>);
	if (any)
	{
		registry.SetDirty(); // the drawn matrices change every frame
	}
	// OPENBLACK_DRAW_TRACE=1: the drawn and real position of the first walking villager, every frame for a while
	auto& traced = MobileDrawingDebugHooksData().traced;
	static const debug_env::Variable k_DrawTrace("OPENBLACK_DRAW_TRACE");
	if (traced < 400 && milliseconds > 0.0f && k_DrawTrace.Get() != nullptr)
	{
		registry.Each<const DrawPosition, const Transform, const Villager>(
		    [&](entt::entity entity, const DrawPosition& draw, const Transform& transform, const Villager&) {
			    if (traced >= 400 || draw.position == transform.position ||
			        !Interpolates(lookups, entity, true, transform, draw))
			    {
				    return;
			    }
			    ++traced;
			    SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                       "Draw trace: {} t {:.2f} drawn ({:.3f}, {:.3f}) real ({:.3f}, {:.3f}) yaw {:.3f}",
			                       static_cast<uint32_t>(entity), t, draw.position.x, draw.position.z, transform.position.x,
			                       transform.position.z, draw.yaw);
			    traced += 1000; // one villager per frame
		    },
		    entt::exclude<Unavailable>);
		if (traced >= 1000)
		{
			traced -= 1000;
		}
	}
}

glm::mat4 DrawnModel(const Registry& registry, entt::entity entity, bool slopeShear)
{
	const auto& transform = registry.Get<const Transform>(entity);
	// an object in the render hand first, at the hand's pose (HandDrawPose: the hand's Tug / Holding state draws it out
	// of the map), with no slope shear
	const auto* inHand = registry.TryGet<const HandDrawPose>(entity);
	const auto* flying = inHand == nullptr ? registry.TryGet<const PhysicsDrawPose>(entity) : nullptr;
	// a creature that walks or turns is drawn between where its last turn started and ended (decided here, per row;
	// every other object has no creature pose). Standing, its pose is its Transform, which is drawn as it is
	const auto* creaturePose = registry.TryGet<const CreatureDrawPose>(entity);
	auto between = inHand == nullptr && flying == nullptr && creaturePose != nullptr
	                   ? creature_pose::BetweenTurns(registry, entity)
	                   : std::nullopt;
	if (between && between->position == transform.position && between->rotation == transform.rotation)
	{
		between.reset();
	}
	const auto* draw =
	    inHand == nullptr && flying == nullptr && !between ? registry.TryGet<const DrawPosition>(entity) : nullptr;
	const auto& rotation = inHand != nullptr   ? inHand->rotation
	                       : flying != nullptr ? flying->rotation
	                       : between           ? between->rotation
	                       : draw != nullptr   ? draw->rotation
	                                           : transform.rotation;
	const auto& position = inHand != nullptr   ? inHand->position
	                       : flying != nullptr ? flying->position
	                       : between           ? between->position
	                       : draw != nullptr   ? draw->position
	                                           : transform.position;
	// a creature in its temple's pen is drawn smaller than it is (ECS/PlayerCreature)
	const auto& scale = creaturePose != nullptr && creaturePose->scale.has_value() ? *creaturePose->scale : transform.scale;
	auto model = affine::Model(position, rotation, scale);
	if (inHand != nullptr)
	{
		model[1] *= inHand->upStretch; // the up row of LH, glm's column 1
	}
	if (draw != nullptr && slopeShear)
	{
		model[0] += draw->shearX * model[1];
		model[2] += draw->shearZ * model[1];
	}
	return model;
}

glm::vec3 DrawnPosition(const Registry& registry, entt::entity entity)
{
	return glm::vec3(DrawnModel(registry, entity, false)[3]);
}

glm::mat4 DrawnBodyModel(const Registry& registry, entt::entity entity)
{
	auto model = DrawnModel(registry, entity);
	// (inferred) not over a physics pose: the yaw stage follows the villager's drawn yaw, which that pose does not use
	const auto* draw =
	    registry.AnyOf<PhysicsDrawPose, HandDrawPose>(entity) ? nullptr : registry.TryGet<const DrawPosition>(entity);
	if (draw == nullptr || draw->followDrawnTurn == 0.0f)
	{
		return model;
	}
	// the sheared matrix copied, only the copy turned (rows 0 and 2): c stored as a float, s kept in extended precision;
	// the translation is not touched
	const double turn = static_cast<double>(draw->followDrawnTurn);
	glm::mat3 axes(model);
	affine::RotateY(axes, static_cast<float>(std::cos(turn)), std::sin(turn));
	for (int i = 0; i < 3; ++i)
	{
		model[i] = glm::vec4(axes[i], model[i][3]);
	}
	return model;
}

} // namespace openblack::ecs
