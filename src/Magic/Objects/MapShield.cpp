/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapShield.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <numbers>
#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/MagicObjectsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellShield.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ShieldDebugHooks.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MapShield;
using ecs::components::Transform;

namespace
{
/// The shields, newest first (Locator::magicObjectsSystem)
std::vector<entt::entity>& ShieldList()
{
	if (!Locator::magicObjectsSystem::has_value())
	{
		std::fputs("magic::map_shield: no magic objects in the locator (Locator::magicObjectsSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicObjectsSystem::value().Shields();
}

/// The first shield's creation turn (OPENBLACK_TEST_SHIELD_SHOT counts from it), in the debug hooks' store
/// (Locator::debugHooks). Clear forgets that a shield was made but keeps the turn
struct MapShieldDebugHooksState
{
	bool anyCreated {false};
	unsigned int firstCreated {0};
};

MapShieldDebugHooksState& MapShieldDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("magic::map_shield: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<MapShieldDebugHooksState>();
}

constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

float TurnSeconds()
{
	return static_cast<float>(k_TurnMs) * 0.001f;
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// fmod(a, 2 pi) in doubles, then + 2 pi when negative
float WrapAngle(float angle)
{
	auto wrapped = static_cast<float>(std::fmod(static_cast<double>(angle), static_cast<double>(k_TwoPi)));
	if (wrapped < 0.0f)
	{
		wrapped += k_TwoPi;
	}
	return wrapped;
}

const GMagicShieldInfo* ShieldInfoOf(const MapShield& shield)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	return GetMagicInfoAs<GMagicShieldInfo>(Locator::infoConstants::value(), shield.magicType);
}

bool SpellAlive(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	return spell != entt::null && registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell);
}

/// Nothing when it is already that scale. The body the physics built at the old size goes;
/// the next moving body near it makes a new one (PhysicsObjects BeginTurn)
void SetScale(entt::entity entity, MapShield& shield, float scale)
{
	if (shield.objectScale == scale)
	{
		return;
	}
	shield.objectScale = scale;
	if (shield.kind == MapShield::Kind::Physical)
	{
		ecs::physics::PhysicsObjects::RemoveObject(entity);
	}
}

/// The world point of MapCoords (x, z, y above the land)
glm::vec3 WorldOf(const glm::vec3& mapPoint)
{
	return {mapPoint.x, LandAt(mapPoint.x, mapPoint.z) + mapPoint.y, mapPoint.z};
}

/// The physical shield's turn: spin, grow or fade, bob
void ProcessPhysical(entt::entity entity, MapShield& shield)
{
	const float dt = TurnSeconds();
	const float t = static_cast<float>(CurrentTurn() - shield.creationTurn) * dt;
	const auto curves = map_shield::CurvesAt(t);
	if (curves.spinning)
	{
		const float spin = shield.startSpin + (shield.endSpin - shield.startSpin) * curves.spinDown;
		shield.angle = WrapAngle(shield.angle + spin * dt);
	}
	float grow = curves.grow;
	if (shield.dying)
	{
		shield.dieTime += dt;
		if (shield.dieTime > map_shield::k_FadeTime * map_shield::k_DieTimeFactor)
		{
			map_shield::ToBeDeleted(entity);
			return;
		}
		const float k = std::clamp(shield.dieTime / map_shield::k_FadeTime, 0.0f, 1.0f);
		grow -= grow * k;
	}
	const float scale = shield.startScale + (shield.finalScale - shield.startScale) * grow;
	shield.bob = WrapAngle(shield.bob + map_shield::k_BobSpeed * dt);
	shield.previousRotation = shield.rotation;
	shield.previousTranslation = shield.translation;
	shield.previousScale = shield.scale;
	// The height above the land. The bob's size goes with the new scale (the code multiplies by it, not by the
	// grow curve)
	if (const auto* info = ShieldInfoOf(shield); info != nullptr)
	{
		shield.position.y = info->shieldHeight + info->raiseWithScale * shield.finalScale +
		                    (std::sin(shield.bob) + 1.0f) * info->bobMagnitude * scale * 0.5f;
	}
	shield.scale = scale;
	if (scale != shield.previousScale && std::abs(static_cast<double>(scale - shield.objectScale)) > map_shield::k_RescaleDelta)
	{
		SetScale(entity, shield, scale);
	}
	// the matrix: diag(scale) turned by the angle about Y (r0' = c r0 + s r2, r2' = c r2 - s r0, c rounded to a float),
	// at (x, land + height, z). On the identity that is affine::AngleY bit for bit; the scale is kept apart here, so
	// its cells are rounded twice (s x scale): the last bit
	shield.rotation = affine::AngleY(shield.angle);
	shield.translation = WorldOf(shield.position);
	if (TraceEnabled())
	{
		const auto* body = ecs::physics::PhysicsObjects::Find(entity);
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: turn {} PhysicalShield {} t {:.1f}: grow {:.3f} scale {:.4f} (object {:.4f}) angle {:.2f} "
		    "height {:.2f} dying {} {:.1f}; physics body {} (radius {:.1f}, {} vertices)",
		    CurrentTurn(), static_cast<uint32_t>(entity), t, grow, scale, shield.objectScale, shield.angle, shield.position.y,
		    shield.dying, shield.dieTime, body != nullptr, body != nullptr ? body->body.Radius() : 0.0f,
		    body != nullptr ? body->body.Vertices().size() : 0);
	}
	// the effect processed with a process info of zeros, strength 1, enabled
	if (shield.fx != 0)
	{
		psys::ProcessInfo info {
		    .interfacePos = glm::vec3(0.0f),
		    .handPos = glm::vec3(0.0f),
		    .cameraForward = glm::vec3(0.0f),
		    .direction = glm::vec3(0.0f),
		    .power = 1.0f,
		    .curl = 0.0f,
		    .enabled = true,
		};
		if (!psys::manager::ProcessForSpell(shield.fx, info, dt))
		{
			shield.fx = 0;
		}
	}
}

/// The physical shield's draw, `fraction` the part of the turn since the last one
void DrawPhysical(entt::entity entity, MapShield& shield, float fraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	// the 3D object: scale and matrix lerped from the last turn's by the turn fraction
	const auto lerp = [fraction](const auto& a, const auto& b) { return a + (b - a) * fraction; };
	transform->scale = glm::vec3(lerp(shield.previousScale, shield.scale));
	transform->rotation =
	    glm::mat3(lerp(shield.previousRotation[0], shield.rotation[0]), lerp(shield.previousRotation[1], shield.rotation[1]),
	              lerp(shield.previousRotation[2], shield.rotation[2]));
	transform->position = lerp(shield.previousTranslation, shield.translation);
	const float t = static_cast<float>(CurrentTurn() - shield.creationTurn) * TurnSeconds();
	if (SpellAlive(shield.spell))
	{
		const float strength = std::min(GetSpellStrength(shield.spell), 1.0f);
		const auto byte = static_cast<uint8_t>(static_cast<int>(strength * 255.0f) & 0xFF);
		shield.alpha = std::max(byte, map_shield::k_MinAlpha);
	}
	// drawn only past 0.5 s, with a global alpha of 1 (the blended table) and the white tint: the mesh's own
	// alpha shows through (components::Alpha 1 is that pass; 0 hides it)
	if (auto* alpha = registry.TryGet<ecs::components::Alpha>(entity); alpha != nullptr)
	{
		alpha->value = t > map_shield::k_HiddenTime ? 1.0f : 0.0f;
	}
	if (shield.fx != 0)
	{
		auto a = static_cast<float>(shield.alpha);
		if (shield.dying)
		{
			// the code's clamp of dieTime / 1.5 is inverted: 1 up to 1.5 s, then 0
			a *= shield.dieTime / map_shield::k_FadeTime <= 1.0f ? 1.0f : 0.0f;
		}
		if (auto* effect = psys::manager::Find(shield.fx); effect != nullptr)
		{
			effect->SetGlobalAlpha(static_cast<float>(static_cast<int>(a) & 0xFF));
		}
	}
}

void Unlink(entt::entity shield)
{
	std::erase(ShieldList(), shield);
}
} // namespace

