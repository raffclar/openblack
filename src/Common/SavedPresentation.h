/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{

/// A part of a saved value that only the frames of this computer change and that no game turn reads. A value gives it
/// to the visitor of its saved values in place of the part. The save keeps the part with the same bytes, but the tools
/// that compare the synced state do not look at it.
template <typename Type>
struct SavedPresentation
{
	Type* value;
};

/// The presentation part of a saved value, for a visitor of the saved values (see SavedPresentation)
template <typename Type>
[[nodiscard]] constexpr SavedPresentation<Type> PresentationOf(Type& value)
{
	return {.value = &value};
}

} // namespace openblack
