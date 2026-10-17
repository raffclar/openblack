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

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include <CreatureRecord.h>
#include <LHVM.h>
#include <MindFile.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <WorldFile.h>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "Common/PlayerProfiles.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureSaveState.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
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

/// The creature part of the record and the parts that openblack keeps of what every object writes
gamesave::CreatureChunk Capture(ecs::Registry& registry, entt::entity entity, uint32_t index)
{
	gamesave::CreatureChunk chunk;
	chunk.self = save_state::ToRef(entity);
	const auto& body = registry.Get<const Creature>(entity);
	if (const auto* transform = registry.TryGet<const Transform>(entity))
	{
		chunk.position = {transform->position.x, transform->position.y, transform->position.z};
		chunk.heading = HeadingOf(transform->rotation);
	}
	auto& record = chunk.record;
	record.savedInTemple = Locator::temple::has_value() && Locator::temple::value().Active() ? 1 : 0;
	record.index = static_cast<uint8_t>(std::min<uint32_t>(index, gamesave::k_None));
	record.owner = save_state::PlayerNumber(body.owner);
	auto* spells = registry.TryGet<CreatureSpells>(entity);
	if (spells != nullptr)
	{
		std::ranges::transform(spells->spells.slots, record.spells.begin(), save_state::ToSlot);
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
	const auto saved = spells != nullptr ? save_state::FizzToSave(now, spells->spells) : save_state::FizzToSave(now, {});
	if (saved != now)
	{
		if (fizz != nullptr)
		{
			fizz->fizz = saved;
		}
		if (spells != nullptr)
		{
			spells->fizz = saved.now;
		}
	}
	record.fizzedForGood = saved.goesForGood ? 1 : 0;
	record.fizz = saved.now;
	record.fizzTarget = saved.target;
	record.fizzPerSecond = saved.perSecond;
	const auto* leash = registry.TryGet<const CreatureLeash>(entity);
	if (leash != nullptr && leash->home.has_value())
	{
		record.home = {leash->home->x, leash->home->y, leash->home->z};
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
			return id.has_value() ? save_state::ToRef(static_cast<entt::entity>(*id)) : 0u;
		};
		kept.objects = {ref(plan.activityObject), ref(plan.object), ref(plan.instrument)};
		// openblack builds the agenda again from the plan. The save keeps the step that the creature does.
		kept.agenda = {.word08 = 1, .current = 0, .state = 0, .count = 0, .creature = chunk.self};
		chunk.agendaStep = static_cast<uint32_t>(mind->idle.step);
		record.plan = std::move(kept);
	}
	if (leash != nullptr && leash->worn.has_value())
	{
		const auto& worn = *leash->worn;
		const auto lengths = creature_leash::InHand(body.size);
		chunk.leash = gamesave::LeashRecord {.on = 1,
		                                     .works = worn.works ? 1u : 0u,
		                                     .type = save_state::LeashTypeNumber(worn.type),
		                                     .player = save_state::PlayerNumber(worn.holder),
		                                     .tiedTo = save_state::ToRef(worn.tiedTo),
		                                     .length = lengths.slack,
		                                     .fullLength = lengths.max};
	}
	return chunk;
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
	const auto bytes = path.has_value() ? ReadFile(*path) : std::nullopt;
	MindForLoad found {.resolves = save_state::ResolvesOnLoad(bytes.has_value() ? std::optional(bytes->size()) : std::nullopt)};
	if (!found.resolves)
	{
		return found;
	}
	auto mind = std::make_shared<creaturemind::MindFileData>();
	if (creaturemind::Read(*bytes, *mind) != creaturemind::MindResult::Success)
	{
		SPDLOG_LOGGER_WARN(Log(), "The mind file {} could not be read", path->generic_string());
		return found;
	}
	found.mind = std::move(mind);
	return found;
}

