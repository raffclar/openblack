/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpTextSystem.h"

#include <utility>

#include "3D/TempleInteriorInterface.h"
#include "Common/GameLanguage.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AdvisorVoiceSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Gui/TextDatabase.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
constexpr int k_GoodAdvisor = AdvisorVoiceSystemInterface::k_GoodAdvisor;
constexpr int k_EvilAdvisor = AdvisorVoiceSystemInterface::k_EvilAdvisor;

/// The advisors' voices that this computer plays. A computer that shows nothing has none.
AdvisorVoiceSystemInterface* Voices()
{
	return Locator::advisorVoiceSystem::has_value() ? &Locator::advisorVoiceSystem::value() : nullptr;
}
} // namespace

help::DialogueText::Queries HelpTextSystem::DialogueQueries()
{
	help::DialogueText::Queries queries;
	queries.text = [this](uint32_t number) {
		const auto& text = _texts->GetHelpText(number);
		return help::DialogueText::Text {.narrator = text.narrator, .important = text.important, .text = text.text};
	};
	queries.voice = [](uint32_t number) -> std::optional<help::DialogueVoice> {
		const auto sample = Locator::helpSpeechSystem::value().GetTable().Find(number);
		const bool known = sample && Locator::resources::value().GetSounds().Contains(sample->sound);
		const bool deviceWorks = Locator::sounds::has_value() && Locator::sounds::value().IsInstalled();
		return help::VoiceFor(known, known && sample->bank == audio::SpeechBank::HelpSprites, deviceWorks);
	};
	queries.turn = []() { return Locator::time::value().GetTurn(); };
	// The texts are timed in milliseconds by the scripts' clock, which only the turns move on, in the world and in the
	// temple, and which runs on through every change of land
	queries.nowMs = []() { return static_cast<int32_t>(Locator::helpSpeechSystem::value().GetClockMs()); };
	// The turn reads whether the player is in the temple when it needs it. A computer with no temple has none.
	queries.inTemple = []() { return Locator::temple::has_value() && Locator::temple::value().Active(); };
	// The scripts find the advisors talking by the scripts' clock, not by the sound that the advisors play
	queries.advisorsTalking = [this]() {
		return _scriptAdvisorLine.has_value() &&
		       audio::TalkingForScripts(*_scriptAdvisorLine, Locator::helpSpeechSystem::value().GetClockMs(),
		                                help::AdvisorVoices::k_JustStoppedMs);
	};
	queries.narrationSounding = [](uint32_t number) {
		return Locator::helpSpeechSystem::value().IsSaying(number, audio::SpeechVoice::First);
	};
	queries.scriptWideScreen = []() {
		const auto& director = Locator::cinematicDirectorSystem::value();
		return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	};
	return queries;
}

help::DialogueText::Hooks HelpTextSystem::DialogueHooks()
{
	help::DialogueText::Hooks hooks;
	hooks.advisorSays = [this](int32_t narrator, uint32_t text) { AdvisorSays(narrator, text); };
	// The narration is the first of the scripts' two voices, heard everywhere
	hooks.narrate = [](uint32_t text) {
		Locator::helpSpeechSystem::value().Say(text, audio::SpeechVoice::First, std::nullopt);
	};
	hooks.interruptAdvisors = [this]() { InterruptAdvisors(); };
	hooks.stopNarration = []() {
		Locator::helpSpeechSystem::value().Stop(audio::SpeechVoice::First, audio::SpeechBank::Villagers);
	};
	return hooks;
}

void HelpTextSystem::Start(const gui::TextDatabase& texts, int screenHeight)
{
	_texts = &texts;
	// The reading time of a text comes from the info table: so many game turns a word, and so many more
	help::DialogueText::Settings settings {};
	if (Locator::infoConstants::has_value())
	{
		const auto& help = Locator::infoConstants::value().helpSystem;
		settings.turnsPerWord = help.readDefaultWordGTTime;
		settings.extraTurns = help.readDefaultAdjustGTTime;
	}
	// Some languages use bigger lines of text
	const bool biggerText = Locator::config::has_value() && game_language::NeedsBiggerText(Locator::config::value().language);
	_dialogue = std::make_unique<help::DialogueText>(screenHeight, biggerText, settings, DialogueQueries(), DialogueHooks());
	SetTextChoices(_textChoices);
}

