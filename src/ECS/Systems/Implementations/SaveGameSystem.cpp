/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SaveGameSystem.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <CreatureRecord.h>
#include <LHVM.h>
#include <MindFile.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <SavedObjects.h>
#include <VanillaSaveFile.h>
#include <WorldFile.h>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/PlayerProfiles.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureSaveState.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCreedPowers.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTarget.h"
#include "ECS/Components/CreatureVanillaRecord.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace save_state = openblack::creature_save_state;

namespace
{
constexpr float k_TurnsPerSecond = 10.0f;
constexpr std::string_view k_CurseScript = "CreatureCurse";

/// A table from the keys of one stream to the keys of another
using ObjectRefKeys = std::unordered_map<gamesave::ObjectKey, gamesave::ObjectKey>;

std::shared_ptr<spdlog::logger> Log()
{
	return spdlog::get("game");
}

/// The folder of the current player, relative to the profile folder. A game with no player profile uses the
/// developer's folder.
std::optional<std::filesystem::path> PlayerFolder()
{
	if (!Locator::profileFolderSystem::has_value())
	{
		return std::nullopt;
	}
	auto name = gui::ToUtf16(player_profiles::k_DeveloperName);
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			name = gui::ToUtf16(profile->name);
		}
	}
	const auto& folders = Locator::profileFolderSystem::value();
	const auto root = folders.GetFolder();
	const auto player = folders.GetPlayerFolder(name);
	if (!root.has_value() || !player.has_value())
	{
		return std::nullopt;
	}
	return player->lexically_relative(*root);
}

bool WriteFile(const std::filesystem::path& relativeFolder, std::string_view file, std::span<const uint8_t> bytes)
{
	const auto folder = Locator::profileFolderSystem::value().MakeFolder(relativeFolder);
	if (!folder.has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	const auto written = Locator::filesystem::value().WriteAll(*folder / file, bytes);
	if (!written)
	{
		SPDLOG_LOGGER_WARN(Log(), "The game could not write {}", (*folder / file).generic_string());
	}
	return written;
}

std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path)
{
	if (!Locator::filesystem::has_value() || !Locator::filesystem::value().Exists(path))
	{
		return std::nullopt;
	}
	try
	{
		return Locator::filesystem::value().ReadAll(path);
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(Log(), "The game could not read {}: {}", path.generic_string(), error.what());
		return std::nullopt;
	}
}

uint32_t LandNumber()
{
	return static_cast<uint32_t>(std::max(Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 0));
}

PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// The date and the time now, the time first, as the save game room shows them. On Windows the player's own formats of
/// the time and of the long date give the text, as in the game.
std::u16string DateNow()
{
#ifdef _WIN32
	std::array<wchar_t, 128> time {};
	std::array<wchar_t, 128> date {};
	const auto timeLength =
	    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, time.data(), static_cast<int>(time.size()));
	const auto dateLength =
	    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, nullptr, nullptr, date.data(), static_cast<int>(date.size()));
	if (timeLength > 0 && dateLength > 0)
	{
		std::u16string text(time.data(), time.data() + timeLength - 1);
		text += u' ';
		text.append(date.data(), date.data() + dateLength - 1);
		return text;
	}
#endif
	const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	try
	{
		const std::chrono::zoned_time local {std::chrono::current_zone(), now};
		return gui::ToUtf16(std::format("{:%H:%M:%S %A, %d %B %Y}", local));
	}
	catch (const std::exception&)
	{
		return gui::ToUtf16(std::format("{:%H:%M:%S %A, %d %B %Y}", now));
	}
}

/// Writes a mind file of the saved game through the mind file component
bool WriteMind(const std::filesystem::path& relativeFolder, std::string_view file, const creaturemind::MindFileData& mind)
{
	const auto folder = Locator::profileFolderSystem::value().MakeFolder(relativeFolder);
	if (!folder.has_value())
	{
		return false;
	}
	const auto result = creaturemind::WriteFile(*folder / file, mind);
	if (result != creaturemind::MindResult::Success)
	{
		SPDLOG_LOGGER_WARN(Log(), "The game could not write the mind file {}", (*folder / file).generic_string());
		return false;
	}
	return true;
}

/// The heading of a thing that its turn about the vertical gives
float HeadingOf(const glm::mat3& rotation)
{
	return std::atan2(rotation[2][0], rotation[2][2]);
}

/// The creatures in the order that the game made them, which names their mind files
std::vector<entt::entity> CreaturesInOrder()
{
	std::vector<std::pair<uint64_t, entt::entity>> found;
	Locator::entitiesRegistry::value().Each<const Creature>(
	    [&found](entt::entity entity, const Creature& creature) { found.emplace_back(creature.made, entity); });
	std::ranges::sort(found);
	std::vector<entt::entity> creatures;
	creatures.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(creatures), [](const auto& entry) { return entry.second; });
	return creatures;
}

