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

#include <optional>
#include <string_view>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "Locator.h"

/// Helpers that write to the fight transcript only when it is there. They make the fields of a record only then, so a
/// game with no transcript pays one test for each record.
namespace openblack::fight_transcript
{

/// Whether the fights keep a transcript now
[[nodiscard]] inline bool Recording()
{
	return Locator::fightTranscript::has_value();
}

/// A record for a creature in its last fight. The function makes the fields. The index of the record, if there is one.
template <typename MakeFields>
std::optional<uint64_t> Note(entt::entity creature, std::string_view kind, MakeFields&& make,
                             std::optional<uint64_t> cause = std::nullopt)
{
	if (!Recording())
	{
		return std::nullopt;
	}
	return Locator::fightTranscript::value().Note(creature, kind, make(), cause);
}

/// The index of the last record of a kind for a creature, in its last fight
[[nodiscard]] inline std::optional<uint64_t> LastOf(entt::entity creature, std::string_view kind)
{
	return Recording() ? Locator::fightTranscript::value().LastOf(creature, kind) : std::nullopt;
}

/// An entity as the number that the records keep
[[nodiscard]] inline int64_t Id(entt::entity entity)
{
	return entity == entt::null ? -1 : static_cast<int64_t>(entt::to_integral(entity));
}

} // namespace openblack::fight_transcript
