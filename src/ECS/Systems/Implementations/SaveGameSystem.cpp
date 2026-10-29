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
#include <ctime>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ChallengePictureFiles.h>
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
#include <VanillaWorld.h>
#include <WorldFile.h>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/PlayerProfiles.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureSaveState.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/ChallengePictures.h"
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
#include "ECS/SaveDateLabel.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
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
#include "Gui/SavingBox.h"
#include "Gui/TextDatabase.h"
#include "LandChange.h"
#include "Locator.h"
#include "MainThreadWork.h"

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
/// the time and of the long date give the text, as in the game; when the long date is not available, the short date
/// takes its place. The time is the machine's clock as the game reads it.
std::u16string DateNow()
{
	const auto local = machine_clock::LocalTime();
#ifdef _WIN32
	const SYSTEMTIME now {.wYear = static_cast<WORD>(local.tm_year + 1900),
	                      .wMonth = static_cast<WORD>(local.tm_mon + 1),
	                      .wDayOfWeek = static_cast<WORD>(local.tm_wday),
	                      .wDay = static_cast<WORD>(local.tm_mday),
	                      .wHour = static_cast<WORD>(local.tm_hour),
	                      .wMinute = static_cast<WORD>(local.tm_min),
	                      .wSecond = static_cast<WORD>(local.tm_sec),
	                      .wMilliseconds = 0};
	const auto part = [](auto&& format) -> std::optional<std::u16string> {
		const auto size = format(nullptr, 0);
		if (size <= 0)
		{
			return std::nullopt;
		}
		std::vector<wchar_t> text(static_cast<size_t>(size));
		const auto written = format(text.data(), size);
		if (written <= 0)
		{
			return std::nullopt;
		}
		return std::u16string(text.data(), text.data() + written - 1);
	};
	const auto dateIn = [&now](DWORD flags) {
		return [&now, flags](wchar_t* text, int size) {
			return GetDateFormatW(LOCALE_USER_DEFAULT, flags, &now, nullptr, text, size);
		};
	};
	auto date = part(dateIn(DATE_LONGDATE));
	if (!date.has_value())
	{
		date = part(dateIn(DATE_SHORTDATE));
	}
	const auto time =
	    part([&now](wchar_t* text, int size) { return GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &now, nullptr, text, size); });
	return ecs::save_files::SaveDateLabel(time, date);
#else
	// Away from Windows, fixed formats of the time and of the long date take the place of the player's own
	const auto format = [&local](const char* pattern) {
		std::array<char, 128> text {};
		const auto length = std::strftime(text.data(), text.size(), pattern, &local);
		return gui::ToUtf16(std::string(text.data(), length));
	};
	return ecs::save_files::SaveDateLabel(format("%H:%M:%S"), format("%A, %d %B %Y"));
#endif
}

/// The full path of a folder in the profile folder. This does not make the folder: the writer makes it on its own
/// thread. None without a profile folder.
std::optional<std::filesystem::path> ProfilePath(const std::filesystem::path& relative)
{
	if (!Locator::profileFolderSystem::has_value())
	{
		return std::nullopt;
	}
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	return root.has_value() ? std::optional(*root / relative) : std::nullopt;
}

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

std::optional<std::span<const uint8_t>> View(const std::optional<std::vector<uint8_t>>& bytes)
{
	return bytes.has_value() ? std::optional<std::span<const uint8_t>>(*bytes) : std::nullopt;
}

/// A step that writes a mind file through the mind file component, from the copy of the mind that the game took
ecs::save_files::SaveStep MindStep(std::filesystem::path path, creaturemind::MindFileData mind)
{
	return [path = std::move(path), mind = std::make_shared<const creaturemind::MindFileData>(std::move(mind))](
	           filesystem::FileSystemInterface& /*files*/) {
		const bool written = creaturemind::WriteFile(path, *mind) == creaturemind::MindResult::Success;
		if (!written)
		{
			SPDLOG_LOGGER_WARN(Log(), "The game could not write {}", path.generic_string());
		}
		return written;
	};
}

/// A step that makes the pictures folder and copies the pictures of the temporary picture file into the slot's cells
ecs::save_files::SaveStep SlotPictureStep(std::filesystem::path pictures, uint32_t slot)
{
	return [pictures = std::move(pictures), slot](filesystem::FileSystemInterface& files) {
		if (!files.MakeFolder(pictures))
		{
			SPDLOG_LOGGER_WARN(Log(), "The game could not make the folder {}", pictures.generic_string());
			return false;
		}
		const auto largePath = pictures / gamesave::LargePictureFileName(slot);
		const auto smallPath = pictures / gamesave::SmallPictureFileName(slot);
		const auto temporary = ReadFile(pictures / gamesave::k_TemporaryPictureFileName);
		const auto large = ReadFile(largePath);
		const auto small = ReadFile(smallPath);
		const auto put = gamesave::PutSlotPictures(View(temporary), View(large), View(small), slot);
		const bool largeWritten = files.WriteAll(largePath, put.large);
		return files.WriteAll(smallPath, put.small) && largeWritten;
	};
}

