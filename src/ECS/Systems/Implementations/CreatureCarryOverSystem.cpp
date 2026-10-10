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
#include "ECS/Components/CreatureSpells.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
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

/// The player's creature: the first they got, the one they lead on the leash
std::optional<entt::entity> PlayersCreature()
{
	return Locator::leashSystem::has_value() ? Locator::leashSystem::value().PlayersCreature(LocalPlayer()) : std::nullopt;
}
} // namespace

CreatureCarryOverSystem::CreatureCarryOverSystem(std::optional<std::filesystem::path> folder)
    : _folder(std::move(folder))
{
}

void CreatureCarryOverSystem::KeepPlayersCreature()
{
	const auto creature = PlayersCreature();
	if (!creature.has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto file = Locator::creatureMindSystem::value().SaveMind(*creature);
	if (!file.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature {} has no mind yet to keep for the next land",
		                   entt::to_integral(*creature));
		return;
	}
	// Its physique is saved beside its mind: its body as it is now, but for the alignment, as no spell made it
	const auto& registry = Locator::entitiesRegistry::value();
	const auto now = ecs::creature_body_file::Capture(registry, *creature);
	const creature_spells::SavedBody nowValues {
	    .size = now.size.value_or(1.0f), .strength = now.strength, .alignment = now.alignment.value_or(0.0f)};
	const auto* spells = registry.TryGet<const CreatureSpells>(*creature);
	const auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, nowValues) : nowValues;
	const auto physique = creature_mind_body::ToPhysiqueFile(now, file->speciesRow, saved.alignment);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Kept the player's creature {} for the next land", entt::to_integral(*creature));
	Write(*file, physique);
	Keep(std::make_shared<const creaturemind::MindFileData>(std::move(*file)));
}

void CreatureCarryOverSystem::Write(const creaturemind::MindFileData& mind,
                                    const creaturemind::PhysiqueFileData& physique) const
{
	if (!_folder.has_value())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(*_folder, error);
	const auto files = creature_carry_over::KeptFilesIn(*_folder, k_ProfileFile);
	const auto written = creaturemind::WriteFile(files.mind, mind);
	if (written != creaturemind::MindResult::Success)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature could not be written to {}: {}", files.mind.string(),
		                   creaturemind::ResultToStr(written));
		return;
	}
	if (!creaturemind::WritePhysiqueFile(files.physique, physique))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The player's creature's physique could not be written to {}",
		                   files.physique.string());
	}
}

void CreatureCarryOverSystem::ReadIfNoneKept()
{
	if (_kept != nullptr || !_folder.has_value())
	{
		return;
	}
	const auto path = creature_carry_over::KeptFilesIn(*_folder, k_ProfileFile).mind;
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
	const auto creature = archetypes::CreatureArchetype::Create(
	    position, LocalPlayer(), species, 0, glm::radians(k_ArrivalYawDegrees),
	    body->size.value_or(archetypes::CreatureArchetype::StartScale(species)),
	    {.alignment = body->alignment.value_or(0.0f), .fatness = body->fatness, .strength = body->strength});
	auto& registry = Locator::entitiesRegistry::value();
	ecs::creature_body_file::Apply(registry, creature, *body);
	if (Locator::creatureMindSystem::has_value())
	{
		// Its mind is taken up on its first turn of thought
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

void CreatureCarryOverSystem::ProcessTurn()
{
	if (_arriving.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(_arriving, [&registry](Arriving& arriving) {
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
}
