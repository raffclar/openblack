/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureWebPageSystem.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

#include <CreatureHtml.h>
#include <LhpFile.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAttention.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreaturePictures.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/ScreenshotCapture.h"
#include "Help/ScriptSpirits.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());

PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// What the plan of a creature's mind does: its action, its desire, and its priority. The action of the last plan stays
/// after the plan stops. A creature with no plan has no desire, and a priority of 0.
struct PlanNow
{
	uint32_t action {0};
	std::optional<size_t> desire;
	float priority {0.0f};
};

PlanNow PlanOf(const CreatureMindState* mind)
{
	if (mind == nullptr)
	{
		return {};
	}
	const auto& planner = mind->planner;
	if (!planner.current.has_value())
	{
		return {.action = planner.lastAction};
	}
	const bool runs = mind->planActive && !planner.currentHasNoSteps;
	return {.action = planner.current->action,
	        .desire = static_cast<size_t>(planner.current->desire),
	        .priority = runs ? planner.current->priority : 0.0f};
}

float PictureWorth(uint32_t action)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& actions = Locator::infoConstants::value().creatureAction;
	return action < actions.size() ? actions.at(action).pictureWorth : 0.0f;
}

std::string TextOf(std::span<const char> text)
{
	const auto end = std::ranges::find(text, '\0');
	return {text.begin(), end};
}

std::vector<uint8_t> AsBytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// The bytes of the file, or nothing when the file is not there or the game cannot read it
std::optional<std::vector<uint8_t>> ReadFileIfThere(filesystem::FileSystemInterface& files, const std::filesystem::path& path)
{
	try
	{
		if (!files.Exists(path))
		{
			return std::nullopt;
		}
		return files.ReadAll(path);
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game could not read {}: {}", path.generic_string(), error.what());
		return std::nullopt;
	}
}

} // namespace

CreatureWebPageSystem::CreatureWebPageSystem(PlayerOfFiles player)
    : _player(std::move(player))
{
}

void CreatureWebPageSystem::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureAnimation>(
	    [&creatures](entt::entity entity, const Creature&, CreatureAnimation&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		CreatureTurn(entity);
	}
}

void CreatureWebPageSystem::CreatureTurn(entt::entity entity)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::time::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto turn = Locator::time::value().GetTurn();
	const auto local = LocalPlayer();
	const auto player = _player ? _player() : std::nullopt;
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	if (!registry.Valid(entity) || !registry.AllOf<Creature, CreatureAnimation>(entity))
	{
		return;
	}
	const auto& creature = registry.Get<const Creature>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	// The moment of the body's action counts for this turn only
	const bool moment = std::exchange(animation.actionMoment, false);
	// Only the player of a creature looks at it, so only the player's own creatures have pictures
	const auto* at = registry.TryGet<const Transform>(entity);
	if (creature.owner != local || at == nullptr || camera == nullptr)
	{
		return;
	}
	auto& kept = registry.AllOf<CreaturePictures>(entity) ? registry.Get<CreaturePictures>(entity)
	                                                      : registry.Assign<CreaturePictures>(entity);
	const auto* mind = registry.TryGet<const CreatureMindState>(entity);
	const auto plan = PlanOf(mind);
	const creature_web_page::Chance chance {
	    .lookedAt = creature_attention::IsLookingAt(camera->GetOrigin(), camera->GetFocus(), at->position),
	    .seconds = creature_web_page::WholeSecondsBetween(kept.lastTurn, turn, k_TurnMilliseconds),
	    .actionMoment = moment,
	    .action = plan.action,
	};
	if (!creature_web_page::TakesPicture(kept.pictures, chance))
	{
		return;
	}
	const auto* needs = registry.TryGet<const CreatureNeeds>(entity);
	const auto age = needs != nullptr ? needs->needs.age : 0u;
	const auto score = creature_web_page::Score(PictureWorth(plan.action), plan.priority, age);
	const auto slot = creature_web_page::Keep(kept.pictures, {.score = score, .age = age, .action = plan.action});
	// The next picture waits ten seconds, whether the creature keeps this one or not
	kept.lastTurn = turn;
	if (!slot.has_value())
	{
		return;
	}
	const auto text = creature_web_page::CaptionText(plan.desire);
	_captions.at(*slot) = creature_web_page::Caption(
	    Locator::helpTextSystem::has_value() ? Locator::helpTextSystem::value().GetWords(text) : std::u16string {});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game took picture {} of creature {}: action {}, score {}", *slot,
	                   entt::to_integral(entity), plan.action, score);
	if (!player.has_value() || !Locator::rendererInterface::has_value())
	{
		return;
	}
	auto name =
	    fmt::format("{}creature-picture/{}/{}", graphics::ScreenPictures::k_Prefix, entt::to_integral(entity), _asked++);
	Locator::rendererInterface::value().RequestScreenPicture(name, &creature_web_page::TakePicture);
	_waiting.push_back({.name = std::move(name), .player = *player, .slot = *slot});
}

