/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceMoves.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include "3D/AllMeshes.h"
#include "Common/GUtilsAngle.h"

using namespace openblack;
using namespace openblack::ecs;
using components::Dance;
using components::DanceGroup;
using components::DanceMove;
using dance_moves::Action;
using map_coords::FtoL;
using map_coords::MapCoords;

namespace
{
/// Each step rounded to a float, as the game's are
float Round(double value)
{
	return static_cast<float>(value);
}

float Add(float a, float b)
{
	return Round(static_cast<double>(a) + static_cast<double>(b));
}

float Sub(float a, float b)
{
	return Round(static_cast<double>(a) - static_cast<double>(b));
}

float Mul(float a, float b)
{
	return Round(static_cast<double>(a) * static_cast<double>(b));
}

float Div(float a, float b)
{
	return Round(static_cast<double>(a) / static_cast<double>(b));
}

/// The way round a shape, and a turn of a group's rotation, in their own units
constexpr float k_ShapeRound = 256.0f;
constexpr float k_RotationTurn = 2048.0f;
constexpr float k_HalfTurn = 1024.0f;
constexpr float k_TwoPi = 6.2831854820251465f;
constexpr float k_Pi = 3.1415927410125732f;
constexpr float k_HalfPi = 1.5707963705062866f;
constexpr float k_ThreeHalvesPi = 4.71238899230957f;
/// A shape's points are 65535 to the radius, scaled to metres through halves of 65536ths
constexpr float k_ShapeHalf = 0.5f;
constexpr float k_UnitsToFraction = 1.52587890625e-05f;
constexpr float k_FixedPerTenMetres = 65536.0f;
constexpr float k_TenMetres = 10.0f;
/// Turning on the spot goes this far each turn
constexpr uint16_t k_TurnStep = 0x80;

float AsFloat(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

DanceGroup* GroupAt(Dance& dance, std::size_t index)
{
	return index < dance.groups.all.size() ? &dance.groups.all[index] : nullptr;
}

/// A map position's metres, as the game reckons them: the whole units, taken exactly, scaled by ten 65536ths
float ToMetres(int32_t fixed)
{
	return Mul(Round(static_cast<double>(fixed) * static_cast<double>(k_TenMetres)), k_UnitsToFraction);
}

/// Metres back to whole units, truncated
int32_t ToFixed(float metres)
{
	return FtoL(Div(Mul(metres, k_FixedPerTenMetres), k_TenMetres));
}

/// A shape point's share of its group's reach: a 65535th of the radius, halved
float ShapeShare(float unit)
{
	return Mul(Mul(unit, k_ShapeHalf), k_UnitsToFraction);
}

/// An angle round the shape: the place's share of the way round, on from the group's spin, once round at most
float RoundTheShape(uint32_t place, uint32_t places, float spin)
{
	float angle = Add(Div(Mul(static_cast<float>(place), k_ShapeRound), static_cast<float>(places)), spin);
	if (!(angle < k_ShapeRound))
	{
		angle = Sub(angle, k_ShapeRound);
	}
	return angle;
}

/// A half turn on from an angle of 2048 to the turn, once round at most
float HalfTurnOn(float angle)
{
	float turned = Add(angle, k_HalfTurn);
	if (turned > k_RotationTurn)
	{
		turned = Sub(turned, k_RotationTurn);
	}
	return turned;
}

constexpr int32_t k_StandClip = static_cast<int32_t>(AnimId::PStand);

/// The clips of the dances' clip table: a man's, then a woman's
constexpr std::array<std::array<int32_t, 2>, 48> k_ClipTable {{
    {0x13A, 0x109}, // DanceA
    {0x13B, 0x10A}, // DanceB
    {0x13C, 0x10B}, // DanceC
    {0xEC, 0xEC},   // Playful
    {0xEE, 0xEE},   // TogetherArmLink
    {0xEF, 0xEF},   // TogetherCircling
    {0xF0, 0xF0},   // TogetherCirclingPartner
    {0xF1, 0xF1},   // TogetherSideways
    {0xF2, 0xF2},   // TogetherSidewaysPartner
    {0x110, 0x111}, // Gossip
    {0x107, 0x107}, // FrightDuck
    {0x108, 0x108}, // FrightJump
    {0x163, 0x163}, // ScaredStiff
    {0x118, 0x118}, // InspectObject1
    {0x119, 0x119}, // InspectObject2
    {0x11A, 0x11A}, // InspectObject3
    {0x11D, 0x11D}, // IntoMourning
    {0x139, 0x139}, // Mourning
    {0x141, 0x141}, // OutOfMourning
    {0x11E, 0x11E}, // IntoPointing
    {0x142, 0x142}, // OutOfPointing
    {0x11F, 0x11F}, // IntoPray
    {0x157, 0x157}, // Pray
    {0x143, 0x143}, // OutOfPray
    {0x15E, 0x15E}, // Puzzled
    {0x16F, 0x16F}, // IntoSitting1
    {0x171, 0x171}, // Sitting1
    {0x170, 0x170}, // OutOfSitting1
    {0x172, 0x172}, // IntoSitting2
    {0x174, 0x174}, // Sitting2
    {0x173, 0x173}, // OutOfSitting2
    {0x172, 0x172}, // IntoSitting3
    {0x17A, 0x17A}, // Sitting3
    {0x173, 0x173}, // OutOfSitting3
    {0x182, 0x182}, // Despair1
    {0x183, 0x183}, // Despair2
    {0x184, 0x184}, // Despair3
    {0x1B5, 0x1B5}, // Yawn1
    {0x1B6, 0x1B6}, // Yawn2
    {0x14C, 0x14C}, // Overworked1
    {0x14D, 0x14D}, // Overworked2
    {0xDF, 0xE0},   // Kiss
    {0xE3, 0xE3},   // Impressed
    {0xEA, 0xEA},   // Happy
    {0xEB, 0xEB},   // Happy2
    {0xE5, 0xE5},   // Sad
    {0xE6, 0xE6},   // Sad2
    {0x1A2, 0x1A2}, // WaitImpatiently
}};
} // namespace

const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& dance_moves::Shapes()
{
	static const auto k_Shapes = [] {
		std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount> shapes {};
		for (std::size_t i = 0; i < shapes.size(); ++i)
		{
			shapes[i] = dance_shapes::Build(i);
		}
		return shapes;
	}();
	return k_Shapes;
}

bool dance_moves::ApplyAction(Dance& dance, std::size_t index, uint32_t type, const std::array<uint32_t, 14>& arguments,
                              int32_t now)
{
	auto* group = GroupAt(dance, index);
	if (group == nullptr)
	{
		return false;
	}
	switch (type)
	{
	case 0:
		group->shape = arguments[0];
		return false;
	case 1:
		group->offset = {AsFloat(arguments[0]), AsFloat(arguments[1])};
		return false;
	case 2:
		group->radius = AsFloat(arguments[0]);
		return false;
	case 3:
		StartMove(*group,
		          DanceMove {.action = arguments[4], .first = arguments[5], .second = arguments[6], .end = arguments[7]}, now,
		          dance.rate);
		return true;
	case 4:
	{
		// It stands about another group, as the next of its parts
		const auto parentIndex = static_cast<std::size_t>(arguments[0]);
		auto* parent = GroupAt(dance, parentIndex);
		if (parent == nullptr || group->parent == parentIndex)
		{
			return false;
		}
		group->parent = parentIndex;
		group->indexInParent = static_cast<uint8_t>(parent->children.size());
		parent->children.push_back(index);
		return false;
	}
	case 5:
		// It stands about nothing again
		if (group->parent.has_value())
		{
			if (auto* parent = GroupAt(dance, *group->parent); parent != nullptr)
			{
				const auto found = std::ranges::find(parent->children, index);
				if (found != parent->children.end())
				{
					parent->children.erase(found);
				}
			}
			group->parent.reset();
		}
		return false;
	case 11:
		group->rotation = AsFloat(arguments[0]);
		return false;
	case 12:
		group->flag = false;
		return false;
	case 13:
		group->flag = true;
		return false;
	case 17:
	{
		// Turned half the spacing of another group's dancers
		const auto* other = GroupAt(dance, arguments[0]);
		if (other == nullptr)
		{
			return false;
		}
		const auto count = static_cast<uint32_t>(other->dancers.size());
		group->rotation = count > 0 ? static_cast<float>(2048u / (count * 2u)) : 0.0f;
		return false;
	}
	default:
		// TODO(opening): dancers moved over from another group (7), what each dancer is told (9, 10), the dance's camera
		// (18), the setting kept for later (19) and the dance's lights (20-23)
		return false;
	}
}

void dance_moves::StartMove(DanceGroup& group, const DanceMove& move, int32_t now, float rate)
{
	group.move = move;
	const int32_t end = static_cast<int32_t>(move.end);
	const int32_t left = end - now;
	const float perTurn = left > 0 ? Div(rate, static_cast<float>(left)) : 0.0f;
	if (end <= now)
	{
		return;
	}
	switch (static_cast<Action>(move.action))
	{
	case Action::Spin:
		group.spinRate = Mul(perTurn, k_ShapeRound);
		break;
	case Action::Turn:
	{
		const float target = AsFloat(move.second);
		const float current = group.rotation;
		// Forwards by the short way to a later angle, else on round; backwards the other way
		const bool straight = move.first == 1 ? current < target : current > target;
		const float by = straight ? Sub(target, current) : Add(Sub(k_RotationTurn, current), target);
		group.rotationRate = Mul(by, perTurn);
		break;
	}
	case Action::Grow:
		group.radiusRate = Mul(Sub(AsFloat(move.first), group.radius), perTurn);
		break;
	case Action::MoveOff:
		group.offsetRate = {Mul(AsFloat(move.first), perTurn), Mul(AsFloat(move.second), perTurn)};
		break;
	default:
		break;
	}
}

void dance_moves::ProcessGroup(DanceGroup& group, int32_t now)
{
	group.moved = false;
	const auto finish = [&group, now] {
		if (now >= static_cast<int32_t>(group.move.end))
		{
			group.move.action = 0;
			group.offsetRate = {0.0f, 0.0f};
		}
	};
	switch (static_cast<Action>(group.move.action))
	{
	case Action::Spin:
		if (group.move.first == 0)
		{
			const float spun = Add(group.spin, group.spinRate);
			if (spun < k_ShapeRound)
			{
				group.spin = spun;
			}
			else
			{
				const float over = Sub(group.spinRate, Sub(k_ShapeRound, group.spin));
				group.spin = over > 0.0f ? over : 0.0f;
			}
		}
		else if (group.spin < group.spinRate)
		{
			group.spin = Sub(255.0f, Sub(Sub(group.spinRate, group.spin), 1.0f));
		}
		else
		{
			group.spin = Sub(group.spin, group.spinRate);
		}
		break;
	case Action::Turn:
		if (group.move.first == 0)
		{
			group.rotation = Add(group.rotationRate, group.rotation);
			if (!(group.rotation < k_RotationTurn))
			{
				group.rotation = 0.0f;
			}
		}
		else
		{
			group.rotation = Sub(group.rotation, group.rotationRate);
			if (group.rotation < 0.0f)
			{
				group.rotation = k_RotationTurn;
			}
		}
		break;
	case Action::Grow:
		group.radius = Add(group.radiusRate, group.radius);
		finish();
		break;
	case Action::MoveOff:
		group.offset = {Add(group.offsetRate.x, group.offset.x), Add(group.offsetRate.y, group.offset.y)};
		finish();
		break;
	default:
		return;
	}
	group.moved = true;
}

glm::vec2 dance_moves::RotatePointByAngle(glm::vec2 point, float angle)
{
	const float length = Round(std::sqrt(static_cast<double>(Add(Mul(point.y, point.y), Mul(point.x, point.x)))));
	float own = 0.0f;
	if (point.x == 0.0f)
	{
		own = point.y > 0.0f ? k_HalfPi : k_ThreeHalvesPi;
	}
	else if (point.x > 0.0f)
	{
		if (point.y != 0.0f)
		{
			own = Round(std::atan(static_cast<double>(Div(point.y, point.x))));
			if (point.y < 0.0f)
			{
				own = Add(own, k_TwoPi);
			}
		}
	}
	else if (point.y == 0.0f)
	{
		own = k_Pi;
	}
	else
	{
		own = Add(Round(std::atan(static_cast<double>(Div(point.y, point.x)))), k_Pi);
	}
	float turned = Add(own, angle);
	if (turned < 0.0f)
	{
		turned = Add(turned, k_TwoPi);
	}
	else if (turned > k_TwoPi)
	{
		turned = Sub(turned, k_TwoPi);
	}
	const auto game = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(turned));
	return {Mul(Mul(static_cast<float>(gutils::Cos(game)), length), k_UnitsToFraction),
	        Mul(Mul(static_cast<float>(gutils::Sin(game)), length), k_UnitsToFraction)};
}