void HelpTextSystem::SetTextChoices(const TextChoices& choices)
{
	_textChoices = choices;
	if (_dialogue)
	{
		_dialogue->SetPlayerChoices(choices.textDraw, choices.topToBottom, choices.readSpeed);
	}
}

namespace
{
/// The screen height of a box that only takes the bytes of a saved dialogue
constexpr int k_DroppedScreenHeight = 480;
} // namespace

void HelpTextSystem::SaveState(reflection::BinaryWriter& out) const
{
	// The voices that speak the texts are sounds, which a save does not keep. The scripts' clock goes first.
	const auto now =
	    Locator::helpSpeechSystem::has_value() ? static_cast<int32_t>(Locator::helpSpeechSystem::value().GetClockMs()) : 0;
	reflection::Save(out, now);
	reflection::Save(out, _dialogue != nullptr);
	if (_dialogue != nullptr)
	{
		// The times of the text are on the scripts' clock, which the save keeps before them. Thus the load puts them back
		// on the same clock. A load empties the box and shows the kept texts again, and the newest text reads from the load.
		// The save keeps the text as the load makes it, so that a save at once after a load has the same bytes.
		auto saved = *_dialogue;
		saved.EmptyBox();
		saved.ShowAgain();
		reflection::Save(out, saved);
	}
	reflection::Save(out, _storyHistory);
}

bool HelpTextSystem::LoadState(reflection::BinaryReader& in)
{
	int32_t savedNow = 0;
	bool hasDialogue = false;
	reflection::Load(in, savedNow);
	reflection::Load(in, hasDialogue);
	// The scripts' clock continues from where the save left it
	if (Locator::helpSpeechSystem::has_value())
	{
		Locator::helpSpeechSystem::value().SetClockMs(static_cast<uint32_t>(savedNow));
	}
	if (hasDialogue)
	{
		// A dialogue that has not started yet reads into a box of its own, which is then dropped
		help::DialogueText dropped(k_DroppedScreenHeight, false, {}, {}, {});
		auto& dialogue = _dialogue != nullptr ? *_dialogue : dropped;
		reflection::Load(in, dialogue);
		// The box does not come back as it was: it is made again from the kept texts once every system has loaded
		dialogue.EmptyBox();
	}
	reflection::Load(in, _storyHistory);
	return !in.Failed();
}

void HelpTextSystem::Reset()
{
	// The story history starts again when the scripts start again
	_storyHistory.Clear();
	if (_dialogue)
	{
		_dialogue->ClearAllText();
	}
	StopVoices();
}

void HelpTextSystem::StopVoices()
{
	if (auto* voices = Voices(); voices != nullptr)
	{
		voices->Stop(k_GoodAdvisor);
		voices->Stop(k_EvilAdvisor);
	}
	_scriptAdvisorLine.reset();
}

void HelpTextSystem::AdvisorSays(int32_t narrator, uint32_t text)
{
	const auto sample = Locator::helpSpeechSystem::value().GetTable().Find(text);
	if (!sample)
	{
		return;
	}
	// The evil advisor stops first, then the good one
	auto* voices = Voices();
	if (voices != nullptr)
	{
		voices->Stop(k_EvilAdvisor);
		voices->Stop(k_GoodAdvisor);
	}
	const int advisor = narrator == help::k_NarratorGoodAdvisor ? k_GoodAdvisor : k_EvilAdvisor;
	float hoverX = 0.0f;
	if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
	{
		hoverX = Locator::advisorSystem::value().GetController().Dude(advisor).HoverX().GetValue();
	}
	if (voices != nullptr)
	{
		voices->Say(advisor, sample->sample, hoverX);
	}
	// The scripts time the line on their clock: after the same delay, for the length of its sample. A line with no sound
	// is not said.
	_scriptAdvisorLine.reset();
	const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::HelpSprites, sample->sample);
	const auto& sounds = Locator::resources::value().GetSounds();
	if (sound && sounds.Contains(*sound))
	{
		_scriptAdvisorLine = audio::SilentPlayback {
		    .startMs = Locator::helpSpeechSystem::value().GetClockMs() + help::AdvisorVoices::SayDelayMs(hoverX),
		    .lengthMs = sounds.Handle(*sound)->duration * 1000.0f,
		};
	}
}

void HelpTextSystem::InterruptAdvisors()
{
	if (auto* voices = Voices(); voices != nullptr)
	{
		voices->Interrupt(k_GoodAdvisor, help::AdvisorVoices::InterruptedBy::Text);
		voices->Interrupt(k_EvilAdvisor, help::AdvisorVoices::InterruptedBy::Text);
	}
	_scriptAdvisorLine.reset();
}

