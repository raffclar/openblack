/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FormTable.h"

#include <algorithm>
#include <array>

#include "ChlSyntax.h"

namespace openblack::lhvm::chl
{

namespace
{

struct TypeOverride
{
	std::string_view native;
	/// Parameter index, or -1 for the result
	int index;
	ArgType type;
};

// Places where the game's compiled scripts show a value of a different type than the signature table gives: these
// "enable" flags are pushed as truth values, the player of TOGGLE_LEASH as a number, the property read by
// GET_PROPERTY is used as a number without conversion, and MUSIC_PLAYED is a condition.
constexpr auto k_TypeOverrides = std::to_array<TypeOverride>({
    {.native = "ENABLE_DISABLE_ALIGNMENT_MUSIC", .index = 0, .type = ArgType::Bool},
    {.native = "GAME_TIME_ON_OFF", .index = 0, .type = ArgType::Bool},
    {.native = "PAUSE_UNPAUSE_CLIMATE_SYSTEM", .index = 0, .type = ArgType::Bool},
    {.native = "SET_DRAW_LEASH", .index = 0, .type = ArgType::Bool},
    {.native = "SET_LEASH_WORKS", .index = 0, .type = ArgType::Bool},
    {.native = "TOGGLE_LEASH", .index = 0, .type = ArgType::Float},
    {.native = "GET_PROPERTY", .index = -1, .type = ArgType::Float},
    {.native = "MUSIC_PLAYED", .index = -1, .type = ArgType::Bool},
});

struct OverloadChoice
{
	std::string_view native;
	/// Words that pick the second of two natives of the same name and arity
	std::string_view words;
};

// Some natives share a name. The original compiler calls the first for one form and the second for another.
constexpr auto k_SecondOverloads = std::to_array<OverloadChoice>({
    {.native = "START_ANGLE_SOUND", .words = "pitch"},
    {.native = "ENABLE_DISABLE_COMPUTER_PLAYER", .words = "pause"},
    {.native = "GET_OBJECT_HELD", .words = "held by"},
    {.native = "GET_REAL_DAY", .words = "weekday"},
    {.native = "MUSIC_PLAYED", .words = "music $0"},
});

void CollectWords(const std::vector<PatternItem>& items, std::set<std::string, std::less<>>& words)
{
	const auto addWords = [&words](std::string_view text) {
		size_t start = 0;
		while (start < text.size())
		{
			auto end = text.find(' ', start);
			if (end == std::string_view::npos)
			{
				end = text.size();
			}
			if (end > start)
			{
				words.emplace(text.substr(start, end - start));
			}
			start = end + 1;
		}
	};
	for (const auto& item : items)
	{
		switch (item.kind)
		{
		case PatternItemKind::Word:
		case PatternItemKind::Flag:
			addWords(item.text);
			break;
		case PatternItemKind::Choice:
			for (const auto& [text, value] : item.choices)
			{
				addWords(text);
			}
			break;
		case PatternItemKind::Optional:
			CollectWords(item.items, words);
			break;
		default:
			break;
		}
	}
}

/// Highest parameter index the items give a value to, plus one
size_t CountArguments(const std::vector<PatternItem>& items)
{
	size_t count = 0;
	for (const auto& item : items)
	{
		if (item.argument >= 0)
		{
			count = std::max(count, static_cast<size_t>(item.argument) + 1);
		}
		if (item.kind == PatternItemKind::Optional)
		{
			count = std::max(count, CountArguments(item.items));
		}
	}
	return count;
}

} // namespace

ArgType ParameterType(const NativeSignature& signature, size_t index)
{
	for (const auto& override : k_TypeOverrides)
	{
		if (override.native == signature.name && override.index == static_cast<int>(index))
		{
			return override.type;
		}
	}
	return index < signature.params.size() ? signature.params[index].type : ArgType::Any;
}

ArgType ResultType(const NativeSignature& signature)
{
	for (const auto& override : k_TypeOverrides)
	{
		if (override.native == signature.name && override.index < 0)
		{
			return override.type;
		}
	}
	return signature.returnType;
}

FormTable::FormTable(std::span<const NativeSignature> natives, const std::multimap<std::string, uint32_t, std::less<>>& index)
{
	const auto shared = StatementForms();
	const auto supplement = SupplementaryForms();
	std::vector<const StatementForm*> forms;
	for (const auto& form : shared)
	{
		forms.push_back(&form);
	}
	for (const auto& form : supplement)
	{
		forms.push_back(&form);
	}
	_forms.reserve(forms.size());
	for (const auto* formPointer : forms)
	{
		const auto& form = *formPointer;
		// Positions are written [Thing] by the expression grammar itself
		if (form.native == "GET_POSITION")
		{
			continue;
		}
		CompiledForm compiled;
		compiled.source = &form;
		compiled.items = ParsePattern(form.pattern);
		if (compiled.items.empty())
		{
			_rejected.emplace_back(form.pattern);
			continue;
		}
		// Comparisons are operators, and the distance tests "A near B" and "A at B" compare a distance, which the
		// parser builds itself
		if (form.pattern.find(">=") != std::string_view::npos ||
		    (form.native == "GET_DISTANCE" && form.pattern.find(" at ") != std::string_view::npos))
		{
			_rejected.emplace_back(form.pattern);
			continue;
		}
		compiled.argumentCount = CountArguments(compiled.items);
		// Pick the overload whose parameters the pattern fills: the first, or the second for the forms that call it
		const bool second = std::ranges::any_of(k_SecondOverloads, [&form](const OverloadChoice& choice) {
			return choice.native == form.native && form.pattern.find(choice.words) != std::string_view::npos;
		});
		const auto [first, last] = index.equal_range(std::string(form.native));
		const NativeSignature* chosen = nullptr;
		size_t fitting = 0;
		for (auto it = first; it != last; ++it)
		{
			const auto& signature = natives[it->second];
			if (signature.stackIn < 0 && form.native != "SNAPSHOT" && form.native != "UPDATE_SNAPSHOT")
			{
				continue;
			}
			const bool fits = signature.params.size() == compiled.argumentCount;
			if (chosen == nullptr || (fits && (fitting == 0 || second)))
			{
				if (fits && fitting < 2)
				{
					++fitting;
				}
				if (chosen == nullptr || fits)
				{
					chosen = &signature;
					compiled.native = it->second;
				}
			}
		}
		if (chosen == nullptr || chosen->params.size() < compiled.argumentCount)
		{
			_rejected.emplace_back(form.pattern);
			continue;
		}
		compiled.signature = chosen;
		compiled.postfix = compiled.items.front().kind == PatternItemKind::Argument;
		compiled.negated = form.negated;
		_forms.push_back(std::move(compiled));
	}

	for (const auto& form : _forms)
	{
		CollectWords(form.items, _keywords);
		if (form.postfix)
		{
			_postfix.push_back(&form);
		}
		else
		{
			const auto& first = form.items.front();
			std::set<std::string, std::less<>> starts;
			if (first.kind == PatternItemKind::Word)
			{
				starts.insert(first.text);
			}
			else
			{
				// A choice, flag or optional group: every word that can open it
				CollectWords({first}, starts);
				if (first.kind == PatternItemKind::Optional || first.kind == PatternItemKind::Flag)
				{
					// The group may be absent: the form can also start with what follows. Index it under every
					// keyword and let the matcher decide.
					CollectWords(form.items, starts);
				}
			}
			for (const auto& word : starts)
			{
				_prefix[word].push_back(&form);
			}
		}
	}
	for (const auto keyword : k_Keywords)
	{
		_keywords.emplace(keyword);
	}
}

std::span<const CompiledForm* const> FormTable::Prefix(std::string_view word) const
{
	const auto it = _prefix.find(word);
	if (it == _prefix.end())
	{
		return {};
	}
	return it->second;
}

bool FormTable::IsKeyword(std::string_view word) const
{
	return _keywords.contains(word);
}

std::vector<const CompiledForm*> FormTable::FormsOf(std::string_view native) const
{
	std::vector<const CompiledForm*> result;
	for (const auto& form : _forms)
	{
		if (form.source->native == native)
		{
			result.push_back(&form);
		}
	}
	return result;
}

} // namespace openblack::lhvm::chl
