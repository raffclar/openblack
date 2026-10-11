/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptControl.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <LHVMNatives.h>

using namespace openblack::inspector;

namespace
{

/// The most arguments a native call may be given
constexpr size_t k_MostArguments = 32;

std::string Upper(std::string_view text)
{
	std::string upper(text);
	std::ranges::transform(upper, upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return upper;
}

std::string_view TypeName(ScriptValue::Type type)
{
	switch (type)
	{
	case ScriptValue::Type::Int:
		return "int";
	case ScriptValue::Type::Float:
		return "float";
	case ScriptValue::Type::Vector:
		return "vector";
	case ScriptValue::Type::Object:
		return "object";
	case ScriptValue::Type::Boolean:
		return "boolean";
	case ScriptValue::Type::String:
		return "string";
	}
	return "unknown";
}

ParameterDescription Parameter(std::string name, std::string type, std::string description, bool required)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = required};
}

QueryDescription Description(std::string name, std::string description, std::vector<ParameterDescription> parameters,
                             bool writes, ResultKind kind = ResultKind::Object)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = false,
	        .writes = writes};
}

std::optional<ScriptNative> FindNative(const ScriptTargetInterface& scripts, const Json& params)
{
	const auto natives = scripts.Natives();
	const auto it = params.find("native");
	if (it == params.end())
	{
		return std::nullopt;
	}
	if (it->is_number_unsigned() || it->is_number_integer())
	{
		const auto id = it->get<int64_t>();
		const auto found =
		    std::ranges::find_if(natives, [id](const auto& native) { return static_cast<int64_t>(native.id) == id; });
		return found != natives.end() ? std::optional(*found) : std::nullopt;
	}
	if (it->is_string())
	{
		const auto name = Upper(it->get<std::string>());
		const auto found = std::ranges::find_if(natives, [&name](const auto& native) { return native.name == name; });
		return found != natives.end() ? std::optional(*found) : std::nullopt;
	}
	return std::nullopt;
}

/// A plain number, whose type the native's slot would have given it
bool IsUntypedNumber(const ScriptValue& value)
{
	return value.type == ScriptValue::Type::Float && !value.typeGiven;
}

/// Calls a native with values on the stack and answers what it gave back
QueryResult Called(ScriptTargetInterface& scripts, const ScriptNative& native, const std::vector<ScriptValue>& values)
{
	auto called = scripts.CallNative(native.id, values);
	if (const auto* why = std::get_if<std::string>(&called))
	{
		return QueryResult::Error(*why);
	}
	Json results = Json::array();
	for (const auto& value : std::get<std::vector<ScriptValue>>(called))
	{
		results.push_back({{"type", TypeName(value.type)}, {"value", ToJson(value)}});
	}
	return QueryResult::Value({{"native", native.name}, {"returned", std::move(results)}});
}

} // namespace

Json openblack::inspector::ToJson(const ScriptValue& value)
{
	switch (value.type)
	{
	case ScriptValue::Type::Int:
		return value.integer;
	case ScriptValue::Type::Float:
	case ScriptValue::Type::Vector:
		return value.number;
	case ScriptValue::Type::Object:
		return {{"object", value.object}};
	case ScriptValue::Type::Boolean:
		return value.boolean;
	case ScriptValue::Type::String:
		return value.text;
	}
	return nullptr;
}