glm::vec2 dance_moves::ShapePoint(const DanceGroup& group, const dance_shapes::Shape& shape, float angle)
{
	const int32_t at = FtoL(angle);
	const float along = Sub(angle, static_cast<float>(at));
	const auto from = static_cast<std::size_t>(at) % shape.size();
	const auto to = (from + 1) % shape.size();
	const float back = Sub(1.0f, along);
	glm::vec2 point {
	    Add(Mul(back, static_cast<float>(shape[from].x)), Mul(static_cast<float>(shape[to].x), along)),
	    Add(Mul(back, static_cast<float>(shape[from].y)), Mul(static_cast<float>(shape[to].y), along)),
	};
	if (group.rotation != 0.0f)
	{
		point = RotatePointByAngle(point, group.rotation);
	}
	return point;
}

MapCoords dance_moves::SlotPosition(const Dance& dance, std::size_t index, std::size_t slot,
                                    const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& shapes)
{
	const auto& all = dance.groups.all;
	if (index >= all.size())
	{
		return dance.place;
	}
	const auto shapeOf = [&shapes](const DanceGroup& group) -> const dance_shapes::Shape& {
		return shapes.at(std::min<std::size_t>(group.shape, shapes.size() - 1));
	};
	const auto& group = all[index];
	const auto dancers = static_cast<uint32_t>(group.dancers.size());
	const uint32_t places =
	    group.formation != 0 ? group.inFormation : (group.limited ? std::max(group.quota, dancers) : dancers);
	if (places == 0)
	{
		// The game divides by nothing here; no dance in its files does it
		return dance.place;
	}
	const auto point = ShapePoint(group, shapeOf(group), RoundTheShape(static_cast<uint32_t>(slot), places, group.spin));
	glm::ivec2 local {ToFixed(Add(Mul(ShapeShare(point.x), group.radius), group.offset.x)),
	                  ToFixed(Add(Mul(ShapeShare(point.y), group.radius), group.offset.y))};
	// Then about each group it is part of, as the next of its parts
	const DanceGroup* child = &group;
	for (auto parentIndex = group.parent; parentIndex.has_value() && *parentIndex < all.size();
	     parentIndex = all[*parentIndex].parent)
	{
		const auto& parent = all[*parentIndex];
		const auto parts = static_cast<uint32_t>(parent.children.size() & 0xFF);
		const auto at = ShapePoint(parent, shapeOf(parent), RoundTheShape(child->indexInParent, parts, parent.spin));
		local = {ToFixed(Add(ToMetres(local.x), Add(Mul(ShapeShare(at.x), parent.radius), parent.offset.x))),
		         ToFixed(Add(ToMetres(local.y), Add(Mul(ShapeShare(at.y), parent.radius), parent.offset.y)))};
		child = &parent;
	}
	// Turned the way the dance faces, through the game's sine table even when it faces 0
	const auto turned = RotatePointByAngle({ToMetres(local.x), ToMetres(local.y)}, dance.angle);
	local = {ToFixed(turned.x), ToFixed(turned.y)};
	MapCoords position = dance.place;
	position.x = ToFixed(Add(ToMetres(local.x), ToMetres(dance.place.x)));
	position.z = ToFixed(Add(ToMetres(local.y), ToMetres(dance.place.z)));
	return position;
}

