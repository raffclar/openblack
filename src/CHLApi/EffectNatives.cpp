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

#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/Mists.h"
#include "3D/TempleInteriorInterface.h"
#include "Sound/GameMusic.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Creature/CreatureDesires.h"
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
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptPoison.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/ScriptWeatherRules.h"
#include "ECS/SpellCreator.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandGrabRules.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/ScriptCast.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptCastHistory.h"
#include "ScriptHeaders/ScriptCreateRules.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptLessonNatives.h"
#include "Temple/TempleEntrancePoint.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;
using openblack::Locator;
using openblack::ecs::components::Transform;
using openblack::lhvm::DataType;
using openblack::script::ObjectType;

ecs::components::Player* ScriptPlayer(float number);

/// A script effect's seconds as game turns: a whole number of turns a second, as the game's turn length gives it
int SpecialEffectTurns(float seconds)
{
	constexpr auto k_TurnsPerSecond = 1000 / static_cast<int>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
	return static_cast<int>(seconds * static_cast<float>(k_TurnsPerSecond));
}

/// One of the spot visuals started by a script, at a point or on an object that ends it when it goes, for the first
/// player; numbers past the table's last start nothing
uint32_t StartScriptSpotVisual(int32_t effect, glm::vec3 position, int turns, entt::entity owner)
{
	constexpr int32_t k_LastSpotVisual = 49;
	if (effect < 0 || effect > k_LastSpotVisual || !Locator::effectRequestSystem::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	auto& effects = Locator::effectRequestSystem::value();
	// The visual shows the colour of the first player, the same on every computer (as the script player 0)
	const auto handle = effects.Start({
	    .type = static_cast<SpotVisualType>(effect),
	    .position = position,
	    .turns = turns,
	    .owner = owner,
	    .player = Locator::playerSystem::has_value()
	                  ? std::optional(static_cast<int>(Locator::playerSystem::value().GetLocalPlayer()))
	                  : std::nullopt,
	});
	if (!effects.IsRunning(handle))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	return handle;
}

/// The thing a script holds for a visual it started, taking its place in the scripts' table as made by a script; none
/// when no visual was started. The script holds the thing, never the visual's own number.
static entt::entity ScriptSpotVisualThing(uint32_t effect, glm::vec3 position)
{
	if (effect == ecs::systems::k_NoEffectHandle)
	{
		return entt::null;
	}
	const auto thing = ecs::script_spot_visuals::MakeThing(Locator::entitiesRegistry::value(), effect, position);
	RegisterCreated(thing);
	return thing;
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, entt::null), position));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto object = PopObject();
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<Transform>(object) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SPECIAL_EFFECT_OBJECT: thing invalid, spell not created");
		PushObject(entt::null);
		return;
	}
	// The effect stands on the object and ends when it goes
	const auto position = transform->position;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, object), position));
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	const auto calm = Popf();
	const auto outer = Popf();
	const auto inner = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && ecs::script_flocks::IsFlock(registry, object))
	{
		// A nought leaves that distance as it was; the calm is kept whatever it is
		auto& flock = registry.Get<ecs::components::Flock>(object);
		if (outer != 0.0f)
		{
			flock.domainRadius = static_cast<uint16_t>(map_coords::FtoL(outer));
		}
		if (inner != 0.0f)
		{
			flock.flockDistance = static_cast<uint16_t>(map_coords::FtoL(inner));
		}
		flock.calm = map_coords::FtoL(calm);
		return;
	}
	// A weather thing's storm takes the inner and outer distances as they are; its calm is not used
	if (object != entt::null && Locator::weatherSystem::has_value() &&
	    Locator::weatherSystem::value().ChangeWeatherThing(
	        object, [=](ecs::components::Storm& storm) { ecs::script_weather::SetInnerOuter(storm, inner, outer); }))
	{
		return;
	}
	ScriptMessage("Invalid thing for Changing Variables");
}

