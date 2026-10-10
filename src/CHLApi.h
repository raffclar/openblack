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
#include <vector>

#include <LHVMTypes.h>

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

	/// The scripts call a native: the one an unwritten native reports itself as
	void EnterNative(uint32_t native) { _currentNative = native; }
	/// The native being run isn't written (or this case of it isn't): logged the first time only, every call counted
	void NotImplemented(std::optional<int32_t> detail = std::nullopt);
	[[nodiscard]] const script::NativeStubCalls& GetStubCalls() const { return _stubCalls; }
	/// Logs how often each unwritten native was called, the most called first
	void LogStubCalls() const;

private:
	void InitFunctionsTable0();
	void InitFunctionsTable1();
	void InitFunctionsTable2();
	void InitFunctionsTable3();
	void InitFunctionsTable4();

	std::vector<lhvm::NativeFunction> _functionsTable;
	uint32_t _currentNative {0};
	script::NativeStubCalls _stubCalls;
};

} // namespace openblack::chlapi
