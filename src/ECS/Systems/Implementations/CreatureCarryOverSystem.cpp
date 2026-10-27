/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureCarryOverSystem.h"

#include <chrono>
#include <system_error>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <PhysiqueFile.h>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Sound.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFileSave.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "ECS/SaveFileWriter.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// A game turn's milliseconds
constexpr float k_TurnMilliseconds = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration).count();
/// A creature made from its file faces as one a land's set-up makes from a file does
constexpr float k_ArrivalYawDegrees = 180.0f;

PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

bool NetworkGame()
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

/// The player's creature: the first they got, the one they lead on the leash
std::optional<entt::entity> PlayersCreature()
{
	return Locator::leashSystem::has_value() ? Locator::leashSystem::value().PlayersCreature(LocalPlayer()) : std::nullopt;
}
} // namespace

CreatureCarryOverSystem::CreatureCarryOverSystem(std::optional<std::filesystem::path> folder, ProfileFile profileFile)
    : _folder(std::move(folder))
    , _profileFile(std::move(profileFile))
{
}

CreatureCarryOverSystem::~CreatureCarryOverSystem() = default;

void CreatureCarryOverSystem::FinishWrites() const
{
	if (_writer != nullptr)
	{
		_writer->Flush();
	}
}

std::optional<std::filesystem::path> CreatureCarryOverSystem::ProfilePhysique() const
{
	// The caller reads the file, so the files that the system wrote before go to the disk first
	FinishWrites();
	if (!_folder.has_value() || !_profileFile)
	{
		return std::nullopt;
	}
	const auto file = _profileFile();
	return file.has_value() ? std::optional(creature_carry_over::KeptFilesIn(*_folder, *file).physique) : std::nullopt;
}

std::optional<std::filesystem::path> CreatureCarryOverSystem::ProfileMind() const
{
	// The caller reads the file, so the files that the system wrote before go to the disk first
	FinishWrites();
	if (!_folder.has_value() || !_profileFile)
	{
		return std::nullopt;
	}
	const auto file = _profileFile();
	return file.has_value() ? std::optional(creature_carry_over::KeptFilesIn(*_folder, *file).mind) : std::nullopt;
}

std::optional<creature_carry_over::KeptFiles> CreatureCarryOverSystem::Files() const
{
	if (!_folder.has_value() || !_profileFile)
	{
		return std::nullopt;
	}
	// The game's own file, once a game has started; before that the profile picked
	const auto file = _gameFile.has_value() ? _gameFile : _profileFile();
	if (!file.has_value())
	{
		return std::nullopt;
	}
	return creature_carry_over::KeptFilesIn(*_folder, *file);
}

void CreatureCarryOverSystem::KeepPlayersCreature()
{
	// In a network game, the player's creature is the creature that the god of this computer brought
	if (const auto creature = NetworkGame() ? _brought : PlayersCreature(); creature.has_value())
	{
		KeepCreature(*creature);
	}
}

void CreatureCarryOverSystem::KeepCreature(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr || !Locator::creatureMindSystem::has_value() ||
	    !creature_carry_over::WritesToPlayersFiles(
	        {.networkGame = NetworkGame(), .localPlayers = body->owner == LocalPlayer(), .broughtHere = _brought == creature}))
	{
		return;
	}
	auto file = Locator::creatureMindSystem::value().SaveMind(creature);
	if (!file.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature {} has no mind yet to keep for the next land",
		                   entt::to_integral(creature));
		return;
	}
	// Its physique is saved beside its mind: its body as it is now, but for the alignment, as no spell made it
	const auto now = ecs::creature_body_file::Capture(registry, creature);
	const creature_spells::SavedBody nowValues {
	    .size = now.size.value_or(1.0f), .strength = now.strength, .alignment = now.alignment.value_or(0.0f)};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	const auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, nowValues) : nowValues;
	const auto physique = creature_mind_body::ToPhysiqueFile(now, file->speciesRow, saved.alignment);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Kept the player's creature {} for the next land", entt::to_integral(creature));
	auto kept = std::make_shared<const creaturemind::MindFileData>(std::move(*file));
	Write(kept, physique);
	Keep(std::move(kept));
}

