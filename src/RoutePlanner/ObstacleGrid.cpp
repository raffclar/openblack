/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObstacleGrid.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Common/TruncateToInt.h"
#include "ECS/Systems/RoutePlanStateSystemInterface.h"
#include "Locator.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack::route_planner;

namespace
{
/// The callbacks InstallCallbacks installs, and the search's obstacle hook and its context
/// (Locator::routePlanStateSystem)
openblack::ecs::systems::RoutePlanStateSystemInterface::Callbacks& State()
{
	if (!openblack::Locator::routePlanStateSystem::has_value())
	{
		std::fputs("rplan::RPHolder: no route plan state in the locator (Locator::routePlanStateSystem)\n", stderr);
		std::abort();
	}
	return openblack::Locator::routePlanStateSystem::value().GetCallbacks();
}

constexpr float k_Epsilon = 0.001f;
constexpr float k_TinyNormSq = 0.0001f;
constexpr float k_CutShrink = 0.005f;
constexpr float k_EdgeShrink = 0.002f;
constexpr float k_Concentric = 1e-6f;

int32_t SquareOf(float v)
{
	return openblack::TruncateToInt(v / ObstacleGrid::k_SquareSize);
}
} // namespace

bool RouteObstacle::ContainsCircle(const Point2D& p, float radius) const
{
	// k = (r - radius) - 0.001; below 0 -> 0
	const float dx = p.x - c.x;
	const float dz = p.z - c.z;
	const float k = (r - radius) - k_Epsilon;
	if (k < 0.0f)
	{
		return false;
	}
	// k^2 > dz^2 + dx^2
	return k * k > dz * dz + dx * dx;
}

void ObstacleGrid::InstallCallbacks(FillSquareFn fill, SpecialObjectsFn special)
{
	auto& state = State();
	state.fillSquare = fill;
	state.specialObjects = special;
}

void ObstacleGrid::SetObstacleHook(ObstacleHook hook, void* context)
{
	auto& state = State();
	state.obstacleHook = hook;
	state.obstacleContext = context;
}

void ObstacleGrid::NotifyObstacle(int32_t objectId)
{
	const auto& state = State();
	if (state.obstacleHook != nullptr && objectId != -1)
	{
		state.obstacleHook(state.obstacleContext, objectId);
	}
}

bool ObstacleGrid::HasObstacleHook()
{
	return State().obstacleHook != nullptr;
}

ObstacleGrid::ObstacleGrid()
{
	_plan = nullptr;
	Empty();
}

void ObstacleGrid::Empty()
{
	_start = nullptr;
	_dest = nullptr;
	_objectCount = 0;
	_margin = 0.0f;
	_chainCount = 0;
	for (auto& square : _squares)
	{
		square.head = k_End;
		square.filled = 0;
	}
}

void ObstacleGrid::FillSquare(int32_t squareX, int32_t squareZ)
{
	SquareAt(squareX, squareZ).filled = 1;
	// z outer (sz x 8 + i), x inner (sx x 8 + j)
	for (int32_t i = 0; i < 8; ++i)
	{
		for (int32_t j = 0; j < 8; ++j)
		{
			if (const auto fill = State().fillSquare; fill != nullptr)
			{
				fill(squareX * 8 + j, squareZ * 8 + i, *this);
			}
		}
	}
}

bool ObstacleGrid::IsNotInSquare(int32_t id, int32_t cellX, int32_t cellZ) const
{
	const auto& square = _squares.at(static_cast<size_t>((cellZ >> 3) * k_Squares + (cellX >> 3)));
	for (int16_t at = square.head; at != k_End; at = _chains.at(static_cast<size_t>(at)).next)
	{
		if (_objects.at(static_cast<size_t>(_chains.at(static_cast<size_t>(at)).object)).id == id)
		{
			return false;
		}
	}
	return true;
}

void ObstacleGrid::Link(const RouteObstacle& entry, int32_t index)
{
	// The box's squares, z outer, x inner
	const int32_t x0 = SquareOf(entry.c.x - entry.r);
	const int32_t x1 = SquareOf(entry.r + entry.c.x);
	const int32_t z0 = SquareOf(entry.c.z - entry.r);
	const int32_t z1 = SquareOf(entry.r + entry.c.z);
	for (int32_t z = z0; z <= z1; ++z)
	{
		for (int32_t x = x0; x <= x1; ++x)
		{
			auto& square = SquareAt(x, z);
			const int16_t oldHead = square.head;
			square.head = static_cast<int16_t>(_chainCount);
			if (_chainCount < k_MaxChains)
			{
				_chains.at(static_cast<size_t>(_chainCount)) = {static_cast<int16_t>(index), oldHead};
				++_chainCount;
			}
		}
	}
}

