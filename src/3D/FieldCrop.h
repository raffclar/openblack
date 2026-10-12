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

/// A field's crop: once sown, it ages and fills with food every ten turns, faster on good land and differently in the
/// rain, until it is ripe. It shows once it has grown a little, rising out of the ground as it fills, green while young,
/// turning to its own colour as it ripens, and swaying once ripe.
namespace openblack::field_crop
{

/// What a type of field's crop does as it grows
struct Type
{
	/// The age it starts to ripen at, and the age it is ripe at
	float ageGrowth;
	float ageRipe;
	/// How many times it is sown before it grows
	float timesToSow;
	/// The food in it once ripe
	float totalFood;
	/// How fast it ages, by the weather and whether it has started to ripen
	float sunWhenGrowing;
	float sunWhenRipening;
	float rainWhenGrowing;
	float rainWhenRipening;
};

struct Crop
{
	uint8_t timesSown {0};
	float age {0.0f};
	float food {0.0f};
};

/// A crop grows on one turn in every ten, each field on its own turn
inline constexpr uint32_t k_TurnsPerGrowth = 10;

[[nodiscard]] bool IsSown(const Crop& crop, const Type& type);
[[nodiscard]] bool IsRipe(const Crop& crop, const Type& type);

/// What taking food out of a field gives: what the taker gets, and whether the field was emptied
struct Harvest
{
	uint32_t given {0};
	bool emptied {false};
};
/// Food taken out of a sown field: an unripe crop loses more than is taken, by its kind's share before it is ripe. The
/// taker gets what it asked for while food is left. An emptied ripe field gives what it had and must be sown again; an
/// emptied unripe one gives only that share and stays sown.
Harvest RemoveFood(Crop& crop, const Type& type, float ratioBeforeRipe, float amount);

/// A farmer sows the field once more, if it still wants sowing; whether it did
bool PlantCrop(Crop& crop, const Type& type);

/// What a field wants of its farmers
enum class Activity : uint8_t
{
	/// It still wants sowing
	Sowing,
	/// It is sown and growing, and wants nothing yet
	Growing,
	/// It has grown, and is harvested once ripe
	Grown,
};
[[nodiscard]] Activity ActivityOf(const Crop& crop, const Type& type);

/// The land's alignment the crop grows on, from -1 for evil to 1 for good, from the sum of each player's influence there
/// times their alignment
[[nodiscard]] float ClampAlignment(float sum);

/// A turn of growth, if the crop is sown and not yet past ripe: it ages by 1 to 3 on evil to good land, times its type's
/// rate for the weather, and fills with food in step
void Grow(Crop& crop, const Type& type, float alignment, bool raining);

/// How the crop looks while it shows
struct Look
{
	/// The colour, 0xRRGGBB, the land's light on it is multiplied by
	uint32_t tint;
	/// How far it stands out of the ground, from -1 when empty to 0 when full
	float height;
	/// Ripe crops sway in the wind
	bool sways;
};

/// How the crop looks, or nothing while it has too little age or food to show. `leastFood` is the least food that shows,
/// a handful's worth.
[[nodiscard]] std::optional<Look> LookOf(const Crop& crop, const Type& type, uint32_t leastFood);

/// Two colours, 0xRRGGBB, blended in whole steps: `t` of 255 of the way from the first to the second
[[nodiscard]] uint32_t Blend(uint32_t from, uint32_t to, int32_t t);

/// The crop's height moving smoothly to where it should be: from where it is and how fast it is moving, it eases in to
/// stop there a second later, setting off again each frame
struct Settle
{
	float position {-1.0f};
	float speed {0.0f};
};
[[nodiscard]] Settle Advance(Settle settle, float target, float seconds);

/// A crop holds little snow: it shows at most a quarter of what an object standing there would
inline constexpr float k_SnowCap = 64.0f;

/// How far into the ground the crop is drawn, as a fraction of its height: it never sinks more than 80% of it
[[nodiscard]] float Sink(float height);

} // namespace openblack::field_crop
