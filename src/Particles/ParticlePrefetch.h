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

#include <algorithm>
#include <span>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <xmmintrin.h>
#endif

#include "ParticleEffect.h"

/// The walks of a collection's atoms ask the processor for the memory of the atoms a few places ahead. Each atom, the list
/// of its collections and each of those collections are separate objects, and the address of each comes from the one
/// before it. Thus a walk that only follows them waits for the memory of each in turn.
///
/// The requests change no value. They only bring memory into the cache of the processor.
namespace openblack::particles::prefetch
{

/// The size of a line of the cache of the processor
constexpr size_t k_LineBytes = 64;
/// The most lines that a request for one list asks for
constexpr size_t k_MostListLines = 4;

/// Asks for the memory from this address, for this count of bytes
inline void Bytes(const void* address, size_t bytes)
{
	const auto* first = static_cast<const char*>(address);
	for (size_t offset = 0; offset < bytes; offset += k_LineBytes)
	{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
		_mm_prefetch(first + offset, _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
		__builtin_prefetch(first + offset);
#endif
	}
}

/// Asks for the memory of the first elements of a list. For an element of one line or more, it asks for the first line of
/// each element. A walk of the list reads the start of each element.
template <typename T, typename Allocator>
void List(const std::vector<T, Allocator>& list)
{
	if (list.empty())
	{
		return;
	}
	if constexpr (sizeof(T) >= k_LineBytes)
	{
		const auto count = std::min(list.size(), k_MostListLines);
		for (size_t i = 0; i < count; ++i)
		{
			Bytes(&list[i], 1);
		}
	}
	else
	{
		Bytes(list.data(), std::min(list.size() * sizeof(T), k_MostListLines * k_LineBytes));
	}
}

/// What a walk reads of each atom
enum class Reach
{
	/// Only the atoms
	Atoms,
	/// The atoms and the collections under them
	Collections,
};

/// Which fields of each atom a walk reads
enum class Fields
{
	/// All the fields, as the walk that draws the atoms
	All,
	/// Only the fields at the start of the atom that each step reads (see Atom)
	Stepped,
};

/// The bytes of an atom that a walk reads
[[nodiscard]] inline size_t BytesOf(const Atom& atom, Fields fields)
{
	if (fields == Fields::All)
	{
		return sizeof(Atom);
	}
	return static_cast<size_t>(reinterpret_cast<const char*>(&atom.data + 1) - reinterpret_cast<const char*>(&atom));
}

/// Asks for the memory that the walk comes to after the atom at this index. `direction` is 1 for a walk from the first
/// atom and -1 for a walk from the last atom. A walk that reads the collections under the atoms asks for each object some
/// steps after it asked for the object that holds its address. Thus the walk can read that address from the cache.
inline void AtomsAhead(std::span<const AtomPtr> atoms, size_t index, std::ptrdiff_t direction, Reach reach,
                       Fields fields = Fields::All)
{
	constexpr std::ptrdiff_t k_AtomsOnly = 6;
	constexpr std::ptrdiff_t k_Atom = 8;
	constexpr std::ptrdiff_t k_ListOfCollections = 6;
	constexpr std::ptrdiff_t k_Collection = 4;
	constexpr std::ptrdiff_t k_ListsOfCollection = 2;
	const auto at = [&atoms, index, direction](std::ptrdiff_t ahead) -> const Atom* {
		const auto place = static_cast<std::ptrdiff_t>(index) + ahead * direction;
		return place >= 0 && place < static_cast<std::ptrdiff_t>(atoms.size()) ? atoms[static_cast<size_t>(place)].get()
		                                                                       : nullptr;
	};
	if (reach == Reach::Atoms)
	{
		if (const auto* atom = at(k_AtomsOnly); atom != nullptr)
		{
			Bytes(atom, BytesOf(*atom, fields));
		}
		return;
	}
	// At the start of the walk, it also asks for the atoms before the first atom that it asks for in each step
	const bool first = direction > 0 ? index == 0 : index + 1 == atoms.size();
	if (first)
	{
		for (std::ptrdiff_t ahead = 1; ahead < k_Atom; ++ahead)
		{
			if (const auto* atom = at(ahead); atom != nullptr)
			{
				Bytes(atom, BytesOf(*atom, fields));
			}
		}
	}
	if (const auto* atom = at(k_Atom); atom != nullptr)
	{
		Bytes(atom, BytesOf(*atom, fields));
	}
	if (const auto* atom = at(k_ListOfCollections); atom != nullptr)
	{
		List(atom->subCollections);
	}
	if (const auto* atom = at(k_Collection); atom != nullptr)
	{
		for (const auto& sub : atom->subCollections)
		{
			Bytes(sub.get(), sizeof(Collection));
		}
	}
	if (const auto* atom = at(k_ListsOfCollection); atom != nullptr)
	{
		for (const auto& sub : atom->subCollections)
		{
			List(sub->modifiers);
			List(sub->atoms);
		}
	}
}

} // namespace openblack::particles::prefetch
