/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LogoEffect.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/CrtQsort.h"
#include "Common/X87.h"

namespace openblack::logo_effect
{

namespace
{
// The game's constants, from their bits where the decimal is not exact
constexpr float k_FirstTime = std::bit_cast<float>(0x3A83126Fu);    // 0.001
constexpr float k_TimeRate = std::bit_cast<float>(0x3A902DE0u);     // 0.0011
constexpr float k_TwelfthOf = std::bit_cast<float>(0x3DAAAAABu);    // 1 / 12
constexpr float k_EleventhOf = std::bit_cast<float>(0x3DBA2E8Cu);   // 1 / 11
constexpr float k_FifteenthOf = std::bit_cast<float>(0x3D888889u);  // 1 / 15
constexpr float k_Growth = std::bit_cast<float>(0x3FC51EB8u);       // 1.54
constexpr float k_Tenth = std::bit_cast<float>(0x3DCCCCCDu);        // 0.1
constexpr float k_WheelRate = std::bit_cast<float>(0x3A2EC33Eu);    // 1 / 1500
constexpr float k_Millisecond = std::bit_cast<float>(0x3A83126Fu);  // 0.001
constexpr float k_Damping = std::bit_cast<float>(0xBA83126Fu);      // -0.001
constexpr float k_Spring = std::bit_cast<float>(0x412028F6u);       // 10.01
constexpr float k_SmallestSize = std::bit_cast<float>(0x38D1B717u); // 0.0001
constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBu);     // 2 pi
constexpr float k_Pi = std::bit_cast<float>(0x40490FDBu);
constexpr float k_TitleSize = std::bit_cast<float>(0x3D4CCCCDu);    // 0.05
constexpr float k_FiveSevenths = std::bit_cast<float>(0x3F36DB6Eu); // 5 / 7
constexpr float k_SponsorScale = std::bit_cast<float>(0x3AA3D70Au); // 1 / 800
// The rates of the camera's turns, kept as doubles by the game
constexpr double k_TurnRateX = std::bit_cast<double>(0x3FD68FE260B2C83Full);
constexpr double k_TurnRateY = std::bit_cast<double>(0x3FD125C3DEE78184ull);
constexpr double k_TurnRateZ = std::bit_cast<double>(0x3FC367F4DBDF8F47ull);
constexpr double k_TradeMarkTop = std::bit_cast<double>(0x3FD47AE147AE147Bull);  // 0.32
constexpr double k_TradeMarkLeft = std::bit_cast<double>(0x3FE47AE147AE147Bull); // 0.64

/// The camera starts here and comes back here: the dots make the logo from here
constexpr glm::vec3 k_LogoPosition {0.0f, 0.0f, 100.0f};
/// The depths and the sizes of the dots
constexpr float k_NearestDot = 70.0f;
constexpr float k_FarthestDot = 130.0f;
constexpr float k_SmallSizeDivisor = 200.0f;
constexpr float k_LargeSizeDivisor = 320.0f;
/// Each pixel of the logo is a 240th of the screen's width from the next
constexpr int32_t k_PixelSpacing = 240;
/// The mouse moves the dots in a circle of a tenth of the screen's width round the pointer, by at most 20 pixels a frame
constexpr int32_t k_PushReach = 10;
constexpr int32_t k_LargestPush = 20;
constexpr int32_t k_LargestWheel = 240;
constexpr float k_SlowestSpeed = k_Tenth;
/// The grey of the sky sphere goes from 255 down to 65 as its alpha goes from 160 up to 230
constexpr int32_t k_DarkestSky = 190;

/// Two to the power of x as the processor computes it: the fraction's power is rounded, the whole power is exact
float Exp2(float x)
{
	const auto whole = std::nearbyint(static_cast<double>(x));
	return static_cast<float>(
	    std::ldexp(static_cast<double>(x87::RoundToFloat(std::exp2(x - whole))), static_cast<int>(whole)));
}

/// e to the power of a float, through the powers of two
float Exp(float x)
{
	return Exp2(x87::Mul(x, std::numbers::log2e));
}

/// The ease of the camera's return: half a turn of a cosine from 0 to 1
float Ease(float share)
{
	if (share < 0.0f)
	{
		return 0.0f;
	}
	if (share > 1.0f)
	{
		return 1.0f;
	}
	return x87::Sub(1.0, x87::Cos(share * k_Pi)) * 0.5f;
}

int32_t ClampByte(int32_t value)
{
	if (value <= 0)
	{
		return 0;
	}
	return value >= 255 ? 255 : value;
}

int32_t ClampPush(int32_t value)
{
	if (value <= -k_LargestPush)
	{
		return -k_LargestPush;
	}
	return value >= k_LargestPush ? k_LargestPush : value;
}

} // namespace

