/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameHandGrabWorld.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureHelp.h"
#include "ECS/AltarOffering.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/GodHand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ReportedCamera.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerMemory.h"
#include "ECS/WorldObjects.h"
#include "EngineConfig.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandReach.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"
#include "Magic/MagicTables.h"
#include "Magic/ResourcePiles.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/TempleEntranceTap.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A pulled-up tree's hole of roots is this share of its width across
constexpr float k_RootsHoleShare = 0.3f;

/// The particles of a handful poured out of the hand: food and wood
constexpr auto k_PourFood = ParticleType::FoodPutdown;
constexpr auto k_PourWood = ParticleType::WoodPutdown;
constexpr auto k_PourPoisonedFood = ParticleType::FoodPutdownPoisoned;
/// The particles of what is scooped streaming into the hand: food and wood
constexpr auto k_ScoopFood = ParticleType::FoodPickup;
constexpr auto k_ScoopWood = ParticleType::WoodPickup;
constexpr auto k_ScoopPoisonedFood = ParticleType::FoodPickupPoisoned;
/// The scooping sounds of the in-game bank, for food and for wood: loops that play until stopped
constexpr auto k_ScoopSound = audio::SoundId::G_PickUpFood;
constexpr auto k_ScoopWoodSound = audio::SoundId::G_PickUpWood;

const graphics::L3DMesh* MeshOf(const Registry& registry, entt::entity object)
{
	const auto* mesh = registry.TryGet<const Mesh>(object);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh->id) ? &*meshes.Handle(mesh->id) : nullptr;
}

const InfoConstants* Info()
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

PhysicsEntry* EntryOf(entt::entity object)
{
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(object) : nullptr;
}
} // namespace

Registry& GameHandGrabWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity GameHandGrabWorld::Hand() const
{
	if (!Locator::handSystem::has_value())
	{
		return entt::null;
	}
	const auto& hands = Locator::handSystem::value();
	constexpr auto k_Left = static_cast<size_t>(HandSystemInterface::Side::Left);
	// A command acts with its player's hand, which a player of a network game has from the first turn
	if (_acting.has_value())
	{
		const auto acting = hands.GetHandsOf(*_acting);
		return acting.has_value() ? (*acting)[k_Left] : entt::null;
	}
	return hands.GetPlayerHands()[k_Left];
}

bool GameHandGrabWorld::IsNetworkGame() const
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

std::optional<PlayerNames> GameHandGrabWorld::OwnHandPlayer() const
{
	if (!Locator::handSystem::has_value() || Locator::handSystem::value().GetOwnPlayer() == PlayerNames::PLAYER_ONE)
	{
		return std::nullopt;
	}
	return Locator::handSystem::value().GetOwnPlayer();
}

