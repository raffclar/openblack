/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeed.h"

#include <cmath>

#include <algorithm>
#include <optional>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellForest.h"
#include "Particles/PSysManager.h"
#include "Players.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Spell.h"
#include "SpellCreator.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
SpellSeed& SeedOf(entt::entity seed)
{
	return Locator::entitiesRegistry::value().Get<SpellSeed>(seed);
}

bool ValidSpell(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	return spell != entt::null && registry.Valid(spell) && registry.AllOf<Spell>(spell);
}

/// Whether the seed's spell is cast while it is held in the hand
bool IsSpellCastInHand(const SpellSeed& seed)
{
	return seed::InfoOf(seed).castType == SpellCastType::SpellCastInHand;
}

/// The seed's 2D radius: its scale times the mesh's largest horizontal extent. Uses the info's mesh, which the seed
/// keeps even while it is not drawn, not the Mesh component, which goes while the seed is hidden
float SeedRadius(entt::entity entity, const SpellSeed& seed)
{
	if (!Locator::entitiesRegistry::value().AllOf<Transform>(entity))
	{
		return 0.0f;
	}
	return ecs::object::MeshRadius2D(resources::HashIdentifier(seed::InfoOf(seed).mesh), ecs::object::GetScale(entity));
}

/// The highest top of the objects of the seed's cell (fixed list, then mobile) that are not the seed, not living and
/// not moving, overlapping the seed's circle. 0 if none or out of bounds
float TopOfObjectsUnder(entt::entity entity, const SpellSeed& seed, const glm::vec3& position)
{
	if (!cast_rules::InBounds(position))
	{
		return 0.0f;
	}
	const auto coords = map_coords::FromMetres(glm::vec2(position.x, position.z));
	return ecs::map_cells::TallestOverlapping(coords, entity, coords, SeedRadius(entity, seed), true);
}

/// Shows or hides the seed this frame (openblack: its Mesh, which the renderer and the hand's pick see)
void ShowMesh(entt::entity entity, const SpellSeed& seed, bool show)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool has = registry.AllOf<Mesh>(entity);
	if (show && !has)
	{
		registry.Assign<Mesh>(entity, resources::HashIdentifier(seed::InfoOf(seed).mesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.SetDirty();
	}
	else if (!show && has)
	{
		registry.Remove<Mesh>(entity);
		registry.SetDirty();
	}
}

/// Draws a seed over the spell it belongs to
void DrawFromSpell(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = SeedOf(entity);
	// the seed must follow its spell, and its own spell must be set
	if (!seed::FollowsSpell(seed) || !ValidSpell(seed.spell))
	{
		// not drawn: out of the hand nothing else draws it (the hand draws the one it holds, HandSpellSeed.cpp)
		const auto held = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHeldObject() : std::nullopt;
		if (!held.has_value() || *held != entity)
		{
			ShowMesh(entity, seed, false);
		}
		return;
	}
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	const auto& spell = registry.Get<const Spell>(seed.spell);
	// the altitude is the top of the objects under the seed, then the spell may adjust it: only the forest spell does,
	// every other spell class leaves it as is
	float altitude = TopOfObjectsUnder(entity, seed, transform->position);
	if (spell.spellClass == SpellClass::Forest)
	{
		altitude = spell_forest::AdjustSpellSeedPos(seed.spell, altitude);
	}
	// The seed goes to (x, ground + altitude, z) as a pure translation, so it is drawn upright, unturned and unscaled
	// whatever the hand left in it (the spin of the worship icon / hand is not kept). openblack's Transform also holds
	// the object scale: the four seeds that reach here (STORM, NATURE, SHIELD, PHYSICAL_SHIELD: seedFollowsSpell,
	// neither cast nor kept in the hand) all have the info scale 1, so scale 1 is the same.
	// Drawing the seed also gives it its draw collision
	const glm::vec3 at = ToWorld(glm::vec3(transform->position.x, altitude, transform->position.z));
	if (TraceEnabled() && (!registry.AllOf<Mesh>(entity) || std::abs(transform->position.y - at.y) > 0.25f))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: seed {} drawn over spell {} at ({:.1f}, {:.2f}, {:.1f}), altitude {:.2f}",
		                   static_cast<uint32_t>(entity), static_cast<uint32_t>(seed.spell), at.x, at.y, at.z, altitude);
	}
	if (transform->position != at || transform->rotation != glm::mat3(1.0f) || transform->scale != glm::vec3(1.0f))
	{
		transform->position = at;
		transform->rotation = glm::mat3(1.0f); // no turn
		transform->scale = glm::vec3(1.0f);
		registry.SetDirty();
	}
	ShowMesh(entity, seed, true);
}