Lens LensOf(float viewAcross, float nearClip, Screen screen)
{
	const auto tanHalf = x87::Tan(viewAcross * 0.5f);
	const auto aspect = static_cast<float>(screen.width) / static_cast<float>(screen.height);
	const auto extentX = x87::Mul(tanHalf, nearClip);
	const auto clipScaleX = x87::RoundToFloat(1.0 / tanHalf);
	return {.extentX = extentX,
	        .extentY = extentX / aspect,
	        .nearClip = nearClip,
	        .clipScaleX = clipScaleX,
	        .clipScaleY = aspect * clipScaleX};
}

View LookAt(glm::vec3 position, glm::vec3 focus)
{
	constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
	const auto forward = glm::normalize(focus - position);
	const auto right = glm::normalize(glm::cross(k_Up, forward));
	return {.position = position, .right = right, .up = glm::cross(forward, right), .forward = forward};
}

glm::vec3 ScreenToWorld(const View& view, const Lens& lens, Screen screen, glm::ivec2 pixel, float depth)
{
	const auto halfX = static_cast<float>(screen.width) * 0.5f;
	const auto halfY = static_cast<float>(screen.height) * 0.5f;
	auto x = ((static_cast<float>(pixel.x) - halfX) * lens.extentX) / halfX;
	auto y = ((halfY - static_cast<float>(pixel.y)) * lens.extentY) / halfY;
	auto z = lens.nearClip;
	if (depth != 0.0f)
	{
		const auto scale = depth / lens.nearClip;
		x = x * scale;
		y = y * scale;
		z = depth;
	}
	const auto axis = [x, y, z](float right, float up, float forward) { return ((forward * z) + (up * y)) + (right * x); };
	return view.position + glm::vec3(axis(view.right.x, view.up.x, view.forward.x),
	                                 axis(view.right.y, view.up.y, view.forward.y),
	                                 axis(view.right.z, view.up.z, view.forward.z));
}

std::optional<Projected> WorldToScreen(const View& view, const Lens& lens, Screen screen, glm::vec3 point)
{
	const auto relative = point - view.position;
	const auto depth = glm::dot(view.forward, relative);
	if (depth < lens.nearClip)
	{
		return std::nullopt;
	}
	const auto clipX = glm::dot(view.right, relative) * lens.clipScaleX;
	const auto clipY = glm::dot(view.up, relative) * lens.clipScaleY;
	const auto inverse = 1.0f / depth;
	const auto halfX = static_cast<float>(screen.width) * 0.5f;
	const auto halfY = static_cast<float>(screen.height) * 0.5f;
	const auto y = static_cast<int32_t>((1.0f - (inverse * clipY)) * halfY);
	const auto x = static_cast<int32_t>(((inverse * clipX) + 1.0f) * halfX);
	return Projected {.pixel = {x, y}, .depth = depth, .onScreen = x >= 0 && y >= 0 && x < screen.width && y < screen.height};
}

std::optional<Picture> ReadUserLogo(std::span<const uint8_t> file)
{
	constexpr size_t k_TextStart = k_LogoPixels;
	if (file.size() < k_TextStart + sizeof(char16_t))
	{
		return std::nullopt;
	}
	Picture picture;
	for (size_t i = 0; i < k_LogoPixels; ++i)
	{
		std::fill_n(picture.rgb.begin() + static_cast<std::ptrdiff_t>(i * 3), 3, file[i]);
	}
	const auto characters = std::min((file.size() - k_TextStart) / sizeof(char16_t), k_UserTextLength);
	for (size_t i = 0; i < characters; ++i)
	{
		const auto c = static_cast<char16_t>(file[k_TextStart + (i * 2)] | (file[k_TextStart + (i * 2) + 1] << 8U));
		if (c == 0)
		{
			break;
		}
		picture.text.push_back(c);
	}
	if (picture.text.empty())
	{
		return std::nullopt;
	}
	return picture;
}

std::optional<Picture> LionheadLogo(std::span<const uint8_t> file)
{
	if (file.size() < k_LogoBytes)
	{
		return std::nullopt;
	}
	Picture picture {.text = std::u16string(k_LionheadText), .lionhead = true};
	std::copy_n(file.begin(), k_LogoBytes, picture.rgb.begin());
	return picture;
}