void ObstacleGrid::AddObject(int32_t id, const Point2D& c, float r, int32_t notify)
{
	// The plan's dest, then its start, strictly inside the circle -> not added
	for (const auto* end : {_dest, _start})
	{
		if (end == nullptr)
		{
			continue;
		}
		const float dx = end->x - c.x;
		const float dz = end->z - c.z;
		if (r * r > dz * dz + dx * dx)
		{
			return;
		}
	}
	if (_objectCount >= k_MaxObjects)
	{
		return;
	}
	// With a follower, an obstacle within 0.1 of the follower's position is not added; else its radius is at most that
	// distance (the follower stays outside it). The argument is overwritten: every use below sees the capped radius
	if (_plan != nullptr)
	{
		const float d = _plan->GetPosition().DistanceTo(c);
		if (d < 0.1f)
		{
			return;
		}
		if (r > d)
		{
			r = d;
		}
	}
	// The squares it covers, all inside the grid and room for their chain entries
	const int32_t x0 = SquareOf(c.x - r);
	const int32_t x1 = SquareOf(r + c.x);
	const int32_t z0 = SquareOf(c.z - r);
	const int32_t z1 = SquareOf(r + c.z);
	if ((z1 - z0 + 1) * (x1 - x0 + 1) + _chainCount > k_MaxChains)
	{
		return;
	}
	if (x0 < 0 || x1 >= k_Squares || z0 < 0 || z1 >= k_Squares)
	{
		return;
	}
	// Against every entry already in those squares
	for (int32_t z = z0; z <= z1; ++z)
	{
		for (int32_t x = x0; x <= x1; ++x)
		{
			for (int16_t at = SquareAt(x, z).head; at != k_End; at = _chains.at(static_cast<size_t>(at)).next)
			{
				auto& other = _objects.at(static_cast<size_t>(_chains.at(static_cast<size_t>(at)).object));
				// both anonymous at the same centre (exact x and z) -> not added
				if (other.id == -1 && id == -1 && other.c.x == c.x && other.c.z == c.z)
				{
					return;
				}
				const float d = other.c.DistanceTo(c);
				// inside the existing one -> not added
				if ((d + r) - k_Epsilon < other.r)
				{
					return;
				}
				// the existing one inside the new -> no longer active
				if ((d + other.r) - k_Epsilon < r)
				{
					other.active = 0;
				}
			}
		}
	}
	// The entry, its chain entries, the count
	auto& entry = _objects.at(static_cast<size_t>(_objectCount));
	entry = {id, 1, c, r};
	Link(entry, _objectCount);
	++_objectCount;
	// notify == 1 -> the follower's OnObjectAdded(entry). (openblack, guard) _plan: the original calls it with no null
	// check
	if (notify == 1 && _plan != nullptr)
	{
		_plan->OnObjectAdded(entry);
	}
}

int32_t ObstacleGrid::StartObstacleSidePoint(int32_t obj, const Point2D& p, Point2D& out, int32_t side) const
{
	const auto& o = Object(obj);
	const Point2D d {o.c.x - p.x, o.c.z - p.z};
	const float n2 = d.LengthSquared();
	// p on the centre -> the circle's top
	if (n2 < k_TinyNormSq)
	{
		out.x = o.c.x;
		out.z = o.r + o.c.z;
		return 0;
	}
	const float r2 = o.r * o.r;
	// p inside or on the circle (r^2 + 0.001 > n2) -> pushed out to it
	if (!(r2 + k_Epsilon <= n2))
	{
		Point2D v {p.x - o.c.x, p.z - o.c.z};
		v.SetSize(o.r);
		out.x = v.x + o.c.x;
		out.z = v.z + o.c.z;
		return 0;
	}
	// The tangent points
	float t = r2 / (n2 - r2);
	if (!(t > 0.0f))
	{
		t = 0.0f;
	}
	const float k = 1.0f / (t + 1.0f);
	const float m = std::sqrt(t) * k;
	if (side == 2)
	{
		out.x = (d.x * k + p.x) - d.z * m;
		out.z = (d.x * m + d.z * k) + p.z;
	}
	else
	{
		out.x = (d.z * m + d.x * k) + p.x;
		out.z = (p.z - d.x * m) + d.z * k;
	}
	return 1;
}