void CreatureCarryOverSystem::Write(std::shared_ptr<const creaturemind::MindFileData> mind,
                                    creaturemind::PhysiqueFileData physique) const
{
	const auto kept = Files();
	if (!kept.has_value())
	{
		return;
	}
	if (_writer == nullptr)
	{
		_writer = std::make_unique<ecs::save_files::BackgroundWriter>();
	}
	// The game makes the contents now. The writer puts them on the disk later, in the order that they came.
	_writer->Queue(
	    kept->mind.string(), [folder = *_folder, files = *kept, mind = std::move(mind), physique = std::move(physique)] {
		    std::error_code error;
		    std::filesystem::create_directories(folder, error);
		    const auto written = creaturemind::WriteFile(files.mind, *mind);
		    if (written != creaturemind::MindResult::Success)
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature could not be written to {}: {}",
			                       files.mind.string(), creaturemind::ResultToStr(written));
			    return false;
		    }
		    if (!creaturemind::WritePhysiqueFile(files.physique, physique))
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature's physique could not be written to {}",
			                       files.physique.string());
			    return false;
		    }
		    SPDLOG_LOGGER_INFO(spdlog::get("game"), "Wrote the player's creature to {} and its physique beside it",
		                       files.mind.string());
		    return true;
	    });
}

void CreatureCarryOverSystem::ReadIfNoneKept()
{
	if (_kept != nullptr)
	{
		return;
	}
	FinishWrites();
	const auto files = Files();
	if (!files.has_value())
	{
		return;
	}
	const auto& path = files->mind;
	std::error_code error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	auto file = std::make_shared<creaturemind::MindFileData>();
	const auto read = creaturemind::ReadFile(path, *file);
	if (read != creaturemind::MindResult::Success)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's kept creature could not be read from {}: {}", path.string(),
		                   creaturemind::ResultToStr(read));
		return;
	}
	_kept = std::move(file);
}

void CreatureCarryOverSystem::Keep(std::shared_ptr<const creaturemind::MindFileData> file)
{
	if (file != nullptr)
	{
		_kept = std::move(file);
	}
}

std::shared_ptr<const creaturemind::MindFileData> CreatureCarryOverSystem::Kept() const
{
	return _kept;
}

std::shared_ptr<const creaturemind::MindFileData> CreatureCarryOverSystem::KeptOrRead()
{
	ReadIfNoneKept();
	return _kept;
}

std::optional<entt::entity> CreatureCarryOverSystem::LoadPlayersCreature(glm::vec2 place)
{
	if (PlayersCreature().has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player already has a creature, so theirs is not loaded");
		return std::nullopt;
	}
	// The creature is loaded from its file, as the last land kept it or as an earlier game did
	ReadIfNoneKept();
	if (_kept == nullptr || !Locator::terrainSystem::has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player has no creature kept to load");
		return std::nullopt;
	}
	const auto body = creature_mind_body::FromMindFile(*_kept);
	if (!body.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's kept creature is of no species the game knows");
		return std::nullopt;
	}
	const auto position = map_coords::ToWorld(Locator::terrainSystem::value(), creature_carry_over::ArrivalCoords(place));
	const auto species = body->species;
	// The game makes a new creature of the species first, so the body draws its spread strength and fatness. The
	// numbers of the file then replace them.
	auto start = archetypes::CreatureArchetype::StartBody(species);
	start.alignment = body->alignment.value_or(0.0f);
	start.fatness = body->fatness;
	start.strength = body->strength;
	const auto creature =
	    archetypes::CreatureArchetype::Create(position, LocalPlayer(), species, 0, glm::radians(k_ArrivalYawDegrees),
	                                          body->size.value_or(archetypes::CreatureArchetype::StartScale(species)), start);
	auto& registry = Locator::entitiesRegistry::value();
	// It keeps the exact map position where it arrives
	ecs::thing_map_place::Keep(registry, creature, creature_carry_over::ArrivalCoords(place));
	ecs::creature_body_file::Apply(registry, creature, *body);
	if (Locator::creatureMindSystem::has_value())
	{
		// It reads its mind from the file at once, over the mind that it got as the game made it
		Locator::creatureMindSystem::value().LoadMind(creature, _kept);
	}

	// It starts out of sight, with the sound of sparkling, and sparkles into sight
	const auto fizz = creature_carry_over::ArrivalFizz();
	auto* spells = registry.TryGet<CreatureSpells>(creature);
	if (spells == nullptr)
	{
		spells = &registry.Assign<CreatureSpells>(creature);
	}
	spells->fizz = fizz.now;
	_arriving.push_back({.creature = creature, .fizz = fizz});
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().CreatePointSound(static_cast<entt::id_type>(audio::SoundId::G_SpellTeleportEnergiseGo),
		                                                  position, false);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Loaded the player's creature {} at {:.1f}, {:.1f}", entt::to_integral(creature),
	                   position.x, position.z);
	return creature;
}

