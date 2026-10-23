/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstring>

#include <array>
#include <memory>
#include <type_traits>

#include "ReflectionTraits.h"

/// The bytes that a save keeps for a plain value: the value as it is in memory, with zero in each gap between its fields.
/// The compiler puts gaps between fields and after the last field, and a copy of a value can leave any bytes in them.
/// Without this, two saves of the same value can have different bytes.
namespace openblack::ecs::reflection
{

/// A plain type with gaps between its fields or after them. The type gives each of its fields to a visitor through its
/// static member VisitPlainFields, so that the save writes zero into the gaps. A field that the visitor does not get is
/// written as zero, so the visitor must get every field.
template <typename Type>
concept HasPlainFields = requires(const Type& value) { Type::VisitPlainFields(value, [](const auto & ... /*fields*/) {}); };

/// The bytes of a plain value, aligned as the value is
template <typename Type>
struct alignas(Type) PlainBytes
{
	std::array<std::byte, sizeof(Type)> bytes {};
};

template <typename Type>
    requires std::is_trivially_copyable_v<Type>
[[nodiscard]] PlainBytes<Type> PlainImage(const Type& value);

namespace detail
{

/// Puts the bytes of a field of a value at the same place in the bytes of the value
template <typename Type, typename Field>
void PutPlainField(PlainBytes<Type>& image, const Type& value, const Field& field)
{
	const auto offset =
	    static_cast<size_t>(reinterpret_cast<const std::byte*>(&field) - reinterpret_cast<const std::byte*>(&value));
	const auto fieldImage = PlainImage(field);
	std::memcpy(image.bytes.data() + offset, fieldImage.bytes.data(), sizeof(Field));
}

} // namespace detail

/// The bytes of a plain value with zero in its gaps. The fields of a type with HasPlainFields, the elements of an array
/// and the value in an option get their own gaps set to zero. An empty option has zero in place of its value.
template <typename Type>
    requires std::is_trivially_copyable_v<Type>
PlainBytes<Type> PlainImage(const Type& value)
{
	PlainBytes<Type> image;
	if constexpr (HasPlainFields<Type>)
	{
		Type::VisitPlainFields(value,
		                       [&image, &value](const auto&... fields) { (detail::PutPlainField(image, value, fields), ...); });
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		using Held = typename Type::value_type;
		// An option made on the zero bytes keeps zero in the bytes that it does not use
		auto* option = std::construct_at(reinterpret_cast<Type*>(image.bytes.data()));
		if (value.has_value())
		{
			option->emplace();
			const auto held = PlainImage(*value);
			std::memcpy(static_cast<void*>(std::addressof(**option)), held.bytes.data(), sizeof(Held));
		}
	}
	else if constexpr (detail::k_IsStdArray<Type>)
	{
		for (const auto& element : value)
		{
			detail::PutPlainField(image, value, element);
		}
	}
	else
	{
		// The compiler cannot show that a value with fields has no gaps, unless all its bytes are in whole numbers
		static_assert((!std::is_class_v<Type> && !std::is_union_v<Type>) || detail::PlainBits<Type>() ||
		                  std::has_unique_object_representations_v<Type>,
		              "A plain type that can have gaps between its fields must give its fields with VisitPlainFields");
		std::memcpy(image.bytes.data(), &value, sizeof(Type));
	}
	return image;
}

} // namespace openblack::ecs::reflection
