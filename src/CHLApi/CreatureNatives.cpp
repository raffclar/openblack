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
#include "CHLApi.h"
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDevelopment.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureSwap.h"
#include "Creature/LeashRules.h"
#include "Creature/TemplePen.h"
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
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/ScriptRemind.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/CreatureScars.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/GamePlayers.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptCreatureCounts.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptPopulate.h"
#include "ECS/SpellCreator.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/TownDesire.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandGrabRules.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/TownBelief.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptCreatureRules.h"
#include "ScriptHeaders/ScriptDeadLiving.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptHeaders/ScriptStart.h"
#include "Sound/GameMusic.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;
using openblack::Locator;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;
using openblack::script::ObjectType;

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	const auto clip = Pop().intVal;
	const auto object = PopObject();
	// The game has clips 1 to 440; it complains of any other but plays it all the same
	constexpr int32_t k_LastClip = 440;
	if (clip < 1 || clip > k_LastClip)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid animation forced");
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		Locator::livingActionSystem::value().VillagerOverrideAnimation(object, clip);
		return;
	}
	if (auto* animal = registry.TryGet<ecs::components::Animal>(object))
	{
		// A clip the game doesn't have plays its first; another clip than the one playing starts from its beginning,
		// and plays until the animal's state next chooses one
		const auto played =
		    clip >= 0 && clip < static_cast<int32_t>(AnimId::_count) ? static_cast<AnimId>(clip) : static_cast<AnimId>(0);
		if (animal->animation != played)
		{
			animal->animation = played;
			animal->clipPlace = 0;
		}
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		// A creature's body plays the creature's own animations, which a clip forced this way never reaches: the game
		// takes it and nothing shows
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing must be living");
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	// A new creature of the species given, made from another: as big and as strong as that one times the scale, standing
	// on the ground where the script says. It belongs to the first player who has no creature of their own.
	// It takes no notice of reactions until a script tells it to. How strong its body looks comes from how strong the
	// other creature's body looks, until its strength is set or changed by an amount.
	const auto type = Pop().intVal;
	const auto position = PopVec();
	const auto scale = Popf();
	const auto source = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto species = type >= 0 ? script::property_rules::CreatureTypeFromScript(static_cast<uint32_t>(type)) : std::nullopt;
	if (!species.has_value())
	{
		ScriptMessage("Invalid creature type");
	}
	const auto* from =
	    source != entt::null && registry.Valid(source) ? registry.TryGet<const ecs::components::Creature>(source) : nullptr;
	if (from == nullptr)
	{
		ScriptMessage("No creature for script");
		PushObject(entt::null);
		return;
	}
	if (!species.has_value())
	{
		ScriptMessage("Creature not created!");
		PushObject(entt::null);
		return;
	}
	// Each of the seven players can get the copy, also a player that nobody plays. On a land with one player who has a
	// creature, the copy belongs to the second player.
	std::array<script::property_rules::PlayerPlace, 7> places {};
	for (size_t i = 0; i < places.size(); ++i)
	{
		places.at(i).hasCreature = Locator::playerSystem::has_value() &&
		                           Locator::playerSystem::value().GetPrimaryCreature(static_cast<PlayerNames>(i)).has_value();
	}
	const auto owner = script::property_rules::OwnerOfCopy(places).value_or(PlayerNames::NEUTRAL);
	const auto copied = script::property_rules::CopyBody(from->size, from->strength, scale);
	auto body = ecs::archetypes::CreatureArchetype::StartBody(*species);
	body.strength = copied.strength;
	const auto ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	const auto created = ecs::archetypes::CreatureArchetype::Create({position.x, ground, position.z}, owner, *species, 0, 0.0f,
	                                                                copied.size, body);
	if (!registry.AllOf<ecs::components::CreatureReactionsOff>(created))
	{
		registry.Assign<ecs::components::CreatureReactionsOff>(created);
	}
	// The look of the other creature as its body shows it now
	const auto* fromMorph = registry.TryGet<const ecs::components::CreatureMorph>(source);
	auto* morph = registry.TryGet<ecs::components::CreatureMorph>(created);
	if (fromMorph != nullptr && morph != nullptr)
	{
		const auto& other = registry.Get<const ecs::components::Creature>(source);
		const auto look = fromMorph->strengthLook.value_or(
		    creature_morph::StrengthLook(other.strength, ecs::archetypes::CreatureArchetype::SpeciesStrength(other.species)));
		morph->strengthLook = creature_morph::CopiedWeakStrong(look, scale);
	}
	RegisterCreated(created);
	PushObject(created);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	// The creature knows every skill and every miracle, with no lesson shown
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("No creature for script");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("No script for creature");
		return;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LearnEverything(creature);
	}
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	const auto knows = Pop().intVal != 0;
	const auto action = Pop().uintVal;
	const auto typeOfAction = Pop().uintVal;
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No creature for script");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		// The game goes on to teach nothing in particular and fails
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No script for creature");
		return;
	}
	Locator::creatureMindSystem::value().SetKnowsAction(creature, static_cast<CreatureActionLearningType>(typeOfAction), action,
	                                                    knows);
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	const auto priority = Popf();
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("No creature for script");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("No script for creature");
		return;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().SetAgendaPriority(creature, priority);
	}
}