PlayerNames GameHandGrabWorld::HandPlayer() const
{
	if (_acting.has_value())
	{
		return *_acting;
	}
	// The first hands are the local player's
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

std::optional<entt::entity> GameHandGrabWorld::ObjectUnderCursor() const
{
	// The object the interface picked under the cursor as the last frame was drawn
	if (!Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::pickingSystem::value().GetPick().object;
}

namespace
{
/// Whether a point is over a cell of water, a cell off the map or without land counting as water
bool OverWater(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto cell = map_coords::CellOf(point);
	if (!map_coords::InBounds(cell))
	{
		return true;
	}
	const auto* landCell = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
	return landCell == nullptr || landCell->properties.hasWater != 0;
}
} // namespace

std::optional<entt::entity> GameHandGrabWorld::ObjectNearHand() const
{
	if (!Locator::pickingSystem::has_value() || !Locator::entitiesMap::has_value())
	{
		return std::nullopt;
	}
	const auto& picking = Locator::pickingSystem::value();
	const auto behind = picking.GetBehindHand();
	if (!behind.has_value())
	{
		return std::nullopt;
	}
	const auto behindHand = map_coords::FromMetres({behind->x, behind->z});
	// The point the interface picked, the map's corner when it picked none
	const auto picked = picking.GetPick().point;
	// With the hand over water, a fish farm whose fish show under the picked point is taken first, as a farm has no model
	if (OverWater(*behind) && picked.has_value() && Locator::fishFarmSystem::has_value())
	{
		if (const auto farm = Locator::fishFarmSystem::value().FarmWithFishAt({picked->x, picked->z}))
		{
			return farm;
		}
	}
	const auto cursor = picked.has_value() ? map_coords::FromMetres({picked->x, picked->z}) : map_coords::MapCoords {};
	// The things in the cells about the point, as a search meets them; broken pieces of buildings are passed by
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto cells = hand_reach::CellsAround(behindHand);
	std::vector<entt::entity> things;
	std::vector<map_coords::MapCoords> places;
	for (int32_t x = cells.first.x; x <= cells.last.x; ++x)
	{
		for (int32_t z = cells.first.y; z <= cells.last.y; ++z)
		{
			for (const auto entity : map.GetAllInCell({x, z}))
			{
				const auto* transform = registry.TryGet<const Transform>(entity);
				if (transform == nullptr || registry.AllOf<BuildingPiece>(entity))
				{
					continue;
				}
				things.push_back(entity);
				places.push_back(map_coords::FromMetres({transform->position.x, transform->position.z}));
			}
		}
	}
	const auto nearest = hand_reach::Nearest(behindHand, cursor, places);
	return nearest.has_value() ? std::optional<entt::entity>(things[*nearest]) : std::nullopt;
}

std::optional<glm::vec3> GameHandGrabWorld::PickedPoint() const
{
	return Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().point : std::nullopt;
}

bool GameHandGrabWorld::InInfluence(PlayerNames player, glm::vec3 point) const
{
	return Locator::influenceSystem::has_value() && Locator::influenceSystem::value().PlayerInfluence(player, point) > 0.0f;
}

bool GameHandGrabWorld::HandInInfluence(PlayerNames player, glm::vec3 hand) const
{
	return Locator::influenceSystem::has_value() && Locator::influenceSystem::value().IsHandInInfluence(player, hand);
}

void GameHandGrabWorld::HeldThingUsedOnLand(PlayerNames player)
{
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().HeldThingUsedOnLand(player);
	}
}

bool GameHandGrabWorld::InBounds(glm::vec3 point) const
{
	return map_coords::InBounds(point);
}

glm::vec3 GameHandGrabWorld::LandNormalAt(glm::vec3 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetNormalAt({point.x, point.z})
	                                           : glm::vec3(0.0f, 1.0f, 0.0f);
}

hand_grab::HandGrabWorldInterface::Size GameHandGrabWorld::SizeOf(entt::entity object) const
{
	// A fish farm has no model: it reaches as far as its table says, and the hand keeps that high over it
	if (Locator::entitiesRegistry::value().AllOf<FishFarm>(object))
	{
		return {.radius = fish_farm::k_Radius, .height = fish_farm::k_HandHeight};
	}
	const auto size = world_objects::SizeOf(object);
	return {.radius = size.radius, .height = size.height};
}

float GameHandGrabWorld::WeightOf(entt::entity object) const
{
	const auto* info = world_objects::InfoOf(object);
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	if (info == nullptr || transform == nullptr)
	{
		return 0.0f;
	}
	return physics_classes::Weight(info->weight, transform->scale.x);
}

float GameHandGrabWorld::LifeOf(entt::entity object) const
{
	return world_objects::LifeOf(object);
}

bool GameHandGrabWorld::IsFlying(entt::entity object) const
{
	return Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(object);
}

bool GameHandGrabWorld::SpeciesAllowsPickUp(entt::entity animal) const
{
	return Locator::animalSystem::has_value() && Locator::animalSystem::value().CanPlayerPickUp(animal);
}

MobileStaticInfo GameHandGrabWorld::StaticKindOf(MobileStaticInfo type) const
{
	const auto* info = Info();
	const auto index = static_cast<size_t>(type);
	return info != nullptr && index < info->mobileStatic.size() ? info->mobileStatic[index].mobileType : MobileStaticInfo::None;
}

MeshId GameHandGrabWorld::StaticMeshOf(MobileStaticInfo type) const
{
	const auto* info = Info();
	const auto index = static_cast<size_t>(type);
	return info != nullptr && index < info->mobileStatic.size() ? info->mobileStatic[index].meshId : MeshId::Dummy;
}

bool GameHandGrabWorld::IsLoosePot(PotInfo type) const
{
	const auto* info = Info();
	const auto index = static_cast<size_t>(type);
	return info != nullptr && index < info->pot.size() && info->pot[index].potType == PotType::Pot;
}

bool GameHandGrabWorld::IsOfRockMaterial(entt::entity object) const
{
	const auto* info = Info();
	if (info == nullptr)
	{
		return false;
	}
	return physics_classes::Classify(Locator::entitiesRegistry::value(), object, *info, {}).row == physics::MaterialRow::Rock;
}

bool GameHandGrabWorld::IsSexuallyActive(entt::entity villager) const
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Villager>(villager);
	const auto* info = Info();
	if (data == nullptr || info == nullptr)
	{
		return false;
	}
	const auto& kind = info->villager.at(static_cast<size_t>(RowOf(*data, GVillagerInfo::Find)));
	const auto age = villager_age::AgeNow(*data);
	return kind.startHavingSexAge <= age && age < kind.stopHavingSexAge;
}

std::optional<PlayerNames> GameHandGrabWorld::PlayerOf(entt::entity object) const
{
	return world_objects::PlayerOf(object);
}

void GameHandGrabWorld::LeavePhysicsAndMap(entt::entity object)
{
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().RemoveObject(object, false, false);
	}
	Locator::entitiesRegistry::value().Remove<MapCellResident, MapCellMover, InPhysics>(object);
}

void GameHandGrabWorld::CreateReaction(const ReactionRequest& request)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create(
		    {.initiator = request.initiator, .type = request.type, .player = request.player, .position = request.position});
	}
}

void GameHandGrabWorld::RemoveReactions(entt::entity initiator, Reaction type)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().RemoveFrom(initiator, type);
	}
}

void GameHandGrabWorld::FireStartedMoving(entt::entity object, bool inHand)
{
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().StartedMoving(object, inHand);
	}
}

void GameHandGrabWorld::CatchFirefly(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	if (transform == nullptr || !Locator::fireflySystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	Locator::fireflySystem::value().Catch(map_coords::FromWorld(Locator::terrainSystem::value(), transform->position));
}

void GameHandGrabWorld::CountHelpEvent(uint32_t event)
{
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(event);
	}
}

