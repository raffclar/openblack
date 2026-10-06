/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChlWriter.h"

#include <algorithm>
#include <format>
#include <set>

#include "ChlSyntax.h"
#include "Simplify.h"

namespace openblack::lhvm::detail
{

using namespace openblack::lhvm::chl;

namespace
{

[[nodiscard]] bool IsConstant(const ExprPtr& expr)
{
	return expr != nullptr && (expr->kind == ExprKind::Literal || expr->kind == ExprKind::Constant);
}

/// A position of [0, 0, 0], what an absent optional position is compiled as
[[nodiscard]] bool IsOrigin(const ExprPtr& expr)
{
	if (expr->kind != ExprKind::VectorLiteral || expr->args.size() != 3)
	{
		return false;
	}
	return std::ranges::all_of(expr->args, [](const auto& part) { return IsNumber(part, 0.0); });
}

[[nodiscard]] bool IsCallOf(const ExprPtr& expr, std::string_view name, size_t args)
{
	return expr != nullptr && expr->kind == ExprKind::NativeCall && expr->text == name && expr->args.size() == args;
}

void CollectArguments(const std::vector<PatternItem>& items, std::set<int>& out)
{
	for (const auto& item : items)
	{
		if (item.argument >= 0)
		{
			out.insert(item.argument);
		}
		CollectArguments(item.items, out);
	}
}

/// The value under a conversion the statement makes itself
[[nodiscard]] const ExprPtr& Uncast(const ExprPtr& expr)
{
	return expr->kind == ExprKind::Cast && !expr->args.empty() ? expr->args[0] : expr;
}

[[nodiscard]] std::string Join(const std::vector<std::string>& parts)
{
	std::string text;
	for (const auto& part : parts)
	{
		if (part.empty())
		{
			continue;
		}
		if (!text.empty())
		{
			text += ' ';
		}
		text += part;
	}
	return text;
}

} // namespace

ChlWriter::ChlWriter(std::span<const NativeSignature> natives, const ConstantTable* constants,
                     std::vector<Diagnostic>& diagnostics)
    : _natives(natives)
    , _constants(constants)
    , _diagnostics(diagnostics)
{
	for (const auto& native : _natives)
	{
		_byName[native.name].push_back(&native);
	}
}

const NativeSignature* ChlWriter::Signature(const std::string& name, size_t argCount) const
{
	const auto it = _byName.find(name);
	if (it == _byName.end())
	{
		return nullptr;
	}
	for (const auto* native : it->second)
	{
		if (native->params.size() == argCount)
		{
			return native;
		}
	}
	return it->second.front();
}

void ChlWriter::Line(int indent, std::string text, std::vector<uint32_t> ips)
{
	if (_pendingChallenge && _pendingChallenge != _challenge)
	{
		// Snapshots and highlights belong to the challenge named last
		_challenge = _pendingChallenge;
		std::string name;
		if (_constants != nullptr)
		{
			name = _constants->NameOf("ScriptChallengeEnums", *_challenge).value_or("");
		}
		if (name.starts_with("CHALLENGE_"))
		{
			name = name.substr(10);
		}
		if (name.empty())
		{
			name = std::to_string(*_challenge);
			_diagnostics.push_back({.severity = DiagnosticSeverity::Info,
			                        .ip = ips.empty() ? 0 : ips.front(),
			                        .message = "Challenge written as a number: load the script headers to name it"});
		}
		_lines.push_back({.indent = indent, .text = "challenge " + name, .ips = {}});
	}
	_pendingChallenge.reset();
	_lines.push_back({.indent = indent, .text = std::move(text), .ips = std::move(ips)});
}

void ChlWriter::AttachToLast(size_t firstLine, const std::vector<uint32_t>& ips)
{
	if (ips.empty())
	{
		return;
	}
	if (_lines.size() > firstLine)
	{
		auto& last = _lines.back().ips;
		last.insert(last.end(), ips.begin(), ips.end());
	}
	else
	{
		Line(_lines.empty() ? 0 : _lines.back().indent, "// no code", ips);
	}
}

void ChlWriter::Fallback(const Stmt& stmt, std::string_view what)
{
	++_fallbacks;
	const auto ip = stmt.ips.empty() ? stmt.startIp : stmt.ips.front();
	_diagnostics.push_back(
	    {.severity = DiagnosticSeverity::Warning, .ip = ip, .message = std::format("{} can't be compiled back", what)});
}

// Expressions ---------------------------------------------------------------------------------------------------------

std::optional<std::string> ChlWriter::SpecialForm(const ExprPtr& expr)
{
	// Distance tests: "A near B [radius R]", "A at B", and their negations
	const auto* test = expr.get();
	bool negated = false;
	if (test->kind == ExprKind::Unary && test->op == Op::Not)
	{
		test = test->args[0].get();
		negated = true;
	}
	if (test->kind != ExprKind::Binary || !IsCallOf(test->args[0], "GET_DISTANCE", 2))
	{
		return std::nullopt;
	}
	const auto& distance = test->args[0];
	const auto from = Argument(distance->args[0], ArgType::Coord, {});
	const auto to = Argument(distance->args[1], ArgType::Coord, {});
	const auto* nearWord = negated ? "not near" : "near";
	if (test->op == Op::Lt)
	{
		return std::format("{} {} {} radius {}", from, nearWord, to, Argument(test->args[1], ArgType::Float, {}));
	}
	if (test->op == Op::Eq && IsNumber(test->args[1], 0.0) && test->args[1]->type == ValueType::Float)
	{
		return std::format("{} {} {}", from, negated ? "not at" : "at", to);
	}
	return std::nullopt;
}

uint8_t ChlWriter::Precedence(const ExprPtr& expr)
{
	switch (expr->kind)
	{
	case ExprKind::Unary:
	case ExprKind::Binary:
		if (SpecialForm(expr) || (expr->kind == ExprKind::Unary && NegatedForm(expr)))
		{
			return k_FormPrecedence;
		}
		return GetOperator(expr->op).precedence;
	case ExprKind::Cast:
		if (expr->type == ValueType::Float && expr->args[0]->type == ValueType::Int)
		{
			return k_AtomPrecedence;
		}
		if (expr->type == ValueType::Int)
		{
			return k_FormPrecedence;
		}
		return Precedence(expr->args[0]);
	case ExprKind::Duplicate:
		return Precedence(expr->args[0]);
	case ExprKind::Elapsed:
		return k_FormPrecedence;
	case ExprKind::NativeCall:
	{
		// Positions read as one phrase ("camera position", "hand position")
		if (IsCallOf(expr, "GET_POSITION", 1) || expr->type == ValueType::Vector)
		{
			return k_AtomPrecedence;
		}
		// Count nothing while measuring
		const auto savedCalls = _nativeCalls;
		const auto savedFallbacks = _fallbacks;
		const auto savedDiagnostics = _diagnostics.size();
		_greedy = false;
		const auto text = NativeCall(expr);
		_nativeCalls = savedCalls;
		_fallbacks = savedFallbacks;
		_diagnostics.resize(savedDiagnostics);
		return _greedy ? k_FormPrecedence : k_AtomPrecedence;
	}
	case ExprKind::Literal:
		return expr->number < 0 ? GetOperator(Op::Neg).precedence : k_AtomPrecedence;
	default:
		return k_AtomPrecedence;
	}
}

std::string ChlWriter::Expression(const ExprPtr& expr, uint8_t minPrecedence)
{
	if (expr == nullptr)
	{
		return "?";
	}
	std::string text;
	switch (expr->kind)
	{
	case ExprKind::Literal:
		switch (expr->type)
		{
		case ValueType::Float:
		case ValueType::Vector:
			text = FormatNumber(static_cast<float>(expr->number));
			break;
		case ValueType::Int:
		case ValueType::Object:
		case ValueType::Unknown:
			text = std::format("{}", static_cast<int64_t>(expr->number));
			break;
		default:
			text = expr->text;
			break;
		}
		break;
	case ExprKind::Variable:
	case ExprKind::Constant:
		text = expr->text;
		break;
	case ExprKind::Unary:
		if (auto special = SpecialForm(expr))
		{
			text = std::move(*special);
		}
		else if (auto negated = NegatedForm(expr))
		{
			text = std::move(*negated);
		}
		else if (expr->op == Op::Not)
		{
			text = "not " + Expression(expr->args[0], GetOperator(Op::Not).precedence);
		}
		else
		{
			text = "-" + Expression(expr->args[0], GetOperator(Op::Neg).precedence);
		}
		break;
	case ExprKind::Binary:
		if (auto special = SpecialForm(expr))
		{
			text = std::move(*special);
		}
		else
		{
			const auto& info = GetOperator(expr->op);
			text = std::format("{} {} {}", Expression(expr->args[0], info.precedence), info.spelling,
			                   Expression(expr->args[1], static_cast<uint8_t>(info.precedence + 1)));
		}
		break;
	case ExprKind::Cast:
		if (expr->type == ValueType::Float && expr->args[0]->type == ValueType::Int)
		{
			// A game constant used as a number
			text = "variable " +
			       (IsConstant(expr->args[0]) ? Constant(expr->args[0], {}) : Expression(expr->args[0], k_AtomPrecedence));
		}
		else if (expr->type == ValueType::Int)
		{
			// A number used where a game constant is expected
			text = "constant " + Expression(expr->args[0]);
		}
		else
		{
			// Conversions to objects, truth values and positions belong to the forms around them
			text = Expression(expr->args[0], minPrecedence);
			return text;
		}
		break;
	case ExprKind::NativeCall:
		text = NativeCall(expr);
		break;
	case ExprKind::VectorLiteral:
		if (expr->text == "flat")
		{
			// A position on the ground, height zero
			text = std::format("[{}, {}]", Expression(expr->args[0]), Expression(expr->args[2]));
		}
		else
		{
			text = std::format("[{}, {}, {}]", Expression(expr->args[0]), Expression(expr->args[1]), Expression(expr->args[2]));
		}
		break;
	case ExprKind::Component:
		++_fallbacks;
		text = std::format("{} of {}", std::array {"x", "y", "z"}[expr->component % 3],
		                   Expression(expr->args[0], k_AtomPrecedence));
		break;
	case ExprKind::Elapsed:
		text = Expression(expr->args[0], static_cast<uint8_t>(k_FormPrecedence + 1)) + " seconds";
		break;
	case ExprKind::Duplicate:
		text = Expression(expr->args[0], minPrecedence);
		return text;
	case ExprKind::Placeholder:
		++_fallbacks;
		text = expr->text;
		break;
	}
	if (Precedence(expr) < minPrecedence)
	{
		return "(" + text + ")";
	}
	return text;
}

std::string ChlWriter::Constant(const ExprPtr& value, const std::string& enumName)
{
	const auto number = static_cast<int32_t>(value->number);
	if (_constants != nullptr && !enumName.empty())
	{
		if (auto name = _constants->NameOf(enumName, number))
		{
			return *name;
		}
	}
	return std::to_string(number);
}

std::string ChlWriter::Argument(const ExprPtr& arg, ArgType type, const std::string& enumName)
{
	if (type == ArgType::Int && arg->kind == ExprKind::Literal && arg->type == ValueType::Int)
	{
		return Constant(arg, enumName);
	}
	if (type == ArgType::Int && arg->kind == ExprKind::Cast && arg->type == ValueType::Int)
	{
		// "constant" takes the rest of the expression, so it needs no brackets of its own
		return "constant " + Expression(arg->args[0]);
	}
	return Expression(arg, static_cast<uint8_t>(k_FormPrecedence + 1));
}

std::optional<std::string> ChlWriter::Items(const std::vector<PatternItem>& items, const ExprPtr& call,
                                            const NativeSignature* signature)
{
	const auto& args = call->args;
	const auto typeOf = [signature](int index) {
		return signature != nullptr && static_cast<size_t>(index) < signature->params.size()
		           ? signature->params[static_cast<size_t>(index)].type
		           : ArgType::Any;
	};
	// The enum a constant argument is named from: the pattern's, else the parameter's. An object subtype's enum depends
	// on the object type before it: VILLAGER_INFO for SCRIPT_OBJECT_TYPE_VILLAGER.
	const auto enumOf = [this, signature, &args](const PatternItem& item) -> std::string {
		if (!item.enumName.empty() || signature == nullptr || static_cast<size_t>(item.argument) >= signature->params.size())
		{
			return item.enumName;
		}
		std::string name(signature->params[static_cast<size_t>(item.argument)].enumName);
		if (name == "SCRIPT_OBJECT_SUBTYPE" && item.argument > 0 && _constants != nullptr &&
		    IsConstant(args[static_cast<size_t>(item.argument) - 1]))
		{
			constexpr std::string_view k_TypePrefix = "SCRIPT_OBJECT_TYPE_";
			const auto type = _constants->NameOf("SCRIPT_OBJECT_TYPE",
			                                     static_cast<int32_t>(args[static_cast<size_t>(item.argument) - 1]->number));
			if (type && type->starts_with(k_TypePrefix))
			{
				return type->substr(k_TypePrefix.size()) + "_INFO";
			}
		}
		return name;
	};
	std::vector<std::string> parts;
	for (const auto& item : items)
	{
		// A form whose last written part is an expression takes everything after it, like a prefix operator
		_greedy = item.kind == PatternItemKind::Argument &&
		          (typeOf(item.argument) == ArgType::Float || typeOf(item.argument) == ArgType::Any);
		if (item.argument >= static_cast<int>(args.size()))
		{
			return std::nullopt;
		}
		const auto& arg = item.argument >= 0 ? args[static_cast<size_t>(item.argument)] : nullptr;
		switch (item.kind)
		{
		case PatternItemKind::Word:
			parts.push_back(item.text);
			break;
		case PatternItemKind::Argument:
			parts.push_back(Argument(arg, typeOf(item.argument), enumOf(item)));
			break;
		case PatternItemKind::Fixed:
			if (!item.value || !(IsNumber(arg, *item.value) || (*item.value == 0.0 && IsOrigin(arg))))
			{
				return std::nullopt;
			}
			break;
		case PatternItemKind::Flag:
			if (!IsConstant(arg))
			{
				return std::nullopt;
			}
			if (arg->number != 0.0)
			{
				parts.push_back(item.text);
			}
			break;
		case PatternItemKind::Choice:
		{
			if (!IsConstant(arg))
			{
				return std::nullopt;
			}
			const auto it = std::ranges::find(item.choices, arg->number, &std::pair<std::string, double>::second);
			if (it == item.choices.end())
			{
				return std::nullopt;
			}
			parts.push_back(it->first);
			break;
		}
		case PatternItemKind::Optional:
		{
			if (item.argument >= 0)
			{
				// Present exactly when the controlling argument is true; absent arguments are zero
				if (!IsConstant(arg))
				{
					return std::nullopt;
				}
				if (arg->number == 0.0)
				{
					std::set<int> inner;
					CollectArguments(item.items, inner);
					for (const auto index : inner)
					{
						const auto& value = args[static_cast<size_t>(index)];
						if (!IsNumber(value, 0.0) && !IsOrigin(value))
						{
							return std::nullopt;
						}
					}
					break;
				}
			}
			else
			{
				const auto defaulted = std::ranges::find_if(item.items, [](const auto& i) { return i.optionalArgument; });
				if (defaulted != item.items.end() && defaulted->value &&
				    static_cast<size_t>(defaulted->argument) < args.size() &&
				    IsNumber(args[static_cast<size_t>(defaulted->argument)], *defaulted->value))
				{
					break;
				}
			}
			auto inner = Items(item.items, call, signature);
			if (!inner)
			{
				return std::nullopt;
			}
			parts.push_back(std::move(*inner));
			break;
		}
		}
	}
	return Join(parts);
}

std::optional<std::string> ChlWriter::Form(const StatementForm& form, const ExprPtr& call, const NativeSignature* signature)
{
	auto [it, inserted] = _patterns.try_emplace(&form);
	if (inserted)
	{
		it->second = ParsePattern(form.pattern);
	}
	const auto& items = it->second;
	if (items.empty())
	{
		return std::nullopt;
	}
	// The pattern must account for every argument, or the compiler couldn't rebuild the call
	std::set<int> used;
	CollectArguments(items, used);
	if (used.size() != call->args.size() || (!used.empty() && *used.rbegin() != static_cast<int>(call->args.size()) - 1))
	{
		return std::nullopt;
	}
	return Items(items, call, signature);
}

std::optional<std::string> ChlWriter::PropertyCompound(const Stmt& stmt)
{
	// "P of O += e" reads the property inside the value it sets, without the throw-away read of "P of O = e"
	if (stmt.discardedLoad || CallName(stmt) != "SET_PROPERTY" || stmt.expr->args.size() != 3)
	{
		return std::nullopt;
	}
	const auto& args = stmt.expr->args;
	const auto& value = args[2];
	if (value->kind != ExprKind::Binary || !IsCallOf(value->args[0], "GET_PROPERTY", 2) || value->args[0]->swapped)
	{
		return std::nullopt;
	}
	const auto& read = value->args[0]->args;
	if (!IsConstant(read[0]) || !IsConstant(args[0]) || read[0]->number != args[0]->number ||
	    read[1]->kind != ExprKind::Variable || args[1]->kind != ExprKind::Variable || read[1]->text != args[1]->text)
	{
		return std::nullopt;
	}
	const auto op = value->op;
	const char* assign = op == Op::Add   ? "+="
	                     : op == Op::Sub ? "-="
	                     : op == Op::Mul ? "*="
	                     : op == Op::Div ? "/="
	                     : op == Op::Mod ? "%="
	                                     : nullptr;
	if (assign == nullptr)
	{
		return std::nullopt;
	}
	const auto target = Argument(args[0], ArgType::Int, "SCRIPT_OBJECT_PROPERTY_TYPE") + " of " + args[1]->text;
	if ((op == Op::Add || op == Op::Sub) && value->args[1]->kind == ExprKind::Literal &&
	    value->args[1]->type == ValueType::Float && value->args[1]->number == 1.0)
	{
		return target + (op == Op::Add ? "++" : "--");
	}
	return std::format("{} {} {}", target, assign, Expression(value->args[1]));
}

std::optional<std::string> ChlWriter::ChallengeForm(const ExprPtr& call)
{
	const auto& args = call->args;
	const auto challenge = [this](const ExprPtr& id) {
		if (!IsConstant(id))
		{
			return false;
		}
		_pendingChallenge = static_cast<int32_t>(id->number);
		return true;
	};
	const auto camera = [this](const ExprPtr& position, std::string_view callName, std::string_view words) {
		return IsCallOf(position, callName, 0) ? std::string() : std::string(words) + Argument(position, ArgType::Coord, {});
	};
	// The reminder script and its arguments: "Name(a, b)"
	const auto reminder = [this](const ExprPtr& name, std::span<const ExprPtr> arguments) -> std::optional<std::string> {
		if (name->kind != ExprKind::Literal || name->type != ValueType::String || name->text.size() < 2)
		{
			return std::nullopt;
		}
		auto text = name->text.substr(1, name->text.size() - 2);
		if (!arguments.empty())
		{
			text += "(";
			for (size_t i = 0; i < arguments.size(); ++i)
			{
				text += (i == 0 ? "" : ", ") + Expression(arguments[i]);
			}
			text += ")";
		}
		return text;
	};

	if (IsCallOf(call, "CREATE_HIGHLIGHT", 3) && challenge(args[2]))
	{
		return "create highlight " + Argument(args[0], ArgType::Int, "HIGHLIGHT_INFO") + " at " +
		       Argument(args[1], ArgType::Coord, {});
	}
	if (call->text == "SNAPSHOT" && args.size() >= 9 && IsConstant(args[0]) &&
	    IsNumber(args[args.size() - 2], static_cast<double>(args.size() - 9)) && challenge(args.back()))
	{
		const auto function = reminder(args[6], std::span(args).subspan(7, args.size() - 9));
		if (!function)
		{
			return std::nullopt;
		}
		return Join({"snapshot", args[0]->number != 0.0 ? "quest" : "challenge",
		             camera(args[1], "GET_CAMERA_POSITION", "at position "), camera(args[2], "GET_CAMERA_FOCUS", "focus "),
		             "success " + Argument(args[3], ArgType::Float, {}), "alignment " + Argument(args[4], ArgType::Float, {}),
		             Argument(args[5], ArgType::Int, "HELP_TEXT*"), *function});
	}
	if (call->text == "UPDATE_SNAPSHOT" && args.size() >= 6 &&
	    IsNumber(args[args.size() - 2], static_cast<double>(args.size() - 6)) && challenge(args.back()))
	{
		const auto function = reminder(args[3], std::span(args).subspan(4, args.size() - 6));
		if (!function)
		{
			return std::nullopt;
		}
		return Join({"update snapshot", "success " + Argument(args[0], ArgType::Float, {}),
		             "alignment " + Argument(args[1], ArgType::Float, {}), Argument(args[2], ArgType::Int, "HELP_TEXT*"),
		             *function});
	}
	if (IsCallOf(call, "UPDATE_SNAPSHOT_PICTURE", 7) && IsConstant(args[5]) && challenge(args[6]))
	{
		return Join({"update snapshot details", camera(args[0], "GET_CAMERA_POSITION", "at position "),
		             camera(args[1], "GET_CAMERA_FOCUS", "focus "), "success " + Argument(args[2], ArgType::Float, {}),
		             "alignment " + Argument(args[3], ArgType::Float, {}), Argument(args[4], ArgType::Int, "HELP_TEXT*"),
		             args[5]->number != 0.0 ? "taking picture" : ""});
	}
	return std::nullopt;
}

std::optional<std::string> ChlWriter::FormFor(const ExprPtr& call, bool negated)
{
	const auto* signature = Signature(call->text, call->args.size());
	for (const auto* form : FormsForNative(call->text))
	{
		if (form->swapped != call->swapped || form->negated != negated)
		{
			continue;
		}
		if (auto text = Form(*form, call, signature))
		{
			return text;
		}
	}
	return std::nullopt;
}

std::optional<std::string> ChlWriter::NegatedForm(const ExprPtr& expr)
{
	if (expr->kind != ExprKind::Unary || expr->op != Op::Not)
	{
		return std::nullopt;
	}
	// The condition may carry the conversion to a truth value its form makes
	auto call = expr->args[0];
	if (call->kind == ExprKind::Cast && (call->type == ValueType::Bool || call->type == ValueType::Object))
	{
		call = call->args[0];
	}
	if (call->kind != ExprKind::NativeCall)
	{
		return std::nullopt;
	}
	return FormFor(call, true);
}

std::string ChlWriter::NativeCall(const ExprPtr& call)
{
	if (IsCallOf(call, "GET_POSITION", 1))
	{
		return "[" + Expression(call->args[0]) + "]";
	}
	if (auto text = ChallengeForm(call))
	{
		return std::move(*text);
	}
	// "marker at camera NAME": a marker at the focus of a camera position from the camera editor
	if (IsCallOf(call, "CREATE", 3) && IsNumber(call->args[0], 1.0) && IsNumber(call->args[1], 0.0) &&
	    IsCallOf(call->args[2], "CONVERT_CAMERA_FOCUS", 1) && IsConstant(call->args[2]->args[0]))
	{
		return "marker at camera " + Constant(call->args[2]->args[0], "ScriptCameraPosition");
	}
	if (auto text = FormFor(call, false))
	{
		return std::move(*text);
	}
	const auto* signature = Signature(call->text, call->args.size());
	++_nativeCalls;
	std::string text = "native " + call->text + "(";
	for (size_t i = 0; i < call->args.size(); ++i)
	{
		if (i != 0)
		{
			text += ", ";
		}
		const auto type = signature != nullptr && i < signature->params.size() ? signature->params[i].type : ArgType::Any;
		text += type == ArgType::Int ? Argument(call->args[i], type, {}) : Expression(call->args[i]);
	}
	return text + ")";
}

// Statements ----------------------------------------------------------------------------------------------------------

void ChlWriter::Handlers(const StmtList& handlers, int indent)
{
	for (const auto& handler : handlers)
	{
		Statement(*handler, indent);
	}
}

void ChlWriter::Statements(const StmtList& list, int indent)
{
	for (const auto& stmt : list)
	{
		Statement(*stmt, indent);
	}
}

void ChlWriter::Statement(const Stmt& stmt, int indent)
{
	switch (stmt.kind)
	{
	case StmtKind::Expression:
		if (stmt.name == "play" && stmt.args.size() == 3)
		{
			auto text =
			    std::format("{} play {}", Expression(stmt.args[0], k_AtomPrecedence), Argument(stmt.args[1], ArgType::Int, {}));
			// The loop count is a number the statement converts itself
			text += " loop " + Expression(Uncast(stmt.args[2]), static_cast<uint8_t>(k_FormPrecedence + 1));
			Line(indent, std::move(text), stmt.ips);
		}
		else if (stmt.name == "state" && stmt.args.size() == 6)
		{
			auto text = std::format("state {} {}", Expression(stmt.args[0], k_AtomPrecedence),
			                        Argument(stmt.args[1], ArgType::Int, "VILLAGER_STATES"));
			if (stmt.args[2] != nullptr)
			{
				text += " position " + Argument(stmt.args[2], ArgType::Coord, {});
			}
			if (stmt.args[3] != nullptr)
			{
				text += " float " + Argument(stmt.args[3], ArgType::Float, {});
			}
			if (stmt.args[4] != nullptr)
			{
				text += std::format(" ulong {}, {}", Expression(Uncast(stmt.args[4])), Expression(Uncast(stmt.args[5])));
			}
			Line(indent, std::move(text), stmt.ips);
		}
		else if ((stmt.name == "move camera to" && stmt.args.size() == 2) ||
		         (stmt.name == "set camera to" && stmt.args.size() == 1))
		{
			auto text = stmt.name + " " + Argument(stmt.args[0], ArgType::Int, "ScriptCameraPosition");
			if (stmt.args.size() == 2)
			{
				text += " time " + Argument(stmt.args[1], ArgType::Float, {});
			}
			Line(indent, std::move(text), stmt.ips);
		}
		else if (auto compound = PropertyCompound(stmt))
		{
			Line(indent, std::move(*compound), stmt.ips);
		}
		else if (stmt.expr != nullptr && stmt.expr->kind == ExprKind::NativeCall)
		{
			Line(indent, Expression(stmt.expr), stmt.ips);
		}
		else
		{
			Fallback(stmt, "A value left unused");
			Line(indent, "// unused value: " + Expression(stmt.expr), stmt.ips);
		}
		break;
	case StmtKind::Assign:
		if (stmt.op == "++" || stmt.op == "--")
		{
			Line(indent, stmt.name + stmt.op, stmt.ips);
		}
		else if (stmt.op == "zero")
		{
			Fallback(stmt, "Clearing a variable");
			Line(indent, stmt.name + " = 0", stmt.ips);
		}
		else
		{
			Line(indent, std::format("{} {} {}", stmt.name, stmt.op, Expression(stmt.expr)), stmt.ips);
		}
		break;
	case StmtKind::Declaration:
		Line(indent, std::format("{} = {}", stmt.name, Expression(stmt.expr)), stmt.ips);
		break;
	case StmtKind::RunScript:
	{
		auto text = std::format("run {}script {}", stmt.async ? "background " : "", stmt.name);
		if (!stmt.args.empty())
		{
			text += "(";
			for (size_t i = 0; i < stmt.args.size(); ++i)
			{
				text += (i == 0 ? "" : ", ") + Expression(stmt.args[i]);
			}
			text += ")";
		}
		Line(indent, std::move(text), stmt.ips);
		break;
	}
	case StmtKind::WaitUntil:
		if (stmt.expr != nullptr && stmt.expr->kind == ExprKind::Elapsed)
		{
			Line(indent,
			     std::format("wait {} seconds", Expression(stmt.expr->args[0], static_cast<uint8_t>(k_FormPrecedence + 1))),
			     stmt.ips);
		}
		else
		{
			Line(indent, "wait until " + Expression(stmt.expr), stmt.ips);
		}
		break;
	case StmtKind::If:
	{
		std::vector<uint32_t> carried;
		for (size_t i = 0; i < stmt.branches.size(); ++i)
		{
			const auto& branch = stmt.branches[i];
			auto ips = std::move(carried);
			ips.insert(ips.end(), branch.ips.begin(), branch.ips.end());
			if (i == 0)
			{
				Line(indent, "if " + Expression(branch.cond), std::move(ips));
			}
			else if (branch.cond != nullptr)
			{
				Line(indent, "elsif " + Expression(branch.cond), std::move(ips));
			}
			else
			{
				Line(indent, "else", std::move(ips));
			}
			Statements(branch.body, indent + 1);
			carried = branch.closeIps;
		}
		carried.insert(carried.end(), stmt.closeIps.begin(), stmt.closeIps.end());
		Line(indent, "end if", std::move(carried));
		break;
	}
	case StmtKind::While:
	{
		auto close = stmt.closeIps;
		close.insert(close.end(), stmt.midIps.begin(), stmt.midIps.end());
		if (stmt.expr != nullptr)
		{
			Line(indent, "while " + Expression(stmt.expr), stmt.ips);
			Statements(stmt.body, indent + 1);
			Handlers(stmt.handlers, indent + 1);
			Line(indent, "end while", std::move(close));
		}
		else
		{
			Line(indent, "begin loop", stmt.ips);
			Statements(stmt.body, indent + 1);
			Handlers(stmt.handlers, indent + 1);
			Line(indent, "end loop", std::move(close));
		}
		if (!stmt.guarded)
		{
			_diagnostics.push_back({.severity = DiagnosticSeverity::Info,
			                        .ip = stmt.ips.empty() ? stmt.startIp : stmt.ips.front(),
			                        .message = "Loop without the usual exception scope"});
		}
		break;
	}
	case StmtKind::DoWhile:
	{
		// Body, then a backward test: written with a label and goto
		++_gotos;
		const auto label = std::format("label_{}", stmt.startIp);
		Line(indent, label + ":", stmt.ips);
		Statements(stmt.body, indent);
		Line(indent, "if " + Expression(stmt.expr), stmt.closeIps);
		Line(indent + 1, "goto " + label, {});
		Line(indent, "end if", {});
		break;
	}
	case StmtKind::Block:
	{
		auto opening = "begin " + stmt.name;
		if (stmt.name == "dual camera" && stmt.args.size() == 2)
		{
			opening += std::format(" to {} {}", Expression(stmt.args[0], k_AtomPrecedence),
			                       Expression(stmt.args[1], k_AtomPrecedence));
		}
		Line(indent, std::move(opening), stmt.ips);
		Statements(stmt.body, indent + 1);
		Line(indent, "end " + stmt.name + stmt.op, stmt.closeIps);
		break;
	}
	case StmtKind::Marker:
	{
		auto text = stmt.name;
		if (stmt.name == "begin dual camera" && stmt.args.size() == 2)
		{
			text += std::format(" to {} {}", Expression(stmt.args[0], k_AtomPrecedence),
			                    Expression(stmt.args[1], k_AtomPrecedence));
		}
		_diagnostics.push_back({.severity = DiagnosticSeverity::Info,
		                        .ip = stmt.ips.empty() ? stmt.startIp : stmt.ips.front(),
		                        .message = std::format("\"{}\" has no matching end or begin in the same block", stmt.name)});
		Line(indent, std::move(text), stmt.ips);
		break;
	}
	case StmtKind::When:
	case StmtKind::Until:
	{
		Line(indent, std::format("{} {}", stmt.kind == StmtKind::When ? "when" : "until", Expression(stmt.expr)), stmt.ips);
		const auto first = _lines.size();
		Statements(stmt.body, indent + 1);
		AttachToLast(first - 1, stmt.closeIps);
		break;
	}
	case StmtKind::Exception:
	{
		Fallback(stmt, "Exception handlers outside a loop");
		Line(indent, "// exception handlers guard the following", stmt.ips);
		Statements(stmt.body, indent);
		auto mid = stmt.midIps;
		Line(indent, "// guarded by", std::move(mid));
		Handlers(stmt.handlers, indent);
		AttachToLast(0, stmt.closeIps);
		break;
	}
	case StmtKind::Break:
		Fallback(stmt, "Leaving a loop early");
		Line(indent, "break", stmt.ips);
		break;
	case StmtKind::Continue:
		Fallback(stmt, "Skipping to a loop's next turn");
		Line(indent, "continue", stmt.ips);
		break;
	case StmtKind::Goto:
		++_gotos;
		Line(indent, "goto " + stmt.name, stmt.ips);
		break;
	case StmtKind::Label:
		Line(indent, stmt.name + ":", stmt.ips);
		break;
	case StmtKind::Return:
		Fallback(stmt, "Ending a script early");
		Line(indent, "return", stmt.ips);
		break;
	case StmtKind::Raw:
		Line(indent, "// " + stmt.name, stmt.ips);
		break;
	}
}

void ChlWriter::WriteScript(const Script& script)
{
	_challenge.reset();
	auto opening = std::format("begin {} {}", ScriptKindKeyword(script.kind), script.name);
	if (!script.params.empty())
	{
		opening += "(";
		for (size_t i = 0; i < script.params.size(); ++i)
		{
			opening += (i == 0 ? "" : ", ") + script.params[i];
		}
		opening += ")";
	}
	Line(0, std::move(opening), script.beginIps);
	Statements(script.locals, 1);
	Line(0, "start", script.startIps);
	Statements(script.body, 1);
	Handlers(script.handlers, 1);
	auto closing = script.midIps;
	closing.insert(closing.end(), script.endIps.begin(), script.endIps.end());
	Line(0, "end script " + script.name, std::move(closing));
}

} // namespace openblack::lhvm::detail
