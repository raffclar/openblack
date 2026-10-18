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

#include <array>
#include <bitset>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

/// Tests on the types that the fields of components hold, which the reflection writes and reads by their kind
namespace openblack::ecs::reflection::detail
{

template <typename>
inline constexpr bool k_IsOptional = false;
template <typename Type>
inline constexpr bool k_IsOptional<std::optional<Type>> = true;

/// A list that the JSON of the inspector writes as an array: a vector or a fixed array
template <typename>
inline constexpr bool k_IsList = false;
template <typename Type, typename Allocator>
inline constexpr bool k_IsList<std::vector<Type, Allocator>> = true;
template <typename Type, size_t Size>
inline constexpr bool k_IsList<std::array<Type, Size>> = true;

template <typename>
inline constexpr bool k_IsGlmVector = false;
template <glm::length_t Length, typename Type, glm::qualifier Qualifier>
inline constexpr bool k_IsGlmVector<glm::vec<Length, Type, Qualifier>> = true;

template <typename>
inline constexpr bool k_IsGlmMatrix = false;
template <glm::length_t Columns, glm::length_t Rows, typename Type, glm::qualifier Qualifier>
inline constexpr bool k_IsGlmMatrix<glm::mat<Columns, Rows, Type, Qualifier>> = true;

template <typename>
inline constexpr bool k_IsBitset = false;
template <size_t Size>
inline constexpr bool k_IsBitset<std::bitset<Size>> = true;

template <typename>
inline constexpr bool k_IsStdArray = false;
template <typename Type, size_t Size>
inline constexpr bool k_IsStdArray<std::array<Type, Size>> = true;

/// A fixed list's size, 0 for any other type
template <typename Type>
inline constexpr size_t k_FixedSize = 0;
template <typename Type, size_t Size>
inline constexpr size_t k_FixedSize<std::array<Type, Size>> = Size;

/// A list of any length that keeps its elements in order: a vector or a double-ended queue
template <typename>
inline constexpr bool k_IsSequence = false;
template <typename Type, typename Allocator>
inline constexpr bool k_IsSequence<std::vector<Type, Allocator>> = true;
template <typename Type, typename Allocator>
inline constexpr bool k_IsSequence<std::deque<Type, Allocator>> = true;

/// A map from keys to values, in order or by hash
template <typename>
inline constexpr bool k_IsMap = false;
template <typename Key, typename Value, typename Compare, typename Allocator>
inline constexpr bool k_IsMap<std::map<Key, Value, Compare, Allocator>> = true;
template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
inline constexpr bool k_IsMap<std::unordered_map<Key, Value, Hash, Equal, Allocator>> = true;

/// A set of keys, in order or by hash
template <typename>
inline constexpr bool k_IsSet = false;
template <typename Key, typename Compare, typename Allocator>
inline constexpr bool k_IsSet<std::set<Key, Compare, Allocator>> = true;
template <typename Key, typename Hash, typename Equal, typename Allocator>
inline constexpr bool k_IsSet<std::unordered_set<Key, Hash, Equal, Allocator>> = true;

template <typename>
inline constexpr bool k_IsPair = false;
template <typename First, typename Second>
inline constexpr bool k_IsPair<std::pair<First, Second>> = true;

template <typename>
inline constexpr bool k_IsVariant = false;
template <typename... Types>
inline constexpr bool k_IsVariant<std::variant<Types...>> = true;

/// A value that only points to data that is somewhere else: the binary archive cannot keep it
template <typename Type>
inline constexpr bool k_IsIndirect = std::is_pointer_v<Type> || std::is_member_pointer_v<Type>;
template <typename Type>
inline constexpr bool k_IsIndirect<std::shared_ptr<Type>> = true;
template <typename Type, typename Deleter>
inline constexpr bool k_IsIndirect<std::unique_ptr<Type, Deleter>> = true;
template <typename Type>
inline constexpr bool k_IsIndirect<std::weak_ptr<Type>> = true;

} // namespace openblack::ecs::reflection::detail