/// Before a cast: drops the old spell link, takes the hand's spell info and sets the cast's duration and chants
void PrepareCast(entt::entity entity, MagicType type, psys::ProcessInfo& info, SpellCastData& castData,
                 const psys::ProcessInfo& handInfo)
{
	auto& seed = SeedOf(entity);
	seed.flags &= static_cast<uint8_t>(~1u);
	seed::ClearSpellLink(entity);
	if (seed.icon != entt::null)
	{
		seed::ClearSpellLink(entity);
	}
	// the interface's spell info: hand position, camera, direction and curl
	info.interfacePos = handInfo.interfacePos;
	info.cameraForward = handInfo.cameraForward;
	info.handPos = handInfo.handPos;
	info.direction = handInfo.direction;
	info.curl = handInfo.curl;
	const auto& tables = Locator::infoConstants::value();
	castData.duration = GetTimerWhenPlayerCasting(tables, type) * seed.castMultiplier;
	castData.chants = GetMagicEffectInfo(tables, type).initialChants * seed.castMultiplier;
	// A magic of the FIRE seed casts with magnitude 1 whatever circle was drawn (the magnitude is otherwise the
	// gesture's size): the seed its info names while the game runs, the first seed with that magic type
	if (GetSpellSeedOfMagicInfo(tables, type) == SpellSeedType::Fire)
	{
		castData.magnitude = 1.0f;
	}
}

/// After a cast: the seed and the new spell are linked, and the spell gets the seed's stored chants and age
void FinishCast(entt::entity entity, entt::entity spellEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = SeedOf(entity);
	auto& spell = registry.Get<Spell>(spellEntity);
	// a tribe with tribal power would use the tribal power column and the tribe's sound (never in vanilla: tribal
	// power 1)
	// linked to a worship icon: the icon's charge is cancelled
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		worship::icon::CancelCharge(seed.icon, seed.creator.player);
	}
	// the spell remembers the interface that cast it, and whether it is mine
	// (inferred: one local interface, both taken from the seed's inInterface flag)
	spell.castFromInterface = seed.inInterface;
	spell.isMyInterfaceCasting = seed.inInterface;
	seed.lastMagic = spell.magicType;
	if (seed.storedChants >= 0.0f)
	{
		chants::SetChants(spell, seed.storedChants);
		spell.age = seed.storedAge;
	}
	seed::SetChantStore(seed, 0.0f);
	// when the seed is my interface's (inferred, as above: inInterface), the help profile gets CastCreatureSpell (10)
	// for a creature spell, else CastSpell (9)
	if (seed.inInterface)
	{
		const bool creatureSpell = SlotOf(spell.magicType).section == MagicInfoSection::CreatureSpell;
		help_profile::Trigger(creatureSpell ? help_profile::Event::CastCreatureSpell : help_profile::Event::CastSpell);
	}
	if (!IsSpellCastInHand(seed))
	{
		if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
		{
			transform->position = ToWorld(spell.castPos);
		}
	}
	if (spell.seed == entt::null)
	{
		spell.seed = entity;
	}
	seed.spell = spellEntity;
	seed.hasCast = true;
	// TODO: start the magic's immersion (force feedback)
}
} // namespace

const GSpellSeedInfo& seed::InfoOf(const SpellSeed& seed)
{
	return GetSpellSeedInfo(Locator::infoConstants::value(), seed.seedType);
}