/// A world point as the game's map position. The altitude is the height above the ground.
gamesave::MapCoords MapPosition(const glm::vec3& point)
{
	map_coords::MapCoords coords {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), 0.0f};
	if (Locator::terrainSystem::has_value())
	{
		coords = map_coords::FromWorld(Locator::terrainSystem::value(), point);
	}
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// The game's map position as a world point, on the ground plus the altitude
glm::vec3 WorldPosition(const gamesave::MapCoords& position)
{
	const map_coords::MapCoords coords {position.x, position.z, position.altitude};
	if (Locator::terrainSystem::has_value())
	{
		return map_coords::ToWorld(Locator::terrainSystem::value(), coords);
	}
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

/// The creature's alignment as the save writes it: while the land's curse script runs on the local player's creature,
/// the alignment that the curse found
float AlignmentToSave(const Creature& body)
{
	if (body.owner != LocalPlayer() || !Locator::vm::has_value())
	{
		return body.alignment;
	}
	const auto& vm = Locator::vm::value();
	const bool cursed =
	    std::ranges::any_of(vm.GetTasks(), [](const auto& entry) { return entry.second.name == k_CurseScript; });
	if (!cursed)
	{
		return body.alignment;
	}
	const auto& variables = vm.GetVariables();
	const auto found = std::ranges::find(variables, std::string_view {"OriginalAlignmentOfMyCreature"}, &lhvm::VMVar::name);
	return found != variables.end() ? found->value.floatVal : 0.0f;
}

/// The whole record of the creature as openblack writes it from the creature now, its player's leash on it, and the
/// values that openblack keeps beside the record
gamesave::SavedCreature Capture(ecs::Registry& registry, entt::entity entity, uint32_t index)
{
	gamesave::SavedCreature saved {.key = save_state::ToRef(entity).key};
	const auto& body = registry.Get<const Creature>(entity);
	auto& object = saved.creature;
	if (const auto* transform = registry.TryGet<const Transform>(entity))
	{
		object.position.position = MapPosition(transform->position);
		saved.heading = HeadingOf(transform->rotation);
	}
	object.position.flags = save_state::k_CreaturePositionFlags;
	object.object.info = save_state::CreatureInfo(body.species).value_or(0);
	object.object.coords = object.position.position;
	object.object.scale = body.size;
	object.move.speed = save_state::k_CreatureSpeedWord;

	auto& record = object.creature;
	record.savedInTemple = Locator::temple::has_value() && Locator::temple::value().Active() ? 1 : 0;
	record.index = static_cast<uint8_t>(std::min<uint32_t>(index, gamesave::k_None));
	record.owner = save_state::PlayerNumber(body.owner);
	auto* spells = registry.TryGet<CreatureSpells>(entity);
	if (spells != nullptr)
	{
		for (uint32_t place = 0; place < record.spells.size(); ++place)
		{
			record.spells.at(place) = save_state::ToSlot(spells->spells.slots.at(place), place);
		}
		std::ranges::transform(spells->spells.waiting, std::back_inserter(record.queued), save_state::ToQueued);
		record.mindProcessing = spells->pausedMind ? 0 : 1;
		record.largestSize = spells->largestSize;
		record.smallestSize = spells->smallestSize;
		record.freeze = spells->freeze;
	}
	record.mayGoThroughVortex = body.mayGoThroughVortex ? 1 : 0;
	// The save writes the fizz only while the invisibility spell is on the creature. Else it puts the creature in full
	// sight at once, as it writes it.
	auto* fizz = registry.TryGet<CreatureFizz>(entity);
	creature_fizz::Fizz now = fizz != nullptr ? fizz->fizz : creature_fizz::Fizz {};
	if (fizz == nullptr && spells != nullptr)
	{
		now.now = spells->fizz;
	}
	const auto fizzSaved = spells != nullptr ? save_state::FizzToSave(now, spells->spells) : save_state::FizzToSave(now, {});
	if (fizzSaved != now)
	{
		if (fizz != nullptr)
		{
			fizz->fizz = fizzSaved;
		}
		if (spells != nullptr)
		{
			spells->fizz = fizzSaved.now;
		}
	}
	record.fizzedForGood = fizzSaved.goesForGood ? 1 : 0;
	record.fizz = fizzSaved.now;
	record.fizzTarget = fizzSaved.target;
	record.fizzPerSecond = fizzSaved.perSecond;
	if (const auto* target = registry.TryGet<const CreatureTarget>(entity))
	{
		record.targetPoint = {target->point.x, target->point.y, target->point.z};
		record.targetObject = save_state::ToRef(target->object);
		record.hasTarget = target->has ? 1 : 0;
	}
	const auto* leash = registry.TryGet<const CreatureLeash>(entity);
	if (leash != nullptr && leash->home.has_value())
	{
		record.home = MapPosition(*leash->home);
	}
	if (const auto* fight = registry.TryGet<const CreatureFightRecord>(entity))
	{
		record.fightHealth = fight->health;
		record.autoFight = static_cast<uint32_t>(fight->control);
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(entity))
	{
		record.arena = save_state::ToRef(fighting->arenaEntity);
	}
	const auto* dance = registry.TryGet<const CreatureDance>(entity);
	if (dance != nullptr)
	{
		std::ranges::transform(dance->dancers, std::back_inserter(record.dancePartners),
		                       [](entt::entity dancer) { return save_state::ToRef(dancer); });
		record.dance = save_state::ToRef(dance->dance);
	}
	record.canDie = body.canDie ? 1 : 0;
	record.strength = body.strength;
	record.size = body.size;
	record.alignment = AlignmentToSave(body);
	record.autoScale = body.autoScale.has_value() ? 1 : 0;
	record.autoScaleShare = body.autoScale.value_or(0.0f);
	if (const auto* creed = registry.TryGet<const CreatureCreedPowers>(entity))
	{
		for (size_t glow = 0; glow < record.creedPowers.size(); ++glow)
		{
			record.creedPowers.at(glow) = {.scale = creed->glows.at(glow).scale, .power = creed->glows.at(glow).power};
		}
	}

	// The plan and its agenda, while a script controls the creature, it fights, or villagers dance with it
	const bool fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(entity);
	const auto* mind = registry.TryGet<const CreatureMindState>(entity);
	if (mind != nullptr && mind->planner.current.has_value() &&
	    save_state::KeepsPlan(registry.AllOf<ScriptControlled>(entity), fighting, record.dancePartners.size()))
	{
		const auto& plan = *mind->planner.current;
		gamesave::KeptPlan kept;
		kept.desire = static_cast<uint32_t>(plan.desire);
		kept.action = plan.action;
		const auto ref = [](const std::optional<uint32_t>& id) {
			return id.has_value() ? save_state::ToRef(static_cast<entt::entity>(*id)) : gamesave::ObjectRef {};
		};
		kept.objects = {ref(plan.activityObject), ref(plan.object), ref(plan.instrument)};
		kept.agenda = save_state::AgendaToSave(mind->idle, save_state::ToRef(entity));
		saved.agendaStep = static_cast<uint32_t>(mind->idle.step);
		record.plan = std::move(kept);
	}
	if (leash != nullptr)
	{
		gamesave::LeashRecord kept {.on = leash->worn.has_value() ? 1u : 0u,
		                            .works = leash->leashWorks ? 1u : 0u,
		                            .type = save_state::LeashTypeNumber(leash->selected),
		                            .player = save_state::PlayerNumber(body.owner)};
		if (leash->worn.has_value())
		{
			const auto& worn = *leash->worn;
			const auto lengths = creature_leash::InHand(body.size);
			kept.works = worn.works ? 1u : 0u;
			kept.type = save_state::LeashTypeNumber(worn.type);
			kept.player = save_state::PlayerNumber(worn.holder);
			kept.tiedTo = save_state::ToRef(worn.tiedTo);
			kept.length = lengths.slack;
			kept.fullLength = lengths.max;
		}
		saved.leash = gamesave::LeashStatusObject {.thing = {}, .leash = kept};
	}
	return saved;
}

/// The creature of openblack that a creature of a file of the game is: the first creature of its player, or for a
/// creature that belongs to no one, the next creature in the order of making that belongs to no one and has its species.
/// A creature that a creature of the file is already does not count.
std::optional<entt::entity> MatchCreature(ecs::Registry& registry, PlayerNames owner, std::optional<CreatureType> species,
                                          std::span<const entt::entity> inOrder, std::span<const entt::entity> used)
{
	const auto isUsed = [&used](entt::entity entity) { return std::ranges::find(used, entity) != used.end(); };
	if (owner != PlayerNames::NEUTRAL)
	{
		if (Locator::playerSystem::has_value())
		{
			const auto primary = Locator::playerSystem::value().GetPrimaryCreature(owner);
			if (primary.has_value() && !isUsed(*primary))
			{
				return primary;
			}
		}
		return std::nullopt;
	}
	for (const auto entity : inOrder)
	{
		const auto& body = registry.Get<const Creature>(entity);
		if (body.owner == PlayerNames::NEUTRAL && !isUsed(entity) && (!species.has_value() || body.species == *species))
		{
			return entity;
		}
	}
	return std::nullopt;
}

/// The mind file that a load gives the creature: the profile's file for the local player's creature, the saved game's
/// file for any other creature
struct MindForLoad
{
	/// The file opened and is not empty, so the load puts the creature back on the map
	bool resolves {false};
	/// The mind that the file holds, when openblack could read it
	std::shared_ptr<const creaturemind::MindFileData> mind;
};

MindForLoad MindToLoad(const gamesave::CreatureRecord& record, const std::filesystem::path& mindFolder)
{
	std::optional<std::filesystem::path> path;
	if (save_state::PlayerFromNumber(record.owner) == LocalPlayer())
	{
		if (Locator::creatureCarryOverSystem::has_value())
		{
			path = Locator::creatureCarryOverSystem::value().ProfileMind();
		}
	}
	else
	{
		path = mindFolder / gamesave::SavedCreatureFileName(record.index);
	}
	// The game puts the creature back only when its mind file opens and is not empty
	std::error_code error;
	const auto size = path.has_value() && std::filesystem::is_regular_file(*path, error)
	                      ? std::optional(static_cast<size_t>(std::filesystem::file_size(*path, error)))
	                      : std::nullopt;
	MindForLoad found {.resolves = save_state::ResolvesOnLoad(error ? std::nullopt : size)};
	if (!found.resolves)
	{
		return found;
	}
	auto mind = std::make_shared<creaturemind::MindFileData>();
	if (creaturemind::ReadFile(*path, *mind) != creaturemind::MindResult::Success)
	{
		SPDLOG_LOGGER_WARN(Log(), "The mind file {} could not be read", path->generic_string());
		return found;
	}
	found.mind = std::move(mind);
	return found;
}

/// Puts the creature back as the save keeps it: the values of its record, then the mind of its mind file over them,
/// then the plan that the save kept, then its leash
void Restore(ecs::Registry& registry, entt::entity entity, const gamesave::SavedCreature& saved, const MindForLoad& found)
{
	const auto& object = saved.creature;
	const auto& record = object.creature;
	auto& body = registry.Get<Creature>(entity);
	// It starts as a new creature does, which takes notice of every reaction
	registry.Remove<CreatureReactionsOff>(entity);
	if (auto* transform = registry.TryGet<Transform>(entity))
	{
		transform->position = WorldPosition(object.position.position);
		transform->rotation = glm::eulerAngleY(saved.heading);
	}
	if (auto* locomotion = registry.TryGet<CreatureLocomotion>(entity))
	{
		// It reads its heading again from where it faces, and stands
		locomotion->started = false;
		locomotion->motion = CreatureLocomotion::Motion::Standing;
		locomotion->speed = 0.0f;
	}
	auto& spells =
	    registry.AllOf<CreatureSpells>(entity) ? registry.Get<CreatureSpells>(entity) : registry.Assign<CreatureSpells>(entity);
	std::ranges::transform(record.spells, spells.spells.slots.begin(), save_state::FromSlot);
	spells.spells.waiting.clear();
	for (const auto& queued : record.queued)
	{
		if (const auto waiting = save_state::FromQueued(queued, k_TurnsPerSecond); waiting.has_value())
		{
			spells.spells.waiting.push_back(*waiting);
		}
	}
	spells.pausedMind = record.mindProcessing == 0;
	spells.largestSize = record.largestSize;
	spells.smallestSize = record.smallestSize;
	spells.freeze = record.freeze;
	spells.fizz = record.fizz;
	const creature_fizz::Fizz fizz {.now = record.fizz,
	                                .target = record.fizzTarget,
	                                .perSecond = record.fizzPerSecond,
	                                .goesForGood = record.fizzedForGood != 0};
	if (creature_fizz::Settled(fizz))
	{
		registry.Remove<CreatureFizz>(entity);
	}
	else
	{
		registry.AssignOrReplace<CreatureFizz>(entity, CreatureFizz {.fizz = fizz});
	}
	body.mayGoThroughVortex = record.mayGoThroughVortex != 0;
	const auto targetObject = save_state::FromRef(record.targetObject);
	registry.AssignOrReplace<CreatureTarget>(
	    entity,
	    CreatureTarget {.point = glm::vec3(record.targetPoint[0], record.targetPoint[1], record.targetPoint[2]),
	                    .object = targetObject.has_value() && registry.Valid(*targetObject) ? targetObject : std::nullopt,
	                    .has = record.hasTarget != 0});
	if (auto* leash = registry.TryGet<CreatureLeash>(entity); leash != nullptr && record.home != gamesave::MapCoords {})
	{
		leash->home = WorldPosition(record.home);
	}
	auto& fight = registry.AllOf<CreatureFightRecord>(entity) ? registry.Get<CreatureFightRecord>(entity)
	                                                          : registry.Assign<CreatureFightRecord>(entity);
	fight.health = record.fightHealth;
	if (record.autoFight <= static_cast<uint32_t>(creature_fight::Control::Computer))
	{
		fight.control = static_cast<creature_fight::Control>(record.autoFight);
	}
	if (auto* dance = registry.TryGet<CreatureDance>(entity))
	{
		dance->dancers.clear();
		for (const auto& partner : record.dancePartners)
		{
			if (const auto dancer = save_state::FromRef(partner); dancer.has_value() && registry.Valid(*dancer))
			{
				dance->dancers.push_back(*dancer);
			}
		}
		const auto kept = save_state::FromRef(record.dance);
		dance->dance = kept.has_value() && registry.Valid(*kept) ? *kept : entt::null;
	}
	body.canDie = record.canDie != 0;
	body.strength = record.strength;
	body.size = record.size;
	body.alignment = record.alignment;
	body.autoScale = record.autoScale != 0 ? std::optional(record.autoScaleShare) : std::nullopt;
	auto& creed = registry.AllOf<CreatureCreedPowers>(entity) ? registry.Get<CreatureCreedPowers>(entity)
	                                                          : registry.Assign<CreatureCreedPowers>(entity);
	for (size_t glow = 0; glow < record.creedPowers.size(); ++glow)
	{
		creed.glows.at(glow) = {.scale = record.creedPowers.at(glow).scale, .power = record.creedPowers.at(glow).power};
	}

	// The mind file's physique and mind come over the record's values. When the mind file is missing or empty, the
	// game does not put the creature back on the map, and the creature is lost from the land.
	if (!found.resolves)
	{
		SPDLOG_LOGGER_WARN(Log(), "The creature {} has no mind file to load, so it is not put back on the map",
		                   entt::to_integral(entity));
		registry.Remove<MapCellResident, MapCellMover>(entity);
		registry.AssignOrReplace<HiddenByState>(entity);
	}
	else if (found.mind != nullptr)
	{
		const auto& mind = found.mind;
		if (const auto kept = creature_mind_body::FromMindFile(*mind); kept.has_value())
		{
			ecs::creature_body_file::Apply(registry, entity, *kept);
		}
		std::optional<CreatureMindSystemInterface::RestoredPlan> plan;
		if (record.plan.has_value())
		{
			const auto& kept = *record.plan;
			plan = CreatureMindSystemInterface::RestoredPlan {
			    .desire = static_cast<creature_desires::Desire>(kept.desire),
			    .action = kept.action,
			    .object = save_state::FromRef(kept.objects[1]),
			    .activityObject = save_state::FromRef(kept.objects[0]),
			    .instrument = save_state::FromRef(kept.objects[2]),
			    .step = saved.agendaStep,
			};
		}
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().RestoreMind(entity, mind, plan);
		}
	}

	// The leash goes on or off again as the player had it
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	if (!saved.leash.has_value() || saved.leash->leash.on == 0)
	{
		if (leashes.IsLeashed(entity))
		{
			leashes.TakeOff(entity);
		}
		return;
	}
	const auto& kept = saved.leash->leash;
	const auto type = save_state::LeashTypeFromNumber(kept.type).value_or(LeashType::Rope);
	if (!leashes.IsLeashed(entity))
	{
		leashes.PutOn(entity, type);
	}
	else if (leashes.TypeOf(entity) != type)
	{
		leashes.ChangeType(entity, type);
	}
	leashes.SetWorks(entity, kept.works != 0);
	if (const auto tied = save_state::FromRef(kept.tiedTo); tied.has_value() && registry.Valid(*tied))
	{
		leashes.TieTo(entity, *tied);
	}
}

