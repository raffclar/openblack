/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureHandGlow.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptPopulate.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"

namespace openblack::chlapi
{

using openblack::Locator;
using openblack::ecs::components::Transform;
using openblack::script::ObjectType;

/// A native that takes control gives each member of a container that it goes through to the script, before it does
/// its work on the member. The member stays under the script's control until a script releases it.
void TakeControlInLoop(ecs::Registry& registry, entt::entity member)
{
	if (registry.Valid(member))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(member);
	}
}

/// The villagers of a town in the order the scripts look through them: each building's people, the newest building
/// first and each building's newest person first, then the homeless, the newest first
static std::vector<entt::entity> TownVillagersInOrder(const ecs::Registry& registry, entt::entity town)
{
	std::vector<entt::entity> villagers;
	const auto* data = registry.TryGet<const ecs::components::Town>(town);
	if (data == nullptr)
	{
		return villagers;
	}
	for (const auto abode : data->abodes)
	{
		if (const auto* building = registry.Valid(abode) ? registry.TryGet<const ecs::components::Abode>(abode) : nullptr)
		{
			villagers.insert(villagers.end(), building->inhabitants.begin(), building->inhabitants.end());
		}
	}
	villagers.insert(villagers.end(), data->homelessVillagers.begin(), data->homelessVillagers.end());
	return villagers;
}

/// Whether a container a script can look in: a town, a flock or a dance
bool IsScriptContainer(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && (registry.AllOf<ecs::components::Town>(thing) ||
	                                 ecs::script_flocks::IsFlock(registry, thing) || ecs::dances::IsDance(registry, thing));
}

/// The things in a script container, in the order the scripts look through them
std::vector<entt::entity> ContainerMembers(const ecs::Registry& registry, entt::entity container)
{
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		return registry.Get<const ecs::components::Flock>(container).members;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		return TownVillagersInOrder(registry, container);
	}
	if (ecs::dances::IsDance(registry, container))
	{
		return ecs::dances::Dancers(registry, container);
	}
	return {};
}

/// A villager's town: it leaves its old town's homeless, then joins the new town
static void MoveVillagerToTown(entt::entity villager, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& person = registry.Get<ecs::components::Villager>(villager);
	if (auto* old = registry.Valid(person.town) ? registry.TryGet<ecs::components::Town>(person.town) : nullptr)
	{
		std::erase(old->homelessVillagers, villager);
	}
	static_cast<void>(Locator::townSystem::value().AddVillagerToTown(town, villager));
}

/// A living a script puts in a flock keeps up with it from then on
static void SetFlockState(entt::entity living)
{
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::MoveInFlock);
	}
	else if (Locator::animalSystem::has_value() && Locator::entitiesRegistry::value().AllOf<ecs::components::Animal>(living))
	{
		Locator::animalSystem::value().SetScriptState(living, LivingStates::LivingMoveInFlock);
	}
}

/// Whether a thing is a living a flock can take: a villager or an animal
static bool IsFlockLiving(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(thing);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	// Nobody owns a script's flock: it belongs to the neutral player
	const auto flock = ecs::script_flocks::Create(registry, map_coords::FromMetres({position.x, position.z}));
	RegisterCreated(flock);
	Pusho(static_cast<uint32_t>(flock));
}

/// Puts a living, or each member of a flock, into a flock
static void FlockAttachToFlock(entt::entity object, entt::entity flock, bool asLeader)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (ecs::script_flocks::IsFlock(registry, object))
	{
		// The flock takes in every member of the other, which goes once it is empty
		while (ecs::script_flocks::Size(registry, object) > 0)
		{
			ecs::script_flocks::AddLiving(registry, flock, registry.Get<const ecs::components::Flock>(object).members.front());
		}
		Pusho(static_cast<uint32_t>(flock));
		return;
	}
	if (!IsFlockLiving(registry, object))
	{
		ScriptMessage("JONTY-Trying to add non living to flock");
		ScriptMessage("Thing not added to Flock");
		Pusho(0);
		return;
	}
	if (asLeader)
	{
		ecs::script_flocks::AddLeader(registry, flock, object);
	}
	else if (!ecs::script_flocks::AddLiving(registry, flock, object))
	{
		ScriptMessage("Living already in Flock");
	}
	SetFlockState(object);
	Pusho(static_cast<uint32_t>(flock));
	scripts.AddReference(object);
}