entt::entity seed::Create(const glm::vec3& worldPosition, SpellSeedType seedType, PlayerNames player, int powerUp,
                          float multiplier)
{
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seedType);
	const auto entity = ecs::archetypes::SpellSeedArchetype::Create(worldPosition, seedType, info.scale);
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = registry.Assign<SpellSeed>(entity);
	seed.seedType = seedType;
	seed.powerUp = powerUp;
	seed.castMultiplier = multiplier;
	seed.inInterface = true;
	seed.creator = creator::OfPlayer(player); // the interface's player
	// the stored chants, age and chant store start at 0 and the stored object count at -1 (stored chants of -1, "none", are
	// only written by StoreChantsAndAgeFromSpell)
	seed.storedAge = 0.0f;
	seed.storedChants = 0.0f;
	seed.chantStoreCopy = 0.0f;
	seed.chantStore = 0.0f;
	seed.storedMaxObjects = -1;
	return entity;
}

MagicType seed::MagicTypeOf(const SpellSeed& seed)
{
	return MagicInfoForPowerUpLevel(Locator::infoConstants::value(), InfoOf(seed), seed.powerUp).magicType;
}

float seed::GetChantNeeded(const SpellSeed& seed, int powerUp)
{
	const auto& tables = Locator::infoConstants::value();
	const auto type = MagicInfoForPowerUpLevel(tables, InfoOf(seed), powerUp).magicType;
	return GetChantsRequiredToCreate(tables, type) - seed.chantStore;
}

float seed::GetPower(const SpellSeed& seed)
{
	const float cost = GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(seed));
	// the original divides without a test: store / 0 is inf or NaN, and the min against 1.0 then gives 1
	const float power = cost > 0.0f ? seed.chantStore / cost : 1.0f;
	return power < 1.0f ? power : 1.0f;
}

void seed::SetChantStore(SpellSeed& seed, float chants)
{
	seed.chantStore = chants;
	seed.chantStoreCopy = chants;
}

void seed::AddToChantStore(SpellSeed& seed, float chants)
{
	SetChantStore(seed, seed.chantStore + chants);
}

void seed::SetPowerUp(entt::entity entity, int powerUp)
{
	auto& seed = SeedOf(entity);
	const bool lower = powerUp < seed.powerUp;
	seed.powerUp = powerUp;
	const float excess = -GetChantNeeded(seed, powerUp);
	// a charge above the cost goes back to the icon's worship site, the store keeps the cost
	auto& registry = Locator::entitiesRegistry::value();
	if (excess > 0.0f && seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		const auto site = registry.Get<const WorshipSpellIcon>(seed.icon).site;
		if (site != entt::null && registry.Valid(site) && registry.AllOf<WorshipSite>(site))
		{
			registry.Get<WorshipSite>(site).battery += excess;
			SetChantStore(seed, seed.chantStore - excess);
		}
	}
	if (!seed.inInterface)
	{
		return;
	}
	// The local interface: the effect's tooltip (no tooltips yet), the hand's in-hand effect, the hand FX's power-up
	// level (delayed) and, unless the level went down, the spell's hand visuals; the effect files; the level's voice
	hand_fx::CreateInHandEffect(entity);
	hand_fx::SetPowerUpLevel(powerUp + 1, true);
	if (!lower)
	{
		hand_fx::AddSpellToHandVisuals(false);
	}
	// power-up 0 / 1 / 2 -> SpellDialogue samples 10 / 11 / 12; none for -1. No owner, mode 2, no loop, not 3D
	if (powerUp >= 0 && powerUp <= 2)
	{
		audio::PlaySoundEffect(audio::Owner::None(), 10 + powerUp, 2, 0, false, false, audio::SfxBank::SpellDialogue);
	}
}

void seed::SetInactive(SpellSeed& seed, bool inactive)
{
	if (inactive)
	{
		seed.turnsInHand = 0;
		seed.ready = false;
	}
	else
	{
		seed.ready = true;
	}
}