/// The record that a save writes for the creature: openblack's record, or for a creature that came from a file of the
/// game, the file's record with the values that openblack changed since the load
gamesave::SavedCreature ToSave(ecs::Registry& registry, entt::entity entity, uint32_t index)
{
	auto saved = Capture(registry, entity, index);
	if (const auto* vanilla = registry.TryGet<const CreatureVanillaRecord>(entity))
	{
		saved.creature = save_state::MergeOnto(vanilla->loaded, vanilla->baseline, saved.creature);
		saved.vanillaNumber = vanilla->number;
		if (vanilla->loadedLeash.has_value())
		{
			const auto now = saved.leash.value_or(vanilla->baselineLeash.value_or(*vanilla->loadedLeash));
			saved.leash = save_state::MergeLeashOnto(*vanilla->loadedLeash, vanilla->baselineLeash.value_or(now), now);
		}
	}
	return saved;
}

/// Changes the keys of the creatures of openblack into the numbers of the game's file, for a write into that file
ObjectRefKeys VanillaKeys(ecs::Registry& registry)
{
	ObjectRefKeys keys;
	registry.Each<const CreatureVanillaRecord>([&keys](entt::entity entity, const CreatureVanillaRecord& vanilla) {
		keys.emplace(save_state::ToRef(entity).key, gamesave::NumberKey(vanilla.number));
	});
	return keys;
}