/// A villager, or each villager of a container, joins a town
static void FlockAttachToTown(entt::entity object, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (registry.Valid(member) && registry.AllOf<ecs::components::Villager>(member))
			{
				MoveVillagerToTown(member, town);
				continue;
			}
			// A member that is not a villager stays where it is. It puts no value on the script's stack (deviation 178).
			ScriptMessage("JONTY-Trying to add non villager to town");
		}
		Pusho(static_cast<uint32_t>(town));
		return;
	}
	if (!registry.AllOf<ecs::components::Villager>(object))
	{
		// TODO(opening): a spell dispenser given to a town is the town's from then on
		ScriptMessage("JONTY-Trying to add non villager to town");
		Pusho(0);
		return;
	}
	// It stays in any flock it is in, doing what it was doing
	MoveVillagerToTown(object, town);
	Pusho(static_cast<uint32_t>(town));
}

/// A living joins a dance, dancing in the group whose turn it is that takes it, or standing in the dance in none
static void JoinDance(entt::entity living, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (asCentre)
	{
		registry.Get<ecs::components::Dance>(dance).owner = living;
	}
	// One already dancing leaves its dance first
	ecs::dances::RemoveDancer(registry, living);
	const auto sex = registry.AllOf<ecs::components::Villager>(living) ? ecs::villager_dance::DanceSex(living)
	                                                                   : ecs::components::DanceGroup::k_AnySex;
	static_cast<void>(ecs::dances::AddDancer(registry, dance, living, sex));
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::InDance);
	}
}

/// A living, or each villager of a container, joins a dance
static void FlockAttachToDance(entt::entity object, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsFlockLiving(registry, member))
			{
				JoinDance(member, dance, asCentre);
				scripts.AddReference(member);
			}
		}
		Pusho(static_cast<uint32_t>(dance));
		return;
	}
	if (!IsFlockLiving(registry, object))
	{
		ScriptMessage("JONTY-Trying to add non living to dance");
		Pusho(0);
		return;
	}
	JoinDance(object, dance, asCentre);
	scripts.AddReference(object);
	Pusho(static_cast<uint32_t>(dance));
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	const auto asLeader = Pop().intVal != 0;
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Id deleted to attach");
		Pusho(0);
		return;
	}
	if (target == entt::null || !registry.Valid(target))
	{
		ScriptMessage("Id deleted to attach to");
		Pusho(0);
		return;
	}
	// The type of the target sets the operation: first a dance, then a flock, then a town
	if (ecs::dances::IsDance(registry, target))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(target);
		FlockAttachToDance(object, target, asLeader);
		return;
	}
	if (ecs::script_flocks::IsFlock(registry, target))
	{
		FlockAttachToFlock(object, target, asLeader);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(target))
	{
		FlockAttachToTown(object, target);
		return;
	}
	ScriptMessage("Thing not added to id");
	Pusho(0);
}

/// A living leaves a flock, keeping its state, the flock staying when it is left empty
/// A member that a script takes out of a flock: an animal goes into a new flock of its own, anything else is in no flock
/// after it. The flock stays, also when it is empty.
static void LeaveFlockForScript(ecs::Registry& registry, entt::entity member)
{
	if (registry.AllOf<ecs::components::Animal>(member))
	{
		ecs::script_flocks::SeparateIntoNewFlock(registry, member);
		return;
	}
	ecs::script_flocks::Remove(registry, member, false);
}

static void DetachFromFlock(uint32_t objectId, entt::entity object, entt::entity flock)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from flock");
			ScriptMessage("Thing not removed from Flock");
			Pusho(0);
			return;
		}
		LeaveFlockForScript(registry, object);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	// A member at random, not the leader when there are others
	const auto member = ecs::script_flocks::MemberToDetach(registry, flock,
	                                                       [](uint32_t n) { return Locator::gameRandom::value().GameRand(n); });
	if (member == entt::null)
	{
		ScriptMessage("Thing not found from Flock");
		ScriptMessage("Thing not removed from Flock");
		Pusho(0);
		return;
	}
	LeaveFlockForScript(registry, member);
	scripts.Register(member, false);
	scripts.RemoveReference(member);
	Pusho(static_cast<uint32_t>(member));
}

