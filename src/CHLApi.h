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
#include <vector>

#include <LHVMTypes.h>
#include <glm/vec3.hpp>

#include "ScriptHeaders/NativeStubCalls.h"

namespace openblack::chlapi
{

using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;

class CHLApi
{
public:
	CHLApi();

	[[nodiscard]] const std::vector<lhvm::NativeFunction>& GetFunctionsTable();
	/// A script task has stopped: whatever it had control of goes back
	static void TaskStopped(uint32_t task);
	/// The help system starts a help script by name, unless a script holds the dialogue: a help script holding it is
	/// stopped first. True when it started.
	static bool StartHelpScript(std::string_view name);
	/// A challenge's scroll tapped again: its record's reminder script starts with its arguments, unless a script holds
	/// the dialogue (a help script holding it is stopped first). True when it started; false too before the challenge
	/// has a record.
	static bool StartChallengeReminder(uint32_t challenge);
	/// A help script holds the dialogue
	[[nodiscard]] static bool HelpScriptHoldsDialogue();
	/// As the player's Escape does outside a cut scene: the help scripts stop and both advisors are cut short
	static void StopHelpScriptsByEscape();

	/// The scripts call a native: the one an unwritten native reports itself as
	void EnterNative(uint32_t native) { _currentNative = native; }
	/// The native being run isn't written (or this case of it isn't): logged the first time only, every call counted
	void NotImplemented(std::optional<int32_t> detail = std::nullopt);
	[[nodiscard]] const script::NativeStubCalls& GetStubCalls() const { return _stubCalls; }
	/// Logs how often each unwritten native was called, the most called first
	void LogStubCalls() const;

	/// Whether the scripts let the game's sound effects play: off, only the advisors' and villagers' speech plays
	[[nodiscard]] bool IsGameSoundOn() const { return _gameSoundOn; }
	void SetGameSoundOn(bool on) { _gameSoundOn = on; }
	/// The scripts start again: their switches go back to how a new game has them, the game's sound effects on, every
	/// creature heard in its own voice and no lesson of growing up running for the local player's creature
	void ResetSwitches();

	/// Whether the scripts show the challenge scrolls; the signs always show
	[[nodiscard]] bool IsHighlightDrawOn() const { return _highlightDrawOn; }
	void SetHighlightDrawOn(bool on) { _highlightDrawOn = on; }
	/// The help level the player picked in the options, from no help (0) to all help (4)
	[[nodiscard]] uint32_t GetPlayerHelpLevel() const { return _playerHelpLevel; }
	void SetPlayerHelpLevel(uint32_t level) { _playerHelpLevel = level; }
	/// Whether the scripts let the help system run; they turn it off and on again, and it is on as they start again
	void SetScriptHelpOn(bool on) { _scriptHelpOn = on; }
	/// Whether the help system is on: the scripts let it run and the player wants help
	[[nodiscard]] bool IsHelpSystemOn() const { return _scriptHelpOn && _playerHelpLevel != 0; }
	/// A sound of one of the game's banks, by its number in the bank, through the game's common sound effect path: its
	/// gates may keep it quiet (see audio::SoundEffectHeard). Placed, it isn't started further from the camera than the
	/// sample's maximum distance.
	void PlayBankSoundEffect(int32_t bank, int32_t sample, std::optional<glm::vec3> position) const;

private:
	void InitFunctionsTable0();
	void InitFunctionsTable1();
	void InitFunctionsTable2();
	void InitFunctionsTable3();
	void InitFunctionsTable4();

	std::vector<lhvm::NativeFunction> _functionsTable;
	uint32_t _currentNative {0};
	script::NativeStubCalls _stubCalls;
	bool _gameSoundOn {true};
	bool _highlightDrawOn {true};
	bool _scriptHelpOn {true};
	/// The game's own help level until the player's profile gives one
	uint32_t _playerHelpLevel {3};
};

} // namespace openblack::chlapi
