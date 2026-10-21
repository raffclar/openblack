/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Which things a miracle may not destroy while a script shows a cut scene
namespace openblack::magic::spell_sparing
{

/// Whether a cut scene keeps a thing from a miracle that would destroy it. A script that owns the widescreen bars keeps
/// the things that it holds, so that its scene can use them. A miracle that a script controls can still destroy them.
[[nodiscard]] constexpr bool CutsceneSpares(bool thingInScript, bool scriptOwnsWideScreen, bool miracleScriptControlled)
{
	return thingInScript && scriptOwnsWideScreen && !miracleScriptControlled;
}

} // namespace openblack::magic::spell_sparing
