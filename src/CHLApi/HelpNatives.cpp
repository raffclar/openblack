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
#include "Creature/CreatureScriptPlay.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/ScriptTexts.h"
#include "Help/DialogueText.h"
#include "Help/ScriptHelpText.h"
#include "Help/ScriptSpirits.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptSwitchChanges.h"

namespace openblack::chlapi
{

using openblack::Locator;

/// The task whose cinema bars are in, 0 for none
uint32_t WideScreenOwner()
{
	return Locator::cinematicDirectorSystem::value().GetWideScreenOwner();
}

/// Every help script stops, in the world, the temple or a multiplayer game
void StopHelpScripts()
{
	Locator::vm::value().StopTasksOfType(lhvm::ScriptType::Help | lhvm::ScriptType::TempleHelp |
	                                     lhvm::ScriptType::MultiplayerHelp);
}

/// The task with the dialogue gives it back: the cinema bars go, and the advisors are sent home. The bars go whoever
/// brought them in, another task included, as they do in the game
void ReleaseDialogue(uint32_t task)
{
	if (Locator::dialogueControlSystem::value().Release(task, TaskType(task) == lhvm::ScriptType::Help))
	{
		Locator::cinematicDirectorSystem::value().SetWideScreen(false, 0);
	}
}

/// The advisors the scripts drive, nothing before they are loaded
help::spirits::AdvisorSpiritController* ScriptAdvisors()
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return nullptr;
	}
	return &Locator::advisorSystem::value().GetController();
}

/// The advisor a script's spirit means, 1 the good one or 2 the evil one, by the first player's alignment for those
/// that go by it. A random spirit comes from the random numbers of the help, which no frame draws.
int32_t ScriptHelpSpirit(int32_t scriptSpirit)
{
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto alignment =
	    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(player) : 0.0f;
	return help::script_spirits::HelpSpiritOf(scriptSpirit, help::script_spirits::DiscreteAlignment(alignment), []() {
		return Locator::helpMessageSystem::has_value() ? Locator::helpMessageSystem::value().HelpRand(100) : 0u;
	});
}

/// The last of the advisors' animations a script can play
constexpr int32_t k_LastSpiritAnim = 80;

/// A script's place on the screen, as fractions across and down: each out of 0 to 1 is reported, down first, and the
/// script goes on
void CheckScreenFractions(float x, float y)
{
	if (!help::script_spirits::IsScreenFraction(y))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid Y");
	}
	if (!help::script_spirits::IsScreenFraction(x))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid X");
	}
}

void SpiritEject() // 007 SPIRIT_EJECT
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		// A help script's advisor appears where it is rather than flying out
		advisors->SpiritEject(spirit, TaskType(CurrentTask()) == lhvm::ScriptType::Help);
	}
}

void SpiritHome() // 008 SPIRIT_HOME
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		// A help script's advisor vanishes rather than flying home
		advisors->SpiritHome(spirit, TaskType(CurrentTask()) == lhvm::ScriptType::Help);
	}
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	// Only exactly 1 points in the world
	const auto inWorld = Pop().uintVal == 1;
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritPointPosition(spirit, position, inWorld);
	}
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	// The advisor comes out and keeps pointing at the top of the thing, wherever it goes, until it is gone or told
	// otherwise. Only exactly 1 points in the world.
	const auto inWorld = Pop().uintVal == 1;
	const auto target = PopObject();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (target == entt::null || !Locator::entitiesRegistry::value().Valid(target))
	{
		ObjectNoLongerValid();
		return;
	}
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritPointObject(spirit, entt::to_integral(target), inWorld);
	}
}

void RunText() // 013 RUN_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto text = Pop().uintVal;
	const auto singleLine = Pop().uintVal != 0;
	if (!Locator::helpTextSystem::value().RunText(singleLine, text, withInteraction))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid text");
	}
}

void TempText() // 014 TEMP_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto words = PopString();
	const auto singleLine = Pop().uintVal != 0;
	// The game warns that words written into a script are shown rather than a translated help text
	SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Development text being used in game: {}", words);
	Locator::helpTextSystem::value().TempText(singleLine, words, withInteraction);
}

void TextRead() // 015 TEXT_READ
{
	Pushb(Locator::helpTextSystem::value().IsTextRead());
}