void HelpTextSystem::InterruptAdvisorsByEscape()
{
	if (auto* voices = Voices(); voices != nullptr)
	{
		voices->Interrupt(k_GoodAdvisor, help::AdvisorVoices::InterruptedBy::Escape);
		voices->Interrupt(k_EvilAdvisor, help::AdvisorVoices::InterruptedBy::Escape);
	}
	_scriptAdvisorLine.reset();
}

int32_t HelpTextSystem::GetNarrator(uint32_t text) const
{
	return _texts != nullptr ? _texts->GetHelpText(text).narrator : 0;
}

std::u16string HelpTextSystem::GetWords(uint32_t text) const
{
	return _texts != nullptr ? _texts->GetHelpText(text).text : std::u16string {};
}

void HelpTextSystem::Update(const Frame& frame)
{
	if (!_dialogue)
	{
		return;
	}
	_dialogue->Update(static_cast<float>(frame.inTemple ? frame.realMs : frame.gameMs));
	_dialogue->AdvanceClickCue(static_cast<float>(frame.realMs) * 0.001f);
	_dialogue->AdvanceControlIcons(static_cast<float>(frame.realMs) * 0.001f);
}

void HelpTextSystem::TakeClick(bool click, bool skipKey)
{
	if (!_dialogue)
	{
		return;
	}
	// The next turn of the dialogue takes the click and the key, so that the scripts see them at the same turn at any
	// frame rate
	_pendingClick = _pendingClick || click;
	_pendingSkipKey = _pendingSkipKey || skipKey;
}

void HelpTextSystem::ProcessTurn(uint32_t milliseconds)
{
	Locator::helpSpeechSystem::value().AdvanceClock(milliseconds);
	const bool click = std::exchange(_pendingClick, false);
	const bool skipKey = std::exchange(_pendingSkipKey, false);
	if (_dialogue)
	{
		_dialogue->ProcessClick(click, skipKey);
	}
}

void HelpTextSystem::RecordStoryLine(uint32_t text, int32_t withInteraction, float number)
{
	const auto valid = text < help::k_HelpTextCount ? text : 0u;
	_storyHistory.Add({.text = valid, .interaction = withInteraction, .number = number, .narrator = GetNarrator(valid)});
}

bool HelpTextSystem::RunText(bool singleLine, uint32_t text, int32_t withInteraction)
{
	// The story history keeps every text that a script shows, also before the dialogue starts
	RecordStoryLine(text, withInteraction, 0.0f);
	return _dialogue ? _dialogue->RunText(singleLine, text, withInteraction) : false;
}

bool HelpTextSystem::RunTextWithNumber(bool singleLine, uint32_t text, float number, int32_t withInteraction)
{
	RecordStoryLine(text, withInteraction, number);
	return _dialogue ? _dialogue->RunText(singleLine, text, withInteraction, number) : false;
}

void HelpTextSystem::TempText(bool singleLine, std::string_view words, int32_t withInteraction)
{
	if (_dialogue)
	{
		_dialogue->TempText(singleLine, words, withInteraction);
	}
}

bool HelpTextSystem::IsTextRead() const
{
	// Without the help texts there is nothing to wait for
	return _dialogue ? _dialogue->IsTextRead() : true;
}

void HelpTextSystem::ClearAllText()
{
	if (_dialogue)
	{
		_dialogue->ClearAllText();
	}
}

void HelpTextSystem::CloseDialogue()
{
	if (_dialogue)
	{
		_dialogue->CloseDialogue();
	}
}

help::DialogueText::Held HelpTextSystem::HoldText() const
{
	return _dialogue ? _dialogue->Hold() : help::DialogueText::Held {};
}

void HelpTextSystem::CloseTextBox()
{
	if (_dialogue)
	{
		_dialogue->CloseBox();
	}
}

void HelpTextSystem::RestoreText(const help::DialogueText::Held& held)
{
	if (_dialogue)
	{
		_dialogue->Restore(held);
	}
}

void HelpTextSystem::ShowTextAgain()
{
	if (_dialogue)
	{
		_dialogue->ShowAgain();
	}
}

const help::TextFrame* HelpTextSystem::Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const
{
	return _dialogue ? &_dialogue->Layout(screen.x, screen.y, barPixels, widthFn) : nullptr;
}
