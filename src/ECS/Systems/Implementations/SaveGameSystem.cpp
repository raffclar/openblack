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
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <CreatureRecord.h>
#include <InstantSaves.h>
#include <LHVM.h>
#include <MindFile.h>
#include <SaveGameInfo.h>
#include <SavePictureMaker.h>
#include <SavePictures.h>
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
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureHandGlows.h"
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
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldSave.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/RendererInterface.h"
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
	return ecs::save_files::ReadFile(path);
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

/// A world point as the game's map position. The altitude is the height above the ground. A point that openblack took
/// from a map position gives that position back: x and z go to the nearest 65536th of a cell.
gamesave::MapCoords MapPosition(const glm::vec3& point)
{
	constexpr double k_FixedPerMetre = 65536.0 / 10.0;
	map_coords::MapCoords coords {static_cast<int32_t>(std::llround(static_cast<double>(point.x) * k_FixedPerMetre)),
	                              static_cast<int32_t>(std::llround(static_cast<double>(point.z) * k_FixedPerMetre)), 0.0f};
	if (Locator::terrainSystem::has_value())
	{
		coords.altitude = point.y - map_coords::ToWorld(Locator::terrainSystem::value(), coords).y;
	}
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// The height of a world point above the ground at a map position
float AltitudeAt(const map_coords::MapCoords& place, float height)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	return height - map_coords::ToWorld(Locator::terrainSystem::value(), {.x = place.x, .z = place.z, .altitude = 0.0f}).y;
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
		// The creature's exact map position, and its height above the ground there
		const auto place = ecs::thing_map_place::Of(registry, entity);
		object.position.position = {.x = place.x, .z = place.z, .altitude = AltitudeAt(place, transform->position.y)};
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
		// A home that came from a map position goes back as it was; metres lose units far from the corner of the map
		record.home = leash->homeOnMap.has_value() ? gamesave::MapCoords {.x = leash->homeOnMap->x,
		                                                                  .z = leash->homeOnMap->z,
		                                                                  .altitude = leash->homeOnMap->altitude}
		                                           : MapPosition(*leash->home);
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
	if (const auto* glows = registry.TryGet<const CreatureHandGlows>(entity))
	{
		for (size_t place = 0; place < record.creedPowers.size(); ++place)
		{
			const auto kept = creature_hand_glow::SavedPowerOf(glows->glows, static_cast<creature_hand_glow::Place>(place));
			record.creedPowers.at(place) = {.scale = kept.scale, .power = kept.power};
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
		const auto inHand = creature_leash::InHand(body.size);
		gamesave::LeashRecord kept {.on = leash->worn.has_value() ? 1u : 0u,
		                            .works = leash->leashWorks ? 1u : 0u,
		                            .type = save_state::LeashTypeNumber(leash->selected),
		                            .player = save_state::PlayerNumber(body.owner),
		                            .length = inHand.slack,
		                            .fullLength = inHand.max};
		if (leash->worn.has_value())
		{
			const auto& worn = *leash->worn;
			kept.works = worn.works ? 1u : 0u;
			kept.type = save_state::LeashTypeNumber(worn.type);
			kept.player = save_state::PlayerNumber(worn.holder);
			kept.tiedTo = save_state::ToRef(worn.tiedTo);
			kept.length = worn.rope.slackLength;
			kept.fullLength = worn.rope.maxLength;
			// The turn that the leash was last tied
			kept.word34 = worn.tiedTurn;
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
		// It keeps the exact map position of the save, which its metres cannot keep far from the corner of the map
		ecs::thing_map_place::Keep(registry, entity,
		                           {.x = object.position.position.x, .z = object.position.position.z, .altitude = 0.0f});
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
		leash->homeOnMap = map_coords::MapCoords {.x = record.home.x, .z = record.home.z, .altitude = record.home.altitude};
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
	// Last, each place gets its saved creed glow in turn. The creature hums when a glow has power.
	std::array<creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount> creed {};
	std::ranges::transform(record.creedPowers, creed.begin(), [](const gamesave::CreedPower& kept) {
		return creature_hand_glow::SavedPower {.scale = kept.scale, .power = kept.power};
	});
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().LoadCreedPowers(entity, creed);
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
	auto open = instance->GetLandscapePath().generic_string();
	std::ranges::replace(open, '\\', '/');
	const auto openName = std::filesystem::path(open).filename().string();
	return std::ranges::equal(savedName, openName, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}
} // namespace

bool SaveGameSystem::Save(uint32_t slot, std::u16string_view name)
{
	return SaveSlot(slot, name, true);
}

bool SaveGameSystem::SaveSlot(uint32_t slot, std::u16string_view name, bool putPicture)
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
	                          .creatureInTemple = 0,
	                          .lastAutosaveTurn = _lastAutosaveTurn,
	                          .lastQuickSaveTurn = _lastQuickSaveTurn};
	if (const auto* instance = Game::Instance(); instance != nullptr)
	{
		game.landscape = instance->GetLandPath().generic_string();
		game.landscapeFile = instance->GetLandscapePath().generic_string();
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
	// Then the whole world in openblack's own layout
	ecs::world_save::SaveSummary summary;
	const auto whole = ecs::world_save::SaveWorld(Reflection(), summary);
	world.chunks.insert(world.chunks.end(), whole.begin(), whole.end());
	written = WriteFile(slotFolder, gamesave::WorldFileName(slot), gamesave::Write(world)) && written;

	// A game that came from a file of the game also goes back into that file's layout: the file as it was, with the
	// records of its creatures and their leashes as openblack has them now
	if (_vanilla.has_value() && _vanilla->land == land)
	{
		written = WriteVanilla(slot, slotFolder, saved, savedEntities) && written;
	}
	// The slot's pictures come from the temporary picture file, as the screen was when the game last took a picture
	if (putPicture)
	{
		PutSlotPicture(slot);
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
	// A save of the whole world loads its own land, whatever land is open
	if (game.has_value() && ecs::world_save::HasWholeWorld(*world))
	{
		return LoadWholeWorld(slot, *world, *game);
	}
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
	if (restored > 0)
	{
		// The next autosave and the next quick load come only after their full gap from the turn of the load
		const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
		_lastAutosaveTurn = turn;
		_lastLoadTurn = turn;
		_lastQuickSaveTurn = game->lastQuickSaveTurn;
	}
	return restored > 0;
}

const entt::meta_ctx& SaveGameSystem::Reflection()
{
	if (!_reflection)
	{
		_reflection = std::make_unique<entt::meta_ctx>();
		ecs::reflection::RegisterComponents(*_reflection);
	}
	return *_reflection;
}

bool SaveGameSystem::LoadWholeWorld(uint32_t slot, const gamesave::WorldFile& world, const gamesave::GameChunk& game)
{
	auto* instance = Game::Instance();
	if (instance == nullptr || (game.landscapeFile.empty() && game.landscape.empty()))
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} keeps no landscape, so its world cannot load", slot);
		return false;
	}
	const auto& reflection = Reflection();
	// The scripts of the game that runs now stop before its entities go, so that they let go of those entities and not
	// of the saved ones
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StopAllTasks();
	}
	const bool loaded = instance->LoadSavedLand(game.landscape, game.landscapeFile, [&reflection, &world]() {
		return ecs::world_save::LoadEntities(reflection, world).has_value();
	});
	if (!loaded)
	{
		SPDLOG_LOGGER_WARN(Log(), "The world of slot {} did not load", slot);
		return false;
	}
	// The clock, the random numbers and the scripts go on from the saved turn once the land has started
	if (!ecs::world_save::LoadRunningState(world, game.turn))
	{
		SPDLOG_LOGGER_WARN(Log(), "A part of the running state of slot {} did not read", slot);
	}
	// A load from a file of the game is not the origin of this world
	_vanilla.reset();
	// The next autosave and the next quick load come only after their full gap from the turn of the load
	_lastAutosaveTurn = game.turn;
	_lastLoadTurn = game.turn;
	_lastQuickSaveTurn = game.lastQuickSaveTurn;
	SPDLOG_LOGGER_INFO(Log(), "Loaded the whole world of slot {} at turn {}", slot, game.turn);
	return true;
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
	const auto savedGames = *root / *player / gamesave::k_SavedGamesFolder;
	// The room reads the folders in the order the system lists them
	std::vector<std::string> names;
	std::error_code error;
	for (std::filesystem::directory_iterator it(savedGames, error), end; !error && it != end; it.increment(error))
	{
		if (it->is_directory(error))
		{
			names.push_back(it->path().filename().string());
		}
	}
	std::ranges::sort(names, gamesave::ListedBefore);
	const auto land = LandNumber();
	for (const auto& name : names)
	{
		const auto found = gamesave::RoomFolderOf(name, land);
		if (!found.has_value())
		{
			continue;
		}
		const auto folder = savedGames / found->folder;
		// The room lists a folder only when its world file is beside its info file. The number in the world file's
		// name comes from the folder's name; the slot is the info file's.
		const auto number = std::to_string(found->fileSlot);
		if (!Locator::filesystem::value().Exists(folder / (number + ".obsav")) &&
		    !Locator::filesystem::value().Exists(folder / (number + ".sav")))
		{
			continue;
		}
		const auto bytes = ReadFile(folder / gamesave::k_InfoFileName);
		const auto info = bytes.has_value() ? gamesave::ReadSaveGameInfo(*bytes) : std::nullopt;
		if (info.has_value())
		{
			const auto landFolder =
			    found->folder.has_parent_path() ? gamesave::LeadingNumber(found->folder.filename().string()) : 0;
			slots.push_back(
			    {.slot = info->slot, .land = static_cast<uint32_t>(landFolder), .date = info->date, .name = info->name});
		}
	}
	return slots;
}

