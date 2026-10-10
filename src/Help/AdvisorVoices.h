/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>

namespace openblack::help
{

/// The voices of the two advisors, 0 the good one and 1 the evil one: which of them is saying a line, when it started
/// and when it last talked.
///
/// Only one advisor speaks at a time. A line starts after a short delay when the advisor is near the edge of the
/// screen, as if it had to come in to say it. An advisor counts as talking while its line plays, and for 200 ms after,
/// so that one advisor's line runs into the other's.
class AdvisorVoices
{
public:
	static constexpr int k_Advisors = 2;
	/// How long after its line an advisor still counts as talking
	static constexpr uint32_t k_JustStoppedMs = 200;

	/// What the voices need from the rest of the game; unset reads as the value next to each
	struct Audio
	{
		/// The advisors' bank's number of lines. Unset: 0, so nothing is said
		std::function<uint32_t()> lineCount;
		/// Starts an advisor's line (1 to the line count), false when it can't be played. Unset: false
		std::function<bool(uint32_t line)> start;
		/// Whether the line is still playing. Unset: false
		std::function<bool(uint32_t line)> isPlaying;
		/// How far through its line it is, 0 to 1. Unset: 1
		std::function<float(uint32_t line)> percentageDone;
		/// Stops the line. Unset: nothing
		std::function<void(uint32_t line)> stop;
		/// The computer's clock in milliseconds. Unset: 0
		std::function<uint32_t()> tickMs;
		/// A number below n from the game's local random numbers. Unset: 0
		std::function<uint32_t(int32_t n)> localRand;
	};

	explicit AdvisorVoices(Audio audio);

	/// The delay before an advisor's line starts, from how far across the screen it hovers (-1 the left edge, 1 the
	/// right): none inside 0.95 of the way to an edge, then a quarter of a second rising to at most half a second
	[[nodiscard]] static uint32_t SayDelayMs(float hoverX);

	/// The advisor is given a line to say after the delay for where it hovers
	void Say(int advisor, uint32_t line, bool onlyIfSilent, float hoverX);
	/// The advisor becomes the speaker and its line starts after the delay. While another line plays, nothing happens
	/// if `onlyIfSilent` and the advisor is talking, else that line is stopped. A line the bank does not have is not said.
	void SaySentence(int advisor, uint32_t line, bool onlyIfSilent, uint32_t delayMs);
	/// Once a frame, the good advisor first: a line whose delay is over starts, and whether each talks is looked at
	void Update();

	/// The speaker is waiting to start or still saying its line. A line that has finished is stopped.
	[[nodiscard]] bool IsTalking(int advisor);
	/// Talking, or stopped less than 200 ms ago
	[[nodiscard]] bool TalkingOrJustStopped(int advisor);
	/// How far through its line the speaker is: 0 while waiting to start, 1 for an advisor that is not speaking
	[[nodiscard]] float PercentageDone(int advisor);
	/// The advisor's line is stopped, or its start called off
	void StopSentence(int advisor);
	/// The line stopped and the advisor no longer counts as given one
	void Stop(int advisor);
	/// The advisor was given a line, until stopped
	[[nodiscard]] bool IsActive(int advisor) const { return _advisors.at(static_cast<size_t>(advisor)).active; }
	/// Either advisor was given a line and is talking or just stopped
	[[nodiscard]] bool AnyTalking();
	/// The advisor is cut short, as a click cutting a text does: one that is saying a line picks one of five
	/// interruption lines, but stopping its line leaves it no longer the speaker, so the interruption is never heard
	void Interrupt(int advisor);

	/// The advisor speaking, -1 for none
	[[nodiscard]] int GetSpeaker() const { return _speaker; }
	/// The line being said, 0 for none
	[[nodiscard]] uint32_t GetSentence() const { return _sentence; }

private:
	struct Advisor
	{
		uint32_t line {0};
		/// When the line is to start, 0 when not waiting
		uint32_t startTick {0};
		uint32_t lastTalkTick {0};
		bool active {false};
	};

	void UpdateSaySentence(Advisor& advisor);
	[[nodiscard]] uint32_t Now() const { return _audio.tickMs ? _audio.tickMs() : 0; }

	Audio _audio;
	std::array<Advisor, k_Advisors> _advisors {};
	int _speaker {-1};
	uint32_t _sentence {0};
};

} // namespace openblack::help