void GameHandGrabWorld::ForgetGesturePath()
{
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ForgetPath();
	}
}

void GameHandGrabWorld::HeatHeld(entt::entity object)
{
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().HeatHeldObject(object);
	}
}

void GameHandGrabWorld::PlaySample(uint32_t sample, glm::vec3 position)
{
	if (Locator::audio::has_value())
	{
		// Each of these sounds is a sound of its own to the voice rules. It stays where it started, and it stops when
		// the camera goes too far.
		audio::StartGameSoundEffect(
		    entt::hashed_string(fmt::format("InGame.sad/{}", sample).c_str()).value(),
		    {.position = position, .voiceOwner = audio::VoiceOwner::OwnSound, .follows = audio::Follows::Place});
	}
}

void GameHandGrabWorld::SendCommand(player_commands::Action action)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(std::move(action));
	}
}

uint32_t GameHandGrabWorld::LocalRandom(uint32_t count)
{
	if (count == 0 || !Locator::gameRandom::has_value())
	{
		return 0;
	}
	return Locator::gameRandom::value().LocalRand(static_cast<int32_t>(count)) % count;
}

void GameHandGrabWorld::VillagerIntoHand(entt::entity villager)
{
	villager_physics::IntoHand(villager);
}

void GameHandGrabWorld::ArtefactTaken(entt::entity object, PlayerNames player)
{
	object_physics::ArtefactTaken(object, player);
}

void GameHandGrabWorld::AnimalIntoOwnFlock(entt::entity animal)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* data = registry.TryGet<Animal>(animal);
	if (data == nullptr || !Locator::animalSystem::has_value())
	{
		return;
	}
	// In the hand it plays its kind's clip for being held
	Locator::animalSystem::value().IntoHand(animal);
	auto* flock = registry.TryGet<Flock>(data->flock);
	if (flock == nullptr)
	{
		return;
	}
	// It goes into a flock of its own, as wide as the one it left, which goes once it has no one left
	// TODO(hand): a flock a script holds stays even when it is left empty (openblack's scripts hold no flocks)
	const auto old = data->flock;
	std::erase(flock->members, animal);
	const auto& position = registry.Get<const Transform>(animal).position;
	const auto own = Locator::animalSystem::value().CreateFlock(
	    glm::vec2(position.x, position.z), static_cast<float>(flock->domainRadius), static_cast<float>(flock->flockDistance));
	registry.Get<Flock>(own).members.insert(registry.Get<Flock>(own).members.begin(), animal);
	data->flock = own;
	if (registry.Get<const Flock>(old).members.empty())
	{
		registry.Destroy(old);
	}
}

void GameHandGrabWorld::TreeUprooted(PlayerNames player, entt::entity /*tree*/)
{
	// Pulling up a tree is bad, as planting one is good
	if (player == PlayerNames::NEUTRAL || !Locator::alignmentSystem::has_value() || Info() == nullptr)
	{
		return;
	}
	auto& alignment = Locator::alignmentSystem::value();
	alignment.AddPendingAlignment(
	    player, magic::DampAlignmentChange(-Info()->player.treePullPutAlignmentChange, alignment.GetPlayerAlignment(player)));
}

std::optional<entt::entity> GameHandGrabWorld::TreeFromBigForest(entt::entity bigForest)
{
	if (!Locator::forestSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::forestSystem::value().TreeFromBigForest(bigForest);
}

bool GameHandGrabWorld::LeashTakesTap(entt::entity object, PlayerNames player)
{
	return Locator::leashSystem::has_value() && Locator::leashSystem::value().TakesTap(player, object);
}

void GameHandGrabWorld::GlobeIntoHand(entt::entity globe, PlayerNames player)
{
	const auto* info = Info();
	const auto* component = Entities().TryGet<const OneOffSpellSeed>(globe);
	if (info == nullptr || component == nullptr || !Locator::playerSystem::has_value())
	{
		return;
	}
	auto* data = Entities().TryGet<Player>(Locator::playerSystem::value().GetPlayer(player));
	if (data == nullptr)
	{
		return;
	}
	// The miracle the globe holds at its power-up counts as ever enabled for the player, though it enables nothing
	const auto type =
	    magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(*info, component->seedType), component->powerUp);
	player_miracles::MarkMagicTypeEverEnabled(data->miracles, type);
}