std::optional<ScriptValue> openblack::inspector::ValueOfType(ScriptValue::Type type, const Json& json, std::string& error)
{
	// No global is text: a number or a truth written as text, as some tools send one, is read as JSON
	if (json.is_string())
	{
		if (const auto parsed = Parse(json.get<std::string>());
		    parsed.has_value() && (parsed->is_number() || parsed->is_boolean()))
		{
			return ValueOfType(type, *parsed, error);
		}
	}
	ScriptValue value {.type = type};
	switch (type)
	{
	case ScriptValue::Type::Int:
		if (!json.is_number_integer())
		{
			error = "this global is a whole number";
			return std::nullopt;
		}
		value.integer = static_cast<int32_t>(json.get<int64_t>());
		return value;
	case ScriptValue::Type::Float:
	case ScriptValue::Type::Vector:
		if (!json.is_number())
		{
			error = "this global is a number";
			return std::nullopt;
		}
		value.number = static_cast<float>(json.get<double>());
		return value;
	case ScriptValue::Type::Object:
	{
		const auto id = json.is_object() ? json.find("object") : json.end();
		if (json.is_object() && id != json.end() && id->is_number_unsigned())
		{
			value.object = static_cast<uint32_t>(id->get<uint64_t>());
			return value;
		}
		if (json.is_number_unsigned())
		{
			value.object = static_cast<uint32_t>(json.get<uint64_t>());
			return value;
		}
		error = "this global is an object: {\"object\": id}";
		return std::nullopt;
	}
	case ScriptValue::Type::Boolean:
		if (!json.is_boolean())
		{
			error = "this global is true or false";
			return std::nullopt;
		}
		value.boolean = json.get<bool>();
		return value;
	case ScriptValue::Type::String:
		error = "no global is text";
		return std::nullopt;
	}
	error = "unknown type";
	return std::nullopt;
}

std::optional<std::vector<ScriptValue>> openblack::inspector::ArgumentsFromJson(const Json& args, std::string& error)
{
	std::vector<ScriptValue> values;
	if (args.is_null())
	{
		return values;
	}
	if (!args.is_array())
	{
		error = "args is a list";
		return std::nullopt;
	}
	for (const auto& arg : args)
	{
		if (arg.is_boolean())
		{
			values.push_back({.type = ScriptValue::Type::Boolean, .boolean = arg.get<bool>()});
		}
		else if (arg.is_number())
		{
			values.push_back({.type = ScriptValue::Type::Float, .number = static_cast<float>(arg.get<double>())});
		}
		else if (arg.is_string())
		{
			values.push_back({.type = ScriptValue::Type::String, .text = arg.get<std::string>()});
		}
		else if (arg.is_object() && arg.contains("string") && arg.find("string")->is_string())
		{
			values.push_back(
			    {.type = ScriptValue::Type::String, .text = arg.find("string")->get<std::string>(), .typeGiven = true});
		}
		else if (arg.is_array() && arg.size() == 3 && std::ranges::all_of(arg, [](const Json& c) { return c.is_number(); }))
		{
			for (const auto& component : arg)
			{
				values.push_back({.type = ScriptValue::Type::Vector, .number = static_cast<float>(component.get<double>())});
			}
		}
		else if (arg.is_object() && arg.contains("object") && arg.find("object")->is_number_unsigned())
		{
			values.push_back(
			    {.type = ScriptValue::Type::Object, .object = static_cast<uint32_t>(arg.find("object")->get<uint64_t>())});
		}
		else if (arg.is_object() && arg.contains("int") && arg.find("int")->is_number_integer())
		{
			values.push_back({.type = ScriptValue::Type::Int,
			                  .integer = static_cast<int32_t>(arg.find("int")->get<int64_t>()),
			                  .typeGiven = true});
		}
		else if (arg.is_object() && arg.contains("float") && arg.find("float")->is_number())
		{
			values.push_back({.type = ScriptValue::Type::Float,
			                  .number = static_cast<float>(arg.find("float")->get<double>()),
			                  .typeGiven = true});
		}
		else
		{
			error = "an argument is a number, true or false, a text, [x, y, z], {\"object\": id}, {\"int\": n}, "
			        "{\"float\": x} or {\"string\": \"...\"}";
			return std::nullopt;
		}
		if (values.size() > k_MostArguments)
		{
			error = "too many arguments";
			return std::nullopt;
		}
	}
	return values;
}