/// A dancer leaves a dance: the one given, or with none given the first of its groups', passing over what the dance is
/// danced about while others dance
static void DetachFromDance(uint32_t objectId, entt::entity object, entt::entity dance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from dance");
			ScriptMessage("Thing not removed from Dance");
			Pusho(0);
			return;
		}
		ecs::dances::RemoveDancer(registry, object);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	const auto& data = registry.Get<const ecs::components::Dance>(dance);
	const auto exclude = data.dancers == 1 ? entt::null : data.owner;
	const auto dancer = ecs::dances::FirstDancer(registry, dance, exclude);
	if (dancer == entt::null)
	{
		ScriptMessage("Thing not removed from Dance");
		Pusho(0);
		return;
	}
	ecs::dances::RemoveDancer(registry, dancer);
	scripts.Register(dancer, false);
	Pusho(static_cast<uint32_t>(dancer));
	scripts.RemoveReference(dancer);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	const auto container = PopObject();
	const auto objectId = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		// The game pushes nothing here
		ScriptMessage("From thing dead!");
		return;
	}
	const auto object = objectId != 0 ? Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(objectId)) : entt::null;
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		DetachFromFlock(objectId, object, container);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		const auto* person = registry.Valid(object) ? registry.TryGet<const ecs::components::Villager>(object) : nullptr;
		if (person != nullptr && person->town == container)
		{
			// The villager leaves the town altogether, and the script is given nothing back
			Locator::townSystem::value().RemoveVillager(object);
			Pusho(0);
			return;
		}
		if (person != nullptr)
		{
			ScriptMessage("Wrong Town");
		}
		ScriptMessage("Thing not removed from Town");
		Pusho(0);
		return;
	}
	if (IsFlockLiving(registry, container))
	{
		const auto flock = ecs::script_flocks::FlockOf(registry, container);
		if (flock == entt::null)
		{
			ScriptMessage("No flock for living so what the...");
			Pusho(0);
			return;
		}
		DetachFromFlock(objectId, object, flock);
		return;
	}
	if (ecs::dances::IsDance(registry, container))
	{
		DetachFromDance(objectId, object, container);
		return;
	}
	ScriptMessage("Not living - confused");
	Pusho(0);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ScriptMessage("Disbanding a NULL object");
		ScriptMessage("Bad Id for Disband");
		return;
	}
	// Every member of a flock or dance leaves it, and one the script controls waits for it; the flock or dance stays,
	// empty. A town or an abode keeps its own
	if (Locator::scriptObjects::value().Disband(container) || registry.AllOf<ecs::components::Abode>(container))
	{
		return;
	}
	ScriptMessage("Bad Id for Disband");
}

void IdSize() // 040 ID_SIZE
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container != entt::null && ecs::script_flocks::IsFlock(registry, container))
	{
		Pushf(static_cast<float>(ecs::script_flocks::Size(registry, container)));
		return;
	}
	if (container != entt::null && registry.Valid(container) && registry.AllOf<ecs::components::Town>(container))
	{
		// Its grown-ups and its children
		uint32_t people = 0;
		registry.Each<const ecs::components::Villager>(
		    [&people, container](const ecs::components::Villager& person) { people += person.town == container ? 1 : 0; });
		Pushf(static_cast<float>(people));
		return;
	}
	if (container != entt::null && ecs::dances::IsDance(registry, container))
	{
		Pushf(static_cast<float>(ecs::dances::Size(registry, container)));
		return;
	}
	// TODO(opening): footballs
	ScriptMessage("Cannot Find Flock/Dance/Town Size");
	Pushf(0.0f);
}

void FlockMember() // 041 FLOCK_MEMBER
{
	const auto flock = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::script_flocks::IsFlock(registry, flock) || !IsFlockLiving(registry, object))
	{
		ScriptMessage("Invalid Flock or Thing");
		Pushb(false);
		return;
	}
	Pushb(ecs::script_flocks::IsMember(registry, flock, object));
}