std::optional<std::array<glm::vec3, 4>> DotCorners(const Dot& dot, const View& view, float nearClip)
{
	if (glm::dot(view.forward, dot.position - view.position) <= nearClip)
	{
		return std::nullopt;
	}
	const auto size = dot.halfSize;
	std::array<glm::vec2, 4> corners {glm::vec2 {-size, size}, {size, size}, {size, -size}, {-size, -size}};
	if (dot.angle != 0.0f)
	{
		const auto cosine = std::cos(dot.angle);
		const auto sine = std::sin(dot.angle);
		for (auto& corner : corners)
		{
			corner = {(cosine * corner.x) + (sine * corner.y), (cosine * corner.y) - (sine * corner.x)};
		}
	}
	std::array<glm::vec3, 4> world {};
	std::ranges::transform(corners, world.begin(), [&dot, &view](glm::vec2 corner) {
		return dot.position + (view.right * corner.x) + (view.up * corner.y);
	});
	return world;
}

uint8_t FadeAt(float time)
{
	int32_t alpha = 0;
	if (time < 12.0f)
	{
		alpha = static_cast<int32_t>(255.0f - ((time * k_TwelfthOf) * 255.0f));
	}
	if (time > 31.0f)
	{
		alpha = static_cast<int32_t>(255.0f - ((33.0f - time) * 127.5f));
	}
	return static_cast<uint8_t>(ClampByte(alpha));
}

Argb SkyColourAt(float time)
{
	auto shade = static_cast<int32_t>((time - 22.0f) * 50.0f);
	shade = shade <= 0 ? 0 : std::min(shade, k_DarkestSky);
	// The game's integer steps for 255 - 255 * shade / 256, one for each colour
	const auto light = static_cast<uint32_t>(-255 * shade);
	const auto red = (static_cast<uint32_t>(static_cast<int32_t>(light << 16U) >> 8) - 0x10000U) & 0xFF0000U;
	const auto green = (static_cast<uint32_t>(static_cast<int32_t>(light << 8U) >> 8) - 0x100U) & 0xFF00U;
	const auto blue = (static_cast<uint32_t>(static_cast<int32_t>(light) >> 8) - 1U) & 0xFFU;
	const auto alpha = static_cast<uint32_t>(((95 * shade) / 256) + 160);
	return (alpha << 24U) | red | green | blue;
}

float TextShareAt(float time)
{
	const auto rising = time - 22.0f;
	const auto falling = 1.0f - (time - 33.0f);
	const auto share = time > 33.0f ? falling : rising;
	if (!(share > 0.0f))
	{
		return 0.0f;
	}
	return share < 1.0f ? share : 1.0f;
}

float ReturnAt(float time)
{
	auto share = (time - 15.0f) * k_EleventhOf;
	if (!(share > 0.0f))
	{
		share = 0.0f;
	}
	else if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	return Ease(share);
}

float FlightAt(float time)
{
	return 1.0f - ((time - 30.0f) * 0.5f);
}

bool EndsAt(float time)
{
	return !(FlightAt(time) > -2.0f);
}

Effect::Effect(const Picture& picture, Screen screen, const Lens& lens, const Random& random)
    : _screen(screen)
{
	const auto view = LookAt(k_LogoPosition, glm::vec3(0.0f));
	_dots.reserve(k_DotCount);
	// The dots go down each column of the logo in turn, from its left column
	for (int32_t column = 0; column < k_LogoSize; ++column)
	{
		for (int32_t row = 0; row < k_LogoSize; ++row)
		{
			const auto depth = random(k_NearestDot, k_FarthestDot);
			const glm::ivec2 pixel {((screen.width * (column - (k_LogoSize / 2))) / k_PixelSpacing) + (screen.width >> 1),
			                        ((screen.width * (row - (k_LogoSize / 2))) / k_PixelSpacing) + (screen.height >> 1)};
			const auto at = static_cast<size_t>((row * k_LogoSize) + column) * 3;
			const auto red = picture.rgb.at(at);
			const auto green = picture.rgb.at(at + 1);
			const auto blue = picture.rgb.at(at + 2);
			Dot dot {.position = ScreenToWorld(view, lens, screen, pixel, depth),
			         .halfSize = 1.0f,
			         .angle = 0.0f,
			         .colour = 0xFF000000U | (static_cast<uint32_t>(red) << 16U) | (static_cast<uint32_t>(green) << 8U) | blue,
			         .additive = green >= 0x80};
			dot.angle = random(0.0f, k_FullTurn);
			const auto size = depth / random(k_SmallSizeDivisor, k_LargeSizeDivisor);
			dot.halfSize = size < k_SmallestSize ? k_SmallestSize : size;
			_dots.push_back(dot);
		}
	}
	// The dots are drawn from the least blue, in the order of the game's sort
	crt_qsort::Sort(std::span(_dots), [](const Dot& a, const Dot& b) {
		return static_cast<int32_t>(a.colour & 0xFFU) - static_cast<int32_t>(b.colour & 0xFFU);
	});
	_homes.reserve(_dots.size());
	_sizes.reserve(_dots.size());
	for (const auto& dot : _dots)
	{
		_homes.push_back(dot.position);
		_sizes.push_back(dot.halfSize);
	}
	_velocities.assign(_dots.size(), glm::vec3(0.0f));
}