bool GameHandGrabWorld::TapThing(entt::entity object, glm::vec3 handPoint, PlayerNames player)
{
	const bool ownHand = !Locator::playerSystem::has_value() || Locator::playerSystem::value().GetLocalPlayer() == player;
	// The help counts each tap of this computer's hand on a thing that takes the tap, before the thing answers it
	const auto countTap = [ownHand]() {
		if (ownHand && Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Trigger(help::profile::k_TapThing);
		}
	};
	// A rock tall enough breaks when tapped
	if (object_physics::CanTapRock(object) && Locator::dynamicsSystem::has_value())
	{
		countTap();
		object_physics::TapRock(Locator::dynamicsSystem::value(), object, handPoint, player);
		return true;
	}
	// The hand knocks on a building of a town, whoever owns the town. A miracle dispenser is such a building. When the
	// leash takes the tap as an order to the creature, the hand does not knock.
	if (Entities().AnyOf<Abode, SpellDispenser>(object) && Locator::abodeKnockSystem::has_value() &&
	    !LeashTakesTap(object, player))
	{
		countTap();
		return Locator::abodeKnockSystem::value().Tap(object, handPoint, ownHand);
	}
	// A temple's entrance takes the player of this computer into their own temple. The held leash takes the tap first.
	if (const auto* entrance = Entities().TryGet<const TempleEntrance>(object); entrance != nullptr)
	{
		const auto* heart = Entities().Valid(entrance->temple) ? Entities().TryGet<const Temple>(entrance->temple) : nullptr;
		const auto tap = openblack::temple::TapEntrance({
		    .scriptLetsTap = Entities().Context().scriptLetsTempleBeEntered,
		    .ownTemple = ownHand && heart != nullptr && heart->owner == player,
		    .leashHeld = LeashTakesTap(object, player),
		});
		if (tap.tapped)
		{
			countTap();
		}
		if (tap.enters && Locator::temple::has_value())
		{
			Locator::temple::value().ActivateAfterFrame(TempleRoom::Main);
		}
		return tap.tapped;
	}
	// A script's scroll or sign answers the tap itself
	if (const auto* highlight = Entities().TryGet<const ScriptHighlight>(object);
	    highlight != nullptr && Locator::scriptHighlightSystem::has_value())
	{
		if (script_highlights::ValidToTap(highlight->kind, highlight->scriptId, highlight->active))
		{
			countTap();
		}
		return Locator::scriptHighlightSystem::value().Tap(object, ownHand);
	}
	// A one-shot miracle's globe gives the hand its miracle
	if (Entities().AllOf<OneOffSpellSeed>(object) && Locator::magicSystem::has_value())
	{
		countTap();
		return Locator::magicSystem::value().TapGlobe(object);
	}
	// A reward chest is opened by its player's tap, until it is open
	if (const auto* reward = Entities().TryGet<const Reward>(object); reward != nullptr && Locator::rewardSystem::has_value())
	{
		if (reward->player == player && reward->state != Reward::State::Opened)
		{
			countTap();
		}
		return Locator::rewardSystem::value().Tap(object, player);
	}
	// A stone hand on a village centre, or a worship site's icon, charges its miracle for the hand
	if (Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().IsIcon(object))
	{
		// A tap that is not valid does nothing: the help does not count it, and the icon does not take it
		if (!Locator::spellIconSystem::value().ValidToTap(object, player))
		{
			return false;
		}
		countTap();
		return Locator::spellIconSystem::value().Tap(object, player);
	}
	// Other things' taps are the clicking and activating of the interface
	return false;
}

std::optional<creature_giving::Verdict> GameHandGrabWorld::GiveVerdict(entt::entity target, entt::entity held) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = registry.Valid(target) ? registry.TryGet<const Creature>(target) : nullptr;
	if (creature == nullptr || !registry.Valid(held))
	{
		return std::nullopt;
	}
	// openblack has no alliances between players, so only the creature's own player may give it things. The hand never
	// holds a miracle's seed as a thing (seeds are held by the miracles), so no seed is ever offered this way.
	const auto giver = HandPlayer();
	const bool heldByHand =
	    Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetLockedCreature(giver) == target;
	return creature_giving::MayGive({
	    .creatureHasPlayer = true,
	    .giverHasPlayer = true,
	    .friendly = creature->owner == giver,
	    .spellSeed = false,
	    .creatureCanPickUp = Locator::creatureObjectActionSystem::has_value() &&
	                         Locator::creatureObjectActionSystem::value().CanTakeFromHand(target, held),
	    .ogre = creature->species == CreatureType::Ogre,
	    .scriptControlled = registry.AllOf<ScriptControlled>(target),
	    .heldByHand = heldByHand,
	});
}

void GameHandGrabWorld::SayCantPickUp(entt::entity creature)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(
		    creature, {.kind = creature_help::Kind::MiscellaneousStacked, .parameter = creature_giving::k_CantPickUpHelp});
	}
}

bool GameHandGrabWorld::GiveTo(entt::entity creature, entt::entity held, bool leftHand)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	const auto camera = openblack::ecs::ReportedPlayerCamera(HandPlayer());
	const auto eye = camera.has_value() ? camera->origin : glm::vec3(0.0f);
	return Locator::creatureMindSystem::value().TakeFromHand(creature, held, leftHand, glm::vec2(eye.x, eye.z));
}

std::optional<hand_grab::HandGrabWorldInterface::Taking> GameHandGrabWorld::TakingOf(entt::entity creature) const
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return std::nullopt;
	}
	const auto taking = Locator::creatureObjectActionSystem::value().GetTakingFromHand(creature);
	if (!taking.has_value())
	{
		return std::nullopt;
	}
	return Taking {.object = taking->object, .msFromTake = taking->msFromTake};
}

std::optional<glm::vec3> GameHandGrabWorld::TakePointOf(entt::entity creature, glm::vec3 hand) const
{
	return Locator::creatureObjectActionSystem::has_value()
	           ? Locator::creatureObjectActionSystem::value().TakePoint(creature, hand)
	           : std::nullopt;
}

bool GameHandGrabWorld::StillTaking(entt::entity creature) const
{
	return Locator::creatureMindSystem::has_value() && Locator::creatureMindSystem::value().IsTakingFromHand(creature);
}