int32_t ObstacleGrid::GetTangent(int32_t objA, int32_t sideA, Point2D& outA, int32_t objB, int32_t sideB, Point2D& outB) const
{
	const auto& a = Object(objA);
	const auto& b = Object(objB);
	const Point2D v {b.c.x - a.c.x, b.c.z - a.c.z};
	const Point2D n = sideA == 1 ? Point2D {-v.z, v.x} : Point2D {v.z, -v.x};
	const float rA = a.r;
	const float rB = b.r;
	const float dr = sideB == sideA ? rA - rB : rB + rA;
	const float length = v.GetLength();
	const float c = dr / length;
	const float h = 1.0f - c * c;
	// No tangent
	if (!(h > 0.0f))
	{
		return 0;
	}
	const float s = std::sqrt(h) / length;
	const float snx = s * n.x;
	const float snz = s * n.z;
	const float cl = c / length;
	const float cvx = cl * v.x;
	const float cvz = cl * v.z;
	const Point2D u {cvx + snx, cvz + snz};
	const float uzA = u.z * rA;
	outA.x = u.x * rA + a.c.x;
	outA.z = uzA + a.c.z;
	const float uxB = u.x * rB;
	const float uzB = u.z * rB;
	if (sideB == sideA)
	{
		outB.x = uxB + b.c.x;
		outB.z = uzB + b.c.z;
	}
	else
	{
		outB.x = b.c.x - uxB;
		outB.z = b.c.z - uzB;
	}
	return 1;
}

int32_t ObstacleGrid::GetFirstObject(const Point2D& from, Point2D& to, int32_t ignore, Point2D& hit, int32_t& side, float r)
{
	Point2D v {to.x - from.x, to.z - from.z};
	if (v.z * v.z + v.x * v.x < k_TinyNormSq)
	{
		return -1;
	}
	float length = v.Normalize();
	// Cut r short
	if (r > 0.0f)
	{
		length = length - r;
		if (length < k_Epsilon)
		{
			to = from;
			return -1;
		}
		const float k = length - k_CutShrink;
		to.x = k * v.x + from.x;
		to.z = k * v.z + from.z;
	}
	const Point2D w {-v.z, v.x};
	int32_t best = -1;
	float bestS = 0.0f;
	// The squares and the direction code
	int32_t x = SquareOf(from.x);
	int32_t z = SquareOf(from.z);
	const int32_t x1 = SquareOf(to.x);
	const int32_t z1 = SquareOf(to.z);
	const int32_t dx = x1 - x;
	const int32_t dz = z1 - z;
	int32_t code = 0;
	if (dx == 0)
	{
		code = dz == 0 ? 0 : (dz < 0 ? 4 : 3);
	}
	else if (dx < 0)
	{
		code = dz == 0 ? 2 : (dz < 0 ? 8 : 7);
	}
	else
	{
		code = dz == 0 ? 1 : (dz < 0 ? 6 : 5);
	}
	Point2D cur = from;
	// Square 0 counts as off the grid
	while (x > 0)
	{
		if (x >= k_Squares || z <= 0 || z >= k_Squares)
		{
			break;
		}
		if (SquareAt(x, z).filled == 0)
		{
			FillSquare(x, z);
		}
		for (int16_t at = SquareAt(x, z).head; at != k_End; at = _chains.at(static_cast<size_t>(at)).next)
		{
			const int32_t index = _chains.at(static_cast<size_t>(at)).object;
			const auto& o = _objects.at(static_cast<size_t>(index));
			if (o.active == 0 || index == ignore)
			{
				continue;
			}
			const Point2D f {o.c.x - from.x, o.c.z - from.z};
			const float t = f.z * v.z + f.x * v.x;
			if (!(t > 0.0f))
			{
				continue;
			}
			if (length + o.r <= t)
			{
				continue;
			}
			const float q = w.z * f.z + f.x * w.x;
			const float rp = o.r - k_EdgeShrink;
			if (!(-rp < q) || !(q < rp))
			{
				continue;
			}
			const float b = t * -2.0f;
			const float bb = b * b;
			const float disc = bb - (f.LengthSquared() - o.r * o.r) * 4.0f;
			if (disc < 0.0f)
			{
				continue;
			}
			const float s = (std::sqrt(disc) + b) * -0.5f;
			if (s <= -k_Epsilon)
			{
				continue;
			}
			if (length - k_Epsilon <= s)
			{
				continue;
			}
			if (best != -1 && !(s < bestS))
			{
				continue;
			}
			best = index;
			bestS = s;
			side = q > 0.0f ? 2 : 1;
		}
		// The nearest of the square
		if (best != -1)
		{
			hit.x = bestS * v.x + from.x;
			hit.z = bestS * v.z + from.z;
			return best;
		}
		// The last square
		bool done = false;
		switch (code)
		{
		case 0:
			done = true;
			break;
		case 1:
		case 2:
			done = x == x1;
			break;
		case 3:
		case 4:
			done = z == z1;
			break;
		case 5:
			done = x >= x1 && z >= z1;
			break;
		case 6:
			done = x >= x1 && z <= z1;
			break;
		case 7:
			done = x <= x1 && z >= z1;
			break;
		default:
			done = x <= x1 && z <= z1;
			break;
		}
		if (done)
		{
			break;
		}
		// The next square
		switch (code)
		{
		case 1:
			++x;
			break;
		case 2:
			--x;
			break;
		case 3:
			++z;
			break;
		case 4:
			--z;
			break;
		default:
		{
			const bool xUp = code == 5 || code == 6;
			const bool zUp = code == 5 || code == 7;
			const float sx = static_cast<float>(xUp ? x + 1 : x) * k_SquareSize;
			const float sz = static_cast<float>(zUp ? z + 1 : z) * k_SquareSize;
			const float tx = (sx - cur.x) / v.x;
			const float tz = (sz - cur.z) / v.z;
			if (tx > tz)
			{
				z += zUp ? 1 : -1;
				cur.z = sz;
				cur.x = tz * v.x + cur.x;
			}
			else
			{
				x += xUp ? 1 : -1;
				cur.x = sx;
				cur.z = tx * v.z + cur.z;
			}
			break;
		}
		}
	}
	// Nothing on the way
	hit = from;
	return -1;
}