static void ChangeScriptCreatureDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change);

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	// Every desire of the creature stops
	const auto creature = PopObject();
	ChangeScriptCreatureDesires(creature, [](creature_desires::Desires& desires) {
		for (auto& state : desires.desires)
		{
			state.activated = false;
		}
	});
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	// The value is on top of the stack, then the attribute, the desire and the creature. The tree of the desire that
	// tells which things to act on then tests only the attribute, and only the value is good. The game tells of an
	// attribute or a desire out of its range, or of a thing that is not a creature, and then uses it. openblack
	// does nothing then.
	const auto value = Pop().uintVal;
	const auto attribute = Pop().intVal;
	const bool attributeValid = attribute >= 0 && attribute < static_cast<int32_t>(creature_tree::k_AttributeCount);
	if (!attributeValid)
	{
		ScriptMessage("Invalid type");
	}
	const auto desire = Pop().intVal;
	const bool desireValid = desire >= 0 && desire < static_cast<int32_t>(creature_desires::k_DesireCount);
	if (!desireValid)
	{
		ScriptMessage("Invalid creature desire");
	}
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("No creature for script");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("No script for creature");
		return;
	}
	if (attributeValid && desireValid && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LearnDistinction(creature, static_cast<uint32_t>(desire),
		                                                      static_cast<uint32_t>(attribute), value);
	}
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	// The thing used may be none; the creature and the thing acted on may not
	const auto with = PopObject();
	const auto target = PopObject();
	const auto action = Pop().intVal;
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const bool hasCreature = creature != entt::null && registry.Valid(creature);
	const bool hasTarget = target != entt::null && registry.Valid(target);
	if (!hasCreature)
	{
		ScriptMessage("No creature for script");
	}
	if (!hasTarget)
	{
		ScriptMessage("No object to act on for script");
	}
	if (!hasCreature || !hasTarget)
	{
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("No creature for script");
		return;
	}
	if (!Locator::creatureMindSystem::has_value() || action < 0 ||
	    !Locator::creatureMindSystem::value().ScriptDoAction(creature, static_cast<uint32_t>(action), target,
	                                                         with != entt::null && registry.Valid(with) ? std::optional(with)
	                                                                                                    : std::nullopt))
	{
		// An action openblack's creatures can't carry out yet
		NotImplemented(action);
	}
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	const auto creature = PopObject();
	const auto thing = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || thing == entt::null || !registry.Valid(creature) || !registry.Valid(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid creature or thing");
		Pushb(false);
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not a creature");
		Pushb(false);
		return;
	}
	// The thing in the hand of the creature. While the creature prepares a throw, it carries nothing, but the thing stays
	// in its hand until the release, and the test sees it there (deviation 182).
	const auto inHand = ecs::creature_carried::InHandOf(registry, creature);
	const auto* action = registry.TryGet<const ecs::components::CreatureObjectAction>(creature);
	const auto eating = action != nullptr && action->kind == creature_object_actions::Kind::Eat ? action->target : std::nullopt;
	Pushb(script::property_rules::InCreatureHand(thing, inHand.value_or(entt::null), eating));
}

/// Changes the desires of the creature a script names, telling the script when it names no creature
static void ChangeScriptCreatureDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("no creature");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("not a creature");
		return;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesires(creature, change);
	}
}

