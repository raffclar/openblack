/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "Creature/FightTranscript.h"

namespace openblack::ecs::systems
{

/// The transcript of every creature fight, for the debug inspector and the tools that check the fight rules. It is
/// there only in builds with the inspector, after the inspector starts. Without it, the fights record nothing and pay
/// nothing for it.
class FightTranscriptSystemInterface
{
public:
	virtual ~FightTranscriptSystemInterface() = default;

	/// Once a frame: the number of the frame, and the game time of the frame in milliseconds
	virtual void Frame(uint64_t frame, float gameMs) = 0;
	/// A new fight between two creatures, with its "fight_start" record. The number of the fight.
	virtual uint32_t Begin(entt::entity first, entt::entity second, fight_transcript::Fields fields) = 0;
	/// A record in the last fight of a creature. Its index, or none when the creature has no fight.
	virtual std::optional<uint64_t> Note(entt::entity creature, std::string_view kind, fight_transcript::Fields fields,
	                                     std::optional<uint64_t> cause) = 0;
	/// The index of the last record of a kind for a creature, in its last fight
	[[nodiscard]] virtual std::optional<uint64_t> LastOf(entt::entity creature, std::string_view kind) const = 0;
	/// The last fight of the creature is over
	virtual void End(entt::entity creature) = 0;
	/// A script read the fight state of a creature and got an answer. A read is kept when its answer differs from the last
	/// read of the same native for the same creature, with the number of identical reads before it.
	virtual void ScriptRead(entt::entity creature, std::string_view native, double answer) = 0;

	[[nodiscard]] virtual const fight_transcript::Recorder& GetRecorder() const = 0;
	virtual void Clear() = 0;
	/// A folder to write each fight to, as one file of JSON lines; none to write no files
	virtual void SetOutputFolder(std::optional<std::filesystem::path> folder) = 0;
	[[nodiscard]] virtual std::optional<std::filesystem::path> GetOutputFolder() const = 0;
};

} // namespace openblack::ecs::systems
