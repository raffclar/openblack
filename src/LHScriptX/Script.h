/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "Lexer.h"

namespace openblack::lhscriptx
{

/// A line the script reader cannot understand (syntax, unknown command, wrong arguments): the line is skipped
class ScriptError: public std::runtime_error
{
public:
	explicit ScriptError(const std::string& msg)
	    : std::runtime_error(msg)
	{
	}
};

class Script
{
public:
	Script();

	/// The reader's integer slots (one set for every script): reading a line writes slot i only for an 'N' argument
	/// (atol) or an int variable there; any other argument leaves the value of an earlier command. CREATE_VILLAGER "AAL"
	/// reads slot 2 as its age: what the last command with an 'N' third argument left
	[[nodiscard]] static int32_t IntSlot(size_t index);

	/// At the start of a map's features: the position offset none, the player and tribe overrides -1, the VERSION 0, the
	/// last created object none. Not per line. Also (inferred) the vagrants list (town_villagers::ClearVagrants)
	static void BeginMapFeatures();
	/// Added (as MapCoords) to every script position; none: 0. (pending) its writer is the vortex reader
	static void SetPositionOffset(std::optional<map_coords::MapCoords> offset);
	[[nodiscard]] static std::optional<map_coords::MapCoords> PositionOffset();
	/// What the last creation command made, written right after creating, null too; the vortex reader returns it
	[[nodiscard]] static entt::entity LastCreated();
	static void SetLastCreated(entt::entity entity);
	/// (-1 none): CREATE_VILLAGER / _POS take this tribe's villager of the same number when there is one. (pending) its
	/// writer is the vortex reader
	static void SetTribeOverride(std::optional<Tribe> tribe);
	[[nodiscard]] static std::optional<Tribe> TribeOverride();

	void Load(const std::string&);

private:
	[[nodiscard]] bool IsCommand(const std::string& identifier) const;
	void RunCommand(const std::string& identifier, const std::vector<Token>& args);

	const Token* PeekToken(Lexer&);
	const Token* AdvanceToken(Lexer&);

	// The current token.
	Token _token {Token::MakeInvalidToken()};
};

} // namespace openblack::lhscriptx