bool SaveGameSystem::Delete(uint32_t slot)
{
	const auto player = PlayerFolder();
	if (!player.has_value())
	{
		return false;
	}
	const auto listed = Slots();
	if (std::ranges::none_of(listed, [slot](const Slot& saved) { return saved.slot % 100 == slot; }))
	{
		return false;
	}
	// The folder of the slot, as the room reads it: the land's folder for an autosave, the previous land's or the next
	// land's folder for theirs
	const auto found = gamesave::RoomFolderOf(std::to_string(slot), LandNumber());
	if (!found.has_value())
	{
		return false;
	}
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	const auto folder = *root / *player / gamesave::k_SavedGamesFolder / found->folder;
	// The files go. A folder in it stays, and then the folder stays too.
	std::error_code error;
	std::vector<std::filesystem::path> files;
	for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
	{
		if (!it->is_directory(error))
		{
			files.push_back(it->path());
		}
	}
	for (const auto& file : files)
	{
		std::filesystem::remove(file, error);
	}
	std::filesystem::remove(folder, error);
	SPDLOG_LOGGER_INFO(Log(), "Deleted the saved game in slot {}", slot);
	return true;
}

std::optional<std::vector<uint8_t>> SaveGameSystem::ReadPictureFile(std::string_view name) const
{
	const auto player = PlayerFolder();
	if (!player.has_value())
	{
		return std::nullopt;
	}
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	const auto path = *root / *player / gamesave::k_SavedGamesFolder / gamesave::k_PicturesFolder / name;
	if (!Locator::filesystem::value().Exists(path))
	{
		return std::nullopt;
	}
	return ReadFile(path);
}

