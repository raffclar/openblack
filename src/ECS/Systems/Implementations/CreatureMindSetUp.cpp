/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a mind starts: the game's tables of the mind, a new mind, and a mind that the game loads from a file or saves
// to a file

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMusicReaction.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "CreatureMindFootball.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/FixedKinds.h"
#include "ECS/LearningMarker.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownDesire.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/MusicMood.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

namespace
{
/// The land's script that curses the player's creature, and how tall a creature of size 1 is
constexpr std::string_view k_CurseScript = "CreatureCurse";
constexpr float k_HeightOfSizeOne = 15.0f;

/// The leash word of a mind file for a leash type: the word of the type's number
size_t LeashWordOf(LeashType type)
{
	return static_cast<size_t>(type);
}
} // namespace

uint32_t CreatureMindSystem::Random(uint32_t range)
{
	return _random.GameRand(range);
}

const creature_mind_tables::Tables* CreatureMindSystem::GetTables()
{
	if (!_tables.has_value() && Locator::infoConstants::has_value())
	{
		_tables = creature_mind_tables::Build(Locator::infoConstants::value());
		_executors.clear();
		std::ranges::transform(
		    _tables->actions, std::back_inserter(_executors),
		    [](const creature_mind_tables::ActionInfo& action) { return creature_plan_actions::For(action.name); });
	}
	return _tables.has_value() ? &*_tables : nullptr;
}

const creature_plan_actions::Executor* CreatureMindSystem::ExecutorOf(uint32_t action)
{
	return GetTables() != nullptr && action < _executors.size() ? _executors[action] : nullptr;
}

void CreatureMindSystem::StartMind(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body != nullptr && mind != nullptr)
	{
		SetUpMind(creature, *body, *mind);
		// A creature that the game makes for a mind file reads the file at once, over the mind that it got
		if (mind->pendingFile != nullptr)
		{
			TakeUpFile(creature, *mind);
		}
	}
}

void CreatureMindSystem::SetUpMind(entt::entity creature, const Creature& body, CreatureMindState& mind)
{
	const bool made = !mind.desires.has_value();
	if (made)
	{
		mind.desires = creature_desires::Create(SetupFor(body.species), _random.GameFloat());
		// It is born with its species' personality, spread at random
		mind.innate = creature_desire_sources::SpreadTraits(InnateTraitsOf(body.species), _random.GameFloat());
		// Its first round of desires to plan starts as it is made
		mind.planner.round = creature_planner::RoundOf(*mind.desires);
	}
	if (!mind.learnt.has_value())
	{
		SetUpLearning(creature, mind);
	}
	// A new creature of the local player is told about the custom music track that plays now
	if (made && IsLocalPrimaryCreature(creature) && Locator::musicMoodSystem::has_value())
	{
		Locator::musicMoodSystem::value().ReadRecords(std::nullopt, true);
	}
}

