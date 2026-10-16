/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IFRFile.h"

#include <charconv>
#include <cstring>

#include <algorithm>
#include <ranges>

namespace openblack::ifr
{

namespace
{

constexpr std::string_view k_Magic = "ifpr";
constexpr size_t k_HeaderSize = 8;

[[nodiscard]] uint32_t ReadU32(std::span<const uint8_t> data, size_t at)
{
	uint32_t value = 0;
	std::memcpy(&value, data.data() + at, sizeof(value));
	return value;
}

/// The text up to the next zero byte, and the place after that byte. None when the data has no zero byte.
[[nodiscard]] std::optional<std::string_view> NextString(std::span<const uint8_t> data, size_t& at)
{
	const auto* begin = reinterpret_cast<const char*>(data.data()) + at;
	const auto rest = data.size() - at;
	const auto* end = static_cast<const char*>(std::memchr(begin, 0, rest));
	if (end == nullptr)
	{
		return std::nullopt;
	}
	const auto length = static_cast<size_t>(end - begin);
	at += length + 1;
	return std::string_view(begin, length);
}

/// The values that every effect kind reads first: how long it plays, after what delay, how many times and how strong
[[nodiscard]] Timing ReadTiming(const Object& object)
{
	Timing timing;
	timing.iterations = static_cast<uint32_t>(object.Integer("Iterations", 1));
	if (object.Integer("InfiniteDuration", 0) == 0)
	{
		const auto duration = static_cast<uint32_t>(object.Integer("Duration", -1));
		// The largest value means that the effect plays until something stops it
		if (duration != UINT32_MAX)
		{
			timing.duration = duration;
		}
	}
	timing.startDelay = static_cast<uint32_t>(object.Integer("StartDelay", 0));
	timing.gain = static_cast<uint32_t>(object.Integer("Gain", 10000));
	return timing;
}

[[nodiscard]] Axes ReadAxes(const Object& object)
{
	switch (object.Integer("Axes", 0))
	{
	case 1:
		return Axes::Y;
	case 2:
		return Axes::Both;
	default:
		return Axes::X;
	}
}

[[nodiscard]] TextureAxis ReadTextureAxis(const Object& object, std::string_view suffix)
{
	const auto key = [suffix](std::string_view name) { return std::string(name) + std::string(suffix); };
	return {
	    .offset = static_cast<int32_t>(object.Integer(key("Offset"), 0)),
	    .positiveMagnitude = static_cast<int32_t>(object.Integer(key("PosMagnitude"), 10000)),
	    .negativeMagnitude = static_cast<int32_t>(object.Integer(key("NegMagnitude"), 10000)),
	    .positiveWidth = static_cast<uint32_t>(object.Integer(key("PosBumpWidth"), 5)),
	    .negativeWidth = static_cast<uint32_t>(object.Integer(key("NegBumpWidth"), 5)),
	    .positiveSpace = static_cast<uint32_t>(object.Integer(key("PosBumpSpace"), 20)),
	    .negativeSpace = static_cast<uint32_t>(object.Integer(key("NegBumpSpace"), 20)),
	};
}

[[nodiscard]] ConditionAxis ReadConditionAxis(const Object& object, std::string_view suffix)
{
	const auto key = [suffix](std::string_view name) { return std::string(name) + std::string(suffix); };
	return {
	    .offset = static_cast<int32_t>(object.Integer(key("Offset"), 0)),
	    .positiveCoefficient = static_cast<int32_t>(object.Integer(key("PosCoefficient"), 10000)),
	    .negativeCoefficient = static_cast<int32_t>(object.Integer(key("NegCoefficient"), 10000)),
	    .positiveSaturation = static_cast<uint32_t>(object.Integer(key("PosSaturation"), 10000)),
	    .negativeSaturation = static_cast<uint32_t>(object.Integer(key("NegSaturation"), 10000)),
	    .deadBand = static_cast<uint32_t>(object.Integer(key("DeadBand"), 0)),
	};
}

} // namespace

std::string_view ResultToStr(IFRResult result)
{
	switch (result)
	{
	case IFRResult::Success:
		return "Success";
	case IFRResult::ErrTooSmall:
		return "The file is too small";
	case IFRResult::ErrBadMagic:
		return "The file does not start with ifpr";
	case IFRResult::ErrBadObject:
		return "An object of the file is not complete";
	}
	return "Unknown result";
}

std::optional<std::string_view> Object::Find(std::string_view key) const
{
	const auto found = std::ranges::find(properties, key, &Property::key);
	if (found == properties.end())
	{
		return std::nullopt;
	}
	return found->value;
}

int64_t Object::Integer(std::string_view key, int64_t fallback) const
{
	const auto text = Find(key);
	if (!text.has_value())
	{
		return fallback;
	}
	int64_t value = 0;
	const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);
	return error == std::errc() ? value : fallback;
}

