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
#include <string>
#include <vector>

#include "Help/HelpMessages.h"

namespace openblack::ecs::systems
{

/// The help the game starts by itself (see help_messages): the answers to the Help key and the advisors' banter, said
/// through the help scripts. A new message stops a help script already talking; a story script holding the dialogue
/// keeps it.
class HelpMessageSystemInterface
{
public:
	/// A help script started, for the inspector
	struct Started
	{
		std::string script;
		uint32_t first;
		uint32_t last;
		/// A town's need: its text and how much the town wants it, in percent
		std::optional<uint32_t> statText;
		std::optional<float> need;
		uint32_t turn;
	};

	virtual ~HelpMessageSystemInterface() = default;

	/// Starts the script with the texts from the first to the last, unless the first is past the last or a story script
	/// holds the dialogue. The town's need's text and value go before them for the town needs' script. True when it
	/// started.
	virtual bool RunMessage(uint32_t first, uint32_t last, help_messages::Script script,
	                        std::optional<std::pair<uint32_t, float>> townNeed = std::nullopt) = 0;
	/// Says a set: counts it said, then its texts (a thing's set, the thing's texts) through its script
	virtual bool RunMessageSet(size_t set, std::optional<help_messages::Texts> thing = std::nullopt) = 0;
	/// A kind of help counts as given now
	virtual void TriggerCategory(uint32_t category) = 0;
	/// The Help key: about what is in the hand, else what the hand is over, else the ground
	virtual void HelpQuery() = 0;
	/// Once a game turn: the banter, while the help system is on
	virtual void ProcessTurn() = 0;

	[[nodiscard]] virtual const help_messages::SetCounts& GetSentCounts() const = 0;
	[[nodiscard]] virtual const std::vector<Started>& GetStarted() const = 0;
};

} // namespace openblack::ecs::systems