/// A desire as a script numbers it, if there is one of that number
static std::optional<creature_desires::Desire> ScriptDesire(int32_t desire)
{
	if (desire < 0 || desire >= static_cast<int32_t>(creature_desires::k_DesireCount))
	{
		return std::nullopt;
	}
	return static_cast<creature_desires::Desire>(desire);
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	const auto value = Popf();
	const auto desire = ScriptDesire(Pop().intVal);
	const auto creature = PopObject();
	ChangeScriptCreatureDesires(creature, [desire, value](creature_desires::Desires& desires) {
		if (desire.has_value())
		{
			creature_desires::SetValueFromScript(desires, *desire, value);
		}
	});
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	const auto active = Pop().intVal != 0;
	const auto desire = ScriptDesire(Pop().intVal);
	const auto creature = PopObject();
	ChangeScriptCreatureDesires(creature, [desire, active](creature_desires::Desires& desires) {
		if (desire.has_value())
		{
			desires[*desire].activated = active;
		}
	});
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	// Every desire at once
	const auto active = Pop().intVal != 0;
	const auto creature = PopObject();
	ChangeScriptCreatureDesires(creature, [active](creature_desires::Desires& desires) {
		for (auto& state : desires.desires)
		{
			state.activated = active;
		}
	});
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	// The most it can want the desire, taken as it is
	const auto maximum = Popf();
	const auto desire = ScriptDesire(Pop().intVal);
	const auto creature = PopObject();
	ChangeScriptCreatureDesires(creature, [desire, maximum](creature_desires::Desires& desires) {
		if (desire.has_value())
		{
			desires[*desire].max = maximum;
		}
	});
}

/// The creature becomes the first player's: their creature from now on, which they lead on the leash when they have no
/// other to lead, named with their profile's creature name. The newest reaction that the creature started also becomes
/// the player's.
void MakePlayersCreature(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto name = ecs::creature_naming::ProfileCreatureName(); name.has_value())
	{
		registry.Get<ecs::components::Creature>(object).name = std::move(*name);
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetOwner(object, player);
		Locator::leashSystem::value().ClaimOnArrival(object);
	}
	else
	{
		registry.Get<ecs::components::Creature>(object).owner = player;
	}
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().AddCreature(object);
	}
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().SetPlayerOfNewest(object, player);
	}
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("no creature");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		ScriptMessage("not a creature");
		return;
	}
	MakePlayersCreature(object);
}

/// The creature of a native that reads or clears a count of actions. The game tells of a thing that is not a creature,
/// then uses it and fails. openblack does nothing with such a thing.
static ecs::components::Creature* CreatureForCount(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		ScriptMessage("no creature");
		return nullptr;
	}
	auto* creature = registry.TryGet<ecs::components::Creature>(thing);
	if (creature == nullptr)
	{
		ScriptMessage("not a creature");
	}
	return creature;
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	// The creature is on top of the stack, the number of the count under it. The game takes the number as it is, with
	// no test of its range.
	auto* creature = CreatureForCount(PopObject());
	const auto number = Pop().uintVal;
	if (creature != nullptr && !ecs::script_creature_counts::Clear(*creature, number))
	{
		ScriptMessage("not a count of actions");
	}
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	const auto* creature = CreatureForCount(PopObject());
	const auto number = Pop().uintVal;
	const auto count = creature != nullptr ? ecs::script_creature_counts::Count(*creature, number) : std::nullopt;
	if (creature != nullptr && !count.has_value())
	{
		ScriptMessage("not a count of actions");
	}
	Pushf(count.value_or(0.0f));
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	// The same switch as the creature help option: the advisor's messages about the player's creature
	const bool on = Pop().intVal != 0;
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().SetHelpOn(on);
	}
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	// Whether the desire that the action of the creature's plan shows is the desire. The action is that of the plan
	// carried out, else of the last plan. The native gives false for a thing that is not a creature.
	const auto desire = Pop().intVal;
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(object) && registry.AllOf<ecs::components::Creature>(object)
	                       ? registry.TryGet<const CreatureMindState>(object)
	                       : nullptr;
	if (mind == nullptr || !Locator::infoConstants::has_value())
	{
		Pushb(false);
		return;
	}
	const auto& actions = Locator::infoConstants::value().creatureAction;
	const auto action = creature_planner::ActionInView(mind->planner);
	Pushb(action < actions.size() && static_cast<int32_t>(actions.at(action).shownDesire) == desire);
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	// The creature stops what it does at once, as a failure
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature) || !registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("Force finish not implemented on non creatures");
		return;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ForceFinish(creature);
	}
}

