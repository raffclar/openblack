/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// A force-feedback project, the files in Data/Immersion: the effects that a force-feedback mouse plays. The file
/// starts with "ifpr" and the number of objects. Each object has its size (with the size itself), its name, its kind
/// and a list of text keys with their text values. A compound object holds the IDs of the effects that it plays together.
namespace openblack::ifr
{

enum class IFRResult : uint8_t
{
	Success = 0,
	ErrTooSmall,
	ErrBadMagic,
	ErrBadObject,
};

[[nodiscard]] std::string_view ResultToStr(IFRResult result);

struct Property
{
	std::string key;
	std::string value;
};

/// One object of the file: an effect, or a compound of effects
struct Object
{
	std::string name;
	/// The kind of the object: "Periodic", "Texture", "Damper", "Compound" and the other effect kinds
	std::string type;
	std::vector<Property> properties;

	/// The text value of a key, none when the object does not have the key
	[[nodiscard]] std::optional<std::string_view> Find(std::string_view key) const;
	/// The number value of a key, the fallback when the object does not have the key
	[[nodiscard]] int64_t Integer(std::string_view key, int64_t fallback) const;
};

/// The wave of a periodic effect, in the order that the file numbers them
enum class Waveform : uint8_t
{
	Sine = 0,
	Square = 1,
	Triangle = 2,
	SawtoothUp = 3,
	SawtoothDown = 4,
};

/// How the strength of an effect starts and ends. The levels go from 0 to 10000 and the times are in microseconds.
struct Envelope
{
	uint32_t attackLevel {0};
	uint32_t attackTime {0};
	uint32_t fadeLevel {0};
	uint32_t fadeTime {0};
};

/// The values that every effect has. The times are in microseconds and the gain goes from 0 to 10000.
struct Timing
{
	/// None when the effect plays until something stops it
	std::optional<uint32_t> duration;
	uint32_t startDelay {0};
	uint32_t iterations {1};
	uint32_t gain {10000};
};

/// A force that changes as a wave: the strength goes from 0 to 10000 and the period is in microseconds. The phase and
/// the direction are in hundredths of a degree. The direction 0 points to the top of the screen.
struct Periodic
{
	Timing timing;
	Waveform waveform {Waveform::Sine};
	uint32_t magnitude {5000};
	int32_t offset {0};
	uint32_t phase {0};
	uint32_t period {1000000};
	int32_t direction {0};
	std::optional<Envelope> envelope;
};

/// The axes of the screen that a texture or a condition works on
enum class Axes : uint8_t
{
	X,
	Y,
	Both,
};

/// The bumps of a texture along one axis of the screen, in pixels. The bumps for the movement in the positive direction
/// and in the negative direction have their own strength, width and space.
struct TextureAxis
{
	int32_t offset {0};
	int32_t positiveMagnitude {10000};
	int32_t negativeMagnitude {10000};
	uint32_t positiveWidth {5};
	uint32_t negativeWidth {5};
	uint32_t positiveSpace {20};
	uint32_t negativeSpace {20};
};

/// A surface of bumps that the mouse feels as it moves across the screen
struct Texture
{
	Timing timing;
	Axes axes {Axes::X};
	/// The bumps on the first axis: the x axis, or the y axis when only the y axis has bumps
	TextureAxis first;
	/// The bumps on the y axis when both axes have bumps
	TextureAxis second;
};

/// A force that pushes against the movement of the mouse (a damper) or against its distance from a point (a spring)
struct ConditionAxis
{
	int32_t offset {0};
	int32_t positiveCoefficient {10000};
	int32_t negativeCoefficient {10000};
	uint32_t positiveSaturation {10000};
	uint32_t negativeSaturation {10000};
	uint32_t deadBand {0};
};

struct Condition
{
	Timing timing;
	/// The kind of condition, for example "Damper" or "Spring"
	std::string type;
	Axes axes {Axes::X};
	ConditionAxis first;
	ConditionAxis second;
};

/// The values of one effect object, with the game's values for the keys that the object does not have
[[nodiscard]] Periodic ReadPeriodic(const Object& object);
[[nodiscard]] Texture ReadTexture(const Object& object);
[[nodiscard]] Condition ReadCondition(const Object& object);

class IFRFile
{
public:
	[[nodiscard]] IFRResult Open(std::span<const uint8_t> data);

	[[nodiscard]] const std::vector<Object>& GetObjects() const { return _objects; }
	/// The object with this name, none when there is no object with the name
	[[nodiscard]] const Object* Find(std::string_view name) const;
	/// The object with this ID, none when there is no object with the ID
	[[nodiscard]] const Object* FindById(std::string_view id) const;
	/// The effects that an object plays: the objects of a compound in their order, or the object itself
	[[nodiscard]] std::vector<const Object*> Effects(const Object& object) const;

private:
	std::vector<Object> _objects;
};

} // namespace openblack::ifr
