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

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Help/HelpMessages.h"

namespace openblack::ecs::systems
{

/// The help the game starts by itself (see help_messages): the answers to the Help key and the advisors' banter, said
/// through the help scripts. A new message stops a help script already talking; a story script holding the dialogue
/// keeps it.
class HelpMessageSystemInterface: public SavedStateInterface
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

		/// Gives the values that a saved game keeps to a visitor
		template <typename Self, typename Visitor>
		static void VisitSaved(Self& self, Visitor&& visit)
		{
			visit(self.script, self.first, self.last, self.statText, self.need, self.turn);
		}
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
	/// The Help key: the computer of the player finds what is in the hand, else what the hand is over, else the ground.
	/// It sends that to the turn (player_commands::HelpQuery).
	virtual void HelpQuery() = 0;
	/// At the turn: the Help key of this computer's player. The advisors tell the player about the thing, else about
	/// the ground. Nothing occurs while a script's cut scene has the controls of the player.
	virtual void DoHelpQuery(const player_commands::HelpQuery& query) = 0;
	/// Once a game turn: the banter, while the help system is on
	virtual void ProcessTurn() = 0;

	/// The random numbers of the local help: the banter, the texts of a set, the guidance and the spirits of the scripts.
	/// The help draws them only in the turns, and no frame draws them, thus the help says the same things at the same
	/// turns at all frame rates. The draw is as LocalRand: 0 for 0 without a draw, else LHRand(n) on the seed of the help.
	virtual uint32_t HelpRand(int32_t n) = 0;
	/// FloatRand(x) on the seed of the help
	virtual float HelpFloatRand(float x) = 0;

	[[nodiscard]] virtual const help_messages::SetCounts& GetSentCounts() const = 0;
	[[nodiscard]] virtual const std::vector<Started>& GetStarted() const = 0;
};

} // namespace openblack::ecs::systems
