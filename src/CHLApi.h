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
	/// The scripts start again: their switches go back to how a new game has them, the game's sound effects on and every
	/// creature heard in its own voice
	void ResetSwitches();

	/// Whether the scripts show the challenge scrolls; the signs always show
	[[nodiscard]] bool IsHighlightDrawOn() const { return _highlightDrawOn; }
	void SetHighlightDrawOn(bool on) { _highlightDrawOn = on; }
	/// The help level the player chose, 0 for no help. openblack has no option for it yet, so it is the game's default.
	static constexpr uint32_t k_PlayerHelpLevel = 3;
	/// Whether the scripts let the help system run; they turn it off and on again, and it is on as they start again
	void SetScriptHelpOn(bool on) { _scriptHelpOn = on; }
	/// Whether the help system is on: the scripts let it run and the player wants help
	[[nodiscard]] bool IsHelpSystemOn() const { return _scriptHelpOn && k_PlayerHelpLevel != 0; }
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
};

} // namespace openblack::chlapi