map_shield::Curves map_shield::CurvesAt(float seconds)
{
	if (seconds < k_HiddenTime)
	{
		return {1.0f - seconds / k_HiddenTime, 0.0f, false};
	}
	const float u = seconds - k_HiddenTime;
	const auto curve = [](float x) { return x + x * x - x * x * x; }; // x (1 + x (1 - x))
	const float grow = u < k_GrowTime ? curve(u / k_GrowTime) : 1.0f;
	const float spinDown = u < k_SpinDownTime ? curve(u / k_SpinDownTime) : 1.0f;
	return {grow, spinDown, true};
}

entt::entity map_shield::Create(const glm::vec3& position, entt::entity spell, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!SpellAlive(spell))
	{
		return entt::null;
	}
	const auto type = registry.Get<const ecs::components::Spell>(spell).magicType;
	if (type != MagicType::Shield && type != MagicType::PhysicalShield)
	{
		return entt::null;
	}
	// a fixed object at scale 1.0, the head of the list, the spell and its shield info
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	auto& shield = registry.Assign<MapShield>(entity);
	shield.spell = spell;
	shield.magicType = type;
	shield.position = glm::vec3(position.x, 0.0f, position.z);
	registry.Assign<Transform>(entity, WorldOf(shield.position), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& shields = ShieldList();
	shields.insert(shields.begin(), entity);
	if (auto& hooks = MapShieldDebugHooksData(); !hooks.anyCreated)
	{
		hooks.anyCreated = true;
		hooks.firstCreated = CurrentTurn();
	}
	if (type == MagicType::Shield)
	{
		// scale 0.017 r. Not drawn and not processed
		shield.kind = MapShield::Kind::Magic;
		SetScale(entity, shield, k_ScalePerRadius * radius);
		registry.Get<Transform>(entity).scale = glm::vec3(shield.objectScale);
		// the head of its cell's fixed list
		ecs::map_cells::InsertMapObject(entity);
		return entity;
	}
	// the physical shield (its fields start as zeros and ones)
	shield.kind = MapShield::Kind::Physical;
	shield.creationTurn = CurrentTurn();
	SetScale(entity, shield, shield.finalScale); // still 1.0 here
	if (const auto* info = ShieldInfoOf(shield); info != nullptr)
	{
		shield.position.y = info->shieldHeight + info->raiseWithScale * shield.finalScale; // with that 1.0
	}
	// the spell's process info curl
	const float curl = registry.Get<const ecs::components::Spell>(spell).processInfo.curl;
	shield.startSpin = std::clamp(curl * 1.0f, -k_MaxStartSpin, k_MaxStartSpin);
	shield.endSpin = (shield.startSpin < 0.0f ? -1.0f : 1.0f) * k_EndSpin;
	const float finalScale = k_ScalePerRadius * radius;
	shield.startScale = finalScale * 0.01f;
	shield.finalScale = finalScale;
	SetScale(entity, shield, finalScale);
	// the mesh's material types 5 and 4 become 13. On MSH_S_SOLID_SHIELD the inner layer (sub-mesh 1, alpha textured 4)
	// becomes alpha textured additive without Z write 13 (SRCALPHA / ONE); no primitive is type 5. It changes the shared
	// mesh, for every physical shield, as the original. The map fixed base and the footpath links are not modelled
	if (Locator::resources::has_value())
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (const auto id = resources::HashIdentifier(k_Mesh); meshes.Contains(id))
		{
			meshes.Handle(id)->ReplaceMaterialType(5, 13);
			meshes.Handle(id)->ReplaceMaterialType(4, 13);
		}
	}
	registry.Assign<ecs::components::Mesh>(entity, resources::HashIdentifier(k_Mesh), static_cast<int8_t>(0),
	                                       static_cast<int8_t>(0));
	registry.Assign<ecs::components::Alpha>(entity, 0.0f);
	// a morphable 3D object: it melts into the land at creation and on every draw after the lerp, so it follows the land
	// as it grows. The magic shield is static and has no mesh here
	registry.Assign<ecs::components::MorphWithTerrain>(entity, land_morph::Melting::Live);
	// the 3D object's matrix and scale into the current and last ones, two ProcessShields to prime them
	shield.rotation = glm::mat3(1.0f);
	shield.translation = WorldOf(shield.position);
	shield.scale = shield.objectScale;
	shield.previousRotation = shield.rotation;
	shield.previousTranslation = shield.translation;
	shield.previousScale = shield.scale;
	ProcessPhysical(entity, shield);
	ProcessPhysical(entity, registry.Get<MapShield>(entity));
	// SF_PhysicalShieldFX at (x, land + height, z), then its player, the shield as its target (UR_AtomsAtEPTarget
	// follows it) and the radius as its magnitude
	auto& physical = registry.Get<MapShield>(entity);
	const auto file = psys::ParticleTypeFile(ParticleType::PhysicalShieldFx);
	if (!file.empty())
	{
		physical.fx =
		    psys::manager::StartForSpell(std::string(file), WorldOf(physical.position), glm::vec3(0.0f), 1.0f, nullptr);
		if (auto* effect = psys::manager::Find(physical.fx); effect != nullptr)
		{
			const auto& spellComponent = registry.Get<const ecs::components::Spell>(spell);
			if (spellComponent.hasPlayer)
			{
				effect->SetPlayer(static_cast<int>(spellComponent.player));
			}
			effect->AddTarget(entity);
			effect->SetMagnitude(radius);
		}
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: PhysicalShield {} at ({:.1f}, {:.1f}) radius {:.1f}: scale {:.4f} -> {:.4f}, spin "
		                   "{:.2f} -> {:.2f}, fx {}",
		                   static_cast<uint32_t>(entity), position.x, position.z, radius, physical.startScale,
		                   physical.finalScale, physical.startSpin, physical.endSpin, physical.fx);
	}
	// the head of its cell's fixed list
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}