Frame Effect::Step(const Input& input, const Random& random)
{
	const auto lens = LensOf(input.viewAcross, k_NearClip, _screen);
	_time = _time == 0.0f ? k_FirstTime : ((static_cast<float>(input.milliseconds) * _speed) * k_TimeRate) + _time;
	const auto time = _time;

	// The camera turns round the logo, near it at first, then comes back to the logo's place
	const auto back = ReturnAt(time);
	auto nearness = (15.0f - time) * k_FifteenthOf;
	if (!(nearness > 0.0f))
	{
		nearness = 0.0f;
	}
	else if (!(nearness < 1.0f))
	{
		nearness = 1.0f;
	}
	const auto radius = (nearness * 15.0f) + 10.0f;
	const auto distance = ((100.0f - radius) * back) + radius;
	const auto turnX = x87::Sin(x87::Add(x87::Mul(time, k_TurnRateX), 2.0));
	const auto turnY = x87::RoundToFloat(x87::Cos(x87::Sub(3.0, x87::Mul(time, k_TurnRateY))));
	const auto turnZ = x87::RoundToFloat(x87::Sin(x87::Add(x87::Mul(time, k_TurnRateZ), 7.0)));
	const glm::vec3 orbit {x87::Mul(turnX, distance), turnY * distance, turnZ * distance};
	const glm::vec3 camera {((k_LogoPosition.x - orbit.x) * back) + orbit.x, ((k_LogoPosition.y - orbit.y) * back) + orbit.y,
	                        ((k_LogoPosition.z - orbit.z) * back) + orbit.z};
	// Then it flies on through the logo: it moves towards the logo's middle and beyond, and looks the same way
	auto flight = FlightAt(time);
	glm::vec3 focus {0.0f};
	if (!(flight > -2.0f) || flight < 1.0f)
	{
		if (!(flight > -2.0f))
		{
			flight = -2.0f;
		}
		const auto behind = 1.0f - flight;
		focus = glm::vec3(0.0f - (camera.x * behind), 0.0f - (camera.y * behind), 0.0f - (behind * camera.z));
	}
	else
	{
		flight = 1.0f;
	}
	const auto view = LookAt(camera * flight, focus);

	const auto textAlpha = static_cast<uint8_t>(static_cast<int32_t>(TextShareAt(time) * 255.0f));
	const auto logoAlpha = static_cast<uint8_t>(ClampByte(static_cast<int32_t>((flight + 1.0f) * 255.0f)));
	if (!(flight > -2.0f))
	{
		_ended = true;
	}
	const auto backSquared = back * back;
	const auto backFourth = backSquared * backSquared;
	const auto growth = ((backFourth * backFourth) * k_Growth) + 1.0f;

	// The mouse's movement is smoothed. The wheel changes the speed for the next frame.
	_smoothedMove.x = static_cast<int32_t>((static_cast<float>(input.mouseMove.x - _smoothedMove.x) * k_Tenth) +
	                                       static_cast<float>(_smoothedMove.x));
	_smoothedMove.y = static_cast<int32_t>((static_cast<float>(input.mouseMove.y - _smoothedMove.y) * k_Tenth) +
	                                       static_cast<float>(_smoothedMove.y));
	const auto wheel = std::clamp(input.wheel, -k_LargestWheel, k_LargestWheel);
	_speed = Exp(static_cast<float>(wheel) * k_WheelRate) * _speed;
	if (_speed <= k_SlowestSpeed)
	{
		_speed = k_SlowestSpeed;
	}
	else if (_speed >= 1.0f)
	{
		_speed = 1.0f;
	}
	const auto milliseconds = static_cast<float>(input.milliseconds);
	const auto seconds = milliseconds * k_Millisecond;
	const auto damping = Exp(milliseconds * k_Damping);
	if (_smoothedMove != glm::ivec2(0))
	{
		Push(view, lens, input, seconds);
	}

	// Each dot springs back to its home, and grows as the camera comes back
	for (size_t i = 0; i < _dots.size(); ++i)
	{
		auto& dot = _dots[i];
		auto& velocity = _velocities[i];
		const auto away = dot.position - _homes[i];
		velocity = velocity - ((away * seconds) * k_Spring);
		velocity = damping * velocity;
		dot.position = (seconds * velocity) + dot.position;
		dot.colour = (dot.colour & 0xFFFFFFU) | (static_cast<uint32_t>(logoAlpha) << 24U);
		const auto size = growth * _sizes[i];
		dot.halfSize = size < k_SmallestSize ? k_SmallestSize : size;
		dot.angle = random(0.0f, k_FullTurn);
	}
	return {.view = view,
	        .lens = lens,
	        .skyColour = SkyColourAt(time),
	        .fade = FadeAt(time),
	        .textAlpha = textAlpha,
	        .logoAlpha = logoAlpha};
}