MapCoords dance_moves::GroupCentre(const Dance& dance, std::size_t index)
{
	MapCoords centre = dance.place;
	const auto& all = dance.groups.all;
	const auto whole = [](float metres) {
		return FtoL(Mul(Div(static_cast<float>(FtoL(metres)), k_TenMetres), k_FixedPerTenMetres));
	};
	for (std::optional<std::size_t> at = index; at.has_value() && *at < all.size(); at = all[*at].parent)
	{
		centre.x += whole(all[*at].offset.x);
		centre.z += whole(all[*at].offset.y);
	}
	return centre;
}

std::optional<uint16_t> dance_moves::Facing(const FacingView& view)
{
	switch (view.action)
	{
	case Action::TurnOnTheSpot:
		return static_cast<uint16_t>((view.facing + k_TurnStep) & gutils::k_GameAngleMask);
	case Action::FaceCentre:
		return view.towardsCentre;
	case Action::FaceAway:
		return static_cast<uint16_t>(FtoL(HalfTurnOn(static_cast<float>(view.towardsCentre))));
	case Action::Turn:
		// Only once at its place
		if (view.distance != 0.0f)
		{
			return std::nullopt;
		}
		return static_cast<uint16_t>(FtoL(HalfTurnOn(view.rotation)));
	case Action::FaceDance:
	{
		float angle = Add(view.danceAngle, view.second);
		if (angle > k_TwoPi)
		{
			angle = Sub(angle, k_TwoPi);
		}
		return static_cast<uint16_t>(gutils::ConvertAngle3DToGame(angle));
	}
	default:
		return std::nullopt;
	}
}