void map_shield::ProcessShields()
{
	if (const auto& hooks = MapShieldDebugHooksData(); hooks.anyCreated)
	{
		shield_debug::OnTurn(hooks.firstCreated, CurrentTurn()); // OPENBLACK_TEST_SHIELD_SHOT (ShieldDebugHooks.cpp)
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : std::vector<entt::entity>(ShieldList()))
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		if (auto* shield = registry.TryGet<MapShield>(entity); shield != nullptr && shield->kind == MapShield::Kind::Physical)
		{
			ProcessPhysical(entity, *shield); // the magic shield does nothing
		}
	}
}

void map_shield::DrawShields()
{
	// the lerps use the fraction of the turn
	const float fraction = game_clock::TurnFraction();
	auto& registry = Locator::entitiesRegistry::value();
	bool any = false;
	for (const auto entity : ShieldList())
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		if (auto* shield = registry.TryGet<MapShield>(entity); shield != nullptr && shield->kind == MapShield::Kind::Physical)
		{
			DrawPhysical(entity, *shield, fraction);
			any = true;
		}
	}
	if (any)
	{
		registry.SetDirty(); // the instances move every frame
	}
}

int map_shield::SetDying(entt::entity shield)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.Valid(shield) ? registry.TryGet<MapShield>(shield) : nullptr;
	if (component == nullptr)
	{
		return 3;
	}
	if (component->kind == MapShield::Kind::Magic)
	{
		ToBeDeleted(shield);
		return 3;
	}
	component->dying = true;
	component->spell = entt::null;
	return 1;
}