gamesave::ObjectKey Translate(const ObjectRefKeys& keys, gamesave::ObjectKey key)
{
	const auto found = keys.find(key);
	return found != keys.end() ? found->second : key;
}

/// Whether the land's file of the game's saved game is the land that is open
bool IsOpenLand(std::string_view landscape)
{
	const auto* instance = Game::Instance();
	if (instance == nullptr)
	{
		return false;
	}
	auto saved = std::string(landscape);
	std::ranges::replace(saved, '\\', '/');
	const auto savedName = std::filesystem::path(saved).filename().string();
	const auto openName = instance->GetLandPath().filename().string();
	return std::ranges::equal(savedName, openName, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}
} // namespace

bool SaveGameSystem::Save(uint32_t slot, std::u16string_view name)
{
	const auto player = PlayerFolder();
	if (!gamesave::IsSlot(slot) || !player.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "The game cannot save into slot {}: no slot or no player folder", slot);
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto land = LandNumber();
	const auto slotFolder = *player / gamesave::SlotFolder(slot, land);
	const auto mindFolder = *player / gamesave::MindFolder(slot, land);

	gamesave::WorldFile world;
	gamesave::GameChunk game {.land = land,
	                          .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u,
	                          .landscape = {},
	                          .knownLeashes = 0,
	                          .creatureInTemple = 0};
	if (const auto* instance = Game::Instance(); instance != nullptr)
	{
		game.landscape = instance->GetLandPath().generic_string();
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		game.creatureInTemple = Locator::creatureCaveSystem::value().InTemple() ? 1 : 0;
	}
	if (Locator::leashSystem::has_value())
	{
		if (const auto creature = Locator::leashSystem::value().PlayersCreature(LocalPlayer()); creature.has_value())
		{
			if (const auto* leash = registry.TryGet<const CreatureLeash>(*creature))
			{
				game.knownLeashes = gamesave::ToByte(save_state::ToKnown(leash->known));
			}
		}
	}
	world.chunks.push_back(gamesave::ToChunk(game));

	bool written = true;
	const auto creatures = CreaturesInOrder();
	std::vector<gamesave::SavedCreature> saved;
	std::vector<entt::entity> savedEntities;
	for (uint32_t index = 0; index < creatures.size(); ++index)
	{
		const auto entity = creatures[index];
		auto creature = ToSave(registry, entity, index);
		SPDLOG_LOGGER_INFO(Log(), "Saved the creature {} as number {} of its file; the save keeps its plan: {}",
		                   entt::to_integral(entity), creature.creature.creature.index,
		                   creature.creature.creature.plan.has_value());
		// Each creature's mind goes in a mind file of its own beside the world file, named by the creature's index
		if (Locator::creatureMindSystem::has_value())
		{
			if (const auto mind = Locator::creatureMindSystem::value().SaveMind(entity); mind.has_value())
			{
				written =
				    WriteMind(mindFolder, gamesave::SavedCreatureFileName(creature.creature.creature.index), *mind) && written;
			}
		}
		saved.push_back(std::move(creature));
		savedEntities.push_back(entity);
	}
	const auto chunks = gamesave::ToChunks(saved);
	world.chunks.insert(world.chunks.end(), chunks.begin(), chunks.end());
	written = WriteFile(slotFolder, gamesave::WorldFileName(slot), gamesave::Write(world)) && written;

	// A game that came from a file of the game also goes back into that file's layout: the file as it was, with the
	// records of its creatures and their leashes as openblack has them now
	if (_vanilla.has_value() && _vanilla->land == land)
	{
		written = WriteVanilla(slot, slotFolder, saved, savedEntities) && written;
	}
	const gamesave::SaveGameInfo info {.slot = slot, .date = DateNow(), .name = std::u16string(name)};
	written = WriteFile(slotFolder, gamesave::k_InfoFileName, gamesave::Write(info)) && written;
	SPDLOG_LOGGER_INFO(Log(), "Saved the game into slot {} with {} creatures{}", slot, creatures.size(),
	                   written ? "" : ", but a file could not be written");
	return written;
}