namespace
{
/// The folder of the saved games' pictures, relative to the profile folder
std::optional<std::filesystem::path> PicturesFolder()
{
	const auto player = PlayerFolder();
	if (!player.has_value())
	{
		return std::nullopt;
	}
	return *player / gamesave::k_SavedGamesFolder / gamesave::k_PicturesFolder;
}

bool ScriptCutScene()
{
	if (!Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}

uint32_t TurnNow()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
}

bool GameOver()
{
	return Locator::entitiesRegistry::value().Context().gameOver;
}

/// The name of a save that the game does by itself into the slot
std::u16string InstantName(uint32_t slot)
{
	const auto text = Locator::helpTextSystem::has_value()
	                      ? Locator::helpTextSystem::value().GetWords(gamesave::k_SlotNameTexts.at(slot))
	                      : std::u16string {};
	return gamesave::InstantSaveName(text, LandNumber());
}

std::optional<std::span<const uint8_t>> View(const std::optional<std::vector<uint8_t>>& bytes)
{
	return bytes.has_value() ? std::optional<std::span<const uint8_t>>(*bytes) : std::nullopt;
}
} // namespace

void SaveGameSystem::TakeTemplePicture()
{
	AskForPicture({});
}

bool SaveGameSystem::InstantSave(uint32_t slot)
{
	if (!gamesave::IsSlot(slot))
	{
		return false;
	}
	// The picture of the slot comes when the renderer gives back the picture of the screen
	const bool written = SaveSlot(slot, InstantName(slot), false);
	AskForPicture({slot});
	return written;
}

bool SaveGameSystem::SaveOnExit()
{
	const auto* game = Game::Instance();
	const auto& context = Locator::entitiesRegistry::value().Context();
	// No save as the player leaves the testbed, the Gods' Playground, or a game that is over
	if (game == nullptr || game->GetLandPath().empty() || game->GetLandPath() == "testbed" || context.gameOver ||
	    LandNumber() == gamesave::k_PlaygroundLand)
	{
		return false;
	}
	// The game makes the screen black before it saves, so it takes a picture of a black screen
	const std::vector<uint8_t> black(
	    static_cast<size_t>(gamesave::k_ScreenPictureWidth) * gamesave::k_ScreenPictureHeight * gamesave::k_TexelSize, 0);
	WriteTemporaryPicture(black);
	return SaveSlot(gamesave::k_ExitSlot, InstantName(gamesave::k_ExitSlot), true);
}

