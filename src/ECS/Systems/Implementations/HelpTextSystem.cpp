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

#include <chrono>

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "Audio/SoundDecoder.h"
#include "Audio/WaveCueLabels.h"
#include "Common/GameRandom.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
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

/// The real time since the game started, in milliseconds
uint32_t NowMs()
{
	return static_cast<uint32_t>(Locator::time::value().GetElapsedTime().count());
}

/// Whether the length of its sample times the advisor's line, not the sound device
bool TimedByLength()
{
	return audio::TimedByLength(Locator::audio::value().IsDeviceLost(), Locator::time::value().GetFixedFrameTime().has_value());
}

/// A line's recording decoded once more, as the sound plays it, with the cue labels its gestures are read from
help::AdvisorVoices::Recording LineRecording(const audio::Sound& sound)
{
	help::AdvisorVoices::Recording recording;
	for (const auto& part : sound.buffer)
	{
		auto decoded = audio::DecodeSound(part, sound.sampleRate);
		if (!decoded.sound)
		{
			continue;
		}
		recording.channels = decoded.sound->channelLayout == audio::ChannelLayout::Stereo ? 2 : 1;
		recording.sampleRate = decoded.sound->sampleRate;
		recording.samples.insert(recording.samples.end(), decoded.sound->samples.begin(), decoded.sound->samples.end());
	}
	if (!sound.buffer.empty())
	{
		std::vector<help::spirits::TagLabel> labels;
		for (auto& label : audio::ReadWaveCueLabels(sound.buffer.front()))
		{
			labels.push_back({.text = std::move(label.text), .time = label.time});
		}
		recording.labels = std::move(labels);
	}
	return recording;
}
} // namespace

HelpTextSystem::HelpTextSystem()
    : _voices(VoiceAudio(), VoiceHooks())
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
		// Heard everywhere, wherever the advisor is. The voice rules know the advisors' lines by their own number.
		auto& sounds = Locator::audio::value();
		_advisorLine = sounds.StartSoundEffect(
		    *sound, {.voiceOwner = audio::VoiceOwner::Number, .ownerNumber = audio::k_AdvisorOwnerNumber});
		// Its length is known once it has been made ready to play
		_advisorPlayback = {
		    .startMs = NowMs(),
		    .lengthMs = Locator::resources::value().GetSounds().Handle(*sound)->duration * 1000.0f,
		};
		// With the sound device lost, or with a fixed frame time, the line starts and lasts the length of its sample
		return _advisorLine != entt::null || TimedByLength();
	};
	audio.isPlaying = [this](uint32_t) { return AdvisorLinePlays(); };
	audio.percentageDone = [this](uint32_t) {
		if (TimedByLength())
		{
			return _advisorPlayback.Progress(NowMs());
		}
		return Sounding(_advisorLine) ? Locator::audio::value().GetProgress(_advisorLine) : 1.0f;
	};
	audio.stop = [this](uint32_t) {
		if (_advisorLine != entt::null && Locator::audio::value().EmitterExists(_advisorLine))
		{
			Locator::audio::value().StopEmitter(_advisorLine);
		}
		_advisorLine = entt::null;
		_advisorPlayback = {};
	};
	audio.recording = [](uint32_t line) -> std::optional<help::AdvisorVoices::Recording> {
		const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::HelpSprites, line);
		if (!sound || !Locator::resources::value().GetSounds().Contains(*sound))
		{
			return std::nullopt;
		}
		const auto begin = std::chrono::steady_clock::now();
		auto recording = LineRecording(*Locator::resources::value().GetSounds().Handle(*sound));
		if (const auto logger = spdlog::get("audio"); logger != nullptr)
		{
			const std::chrono::duration<float, std::milli> took = std::chrono::steady_clock::now() - begin;
			SPDLOG_LOGGER_DEBUG(logger, "Advisor line {}: {} samples at {} Hz, {} cue labels, read in {:.2f} ms", line,
			                    recording.samples.size(), recording.sampleRate, recording.labels ? recording.labels->size() : 0,
			                    took.count());
		}
		return recording;
	};
	audio.playPositionMs = [this](uint32_t) -> int64_t {
		if (!AdvisorLinePlays())
		{
			return -1;
		}
		if (TimedByLength())
		{
			return static_cast<int64_t>(NowMs() - _advisorPlayback.startMs);
		}
		return static_cast<int64_t>(Locator::audio::value().GetProgress(_advisorLine) * _advisorPlayback.lengthMs);
	};
	audio.tickMs = []() { return NowMs(); };
	audio.localRand = [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); };
	return audio;
}