/// A weather native's change to the storm of the weather thing it was given; a thing that is no weather thing is told
/// of and left alone
void ChangeWeatherThing(entt::entity thing, const std::function<void(ecs::components::Storm&)>& change)
{
	if (thing == entt::null || !Locator::entitiesRegistry::value().Valid(thing))
	{
		return;
	}
	if (!Locator::weatherSystem::has_value() || !Locator::weatherSystem::value().ChangeWeatherThing(thing, change))
	{
		ScriptMessage("Weather properties on non weather thing");
	}
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	const auto fallSpeed = Popf();
	const auto overcast = Popf();
	const auto snowfall = Popf();
	const auto rainfall = Popf();
	const auto temperature = Popf();
	ChangeWeatherThing(PopObject(), [=](ecs::components::Storm& storm) {
		ecs::script_weather::SetWeather(storm, temperature, rainfall, snowfall, overcast, fallSpeed);
	});
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	const auto forkMax = Popf();
	const auto forkMin = Popf();
	const auto sheetMax = Popf();
	const auto sheetMin = Popf();
	ChangeWeatherThing(PopObject(), [=](ecs::components::Storm& storm) {
		ecs::script_weather::SetLightning(storm, sheetMin, sheetMax, forkMin, forkMax);
	});
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	const auto fadeSeconds = Popf();
	const auto lastsFor = Popf();
	ChangeWeatherThing(PopObject(),
	                   [=](ecs::components::Storm& storm) { ecs::script_weather::SetTimeFade(storm, lastsFor, fadeSeconds); });
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	const auto height = Popf();
	const auto shade = Popf();
	const auto count = Popf();
	ChangeWeatherThing(PopObject(),
	                   [=](ecs::components::Storm& storm) { ecs::script_weather::SetClouds(storm, count, shade, height); });
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	const auto container = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ScriptMessage("Thing not valid");
		Pushf(0.0f);
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("No Loop funtion for type");
		Pushf(0.0f);
		return;
	}
	// How many of its members are poisoned
	const auto members = ContainerMembers(registry, container);
	Pushf(static_cast<float>(std::ranges::count_if(
	    members, [&registry](entt::entity member) { return ecs::script_poison::IsPoisoned(registry, member); })));
}

void IsPoisoned() // 162 IS_POISONED
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		Pushb(false);
		return;
	}
	Pushb(ecs::script_poison::IsPoisoned(registry, object));
}

/// The first member of a script container that is of the type and subtype, and poisoned or else not poisoned, as the scripts
/// ask. It is none when there is none, and the script is told of it.
static void CallPoisonedStateIn(bool poisoned, std::string_view notFound)
{
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = PopObject();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto fail = [&notFound]() {
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "{}", notFound);
		Pusho(0);
	};
	if (container == entt::null || !registry.Valid(container))
	{
		fail();
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Cannot look in object");
		fail();
		return;
	}
	const auto objectType = static_cast<ObjectType>(type);
	if (registry.AllOf<ecs::components::Town>(container) && objectType != ObjectType::Villager &&
	    objectType != ObjectType::VillagerChild)
	{
		ScriptMessage("Looking for strange type in Town");
		fail();
		return;
	}
	for (const auto member : ContainerMembers(registry, container))
	{
		if (!registry.Valid(member) || (excludingScripted && registry.AllOf<ecs::components::InScript>(member)) ||
		    ecs::script_poison::IsPoisoned(registry, member) != poisoned)
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, member);
		if (kind.has_value() && ecs::script_find::Matches(*kind, objectType, subtype))
		{
			Locator::scriptObjects::value().Register(member, false);
			Pusho(static_cast<uint32_t>(member));
			return;
		}
	}
	fail();
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	CallPoisonedStateIn(true, "Cannot find poisoned in");
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	CallPoisonedStateIn(false, "Cannot find not poisoned in");
}

void IsOnFire() // 170 IS_ON_FIRE
{
	const auto object = PopObject();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object));
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	const auto radius = Popf();
	const auto position = PopVec();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsFireNear(position, radius));
}

void SetPoisoned() // 173 SET_POISONED
{
	const auto object = PopObject();
	const auto value = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	// A group poisons or cures each of its members
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			ecs::script_poison::SetPoisoned(registry, member, value);
		}
		return;
	}
	if (!registry.AllOf<Transform>(object) || registry.AllOf<ecs::components::ScriptMarker>(object))
	{
		ScriptMessage("Thing not object");
		return;
	}
	ecs::script_poison::SetPoisoned(registry, object, value);
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	const auto temperature = Popf();
	const auto object = PopObject();
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetTemperature(object, temperature, entt::null);
	}
}