void AttachObjectLeashToObject() // 185 ATTACH_OBJECT_LEASH_TO_OBJECT
{
	const auto object = PopObject();
	const auto creature = PopObject();
	if (object == entt::null || creature == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing for leash not found");
		return;
	}
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	leashes.TieTo(creature, object);
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	const auto creature = PopObject();
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	// The leash goes back to the hand and on again, so its leash becomes the picked leash at the temple again
	auto& leashes = Locator::leashSystem::value();
	if (leashes.TiedTo(creature).has_value())
	{
		leashes.UntieToHand(creature);
	}
	if (leashes.IsLeashed(creature))
	{
		leashes.PutOn(creature, leashes.TypeOf(creature));
	}
	else
	{
		leashes.Toggle(creature);
	}
	leashes.PlayHandAttachSound(creature);
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	// The creature's player lets go of the leash. With no leash on, the creature is still freed from the area it is kept
	// in, and from a desire made dominant unless its player has chosen the learning leash.
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("Thing for leash not found!");
		return;
	}
	const auto* body = registry.TryGet<const ecs::components::Creature>(creature);
	if (body == nullptr)
	{
		ScriptMessage("Thing for leash not creature!");
		return;
	}
	if (body->owner == PlayerNames::NEUTRAL)
	{
		ScriptMessage("Creature no player!");
		return;
	}
	if (Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
	{
		Locator::leashSystem::value().TakeOff(creature);
		return;
	}
	// It also forgets the actions that the area that held it refused
	if (auto* mind = registry.TryGet<ecs::components::CreatureMindState>(creature))
	{
		mind->leftAreaActions.clear();
	}
	if (auto* leash = registry.TryGet<ecs::components::CreatureLeash>(creature))
	{
		leash->confinementRadius = 0.0f;
		if (creature_leash::TakingOffEndsDominantDesire(leash->selected) && Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().EndDominantDesire(creature);
		}
	}
}

/// The creature a creature native is given, if it is one; else the game's complaint
std::optional<entt::entity> CreatureForNative(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		ScriptMessage("Thing not found!");
		return std::nullopt;
	}
	if (!registry.AllOf<ecs::components::Creature>(thing))
	{
		ScriptMessage("Thing not creature!");
		return std::nullopt;
	}
	return thing;
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	// The desire is wanted above all for the seconds given, and most others are held down as long
	const auto seconds = Popf();
	const auto desire = Pop().intVal;
	const auto creature = CreatureForNative(PopObject());
	if (desire < 0 || desire >= static_cast<int32_t>(creature_desires::k_DesireCount))
	{
		ScriptMessage("Invalid desire");
		return;
	}
	if (creature.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().SetOnlyDesire(*creature, static_cast<creature_desires::Desire>(desire), seconds);
	}
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	const auto creature = CreatureForNative(PopObject());
	if (creature.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ClearOnlyDesire(*creature);
	}
}

void CallPlayerCreature() // 197 CALL_PLAYER_CREATURE
{
	// The player's creature is their primary one: the first they got that is still theirs
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto creature =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	if (!creature.has_value() || !Locator::entitiesRegistry::value().Valid(*creature))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Player {} has no creature", static_cast<int>(player));
		PushObject(entt::null);
		return;
	}
	// It takes a place in the scripts' table
	Locator::scriptObjects::value().Register(*creature, false);
	PushObject(*creature);
}

/// The story moves a creature to a stage of growing up
void MoveCreatureToStage(entt::entity creature, uint32_t stage)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().MoveToStage(creature, stage);
	}
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	// Each lesson of growing up moves the creature to its stage as it begins
	const auto stage = Pop().intVal;
	const auto creature = CreatureForNative(PopObject());
	if (!creature.has_value())
	{
		return;
	}
	const auto valid = creature_development::StageFromScript(stage);
	if (!valid.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_CREATURE_DEV_STAGE: {} is no stage of growing up", stage);
		return;
	}
	MoveCreatureToStage(*creature, *valid);
}

void SwapCreature() // 210 SWAP_CREATURE
{
	// The first creature's mind moves into the second and the second's into the first; the second becomes the player's
	// creature if the first was
	const auto toCreature = PopObject();
	const auto fromCreature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto valid = [&registry](entt::entity object) { return object != entt::null && registry.Valid(object); };
	const auto isCreature = [&registry, &valid](entt::entity object) {
		return valid(object) && registry.AllOf<ecs::components::Creature>(object);
	};
	if (!valid(toCreature) || !valid(fromCreature))
	{
		ScriptMessage("creature for swap not found!");
		return;
	}
	if (!isCreature(fromCreature) || !isCreature(toCreature))
	{
		ScriptMessage("Thing not creature!");
		return;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().SwapMinds(fromCreature, toCreature);
	}
}

void IsLeashed() // 222 IS_LEASHED
{
	// Whether its player's leash is on it; anything but a creature is not leashed
	const auto creature = CreatureForNative(PopObject());
	Pushb(creature.has_value() && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(*creature));
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	// The creature's home becomes the point, on the ground and kept as precisely as a map position. While its player's
	// temple stands the temple's pen is its home again from the next game turn.
	const auto position = PopVec();
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_HOME: thing not found");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_HOME: thing not creature");
		return;
	}
	const auto home = temple_pen::ScriptHome(position, [](glm::vec2 place) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(place) : 0.0f;
	});
	auto* leash = registry.TryGet<ecs::components::CreatureLeash>(creature);
	if (leash == nullptr)
	{
		leash = &registry.Assign<ecs::components::CreatureLeash>(creature);
	}
	leash->home = home;
	leash->homeOnMap = temple_pen::MapPosition(position);
}