bool HelpTextSystem::AdvisorLinePlays() const
{
	return audio::PlaysOn(Sounding(_advisorLine), TimedByLength(), _advisorPlayback, NowMs());
}

help::AdvisorVoices::Hooks HelpTextSystem::VoiceHooks()
{
	help::AdvisorVoices::Hooks hooks;
	hooks.setTags = [](int advisor, std::vector<help::spirits::AudioTag> tags) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().SetSentenceTags(advisor, std::move(tags));
		}
	};
	hooks.stopTags = [](int advisor, float before) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().StopSentence(advisor, before);
		}
	};
	return hooks;
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
		const bool known = sample && Locator::resources::value().GetSounds().Contains(sample->sound);
		const bool deviceWorks = Locator::audio::has_value() && Locator::audio::value().IsInstalled();
		return help::VoiceFor(known, known && sample->bank == audio::SpeechBank::HelpSprites, deviceWorks);
	};
	queries.turn = []() { return Locator::time::value().GetTurn(); };
	// The texts are timed in milliseconds by the real time since the game started, which runs on whatever the game's
	// speed, while it is paused, in the temple and through every change of land
	queries.nowMs = []() { return static_cast<int32_t>(Locator::time::value().GetElapsedTime().count()); };
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
	// The reading time of a text comes from the info table: so many game turns a word, and so many more
	help::DialogueText::Settings settings {};
	if (Locator::infoConstants::has_value())
	{
		const auto& help = Locator::infoConstants::value().helpSystem;
		settings.turnsPerWord = help.readDefaultWordGTTime;
		settings.extraTurns = help.readDefaultAdjustGTTime;
	}
	// Some languages' texts are drawn bigger; the game's English ones are not
	_dialogue = std::make_unique<help::DialogueText>(screenHeight, false, settings, DialogueQueries(), DialogueHooks());
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
	// The voices that speak the texts are sounds, which a save does not keep
	// The real times of the text go into the save as times from the moment of the save, and the moment is zero. The real
	// clock of a run starts at a different time in each run, so a save of a loaded game then has the same bytes as the
	// save that the game loaded. A load of an older save, which keeps the clock of its run, gives the same times.
	reflection::Save(out, int32_t {0});
	reflection::Save(out, _dialogue != nullptr);
	if (_dialogue != nullptr)
	{
		const auto now = Locator::time::has_value() ? static_cast<int32_t>(Locator::time::value().GetElapsedTime().count()) : 0;
		// A load empties the box and shows the kept texts again, and the newest text reads from the load. The save keeps
		// the text as the load makes it, so that a save at once after a load has the same bytes.
		auto saved = *_dialogue;
		saved.EmptyBox();
		saved.ShowAgain();
		saved.ShiftRealTime(-now);
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
	if (hasDialogue)
	{
		// A dialogue that has not started yet reads into a box of its own, which is then dropped
		help::DialogueText dropped(k_DroppedScreenHeight, false, {}, {}, {});
		auto& dialogue = _dialogue != nullptr ? *_dialogue : dropped;
		reflection::Load(in, dialogue);
		const auto now = Locator::time::has_value() ? static_cast<int32_t>(Locator::time::value().GetElapsedTime().count()) : 0;
		dialogue.ShiftRealTime(now - savedNow);
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

void HelpTextSystem::InterruptAdvisorsByEscape()
{
	_voices.Interrupt(k_GoodAdvisor, help::AdvisorVoices::InterruptedBy::Escape);
	_voices.Interrupt(k_EvilAdvisor, help::AdvisorVoices::InterruptedBy::Escape);
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
	_inTemple = frame.inTemple;
	_voices.Update();
	if (!_dialogue)
	{
		return;
	}
	_dialogue->ProcessClick(frame.click, frame.skipKey);
	_dialogue->Update(static_cast<float>(frame.inTemple ? frame.realMs : frame.gameMs));
	_dialogue->AdvanceClickCue(static_cast<float>(frame.realMs) * 0.001f);
	_dialogue->AdvanceControlIcons(static_cast<float>(frame.realMs) * 0.001f);
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