float GameHandGrabWorld::CreatureSizeOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	return body != nullptr ? ShownSize(*body) : 1.0f;
}

void GameHandGrabWorld::CallOffGiving(entt::entity creature)
{
	// It gives up taking as a failure, and its hand closes on nothing if it was already reaching
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetTakingFromHand(creature).has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AbandonAction(creature);
	}
}

bool GameHandGrabWorld::RightHanded() const
{
	return Locator::config::has_value() && Locator::config::value().rightHandedHand;
}

bool GameHandGrabWorld::HoldsLooseLeash() const
{
	if (!Locator::leashSystem::has_value())
	{
		return false;
	}
	const auto& leashes = Locator::leashSystem::value();
	const auto creature = leashes.PlayersCreature(HandPlayer());
	return creature.has_value() && leashes.HolderPoint(*creature).has_value();
}

bool GameHandGrabWorld::LeashTap(entt::entity object)
{
	return Locator::leashSystem::has_value() && Locator::leashSystem::value().TapThing(HandPlayer(), object);
}

void GameHandGrabWorld::RemakeDrawList(uint32_t frames)
{
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().RequestRebuild(frames);
	}
}

void GameHandGrabWorld::LeaveRootsHole(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(tree);
	const auto* mesh = MeshOf(registry, tree);
	if (transform == nullptr || mesh == nullptr || !Locator::explosionSystem::has_value())
	{
		return;
	}
	// The hole is as wide as the tree's model is wide and deep, a share of it, and turned as the tree was
	const auto half = mesh->GetBoundingBox().Size() * 0.5f;
	const float scale = (half.x + half.z) * transform->scale.x * k_RootsHoleShare;
	const float yaw = std::atan2(-transform->rotation[0].z, transform->rotation[0].x);
	Locator::explosionSystem::value().AddRubble(transform->position, yaw, scale);
}

void GameHandGrabWorld::RemovePotReaction(entt::entity pot)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Pot>(pot);
	const auto* info = Info();
	if (data == nullptr || info == nullptr || !Locator::reactionSystem::has_value())
	{
		return;
	}
	const auto reaction = info->pot.at(static_cast<size_t>(data->type)).associatedReaction;
	if (reaction != Reaction::None)
	{
		Locator::reactionSystem::value().RemoveFrom(pot, reaction);
	}
}

void GameHandGrabWorld::SetUpPotReaction(entt::entity pot, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.TryGet<const Pot>(pot);
	const auto* transform = registry.TryGet<const Transform>(pot);
	const auto* info = Info();
	if (data == nullptr || transform == nullptr || info == nullptr || !Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	const auto reaction = info->pot.at(static_cast<size_t>(data->type)).associatedReaction;
	if (reaction != Reaction::None && !reactions.HasReaction(pot))
	{
		reactions.Create({.initiator = pot, .type = reaction, .player = player, .position = transform->position});
	}
}

glm::vec3 GameHandGrabWorld::ReportedHandPlace() const
{
	if (!Locator::handSystem::has_value())
	{
		return glm::vec3(0.0f);
	}
	return ReportedPlaceOf(Hand());
}

glm::vec3 GameHandGrabWorld::ReportedPlaceOf(entt::entity hand) const
{
	// Where the last report of the hand's player put the hand
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? reported->handPoint : glm::vec3(0.0f);
}

bool GameHandGrabWorld::IsOtherPlayersHand(entt::entity hand) const
{
	// Only a network game, or a testbed scenario that acts as one, gives hands to the other gods
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	// The hands of the first player have no god's mark
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* god = registry.Valid(hand) ? registry.TryGet<const GodHand>(hand) : nullptr;
	const auto player = god != nullptr ? god->player : PlayerNames::PLAYER_ONE;
	return player != Locator::handSystem::value().GetOwnPlayer();
}

std::optional<glm::vec3> GameHandGrabWorld::ReportedCollidePoint() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = Hand();
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? std::optional(reported->collidePoint) : std::nullopt;
}

glm::vec3 GameHandGrabWorld::AsMapPlace(glm::vec3 point) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return point;
	}
	const auto& island = Locator::terrainSystem::value();
	return map_coords::ToWorld(island, map_coords::FromWorld(island, point));
}

std::vector<PlayerNames> GameHandGrabWorld::OtherHandPlayers() const
{
	std::vector<PlayerNames> players;
	if (!Locator::handSystem::has_value())
	{
		return players;
	}
	const auto& hands = Locator::handSystem::value();
	for (size_t i = 1; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		if (hands.GetHandsOf(static_cast<PlayerNames>(i)).has_value())
		{
			players.push_back(static_cast<PlayerNames>(i));
		}
	}
	return players;
}

hand_grab::HandGrabWorldInterface::Pose GameHandGrabWorld::PoseOf(entt::entity object) const
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return {};
	}
	auto axes = transform->rotation;
	for (glm::length_t column = 0; column < 3; ++column)
	{
		axes[column] *= transform->scale[column];
	}
	return {.axes = axes, .origin = transform->position};
}

void GameHandGrabWorld::SetPose(entt::entity object, const Pose& pose)
{
	auto* transform = Locator::entitiesRegistry::value().TryGet<Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	for (glm::length_t column = 0; column < 3; ++column)
	{
		const float length = glm::length(pose.axes[column]);
		transform->rotation[column] = length > 0.0f ? pose.axes[column] / length : pose.axes[column];
		transform->scale[column] = length;
	}
	transform->position = pose.origin;
	Locator::entitiesRegistry::value().SetDirty();
}