/// The creature a fight native is given, or none (logged) when it is not a creature
std::optional<entt::entity> FightCreature(const char* native, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (!registry.AllOf<ecs::components::Creature>(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: thing not creature", native);
		return std::nullopt;
	}
	if (!Locator::creatureFightSystem::has_value())
	{
		return std::nullopt;
	}
	return thing;
}

/// The fight transcript keeps what a script read of the fight state of a creature, when there is a transcript
static void NoteFightRead(std::optional<entt::entity> creature, std::string_view native, double answer)
{
	if (creature.has_value() && Locator::fightTranscript::has_value())
	{
		Locator::fightTranscript::value().ScriptRead(*creature, native, answer);
	}
}

void IsFighting() // 229 IS_FIGHTING
{
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "IS_FIGHTING: thing not found");
	}
	const auto creature = FightCreature("IS_FIGHTING", object);
	// A creature fights, for the scripts, only while it duels
	const bool fighting = creature.has_value() && Locator::creatureFightSystem::value().IsDuelling(*creature);
	NoteFightRead(creature, "IS_FIGHTING", fighting ? 1.0 : 0.0);
	Pushb(fighting);
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	// Whether the spells on a creature put it back as it was once they wear off. The creature is on top of the stack, the
	// flag under it.
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not found");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not creature");
		return;
	}
	auto* spells = registry.TryGet<ecs::components::CreatureSpells>(object);
	if (spells == nullptr)
	{
		spells = &registry.Assign<ecs::components::CreatureSpells>(object);
	}
	spells->spells.reversion = enable;
}

void GetDesire() // 234 GET_DESIRE
{
	// How much a town wants one of its desires, without the boost that scripts give. It gives 0 for a desire number out
	// of range, for an object that is gone and for an object that is not a town.
	const auto desire = Pop().intVal;
	if (desire < 0 || desire >= static_cast<int32_t>(ecs::town_desire::k_Count))
	{
		ScriptMessage("Invalid desire");
		Pushf(0.0f);
		return;
	}
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return;
	}
	if (!registry.AllOf<ecs::components::Town>(town) || !Locator::townDesireSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::townDesireSystem::value().GetRawDesire(town, static_cast<TownDesireInfo>(desire)));
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	// The switch is kept for the next leash put on when no leash is on now
	const auto creature = CreatureForNative(PopObject());
	const auto enable = Pop().intVal != 0;
	if (!creature.has_value() || !Locator::leashSystem::has_value())
	{
		return;
	}
	Locator::leashSystem::value().SetWorks(*creature, enable);
}

/// Debug quickstart: a creature of the species is made at the place and becomes the player's exactly as
/// CREATURE_SET_PLAYER makes the one they pick
void StartWithCreature(CreatureType species, const glm::vec3& position, float yawRadians)
{
	// The scripts give each species its own number, which is not the same as the game's number for the Giant Ape
	const auto creature = CreateScriptObject(ObjectType::Creature, script::property_rules::ScriptCreatureType(species),
	                                         position, 0.0f, 0.0f, yawRadians, 0.0f, 1.0f);
	if (creature == entt::null || creature == static_cast<entt::entity>(0) ||
	    !Locator::entitiesRegistry::value().Valid(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "The debug quickstart could not make the creature");
		return;
	}
	RegisterCreated(creature);
	MakePlayersCreature(creature);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The debug quickstart made the player's creature, species {}, at {:.1f}, {:.1f}",
	                   static_cast<int>(species), position.x, position.z);
}

/// Debug quickstart of the first land: the Cow is made where the creature choice makes the chosen creature (its home in
/// the glade)
void StartWithCow()
{
	const glm::vec3 homePosition {1896.5f, 29.48f, 2520.06f};
	StartWithCreature(CreatureType::Cow, homePosition, 0.0f);
}