int32_t ObstacleGrid::ArcBlocked(int32_t obj, int32_t side, Point2D& end, const Point2D& start, int32_t& hit)
{
	const auto& o = Object(obj);
	Point2D e {end.x - o.c.x, end.z - o.c.z};
	const Point2D s {start.x - o.c.x, start.z - o.c.z};
	int32_t result = 0;
	const int32_t x0 = SquareOf(o.c.x - o.r);
	const int32_t x1 = SquareOf(o.r + o.c.x);
	const int32_t z0 = SquareOf(o.c.z - o.r);
	const int32_t z1 = SquareOf(o.r + o.c.z);
	for (int32_t z = z0; z <= z1; ++z)
	{
		for (int32_t x = x0; x <= x1; ++x)
		{
			if (SquareAt(x, z).filled == 0)
			{
				FillSquare(x, z);
			}
			for (int16_t at = SquareAt(x, z).head; at != k_End; at = _chains.at(static_cast<size_t>(at)).next)
			{
				const int32_t index = _chains.at(static_cast<size_t>(at)).object;
				const auto& o2 = _objects.at(static_cast<size_t>(index));
				if (&o2 == &o || o2.active == 0)
				{
					continue;
				}
				// Apart -> skip; concentric -> 0
				const float sumR = o.r + o2.r;
				const Point2D dd {o2.c.x - o.c.x, o2.c.z - o.c.z};
				const float d2 = dd.LengthSquared();
				if (sumR * sumR <= d2)
				{
					continue;
				}
				if (d2 < k_Concentric)
				{
					return 0;
				}
				const float a = dd.z * s.x - dd.x * s.z;
				const float b = e.x * dd.z - e.z * dd.x;
				const float c = s.x * e.z + (-s.z) * e.x;
				bool cut = false;
				if (side == 1)
				{
					if (c < 0.0f)
					{
						cut = !(a > 0.0f) && !(b < 0.0f);
					}
					else
					{
						cut = !(a > 0.0f) || !(b < 0.0f);
					}
				}
				else
				{
					if (c > 0.0f)
					{
						cut = !(a < 0.0f) && b <= 0.0f;
					}
					else
					{
						cut = !(a < 0.0f) || b <= 0.0f;
					}
				}
				if (!cut)
				{
					// the check: end inside o2
					const float ex = o2.c.x - end.x;
					const float ez = o2.c.z - end.z;
					if (o2.r * o2.r <= ez * ez + ex * ex)
					{
						continue;
					}
				}
				// Where the circles cross on the arc's side
				const float rA2 = o.r * o.r;
				const float rB2 = o2.r * o2.r;
				const float d = std::sqrt(d2);
				const float diff = rB2 - rA2;
				const float k = (diff - d2) / (d * o.r + d * o.r);
				const float h2 = (1.0f - k * k) * rA2;
				if (h2 <= 0.0f)
				{
					return 0;
				}
				const float h = std::sqrt(h2);
				float along = std::sqrt(rA2 - h2);
				if (d2 < diff)
				{
					along = -along;
				}
				const float hd = h / d;
				const float px = hd * -dd.z;
				const float pz = dd.x * hd;
				const float ad = along / d;
				const float ax = ad * dd.x;
				const float az = ad * dd.z;
				const Point2D p = side == 1 ? Point2D {ax + px, az + pz} : Point2D {ax - px, az - pz};
				e = p;
				end.x = p.x + o.c.x;
				end.z = p.z + o.c.z;
				hit = index;
				result = 1;
			}
		}
	}
	return result;
}