void DanceCreate() // 054 DANCE_CREATE
{
	const auto duration = Popf();
	const auto position = PopVec();
	const auto type = Pop().intVal;
	const auto centre = PopObject();
	// What it is danced about may be nothing
	const auto dance = Locator::danceSystem::value().Create(
	    static_cast<DanceInfo>(type), position, centre, static_cast<uint32_t>(map_coords::FtoL(duration)), true, entt::null);
	if (dance == entt::null)
	{
		ScriptMessage("Dance not created");
		Pusho(0);
		return;
	}
	RegisterCreated(dance);
	Pusho(static_cast<uint32_t>(dance));
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	namespace populate = ecs::script_populate;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	const auto count = populate::CountOf(Popf());
	const auto container = PopObject();
	if (!populate::IsValidType(type))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		// The game leaves a value on the stack here although the statement gives none back
		Pusho(0);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(container) || container == static_cast<entt::entity>(0))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Container not valid");
		return;
	}
	// About the container's place: openblack's containers that can be filled are flocks, at their home on the land
	glm::vec3 centre(0.0f);
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(container))
	{
		const auto place = map_coords::ToMetres(flock->place);
		centre = {place.x, Locator::terrainSystem::value().GetHeightAt(place), place.y};
	}
	else if (const auto* transform = registry.TryGet<const Transform>(container))
	{
		centre = transform->position;
	}
	const float spread = populate::SpreadOf(count);
	const auto floatRand = [](float x) { return Locator::gameRandom::value().GameFloatRand(x); };
	auto& scriptObjects = Locator::scriptObjects::value();
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto place = populate::PlaceOf(centre, spread, floatRand);
		const auto thing = CreateScriptObject(static_cast<ObjectType>(type), static_cast<uint32_t>(subtype), place, 0.0f, 0.0f,
		                                      0.0f, 0.0f, 1.0f);
		if (thing == static_cast<entt::entity>(0) || !registry.Valid(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not create thing for populate");
			return;
		}
		if (!registry.AnyOf<ecs::components::Animal, ecs::components::Villager, ecs::components::Creature>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Created Thing Not Living");
			registry.Destroy(thing);
			return;
		}
		RegisterCreated(thing);
		// A flock takes it as its newest member and it goes about with it
		if (!registry.AllOf<ecs::components::Flock>(container) || !IsFlockLiving(registry, thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not implemented - Thing not added to id");
			return;
		}
		ecs::script_flocks::AddLiving(registry, container, thing);
		SetFlockState(thing);
		scriptObjects.AddReference(thing);
	}
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// The flock of a living thing, none when it is in no flock
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !IsLivingThing(object))
	{
		ScriptMessage("Cannot find flock");
		Pusho(0);
		return;
	}
	const auto flock = ecs::script_flocks::FlockOf(registry, object);
	if (flock == entt::null || !registry.Valid(flock))
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(flock, false);
	Pusho(static_cast<uint32_t>(flock));
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// Whether each member of a flock is within the flock's distance of the flock. A thing that is not a flock gives true.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing no longer valid");
		Pushb(true);
		return;
	}
	if (!ecs::script_flocks::IsFlock(registry, object))
	{
		ScriptMessage("Invalid thing");
		Pushb(true);
		return;
	}
	Pushb(ecs::script_flocks::AllMembersWithinFlockDistance(registry, object));
}

/// Whether a thing is a building of the buildings' table for the adult counts of the scripts: an abode or a miracle
/// dispenser
static bool IsAdultHolderBuilding(const ecs::Registry& registry, entt::entity object)
{
	return registry.AnyOf<ecs::components::Abode, ecs::components::SpellDispenser>(object);
}

/// The grown villagers of a town now
static uint32_t TownAdultsNow(const ecs::Registry& registry, entt::entity town)
{
	uint32_t adults = 0;
	registry.Each<const ecs::components::Villager>([town, &adults](const ecs::components::Villager& villager) {
		if (villager.town == town && villager.lifeStage != ecs::components::Villager::LifeStage::Child)
		{
			++adults;
		}
	});
	return adults;
}

/// The room for adults and for children in the built buildings of a town
struct TownRoom
{
	int32_t adults {0};
	int32_t children {0};
};