bool SaveGameSystem::WriteVanilla(uint32_t slot, const std::filesystem::path& slotFolder,
                                  std::span<const gamesave::SavedCreature> saved, std::span<const entt::entity> entities)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& origin = *_vanilla;
	const auto keys = VanillaKeys(registry);
	struct Edit
	{
		size_t at {0};
		size_t end {0};
		gamesave::StreamStart start {};
		uint32_t objectsAfter {0};
		gamesave::ObjectRef ref;
		uint32_t number {0};
	};
	std::vector<Edit> edits;
	for (size_t i = 0; i < saved.size(); ++i)
	{
		const auto* vanilla = registry.TryGet<const CreatureVanillaRecord>(entities[i]);
		if (vanilla == nullptr)
		{
			SPDLOG_LOGGER_WARN(Log(), "The creature {} is not in the game's file, so the file keeps no record of it",
			                   entt::to_integral(entities[i]));
			continue;
		}
		const auto found = std::ranges::find(origin.creatures, vanilla->number, &gamesave::FoundCreature::number);
		if (found == origin.creatures.end())
		{
			continue;
		}
		auto creature = saved[i].creature;
		save_state::Rekey(creature, [&keys](gamesave::ObjectKey key) { return Translate(keys, key); });
		edits.push_back({.at = found->at,
		                 .end = found->end,
		                 .start = found->start,
		                 .objectsAfter = found->objectsAfter,
		                 .ref = {.key = gamesave::NumberKey(vanilla->number),
		                         .record = std::make_shared<gamesave::SavedObject>(gamesave::SavedObject {.value = creature})},
		                 .number = vanilla->number});
		if (saved[i].leash.has_value() && vanilla->leashNumber != 0)
		{
			const auto leash = std::ranges::find(origin.leashes, vanilla->leashNumber, &gamesave::FoundLeash::number);
			if (leash != origin.leashes.end())
			{
				auto record = *saved[i].leash;
				for (auto* ref : {&record.leash.tiedTo, &record.leash.holder})
				{
					ref->key = ref->Empty() ? ref->key : Translate(keys, ref->key);
				}
				edits.push_back(
				    {.at = leash->at,
				     .end = leash->end,
				     .start = leash->start,
				     .objectsAfter = leash->objectsAfter,
				     .ref = {.key = gamesave::NumberKey(vanilla->leashNumber),
				             .record = std::make_shared<gamesave::SavedObject>(gamesave::SavedObject {.value = record})},
				     .number = vanilla->leashNumber});
			}
		}
	}
	// The records go in from the end of the file, so that the places of the records before them stay the same
	std::ranges::sort(edits, std::ranges::greater {}, &Edit::at);
	auto bytes = origin.bytes;
	for (const auto& edit : edits)
	{
		auto changed = gamesave::Replace(bytes, edit.at, edit.end, edit.start, edit.objectsAfter, edit.ref);
		if (!changed.has_value())
		{
			SPDLOG_LOGGER_WARN(Log(),
			                   "The record of object {} refers to an object that the game's file does not have; "
			                   "the file keeps its old record",
			                   edit.number);
			continue;
		}
		bytes = std::move(*changed);
	}
	// A complete save has no attempt to load it yet
	std::fill_n(bytes.begin(), std::min<size_t>(bytes.size(), sizeof(uint32_t)), uint8_t {0});
	bool written = WriteFile(slotFolder, gamesave::VanillaWorldFileName(slot), bytes);
	// The game loads the script machine from the file beside the world file. openblack does not write it yet, so the
	// save keeps the file of the game's save as it was.
	if (const auto script = ReadFile(origin.folder / gamesave::VanillaScriptFileName(origin.slot)); script.has_value())
	{
		written = WriteFile(slotFolder, gamesave::VanillaScriptFileName(slot), *script) && written;
	}
	SPDLOG_LOGGER_INFO(Log(), "Wrote {} records into the game's file of slot {}", edits.size(), slot);
	return written;
}

