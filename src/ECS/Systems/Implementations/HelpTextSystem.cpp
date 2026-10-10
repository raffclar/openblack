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

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The good advisor and the evil one, as the voices number them
constexpr int k_GoodAdvisor = 0;
constexpr int k_EvilAdvisor = 1;

/// The playing sound of an emitter, which has not finished
bool Sounding(entt::entity emitter)
{
	if (emitter == entt::null)
	{
		return false;
	}
	auto& audio = Locator::audio::value();
	return audio.EmitterExists(emitter) && audio.GetStatus(emitter) != audio::AudioStatus::Stopped;
}
} // namespace

HelpTextSystem::HelpTextSystem()
    : _voices(VoiceAudio())
{
}

help::AdvisorVoices::Audio HelpTextSystem::VoiceAudio()
{
	help::AdvisorVoices::Audio audio;
	audio.lineCount = []() {
		return static_cast<uint32_t>(
		    Locator::helpSpeechSystem::value().GetTable().GetBankCount(audio::SpeechBank::HelpSprites));
	};
	audio.start = [this](uint32_t line) {
		const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::HelpSprites, line);
		if (!sound || !Locator::resources::value().GetSounds().Contains(*sound))
		{
			return false;
		}
		// Heard everywhere, wherever the advisor is
		_advisorLine = Locator::audio::value().StartSoundEffect(*sound, {});
		return _advisorLine != entt::null;
	};
	audio.isPlaying = [this](uint32_t) { return Sounding(_advisorLine); };
	audio.percentageDone = [this](uint32_t) {
		return Sounding(_advisorLine) ? Locator::audio::value().GetProgress(_advisorLine) : 1.0f;
	};
	audio.stop = [this](uint32_t) {
		if (_advisorLine != entt::null && Locator::audio::value().EmitterExists(_advisorLine))
		{
			Locator::audio::value().StopEmitter(_advisorLine);
		}
		_advisorLine = entt::null;
	};
	audio.tickMs = []() { return static_cast<uint32_t>(Locator::time::value().GetElapsedTime().count()); };
	audio.localRand = [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); };
	return audio;
}

help::DialogueText::Queries HelpTextSystem::DialogueQueries()
{
	help::DialogueText::Queries queries;
	queries.text = [this](uint32_t number) {
		const auto& text = _texts->GetHelpText(number);
		return help::DialogueText::Text {.narrator = text.narrator, .important = text.important, .text = text.text};
	};
	queries.voice = [](uint32_t number) -> std::optional<help::DialogueVoice> {
		const auto sample = Locator::helpSpeechSystem::value().GetTable().Find(number);
		if (!sample || !Locator::resources::value().GetSounds().Contains(sample->sound))
		{
			return std::nullopt;
		}
		return help::DialogueVoice {.advisorsBank = sample->bank == audio::SpeechBank::HelpSprites};
	};
	queries.turn = []() { return Locator::time::value().GetTurn(); };
	queries.nowMs = [this]() { return _clockMs; };
	queries.inTemple = [this]() { return _inTemple; };
	queries.advisorsTalking = [this]() { return _voices.AnyTalking(); };
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
	// Some languages' texts are drawn bigger; the game's English ones are not
	_dialogue = std::make_unique<help::DialogueText>(screenHeight, false, help::DialogueText::Settings {}, DialogueQueries(),
	                                                 DialogueHooks());
}

void HelpTextSystem::Reset()
{
	if (_dialogue)
	{
		_dialogue->ClearAllText();
	}
	_voices.Stop(k_GoodAdvisor);
	_voices.Stop(k_EvilAdvisor);
}

void HelpTextSystem::AdvisorSays(int32_t narrator, uint32_t text)
{
	const auto sample = Locator::helpSpeechSystem::value().GetTable().Find(text);
	if (!sample)
	{
		return;
	}
	// The evil advisor stops first, then the good one
	_voices.Stop(k_EvilAdvisor);
	_voices.Stop(k_GoodAdvisor);
	const int advisor = narrator == help::k_NarratorGoodAdvisor ? k_GoodAdvisor : k_EvilAdvisor;
	float hoverX = 0.0f;
	if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
	{
		hoverX = Locator::advisorSystem::value().GetController().Dude(advisor).HoverX().GetValue();
	}
	_voices.Say(advisor, sample->sample, false, hoverX);
}

void HelpTextSystem::InterruptAdvisors()
{
	_voices.Interrupt(k_GoodAdvisor);
	_voices.Interrupt(k_EvilAdvisor);
}

int32_t HelpTextSystem::GetNarrator(uint32_t text) const
{
	return _texts != nullptr ? _texts->GetHelpText(text).narrator : 0;
}

void HelpTextSystem::Update(const Frame& frame)
{
	_clockMs += static_cast<int32_t>(frame.gameMs);
	_inTemple = frame.inTemple;
	_voices.Update();
	if (!_dialogue)
	{
		return;
	}
	_dialogue->ProcessClick(frame.click, frame.skipKey);
	_dialogue->Update(static_cast<float>(frame.inTemple ? frame.realMs : frame.gameMs));
}

bool HelpTextSystem::RunText(bool singleLine, uint32_t text, int32_t withInteraction)
{
	return _dialogue ? _dialogue->RunText(singleLine, text, withInteraction) : false;
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

const help::TextFrame* HelpTextSystem::Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const
{
	return _dialogue ? &_dialogue->Layout(screen.x, screen.y, barPixels, widthFn) : nullptr;
}
