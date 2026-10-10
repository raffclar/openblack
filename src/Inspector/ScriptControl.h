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
#include <string_view>
#include <variant>
#include <vector>

#include <InspectorProvider.h>

namespace openblack::inspector
{

/// A value on the scripts' stack or in a global, as the inspector reads and writes it
struct ScriptValue
{
	enum class Type : uint8_t
	{
		Int,
		Float,
		Vector,
		Object,
		Boolean,
	};
	Type type {Type::Float};
	/// A number, an object's id, a vector's component, or 0 or 1
	float number {0.0f};
	uint32_t object {0};
	int32_t integer {0};
	bool boolean {false};
};

/// A value as JSON: a number, true or false, or {"object": id}
[[nodiscard]] Json ToJson(const ScriptValue& value);
/// The values a native is given: a number is a float, true or false a boolean, [x, y, z] a vector (three values),
/// {"object": id} an object and {"int": n} a whole number. None, with why not, when one doesn't read.
[[nodiscard]] std::optional<std::vector<ScriptValue>> ArgumentsFromJson(const Json& args, std::string& error);
/// A value of a type from JSON, for a global
[[nodiscard]] std::optional<ScriptValue> ValueOfType(ScriptValue::Type type, const Json& json, std::string& error);

struct ScriptGlobal
{
	std::string name;
	ScriptValue value;
};

/// A script of the land's: a function, a challenge or a help script, by name, the file it is in and its parameters
struct ScriptInfo
{
	std::string name;
	std::string file;
	std::string type;
	uint32_t parameters {0};
};

struct ScriptNative
{
	uint32_t id {0};
	std::string name;
	int32_t in {0};
	uint32_t out {0};
	bool implemented {false};
};

/// What the inspector runs scripts, natives and globals through: the game's script machine
class ScriptTargetInterface
{
public:
	virtual ~ScriptTargetInterface() = default;
	/// Whether a land's scripts are loaded
	[[nodiscard]] virtual bool Loaded() const = 0;
	[[nodiscard]] virtual std::vector<ScriptInfo> Scripts() const = 0;
	/// Starts a script (a function or a challenge) by its name; the task's number, or why not
	virtual std::variant<uint32_t, std::string> Run(std::string_view name) = 0;
	virtual std::string StopTask(uint32_t task) = 0;
	[[nodiscard]] virtual std::vector<ScriptGlobal> Globals() const = 0;
	virtual std::string SetGlobal(std::string_view name, const ScriptValue& value) = 0;
	[[nodiscard]] virtual std::vector<ScriptNative> Natives() const = 0;
	/// Calls a native with its arguments as a script would; what it gave back, or why not
	virtual std::variant<std::vector<ScriptValue>, std::string> CallNative(uint32_t id,
	                                                                       const std::vector<ScriptValue>& args) = 0;
};

/// Adds the scripts' writes to the script provider:
///   script.scripts {name?}                the land's scripts by name
///   script.run    {name}                  starts a script or challenge by name; its task
///   script.stop   {task}                  stops a task
///   script.global {name}                  one global's value; script.globals {name?} lists them
///   script.set_global {name, value}       sets a global, keeping its type
///   script.call   {native, args?}         calls a native by name or number with arguments; what it gave back
void AddScriptControls(FunctionProvider& provider, ScriptTargetInterface& scripts);

} // namespace openblack::inspector