bool SaveGameSystem::Load(uint32_t slot)
{
	const auto player = PlayerFolder();
	if (!gamesave::IsSlot(slot) || !player.has_value())
	{
		return false;
	}
	const auto land = LandNumber();
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	const auto slotFolder = *root / *player / gamesave::SlotFolder(slot, land);
	const auto mindFolder = *root / *player / gamesave::MindFolder(slot, land);
	const auto vanillaBytes = ReadFile(slotFolder / gamesave::VanillaWorldFileName(slot));
	const auto bytes = ReadFile(slotFolder / gamesave::WorldFileName(slot));
	if (!bytes.has_value())
	{
		// A slot of the game itself: its world file in the game's layout
		return vanillaBytes.has_value() && LoadVanilla(slot, slotFolder, mindFolder, *vanillaBytes);
	}
	const auto world = gamesave::ReadWorldFile(*bytes);
	if (!world.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} has no world file that the game can read", slot);
		return false;
	}
	const auto gameChunk = std::ranges::find(world->chunks, gamesave::k_GameTag, &gamesave::Chunk::tag);
	const auto game = gameChunk != world->chunks.end() ? gamesave::ReadGameChunk(gameChunk->bytes) : std::nullopt;
	if (!game.has_value() || game->land != land)
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} keeps another land; only the creatures of the land that is open load", slot);
		return false;
	}
	const auto creatures = gamesave::ReadCreatures(*world);
	if (!creatures.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} has no creatures that the game can read", slot);
		return false;
	}
	// The game's file beside openblack's file, which openblack wrote at the same save, takes the creatures that came from
	// a file of the game at the next save too
	SetVanillaOrigin(slot, land, slotFolder, vanillaBytes);
	auto& registry = Locator::entitiesRegistry::value();
	size_t restored = 0;
	for (const auto& creature : *creatures)
	{
		const auto entity = save_state::FromRef(gamesave::RefTo(creature.key));
		if (!entity.has_value() || !registry.Valid(*entity) || !registry.AllOf<Creature>(*entity))
		{
			SPDLOG_LOGGER_WARN(Log(), "A creature of slot {} is not on the land", slot);
			continue;
		}
		Restore(registry, *entity, creature, MindToLoad(creature.creature.creature, mindFolder));
		registry.Remove<CreatureVanillaRecord>(*entity);
		if (creature.vanillaNumber != 0 && _vanilla.has_value())
		{
			KeepVanillaRecord(registry, *entity, creature.vanillaNumber, creature.creature, creature.leash);
		}
		SPDLOG_LOGGER_INFO(Log(), "Put the creature {} back, at step {} of its plan", entt::to_integral(*entity),
		                   creature.creature.creature.plan.has_value() ? std::to_string(creature.agendaStep)
		                                                               : std::string("none"));
		++restored;
	}
	// Then the local player's creature knows the leashes that the save keeps for it
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		if (const auto creature = leashes.PlayersCreature(LocalPlayer()); creature.has_value())
		{
			const auto known = save_state::FromKnown(gamesave::KnownLeashesFromByte(game->knownLeashes));
			for (size_t i = 0; i < creature_leash::k_Types.size(); ++i)
			{
				leashes.SetKnown(*creature, creature_leash::k_Types.at(i), known.test(i));
			}
		}
	}
	SPDLOG_LOGGER_INFO(Log(), "Loaded {} creatures from slot {}", restored, slot);
	return restored > 0;
}

