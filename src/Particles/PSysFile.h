/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::psys
{

/// One PROPERTY of a spell file (Data\Spells\ZSpellFiles\SF_*.zzz, the text written by the PSys editor)
struct Value
{
	enum class Type
	{
		Bool,
		Integer,
		Float,
		String,  ///< STRING (a path) and ENUM (an MSH_* / ANIM_* name)
		Pointer, ///< PERSIS_PNTR: the name of another object of the file ("" = NULL_STRING)
		Array,
		Sound, ///< SOUND_ACTION: text = the SOUND_* name, array = LOOPING, ONLYONE, SOFTRELEASE, USESURFACE
	};
	Type type {Type::Integer};
	int integer {0};
	float number {0.0f};
	std::string text;
	std::vector<int> array;
	std::vector<float> numbers; ///< ARRAY: the elements as written (UR_HandSprinkle's KeyPoints are floats)
};

/// BEGINCLASS <Class> <Name> BEGINPROPERTIES ... ENDPROPERTIES ENDCLASS
struct Object
{
	std::string className;
	std::string name;
	std::map<std::string, Value, std::less<>> properties;

	[[nodiscard]] bool Bool(std::string_view key, bool fallback) const;
	[[nodiscard]] int Int(std::string_view key, int fallback) const;
	[[nodiscard]] float Float(std::string_view key, float fallback) const;
	[[nodiscard]] std::string String(std::string_view key) const;
	[[nodiscard]] std::vector<int> Array(std::string_view key) const;
};

/// A parsed spell file: the file data header and its objects
struct File
{
	std::string name;
	Object header; ///< DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge
	std::vector<Object> objects;

	[[nodiscard]] const Object* Find(std::string_view objectName) const;

	/// Parses the text of a spell file; nullopt when it isn't one
	static std::optional<File> Parse(std::string_view text, std::string name);
	/// Data\Spells\ZSpellFiles\<name>.txt if it exists (the original reads a loose .txt first),
	/// else <name>_txt.zzz (u32 size, then zlib)
	static std::shared_ptr<const File> Load(const std::string& name);
};

} // namespace openblack::psys