int seed::InterfaceSetInMagicHand(entt::entity entity)
{
	SetPowerUp(entity, SeedOf(entity).powerUp);
	// my interface's hand (inferred: inInterface, as FinishCast): StopSpell (13) when the seed still has its
	// spell, else GetSpell (12)
	if (SeedOf(entity).inInterface)
	{
		help_profile::Trigger(SeedOf(entity).spell != entt::null ? help_profile::Event::StopSpell
		                                                         : help_profile::Event::GetSpell);
	}
	if (!StoreChantsAndAgeFromSpell(entity) || (SeedOf(entity).flags & 2u) != 0)
	{
		ToBeDeleted(entity);
		return 3;
	}
	auto& seed = SeedOf(entity);
	// the interface's last seed type (R repeats it)
	if (seed.inInterface)
	{
		gestures::State().lastSeedType = static_cast<int>(seed.seedType);
	}
	seed.flags &= static_cast<uint8_t>(~1u);
	seed.lastMagic = MagicType::None;
	seed.hasCast = false;
	seed.turnsInHand = 0;
	seed.ready = true;
	ClearSpellLink(entity);
	if (seed.icon != entt::null)
	{
		seed.flags &= static_cast<uint8_t>(~1u);
		ClearSpellLink(entity);
	}
	// the local hand: LoadFileData(particleType) (the effect files load on use here) and, for a tribe with tribal power
	// above 1, PHandFX::StartTribalPowerRing (never in the vanilla game)
	if (seed.inInterface)
	{
		const auto tribe = GetTribalPowerTribe(GetMagicEffectInfo(Locator::infoConstants::value(), MagicTypeOf(seed)),
		                                       &players::MagicOf(seed.creator.player).tribalPower);
		if (tribe)
		{
			hand_fx::StartTribalPowerRing(*tribe);
		}
	}
	return 1;
}

void seed::ProcessInHand(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	// the first turn, for the local interface: the power-up gestures are set up
	if (seed.turnsInHand == 0 && seed.inInterface)
	{
		gestures::SetupPowerUpGestures();
	}
	++seed.turnsInHand;
	if (!seed.ready)
	{
		const float held = static_cast<float>(seed.turnsInHand) * static_cast<float>(k_TurnMs) * 0.001f;
		if (held > Locator::infoConstants::value().spellSystem.delayBeforeSeedActive)
		{
			seed.ready = true;
			// the local interface ends its action
			if (seed.inInterface && Locator::handSystem::has_value())
			{
				Locator::handSystem::value().EndAction();
			}
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: seed {} ready after {} turns",
				                   static_cast<uint32_t>(entity), seed.turnsInHand);
			}
		}
	}
	if (ValidSpell(seed.spell) && Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown)
	{
		ToBeDeleted(entity);
	}
	// TODO: the generic object processing while in the hand
}

bool seed::StoreChantsAndAgeFromSpell(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	if (!ValidSpell(seed.spell))
	{
		seed.storedChants = -1.0f;
		seed.storedAge = 0.0f;
		seed.storedMaxObjects = -1;
		return true;
	}
	const auto spellEntity = seed.spell;
	auto& spell = Locator::entitiesRegistry::value().Get<Spell>(spellEntity);
	seed.storedChants = spell.chants;
	SetChantStore(seed, spell.chants);
	seed.storedAge = spell.age;
	const auto maxObjects = OpsOf(spell.spellClass).maxObjectsToCreate;
	seed.storedMaxObjects = maxObjects != nullptr ? maxObjects(spellEntity) : spell.maxObjectsToCreate;
	ClearSpellLink(entity);
	SeedOf(entity).hasCast = false;
	return OpsOf(spell.spellClass).hasEnoughChantsForRecast(spellEntity);
}

void seed::ClearSpellLink(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	const auto spellEntity = seed.spell;
	if (!ValidSpell(spellEntity))
	{
		seed.spell = entt::null;
		return;
	}
	// TODO: magicInfo.stopImmersion && the local interface -> StopImmersion(immersion)
	seed.spell = entt::null;
	auto& spell = Locator::entitiesRegistry::value().Get<Spell>(spellEntity);
	if (spell.seed == entity)
	{
		spell.seed = entt::null; // so the spell's own unlink finds no seed now
		CloseDown(spellEntity);
	}
	else if (spell.psys != 0)
	{
		psys::manager::CloseDown(spell.psys); // the spell's PSys only
	}
}

void seed::ApplyUnlockProcess(entt::entity entity)
{
	if (InfoOf(SeedOf(entity)).deleteSeedOnceCast != 0)
	{
		ToBeDeleted(entity);
		return;
	}
	if (!StoreChantsAndAgeFromSpell(entity))
	{
		ToBeDeleted(entity);
	}
}