/// The text that GET_ACTION_TEXT_FOR_OBJECT gives: "Warning: this text shouldn't come up"
constexpr int32_t k_ActionTextForObject = 828;

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	// The game always gives the same text, which says that the text must not show. It does not remove the object from
	// the stack.
	Pushi(k_ActionTextForObject);
}

void StartDialogue() // 120 START_DIALOGUE
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	const auto task = CurrentTask();
	auto owner = dialogue.GetOwner();
	if (owner == 0)
	{
		dialogue.SendSpiritsHome(false);
	}
	else
	{
		if (owner == task)
		{
			ScriptMessage("Script Asking For Dialogue Control It already has! - Dangerous");
			Pushb(true);
			return;
		}
		// A story script takes the dialogue from a help script, whose tasks are stopped and so give it back
		if (TaskType(owner) == lhvm::ScriptType::Help && TaskType(task) == lhvm::ScriptType::Script)
		{
			StopHelpScripts();
			owner = dialogue.GetOwner();
		}
		if (owner != 0)
		{
			Pushb(false);
			return;
		}
	}
	// The script carries on even when another task's cinema bars keep the dialogue from it
	dialogue.Request(task, WideScreenOwner());
	Pushb(true);
}

void EndDialogue() // 121 END_DIALOGUE
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	const auto task = CurrentTask();
	if (dialogue.GetOwner() != task)
	{
		return;
	}
	dialogue.SendSpiritsHome(TaskType(task) == lhvm::ScriptType::Help);
	ReleaseDialogue(task);
	ApplySwitchChanges(script::switches::DialogueEnded());
	// The word of the sung line goes back to none
	if (auto* scriptMusic = WorldScriptMusic())
	{
		scriptMusic->ResetWord();
	}
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	Pushb(!Locator::dialogueControlSystem::value().IsControlled(WideScreenOwner()));
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	const auto time = Popf();
	const auto anim = Pop().intVal;
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	// None of the checks stops it
	if (anim < 0 || anim > k_LastSpiritAnim)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid enum");
	}
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritPlayAnim(spirit, x, y, static_cast<uint32_t>(anim), time);
	}
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto* advisors = ScriptAdvisors();
	Pushb(advisors == nullptr || !advisors->SpiritPlayingAnim(spirit));
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritCling(spirit, x, y);
	}
}

void FlySpirit() // 167 FLY_SPIRIT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritFly(spirit, x, y);
	}
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	Pushb(Locator::chlapi::value().IsHelpSystemOn());
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	// As TEMP_TEXT, with a number that the $D or $P code of the words writes into them. The game gives no warning about
	// the words of the script here.
	const auto withInteraction = Pop().intVal;
	const auto number = Popf();
	const auto words = PopString();
	const auto singleLine = Pop().uintVal != 0;
	if (Locator::time::has_value() && Locator::time::value().IsNetworkGame() && withInteraction != 0)
	{
		ScriptMessage("This is not multiplayer friendly");
	}
	Locator::helpTextSystem::value().TempTextWithNumber(singleLine, words, number, withInteraction);
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	const auto withInteraction = Pop().intVal;
	const auto number = Popf();
	const auto text = Pop().uintVal;
	const auto singleLine = Pop().uintVal != 0;
	if (!Locator::helpTextSystem::value().RunTextWithNumber(singleLine, text, number, withInteraction))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid text");
	}
}

void SpiritSpeaks() // 246 SPIRIT_SPEAKS
{
	const auto text = help::script_spirits::TextToAskSpeakerOf(Pop().uintVal, help::k_HelpTextCount);
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto narrator = Locator::helpTextSystem::value().GetNarrator(text);
	Pushb(help::script_spirits::SpiritWhoTalks(narrator) == spirit);
}

