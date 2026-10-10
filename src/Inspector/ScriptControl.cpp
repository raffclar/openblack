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
#include <utility>

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
	}
	return nullptr;
}

std::optional<ScriptValue> openblack::inspector::ValueOfType(ScriptValue::Type type, const Json& json, std::string& error)
{
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
			values.push_back(
			    {.type = ScriptValue::Type::Int, .integer = static_cast<int32_t>(arg.find("int")->get<int64_t>())});
		}
		else
		{
			error = "an argument is a number, true or false, [x, y, z], {\"object\": id} or {\"int\": n}";
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
	        "[x, y, z]); what it gave back. Natives that wait for something or run over turns aren't for this",
	        {Parameter("native", "string or integer", "The native's name or number, from script.functions", true),
	         Parameter("args", "array",
	                   "Numbers, true or false, [x, y, z], {\"object\": id} or {\"int\": n}, as many as it takes", false)},
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
		    const auto values = ArgumentsFromJson(args != context.params.end() ? *args : Json(nullptr), error);
		    if (!values.has_value())
		    {
			    return QueryResult::Error(error);
		    }
		    if (native->in >= 0 && values->size() != static_cast<size_t>(native->in))
		    {
			    return QueryResult::Error(native->name + " takes " + std::to_string(native->in) +
			                              " values (a vector is three); " + std::to_string(values->size()) + " given");
		    }
		    auto called = scripts.CallNative(native->id, *values);
		    if (const auto* why = std::get_if<std::string>(&called))
		    {
			    return QueryResult::Error(*why);
		    }
		    Json results = Json::array();
		    for (const auto& value : std::get<std::vector<ScriptValue>>(called))
		    {
			    results.push_back({{"type", TypeName(value.type)}, {"value", ToJson(value)}});
		    }
		    return QueryResult::Value({{"native", native->name}, {"returned", std::move(results)}});
	    });
}