/// The folder of the game that the player plays now, in the profile folder. None without a profile folder.
std::optional<std::filesystem::path> CurrentGameFolder()
{
	const auto player = PlayerFolder();
	return player.has_value() ? ProfilePath(*player / gamesave::k_CurrentGameFolder) : std::nullopt;
}

/// A step that copies the challenge picture files of the game that the player plays now into the slot's folder. A file
/// that is not there is not copied, and the file of the same name in the slot stays. The game does not look at the
/// result, so the step does not count a failure.
ecs::save_files::SaveStep ChallengePictureCopyStep(std::filesystem::path from, std::filesystem::path to)
{
	return [from = std::move(from), to = std::move(to)](filesystem::FileSystemInterface& files) {
		for (const auto& name : gamesave::ChallengePictureFileNames())
		{
			if (const auto bytes = ReadFile(from / name); bytes.has_value() && !files.WriteAll(to / name, *bytes))
			{
				SPDLOG_LOGGER_WARN(Log(), "The game could not copy the challenge picture {}", (to / name).generic_string());
			}
		}
		return true;
	};
}

/// The heading of a thing that its turn about the vertical gives
float HeadingOf(const glm::mat3& rotation)
{
	return portable_math::Atan2(rotation[2][0], rotation[2][2]);
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
	object.object.creationIndex = body.creationIndex;
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
			kept.length = worn.slackLength;
			kept.fullLength = worn.maxLength;
		}
		// The turn that a leash was last tied
		kept.word34 = leash->lastTiedTurn;
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
		transform->rotation = portable_math::EulerAngleY(saved.heading);
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
		leashes.RestoreTie(entity, *tied, kept.word34);
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
	if (!Locator::landSystem::has_value())
	{
		return false;
	}
	auto saved = std::string(landscape);
	std::ranges::replace(saved, '\\', '/');
	const auto savedName = std::filesystem::path(saved).filename().string();
	auto open = Locator::landSystem::value().GetLandscapePath().generic_string();
	std::ranges::replace(open, '\\', '/');
	const auto openName = std::filesystem::path(open).filename().string();
	return std::ranges::equal(savedName, openName, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}
} // namespace

ecs::save_files::BackgroundWriter& SaveGameSystem::Writer() const
{
	if (!_writer)
	{
		_writer = std::make_unique<ecs::save_files::BackgroundWriter>(
		    [](const std::string& name) { SPDLOG_LOGGER_WARN(Log(), "The game could not write {}", name); });
	}
	return *_writer;
}

void SaveGameSystem::ShowSaveBox(uint32_t slot, bool loading) const
{
	if (_showBox)
	{
		_showBox(loading, gui::saving_box::DarkensScreen(slot));
	}
}

void SaveGameSystem::FlushWrites() const
{
	if (_writer)
	{
		_writer->Flush();
	}
}

bool SaveGameSystem::Save(uint32_t slot, std::u16string_view name)
{
	// The save game room lists the slot and counts the save after it, so it waits until the writer has written the
	// files. A save whose temporary world file did not open is not counted.
	const auto outcome = SaveSlot(slot, name, true);
	if (outcome == nullptr)
	{
		return false;
	}
	FlushWrites();
	return outcome->load() != ecs::save_files::SaveWrite::NotOpened;
}