/// Puts the creature back as the save keeps it: the values of its record, then the mind of its mind file over them,
/// then the plan that the save kept, then its leash
void Restore(ecs::Registry& registry, entt::entity entity, const gamesave::CreatureChunk& chunk,
             const std::filesystem::path& mindFolder)
{
	const auto& record = chunk.record;
	auto& body = registry.Get<Creature>(entity);
	// It starts as a new creature does, which takes notice of every reaction
	registry.Remove<CreatureReactionsOff>(entity);
	if (auto* transform = registry.TryGet<Transform>(entity))
	{
		transform->position = {chunk.position[0], chunk.position[1], chunk.position[2]};
		transform->rotation = glm::eulerAngleY(chunk.heading);
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
	if (auto* leash = registry.TryGet<CreatureLeash>(entity); leash != nullptr && record.home != gamesave::Point {})
	{
		leash->home = glm::vec3(record.home[0], record.home[1], record.home[2]);
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
		for (const auto partner : record.dancePartners)
		{
			if (const auto dancer = save_state::FromRef(partner); dancer.has_value() && registry.Valid(*dancer))
			{
				dance->dancers.push_back(*dancer);
			}
		}
		const auto saved = save_state::FromRef(record.dance);
		dance->dance = saved.has_value() && registry.Valid(*saved) ? *saved : entt::null;
	}
	body.canDie = record.canDie != 0;
	body.strength = record.strength;
	body.size = record.size;
	body.alignment = record.alignment;
	body.autoScale = record.autoScale != 0 ? std::optional(record.autoScaleShare) : std::nullopt;

	// The mind file's physique and mind come over the record's values. When the mind file is missing or empty, the
	// game does not put the creature back on the map, and the creature is lost from the land.
	const auto found = MindToLoad(record, mindFolder);
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
			const auto& saved = *record.plan;
			plan = CreatureMindSystemInterface::RestoredPlan {
			    .desire = static_cast<creature_desires::Desire>(saved.desire),
			    .action = saved.action,
			    .object = save_state::FromRef(saved.objects[1]),
			    .activityObject = save_state::FromRef(saved.objects[0]),
			    .instrument = save_state::FromRef(saved.objects[2]),
			    .step = chunk.agendaStep,
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
	if (!chunk.leash.has_value() || chunk.leash->on == 0)
	{
		if (leashes.IsLeashed(entity))
		{
			leashes.TakeOff(entity);
		}
		return;
	}
	const auto type = save_state::LeashTypeFromNumber(chunk.leash->type).value_or(LeashType::Rope);
	if (!leashes.IsLeashed(entity))
	{
		leashes.PutOn(entity, type);
	}
	else if (leashes.TypeOf(entity) != type)
	{
		leashes.ChangeType(entity, type);
	}
	leashes.SetWorks(entity, chunk.leash->works != 0);
	if (const auto tied = save_state::FromRef(chunk.leash->tiedTo); tied.has_value() && registry.Valid(*tied))
	{
		leashes.TieTo(entity, *tied);
	}
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
	for (uint32_t index = 0; index < creatures.size(); ++index)
	{
		const auto entity = creatures[index];
		const auto chunk = Capture(registry, entity, index);
		SPDLOG_LOGGER_INFO(Log(), "Saved the creature {} at {:.1f}, {:.1f}; the save keeps its plan: {}",
		                   entt::to_integral(entity), chunk.position[0], chunk.position[2], chunk.record.plan.has_value());
		world.chunks.push_back(gamesave::ToChunk(chunk));
		// Each creature's mind goes in a mind file of its own beside the world file
		if (Locator::creatureMindSystem::has_value())
		{
			if (const auto mind = Locator::creatureMindSystem::value().SaveMind(entity); mind.has_value())
			{
				written = WriteFile(mindFolder, gamesave::SavedCreatureFileName(index), creaturemind::Write(*mind)) && written;
			}
		}
	}
	written = WriteFile(slotFolder, gamesave::WorldFileName(slot), gamesave::Write(world)) && written;
	const gamesave::SaveGameInfo info {.slot = slot, .date = DateNow(), .name = std::u16string(name)};
	written = WriteFile(slotFolder, gamesave::k_InfoFileName, gamesave::Write(info)) && written;
	SPDLOG_LOGGER_INFO(Log(), "Saved the game into slot {} with {} creatures{}", slot, creatures.size(),
	                   written ? "" : ", but a file could not be written");
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
	const auto bytes = ReadFile(slotFolder / gamesave::WorldFileName(slot));
	const auto world = bytes.has_value() ? gamesave::ReadWorldFile(*bytes) : std::nullopt;
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
	auto& registry = Locator::entitiesRegistry::value();
	const auto mindFolder = *root / *player / gamesave::MindFolder(slot, land);
	size_t restored = 0;
	for (const auto& chunk : world->chunks)
	{
		if (chunk.tag != gamesave::k_CreatureTag)
		{
			continue;
		}
		const auto creature = gamesave::ReadCreatureChunk(chunk.bytes);
		const auto entity = creature.has_value() ? save_state::FromRef(creature->self) : std::nullopt;
		if (!entity.has_value() || !registry.Valid(*entity) || !registry.AllOf<Creature>(*entity))
		{
			SPDLOG_LOGGER_WARN(Log(), "A creature of slot {} is not on the land", slot);
			continue;
		}
		Restore(registry, *entity, *creature, mindFolder);
		SPDLOG_LOGGER_INFO(Log(), "Put the creature {} back at {:.1f}, {:.1f}, at step {} of its plan",
		                   entt::to_integral(*entity), creature->position[0], creature->position[2],
		                   creature->record.plan.has_value() ? std::to_string(creature->agendaStep) : std::string("none"));
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
		if (!Locator::filesystem::value().Exists(folder / gamesave::WorldFileName(slot)))
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