void SaveGameSystem::EndTurn()
{
	bool autosaveOn = player_profiles::Settings {}.autoSave;
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			autosaveOn = profile->settings.autoSave;
		}
	}
	const gamesave::AutosaveState state {
	    .autosaveOn = autosaveOn,
	    .gameOver = GameOver(),
	    .scriptCutScene = ScriptCutScene(),
	    .paused = Locator::time::has_value() && Locator::time::value().IsPaused(),
	    .land = LandNumber(),
	    .turn = TurnNow(),
	    .lastAutosaveTurn = _lastAutosaveTurn,
	};
	if (!gamesave::AutosaveDue(state))
	{
		return;
	}
	_lastAutosaveTurn = state.turn;
	// The autosaves of each land go in turn into the three autosave slots
	const auto count =
	    Locator::playerProfileSystem::has_value() ? Locator::playerProfileSystem::value().TakeLandAutosave(state.land) : 0u;
	SPDLOG_LOGGER_INFO(Log(), "Autosave at turn {} into slot {}", state.turn, gamesave::AutosaveSlot(count));
	InstantSave(gamesave::AutosaveSlot(count));
}

bool SaveGameSystem::QuickSave()
{
	const auto turn = TurnNow();
	if (!gamesave::QuickTurnAllowed(turn, _lastQuickSaveTurn, GameOver(), ScriptCutScene()))
	{
		return false;
	}
	_lastQuickSaveTurn = turn;
	return InstantSave(gamesave::k_QuickSaveSlot);
}

bool SaveGameSystem::QuickLoad()
{
	if (!gamesave::QuickTurnAllowed(TurnNow(), _lastLoadTurn, GameOver(), ScriptCutScene()))
	{
		return false;
	}
	return Load(gamesave::k_QuickSaveSlot);
}

void SaveGameSystem::NewLand()
{
	_lastAutosaveTurn = 0;
}

bool SaveGameSystem::AnyPictureWaiting() const
{
	return !_waitingPictures.empty();
}

void SaveGameSystem::AskForPicture(std::vector<uint32_t> slots)
{
	if (!Locator::rendererInterface::has_value())
	{
		for (const auto slot : slots)
		{
			PutSlotPicture(slot);
		}
		return;
	}
	auto name = std::format("{}save-picture/{}", graphics::ScreenPictures::k_Prefix, _pictureRequests++);
	Locator::rendererInterface::value().RequestScreenPicture(name, [](const graphics::ScreenView& screen) {
		return gamesave::SampleScreen({.width = screen.width,
		                               .height = screen.height,
		                               .pitch = screen.pitch,
		                               .bottomUp = screen.yflip,
		                               .bgra = screen.bgra})
		    .value_or(std::vector<uint8_t> {});
	});
	_waitingPictures.push_back({.name = std::move(name), .slots = std::move(slots)});
}

void SaveGameSystem::CollectPictures(const Screens& screens)
{
	while (!_waitingPictures.empty())
	{
		const auto& waiting = _waitingPictures.front();
		auto picture = screens.take ? screens.take(waiting.name) : std::nullopt;
		if (!picture.has_value())
		{
			// The pictures come back in the order that the system asked for them
			if (!screens.failed || !screens.failed(waiting.name))
			{
				return;
			}
			SPDLOG_LOGGER_WARN(Log(), "The renderer gave up the picture of the screen {}", waiting.name);
		}
		else if (!picture->empty())
		{
			SPDLOG_LOGGER_INFO(Log(), "The picture of the screen {} is back for {} slots", waiting.name, waiting.slots.size());
			WriteTemporaryPicture(*picture);
		}
		for (const auto slot : waiting.slots)
		{
			PutSlotPicture(slot);
		}
		_waitingPictures.erase(_waitingPictures.begin());
	}
}

void SaveGameSystem::WriteTemporaryPicture(std::span<const uint8_t> screenPicture)
{
	const auto folder = PicturesFolder();
	if (!folder.has_value())
	{
		return;
	}
	const auto old = ReadPictureFile(gamesave::k_TemporaryPictureFileName);
	WriteFile(*folder, gamesave::k_TemporaryPictureFileName, gamesave::MakeTemporaryPicture(View(old), screenPicture));
}

void SaveGameSystem::PutSlotPicture(uint32_t slot)
{
	const auto folder = PicturesFolder();
	if (!folder.has_value())
	{
		return;
	}
	const auto largeName = gamesave::LargePictureFileName(slot);
	const auto smallName = gamesave::SmallPictureFileName(slot);
	const auto temporary = ReadPictureFile(gamesave::k_TemporaryPictureFileName);
	const auto large = ReadPictureFile(largeName);
	const auto small = ReadPictureFile(smallName);
	const auto files = gamesave::PutSlotPictures(View(temporary), View(large), View(small), slot);
	WriteFile(*folder, largeName, files.large);
	WriteFile(*folder, smallName, files.small);
}