std::shared_ptr<const std::atomic<ecs::save_files::SaveWrite>> SaveGameSystem::SaveSlot(uint32_t slot, std::u16string_view name,
                                                                                        bool putPicture)
{
	const auto player = PlayerFolder();
	const auto playerPath = player.has_value() ? ProfilePath(*player) : std::nullopt;
	if (!gamesave::IsSlot(slot) || !playerPath.has_value() || !Locator::filesystem::has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "The game cannot save into slot {}: no slot or no player folder", slot);
		return nullptr;
	}
	// The box shows first, as a frame of its own, and stays on the screen while the game saves
	ShowSaveBox(slot, false);
	const auto started = std::chrono::steady_clock::now();
	auto& registry = Locator::entitiesRegistry::value();
	const auto land = LandNumber();
	// The game thread only takes copies of the game. The writer makes the folders and writes the files on its own
	// thread, in the order that the game writes them.
	ecs::save_files::SlotWrite files;
	const auto slotFolder = *playerPath / gamesave::SlotFolder(slot, land);
	const auto mindFolder = *playerPath / gamesave::MindFolder(slot, land);
	files.folders.push_back(slotFolder);
	if (mindFolder != slotFolder)
	{
		files.folders.push_back(mindFolder);
	}

	gamesave::WorldFile world;
	gamesave::GameChunk game {.land = land,
	                          .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u,
	                          .landscape = {},
	                          .knownLeashes = 0,
	                          .creatureInTemple = 0,
	                          .lastAutosaveTurn = _autosaveTurnToSave,
	                          .lastQuickSaveTurn = _lastQuickSaveTurn};
	if (Locator::landSystem::has_value())
	{
		const auto& lands = Locator::landSystem::value();
		game.landscape = lands.GetLandPath().generic_string();
		game.landscapeFile = lands.GetLandscapePath().generic_string();
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
			if (auto mind = Locator::creatureMindSystem::value().SaveMind(entity); mind.has_value())
			{
				files.withWorld.push_back(
				    MindStep(mindFolder / gamesave::SavedCreatureFileName(creature.creature.creature.index), std::move(*mind)));
			}
		}
		saved.push_back(std::move(creature));
		savedEntities.push_back(entity);
	}
	const auto chunks = gamesave::ToChunks(saved);
	const auto creaturesDone = std::chrono::steady_clock::now();
	world.chunks.insert(world.chunks.end(), chunks.begin(), chunks.end());
	// Then the whole world in openblack's own layout
	ecs::world_save::SaveSummary summary;
	auto whole =
	    std::make_shared<std::vector<ecs::world_save::PendingChunk>>(ecs::world_save::SaveWorld(Reflection(), summary));
	const auto worldBlocksDone = std::chrono::steady_clock::now();
	// A game that came from a file of the game also goes back into that file's layout: the file as it was, with the
	// records of its creatures and their leashes as openblack has them now
	const bool vanillaLayout = _vanilla.has_value() && _vanilla->land == land;
	auto vanilla =
	    vanillaLayout ? VanillaFiles(slot, slotFolder, saved, savedEntities) : std::vector<ecs::save_files::SaveFile> {};
	// The game loads the script machine from the file beside the world file. openblack does not write it yet, so the
	// save keeps the file of the game's save as it was. The writer reads that file after it wrote the files before it.
	std::optional<std::pair<std::filesystem::path, std::filesystem::path>> scriptCopy;
	if (vanillaLayout)
	{
		scriptCopy.emplace(_vanilla->folder / gamesave::VanillaScriptFileName(_vanilla->slot),
		                   slotFolder / gamesave::VanillaScriptFileName(slot));
	}

	// The challenge picture files go into the slot first. Then the slot's pictures come from the temporary picture
	// file, as the screen was when the game last took a picture.
	auto challengePictures = ChallengePictureCopyStep(*playerPath / gamesave::k_CurrentGameFolder, slotFolder);
	auto slotPictures = putPicture
	                        ? SlotPictureStep(*playerPath / gamesave::k_SavedGamesFolder / gamesave::k_PicturesFolder, slot)
	                        : ecs::save_files::SaveStep {};
	files.pictures = [challengePictures = std::move(challengePictures),
	                  slotPictures = std::move(slotPictures)](filesystem::FileSystemInterface& fileSystem) {
		std::ignore = challengePictures(fileSystem);
		return !slotPictures || slotPictures(fileSystem);
	};
	const gamesave::SaveGameInfo info {.slot = slot, .date = DateNow(), .name = std::u16string(name)};
	files.info = {.path = slotFolder / gamesave::k_InfoFileName, .bytes = gamesave::Write(info)};
	files.world = slotFolder / gamesave::WorldFileName(slot);
	// The writer makes the last blocks and puts the blocks together into the file on its own thread
	files.worldFiles = [path = files.world, world = std::make_shared<gamesave::WorldFile>(std::move(world)), whole,
	                    vanilla = std::make_shared<std::vector<ecs::save_files::SaveFile>>(std::move(vanilla)), scriptCopy]() {
		for (const auto& pending : *whole)
		{
			world->chunks.push_back({.tag = pending.tag, .bytes = pending.bytes()});
		}
		whole->clear();
		std::vector<ecs::save_files::SaveFile> written {{.path = path, .bytes = gamesave::Write(*world)}};
		std::ranges::move(*vanilla, std::back_inserter(written));
		if (scriptCopy.has_value())
		{
			if (auto script = ReadFile(scriptCopy->first); script.has_value())
			{
				written.push_back({.path = scriptCopy->second, .bytes = std::move(*script)});
			}
		}
		return written;
	};

	auto outcome = std::make_shared<std::atomic<ecs::save_files::SaveWrite>>(ecs::save_files::SaveWrite::NotOpened);
	auto* fileSystem = &Locator::filesystem::value();
	Writer().Queue(std::format("the files of slot {}", slot), [fileSystem,
	                                                           files = std::make_shared<const ecs::save_files::SlotWrite>(
	                                                               std::move(files)),
	                                                           outcome, problems = _writeProblems, slot]() {
		const auto writeStarted = std::chrono::steady_clock::now();
		const auto result = ecs::save_files::WriteSlot(*fileSystem, *files);
		outcome->store(result.world);
		if (result.world == ecs::save_files::SaveWrite::NotOpened)
		{
			SPDLOG_LOGGER_WARN(Log(), "The temporary world file of slot {} did not open; the game wrote no world file", slot);
		}
		else if (result.world == ecs::save_files::SaveWrite::Failed)
		{
			// The player is told on the game's thread
			problems->fetch_add(1);
		}
		SPDLOG_LOGGER_INFO(Log(), "The writer did the work of slot {} in {:.1f} ms", slot,
		                   std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - writeStarted).count());
		return result.world == ecs::save_files::SaveWrite::Written && result.otherFailures == 0;
	});
	const auto ms = [](auto from, auto to) { return std::chrono::duration<float, std::milli>(to - from).count(); };
	const auto ended = std::chrono::steady_clock::now();
	SPDLOG_LOGGER_INFO(Log(),
	                   "Saved the game into slot {} with {} creatures in {:.1f} ms on the game's thread (creatures {:.1f}, "
	                   "world blocks {:.1f}, rest {:.1f})",
	                   slot, creatures.size(), ms(started, ended), ms(started, creaturesDone),
	                   ms(creaturesDone, worldBlocksDone), ms(worldBlocksDone, ended));
	return outcome;
}