void SaveGameSystem::SetVanillaOrigin(uint32_t slot, uint32_t land, const std::filesystem::path& folder,
                                      const std::optional<std::vector<uint8_t>>& bytes)
{
	_vanilla.reset();
	if (!bytes.has_value())
	{
		return;
	}
	VanillaOrigin origin {.slot = slot, .land = land, .folder = folder, .bytes = *bytes};
	origin.creatures = gamesave::FindCreatures(origin.bytes);
	origin.leashes = gamesave::FindLeashes(origin.bytes);
	_vanilla = std::move(origin);
}

void SaveGameSystem::KeepVanillaRecord(ecs::Registry& registry, entt::entity entity, uint32_t number,
                                       const gamesave::CreatureObject& loaded,
                                       const std::optional<gamesave::LeashStatusObject>& loadedLeash)
{
	const auto inOrder = CreaturesInOrder();
	const auto index = static_cast<uint32_t>(std::ranges::find(inOrder, entity) - inOrder.begin());
	const auto baseline = Capture(registry, entity, index);
	CreatureVanillaRecord vanilla {.number = number,
	                               .loaded = loaded,
	                               .baseline = baseline.creature,
	                               .loadedLeash = loadedLeash,
	                               .baselineLeash = baseline.leash,
	                               .leashNumber = 0};
	if (loadedLeash.has_value() && _vanilla.has_value())
	{
		// The leash of the creature's player in the file, by its record
		const auto found = std::ranges::find_if(_vanilla->leashes, [&loadedLeash](const gamesave::FoundLeash& leash) {
			return leash.object.leash.player == loadedLeash->leash.player;
		});
		vanilla.leashNumber = found != _vanilla->leashes.end() ? found->number : 0;
	}
	registry.AssignOrReplace<CreatureVanillaRecord>(entity, std::move(vanilla));
}