/// The room in the built buildings of a town now: the places of each building of its tribe's table that has a place
/// for anyone, as the town stats count them
static TownRoom TownRoomNow(const ecs::Registry& registry, entt::entity townEntity, const ecs::components::Town& town)
{
	const auto* tribe = registry.TryGet<const Tribe>(townEntity);
	if (tribe == nullptr || !Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& rows = Locator::infoConstants::value().abode;
	TownRoom room;
	registry.Each<const ecs::components::Abode>([&](entt::entity entity, const ecs::components::Abode& abode) {
		if (abode.townId != town.id)
		{
			return;
		}
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(entity);
		if (progress != nullptr && progress->built < 1.0f)
		{
			return;
		}
		const auto row = std::ranges::find_if(
		    rows, [&](const GAbodeInfo& info) { return info.tribeType == *tribe && info.abodeNumber == abode.type; });
		if (row != rows.end() && row->maxVillagersInAbode + row->maxChildrenInAbode != 0)
		{
			room.adults += static_cast<int32_t>(row->maxVillagersInAbode);
			room.children += static_cast<int32_t>(row->maxChildrenInAbode);
		}
	});
	return room;
}

/// The adults of a town or a building, or the room for them, as ID_ADULT_SIZE and OBJECT_ADULT_CAPACITY give it. Any
/// other thing gives 0 and a message.
static void PushAdultCount(bool room)
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Object should be alive!");
		Pushf(0.0f);
		return;
	}
	if (const auto* town = registry.TryGet<const ecs::components::Town>(object); town != nullptr)
	{
		// The town keeps these counts up to date at all times, so they are counted now and not taken from the turn's
		// town stats
		Pushf(room ? ecs::script_containers::TownAdultRoom(TownRoomNow(registry, object, *town).adults)
		           : ecs::script_containers::TownAdults(TownAdultsNow(registry, object)));
		return;
	}
	if (!IsAdultHolderBuilding(registry, object))
	{
		ScriptMessage("Object should be town, or abode");
		Pushf(0.0f);
		return;
	}
	if (room)
	{
		const auto* info = ecs::world_objects::AbodeInfoOf(object);
		Pushf(ecs::script_containers::AbodeAdultRoom(info != nullptr ? info->maxVillagersInAbode : 0));
		return;
	}
	const bool lived = registry.AllOf<ecs::components::Abode>(object);
	Pushf(ecs::script_containers::AbodeAdults(lived ? ecs::town_homes::HouseholdOf(registry, object).adults : 0));
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	PushAdultCount(false);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// The room for adults and children of a town or of a building. Any other thing gives 0 and a message.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Object should be alive!");
		Pushf(0.0f);
		return;
	}
	if (const auto* town = registry.TryGet<const ecs::components::Town>(object); town != nullptr)
	{
		const auto room = TownRoomNow(registry, object, *town);
		Pushf(ecs::script_containers::TownCapacity(room.adults, room.children));
		return;
	}
	if (!IsAdultHolderBuilding(registry, object))
	{
		ScriptMessage("Object should be town, or abode");
		Pushf(0.0f);
		return;
	}
	const auto* info = ecs::world_objects::AbodeInfoOf(object);
	Pushf(info != nullptr ? ecs::script_containers::AbodeCapacity(info->maxVillagersInAbode, info->maxChildrenInAbode) : 0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	PushAdultCount(true);
}

/// The first thing in a town, a flock or a dance for a script, or nothing
static entt::entity FirstInContainer(const ecs::Registry& registry, entt::entity container)
{
	if (const auto* town = registry.TryGet<const ecs::components::Town>(container); town != nullptr)
	{
		const auto* building = !town->abodes.empty() && registry.Valid(town->abodes.front())
		                           ? registry.TryGet<const ecs::components::Abode>(town->abodes.front())
		                           : nullptr;
		return ecs::script_containers::FirstInTown(building != nullptr ? std::span<const entt::entity>(building->inhabitants)
		                                                               : std::span<const entt::entity> {},
		                                           town->homelessVillagers);
	}
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		const auto& members = registry.Get<const ecs::components::Flock>(container).members;
		return members.empty() ? entt::entity {entt::null} : members.front();
	}
	if (ecs::dances::IsDance(registry, container))
	{
		return ecs::dance_rules::FirstForScript(registry.Get<const ecs::components::Dance>(container).groups);
	}
	return entt::null;
}

/// Pushes a thing that a container gave, or nothing
static void PushContainerThing(entt::entity thing)
{
	if (thing == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(thing, false);
	Pusho(static_cast<uint32_t>(thing));
}

/// Takes the container from the stack and pushes its first thing
static void PushFirstInContainer()
{
	const auto container = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !IsScriptContainer(registry, container))
	{
		ScriptMessage("Invalid container");
		Pusho(0);
		return;
	}
	PushContainerThing(FirstInContainer(registry, container));
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	PushFirstInContainer();
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// With no thing to start after, or one that is gone, the script gets the first thing of the container
	const auto after = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (after == entt::null || !registry.Valid(after))
	{
		PushFirstInContainer();
		return;
	}
	const auto container = PopObject();
	if (container == entt::null || !IsScriptContainer(registry, container))
	{
		ScriptMessage("Invalid container");
		Pusho(0);
		return;
	}
	if (ecs::dances::IsDance(registry, container))
	{
		// The game walks a list of the dance's things that nothing fills while the game runs, so a script never gets
		// a next thing in a dance
		Pusho(0);
		return;
	}
	PushContainerThing(ecs::script_containers::NextAfter(ContainerMembers(registry, container), after));
}

} // namespace openblack::chlapi