void CreatureCarryOverSystem::SaveAsTheyLive()
{
	std::vector<entt::entity> creatures;
	Locator::entitiesRegistry::value().Each<const Creature, const CreatureNeeds>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/, const CreatureNeeds& /*needs*/) {
		    creatures.push_back(entity);
	    });
	for (const auto creature : creatures)
	{
		SaveAsItLives(creature);
	}
}

void CreatureCarryOverSystem::SaveAsItLives(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (NetworkGame())
	{
		// Each computer writes only the creature of its own god, so the count stays out of the world
		if (_brought != entity || !registry.Valid(entity) || !registry.AllOf<CreatureNeeds>(entity))
		{
			return;
		}
		const auto turns = registry.Get<const CreatureNeeds>(entity).needs.turns;
		const auto before = std::exchange(_broughtTurnsSeen, turns);
		if (!before.has_value() || !creature_carry_over::SavesAsItLives(*before, turns))
		{
			return;
		}
	}
	else
	{
		if (!registry.Valid(entity) || !registry.AllOf<Creature, CreatureNeeds>(entity) ||
		    registry.Get<const Creature>(entity).owner != LocalPlayer())
		{
			return;
		}
		const auto turns = registry.Get<const CreatureNeeds>(entity).needs.turns;
		auto* save = registry.TryGet<CreatureFileSave>(entity);
		if (save == nullptr)
		{
			// A creature first looked at now is written only once it gets to the next multiple of 600 turns
			registry.Assign<CreatureFileSave>(entity, turns);
			return;
		}
		const bool due = creature_carry_over::SavesAsItLives(save->turnsSeen, turns);
		save->turnsSeen = turns;
		if (!due)
		{
			return;
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player's creature {} has lived another {} turns", entt::to_integral(entity),
	                   creature_carry_over::k_SaveEveryTurns);
	KeepCreature(entity);
	// The game writes the web page with the files of a creature that lives on, but not when a land keeps it
	if (Locator::creatureWebPageSystem::has_value())
	{
		Locator::creatureWebPageSystem::value().WritePage(entity);
	}
}

void CreatureCarryOverSystem::ProcessTurn()
{
	if (_landKeepsCreatures)
	{
		SaveAsTheyLive();
	}
	StepArrivals(std::nullopt);
}

void CreatureCarryOverSystem::CreatureTurn(entt::entity creature)
{
	if (_landKeepsCreatures)
	{
		SaveAsItLives(creature);
	}
	StepArrivals(creature);
}

void CreatureCarryOverSystem::StepArrivals(std::optional<entt::entity> creature)
{
	if (_arriving.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(_arriving, [&registry, creature](Arriving& arriving) {
		// A creature that is gone takes no turn, and its sparkle ends with it
		if (creature.has_value() && arriving.creature != *creature && registry.Valid(arriving.creature))
		{
			return false;
		}
		auto* spells = registry.Valid(arriving.creature) ? registry.TryGet<CreatureSpells>(arriving.creature) : nullptr;
		if (spells == nullptr)
		{
			return true;
		}
		arriving.fizz = creature_carry_over::StepFizz(arriving.fizz, k_TurnMilliseconds);
		spells->fizz = arriving.fizz.now;
		return arriving.fizz.perSecond == 0.0f;
	});
}

void CreatureCarryOverSystem::Reset()
{
	_arriving.clear();
	_brought.reset();
	_broughtTurnsSeen.reset();
}

void CreatureCarryOverSystem::SetBroughtCreature(entt::entity creature)
{
	_brought = creature;
	_broughtTurnsSeen.reset();
}

void CreatureCarryOverSystem::SetLandKeepsCreatures(bool keeps)
{
	_landKeepsCreatures = keeps;
}

void CreatureCarryOverSystem::EndGame()
{
	if (_landKeepsCreatures)
	{
		KeepPlayersCreature();
	}
	// The files are on the disk before the next game or the menu reads them
	FinishWrites();
}

void CreatureCarryOverSystem::StartGame()
{
	auto file = _profileFile ? _profileFile() : std::nullopt;
	if (file != _gameFile)
	{
		_kept.reset();
	}
	_gameFile = std::move(file);
}