std::vector<std::optional<ScriptValue::Type>> openblack::inspector::NativeSlots(std::string_view name, int32_t stackIn)
{
	std::vector<std::optional<ScriptValue::Type>> slots;
	const auto natives = openblack::lhvm::DefaultNativeSignatures();
	const auto found = std::ranges::find(natives, name, &openblack::lhvm::NativeSignature::name);
	if (found == natives.end() || found->params.empty() || stackIn < 0)
	{
		return {};
	}
	for (const auto& param : found->params)
	{
		switch (param.type)
		{
		case openblack::lhvm::ArgType::Int:
			slots.emplace_back(ScriptValue::Type::Int);
			break;
		case openblack::lhvm::ArgType::Float:
			slots.emplace_back(ScriptValue::Type::Float);
			break;
		case openblack::lhvm::ArgType::Bool:
			slots.emplace_back(ScriptValue::Type::Boolean);
			break;
		case openblack::lhvm::ArgType::Object:
			slots.emplace_back(ScriptValue::Type::Object);
			break;
		case openblack::lhvm::ArgType::Coord:
			slots.insert(slots.end(), 3, ScriptValue::Type::Vector);
			break;
		case openblack::lhvm::ArgType::VarArgs:
			return {};
		case openblack::lhvm::ArgType::String:
			slots.emplace_back(ScriptValue::Type::String);
			break;
		case openblack::lhvm::ArgType::None:
		case openblack::lhvm::ArgType::Any:
			slots.emplace_back(std::nullopt);
			break;
		}
	}
	if (slots.size() != static_cast<size_t>(stackIn))
	{
		return {};
	}
	return slots;
}

bool openblack::inspector::TypeArguments(std::vector<ScriptValue>& values,
                                         std::span<const std::optional<ScriptValue::Type>> slots, std::string& error)
{
	if (slots.empty())
	{
		return true;
	}
	if (slots.size() != values.size())
	{
		error = "the native takes " + std::to_string(slots.size()) + " values; " + std::to_string(values.size()) + " given";
		return false;
	}
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (!slots[i].has_value())
		{
			continue;
		}
		auto& value = values[i];
		const auto wanted = *slots[i];
		// Given with its type, it goes as that: the explicit way past a native's slot
		if (value.type == wanted || value.typeGiven)
		{
			continue;
		}
		// A number or a truth written as text, as some tools send them, is that for a slot that isn't text
		if (value.type == ScriptValue::Type::String)
		{
			if (const auto parsed = Parse(value.text); parsed.has_value() && parsed->is_number())
			{
				value = {.type = ScriptValue::Type::Float, .number = static_cast<float>(parsed->get<double>())};
			}
			else if (parsed.has_value() && parsed->is_boolean())
			{
				value = {.type = ScriptValue::Type::Boolean, .boolean = parsed->get<bool>()};
			}
		}
		const bool number = value.type == ScriptValue::Type::Float || value.type == ScriptValue::Type::Int;
		const float asFloat = value.type == ScriptValue::Type::Int ? static_cast<float>(value.integer) : value.number;
		const bool whole = number && std::floor(asFloat) == asFloat;
		const auto refuse = [&error, i](std::string_view what) {
			error = "argument " + std::to_string(i + 1) + " must be " + std::string(what);
			return false;
		};
		switch (wanted)
		{
		case ScriptValue::Type::Int:
			if (!whole)
			{
				return refuse("a whole number");
			}
			value = {.type = ScriptValue::Type::Int, .integer = static_cast<int32_t>(asFloat)};
			break;
		case ScriptValue::Type::Float:
			if (!number)
			{
				return refuse("a number");
			}
			value = {.type = ScriptValue::Type::Float, .number = asFloat};
			break;
		case ScriptValue::Type::Boolean:
			if (whole && (asFloat == 0.0f || asFloat == 1.0f))
			{
				value = {.type = ScriptValue::Type::Boolean, .boolean = asFloat == 1.0f};
			}
			else if (value.type != ScriptValue::Type::Boolean)
			{
				return refuse("true or false");
			}
			break;
		case ScriptValue::Type::Object:
			if (whole && asFloat >= 0.0f)
			{
				value = {.type = ScriptValue::Type::Object, .object = static_cast<uint32_t>(asFloat)};
			}
			else if (value.type != ScriptValue::Type::Object)
			{
				return refuse("{\"object\": id}");
			}
			break;
		case ScriptValue::Type::Vector:
			if (value.type != ScriptValue::Type::Vector)
			{
				return refuse("part of a position [x, y, z]");
			}
			break;
		case ScriptValue::Type::String:
			if (value.type != ScriptValue::Type::String)
			{
				return refuse("a text: \"...\" or {\"string\": \"...\"}");
			}
			break;
		}
	}
	return true;
}