/// Whether the player has a creature of their own now
bool PlayerHasCreature()
{
	if (!Locator::leashSystem::has_value())
	{
		return false;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	return Locator::leashSystem::value().PlayersCreature(player).has_value();
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	// The player's creature, as kept when a land was last cleared, comes out at the place, unless they have one already
	const auto position = PopVec();
	// The debug quickstart makes the Cow on the first land only. A later land loads the Cow that came through the vortex.
	if (new_game_choice::MakesDebugCow(Locator::tutorialSkipSystem::value().Get(),
	                                   Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber))
	{
		StartWithCow();
		return;
	}
	// Not in the original game: a developer's direct start of a later land reads no creature file of an earlier game. It
	// has no creature that came through the vortex, so the creature of the species the developer named stands there.
	const auto& start = Locator::landSystem::value().GetNewGameStart();
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const auto source = new_game_choice::LoadsCreatureOfEarlierGame(start, landNumber)
	                        ? ecs::systems::KeptCreatureSource::GameOrProfileFile
	                        : ecs::systems::KeptCreatureSource::GameOnly;
	const bool loaded =
	    Locator::creatureCarryOverSystem::value().LoadPlayersCreature({position.x, position.z}, source).has_value();
	if (const auto species = new_game_choice::DirectStartCreature(start, landNumber, loaded, PlayerHasCreature());
	    species.has_value())
	{
		constexpr float k_ArrivalYawDegrees = 180.0f;
		StartWithCreature(*species, {position.x, position.y, position.z}, glm::radians(k_ArrivalYawDegrees));
	}
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	// The creature is on top of the stack, the kind to omit under it: 0 omits the skills, 1 the miracles. The
	// game reads the value as it is.
	const auto object = PopObject();
	const auto excluded = Pop().uintVal;
	const auto creature = CreatureForNative(object);
	if (creature.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LearnEverythingExcluding(*creature, excluded);
	}
}

/// Where a bone of a creature is as its body is posed now, or where any other thing stands. It gives the origin for
/// an object that is gone.
void PushBodyPointOf(const std::function<std::optional<glm::vec3>(entt::entity)>& creaturePoint)
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	glm::vec3 position(0.0f);
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not found!");
	}
	else
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		const auto standing = transform != nullptr ? transform->position : glm::vec3(0.0f);
		position = registry.AllOf<ecs::components::Creature>(object) ? creaturePoint(object).value_or(standing) : standing;
	}
	PushVec(position);
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	// The creature's groin bone, where its poo comes from
	PushBodyPointOf(ecs::creature_scars::GroinOf);
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	// Whether the creature's leash is tied to the other thing. Either of the two may be the creature: the first given is
	// taken when both are creatures.
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto valid = [&registry](entt::entity thing) { return thing != entt::null && registry.Valid(thing); };
	if (!valid(object) || !valid(target))
	{
		ScriptMessage("Thing not found!");
		Pushb(false);
		return;
	}
	using script::creature_rules::LeashedPair;
	const auto pair = script::creature_rules::WhichIsLeashed(registry.AllOf<ecs::components::Creature>(object),
	                                                         registry.AllOf<ecs::components::Creature>(target));
	if (pair == LeashedPair::Neither)
	{
		ScriptMessage("Thing not creature!");
		Pushb(false);
		return;
	}
	const auto creature = pair == LeashedPair::FirstIsCreature ? object : target;
	const auto other = pair == LeashedPair::FirstIsCreature ? target : object;
	if (registry.Get<const ecs::components::Creature>(creature).owner == PlayerNames::NEUTRAL)
	{
		ScriptMessage("No player for leash");
		Pushb(false);
		return;
	}
	Pushb(Locator::leashSystem::has_value() && Locator::leashSystem::value().TiedTo(creature) == other);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	// The five bonus creatures are only there once unlocked; any other species always is. The game's unlocks came from
	// a program outside it, so none is unlocked unless the debug option says so, as on a normal install
	const auto species = Pop().intVal;
	const auto unlocked =
	    Locator::config::has_value() ? Locator::config::value().unlockedBonusCreatures : creature_swap::Unlocked {};
	Pushb(creature_swap::IsAvailable(species, unlocked));
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	// How many times a creature has carried out an action (by the game's numbering) to its end
	const auto creature = PopObject();
	const auto action = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("No creature");
		Pushf(0.0f);
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("Not creature");
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::creatureMindSystem::has_value()
	          ? static_cast<float>(Locator::creatureMindSystem::value().GetActionCount(creature, action))
	          : 0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	// The leash the creature's player has picked on the temple, whatever the creature wears: -1 when none is picked, and
	// 0 when the player has no leashes on a temple
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("No creature");
		Pushi(0);
		return;
	}
	const auto* body = registry.TryGet<const ecs::components::Creature>(object);
	if (body == nullptr)
	{
		ScriptMessage("Not creature");
		Pushi(0);
		return;
	}
	std::vector<script::creature_rules::LeashPostState> posts;
	registry.Each<const ecs::components::LeashPost>([&posts, body](const ecs::components::LeashPost& post) {
		if (post.owner == body->owner)
		{
			posts.push_back({.type = post.type, .selected = post.selected});
		}
	});
	Pushi(script::creature_rules::LeashTypeForScripts(posts));
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	// A script tells a creature to take notice of reactions or not; every creature starts taking notice
	const auto creature = CreatureForNative(PopObject());
	const auto enable = Pop().intVal != 0;
	if (!creature.has_value())
	{
		return;
	}
	ecs::creature_reaction_notice::SetTakesNotice(Locator::entitiesRegistry::value(), *creature, enable);
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	// A lesson script of growing up says it is running for the creature, or that it is over
	const auto creature = CreatureForNative(PopObject());
	const auto enable = Pop().intVal != 0;
	if (!creature.has_value())
	{
		return;
	}
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(*creature))
	{
		mind->inDevelopmentScript = enable;
	}
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	const auto enable = Pop().intVal != 0;
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetDrawn(enable);
	}
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	const auto target = PopObject();
	const auto creature = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	const bool targetFound = target != entt::null && registry.Valid(target);
	const bool creatureFound = creature != entt::null && registry.Valid(creature);
	if (!targetFound || !creatureFound)
	{
		ScriptMessage("Thing not found!");
		return;
	}
	const bool targetIsCreature = registry.AllOf<ecs::components::Creature>(target);
	const bool creatureIsCreature = registry.AllOf<ecs::components::Creature>(creature);
	if (!targetIsCreature)
	{
		ScriptMessage("Thing not creature!");
	}
	if (!creatureIsCreature)
	{
		ScriptMessage("Thing not creature!");
	}
	// The two creatures become friends. A script that tells them to stop being friends does nothing.
	if (!targetIsCreature || !creatureIsCreature || !enable)
	{
		return;
	}
	for (const auto each : {target, creature})
	{
		if (!registry.AllOf<ecs::components::CreatureFriends>(each))
		{
			registry.Assign<ecs::components::CreatureFriends>(each);
		}
	}
	auto& friendsOfTarget = registry.Get<ecs::components::CreatureFriends>(target).friends;
	auto& friendsOfCreature = registry.Get<ecs::components::CreatureFriends>(creature).friends;
	creature_activity::MakeFriends(friendsOfTarget, entt::to_integral(target), friendsOfCreature, entt::to_integral(creature));
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	// The creature is on top of the stack, the thing under it. The answer is yes only while the creature raises or
	// lowers that thing as a totem.
	const auto creature = PopObject();
	const auto thing = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || thing == entt::null || !registry.Valid(creature) || !registry.Valid(thing))
	{
		ScriptMessage("Thing not found!");
		Pushb(false);
		return;
	}
	const auto* mind = registry.TryGet<const ecs::components::CreatureMindState>(creature);
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("Thing not creature!");
		Pushb(false);
		return;
	}
	const auto object = mind != nullptr ? creature_mind::InteractingWith(mind->idle) : std::nullopt;
	Pushb(object.has_value() && *object == static_cast<uint32_t>(thing));
}