int32_t dance_moves::DanceClip(const ClipView& view, const std::function<uint32_t(uint32_t)>& random)
{
	constexpr int32_t k_StoppedClip = 0x171;
	constexpr int32_t k_PrayClip = 0x157;
	constexpr int32_t k_SidewaysClip = 0xF1;
	constexpr int32_t k_WomenDances = 0x109;
	constexpr int32_t k_MenDances = 0x13A;
	constexpr uint32_t k_DanceChoices = 3;
	if (!view.inGroup)
	{
		return k_StandClip;
	}
	if (view.stopped)
	{
		return k_StoppedClip;
	}
	switch (view.action)
	{
	case 0:
		return view.parentAction == Action::Spin ? view.walkClip : k_StandClip;
	case 1:
		return view.rate != 0.0f ? view.walkClip : k_StandClip;
	case 4:
		return static_cast<int32_t>(random(k_DanceChoices)) + (view.female ? k_WomenDances : k_MenDances);
	case 5:
	case 18:
	case 22:
		return k_PrayClip;
	case 6:
		return k_SidewaysClip + (view.female ? 1 : 0);
	case 9:
		// TODO(opening): the game asks the villager something first (and stands it when the answer is no)
		return k_SidewaysClip + (view.female ? 1 : 0);
	case 10:
		// TODO(opening): a number past the table reads on past it in the game
		return view.first < k_ClipTable.size() ? k_ClipTable.at(view.first).at(view.female ? 1 : 0) : k_StandClip;
	case 11:
	case 12:
		return view.walkClip;
	case 15:
		return 0xE5;
	case 16:
		return 0xEA;
	case 17:
		return 0x136;
	case 19:
		return 0x139;
	case 20:
		return 0x18C;
	case 21:
		return 0x18B;
	case 23:
		return 0xF1;
	case 24:
		return 0xF2;
	case 25:
	case 26:
	case 27:
		return 0x109 + static_cast<int32_t>(view.action) - 25;
	case 28:
	case 29:
	case 30:
		return 0x13A + static_cast<int32_t>(view.action) - 28;
	default:
		return k_StandClip;
	}
}