void SetOnFire() // 175 SET_ON_FIRE
{
	const auto burnSpeed = Popf();
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (!Locator::fireSystem::has_value())
	{
		return;
	}
	// Set alight at the speed, or brought back to the air's temperature
	if (enable)
	{
		Locator::fireSystem::value().SetOnFire(object, burnSpeed);
	}
	else
	{
		Locator::fireSystem::value().PutOut(object);
	}
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopObject();
	const auto spell = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const ecs::components::Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid thing");
		Pusho(0);
		return;
	}
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl: on an object as its seed casts, else at the thing's place
	const auto type = static_cast<MagicType>(spell);
	const auto& info = Locator::infoConstants::value();
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(info, type).initialChants, transform->position, from,
	                                        radius, duration, curl);
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool object = IsScriptObject(registry, target);
	const bool onObject = object && seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	auto& magicSystem = Locator::magicSystem::value();
	const auto entity = onObject ? magicSystem.CastOnObject(type, PlayerNames::NEUTRAL, target, cast.cast, cast.info)
	                             : magicSystem.CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopVec();
	const auto spell = Pop().intVal;
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		// The script still takes a result off the stack
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl
	const auto type = static_cast<MagicType>(spell);
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).initialChants,
	                                        target, from, radius, duration, curl);
	const auto entity = Locator::magicSystem::value().CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	// Finds a miracle already at a point, casting nothing: for either shield the first shield standing over the point,
	// else the newest miracle of the kind whose last event was strictly within the radius
	const auto radius = Popf();
	const auto position = PopVec();
	const auto spell = Pop().intVal;
	std::optional<entt::entity> found;
	if (spell == static_cast<int32_t>(MagicType::Shield) || spell == static_cast<int32_t>(MagicType::PhysicalShield))
	{
		if (Locator::magicShieldSystem::has_value())
		{
			found = Locator::magicShieldSystem::value().ShieldAt(position);
		}
	}
	else if (Locator::magicSystem::has_value())
	{
		found = Locator::magicSystem::value().SpellAt(static_cast<MagicType>(spell), position, radius);
	}
	Pusho(found.has_value() ? static_cast<uint32_t>(*found) : 0);
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// A miracle's radius changes at once, and its effect and its shield follow it. Anything that is not a miracle does
	// not change.
	const auto radius = Popf();
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().SetSpellRadius(object, radius);
	}
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	const auto player = Popf();
	const auto magic = Pop().intVal;
	const bool enable = Pop().intVal != 0;
	if (!script::lesson_natives::ValidMagic(magic))
	{
		ScriptMessage(script::lesson_natives::k_InvalidMagic);
		return;
	}
	// One more thing enables the magic type for the player, or one fewer does. A player that is not there is ignored.
	if (ScriptPlayer(player) != nullptr && Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().SetMagicTypeEnabled(ScriptPlayerName(static_cast<int32_t>(player)),
		                                                      static_cast<MagicType>(magic), enable);
	}
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	const auto player = Popf();
	const auto magic = Pop().intVal;
	if (!script::lesson_natives::ValidMagic(magic))
	{
		ScriptMessage(script::lesson_natives::k_InvalidMagic);
	}
	const auto* component = script::lesson_natives::ValidMagic(magic) ? ScriptPlayer(player) : nullptr;
	Pushb(script::lesson_natives::HasPlayerMagic(magic, component != nullptr ? &component->miracles : nullptr));
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	// The vortex starts to fade out, whatever it was doing. Nothing given at all is an error; a thing that isn't a vortex
	// is one too.
	DataType type {};
	const auto value = Pop(type);
	// A script's object 0 is none, as PopObject reads it
	const auto vortex = value.uintVal == 0 ? entt::entity {entt::null}
	                                       : Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(value.uintVal));
	if (vortex == entt::null || !Locator::entitiesRegistry::value().Valid(vortex))
	{
		if (type == DataType::None)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
		}
		return;
	}
	if (!Locator::vortexSystem::value().StartFadeOut(vortex))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not vortex");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
	}
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// Whether the thing made the miracle. With no thing, the game gives the same message as for a thing that is not a
	// miracle.
	const auto caster = PopObject();
	const auto spellObject = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (spellObject == entt::null || !registry.Valid(spellObject))
	{
		ScriptMessage("Spell Object does not exist");
		Pushb(false);
		return;
	}
	const auto* spell = registry.TryGet<const ecs::components::Spell>(spellObject);
	if (spell == nullptr || caster == entt::null)
	{
		ScriptMessage("Object should be a spell");
		Pushb(false);
		return;
	}
	Pushb(ecs::spell_creator::CastBy(*spell, caster));
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// Whether a storm with no clouds of its own, as the wind miracle makes, is near the point. Every storm counts, also
	// one that ended.
	const auto radius = Popf();
	const auto position = PopVec();
	bool found = false;
	Locator::entitiesRegistry::value().Each<const ecs::components::Storm>(
	    [&found, position, radius](const ecs::components::Storm& storm) {
		    found = found ||
		            ecs::script_weather::WindMagicNear(storm.position, storm.outerRadius, storm.cloudCount, position, radius);
	    });
	Pushb(found);
}