void LoadCreature() // 329 LOAD_CREATURE
{
	// A creature of the species for the player from a mind file, which becomes the creature of the player
	const auto position = PopVec();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto fileName = PopString();
	const auto speciesRow = Pop().uintVal;
	ecs::creature_from_file::Make(
	    {.owner = player, .speciesRow = speciesRow, .fileName = fileName, .place = glm::vec2(position.x, position.z)});
}

void OpposingCreature() // 332 OPPOSING_CREATURE
{
	// The species a god's creature is against the player's creature, from the game's table, as the scripts number them
	const auto god = Pop().uintVal;
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto creature =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	const auto* body =
	    creature.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Creature>(*creature) : nullptr;
	const auto& table = Locator::infoConstants::value().scriptOpposingCreature;
	const auto row = body != nullptr ? script::property_rules::ScriptCreatureType(body->species) : 0;
	constexpr uint32_t k_Gods = 3;
	if (body == nullptr || row >= table.size() || god >= k_Gods)
	{
		// The game reads its table regardless (and without a creature fails)
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No opposing creature for god {}", god);
		Pushi(0);
		return;
	}
	static_assert(std::tuple_size_v<decltype(table.at(row).byGod)> == k_Gods);
	Pushi(static_cast<int32_t>(table.at(row).byGod.at(god)));
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	PushBodyPointOf(ecs::creature_scars::BellyOf);
}

