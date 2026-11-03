/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MazePuzzleRules.h"

#include <string_view>

#include "Common/X87.h"

namespace openblack::ecs::maze_puzzle
{

namespace
{
/// The layouts of the mazes, the last row along first. Each row has a character for each cell across:
/// - '~' a wall towards the next cell along,
/// - '|' a wall towards the next cell across,
/// - 'L' both walls,
/// - '.' no wall.
using Layout = std::array<std::string_view, k_MostSide>;
constexpr Layout k_FirstTheseus {"........", "~~~~~~L.", "|~~~~~|.", "..~~~L|.",
                                 ".~~|.||.", "||..|||.", "||..~~|.", ".~~~~~~."};
constexpr Layout k_SecondTheseus {"~~~~~~.L", "|~~~~|.|", ".~..|~.|", "...|...|",
                                  ".||....|", "|L~....|", "|......|", ".~~....|"};
constexpr Layout k_FirstTilt {"........", "~~~~L~L.", "|~L...L.", "..~.L.|.", "~.~..L|.", "~.|~.L|.", "....~.|.", ".L.|..L."};
constexpr Layout k_SecondTilt {"~~~L~~LL", "..|.|..L", "|.|....|", "~...~|~|", "L.....~|", "...L.~.|", ".~.||..|", "..|..|~|"};

constexpr char k_AlongWall = '~';
constexpr char k_AcrossWall = '|';
constexpr char k_BothWalls = 'L';

/// The layout of a maze kind. A kind that is not a maze reads the first layout.
const Layout& LayoutOf(script::PuzzleGameType type) noexcept
{
	switch (type)
	{
	case script::PuzzleGameType::Thesius2:
		return k_SecondTheseus;
	case script::PuzzleGameType::Tilt1:
		return k_FirstTilt;
	case script::PuzzleGameType::Tilt2:
		return k_SecondTilt;
	default:
		return k_FirstTheseus;
	}
}

/// The game angle of a direction, from its angle of a quarter circle before the first direction
constexpr float k_BaseAngle = -512.0f;
constexpr float k_Circle = 2048.0f;

/// The game keeps an angle in the circle: one turn less at a full turn and more, one turn more below zero
float Wrapped(float angle) noexcept
{
	if (!(angle < k_Circle))
	{
		angle -= k_Circle;
	}
	if (angle < 0.0f)
	{
		angle += k_Circle;
	}
	return angle;
}

/// The angle that a direction adds to the base angle
float TurnOf(int32_t direction) noexcept
{
	switch (direction)
	{
	case k_AlongUp:
		return 1024.0f;
	case k_AcrossUp:
		return 512.0f;
	case k_AcrossDown:
		return 1536.0f;
	default:
		return 0.0f;
	}
}

/// The lion keeps a number for its moves: three across up, eighteen across down, nine along up and one along down. A
/// second move of the same kind adds nothing. The number gives the angle that the lion then faces.
constexpr int32_t k_AcrossUpCode = 3;
constexpr int32_t k_AcrossDownCode = 0x12;
constexpr int32_t k_AlongUpCode = 9;
constexpr int32_t k_AlongDownCode = 1;

float TurnOfLionCode(int32_t code) noexcept
{
	switch (code)
	{
	case k_AcrossUpCode:
		return 512.0f;
	case k_AcrossUpCode + k_AlongDownCode:
		return 256.0f;
	case k_AlongUpCode:
		return 1024.0f;
	case k_AcrossUpCode + k_AlongUpCode:
		return 768.0f;
	case k_AcrossDownCode:
		return 1536.0f;
	case k_AcrossDownCode + k_AlongDownCode:
		return 1792.0f;
	case k_AcrossDownCode + k_AlongUpCode:
		return 1280.0f;
	default:
		return 0.0f;
	}
}

/// One move of the lion of a Theseus maze, if it can make one. The move adds its kind to the number of the moves.
bool StepLion(script::PuzzleGameType type, Cell& lion, Cell hero, int32_t& code) noexcept
{
	const auto add = [&code](int32_t kind) {
		if (code != kind)
		{
			code += kind;
		}
	};
	if (lion.across < hero.across && !MoveBlocked(type, lion, k_AcrossUp))
	{
		++lion.across;
		add(k_AcrossUpCode);
		return true;
	}
	if (hero.across < lion.across && !MoveBlocked(type, lion, k_AcrossDown))
	{
		--lion.across;
		add(k_AcrossDownCode);
		return true;
	}
	if (lion.along < hero.along && !MoveBlocked(type, lion, k_AlongUp))
	{
		++lion.along;
		add(k_AlongUpCode);
		return true;
	}
	if (hero.along < lion.along && !MoveBlocked(type, lion, k_AlongDown))
	{
		--lion.along;
		add(k_AlongDownCode);
		return true;
	}
	return false;
}

/// The size of a whole number
constexpr int32_t Magnitude(int32_t value) noexcept
{
	return value < 0 ? -value : value;
}
} // namespace

int32_t SideOf(script::PuzzleGameType type) noexcept
{
	return type == script::PuzzleGameType::Thesius2 || type == script::PuzzleGameType::Tilt2 ? 8 : 7;
}

hanoi::Steps MazeSteps(hanoi::Steps steps) noexcept
{
	return {.cosine = steps.cosine / k_StepDivisor, .sine = steps.sine / k_StepDivisor};
}

bool HasWall(script::PuzzleGameType type, int32_t across, int32_t along, int32_t side) noexcept
{
	if (across < 0 || across >= k_MostSide || along < 0 || along >= k_MostSide)
	{
		return true;
	}
	const char cell = LayoutOf(type).at(static_cast<size_t>(k_MostSide - 1 - along)).at(static_cast<size_t>(across));
	switch (side)
	{
	case 0:
		return cell == k_AlongWall || cell == k_BothWalls;
	case 1:
		return cell == k_AcrossWall || cell == k_BothWalls;
	default:
		return true;
	}
}

bool MoveBlocked(script::PuzzleGameType type, Cell cell, int32_t direction) noexcept
{
	switch (direction)
	{
	case k_AlongUp:
		return HasWall(type, cell.across, cell.along, 0);
	case k_AcrossUp:
		return HasWall(type, cell.across, cell.along, 1);
	case k_AlongDown:
		return HasWall(type, cell.across, cell.along - 1, 0);
	case k_AcrossDown:
		return HasWall(type, cell.across - 1, cell.along, 1);
	default:
		return false;
	}
}

std::vector<Fence> FencesOf(script::PuzzleGameType type)
{
	const auto side = SideOf(type);
	std::vector<Fence> fences;
	for (int32_t across = 0; across <= side; ++across)
	{
		for (int32_t along = 0; along <= side; ++along)
		{
			if (along != side && HasWall(type, across - 1, along, 1))
			{
				fences.push_back({.across = across, .along = along, .facesAcross = true});
			}
			if (across != side && HasWall(type, across, along - 1, 0))
			{
				fences.push_back({.across = across, .along = along, .facesAcross = false});
			}
		}
	}
	return fences;
}

GridStep FenceStep(const Fence& fence) noexcept
{
	if (fence.facesAcross)
	{
		return {.across = fence.across * 2, .along = (fence.along * 2) + 1};
	}
	return {.across = (fence.across * 2) + 1, .along = fence.along * 2};
}

float FenceAngle(uint16_t angle, bool facesAcross) noexcept
{
	// The game angle times pi and a 512th, each product rounded to a float
	constexpr float k_Pi = 3.14159274f;                     ///< 0x40490FDB
	constexpr double k_PerHalfCircle = 1.0 / 512.0;         ///< 0x3F60000000000000
	constexpr double k_QuarterCircle = 1.57079637050628662; ///< 0x3FF921FB60000000
	const float radians = x87::Mul(static_cast<float>(angle) * k_Pi, k_PerHalfCircle);
	return facesAcross ? x87::Add(radians, k_QuarterCircle) : radians;
}

std::array<GridStep, 4> CornerSteps(int32_t side) noexcept
{
	const auto far = side * 2;
	return {
	    {{.across = 0, .along = 0}, {.across = far, .along = far}, {.across = far, .along = 0}, {.across = 0, .along = far}}};
}

std::array<GridStep, 4> LanternSteps(int32_t side) noexcept
{
	const auto far = side * 2;
	const auto middle = ((far + 2) / 2) - 2;
	return {{{.across = middle, .along = 0},
	         {.across = far, .along = middle},
	         {.across = middle, .along = far},
	         {.across = 0, .along = middle}}};
}

Cell HeroStart(script::PuzzleGameType type) noexcept
{
	switch (type)
	{
	case script::PuzzleGameType::Thesius1:
		return {.across = 2, .along = 3};
	case script::PuzzleGameType::Thesius2:
		return {.across = 2, .along = 7};
	case script::PuzzleGameType::Tilt1:
		return {.across = 4, .along = 4};
	case script::PuzzleGameType::Tilt2:
		return {.across = 0, .along = 4};
	default:
		return {};
	}
}

Cell LionStart(script::PuzzleGameType type) noexcept
{
	switch (type)
	{
	case script::PuzzleGameType::Thesius1:
		return {.across = 2, .along = 5};
	case script::PuzzleGameType::Thesius2:
		return {.across = 1, .along = 6};
	case script::PuzzleGameType::Tilt1:
		return {.across = 1, .along = 3};
	case script::PuzzleGameType::Tilt2:
		return {.across = 6, .along = 4};
	default:
		return {};
	}
}

std::optional<GridStep> TappedStep(const Grid& grid, int32_t side, int32_t tapX, int32_t tapZ) noexcept
{
	// A place with no whole metres on either axis is no tap
	constexpr uint32_t k_WholeMask = 0xFFFF0000u;
	const auto x = static_cast<uint32_t>(tapX);
	const auto z = static_cast<uint32_t>(tapZ);
	if ((x & k_WholeMask) == 0 && (z & k_WholeMask) == 0)
	{
		return std::nullopt;
	}
	const auto far = (side * 2) + 2;
	const auto corner = hanoi::GridPlace(grid.place, grid.steps, far, far);
	if (x < static_cast<uint32_t>(grid.place.x) || x > static_cast<uint32_t>(corner.x) ||
	    z < static_cast<uint32_t>(grid.place.z) || z > static_cast<uint32_t>(corner.z))
	{
		return std::nullopt;
	}
	const auto dx = static_cast<double>(static_cast<int32_t>(x - static_cast<uint32_t>(grid.place.x)));
	const auto dz = static_cast<double>(static_cast<int32_t>(z - static_cast<uint32_t>(grid.place.z)));
	const auto cosine = static_cast<double>(grid.steps.cosine);
	const auto sine = static_cast<double>(grid.steps.sine);
	// The size of a grid step squared, from the size of the puzzle
	constexpr float k_StepSquared = 171793456.0f; ///< 0x4D23D5C3
	const float divisor = (grid.scale * grid.scale) * k_StepSquared;
	const float across = (x87::Mul(cosine, dx) - x87::Mul(sine, dz)) / divisor;
	const float along = (x87::Mul(sine, dx) + x87::Mul(cosine, dz)) / divisor;
	const auto toMiddle = [](float steps) { return ((map_coords::FtoL(steps) / 2) * 2) + 1; };
	return GridStep {.across = toMiddle(across), .along = toMiddle(along)};
}

HeroChoice ChooseHeroMove(script::PuzzleGameType type, GridStep tapped, Cell hero) noexcept
{
	const auto middle = MiddleOf(hero);
	const auto dx = tapped.across - middle.across;
	const auto dz = tapped.along - middle.along;
	std::optional<int32_t> direction;
	bool wait = false;
	if (Magnitude(dx) <= 1)
	{
		if (Magnitude(dz) <= 1)
		{
			wait = true;
		}
		else
		{
			direction = dz > 0 ? k_AlongUp : k_AlongDown;
		}
	}
	else if (Magnitude(dz) <= 1)
	{
		direction = dx > 0 ? k_AcrossUp : k_AcrossDown;
	}
	if (wait)
	{
		return {.move = HeroMove::Wait, .direction = -1};
	}
	if (!direction.has_value() || MoveBlocked(type, hero, *direction))
	{
		return {};
	}
	return {.move = HeroMove::Step, .direction = *direction};
}

uint16_t AngleOfDirection(int32_t direction) noexcept
{
	return static_cast<uint16_t>(map_coords::FtoL(Wrapped(k_BaseAngle + TurnOf(direction))));
}

Cell Moved(Cell cell, int32_t direction) noexcept
{
	switch (direction)
	{
	case k_AlongUp:
		++cell.along;
		break;
	case k_AcrossUp:
		++cell.across;
		break;
	case k_AlongDown:
		--cell.along;
		break;
	case k_AcrossDown:
		--cell.across;
		break;
	default:
		break;
	}
	return cell;
}

LionMove MoveLion(script::PuzzleGameType type, Cell lion, Cell hero) noexcept
{
	constexpr int32_t k_MostMoves = 2;
	int32_t code = 0;
	for (int32_t move = 0; move < k_MostMoves; ++move)
	{
		if (!StepLion(type, lion, hero, code))
		{
			break;
		}
	}
	if (code == 0)
	{
		return {.cell = lion};
	}
	return {.cell = lion, .angle = static_cast<uint16_t>(map_coords::FtoL(Wrapped(k_BaseAngle + TurnOfLionCode(code))))};
}

Cell Slide(script::PuzzleGameType type, Cell lion, int32_t direction) noexcept
{
	// A lion off the layout meets a wall at once, so the slide always ends
	while (!MoveBlocked(type, lion, direction))
	{
		lion = Moved(lion, direction);
	}
	return lion;
}

bool Escaped(script::PuzzleGameType type, Cell hero) noexcept
{
	const auto side = SideOf(type);
	return hero.across == side || hero.along == side;
}

std::array<BellPlace, k_Bells> BellPlaces(int32_t side) noexcept
{
	const auto far = (side * 2) + 3;
	constexpr int32_t k_Out = -3;
	return {{{.step = {.across = side, .along = far}, .angle = 0.0f},
	         {.step = {.across = far, .along = side}, .angle = 4.5f},
	         {.step = {.across = side, .along = k_Out}, .angle = 3.0f},
	         {.step = {.across = k_Out, .along = side}, .angle = 1.5f},
	         {.step = {.across = k_Out, .along = k_Out}, .angle = 2.4f}}};
}

} // namespace openblack::ecs::maze_puzzle