bool CreatureWebPageSystem::AnyPictureWaiting() const
{
	return !_waiting.empty();
}

void CreatureWebPageSystem::CollectPictures(const Screens& screens)
{
	std::erase_if(_waiting, [this, &screens](const Waiting& waiting) {
		if (auto picture = screens.take ? screens.take(waiting.name) : std::nullopt; picture.has_value())
		{
			// The writer encodes and writes the picture after the pages and pictures that came before it
			const auto folder = Locator::profileFolderSystem::has_value()
			                        ? Locator::profileFolderSystem::value().GetPlayerFolder(waiting.player)
			                        : std::nullopt;
			if (folder.has_value() && Locator::filesystem::has_value())
			{
				Writer().Queue("a picture of the creature",
				               [files = &Locator::filesystem::value(), folder = *folder,
				                file = creature_web_page::PictureFile(waiting.slot), picture = std::move(*picture)]() {
					               const auto jpeg = creature_web_page::EncodeJpeg(picture, creature_web_page::k_PictureWidth,
					                                                               creature_web_page::k_PictureHeight);
					               return jpeg.empty() || (files->MakeFolder(folder) && files->WriteAll(folder / file, jpeg));
				               });
			}
			return true;
		}
		return screens.failed && screens.failed(waiting.name);
	});
}

std::span<const std::u16string> CreatureWebPageSystem::GetCaptions() const
{
	return _captions;
}