glm::mat3 GameHandGrabWorld::UnstretchedAxes(entt::entity object, const glm::mat3& axes) const
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	// Its own size is the one across, which stretching up never changes
	const float scale = transform != nullptr ? transform->scale.x : 1.0f;
	return hand_grab::AxesAtSize(axes, scale);
}

player_commands::ThrowStart GameHandGrabWorld::ThrowStartOf(entt::entity object, bool alignToSlope)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// A globe starts as it was last drawn, turned to this player's camera. Any other thing starts at its Transform.
	std::optional<std::pair<glm::mat3, glm::vec3>> drawn;
	if (const auto* globe = registry.TryGet<const OneOffSpellSeed>(object);
	    globe != nullptr && globe->drawnTurn.has_value() && globe->drawnModel.has_value())
	{
		drawn = std::pair {*globe->drawnTurn, glm::vec3((*globe->drawnModel)[3])};
	}
	if (Locator::dynamicsSystem::has_value())
	{
		if (const auto start = Locator::dynamicsSystem::value().StartOfThrow(object, drawn, alignToSlope))
		{
			return *start;
		}
	}
	// A thing with no body starts where it is
	const auto* transform = registry.TryGet<const Transform>(object);
	if (!drawn.has_value() && transform != nullptr)
	{
		drawn = std::pair {transform->rotation, transform->position};
	}
	if (!drawn.has_value())
	{
		return {};
	}
	return {.origin = drawn->second, .angles = x87::GameTurnMatrixAngles(drawn->first)};
}

void GameHandGrabWorld::StartThrowFrom(entt::entity object, const player_commands::ThrowStart& start)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.TryGet<Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	// The turn comes from the angles, and the thing keeps its own size across
	transform->rotation = x87::GameTurnMatrix(start.angles.x, start.angles.y, start.angles.z);
	transform->scale = glm::vec3(transform->scale.x);
	transform->position = start.origin;
	registry.SetDirty();
}

std::optional<uint32_t> GameHandGrabWorld::PourPot(entt::entity pot, PlayerNames player)
{
	const auto facts = PotFactsOf(pot);
	if (!facts.has_value())
	{
		return std::nullopt;
	}
	// It pours out of the hand where the last report put the hand
	const auto hand = ReportedHandPlace();
	std::optional<uint32_t> pour;
	if (Locator::particleSystem::has_value() && player == HandPlayer())
	{
		// A poisoned handful pours its own poisoned food
		const auto particles = facts->resource == ResourceType::Wood ? k_PourWood
		                       : facts->poisoned                     ? k_PourPoisonedFood
		                                                             : k_PourFood;
		pour = Locator::particleSystem::value().Start(particles, hand, 1.0f);
	}
	// The handful is gone from the hand first, so that it can't take back what it pours. What it held goes to the stores
	// and piles of it about the point, or makes a pile there; in the water it is lost.
	UseUp(pot);
	PourAt(facts->resource, hand, facts->amount, player, facts->poisoned);
	return pour;
}

std::optional<hand_grab::HandGrabWorldInterface::PotFacts> GameHandGrabWorld::PotFactsOf(entt::entity pot) const
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Pot>(pot);
	const auto* info = Info();
	if (data == nullptr || info == nullptr)
	{
		return std::nullopt;
	}
	const auto& kind = info->pot.at(static_cast<size_t>(data->type));
	// A pile of food is scooped into a handful of food, a pile of wood into one of wood
	return PotFacts {.potType = kind.potType,
	                 .resource = kind.resourceType,
	                 .handful = kind.resourceType == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood,
	                 .amount = data->amount,
	                 .poisoned = data->poisoned};
}

hand_grab::ScoopFacts GameHandGrabWorld::ScoopFactsOf(PotInfo handful) const
{
	const auto* info = Info();
	if (info == nullptr)
	{
		return {};
	}
	const auto& kind = info->pot.at(static_cast<size_t>(handful));
	return {.initial = kind.amountPickedUpInitially,
	        .perTurn = kind.amountPickedUpPerTurn,
	        .perTurnEnd = kind.amountPickedUpPerTurnEnd,
	        .maxPickedUp = kind.maxAmountCanBePickedUp,
	        .rampSeconds = kind.multiPickUpRampTime};
}

uint32_t GameHandGrabWorld::TakeFromPile(entt::entity pile, uint32_t amount)
{
	const auto facts = PotFactsOf(pile);
	if (!facts.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		return 0;
	}
	// Taking from a town's store counts against its owner, and is remembered of the hand's player
	return Locator::resourceStoreSystem::value().TakeFromPile(pile, facts->resource, amount, HandPlayer());
}