void Effect::Push(const View& view, const Lens& lens, const Input& input, float seconds)
{
	const auto reach = _screen.width / k_PushReach;
	for (auto& dot : _dots)
	{
		const auto projected = WorldToScreen(view, lens, _screen, dot.position);
		if (!projected.has_value() || !projected->onScreen)
		{
			continue;
		}
		_smoothedMove = {ClampPush(_smoothedMove.x), ClampPush(_smoothedMove.y)};
		const auto across = projected->pixel.x - input.pointer.x;
		if (std::abs(across) >= reach)
		{
			continue;
		}
		const auto down = projected->pixel.y - input.pointer.y;
		if (std::abs(down) >= reach)
		{
			continue;
		}
		auto share = 1.0f - (std::sqrt(static_cast<float>((down * down) + (across * across))) / static_cast<float>(reach));
		if (share <= 0.0f)
		{
			continue;
		}
		share = share * 4.0f;
		const glm::ivec2 pixel {
		    static_cast<int32_t>((static_cast<float>(_smoothedMove.x) * share) + static_cast<float>(projected->pixel.x)),
		    static_cast<int32_t>((static_cast<float>(_smoothedMove.y) * share) + static_cast<float>(projected->pixel.y))};
		const auto moved = ScreenToWorld(view, lens, _screen, pixel, projected->depth);
		auto& velocity = _velocities[static_cast<size_t>(&dot - _dots.data())];
		velocity = (((moved - dot.position) * seconds) * 100.0f) + velocity;
		dot.position = moved;
	}
}

float TitleSize(Screen screen)
{
	return static_cast<float>(screen.height) * k_TitleSize;
}

TextPlace TitlePlace(Screen screen, float width)
{
	return {.position = {(static_cast<float>(screen.width) * 0.5f) - (width * 0.5f),
	                     static_cast<float>(screen.height) * k_FiveSevenths},
	        .size = TitleSize(screen)};
}

TextPlace TradeMarkPlace(Screen screen)
{
	constexpr int32_t k_SizeDivisor = 70;
	return {.position = {static_cast<float>(static_cast<int32_t>(x87::Mul(screen.width, k_TradeMarkLeft))),
	                     static_cast<float>(static_cast<int32_t>(x87::Mul(screen.height, k_TradeMarkTop)))},
	        .size = static_cast<float>(screen.width / k_SizeDivisor)};
}

SponsorBoxes SponsorPlaces(Screen screen)
{
	const auto scale = static_cast<float>(screen.width) * k_SponsorScale;
	const auto margin = scale * 20.0f;
	const auto left = static_cast<int32_t>(margin);
	const auto bottom = static_cast<float>(screen.height) - margin;
	const auto unit = [scale](float units) { return static_cast<int32_t>(scale * units); };
	return {.logo = {.min = {left, static_cast<int32_t>(bottom - (scale * 104.0f))},
	                 .max = {static_cast<int32_t>((scale * 84.0f) + static_cast<float>(left)), static_cast<int32_t>(bottom)}},
	        .label = {.min = {unit(11.0f), unit(455.0f)}, .max = {unit(111.0f), unit(473.0f)}}};
}

} // namespace openblack::logo_effect