bool CreatureMindSystem::IsLocalPrimaryCreature(entt::entity creature)
{
	if (!Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature((Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
	                                                                    : PlayerNames::PLAYER_ONE)) == creature;
}

void CreatureMindSystem::SetUpLearning(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	const auto opinions = tables != nullptr ? creature_mind_tables::StartingOpinions(*tables) : std::vector<float> {};
	const auto actionCount = opinions.size();
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	mind.learnt = creature_mind_model::Fresh(*mind.desires, opinions, std::move(knowledge));
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(*mind.learnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	// A new mind did each action a short time ago, picked at random for each
	mind.learnt->turnsSinceDone = creature_action_history::Fresh(actionCount, [this](uint32_t range) { return Random(range); });
	if (mind.resourceChecked)
	{
		return;
	}
	mind.resourceChecked = true;
	// A creature made from a mind file takes up what the file holds
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->mind == 0 || !Locator::resources::has_value())
	{
		return;
	}
	const auto& minds = Locator::resources::value().GetCreatureMinds();
	if (minds.Contains(body->mind))
	{
		const auto handle = minds.Handle(body->mind);
		if (handle && handle->Loaded())
		{
			mind.pendingFile = std::make_shared<const creaturemind::MindFileData>(handle->data);
		}
	}
}

void CreatureMindSystem::TakeUpFile(entt::entity creature, CreatureMindState& mind, FileBase base)
{
	const auto file = std::move(mind.pendingFile);
	mind.pendingFile.reset();
	auto* body = Locator::entitiesRegistry::value().TryGet<Creature>(creature);
	if (body == nullptr || !mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	// The records of the custom music tracks in the file are kept only for the local player's creature
	if (Locator::musicMoodSystem::has_value())
	{
		const auto records =
		    file->database.has_value() ? std::optional(music_mood::FromMindFile(*file->database)) : std::nullopt;
		Locator::musicMoodSystem::value().ReadRecords(records, IsLocalPrimaryCreature(creature));
	}
	// It takes the name the file keeps, unless a computer player owns it
	if (creature_names::TakesNameFromFile(ecs::creature_naming::OwnerOf(body->owner)))
	{
		body->name = file->name;
	}
	// The game makes each source of the file as a new source before it reads the numbers of the file
	creature_mind_model::DrawAsSourcesAreRead(*file, SourceSpreadOf, _random.GameFloat());
	const auto* tables = GetTables();
	// Over its own mind, the creature keeps what the file does not hold, and the examples of the file come after its
	// own examples
	if (base == FileBase::Own)
	{
		Abandon(creature, mind);
		ApplyLoaded(creature, mind, creature_mind_model::FromFile(*file, *mind.desires, *mind.learnt), *file);
		return;
	}
	const auto fresh = creature_desires::Create(SetupFor(body->species), [](float most) { return 0.5f * most; });
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	auto freshLearnt = creature_mind_model::Fresh(
	    fresh, tables != nullptr ? creature_mind_tables::StartingOpinions(*tables) : mind.learnt->opinions,
	    std::move(knowledge));
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(freshLearnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	freshLearnt.turnsSinceDone =
	    creature_action_history::Fresh(freshLearnt.opinions.size(), [this](uint32_t range) { return Random(range); });
	ApplyLoaded(creature, mind, creature_mind_model::FromFile(*file, fresh, freshLearnt), *file);
}

void CreatureMindSystem::ApplyLoaded(entt::entity creature, CreatureMindState& mind, creature_mind_model::LoadedMind loaded,
                                     const creaturemind::MindFileData& file)
{
	if (const auto* tables = GetTables(); tables != nullptr)
	{
		creature_mind_model::RebuildTrees(loaded.learnt, tables->attributes);
	}
	Abandon(creature, mind);
	mind.planner = {};
	mind.desires = std::move(loaded.desires);
	mind.planner.round = creature_planner::RoundOf(*mind.desires);
	mind.learnt = std::move(loaded.learnt);
	mind.developmentPhase = std::min(loaded.developmentPhase, CreatureMindState::k_FullyGrownUp);
	// The file's desires are active as it says, for the stage it gives
	mind.desiresPhase = mind.developmentPhase;
	mind.attitudeToPlayer = loaded.attitudeToPlayer;
	mind.turnsLookedAt = loaded.turnsLookedAt;
	auto& registry = Locator::entitiesRegistry::value();
	// The leash words of the file tell which leashes the creature knows
	if (auto* leashes = registry.TryGet<CreatureLeash>(creature); leashes != nullptr && file.leashWords.has_value())
	{
		for (size_t i = 0; i < creature_leash::k_Types.size(); ++i)
		{
			leashes->known.set(i, mind.learnt->leashWords.at(LeashWordOf(creature_leash::k_Types.at(i))) != 0);
		}
	}
	// A file of version 30 or later gives the fight health of the creature. An older file keeps the fight health that the
	// creature has.
	if (file.fightHealth.has_value() && Locator::creatureFightSystem::has_value())
	{
		auto& fights = Locator::creatureFightSystem::value();
		if (fights.GetFightHealth(creature) != *file.fightHealth)
		{
			fights.SetFightHealth(creature, *file.fightHealth);
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} took up a mind file: version {}, stage {}",
	                   entt::to_integral(creature), file.version, mind.developmentPhase);
}

void CreatureMindSystem::LearnEveryLeash(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr && mind->learnt.has_value())
	{
		mind->learnt->leashWords.fill(1);
	}
	if (auto* leashes = registry.TryGet<CreatureLeash>(creature))
	{
		leashes->known.set();
	}
}

bool CreatureMindSystem::HasValidPlan(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr && mind->planner.current.has_value() && IsPlanValid(*mind->planner.current);
}

bool CreatureMindSystem::IsPlanValid(const creature_planner::Plan& plan)
{
	const auto* tables = GetTables();
	const auto desire = static_cast<size_t>(plan.desire);
	if (tables == nullptr || desire >= tables->rules.size())
	{
		return false;
	}
	const auto* executor = ExecutorOf(plan.action);
	const bool usesThing = executor != nullptr && executor->use != Target::None;
	return creature_planner::IsValid(plan, tables->rules.at(desire).aboutSomething, usesThing);
}

std::vector<uint32_t> CreatureMindSystem::ListOfStoppedPlan(entt::entity creature, CreatureMindState& mind,
                                                            const creature_planner::Plan& plan)
{
	if (plan.desire != Desire::Compassion)
	{
		return mind.planActions;
	}
	// Compassion has the list of the town desire that it helps with. With hurt people in the town, the first look at the
	// list can put healing first.
	const auto* tables = GetTables();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto whole = plan.about.has_value() ? plan.about : plan.activityObject;
	if (tables == nullptr || !whole.has_value() || !registry.Valid(static_cast<entt::entity>(*whole)) ||
	    !registry.AllOf<Town>(static_cast<entt::entity>(*whole)))
	{
		return {};
	}
	auto list = TownCompassionActions(registry, creature, mind, static_cast<entt::entity>(*whole), *tables);
	if (list.healDraw)
	{
		const auto dropped = creature_town_compassion::HealComesFirst(Random(creature_town_compassion::k_HealChance)) ? 1 : 0;
		if (static_cast<size_t>(dropped) < list.actions.size())
		{
			list.actions.erase(list.actions.begin() + dropped);
		}
	}
	return list.actions;
}

void CreatureMindSystem::Abandon(entt::entity creature, CreatureMindState& mind)
{
	if (mind.planActive && mind.learnt.has_value() && !mind.learnt->contexts.empty())
	{
		mind.learnt->contexts.back().running = false;
	}
	// Whether carried out or not, the plan's action was done just now. When the plan is valid, the other actions of its
	// list get older. The list is found again as the plan stops.
	if (mind.planActive && mind.learnt.has_value() && mind.planner.current.has_value())
	{
		const auto plan = *mind.planner.current;
		const auto list = IsPlanValid(plan) ? ListOfStoppedPlan(creature, mind, plan) : std::vector<uint32_t> {};
		creature_action_history::PlanEnded(mind.learnt->turnsSinceDone, plan.action, list, mind.agendaTurns);
	}
	mind.planActive = false;
	if (mind.planner.current.has_value())
	{
		mind.planner.lastObject = mind.planner.current->object;
		mind.planner.lastAction = mind.planner.current->action;
	}
	mind.planner.current.reset();
	// To stop what it does clears the marks of the plan, also what the sweep of its eyes found of the plan's things
	mind.sweep.targets = {};
	mind.ownPlanSinceStop = false;
	mind.planForced = false;
	mind.forcedPlanCounts = false;
	mind.ledByLeash = false;
	mind.emergencyRunning = false;
}

void CreatureMindSystem::LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind)
{
	auto* state = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (state == nullptr)
	{
		return;
	}
	state->pendingFile = std::move(mind);
	// The game reads the file at once, over the mind that the creature has. A mind that is not ready yet reads it on
	// its first turn.
	if (state->desires.has_value() && state->learnt.has_value() && state->pendingFile != nullptr)
	{
		TakeUpFile(creature, *state);
	}
}

std::optional<creaturemind::MindFileData> CreatureMindSystem::SaveMind(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value() || !mind->learnt.has_value())
	{
		return std::nullopt;
	}
	auto file = creature_mind_model::ToFile(*mind->desires, *mind->learnt, mind->developmentPhase, mind->attitudeToPlayer,
	                                        mind->turnsLookedAt, static_cast<uint32_t>(creature::InfoRow(body->species)));
	// The leash words tell which leashes the creature knows now
	if (const auto* leashes = registry.TryGet<const CreatureLeash>(creature); leashes != nullptr && file.leashWords.has_value())
	{
		for (size_t i = 0; i < creature_leash::k_Types.size(); ++i)
		{
			auto& word = file.leashWords->at(LeashWordOf(creature_leash::k_Types.at(i)));
			word = creature_mind_model::LeashWord(word, leashes->known.test(i));
		}
	}
	// The mind file keeps the records of the custom music tracks. The current track's record first gets what the local
	// player's creature feels now.
	if (Locator::musicMoodSystem::has_value())
	{
		const auto records = Locator::musicMoodSystem::value().WriteRecords();
		file.database = music_mood::ToMindFile(records);
	}
	// The file keeps the fight health that the creature has now: as its last fight left it, or full before any
	if (Locator::creatureFightSystem::has_value())
	{
		file.fightHealth = Locator::creatureFightSystem::value().GetFightHealth(creature);
	}
	// It is saved as itself: as it was before any spell on it changed it
	const creature_spells::SavedBody now {.size = body->size, .strength = body->strength, .alignment = body->alignment};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, now) : now;
	// While the land's curse on the player's creature runs, it is saved as the curse found it
	if (body->owner == PlayerNames::PLAYER_ONE && Locator::vm::has_value())
	{
		const auto& vm = Locator::vm::value();
		const bool cursed =
		    std::ranges::any_of(vm.GetTasks(), [](const auto& entry) { return entry.second.name == k_CurseScript; });
		if (cursed)
		{
			const auto global = [&vm](std::string_view name) {
				const auto& variables = vm.GetVariables();
				const auto found = std::ranges::find(variables, name, &lhvm::VMVar::name);
				return found != variables.end() ? found->value.floatVal : 0.0f;
			};
			saved.size = global("OriginalSizeOfMyCreature") / k_HeightOfSizeOne;
			saved.strength = global("OriginalStrengthOfMyCreature");
			saved.alignment = global("OriginalAlignmentOfMyCreature");
		}
	}
	// Its body is saved with its mind as it is now, but for what the spells changed
	auto kept = creature_body_file::Capture(registry, creature);
	kept.size = saved.size;
	kept.strength = saved.strength;
	kept.alignment = saved.alignment;
	creature_mind_body::ToMindFile(kept, file);
	// Its name is saved with its mind
	file.name = body->name;
	// The file says who saved it: the creature file of the player at this computer when that player owns the creature,
	// and the profile played
	std::optional<std::string> ownerFile;
	std::u16string profile;
	if (Locator::playerProfileSystem::has_value())
	{
		const auto& profiles = Locator::playerProfileSystem::value();
		if (Locator::playerSystem::has_value() &&
		    body->owner ==
		        (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE))
		{
			ownerFile = profiles.CreatureFile();
		}
		if (const auto current = profiles.GetCurrentProfile(); current.has_value())
		{
			profile = gui::ToUtf16(current->name);
		}
	}
	creature_mind_model::SetSavedBy(file, ownerFile, profile);
	return file;
}