void openblack::inspector::AddScriptControls(FunctionProvider& provider, ScriptTargetInterface& scripts)
{
	const auto notLoaded = [] { return QueryResult::Error("no land's scripts are loaded"); };
	provider.Add(Description("scripts",
	                         "The land's scripts whose names or files hold a text (any case): name, file, "
	                         "type (script, help, challenge_help, temple_help, temple_special, multiplayer_help) "
	                         "and parameters",
	                         {Parameter("name", "string", "Part of the name or file", false)}, false, ResultKind::List),
	             [&scripts](const QueryContext& context) {
		             const auto filter = Upper(StringMember(context.params, "name").value_or(""));
		             Json items = Json::array();
		             for (const auto& script : scripts.Scripts())
		             {
			             if (Upper(script.name).find(filter) != std::string::npos ||
			                 Upper(script.file).find(filter) != std::string::npos)
			             {
				             items.push_back({{"name", script.name},
				                              {"file", script.file},
				                              {"type", script.type},
				                              {"parameters", script.parameters}});
			             }
		             }
		             return QueryResult::Value(std::move(items));
	             });
	provider.Add(Description("run",
	                         "Starts a script by its name, a challenge or any other function the land's scripts have, "
	                         "as the game starts them; the task it runs in",
	                         {Parameter("name", "string", "The script's name", true)}, true),
	             [&scripts, notLoaded](const QueryContext& context) {
		             if (!scripts.Loaded())
		             {
			             return notLoaded();
		             }
		             const auto name = StringMember(context.params, "name").value_or("");
		             auto started = scripts.Run(name);
		             if (const auto* why = std::get_if<std::string>(&started))
		             {
			             return QueryResult::Error(*why);
		             }
		             return QueryResult::Value({{"script", name}, {"task", std::get<uint32_t>(started)}});
	             });
	provider.Add(Description("stop", "Stops a script task by its number, from script.tasks",
	                         {Parameter("task", "integer", "The task's number", true)}, true),
	             [&scripts](const QueryContext& context) {
		             const auto task = NumberMember(context.params, "task");
		             if (!task.has_value() || *task < 1.0 || std::floor(*task) != *task)
		             {
			             return QueryResult::Error("script.stop needs a task number");
		             }
		             if (auto why = scripts.StopTask(static_cast<uint32_t>(*task)); !why.empty())
		             {
			             return QueryResult::Error(why);
		             }
		             return QueryResult::Value({{"stopped", static_cast<uint32_t>(*task)}});
	             });
	provider.Add(Description("globals", "The scripts' global variables whose names hold a text (any case): name, type, value",
	                         {Parameter("name", "string", "Part of the name", false)}, false, ResultKind::List),
	             [&scripts](const QueryContext& context) {
		             const auto filter = Upper(StringMember(context.params, "name").value_or(""));
		             Json items = Json::array();
		             for (const auto& global : scripts.Globals())
		             {
			             if (Upper(global.name).find(filter) != std::string::npos)
			             {
				             items.push_back({{"name", global.name},
				                              {"type", TypeName(global.value.type)},
				                              {"value", ToJson(global.value)}});
			             }
		             }
		             return QueryResult::Value(std::move(items));
	             });
	provider.Add(Description("set_global", "Sets a global variable by its exact name, keeping its type; its value after",
	                         {Parameter("name", "string", "The global's name", true),
	                          Parameter("value", "any", "A number, true or false, or {\"object\": id}", true)},
	                         true),
	             [&scripts, notLoaded](const QueryContext& context) {
		             if (!scripts.Loaded())
		             {
			             return notLoaded();
		             }
		             const auto name = StringMember(context.params, "name").value_or("");
		             const auto globals = scripts.Globals();
		             const auto found = std::ranges::find_if(globals, [&name](const auto& g) { return g.name == name; });
		             const auto value = context.params.find("value");
		             if (found == globals.end() || value == context.params.end())
		             {
			             return QueryResult::Error("no global " + name + "; ask script.globals");
		             }
		             std::string error;
		             const auto typed = ValueOfType(found->value.type, *value, error);
		             if (!typed.has_value())
		             {
			             return QueryResult::Error(error);
		             }
		             if (auto why = scripts.SetGlobal(name, *typed); !why.empty())
		             {
			             return QueryResult::Error(why);
		             }
		             return QueryResult::Value({{"name", name}, {"type", TypeName(typed->type)}, {"value", ToJson(*typed)}});
	             });
	provider.Add(Description("functions",
	                         "The script natives by name (any case): number, name, values taken and given "
	                         "back, and whether openblack has written them",
	                         {Parameter("name", "string", "Part of the name", false)}, false, ResultKind::List),
	             [&scripts](const QueryContext& context) {
		             const auto filter = Upper(StringMember(context.params, "name").value_or(""));
		             Json items = Json::array();
		             for (const auto& native : scripts.Natives())
		             {
			             if (native.name.find(filter) != std::string::npos)
			             {
				             items.push_back({{"id", native.id},
				                              {"name", native.name},
				                              {"in", native.in},
				                              {"out", native.out},
				                              {"implemented", native.implemented}});
			             }
		             }
		             return QueryResult::Value(std::move(items));
	             });
	provider.Add(
	    Description(
	        "call",
	        "Calls a script native as a script's call to it would, with its arguments in order (a vector is "
	        "[x, y, z], a text such as a file's name \"...\"); what it gave back. Natives that wait for something or "
	        "run over turns aren't for this",
	        {Parameter("native", "string or integer", "The native's name or number, from script.functions", true),
	         Parameter("args", "array",
	                   "Numbers, true or false, texts, [x, y, z], {\"object\": id}, {\"int\": n}, {\"float\": x} or "
	                   "{\"string\": \"...\"}, as many as it takes; a plain number goes on the stack as the type the "
	                   "native's slot takes, {\"int\"}, {\"float\"} and {\"string\"} as given. A text is kept where the "
	                   "native reads a script's texts for the length of the call. Each goes on the stack as the type "
	                   "the native takes, as a script's call puts it",
	                   false),
	         Parameter("raw", "boolean",
	                   "Pushes exactly the values given, past the check of how many the native takes and of their "
	                   "types (for a native that takes a different count than the language's table says, as GET_ARENA "
	                   "does): each number must say its type, {\"int\": n} or {\"float\": x}; a text goes as one",
	                   false)},
	        true),
	    [&scripts, notLoaded](const QueryContext& context) {
		    if (!scripts.Loaded())
		    {
			    return notLoaded();
		    }
		    const auto native = FindNative(scripts, context.params);
		    if (!native.has_value())
		    {
			    return QueryResult::Error("no such native; ask script.functions");
		    }
		    if (!native->implemented)
		    {
			    return QueryResult::Error(native->name + " isn't written in openblack yet");
		    }
		    std::string error;
		    const auto args = context.params.find("args");
		    auto values = ArgumentsFromJson(args != context.params.end() ? *args : Json(nullptr), error);
		    if (!values.has_value())
		    {
			    return QueryResult::Error(error);
		    }
		    const auto raw = context.params.find("raw");
		    if (raw != context.params.end() && !raw->is_boolean())
		    {
			    return QueryResult::Error("raw is true or false");
		    }
		    if (raw != context.params.end() && raw->get<bool>())
		    {
			    // Exactly what was given, in its own types: no count or slot types to fill in a number's type from
			    if (const auto untyped = std::ranges::find_if(*values, &IsUntypedNumber); untyped != values->end())
			    {
				    return QueryResult::Error("argument " + std::to_string(std::distance(values->begin(), untyped) + 1) +
				                              ": with raw, a number says its type: {\"int\": n} or {\"float\": x}");
			    }
			    return Called(scripts, *native, *values);
		    }
		    if (native->in >= 0 && values->size() != static_cast<size_t>(native->in))
		    {
			    return QueryResult::Error(native->name + " takes " + std::to_string(native->in) +
			                              " values (a vector is three); " + std::to_string(values->size()) + " given");
		    }
		    // Each value goes on the stack as the type the native takes, as a script's call puts it: a number given for
		    // an integer (a text's number) is an integer, not a float the native would read as a different number
		    if (!TypeArguments(*values, native->slots, error))
		    {
			    return QueryResult::Error(native->name + ": " + error);
		    }
		    return Called(scripts, *native, *values);
	    });
}
