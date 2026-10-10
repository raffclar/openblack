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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::audio
{

/// The sound banks the game's spoken lines are in
enum class SpeechBank : uint8_t
{
	/// The people of the story: the family of the opening, the trainer, Khazar, Lethys...
	Villagers,
	/// The advisors
	HelpSprites,
	/// The villagers' voices of guidance, awe and the like
	Guidance,
};

/// The bank files, by SpeechBank, whatever the case
constexpr std::array<std::string_view, 3> k_SpeechBankFiles {"villagers.sad", "HelpSprites.sad", "Guidance.sad"};

/// A spoken line: a sample of one of the speech banks
struct SpeechSample
{
	SpeechBank bank;
	/// The sample's number in its bank, as the bank's header numbers it
	uint32_t sample;
	/// The sound it is loaded as
	entt::id_type sound;

	bool operator==(const SpeechSample&) const = default;
};

/// A sample of a bank, as its header has it: its number and the file it was made from
struct SpeechBankSample
{
	uint32_t sample;
	std::string_view file;
	/// The sound it is loaded as
	entt::id_type sound;
};

/// Which sample says each of the game's help texts. The texts are numbered in the order of the game's help text script
/// and each is spoken by the sample named after it (the text "HELP_TEXT_X_01" by the sample made from
/// "...\HELP_TEXT_X_01.wav", whatever the case), if any of the speech banks has one. The villagers' bank is looked in
/// first, then the advisors' and the guidance's: one text has a sample in both of the first two, and the game speaks it
/// with the villagers' one.
class HelpSpeechTable
{
public:
	HelpSpeechTable() = default;
	/// `textNames` in the texts' order, `banks` by SpeechBank
	HelpSpeechTable(std::span<const std::string> textNames, std::span<const std::vector<SpeechBankSample>, 3> banks);

	/// The sample that says a text. A number beyond the texts is taken as the first text, which says nothing.
	[[nodiscard]] std::optional<SpeechSample> Find(uint32_t text) const;
	[[nodiscard]] size_t GetCount() const noexcept { return _samples.size(); }
	/// How many of the texts have a sample saying them
	[[nodiscard]] size_t GetSpokenCount() const noexcept;
	/// How many samples a bank has
	[[nodiscard]] size_t GetBankCount(SpeechBank bank) const { return _bankSounds.at(static_cast<size_t>(bank)).size(); }
	/// The sound of a bank's sample by its number, nothing when the bank has no such sample
	[[nodiscard]] std::optional<entt::id_type> FindSound(SpeechBank bank, uint32_t sample) const;

	/// The name of the sample made from a file: its file name without folders or extension
	[[nodiscard]] static std::string_view SampleName(std::string_view file);

private:
	std::vector<std::optional<SpeechSample>> _samples;
	/// Each bank's sounds by sample number
	std::array<std::unordered_map<uint32_t, entt::id_type>, 3> _bankSounds;
};

/// Who a script's spoken line is said by. The game keeps two, so that two people can speak at once and a script can
/// wait for either to finish.
enum class SpeechVoice : uint8_t
{
	First,
	Second,
};

/// The lines the scripts have had said and the sounds saying them, to tell when a voice has finished a line
class SpeechVoices
{
public:
	/// A line started on a voice, playing as `emitter`
	void Add(SpeechVoice voice, SpeechSample sample, entt::entity emitter);
	/// Whether the voice is still saying the sample, by `isPlaying` of its sounds. Sounds that have finished are
	/// forgotten.
	[[nodiscard]] bool IsSaying(SpeechVoice voice, SpeechSample sample, const std::function<bool(entt::entity)>& isPlaying);
	/// Stops what the voice is saying from a bank, by `stop` of each of its sounds
	void Stop(SpeechVoice voice, SpeechBank bank, const std::function<void(entt::entity)>& stop);
	void Clear() { _lines.clear(); }
	[[nodiscard]] size_t GetCount() const noexcept { return _lines.size(); }

private:
	struct Line
	{
		SpeechVoice voice;
		SpeechSample sample;
		entt::entity emitter;
	};
	std::vector<Line> _lines;
};

} // namespace openblack::audio