void map_shield::ToBeDeleted(entt::entity shield)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(shield))
	{
		Unlink(shield);
		return;
	}
	if (auto* component = registry.TryGet<MapShield>(shield); component != nullptr && component->fx != 0)
	{
		psys::manager::Delete(component->fx);
		component->fx = 0;
	}
	Unlink(shield);
	ecs::physics::PhysicsObjects::RemoveObject(shield);
	ecs::map_cells::RemoveMapObject(shield);
	registry.Destroy(shield);
	registry.SetDirty();
}

bool map_shield::IsPointDefinitelyWithinShieldVolume(entt::entity shield, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(shield) ? registry.TryGet<const MapShield>(shield) : nullptr;
	if (component == nullptr)
	{
		return false;
	}
	const glm::vec3 p = WorldOf(point);
	const glm::vec3 c = WorldOf(component->position);
	if (component->kind == MapShield::Kind::Magic)
	{
		// the spell's 2D radius (its magnitude), a sphere in world points
		if (!SpellAlive(component->spell))
		{
			return false;
		}
		const float r = registry.Get<const ecs::components::Spell>(component->spell).magnitude;
		const glm::vec3 d = p - c;
		return r * r > glm::dot(d, d);
	}
	// a cone of its own 2D radius R and height H: d^2 < R^2 (x, z) and the point's MapCoords altitude (above
	// the land) below H (1 - d / R)
	const float r = ecs::object::Get2DRadius(shield);
	const float dx = p.x - c.x;
	const float dz = p.z - c.z;
	const float d2 = dx * dx + dz * dz;
	if (!(d2 < r * r))
	{
		return false;
	}
	const float h = ecs::object::GetHeight(shield);
	return point.y < h - h * (std::sqrt(d2) / r);
}

bool map_shield::IsReactionBlockedByShield(const glm::vec3& living, const glm::vec3& source)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : ShieldList())
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		const auto* component = registry.TryGet<const MapShield>(entity);
		if (component == nullptr)
		{
			continue;
		}
		// the (x, z) distance against the shield's 2D radius
		const float d = gutils::GetDistanceInMetres(living, component->position);
		if (ecs::object::Get2DRadius(entity) > d && !IsPointDefinitelyWithinShieldVolume(entity, source))
		{
			return true;
		}
	}
	return false;
}

bool map_shield::GetPlayer(entt::entity shield, PlayerNames& player)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	if (component == nullptr || !SpellAlive(component->spell))
	{
		return false; // none
	}
	const auto& spell = Locator::entitiesRegistry::value().Get<const ecs::components::Spell>(component->spell);
	player = spell.player;
	return spell.hasPlayer;
}

