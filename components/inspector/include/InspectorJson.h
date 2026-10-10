/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

/// The inspector speaks JSON. The library is built without exceptions, so nothing here may throw: text is parsed
/// without throwing, values are read only after their type is checked, and text that isn't valid UTF-8 is replaced
/// rather than refused as it is written out.
namespace openblack::inspector
{

using Json = nlohmann::json;

/// The text of a JSON value on one line, with any invalid UTF-8 in its strings replaced
[[nodiscard]] std::string Dump(const Json& value);

/// A JSON value from text, none if the text isn't JSON
[[nodiscard]] std::optional<Json> Parse(std::string_view text);

/// The value at a dotted path such as "position.x" or "items.0.id", none if there is nothing there
[[nodiscard]] const Json* Find(const Json& value, std::string_view path);

/// A number held in a JSON value, none if it holds no number
[[nodiscard]] std::optional<double> Number(const Json& value);

/// A member of an object read as a type, none when it is missing or of another type
[[nodiscard]] std::optional<double> NumberMember(const Json& object, std::string_view key);
[[nodiscard]] std::optional<std::string> StringMember(const Json& object, std::string_view key);
[[nodiscard]] std::optional<bool> BoolMember(const Json& object, std::string_view key);

} // namespace openblack::inspector
