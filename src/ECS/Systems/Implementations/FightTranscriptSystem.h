/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <fstream>
#include <map>
#include <string>
#include <tuple>

#include "ECS/Systems/FightTranscriptSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FightTranscriptSystem final: public FightTranscriptSystemInterface
{
public:
	void Frame(uint64_t frame, float gameMs) override;
	uint32_t Begin(entt::entity first, entt::entity second, fight_transcript::Fields fields) override;
	std::optional<uint64_t> Note(entt::entity creature, std::string_view kind, fight_transcript::Fields fields,
	                             std::optional<uint64_t> cause) override;
	[[nodiscard]] std::optional<uint64_t> LastOf(entt::entity creature, std::string_view kind) const override;
	void End(entt::entity creature) override;
	void ScriptRead(entt::entity creature, std::string_view native, double answer) override;

	[[nodiscard]] const fight_transcript::Recorder& GetRecorder() const override { return _recorder; }
	void Clear() override;
	void SetOutputFolder(std::optional<std::filesystem::path> folder) override;
	[[nodiscard]] std::optional<std::filesystem::path> GetOutputFolder() const override { return _folder; }

private:
	[[nodiscard]] fight_transcript::Stamp Now() const;
	/// Writes the records of a fight that are not in its file yet, when there is a folder
	void Write(const fight_transcript::Fight& fight);
	/// Removes the oldest fights that ended, beyond the number that is kept, after it writes them to their files
	void Trim();

	fight_transcript::Recorder _recorder;
	uint64_t _frame {0};
	double _gameMs {0.0};
	std::optional<std::filesystem::path> _folder;
	/// The file of a fight, and the number of its records that are in the file
	struct FightFile
	{
		std::ofstream stream;
		size_t written {0};
	};
	std::map<uint32_t, FightFile> _files;
	/// The last answer of each native for each creature in each fight, and the number of identical reads since it was kept
	struct LastRead
	{
		double answer;
		uint32_t repeats;
	};
	std::map<std::tuple<uint32_t, uint32_t, std::string>, LastRead> _reads;
};

} // namespace openblack::ecs::systems