Periodic ReadPeriodic(const Object& object)
{
	Periodic periodic;
	periodic.timing = ReadTiming(object);
	const auto waveform = object.Integer("Waveform", 0);
	periodic.waveform = waveform >= 0 && waveform <= 4 ? static_cast<Waveform>(waveform) : Waveform::Sine;
	periodic.direction = static_cast<int32_t>(object.Integer("Direction", 0));
	periodic.magnitude = static_cast<uint32_t>(object.Integer("Magnitude", 5000));
	periodic.offset = static_cast<int32_t>(object.Integer("ValueOffset", 0));
	periodic.phase = static_cast<uint32_t>(object.Integer("Phase", 0));
	periodic.period = static_cast<uint32_t>(object.Integer("Period", 1000000));

	// The effect has an envelope only when its start or its end is not at the full strength. An effect that plays until
	// something stops it has no end to fade.
	const bool infinite = !periodic.timing.duration.has_value();
	const Envelope envelope {
	    .attackLevel = static_cast<uint32_t>(object.Integer("AttackLevel", 0)),
	    .attackTime = static_cast<uint32_t>(object.Integer("AttackDuration", 100000)),
	    .fadeLevel = static_cast<uint32_t>(object.Integer("FadeLevel", 0)),
	    .fadeTime = static_cast<uint32_t>(object.Integer("FadeDuration", 100000)),
	};
	const bool attacks = envelope.attackTime != 0 && envelope.attackLevel != periodic.magnitude;
	const bool fades = envelope.fadeTime != 0 && envelope.fadeLevel != periodic.magnitude && !infinite;
	if (attacks || fades)
	{
		periodic.envelope = envelope;
		if (infinite)
		{
			periodic.envelope->fadeLevel = periodic.magnitude;
			periodic.envelope->fadeTime = 0;
		}
	}
	return periodic;
}

Texture ReadTexture(const Object& object)
{
	Texture texture;
	texture.timing = ReadTiming(object);
	texture.axes = ReadAxes(object);
	texture.first = ReadTextureAxis(object, "");
	texture.second = ReadTextureAxis(object, "2");
	return texture;
}

Condition ReadCondition(const Object& object)
{
	Condition condition;
	condition.timing = ReadTiming(object);
	condition.type = object.type;
	condition.axes = ReadAxes(object);
	condition.first = ReadConditionAxis(object, "");
	condition.second = ReadConditionAxis(object, "2");
	return condition;
}

IFRResult IFRFile::Open(std::span<const uint8_t> data)
{
	_objects.clear();
	if (data.size() < k_HeaderSize)
	{
		return IFRResult::ErrTooSmall;
	}
	if (std::string_view(reinterpret_cast<const char*>(data.data()), k_Magic.size()) != k_Magic)
	{
		return IFRResult::ErrBadMagic;
	}
	const auto count = ReadU32(data, k_Magic.size());
	size_t at = k_HeaderSize;
	for (uint32_t i = 0; i < count; ++i)
	{
		if (at + sizeof(uint32_t) > data.size())
		{
			return IFRResult::ErrBadObject;
		}
		const auto size = ReadU32(data, at);
		if (size < sizeof(uint32_t) || at + size > data.size())
		{
			return IFRResult::ErrBadObject;
		}
		const auto block = data.subspan(at + sizeof(uint32_t), size - sizeof(uint32_t));
		at += size;

		size_t inside = 0;
		const auto name = NextString(block, inside);
		const auto type = name.has_value() ? NextString(block, inside) : std::nullopt;
		// An empty text and the size of the first group of keys come next. The keys follow in one list.
		const auto empty = type.has_value() ? NextString(block, inside) : std::nullopt;
		if (!empty.has_value() || inside + sizeof(uint32_t) > block.size())
		{
			return IFRResult::ErrBadObject;
		}
		inside += sizeof(uint32_t);

		Object object {.name = std::string(*name), .type = std::string(*type), .properties = {}};
		while (inside < block.size())
		{
			const auto key = NextString(block, inside);
			const auto value = key.has_value() && inside < block.size() ? NextString(block, inside) : std::nullopt;
			if (!value.has_value())
			{
				return IFRResult::ErrBadObject;
			}
			object.properties.push_back({.key = std::string(*key), .value = std::string(*value)});
		}
		_objects.push_back(std::move(object));
	}
	return IFRResult::Success;
}

const Object* IFRFile::Find(std::string_view name) const
{
	const auto found = std::ranges::find(_objects, name, &Object::name);
	return found != _objects.end() ? &*found : nullptr;
}

const Object* IFRFile::FindById(std::string_view id) const
{
	const auto found = std::ranges::find_if(_objects, [id](const Object& object) { return object.Find("ID") == id; });
	return found != _objects.end() ? &*found : nullptr;
}

std::vector<const Object*> IFRFile::Effects(const Object& object) const
{
	if (object.type != "Compound")
	{
		return {&object};
	}
	std::vector<const Object*> effects;
	const auto contained = object.Find("ContainedObjects").value_or("");
	for (const auto part : std::views::split(contained, ';'))
	{
		const auto id = std::string_view(part.begin(), part.end());
		if (const auto* effect = FindById(id); effect != nullptr && effect->type != "Compound")
		{
			effects.push_back(effect);
		}
	}
	return effects;
}

} // namespace openblack::ifr