std::vector<ecs::save_files::SaveFile> SaveGameSystem::VanillaFiles(uint32_t slot, const std::filesystem::path& slotFolder,
                                                                    std::span<const gamesave::SavedCreature> saved,
                                                                    std::span<const entt::entity> entities)
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
	SPDLOG_LOGGER_INFO(Log(), "Wrote {} records into the game's file of slot {}", edits.size(), slot);
	return {{.path = slotFolder / gamesave::VanillaWorldFileName(slot), .bytes = std::move(bytes)}};
}

void SaveGameSystem::ReportWriteProblems()
{
	// The game tells the player once for each save, in a box with an OK
	for (auto count = _writeProblems->exchange(0); count > 0; --count)
	{
		SPDLOG_LOGGER_WARN(Log(), "A save did not write its world files completely; the old files stay");
		if (_reportProblem)
		{
			_reportProblem();
		}
	}
}

bool SaveGameSystem::Load(uint32_t slot)
{
	FlushWrites();
	const auto player = PlayerFolder();
	if (!gamesave::IsSlot(slot) || !player.has_value())
	{
		return false;
	}
	// Each load starts a new game, which shows the tips screen while it loads. The game shows its loading box only
	// outside the start of a game, so a load never shows it.
	const auto land = LandNumber();
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	// The previous land's save is in the folder of the land before, the next land's in the folder of the land after
	const auto slotFolder = *root / *player / gamesave::LoadSlotFolder(slot, land);
	const auto mindFolder = *root / *player / gamesave::MindFolder(slot, land);
	// Each load first copies the challenge picture files of the slot, also when the slot has no world file
	const auto copiedPictures = CopyChallengePicturesFrom(slotFolder);
	const auto vanillaBytes = ReadFile(slotFolder / gamesave::VanillaWorldFileName(slot));
	const auto bytes = ReadFile(slotFolder / gamesave::WorldFileName(slot));
	if (!bytes.has_value())
	{
		// A slot of the game itself: its world file in the game's layout
		const bool loaded = vanillaBytes.has_value() && LoadVanilla(slot, slotFolder, mindFolder, *vanillaBytes);
		if (loaded)
		{
			ShowChallengePictures(copiedPictures);
		}
		return loaded;
	}
	const auto worldPath = slotFolder / gamesave::WorldFileName(slot);
	const auto world = gamesave::ReadWorldFile(*bytes);
	if (!world.has_value())
	{
		// The game would read the file as far as it goes. openblack does not read a file that it cannot read, and does
		// what the game does with a load that it refuses.
		SPDLOG_LOGGER_WARN(Log(), "Slot {} has no world file that the game can read", slot);
		StartLandAgain();
		return false;
	}
	// A file that stopped the game two times as it loaded does not load again. Each load first adds one to the file's
	// count, and puts it back to none when it has read the world.
	const bool counts = world->version >= gamesave::k_AttemptsVersion;
	if (counts && !gamesave::MayLoad(world->attempts))
	{
		SPDLOG_LOGGER_WARN(Log(), "Slot {} stopped the game {} times as it loaded; the game does not load it again", slot,
		                   world->attempts);
		StartLandAgain();
		return false;
	}
	auto& files = Locator::filesystem::value();
	if (counts)
	{
		std::ignore = files.WriteAt(worldPath, gamesave::k_AttemptsOffset, gamesave::AttemptsBytes(world->attempts + 1));
	}
	const auto gameChunk = std::ranges::find(world->chunks, gamesave::k_GameTag, &gamesave::Chunk::tag);
	const auto game = gameChunk != world->chunks.end() ? gamesave::ReadGameChunk(gameChunk->bytes) : std::nullopt;
	// A save of the whole world loads its own land, whatever land is open
	if (game.has_value() && ecs::world_save::HasWholeWorld(*world))
	{
		if (!LoadWholeWorld(slot, *world, *game))
		{
			StartLandAgain();
			return false;
		}
		ShowChallengePictures(copiedPictures);
		if (counts)
		{
			std::ignore = files.WriteAt(worldPath, gamesave::k_AttemptsOffset, gamesave::AttemptsBytes(0));
		}
		return true;
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
	if (counts)
	{
		std::ignore = files.WriteAt(worldPath, gamesave::k_AttemptsOffset, gamesave::AttemptsBytes(0));
	}
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

std::vector<uint8_t> SaveGameSystem::WorldState()
{
	auto& registry = Locator::entitiesRegistry::value();
	gamesave::WorldFile world;
	gamesave::GameChunk game {.land = LandNumber(),
	                          .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u,
	                          .landscape = {},
	                          .knownLeashes = 0,
	                          .creatureInTemple = 0,
	                          .lastAutosaveTurn = _lastAutosaveTurn,
	                          .lastQuickSaveTurn = _lastQuickSaveTurn};
	if (Locator::landSystem::has_value())
	{
		const auto& lands = Locator::landSystem::value();
		game.landscape = lands.GetLandPath().generic_string();
		game.landscapeFile = lands.GetLandscapePath().generic_string();
	}
	world.chunks.push_back(gamesave::ToChunk(game));
	// The records of the creatures, as a save keeps them
	const auto creatures = CreaturesInOrder();
	std::vector<gamesave::SavedCreature> saved;
	for (uint32_t index = 0; index < creatures.size(); ++index)
	{
		saved.push_back(ToSave(registry, creatures[index], index));
	}
	const auto chunks = gamesave::ToChunks(saved);
	world.chunks.insert(world.chunks.end(), chunks.begin(), chunks.end());
	// Then the whole world in openblack's own layout. This computer makes all the blocks now, with no writer thread.
	ecs::world_save::SaveSummary summary;
	for (const auto& pending : ecs::world_save::SaveWorld(Reflection(), summary))
	{
		world.chunks.push_back({.tag = pending.tag, .bytes = pending.bytes()});
	}
	return gamesave::Write(world);
}

bool SaveGameSystem::LoadWorldState(std::span<const uint8_t> bytes)
{
	FlushWrites();
	const auto world = gamesave::ReadWorldFile(bytes);
	if (!world.has_value() || !ecs::world_save::HasWholeWorld(*world))
	{
		SPDLOG_LOGGER_WARN(Log(), "The world state has no whole world that the game can read");
		return false;
	}
	const auto gameChunk = std::ranges::find(world->chunks, gamesave::k_GameTag, &gamesave::Chunk::tag);
	const auto game = gameChunk != world->chunks.end() ? gamesave::ReadGameChunk(gameChunk->bytes) : std::nullopt;
	if (!game.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "The world state has no game block");
		return false;
	}
	return LoadWholeWorld(0, *world, *game, ecs::world_save::LoadKind::WorldState);
}

void SaveGameSystem::StartLandAgain() const
{
	if (!Locator::landSystem::has_value() || Locator::landSystem::value().GetLandPath().empty())
	{
		return;
	}
	auto& lands = Locator::landSystem::value();
	SPDLOG_LOGGER_WARN(Log(), "The load did not happen; the land {} starts again as a new game",
	                   lands.GetLandPath().generic_string());
	const auto land = lands.GetLandPath();
	std::ignore = lands.LoadMapWithFreshScripts(land, land_change::Kind::NewGame);
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

bool SaveGameSystem::LoadWholeWorld(uint32_t slot, const gamesave::WorldFile& world, const gamesave::GameChunk& game,
                                    ecs::world_save::LoadKind kind)
{
	if (!Locator::landSystem::has_value() || (game.landscapeFile.empty() && game.landscape.empty()))
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
	const bool loaded = Locator::landSystem::value().LoadSavedLand(
	    game.landscape, game.landscapeFile,
	    [&reflection, &world, kind]() { return ecs::world_save::LoadEntities(reflection, world, kind).has_value(); },
	    kind == ecs::world_save::LoadKind::WorldState ? LandSystemInterface::SavedLandUse::WorldState
	                                                  : LandSystemInterface::SavedLandUse::SavedGame);
	if (!loaded)
	{
		SPDLOG_LOGGER_WARN(Log(), "The world of slot {} did not load", slot);
		return false;
	}
	// The clock, the random numbers and the scripts go on from the saved turn once the land has started
	if (!ecs::world_save::LoadRunningState(reflection, world, game.turn))
	{
		SPDLOG_LOGGER_WARN(Log(), "A part of the running state of slot {} did not read", slot);
	}
	// A load from a file of the game is not the origin of this world
	_vanilla.reset();
	// The next autosave and the next quick load come only after their full gap from the turn of the load
	_lastAutosaveTurn = game.turn;
	_lastLoadTurn = game.turn;
	_lastQuickSaveTurn = game.lastQuickSaveTurn;
	_autosaveTurnToSave = game.lastAutosaveTurn;
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
	const bool world = ImportVanillaWorld(slot, bytes);
	return !matched.empty() || world;
}

bool SaveGameSystem::ImportVanillaWorld(uint32_t slot, std::span<const uint8_t> bytes)
{
	// The game reads its whole world from the file. openblack reads the whole file, but it takes only some parts of
	// the world from it.
	const auto read = gamesave::ReadVanillaWorld(bytes);
	if (!read.world.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "The game's file of slot {} does not read as a whole world at byte {}: {}", slot,
		                   read.error->at, read.error->reason);
		return false;
	}
	const auto data = read.world->GameData();
	SPDLOG_LOGGER_INFO(Log(), "The game's file of slot {} has {} objects at turn {}", slot, read.world->Records().size(),
	                   data.has_value() ? data->gameTurn : 0);
	// The towns and the villagers of the file replace the towns and the villagers of the land
	ImportVanillaTowns(*read.world);
	// The game data of the file has the two seeds of the random draws. After the load, the draws continue from them
	// as the game's draws do. The creatures and the towns are made first, because the creation of a thing can draw.
	if (data.has_value() && Locator::gameRandom::has_value())
	{
		Locator::gameRandom::value().SetSeeds({.synced = data->randomSeed, .local = data->localSeed});
	}
	return true;
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
	FlushWrites();
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
	// The writer can still write a picture file
	FlushWrites();
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

/// A network game never saves: not by itself, not by the quick keys and not as the player leaves it
bool NetworkGame()
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

/// The name of a save that the game does by itself into the slot
std::u16string InstantName(uint32_t slot)
{
	const auto text = Locator::helpTextSystem::has_value()
	                      ? Locator::helpTextSystem::value().GetWords(gamesave::k_SlotNameTexts.at(slot))
	                      : std::u16string {};
	return gamesave::InstantSaveName(text, LandNumber());
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
	const bool written = SaveSlot(slot, InstantName(slot), false) != nullptr;
	AskForPicture({slot});
	return written;
}

bool SaveGameSystem::SaveOnExit(bool quitting)
{
	const auto& context = Locator::entitiesRegistry::value().Context();
	// No save as the player leaves the testbed, the Gods' Playground, a skirmish, a network game, or a game that is over
	const auto* lands = Locator::landSystem::has_value() ? &Locator::landSystem::value() : nullptr;
	if (lands == nullptr || lands->GetLandPath().empty() || lands->GetLandPath() == ecs::systems::k_TestbedLandPath ||
	    context.gameOver || context.skirmish || NetworkGame() || LandNumber() == gamesave::k_PlaygroundLand)
	{
		return false;
	}
	if (!quitting)
	{
		// Another game starts next. The picture is the screen of the last frame of this game.
		const bool written = SaveSlot(gamesave::k_ExitSlot, InstantName(gamesave::k_ExitSlot), false) != nullptr;
		AskForPicture({gamesave::k_ExitSlot});
		return written;
	}
	// The game makes the screen black before it saves, so it takes a picture of a black screen
	const std::vector<uint8_t> black(
	    static_cast<size_t>(gamesave::k_ScreenPictureWidth) * gamesave::k_ScreenPictureHeight * gamesave::k_TexelSize, 0);
	WriteTemporaryPicture(black);
	// The game closes next: every file is on the disk before it does
	const auto outcome = SaveSlot(gamesave::k_ExitSlot, InstantName(gamesave::k_ExitSlot), true);
	FlushWrites();
	return outcome != nullptr && outcome->load() != ecs::save_files::SaveWrite::NotOpened;
}

bool SaveGameSystem::HasExitSave() const
{
	FlushWrites();
	const auto player = PlayerFolder();
	if (!player.has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	const auto root = Locator::profileFolderSystem::value().GetFolder();
	if (!root.has_value())
	{
		return false;
	}
	// The slot of the save on exit has one folder for all the lands
	const auto path =
	    *root / *player / gamesave::SlotFolder(gamesave::k_ExitSlot, 0) / gamesave::WorldFileName(gamesave::k_ExitSlot);
	auto& files = Locator::filesystem::value();
	if (!files.Exists(path))
	{
		return false;
	}
	try
	{
		return files.Open(path, openblack::filesystem::Stream::Mode::Read) != nullptr;
	}
	catch (const std::exception&)
	{
		return false;
	}
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
	    .networkGame = NetworkGame(),
	    .land = LandNumber(),
	    .turn = TurnNow(),
	    .lastAutosaveTurn = _lastAutosaveTurn,
	};
	if (!gamesave::AutosaveDue(state))
	{
		return;
	}
	_lastAutosaveTurn = state.turn;
	_autosaveTurnToSave = state.turn;
	// The autosaves of each land go in turn into the three autosave slots
	const auto count =
	    Locator::playerProfileSystem::has_value() ? Locator::playerProfileSystem::value().TakeLandAutosave(state.land) : 0u;
	SPDLOG_LOGGER_INFO(Log(), "Autosave at turn {} into slot {}", state.turn, gamesave::AutosaveSlot(count));
	InstantSave(gamesave::AutosaveSlot(count));
	// The profile file keeps the new count after the save, as in the game. The writer writes it after the save's files.
	if (Locator::playerProfileSystem::has_value())
	{
		if (auto job = Locator::playerProfileSystem::value().ProfilesFileJob(); job)
		{
			Writer().Queue("the player profiles", std::move(job));
		}
	}
}

bool SaveGameSystem::PressQuickSave()
{
	// The key checks the rules and keeps its turn. The save comes at the turn of the command.
	const auto turn = TurnNow();
	if (!gamesave::QuickTurnAllowed(turn, _lastQuickSaveTurn, GameOver(), ScriptCutScene(), NetworkGame()))
	{
		return false;
	}
	_lastQuickSaveTurn = turn;
	return true;
}

bool SaveGameSystem::PressQuickLoad()
{
	return gamesave::QuickTurnAllowed(TurnNow(), _lastLoadTurn, GameOver(), ScriptCutScene(), NetworkGame());
}

bool SaveGameSystem::DoQuickSave()
{
	// A network game never sends the command. The game of another computer does not write the files of this computer.
	if (NetworkGame())
	{
		return false;
	}
	return InstantSave(gamesave::k_QuickSaveSlot);
}

void SaveGameSystem::DoQuickLoad()
{
	if (NetworkGame())
	{
		return;
	}
	// The rest of the turn and the draw of the frame use the world that the player has now. The game loads the slot
	// at the end of the frame.
	_askedLoad = gamesave::k_QuickSaveSlot;
}

bool SaveGameSystem::LoadAskedSlot()
{
	const auto slot = std::exchange(_askedLoad, std::nullopt);
	return slot.has_value() && Load(*slot);
}

void SaveGameSystem::NewLand()
{
	_lastAutosaveTurn = 0;
	// The list of the components that a save writes is made while the land loads. Thus the first save of a game, as when
	// the scripts finish a temple, does not make it in a frame of the game.
	static_cast<void>(Reflection());
	_autosaveTurnToSave = 0;
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
	// The renderer takes the picture at the end of a draw, so the request goes to the main thread
	PostToMainThread(MainThreadApi::Renderer, [name] {
		Locator::rendererInterface::value().RequestScreenPicture(name, [](const graphics::ScreenView& screen) {
			return gamesave::SampleScreen({.width = screen.width,
			                               .height = screen.height,
			                               .pitch = screen.pitch,
			                               .bottomUp = screen.yflip,
			                               .bgra = screen.bgra})
			    .value_or(std::vector<uint8_t> {});
		});
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
	const auto pictures = folder.has_value() ? ProfilePath(*folder) : std::nullopt;
	if (!pictures.has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	// The writer makes the folder, reads the old picture file and writes the new one on its own thread, after the files
	// before it
	auto* files = &Locator::filesystem::value();
	const auto path = *pictures / gamesave::k_TemporaryPictureFileName;
	Writer().Queue(path.generic_string(), [files, folder = *pictures, path,
	                                       screen = std::vector<uint8_t>(screenPicture.begin(), screenPicture.end())]() {
		if (!files->MakeFolder(folder))
		{
			return false;
		}
		const auto old = ReadFile(path);
		return files->WriteAll(path, gamesave::MakeTemporaryPicture(View(old), screen));
	});
}

void SaveGameSystem::PutSlotPicture(uint32_t slot)
{
	const auto folder = PicturesFolder();
	const auto pictures = folder.has_value() ? ProfilePath(*folder) : std::nullopt;
	if (!pictures.has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	// The writer copies the temporary picture into the slot's cells on its own thread, after the files before it
	auto* files = &Locator::filesystem::value();
	Writer().Queue((*pictures / gamesave::LargePictureFileName(slot)).generic_string(),
	               [files, step = SlotPictureStep(*pictures, slot)]() { return step(*files); });
}

void SaveGameSystem::ReadChallengePictures()
{
	const auto folder = CurrentGameFolder();
	if (_challengePicturesRead && folder == _challengeFolder)
	{
		return;
	}
	_challengePictures.Clear();
	_challengeFolder = folder;
	_challengePicturesRead = true;
	if (!folder.has_value())
	{
		return;
	}
	for (const auto& name : gamesave::ChallengePictureFileNames())
	{
		_challengePictures.Set(name, ReadFile(*folder / name));
	}
}

void SaveGameSystem::PutChallengePicture(uint32_t place, std::span<const uint8_t> picture)
{
	ReadChallengePictures();
	// A place past the files that a save copies reads its files the first time that it uses them
	const std::array names {gamesave::ChallengeLargePictureFileName(place), gamesave::ChallengeSmallPictureFileName(place)};
	for (const auto& name : names)
	{
		if (!_challengePictures.File(name).has_value() && _challengeFolder.has_value())
		{
			_challengePictures.Set(name, ReadFile(*_challengeFolder / name));
		}
	}
	std::ignore = _challengePictures.Put(place, picture);
	if (!_challengeFolder.has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	// The writer makes the folder and writes the two files on its own thread, after the files before them
	auto* files = &Locator::filesystem::value();
	for (const auto& name : names)
	{
		const auto bytes = _challengePictures.File(name);
		Writer().Queue((*_challengeFolder / name).generic_string(),
		               [files, folder = *_challengeFolder, path = *_challengeFolder / name,
		                bytes = std::vector<uint8_t>(bytes->begin(), bytes->end())]() {
			               return files->MakeFolder(folder) && files->WriteAll(path, bytes);
		               });
	}
}

gamesave::ChallengePictureCells SaveGameSystem::ChallengePictureCells(uint32_t place)
{
	ReadChallengePictures();
	return _challengePictures.Cells(place);
}

std::vector<std::string> SaveGameSystem::CopyChallengePicturesFrom(const std::filesystem::path& slotFolder)
{
	ReadChallengePictures();
	std::vector<std::string> copied;
	if (!_challengeFolder.has_value() || !Locator::filesystem::has_value())
	{
		return copied;
	}
	auto& files = Locator::filesystem::value();
	if (!files.MakeFolder(*_challengeFolder))
	{
		SPDLOG_LOGGER_WARN(Log(), "The game could not make the folder {}", _challengeFolder->generic_string());
	}
	// A file that the slot does not have is not copied, and the file of the game that the player plays now stays
	for (const auto& name : gamesave::ChallengePictureFileNames())
	{
		auto bytes = ReadFile(slotFolder / name);
		if (!bytes.has_value())
		{
			continue;
		}
		if (!files.WriteAll(*_challengeFolder / name, *bytes))
		{
			SPDLOG_LOGGER_WARN(Log(), "The game could not copy the challenge picture {}",
			                   (*_challengeFolder / name).generic_string());
		}
		_challengePictures.Set(name, std::move(bytes));
		copied.push_back(name);
	}
	return copied;
}

void SaveGameSystem::ShowChallengePictures(const std::vector<std::string>& copied)
{
	if (copied.empty())
	{
		return;
	}
	// A record whose files the slot did not have keeps the picture of openblack's own save
	const auto isCopied = [&copied](const std::string& name) { return std::ranges::find(copied, name) != copied.end(); };
	ecs::challenge_pictures::Refresh(Locator::entitiesRegistry::value(),
	                                 {.put = {}, .cells = [this, &isCopied](uint32_t place) {
		                                  auto cells = _challengePictures.Cells(place);
		                                  if (!isCopied(gamesave::ChallengeLargePictureFileName(place)))
		                                  {
			                                  cells.large.clear();
		                                  }
		                                  if (!isCopied(gamesave::ChallengeSmallPictureFileName(place)))
		                                  {
			                                  cells.small.clear();
		                                  }
		                                  return cells;
	                                  }});
}