void CreatureMindSystem::RestoreMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind,
                                     const std::optional<RestoredPlan>& plan)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* state = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (state == nullptr || body == nullptr || mind == nullptr)
	{
		return;
	}
	// The creature starts as a new creature, with the mind of its file
	SetUpMind(creature, *body, *state);
	state->pendingFile = std::move(mind);
	TakeUpFile(creature, *state, FileBase::Fresh);
	const auto* tables = GetTables();
	if (!plan.has_value() || tables == nullptr || plan->action >= tables->actions.size())
	{
		return;
	}
	if (!ForcePlan(creature, {.desire = plan->desire,
	                          .action = tables->actions[plan->action].name,
	                          .object = plan->object,
	                          .instrument = plan->instrument,
	                          .activityObject = plan->activityObject}))
	{
		return;
	}
	// It does the step of the agenda that it did again, from the start of the step
	auto& idle = state->idle;
	if (!idle.agenda.empty())
	{
		idle.step = std::min(plan->step, idle.agenda.size() - 1);
		idle.stepStarted = false;
	}
}

void CreatureMindSystem::ClearLearning(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr)
	{
		return;
	}
	const auto name = mind->learnt.has_value() ? mind->learnt->name : std::u16string {};
	Abandon(creature, *mind);
	mind->desires = creature_desires::Create(SetupFor(body->species), _random.GameFloat());
	mind->desiresPhase.reset();
	mind->planner = {};
	mind->planner.round = creature_planner::RoundOf(*mind->desires);
	mind->pendingFile.reset();
	mind->resourceChecked = true;
	SetUpLearning(creature, *mind);
	mind->learnt->name = name;
}