bool map_shield::CreatureMustAvoid(entt::entity shield, entt::entity creature, std::optional<PlayerNames> creaturePlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	// no creature, or one controlled by a script -> 0
	if (creature == entt::null || !registry.Valid(creature) || ecs::script_held::IsControlledByScript(creature))
	{
		return false;
	}
	// the creature's player against the shield's: the same player (two missing ones included) -> 0, anything else 1. A
	// shield whose spell is gone takes the interface's player, openblack's PLAYER_ONE; a live spell with no player gives
	// none
	PlayerNames shieldPlayer = PlayerNames::PLAYER_ONE;
	const auto* component = registry.TryGet<const MapShield>(shield);
	const bool spellGone = component == nullptr || !SpellAlive(component->spell);
	const bool hasShieldPlayer = spellGone || GetPlayer(shield, shieldPlayer);
	if (hasShieldPlayer != creaturePlayer.has_value())
	{
		return true;
	}
	return hasShieldPlayer && *creaturePlayer != shieldPlayer;
}

bool map_shield::InteractsWithPhysicsObjects(entt::entity shield)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	return component != nullptr && component->kind == MapShield::Kind::Physical;
}

float map_shield::CollisionScale(entt::entity shield)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	// the object scale (SetScale)
	return component != nullptr ? ecs::object::GetScale(shield) : 1.0f;
}

void map_shield::ReactToPhysicsImpact(entt::entity shield, const ecs::physics::PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const MapShield>(shield);
	if (component == nullptr || po.hitBy == nullptr)
	{
		return;
	}
	// the hitter's object: available, it destroys abodes, and a spell with strength
	const auto hitter = po.hitBy->entity;
	const auto spell = component->spell;
	if (!ecs::IsAvailable(hitter) || !ecs::physics::Buildings::PhysicallyDestroysAbodes(hitter) || !SpellAlive(spell) ||
	    !(GetSpellStrength(spell) > 0.0f))
	{
		return;
	}
	// SpellEvent 5 at the hitter's MapCoords (x, z and its altitude above the land as the y), no target
	const auto* transform = registry.TryGet<const Transform>(hitter);
	const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3(0.0f);
	const psys::SpellEventInfo event {.type = psys::SpellEventInfo::Type::Object,
	                                  .position = glm::vec3(at.x, at.y - LandAt(at.x, at.z), at.z),
	                                  .velocity = glm::vec3(0.0f),
	                                  .strength = 1.0f,
	                                  .checkShields = false,
	                                  .target = entt::null};
	auto& spellComponent = registry.Get<ecs::components::Spell>(spell);
	OpsOf(spellComponent.spellClass).spellEvent(spell, event);
	// PayFor(|v| x mass x chantCostPerImpactMomentum x 0.0001, forced)
	const float momentum = glm::length(po.hitBy->body.velocity) * po.hitBy->body.Mass();
	const auto* info = ShieldInfoOf(*component);
	const float cost = momentum * (info != nullptr ? info->chantCostPerImpactMomentum : 0.0f) * 0.0001f;
	if (registry.Valid(spell))
	{
		chants::PayFor(registry.Get<ecs::components::Spell>(spell), ChantContextOf(spell), cost, true);
	}
	// with a town, the town records the hitter's player as its aggressor. Only the aggressor and its turn are ported;
	// TODO(towns): the per-player aggression slots (the value plus a town info base when the slot is 0, times a weight
	// that then decays by 0.9), the town attack guidance sound and the creature mimic
	if (const auto town = spell_shield::TownOf(spell); registry.Valid(town))
	{
		auto& townComponent = registry.Get<ecs::components::Town>(town);
		// the player whose hand threw it (po.byPlayer), else no player and the town takes the interface's own: both are
		// openblack's PLAYER_ONE
		townComponent.aggressor = PlayerNames::PLAYER_ONE;
		townComponent.aggressorTurn = CurrentTurn();
	}
	const float strength = GetSpellStrength(spell);
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: PhysicalShield {} hit by {}: momentum {:.1f}, pays {:.1f}, strength {:.3f}",
		                   static_cast<uint32_t>(shield), static_cast<uint32_t>(hitter), momentum, cost, strength);
	}
	if (strength > 0.0f)
	{
		spell_shield::UpdateStruckReaction(spell);
	}
	else
	{
		spell_shield::SetUpDestroyedReaction(spell);
	}
}

bool map_shield::IsEffectReceiver(entt::entity shield, float burn)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	return component != nullptr && component->kind == MapShield::Kind::Physical && burn == 0.0f;
}

const std::vector<entt::entity>& map_shield::Shields()
{
	return ShieldList();
}

void map_shield::Clear()
{
	MapShieldDebugHooksData().anyCreated = false;
	ShieldList().clear(); // the entities and effects go with the registry's and the manager's resets
}