bool seed::FollowsSpell(const SpellSeed& seed)
{
	const auto& info = InfoOf(seed);
	// not in the map (a seed is never put in it), not cast in hand, not kept in hand, seedFollowsSpell, the spell (if
	// any) still open, and linked to an icon. The icon link survives the cast: only ToBeDeleted clears it.
	const bool spellOpen = !ValidSpell(seed.spell) || !Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown;
	return !IsSpellCastInHand(seed) && info.isKeptInHand == 0 && info.seedFollowsSpell != 0 && spellOpen &&
	       seed.icon != entt::null;
}

bool seed::ValidForPlaceInHand(entt::entity entity, PlayerNames handPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return false;
	}
	const auto& seed = SeedOf(entity);
	// the seed's player is its interface's, none without one (openblack: inInterface, the creator)
	const bool hasPlayer = seed.inInterface;
	// influence everywhere (the gathering flag) and a neutral player
	if (influence::IsInfluenceEverywhere() && hasPlayer && seed.creator.player == PlayerNames::NEUTRAL)
	{
		return true;
	}
	// the hand's player is the seed's, and the seed is available (a seed being deleted is gone from openblack's
	// registry)
	return hasPlayer && seed.creator.player == handPlayer;
}

void seed::DrawSpells()
{
	// The original first draws a step for each player's six slots (not identified, not ported), the physical shields
	// (map_shield::DrawShields, MagicLoop.cpp) and each catchable fireball's invisible draw collision (not ported
	// here). Then each spell draws an object of its own (not identified) and its seed, the same in every spell class.
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> seeds;
	registry.Each<const Spell>([&](entt::entity, const Spell& spell) {
		if (spell.seed != entt::null && registry.Valid(spell.seed) && registry.AllOf<SpellSeed>(spell.seed))
		{
			seeds.push_back(spell.seed);
		}
	});
	for (const auto entity : seeds)
	{
		if (registry.Valid(entity) && registry.AllOf<SpellSeed>(entity))
		{
			DrawFromSpell(entity);
		}
	}
}

int seed::ProcessFromSpell(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	if (!FollowsSpell(seed))
	{
		return 1;
	}
	const auto& transform = Locator::entitiesRegistry::value().Get<Transform>(entity);
	if (influence::CalculatePlayerInfluence(seed.creator.player, transform.position) <= 0.0f && ValidSpell(seed.spell) &&
	    !Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown)
	{
		CloseDown(seed.spell);
	}
	return 1;
}

bool seed::CanCast(entt::entity entity, const glm::vec3& position)
{
	const auto& seed = SeedOf(entity);
	const auto type = MagicTypeOf(seed);
	// the cast rule for the seed's player; then the spell class's own check
	if (!cast_rules::CanCastRule(GetMagicInfo(Locator::infoConstants::value(), type), position, seed.creator.player))
	{
		return false;
	}
	return cast_rules::CanCastAt(type, position);
}

int seed::Cast(entt::entity entity, const glm::vec3& position, entt::entity* out, float magnitude,
               const psys::ProcessInfo& handInfo)
{
	auto& seed = SeedOf(entity);
	const MagicType type = seed.lastMagic != MagicType::None ? seed.lastMagic : MagicTypeOf(seed);
	if (!ValidSpell(seed.spell))
	{
		if (seed.hasCast)
		{
			*out = entt::null;
			return 0;
		}
	}
	else if (IsSpellCastInHand(seed))
	{
		*out = seed.spell;
		return 1;
	}
	SpellCastData castData {magnitude, 0.0f, 0.0f, seed.storedMaxObjects};
	psys::ProcessInfo info {
	    .power = 1.0f,
	    .enabled = true,
	};
	PrepareCast(entity, type, info, castData, handInfo);
	entt::entity spell = entt::null;
	const int result = CastAtPos(type, SeedOf(entity).creator, position, &spell, &castData, info);
	if (result != 0 && spell != entt::null)
	{
		FinishCast(entity, spell);
	}
	*out = spell;
	return result;
}

void seed::ToBeDeleted(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	// the seed leaves its worship icon, then has no icon
	if (const auto icon = SeedOf(entity).icon; icon != entt::null)
	{
		worship::icon::RemoveSeed(icon, entity);
	}
	SeedOf(entity).icon = entt::null;
	ClearSpellLink(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