/// The help texts of a thing still in the world, as the help scripts ask for them: a reward chest's own, a miracle
/// dispenser's miracle's and the first of its kind's
help::script_help_text::Texts ObjectHelpTexts(entt::entity object)
{
	help::script_help_text::Texts texts;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	if (const auto* reward = registry.TryGet<const ecs::components::Reward>(object); reward != nullptr)
	{
		if (const auto row = static_cast<size_t>(reward->type); row < info.reward.size())
		{
			texts.reward = info.reward.at(row).helpTextEnum;
			texts.kind = info.reward.at(row).helpStartEnum;
		}
		return texts;
	}
	if (const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(object); dispenser != nullptr)
	{
		// The text of the effect its miracle casts
		if (static_cast<size_t>(dispenser->magicType) < magic::k_MagicTypeCount)
		{
			const auto effect = magic::GetMagicInfo(info, dispenser->magicType).magicType;
			texts.dispenser = magic::GetMagicEffectInfo(info, effect).helpStartEnum;
		}
		return texts;
	}
	if (const auto* kind = ecs::world_objects::HelpInfoOf(object); kind != nullptr)
	{
		texts.kind = kind->helpStartEnum;
	}
	return texts;
}

/// The thing a help native is asked about, if it is still in the world; none pushes 0
std::optional<entt::entity> PopHelpObject()
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return std::nullopt;
	}
	return object;
}

/// The first help text the help gives about an object: a spell dispenser's is its miracle's, any other object's its
/// kind's. Both GET_FIRST_HELP and GET_LAST_HELP give this one (the game reads the same text for both), so the scripts'
/// run of texts from the first to the last is this text alone.
void PushObjectHelpText()
{
	if (const auto object = PopHelpObject())
	{
		Pushf(static_cast<float>(help::script_help_text::FirstHelp(ObjectHelpTexts(*object))));
	}
}

void GetHelp() // 248 GET_HELP
{
	// What a reward chest says when it is clicked: its own text; anything else gives its first help text
	const auto object = PopHelpObject();
	if (!object)
	{
		return;
	}
	if (!Locator::entitiesRegistry::value().AllOf<ecs::components::Reward>(*object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not reward ");
	}
	Pushf(static_cast<float>(help::script_help_text::Help(ObjectHelpTexts(*object))));
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	Locator::chlapi::value().SetScriptHelpOn(Pop().intVal != 0);
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritEject(spirit, true);
	}
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritHome(spirit, true);
	}
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	PushObjectHelpText();
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	PushObjectHelpText();
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
	Locator::helpTextSystem::value().ClearAllText();
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
	Locator::helpTextSystem::value().CloseDialogue();
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		const auto& screen = advisors->GetScreen();
		const auto pixel = glm::ivec2(static_cast<int>(static_cast<float>(screen.width) * x),
		                              static_cast<int>(static_cast<float>(screen.height) * y));
		advisors->SpiritScreenPoint(spirit, pixel);
	}
}

/// Writes words across the screen for a cut scene, as GAME_DRAW_TEXT and GAME_DRAW_TEMP_TEXT do: the box's corner and
/// size and the lines' height as shares of the screen, and the time they fade in over
void DrawScreenText(std::u16string_view words, float across, float down, float width, float height, float size, float fade)
{
	Locator::cinematicDirectorSystem::value().WriteText(words, {across, down}, {width, height}, size, fade);
}

void GameDrawText() // 433 GAME_DRAW_TEXT
{
	const auto fade = Popf();
	const auto size = Popf();
	const auto height = Popf();
	const auto width = Popf();
	const auto down = Popf();
	const auto across = Popf();
	const auto textID = Pop().uintVal;
	// A help text by its number; 0 and numbers past the last give the first
	const auto words = Locator::helpTextSystem::value().GetWords(textID);
	DrawScreenText(words, across, down, width, height, size, fade);
}

void GameDrawTempText() // 434 GAME_DRAW_TEMP_TEXT
{
	const auto fade = Popf();
	const auto size = Popf();
	const auto height = Popf();
	const auto width = Popf();
	const auto down = Popf();
	const auto across = Popf();
	// The script's own words, as written, each byte a character
	const auto string = PopString();
	std::u16string words;
	words.reserve(string.size());
	std::ranges::transform(string, std::back_inserter(words),
	                       [](char c) { return static_cast<char16_t>(static_cast<unsigned char>(c)); });
	DrawScreenText(words, across, down, width, height, size, fade);
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	const auto time = Popf();
	Locator::cinematicDirectorSystem::value().FadeAllTexts(time);
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	Locator::cinematicDirectorSystem::value().SetTextColour(gui::script_text::ColourFromScript(red, green, blue));
}

} // namespace openblack::chlapi