void CreateMist() // 263 CREATE_MIST
{
	// A puff of mist at the point given, in the colour given, one over the height ratio times wider than tall seen level
	const auto heightRatio = Popf();
	const auto transparency = Popf();
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	const auto scale = Popf();
	const auto position = PopVec();
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	const auto mist = ecs::archetypes::MistArchetype::Create(
	    position, position.y - ground, mists::ScriptColour(red, green, blue, transparency), scale, 1.0f / heightRatio);
	if (mist == entt::null)
	{
		ScriptMessage("Thing not created");
		PushObject(entt::null);
		return;
	}
	RegisterCreated(mist);
	PushObject(mist);
}

void SetMistFade() // 264 SET_MIST_FADE
{
	// The mist's size and alpha (0 to 255) go from their starts to their ends over the seconds given, a step each turn
	const auto duration = Popf();
	const auto endTransparency = Popf();
	const auto startTransparency = Popf();
	const auto endScale = Popf();
	const auto startScale = Popf();
	const auto mist = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (mist == entt::null || !registry.Valid(mist))
	{
		ScriptMessage("Thing deleted");
		return;
	}
	if (!registry.AllOf<ecs::components::Mist>(mist))
	{
		ScriptMessage("Must be mist object!");
		return;
	}
	Locator::mistSystem::value().SetFade(mist, startScale, endScale, startTransparency, endTransparency, duration);
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	// The seconds since the last cast of the player. The record of the last cast always keeps its start values.
	const auto* player = ScriptPlayer(Popf());
	if (player == nullptr)
	{
		Pushf(script::cast_history::k_NoPlayerTime);
		return;
	}
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	constexpr auto k_MillisecondsPerTurn = static_cast<int32_t>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
	Pushf(script::cast_history::SecondsSinceCast(static_cast<uint32_t>(turn), {}, k_MillisecondsPerTurn));
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	// The magic type of the last cast of the player, from the record that always keeps its start values
	const auto* player = ScriptPlayer(Popf());
	Pushi(player != nullptr ? static_cast<int32_t>(script::cast_history::LastCast {}.type) : 0);
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	// The place of the last cast of the player, from the record that always keeps its start values. The height is the
	// height of the land at the place, plus the height of the record above the land.
	const auto* player = ScriptPlayer(Popf());
	glm::vec3 place {0.0f};
	if (player != nullptr)
	{
		constexpr script::cast_history::LastCast k_Cast {};
		const map_coords::MapCoords coords {k_Cast.x, k_Cast.z};
		place = Locator::terrainSystem::has_value()
		            ? map_coords::ToWorld(Locator::terrainSystem::value(), coords)
		            : glm::vec3(map_coords::ToMetres(coords).x, 0.0f, map_coords::ToMetres(coords).y);
		place.y += k_Cast.height;
	}
	PushVec(place);
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	// The visual's rules also act on the point, such as a beam that ends there
	const auto position = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_SPOT_VISUAL_TARGET_POS: thing not valid");
		return;
	}
	// A thing that holds no visual of a script takes no target
	if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object);
	    effect.has_value() && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddTargetPosition(*effect, position);
	}
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// The visual's rules also act on the target, such as a beam that follows it while it moves
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_SPOT_VISUAL_TARGET_OBJECT: thing not valid");
		return;
	}
	// No target, or a thing that holds no visual of a script, adds nothing
	if (!registry.Valid(target))
	{
		return;
	}
	if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object);
	    effect.has_value() && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddTarget(*effect, target);
	}
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// Any value other than 0 hides the sun and its glare, and 0 shows them. The name of the native says the opposite
	// (see docs/vanilla-bugs.md).
	const auto hide = Pop().uintVal != 0;
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().SetSunHidden(hide);
	}
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetHurtByFire(object, enable);
	}
}