void CreatureWebPageSystem::WritePage(entt::entity creature)
{
	const auto player = _player ? _player() : std::nullopt;
	if (!player.has_value() || !Locator::profileFolderSystem::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return;
	}

	creature_web_page::PageFacts facts {
	    .name = body->name,
	    .alignment = body->alignment,
	    .strength = body->strength,
	    .fatness = body->fatness,
	    .captions = _captions,
	};
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	if (needs != nullptr)
	{
		facts.age = needs->needs.age;
		facts.health = needs->needs.life;
		facts.energy = needs->needs.energy;
		facts.exhaustion = needs->needs.exhaustion;
		facts.dehydration = needs->needs.dehydration;
		facts.itchiness = needs->needs.itchiness;
		facts.poo = needs->needs.poo;
	}
	if (const auto* tallies = registry.TryGet<const CreatureTallies>(creature))
	{
		facts.peopleKilled = tallies->meals.peopleKilled;
		facts.animalsKilled = tallies->meals.animalsKilled;
		facts.creaturesKilled = tallies->meals.creaturesKilled;
		facts.poos = tallies->poos;
		facts.mushrooms = tallies->mushroomsEaten;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(creature))
	{
		facts.battlesFought = record->fights;
		facts.battlesWon = record->wins;
	}
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature))
	{
		if (mind->desires.has_value() && Locator::infoConstants::has_value())
		{
			const auto& infos = Locator::infoConstants::value().creatureInitialDesire;
			for (size_t desire = 0; desire < infos.size() && desire < mind->desires->desires.size(); ++desire)
			{
				facts.desires.emplace_back(TextOf(infos.at(desire).shortName), mind->desires->desires.at(desire).value);
			}
		}
		for (size_t kind = 0; kind < facts.beliefs.size(); ++kind)
		{
			facts.beliefs.at(kind) = mind->beliefs.Count(static_cast<uint32_t>(kind));
		}
		facts.niceness = help::script_spirits::DiscreteAlignment(mind->attitudeToPlayer);
		facts.attention = creature_attention::HowMuch(
		    creature_attention::Share(mind->turnsLookedAt, needs != nullptr ? needs->needs.turns : 0u));
	}

	if (const auto* spells = registry.TryGet<const CreatureSpells>(creature))
	{
		const auto& slots = spells->spells.slots;
		const auto on = std::ranges::find_if(slots, [](const auto& slot) { return slot.phase != creature_spells::Phase::Off; });
		if (on != slots.end())
		{
			facts.spellOn = static_cast<uint32_t>(std::distance(slots.begin(), on));
		}
	}
	auto values = creature_web_page::PageValues(facts);

	// The game draws the stamp of the upload package as it writes the page. The files go to the disk on the writer's
	// thread, so that the frame does not wait for them.
	const auto stamp = Locator::gameRandom::has_value()
	                       ? static_cast<uint16_t>(Locator::gameRandom::value().LocalRand(creature_web_page::k_StampRange))
	                       : uint16_t {0};
	auto& profiles = Locator::profileFolderSystem::value();
	auto& files = Locator::filesystem::value();
	const auto folder = profiles.GetPlayerFolder(*player);
	if (!folder.has_value())
	{
		return;
	}
	std::error_code error;
	auto gameFolder = std::filesystem::absolute(files.GetGamePath(), error);
	if (error)
	{
		gameFolder = files.GetGamePath();
	}
	PageJob job {
	    .files = &files,
	    .templatePath = files.GetPath<filesystem::Path::Scripts>() / creature_web_page::k_Template,
	    .gameFolder = gameFolder.lexically_normal(),
	    .playerFolder = *folder,
	    .pageFile = fmt::format("{}/{}", creature_web_page::k_PageFolder, creature_web_page::PageFile(body->name)),
	    .values = std::move(values),
	    .stamp = stamp,
	};
	Writer().Queue("the web page of the creature", [job = std::move(job)]() {
		WritePageFile(job);
		// The game packs the values and the pictures for the upload tool even when it could not write the page
		std::array<std::optional<std::vector<uint8_t>>, creature_web_page::k_MostPictures> pictures;
		for (size_t slot = 0; slot < pictures.size(); ++slot)
		{
			pictures.at(slot) =
			    ReadFileIfThere(*job.files, job.playerFolder / creature_web_page::PictureFile(static_cast<uint32_t>(slot)));
		}
		const auto package = lhp::Write(creature_web_page::UploadPackage(job.values, pictures, job.stamp));
		if (!job.files->MakeFolder(job.playerFolder) ||
		    !job.files->WriteAll(job.playerFolder / creature_web_page::k_UploadFile, package))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not write {}", creature_web_page::k_UploadFile);
			return false;
		}
		return true;
	});
}

ecs::save_files::BackgroundWriter& CreatureWebPageSystem::Writer()
{
	if (!_writer)
	{
		_writer = std::make_unique<ecs::save_files::BackgroundWriter>(
		    [](const std::string& name) { SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game could not write {}", name); });
	}
	return *_writer;
}

void CreatureWebPageSystem::FlushWrites()
{
	FinishWrites();
}

void CreatureWebPageSystem::FinishWrites() const
{
	if (_writer != nullptr)
	{
		_writer->Flush();
	}
}

void CreatureWebPageSystem::WritePageFile(const PageJob& job)
{
	// The game reads the template each time that it writes the page
	std::vector<uint8_t> page;
	try
	{
		if (!job.files->Exists(job.templatePath))
		{
			return;
		}
		page = job.files->ReadAll(job.templatePath);
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The creature's web page has no template: {}", error.what());
		return;
	}
	const auto filled =
	    creaturehtml::Fill(std::string_view(reinterpret_cast<const char*>(page.data()), page.size()), job.values);
	// The template links to its decorations in the game folder by a path from the profile folder, which is not in the
	// game folder here
	const auto linked = creature_web_page::PointLinksAtGameFolder(filled, job.gameFolder);
	if (!job.files->MakeFolder(job.playerFolder / creature_web_page::k_PageFolder))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game could not make the folder {}",
		                   (job.playerFolder / creature_web_page::k_PageFolder).generic_string());
		return;
	}
	const auto path = job.playerFolder / job.pageFile;
	if (job.files->WriteAll(path, AsBytes(linked)))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Wrote the web page of the creature to {}", path.generic_string());
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game could not write {}", path.generic_string());
	}
}