void SetCreatureCreedProperties() // 338 SET_CREATURE_CREED_PROPERTIES
{
	// A creature gets a creed glow of a scale and a power on its body. The time has no effect.
	[[maybe_unused]] const auto time = Popf();
	const auto power = Popf();
	const auto scale = Popf();
	const auto hand = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		ScriptMessage("Thing not creature!");
		return;
	}
	// The game does not examine the place, and a number out of range changes memory after the glows. Ignore it.
	const auto place = creature_hand_glow::PlaceOf(hand);
	if (!place.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_CREED_PROPERTIES: no hand glow {}", hand);
		return;
	}
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().SetCreedPower(object, *place, scale, power);
	}
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	// The creature takes a help text's words as its name: number 0, or one past the last, gives the first text's
	const auto text = Pop().uintVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	auto* creature = registry.TryGet<ecs::components::Creature>(object);
	if (creature == nullptr)
	{
		ScriptMessage("Thing not creature!");
		return;
	}
	creature->name = ecs::creature_naming::HelpWords(text);
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	// The game asks for a town here, but then clears what a villager keeps in mind of its script's work, on any thing it
	// gets (vanilla bugs 154). A town keeps nothing of that kind. A creature's mind is never touched.
	const auto thing = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing) || !registry.AllOf<ecs::components::Town>(thing))
	{
		ScriptMessage("Thing not valid");
	}
	if (thing != entt::null && registry.Valid(thing))
	{
		registry.Remove<ecs::components::ScriptRemind>(thing);
	}
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	// As the player's leash key does, refused and logged when they have no creature to lead
	Locator::leashSystem::value().PressKey(player, creature_leash::LeashKey::Leash);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	// On, the computer fights the creature until the player makes a move for it; off, nobody does and it makes only the
	// moves queued for it
	const auto object = PopObject();
	const bool enable = Pop().intVal != 0;
	if (const auto creature = FightCreature("SET_CREATURE_AUTO_FIGHTING", object))
	{
		Locator::creatureFightSystem::value().SetAutoFighting(*creature, enable);
	}
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	const auto creature = FightCreature("IS_AUTO_FIGHTING", PopObject());
	const bool autoFighting = creature.has_value() && Locator::creatureFightSystem::value().IsAutoFighting(*creature);
	NoteFightRead(creature, "IS_AUTO_FIGHTING", autoFighting ? 1.0 : 0.0);
	Pushb(autoFighting);
}

/// A move a script adds to the end of a creature's fight queue, numbered as the scripts number them
void QueueScriptFightMove(const char* native, uint32_t value)
{
	const auto creature = FightCreature(native, PopObject());
	const auto move = creature_fight::ScriptMove(value);
	if (creature.has_value() && move.has_value())
	{
		Locator::creatureFightSystem::value().QueueMove(*creature, *move, false);
	}
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	const auto move = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_MOVE", move);
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	const auto spell = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_SPELL", spell | creature_fight::k_ScriptSpellBit);
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	const auto step = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_STEP", step | creature_fight::k_ScriptAnimationBit);
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	const auto creature = FightCreature("GET_CREATURE_FIGHT_ACTION", PopObject());
	const auto action =
	    creature.has_value() ? static_cast<int32_t>(Locator::creatureFightSystem::value().CurrentFightAction(*creature)) : 0;
	NoteFightRead(creature, "GET_CREATURE_FIGHT_ACTION", action);
	Pushi(action);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	const auto creature = FightCreature("CREATURE_FIGHT_QUEUE_HITS", PopObject());
	const auto hits =
	    creature.has_value() ? static_cast<float>(Locator::creatureFightSystem::value().QueuedBlows(*creature)) : 0.0f;
	NoteFightRead(creature, "CREATURE_FIGHT_QUEUE_HITS", hits);
	Pushf(hits);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	// Whether the player may zoom out of the fight view, and the view ends by itself after a fight
	const bool allowed = Pop().intVal != 0;
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetFightExit(allowed);
	}
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	// Whether the temple's creature room shows the player's creature, from the next visit on
	const auto enable = Pop().intVal != 0;
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetCreatureShown(enable);
	}
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	// Whether a creature can walk to the position: the test that a creature's walk does for its destination
	const auto position = PopVec();
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		Pushb(false);
		return;
	}
	const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
	Pushb(land.IsValid({position.x, position.z}, creature_route::k_DestinationClearance));
}

void CreatureAutoscale() // 452 CREATURE_AUTOSCALE
{
	// Keeps a creature growing or shrinking towards a share of the player's creature's size, or stops it
	const auto share = Popf();
	const auto creature = PopObject();
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	auto* body =
	    creature != entt::null && registry.Valid(creature) ? registry.TryGet<ecs::components::Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		ScriptMessage("Thing should be valid!");
		return;
	}
	body->autoScale = enable ? std::optional(share) : std::nullopt;
}

} // namespace openblack::chlapi