std::optional<fish_farm::Type> GameHandGrabWorld::FishFarmOf(entt::entity farm) const
{
	if (!Locator::entitiesRegistry::value().AllOf<FishFarm>(farm) || !Locator::fishFarmSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::fishFarmSystem::value().GetType();
}

uint32_t GameHandGrabWorld::TakeFromFishFarm(entt::entity farm, uint32_t amount)
{
	return Locator::fishFarmSystem::has_value() ? Locator::fishFarmSystem::value().TakeFish(farm, amount) : 0;
}

std::optional<hand_grab::HandGrabWorldInterface::FieldFacts> GameHandGrabWorld::FieldFactsOf(entt::entity field) const
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Field>(field);
	const auto* info = Info();
	if (data == nullptr || info == nullptr)
	{
		return std::nullopt;
	}
	const auto& type = info->fieldType.at(static_cast<size_t>(data->type));
	return FieldFacts {.food = data->crop.food > 0.0f ? static_cast<uint32_t>(data->crop.food) : 0u,
	                   .ripe = data->crop.age >= type.ageRecolt,
	                   .lockable = data->crop.age > 0.0f && data->crop.food > 1.0f};
}

void GameHandGrabWorld::NoteTakenAt(glm::vec3 point)
{
	auto& registry = Entities();
	std::vector<influence_rules::PlayerInfluenceAt> players;
	registry.Each<const Player>([&players, point](entt::entity, const Player& player) {
		const float influence = Locator::influenceSystem::has_value()
		                            ? Locator::influenceSystem::value().PlayerInfluence(player.name, point)
		                            : 0.0f;
		players.push_back({.player = player.name, .influence = influence});
	});
	// The players are asked in the order of their numbers
	std::ranges::sort(players, {}, &influence_rules::PlayerInfluenceAt::player);
	SetHandTookFrom(influence_rules::MostInfluentialPlayer(players));
}

void GameHandGrabWorld::NoteTakenFromOwnerOf(entt::entity thing)
{
	auto& registry = Entities();
	entt::entity town = entt::null;
	if (const auto* pot = registry.TryGet<const Pot>(thing))
	{
		town = pot->town;
	}
	else if (const auto* farm = registry.TryGet<const FishFarm>(thing))
	{
		town = farm->town;
	}
	const auto* owner = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	SetHandTookFrom(owner != nullptr ? owner->owner : PlayerNames::NEUTRAL);
}

void GameHandGrabWorld::SetHandTookFrom(PlayerNames from)
{
	const auto hand = HandPlayer();
	Entities().Each<Player>([hand, from](entt::entity, Player& player) {
		if (player.name == hand)
		{
			player.handTookFrom = from;
		}
	});
}

void GameHandGrabWorld::TakeFromField(entt::entity field, uint32_t amount)
{
	if (!Locator::entitiesRegistry::value().AllOf<Field>(field) || !Locator::fieldSystem::has_value())
	{
		return;
	}
	// The field system puts out an emptied field's fire and has its town look again at what it wants
	Locator::fieldSystem::value().RemoveFood(field, static_cast<float>(amount));
}

entt::entity GameHandGrabWorld::MakeHandful(PotInfo type, glm::vec3 position, uint32_t amount, bool poisoned)
{
	const auto handful = archetypes::PotArchetype::Create(position, 0.0f, type, static_cast<int32_t>(amount));
	if (auto* pot = Locator::entitiesRegistry::value().TryGet<Pot>(handful))
	{
		pot->poisoned = poisoned;
	}
	ResizePot(handful);
	return handful;
}

void GameHandGrabWorld::ResizePot(entt::entity pot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.TryGet<const Pot>(pot);
	auto* transform = registry.TryGet<Transform>(pot);
	const auto* info = Info();
	if (data == nullptr || transform == nullptr || info == nullptr)
	{
		return;
	}
	const auto& kind = info->pot.at(static_cast<size_t>(data->type));
	// A pile is raised out of the ground by what it holds instead
	if (kind.potType != PotType::Pot)
	{
		return;
	}
	transform->scale = glm::vec3(magic::piles::PotScale(data->amount, kind.scaleEvery));
}

std::optional<uint32_t> GameHandGrabWorld::StartScoopStream(ResourceType resource, glm::vec3 source, bool poisoned)
{
	if (!Locator::particleSystem::has_value())
	{
		return std::nullopt;
	}
	// Poisoned food streams as its own
	const auto stream = resource == ResourceType::Wood ? k_ScoopWood : poisoned ? k_ScoopPoisonedFood : k_ScoopFood;
	return Locator::particleSystem::value().Start(stream, source, 1.0f);
}

std::optional<uint32_t> GameHandGrabWorld::StartFishScoopStream(glm::vec3 source)
{
	if (!Locator::particleSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::particleSystem::value().Start(ParticleType::FoodPickupFish, source, 1.0f);
}

void GameHandGrabWorld::StopScoopStream(uint32_t stream)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().CloseDown(stream);
	}
}

void GameHandGrabWorld::PinCursor(bool pinned)
{
	if (Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().PinCursor(pinned);
	}
}

void GameHandGrabWorld::MoveScoopStream(uint32_t stream, glm::vec3 hand)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().SetOrigin(stream, hand);
	}
}

entt::entity GameHandGrabWorld::PlayScoopSound(entt::entity playing, ResourceType resource, glm::vec3 hand, float ramp)
{
	if (!Locator::audio::has_value())
	{
		return entt::null;
	}
	auto& audio = Locator::audio::value();
	// Its pitch rises as the scoop ramps up
	const auto pitch = hand_grab::ScoopSoundPitch(ramp);
	// Already playing, it goes on where it started and only follows the ramp
	if (playing != entt::null && audio.EmitterExists(playing))
	{
		audio.SetEmitterPitch(playing, pitch);
		return playing;
	}
	// Wood rattles in, anything else pours
	const auto sound = static_cast<entt::id_type>(resource == ResourceType::Wood ? k_ScoopWoodSound : k_ScoopSound);
	return audio::StartGameSoundEffect(sound, {.position = hand, .pitchPercent = pitch, .playType = audio::PlayType::Repeat});
}