bool SaveGameSystem::LoadVanilla(uint32_t slot, const std::filesystem::path& slotFolder,
                                 const std::filesystem::path& mindFolder, const std::vector<uint8_t>& bytes)
{
	const auto framing = gamesave::ReadVanillaFraming(bytes);
	if (!framing.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} has a world file of the game that does not start as the game writes it", slot);
		return false;
	}
	if (!IsOpenLand(framing->landscape))
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} keeps the land {}; only the creatures of the land that is open load", slot,
		                   framing->landscape);
		return false;
	}
	SetVanillaOrigin(slot, LandNumber(), slotFolder, bytes);
	auto& registry = Locator::entitiesRegistry::value();
	auto found = _vanilla->creatures;
	std::ranges::sort(found, {}, [](const gamesave::FoundCreature& creature) { return creature.object.creature.index; });

	// First each creature of the file gets its creature of openblack: the player's creature for a creature of a player,
	// the next creature that belongs to no one for the others, or a new creature as the game makes each one that the
	// file has
	std::vector<entt::entity> used;
	std::vector<std::pair<const gamesave::FoundCreature*, entt::entity>> matched;
	std::vector<MindForLoad> minds;
	ObjectRefKeys keys;
	for (const auto& creature : found)
	{
		const auto& record = creature.object.creature;
		auto mind = MindToLoad(record, mindFolder);
		const auto body = mind.mind != nullptr ? creature_mind_body::FromMindFile(*mind.mind) : std::nullopt;
		const auto owner = save_state::PlayerFromNumber(record.owner).value_or(PlayerNames::NEUTRAL);
		auto entity = MatchCreature(registry, owner, body.has_value() ? std::optional(body->species) : std::nullopt,
		                            CreaturesInOrder(), used);
		if (!entity.has_value())
		{
			if (!mind.resolves || !body.has_value())
			{
				SPDLOG_LOGGER_WARN(Log(), "The creature {} of slot {} has no mind file to load, so it is not made",
				                   creature.number, slot);
				continue;
			}
			entity = ecs::archetypes::CreatureArchetype::Create(WorldPosition(creature.object.position.position), owner,
			                                                    body->species, 0, creature.object.object.heading, record.size,
			                                                    ecs::archetypes::CreatureArchetype::StartBody(body->species));
		}
		used.push_back(*entity);
		keys.emplace(gamesave::NumberKey(creature.number), save_state::ToRef(*entity).key);
		matched.emplace_back(&creature, *entity);
		minds.push_back(std::move(mind));
	}

	// Then each creature takes its record, with the references to the creatures of the file as references to their
	// creatures of openblack
	for (size_t i = 0; i < matched.size(); ++i)
	{
		const auto& [creature, entity] = matched[i];
		auto loaded = creature->object;
		save_state::Rekey(loaded, [&keys](gamesave::ObjectKey key) { return Translate(keys, key); });
		std::optional<gamesave::LeashStatusObject> leash;
		if (creature->object.creature.owner != gamesave::k_None)
		{
			const auto ownLeash = std::ranges::find_if(_vanilla->leashes, [creature](const gamesave::FoundLeash& each) {
				return each.object.leash.player == creature->object.creature.owner;
			});
			if (ownLeash != _vanilla->leashes.end())
			{
				leash = ownLeash->object;
				for (auto* ref : {&leash->leash.tiedTo, &leash->leash.holder})
				{
					ref->key = ref->Empty() ? ref->key : Translate(keys, ref->key);
				}
			}
		}
		const gamesave::SavedCreature saved {.key = save_state::ToRef(entity).key,
		                                     .creature = loaded,
		                                     .leash = leash,
		                                     .agendaStep = 0,
		                                     .heading = loaded.object.heading,
		                                     .vanillaNumber = creature->number};
		Restore(registry, entity, saved, minds[i]);
		KeepVanillaRecord(registry, entity, creature->number, loaded, leash);
		SPDLOG_LOGGER_INFO(Log(), "Put creature {} of the game's file into creature {}", creature->number,
		                   entt::to_integral(entity));
	}
	SPDLOG_LOGGER_INFO(Log(), "Loaded {} creatures from the game's file of slot {}", matched.size(), slot);
	return !matched.empty();
}

std::vector<SaveGameSystem::VanillaCheck> SaveGameSystem::CheckVanillaRecords() const
{
	std::vector<VanillaCheck> checks;
	if (!_vanilla.has_value())
	{
		return checks;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto keys = VanillaKeys(registry);
	const auto creatures = CreaturesInOrder();
	for (uint32_t index = 0; index < creatures.size(); ++index)
	{
		const auto* vanilla = registry.TryGet<const CreatureVanillaRecord>(creatures[index]);
		if (vanilla == nullptr)
		{
			continue;
		}
		const auto found = std::ranges::find(_vanilla->creatures, vanilla->number, &gamesave::FoundCreature::number);
		if (found == _vanilla->creatures.end())
		{
			continue;
		}
		auto creature = ToSave(registry, creatures[index], index).creature;
		save_state::Rekey(creature, [&keys](gamesave::ObjectKey key) { return Translate(keys, key); });
		const auto written = gamesave::WriteRecordAt(
		    found->start, {.key = gamesave::NumberKey(vanilla->number),
		                   .record = std::make_shared<gamesave::SavedObject>(gamesave::SavedObject {.value = creature})});
		const std::span original(_vanilla->bytes.data() + found->at, found->end - found->at);
		VanillaCheck check {.number = vanilla->number, .entity = static_cast<uint32_t>(entt::to_integral(creatures[index]))};
		check.size = original.size();
		if (written.has_value())
		{
			const auto [mine, theirs] = std::ranges::mismatch(*written, original);
			check.same = written->size() == original.size() && mine == written->end();
			if (!check.same)
			{
				check.firstDifference = static_cast<size_t>(mine - written->begin());
			}
		}
		checks.push_back(check);
	}
	return checks;
}

std::vector<SaveGameSystem::Slot> SaveGameSystem::Slots() const
{
	std::vector<Slot> slots;
	const auto player = PlayerFolder();
	if (!player.has_value())
	{
		return slots;
	}
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	const auto land = LandNumber();
	for (uint32_t slot = 0; slot < gamesave::k_SlotCount; ++slot)
	{
		const auto folder = *root / *player / gamesave::SlotFolder(slot, land);
		// The room lists a slot only when its world file is beside its info file
		if (!Locator::filesystem::value().Exists(folder / gamesave::WorldFileName(slot)) &&
		    !Locator::filesystem::value().Exists(folder / gamesave::VanillaWorldFileName(slot)))
		{
			continue;
		}
		const auto bytes = ReadFile(folder / gamesave::k_InfoFileName);
		const auto info = bytes.has_value() ? gamesave::ReadSaveGameInfo(*bytes) : std::nullopt;
		if (info.has_value())
		{
			slots.push_back({.slot = slot, .land = land, .date = info->date, .name = info->name});
		}
	}
	return slots;
}