/// Whether a script's thing is one in the world with a component
template <typename Component>
static bool IsThingWith(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Component>(thing);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// A vortex that leads into the land is told the town those who come out join, where its animals gather in a flock,
	// kept within a distance and a radius, and the flock its people follow. A thing given for the town or the flock that
	// is none takes their place as nothing. Without a vortex nothing is set; a vortex without a town is still told the rest
	const auto flock = PopObject();
	const auto radius = Popf();
	const auto distance = Popf();
	const auto place = PopVec();
	const auto town = PopObject();
	const auto vortex = PopObject();
	const bool isVortex = IsThingWith<ecs::components::Vortex>(vortex);
	const bool isTown = IsThingWith<ecs::components::Town>(town);
	if (!isTown || !isVortex)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		if (!isVortex)
		{
			return;
		}
	}
	Locator::vortexSystem::value().SetArrival(vortex,
	                                          {.town = isTown ? town : entt::null,
	                                           .gatheringPlace = place,
	                                           .flockDistance = distance,
	                                           .flockRadius = radius,
	                                           .flock = IsThingWith<ecs::components::Flock>(flock) ? flock : entt::null});
}

/// Whether an icon at the sites of a player's temple has prayer power in its store, for one magic type or for any
bool PlayersIconStores(PlayerNames player, std::optional<MagicType> type)
{
	if (!Locator::worshipSiteSystem::has_value() || !Locator::spellIconSystem::has_value())
	{
		return false;
	}
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	return temple != entt::null && Locator::spellIconSystem::value().AnyIconStores(temple, type);
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	// A miracle charges when an icon of the player has prayer power in its store, whatever hand asked for it
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(PlayersIconStores(player, std::nullopt));
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	// As IS_SPELL_CHARGING, for the icons whose plain miracle is of the magic type
	const auto type = static_cast<MagicType>(Pop().intVal);
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(PlayersIconStores(player, type));
}

/// Whether a thing is a worship site
bool IsWorshipSite(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<ecs::components::WorshipSite>(object);
}

void GameSetMana() // 355 GAME_SET_MANA
{
	// A worship site, or the site of a spell icon, gets this prayer power in its battery. Its icons lose their stores.
	const auto mana = Popf();
	const auto object = PopObject();
	const bool icon = Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().IsIcon(object);
	if (!IsWorshipSite(object) && !icon)
	{
		ScriptMessage("Thing not valid!");
		return;
	}
	if (!Locator::spellIconSystem::has_value() || !Locator::worshipSiteSystem::has_value())
	{
		return;
	}
	auto& icons = Locator::spellIconSystem::value();
	const auto site = IsWorshipSite(object) ? object : icons.SiteOfIcon(object);
	if (site == entt::null)
	{
		return;
	}
	icons.EmptyIconStores(site);
	Locator::worshipSiteSystem::value().SetStoredChants(site, mana);
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	// A dispenser is given the miracle it holds and the seconds between its bubbles; for none it keeps its building's
	// period, and with no turns between them it is turned off
	const auto seconds = Popf();
	const auto number = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return;
	}
	const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(object);
	if (dispenser == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing must be a dispenser");
		return;
	}
	const auto type = script::create_rules::MagicTypeFromScript(number);
	if (!type.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic type {}", number);
		return;
	}
	const auto& building = Locator::infoConstants::value().abode.at(static_cast<size_t>(dispenser->building));
	const auto turns = script::create_rules::DispenserTurns(seconds, building.timeEachMobileObjectTakesToProduce);
	Locator::magicSystem::value().SetDispenserMagic(object, *type, turns);
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// Whether a creature is under a spell of the magic type now. A magic type that casts no spell on a creature gives no.
	const auto spell = static_cast<MagicType>(Pop().intVal);
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Object dead man!");
		Pushb(false);
		return;
	}
	const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(object);
	if (!registry.AllOf<ecs::components::Creature>(object) || spells == nullptr)
	{
		ScriptMessage("Object must be a creature!");
		Pushb(false);
		return;
	}
	const auto creatureSpell = creature_spells::SpellOf(spell);
	Pushb(creatureSpell.has_value() && spells->spells.IsActive(*creatureSpell));
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	// A town gets a magic type or loses it. A power-up level does not give its plain miracle with it.
	const auto object = PopObject();
	const auto type = static_cast<MagicType>(Pop().intVal);
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (!registry.AllOf<ecs::components::Town>(object))
	{
		ScriptMessage("Object should be town");
		return;
	}
	if (!Locator::spellIconSystem::has_value())
	{
		return;
	}
	auto& icons = Locator::spellIconSystem::value();
	if (enable)
	{
		icons.TownHolds(object, type);
	}
	else
	{
		icons.TownReleases(object, type);
	}
}