void GameHandGrabWorld::StopScoopSound()
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	audio.StopSoundEffect(static_cast<entt::id_type>(k_ScoopSound));
	audio.StopSoundEffect(static_cast<entt::id_type>(k_ScoopWoodSound));
}

float GameHandGrabWorld::LandHeightAt(glm::vec3 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({point.x, point.z}) : 0.0f;
}

bool GameHandGrabWorld::StoresResource(entt::entity store, ResourceType resource) const
{
	return Locator::resourceStoreSystem::has_value() && Locator::resourceStoreSystem::value().IsStore(store, resource);
}

uint32_t GameHandGrabWorld::AddToStore(entt::entity store, ResourceType resource, uint32_t amount, bool poisoned)
{
	if (!Locator::resourceStoreSystem::has_value())
	{
		return 0;
	}
	auto& stores = Locator::resourceStoreSystem::value();
	const auto added = stores.AddToStore(store, resource, amount, HandPlayer(), poisoned, PoseOf(Hand()).origin);
	// The player's creature may see the gift again, as the hand gave the whole pot
	if (added != 0)
	{
		stores.ReportGift(store, resource, HandPlayer());
	}
	return added;
}

bool GameHandGrabWorld::TakeIntoStore(entt::entity store, entt::entity object)
{
	// An animal, a static such as a fence, or a tree a script made indestructible refuses to be given
	if (Entities().AllOf<Indestructible>(object) && Entities().AnyOf<Animal, MobileStatic, Tree>(object))
	{
		return false;
	}
	return Locator::resourceStoreSystem::has_value() &&
	       Locator::resourceStoreSystem::value().TakeObject(store, object, HandPlayer());
}

bool GameHandGrabWorld::TakesWhole(entt::entity target, entt::entity held) const
{
	if (altar_offering::Takes(target, held))
	{
		return true;
	}
	if (Locator::animatedStaticSystem::has_value() && Locator::animatedStaticSystem::value().TakesGateStone(target, held))
	{
		return true;
	}
	// A store takes what it is worth, but not an animal, a static or a tree that a script made indestructible
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Indestructible>(held) && registry.AnyOf<Animal, MobileStatic, Tree>(held))
	{
		return false;
	}
	if (!Locator::resourceStoreSystem::has_value())
	{
		return false;
	}
	const auto& stores = Locator::resourceStoreSystem::value();
	const auto resource = stores.ResourceOf(held);
	return resource.type != ResourceType::None && stores.IsStore(target, resource.type);
}

bool GameHandGrabWorld::LayGateStone(entt::entity plinth, entt::entity stone)
{
	return Locator::animatedStaticSystem::has_value() && Locator::animatedStaticSystem::value().LayGateStone(plinth, stone);
}

void GameHandGrabWorld::PourAt(ResourceType resource, glm::vec3 point, uint32_t amount, PlayerNames player, bool poisoned)
{
	if (Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().PourAt(resource, point, amount, false, player, poisoned);
	}
}

void GameHandGrabWorld::UseUp(entt::entity object)
{
	world_objects::LeaveGhost(object);
	world_objects::Remove(object);
}

altar_offering::Answer GameHandGrabWorld::OfferAtAltar(entt::entity altar, entt::entity held)
{
	return altar_offering::Offer(altar, held,
	                             {
	                                 .player = HandPlayer(),
	                                 .hand = ReportedHandPlace(),
	                                 .pointed = PickedPoint(),
	                                 .local = true,
	                             });
}

void GameHandGrabWorld::HelpEvent(uint32_t event)
{
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(event);
	}
}

FromHandResult GameHandGrabWorld::LetGoFromHand(entt::entity object, const FromHand& release)
{
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().LetGoFromHand(object, release)
	                                            : FromHandResult {};
}

bool GameHandGrabWorld::PlayerHasNoWindResistance(PlayerNames player) const
{
	if (!Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto entity = Locator::playerSystem::value().GetPlayer(player);
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Player>(entity);
	return data != nullptr && data->windResistance != 0;
}

std::optional<hand_grab::HandGrabWorldInterface::BodyFacts> GameHandGrabWorld::BodyOf(entt::entity object) const
{
	const auto* entry = EntryOf(object);
	if (entry == nullptr || entry->body == nullptr)
	{
		return std::nullopt;
	}
	return BodyFacts {.mass = entry->body->Mass(), .speed = x87::LengthZyx(entry->body->velocity)};
}

void GameHandGrabWorld::TwistBody(entt::entity object, glm::vec3 torque)
{
	if (auto* entry = EntryOf(object); entry != nullptr && entry->body != nullptr)
	{
		entry->body->externalTorque += torque;
	}
}

void GameHandGrabWorld::DropDrag(entt::entity object)
{
	if (auto* entry = EntryOf(object); entry != nullptr && entry->body != nullptr)
	{
		entry->body->SetDrag(0.0f);
	}
}