ecs::components::Player* ScriptPlayer(float number)
{
	const auto name = ScriptPlayerName(static_cast<int32_t>(number));
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto entity = Locator::playerSystem::value().FindPlayer(name).value_or(entt::null);
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<ecs::components::Player>(entity) : nullptr;
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	auto* player = ScriptPlayer(Popf());
	const auto resistance = Pop().uintVal;
	// What the player's hand throws flies without the air's drag while it is set
	if (player != nullptr)
	{
		player->windResistance = resistance;
	}
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	const auto* player = ScriptPlayer(Popf());
	// Of its two arguments the game reads only the player
	Pop();
	Pushi(player != nullptr ? static_cast<int32_t>(player->windResistance) : 0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetClimateSystemEnabled(enable);
	}
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetStormCreationEnabled(enable);
	}
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	// The prayer power that a miracle of the magic type costs to create
	const auto spell = Pop().intVal;
	if (spell < 0 || static_cast<size_t>(spell) >= magic::k_MagicTypeCount || !Locator::infoConstants::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), static_cast<MagicType>(spell)));
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	const auto radius = Popf();
	const auto position = PopVec();
	// A miracle's storm comes back over its clouds at the next step, fading in again
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().KillStormsInArea(position, radius);
	}
}

void GetMana() // 422 GET_MANA
{
	// The prayer power in the battery of a worship site. Only a site has it, not a spell icon.
	const auto object = PopObject();
	if (!IsWorshipSite(object))
	{
		ScriptMessage("Thing not valid!");
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().StoredChants(object) : 0.0f);
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	// At the sites of the player's temple, each icon that the first player charges stops its charge
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (!Locator::playerSystem::has_value() || !Locator::worshipSiteSystem::has_value() ||
	    !Locator::spellIconSystem::has_value())
	{
		return;
	}
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	if (temple == entt::null)
	{
		return;
	}
	Locator::spellIconSystem::value().CancelChargesFor(temple, Locator::playerSystem::value().GetLocalPlayer());
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetCanBeSetOnFire(object, enable);
	}
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	// The icon of a magic type's first seed at the worship sites of a temple
	const auto temple = PopObject();
	const auto type = static_cast<MagicType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(temple) || !registry.AllOf<ecs::components::Temple>(temple))
	{
		ScriptMessage("Invalid Temple");
		Pusho(0);
		return;
	}
	if (!Locator::spellIconSystem::has_value() || !Locator::infoConstants::has_value())
	{
		Pusho(0);
		return;
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(Locator::infoConstants::value(), type);
	const auto icon = seed.has_value() ? Locator::spellIconSystem::value().IconInTemple(temple, *seed) : entt::null;
	if (icon == entt::null)
	{
		Pusho(0);
		return;
	}
	// The icon takes a place in the scripts' table, not as a thing that a script made
	Locator::scriptObjects::value().Register(icon, false);
	Pusho(static_cast<uint32_t>(icon));
}

} // namespace openblack::chlapi
