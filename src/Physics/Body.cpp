/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Body.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Ground.h"

using namespace openblack::physics;

namespace
{
/// Points this close to the sea's level or the land's are in it
constexpr float k_SeaLevelTolerance = 0.0001f;
/// A floating body soaks up this much density each step it is in the sea
constexpr float k_SoakPerStep = 1.0f / 15000.0f;
/// The sea slows a body this many times more than the air
constexpr float k_WaterDragFactor = 100.0f;
/// The share of the sea's push each point under the surface turns the body by
constexpr float k_WaterTurnShare = 0.02f;
/// How much the damping of a deepening contact counts
constexpr float k_DampingFactor = 200.0f;
/// Two bodies slide off each other this much more easily than off the land
constexpr float k_BodyFrictionFactor = 0.3f;
constexpr float k_DragRadiusFactor = 0.3f;
/// A body falling this many radii under the sea's level is gone
constexpr float k_DeleteDepthRadii = -4.0f;
/// The rest test: the speed limit at first, once resting, and how it loosens after a long time in the physics
constexpr float k_RestThreshold = 1.0f;
constexpr float k_RestingThreshold = 4.0f;
constexpr int k_RestCounterLoosens = 15000;
constexpr float k_RestLoosening = 1.0f / 15000.0f;
constexpr int k_RestCounterPerStep = 5;
constexpr float k_MinRotationStep = 1e-5f;
constexpr float k_AnyFaceStart = -10000.0f;
/// The overlap test takes a face that faces the line more than this, a double
constexpr double k_AnyFacingThreshold = -0.0001;
constexpr float k_MinAheadLength = 0.001f;
/// The spin kept each step is the share kept each second raised to the step's length
constexpr double k_StepExponent = 0.005000000074505806;
/// The cosine of a quarter turn as the game stores it, which is not quite zero

glm::vec3 NormaliseOrKeep(glm::vec3 v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	// The game adds the squares from z to x and multiplies by the inverse of the length
	return v * (1.0f / openblack::x87::LengthZyx(v));
}

/// The axes times a vector, as the game adds the parts of each row: z first, then y, then x
glm::vec3 TurnedZyx(const glm::mat3& axes, glm::vec3 v)
{
	return axes[2] * v.z + axes[1] * v.y + axes[0] * v.x;
}

/// The axes times a vector, as the game adds the parts of each row in each step: y first, then x, then z
glm::vec3 TurnedYxz(const glm::mat3& axes, glm::vec3 v)
{
	return (axes[1] * v.y + axes[0] * v.x) + axes[2] * v.z;
}

glm::mat3 NormaliseAxes(glm::mat3 axes)
{
	for (int i = 0; i < 3; ++i)
	{
		axes[i] = NormaliseOrKeep(axes[i]);
	}
	return axes;
}

/// The inverse of a matrix, its determinant kept at least a tiny size so that a body whose points lie in a line, or a
/// body with none, still gets an inverse (a huge or zero one) as the game gives it
glm::mat3 ClampedInverse(const glm::mat3& matrix)
{
	constexpr float k_SmallestDeterminant = 1e-10f;
	float determinant = glm::determinant(matrix);
	if (std::abs(determinant) < k_SmallestDeterminant)
	{
		determinant = determinant < 0.0f ? -k_SmallestDeterminant : k_SmallestDeterminant;
	}
	const auto& m = matrix;
	glm::mat3 adjugate;
	adjugate[0][0] = m[1][1] * m[2][2] - m[2][1] * m[1][2];
	adjugate[1][0] = -(m[1][0] * m[2][2] - m[2][0] * m[1][2]);
	adjugate[2][0] = m[1][0] * m[2][1] - m[2][0] * m[1][1];
	adjugate[0][1] = -(m[0][1] * m[2][2] - m[2][1] * m[0][2]);
	adjugate[1][1] = m[0][0] * m[2][2] - m[2][0] * m[0][2];
	adjugate[2][1] = -(m[0][0] * m[2][1] - m[2][0] * m[0][1]);
	adjugate[0][2] = m[0][1] * m[1][2] - m[1][1] * m[0][2];
	adjugate[1][2] = -(m[0][0] * m[1][2] - m[1][0] * m[0][2]);
	adjugate[2][2] = m[0][0] * m[1][1] - m[1][0] * m[0][1];
	return adjugate / determinant;
}

glm::vec2 Xz(glm::vec3 v)
{
	return {v.x, v.z};
}

/// A point through a part's matrix, as the game adds the terms of each part. Into the part's space the terms of x add
/// z, y, then x, and the terms of y and z add z, x, then y. Back into the world all three add z, y, then x. A direction
/// takes no move.
glm::vec3 IntoPart(const glm::mat4& m, const glm::vec3& v, float w)
{
	return {(((m[2][0] * v.z) + (m[1][0] * v.y)) + (v.x * m[0][0])) + (m[3][0] * w),
	        (((m[2][1] * v.z) + (m[0][1] * v.x)) + (m[1][1] * v.y)) + (m[3][1] * w),
	        (((m[2][2] * v.z) + (m[0][2] * v.x)) + (m[1][2] * v.y)) + (m[3][2] * w)};
}

glm::vec3 OutOfPart(const glm::mat4& m, const glm::vec3& v, float w)
{
	return {(((v.z * m[2][0]) + (v.y * m[1][0])) + (v.x * m[0][0])) + (m[3][0] * w),
	        (((v.z * m[2][1]) + (v.y * m[1][1])) + (v.x * m[0][1])) + (m[3][1] * w),
	        (((v.z * m[2][2]) + (v.y * m[1][2])) + (v.x * m[0][2])) + (m[3][2] * w)};
}

bool InsideTriangle(glm::vec3 p, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 normal)
{
	// The game adds each dot product from z to x
	using openblack::x87::DotZyx;
	return DotZyx(glm::cross(v1 - v0, p - v0), normal) > 0.0f && DotZyx(glm::cross(v2 - v1, p - v1), normal) > 0.0f &&
	       DotZyx(glm::cross(v0 - v2, p - v2), normal) > 0.0f;
}
} // namespace

Body::Body(const BodySetup& setup, const Shape& shape)
    : density(setup.material.density)
    , _scale(setup.scale)
    , _centreOfMass(shape.centreOfMass)
    , _mass(setup.mass)
    , _stiffness(setup.mass * setup.material.springK)
    , _damping(setup.mass * setup.material.dampK)
    , _friction(setup.material.friction)
    , _spinKeptPerStep(openblack::x87::Pow(setup.material.spinKeptPerSecond, k_StepExponent))
    , _drag(setup.material.drag)
    , _radius(shape.radius)
    , _dynamic(setup.dynamic)
    , _restCounter(-static_cast<int>(std::lrint(setup.scale * setup.halfHeight * 1000.0f)))
{
	_points.reserve(shape.points.size());
	for (const auto& local : shape.points)
	{
		_points.push_back(Point {.local = local});
	}
	_faces.reserve(shape.faces.size());
	for (const auto& indices : shape.faces)
	{
		_faces.push_back(Face {.indices = indices});
	}
	SetUpInertia();
	_drag *= shape.dragFactor;
}

void Body::SetUpInertia()
{
	externalForce = glm::vec3(0.0f);
	externalTorque = glm::vec3(0.0f);
	for (auto& point : _points)
	{
		point.length = openblack::x87::LengthZyx(point.local);
		point.aheadLength = point.length;
	}
	if (_dynamic)
	{
		auto& inertia = _inertia;
		inertia = {};
		for (const auto& point : _points)
		{
			const float pointMass = _mass / static_cast<float>(_points.size());
			const auto [x, y, z] = std::array {point.local.x, point.local.y, point.local.z};
			inertia[0][0] += (y * y + z * z) * pointMass;
			inertia[1][1] += (x * x + z * z) * pointMass;
			inertia[2][2] += (x * x + y * y) * pointMass;
			inertia[0][1] -= x * y * pointMass;
			inertia[1][0] -= x * y * pointMass;
			inertia[0][2] -= x * z * pointMass;
			inertia[2][0] -= x * z * pointMass;
			inertia[2][1] -= y * z * pointMass;
			// The game sums x z here where y z belongs, which makes lopsided bodies tumble its own way
			inertia[1][2] -= x * z * pointMass;
		}
		glm::mat3 matrix;
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				matrix[column][row] = inertia[row][column];
			}
		}
		const auto inverse = ClampedInverse(matrix);
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				_inverseInertia[row][column] = inverse[column][row];
			}
		}
		_drag = _radius * _drag * _radius * k_DragRadiusFactor;
	}
	else
	{
		_inertia = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
		_inverseInertia = _inertia;
	}
	inWater = false;
	touched = false;
	justSetUp = true;
}

void Body::SetUpPose(const Pose& objectMatrix)
{
	// The game adds the parts of the centre of mass in the order y, z, x
	const auto& scaled = objectMatrix.axes;
	_centre = objectMatrix.origin + (scaled[1] * _centreOfMass.y + scaled[2] * _centreOfMass.z + scaled[0] * _centreOfMass.x);
	_axes = NormaliseAxes(objectMatrix.axes);
	_turnStartAxes = _axes;
	_turnStartCentre = _centre;
	_faceBlocksStale = true;
	for (auto& point : _points)
	{
		point.world = TurnedZyx(_axes, point.local) + _centre;
		point.contact = point.world;
		point.firstDepth = 0.0f;
		point.depth = 0.0f;
	}
	_bones.clear();
	_contacts = 0;
	for (auto& face : _faces)
	{
		const auto& a = _points[face.indices[0]].local;
		const auto& b = _points[face.indices[1]].local;
		const auto& c = _points[face.indices[2]].local;
		face.localNormal = NormaliseOrKeep(glm::cross(b - a, c - a));
		// The game adds the parts of each row of the turned normal in its own order: y, z, x for the first, y, x, z for
		// the others
		const auto& n = face.localNormal;
		face.worldNormal = {(_axes[1].x * n.y + _axes[2].x * n.z) + _axes[0].x * n.x,
		                    (_axes[1].y * n.y + _axes[0].y * n.x) + _axes[2].y * n.z,
		                    (_axes[1].z * n.y + _axes[0].z * n.x) + _axes[2].z * n.z};
	}
}

void Body::SetPoseDirect(const glm::mat3& axes, glm::vec3 centre)
{
	_axes = axes;
	_centre = centre;
	_turnStartAxes = _axes;
	_turnStartCentre = _centre;
	_faceBlocksStale = true;
	for (auto& point : _points)
	{
		point.world = _axes * point.local + _centre;
		point.contact = point.world;
		point.firstDepth = 0.0f;
		point.depth = 0.0f;
	}
	for (auto& face : _faces)
	{
		const auto& a = _points[face.indices[0]].local;
		const auto& b = _points[face.indices[1]].local;
		const auto& c = _points[face.indices[2]].local;
		face.localNormal = NormaliseOrKeep(glm::cross(b - a, c - a));
		face.worldNormal = _axes * face.localNormal;
	}
}

void Body::PlacePoints(glm::vec3 centre, std::span<const glm::vec3> worldPoints)
{
	_centre = centre;
	_faceBlocksStale = true;
	_turnStartAxes = _axes;
	_turnStartCentre = _centre;
	for (size_t i = 0; i < _points.size() && i < worldPoints.size(); ++i)
	{
		auto& point = _points[i];
		point.world = worldPoints[i];
		point.contact = worldPoints[i];
		point.firstDepth = 0.0f;
		point.depth = 0.0f;
	}
}

void Body::SetSkeleton(std::vector<Ellipsoid> bones)
{
	_bones = std::move(bones);
}

void Body::SettleOnLand(const Ground& ground, bool noPullDown, bool alignToSlope)
{
	if (alignToSlope)
	{
		const auto normal = ground.NormalAt(Xz(_centre));
		const auto forward = NormaliseOrKeep(glm::cross(_axes[0], normal));
		_axes = glm::mat3(glm::cross(normal, forward), normal, forward);
	}
	float lowest = 1000.0f;
	for (const auto& point : _points)
	{
		const auto world = _axes * point.local + _centre;
		lowest = std::min(lowest, world.y - ground.HeightAt(Xz(world)));
	}
	if (noPullDown && lowest > 0.0f)
	{
		lowest = 0.0f;
	}
	_centre.y -= lowest;
	_turnStartAxes = _axes;
	_turnStartCentre = _centre;
}

void Body::ClearForces()
{
	touched = false;
	if (resting)
	{
		_aheadCentre = _centre;
		_force = glm::vec3(0.0f);
		_torque = glm::vec3(0.0f);
		return;
	}
	const auto ahead = velocity * k_LookAheadSeconds;
	_faceBlocksStale = true;
	for (auto& point : _points)
	{
		point.world = TurnedYxz(_axes, point.local) + _centre + ahead;
		point.aheadLength = std::max(openblack::x87::LengthZyx(point.world - _centre), k_MinAheadLength);
	}
	_aheadCentre = _centre + ahead;
	for (auto& face : _faces)
	{
		// The game adds the first part of the turned normal from z to x, and the others from x to z
		const auto& n = face.localNormal;
		face.worldNormal = _axes * n;
		face.worldNormal.x = (_axes[2].x * n.z + _axes[1].x * n.y) + _axes[0].x * n.x;
	}
	_force = externalForce;
	_torque = externalTorque;
}

void Body::TouchGround(const Ground& ground)
{
	if (resting)
	{
		if (_bones.empty() || !_faces.empty())
		{
			for (auto& point : _points)
			{
				point.contact = point.world;
				point.depth = 0.0f;
			}
		}
		else
		{
			// A resting skeleton's points all stand at its first part's origin
			const auto origin = glm::vec3(_bones.front().localToWorld[3]);
			_faceBlocksStale = true;
			for (auto& point : _points)
			{
				point.world = origin;
				point.contact = origin;
				point.length = 1.0f;
				point.aheadLength = 1.0f;
				point.depth = 0.0f;
			}
		}
		return;
	}

	_force -= velocity * (_speed * _drag);
	_contacts = 0;
	_force.y -= _mass * k_Gravity;

	const bool overSea = ground.HeightAt(Xz(_centre)) < k_SeaLevelTolerance && _centre.y < _radius && !_points.empty() &&
	                     ground.IsSeaCell(Xz(_points.front().world));
	if (overSea)
	{
		const auto submerged = std::ranges::count_if(_points, [](const Point& point) { return point.world.y < 0.0f; });
		if (submerged > 0)
		{
			inWater = true;
			touched = true;
			density += k_SoakPerStep;
			const float share = std::min((_radius - _centre.y) / (_radius + _radius), 1.0f);
			const float drag = share * _speed * _drag * k_WaterDragFactor;
			const glm::vec3 water(-drag * velocity.x, share * _mass * k_Gravity / density - drag * velocity.y,
			                      -drag * velocity.z);
			_force += water;
			const auto turn = water * (k_WaterTurnShare / static_cast<float>(submerged));
			for (auto& point : _points)
			{
				point.contact = point.world;
				point.depth = 0.0f;
				if (point.world.y < 0.0f)
				{
					_torque += glm::cross(point.world - _aheadCentre, turn);
				}
			}
			// While any point is in the sea the body never touches the sea floor
			return;
		}
		inWater = false;
	}

	for (auto& point : _points)
	{
		const float height = ground.HeightAt(Xz(point.world));
		point.depth = height - point.world.y;
		point.other = nullptr;
		point.contact = point.world;
		if (point.depth >= 0.0f)
		{
			++_contacts;
			point.normal = ground.NormalAt(Xz(point.world));
			point.contact.y = height;
			touched = true;
		}
	}
}

std::span<const face_search::FaceBlock> Body::FaceBlocks() const
{
	if (_faceBlocksStale)
	{
		_faceBlocks.resize(face_search::BlocksFor(_faces.size()));
		// An empty place in the last block keeps a zero normal, so it never faces a line
		if (!_faceBlocks.empty())
		{
			_faceBlocks.back() = {};
		}
		for (size_t i = 0; i < _faces.size(); ++i)
		{
			const auto& face = _faces[i];
			face_search::Store(_faceBlocks, i,
			                   {.normal = face.worldNormal,
			                    .a = _points[face.indices[0]].world,
			                    .b = _points[face.indices[1]].world,
			                    .c = _points[face.indices[2]].world});
		}
		_faceBlocksStale = false;
	}
	return _faceBlocks;
}

std::optional<RayHit> Body::FaceBehind(glm::vec3 point, glm::vec3 direction) const
{
	const auto hit = face_search::NearestBehind(FaceBlocks(), point, direction);
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return RayHit {.point = hit->point, .normal = _faces[hit->face].worldNormal};
}

std::optional<glm::vec3> Body::AnyFaceBehind(glm::vec3 point, glm::vec3 direction) const
{
	std::optional<glm::vec3> found;
	float best = k_AnyFaceStart;
	for (const auto& face : _faces)
	{
		const auto& v0 = _points[face.indices[0]].world;
		const auto& v1 = _points[face.indices[1]].world;
		const auto& v2 = _points[face.indices[2]].world;
		const auto normal = glm::cross(v1 - v0, v2 - v0);
		const float facing = openblack::x87::Dot<openblack::x87::SumOrder::Xzy>(normal, direction);
		// This test compares with a double
		if (!openblack::x87::Less(facing, k_AnyFacingThreshold))
		{
			continue;
		}
		const float t = -openblack::x87::DotZyx(point - v0, normal) / facing;
		if (!(t < 0.0f) || !(t > best))
		{
			continue;
		}
		const auto hit = point + t * direction;
		if (!InsideTriangle(hit, v0, v1, v2, normal))
		{
			continue;
		}
		best = t;
		found = hit;
	}
	return found;
}

std::optional<std::pair<RayHit, float>> openblack::physics::EllipsoidHit(std::span<const Ellipsoid> bones, glm::vec3 point,
                                                                         glm::vec3 direction)
{
	constexpr float k_Tiny = 0.0001f;
	std::optional<std::pair<RayHit, float>> best;
	for (const auto& bone : bones)
	{
		const auto half = (bone.boxMax - bone.boxMin) * 0.5f;
		if (!(half.x > k_Tiny && half.y > k_Tiny && half.z > k_Tiny))
		{
			continue;
		}
		// Into the part's own space, where its box is the unit sphere about the box's centre
		const auto localPoint = IntoPart(bone.worldToLocal, point, 1.0f);
		const auto start = (localPoint - bone.boxMin) / half - 1.0f;
		const auto along = IntoPart(bone.worldToLocal, direction, 0.0f) / half;
		// The game adds the squares and the products from z to x
		const float a = openblack::x87::DotZyx(along, along);
		if (!(a > k_Tiny))
		{
			continue;
		}
		const float b = 2.0f * openblack::x87::DotZyx(along, start);
		const float discriminant = b * b - (openblack::x87::DotZyx(start, start) - 1.0f) * a * 4.0f;
		if (!(discriminant >= 0.0f))
		{
			continue;
		}
		const float t = (-b - std::sqrt(discriminant)) / (a + a);
		if (best.has_value() && !(t < best->second))
		{
			continue;
		}
		const auto onSphere = start + along * t;
		const auto hitLocal = (onSphere + 1.0f) * half + bone.boxMin;
		const auto normalLocal = onSphere * half;
		const auto hitWorld = OutOfPart(bone.localToWorld, hitLocal, 1.0f);
		// The 3D engine scales the normal by its own inverse square root of the squares added from x to z
		const auto turned = OutOfPart(bone.localToWorld, normalLocal, 0.0f);
		const auto normalWorld = turned * openblack::gutils::ModelInverseSquareRoot(
		                                      openblack::x87::Dot<openblack::x87::SumOrder::Xyz>(turned, turned));
		best = std::pair {RayHit {.point = hitWorld, .normal = normalWorld}, t};
	}
	return best;
}

std::optional<RayHit> Body::SkeletonHit(glm::vec3 point, glm::vec3 direction) const
{
	if (auto hit = EllipsoidHit(_bones, point, direction))
	{
		return hit->first;
	}
	return std::nullopt;
}

void Body::TouchFaces(Body& other)
{
	const float reach2 = other._radius * other._radius;
	for (auto& point : _points)
	{
		const auto offset = point.world - other._centre;
		if (!(openblack::x87::DotZyx(offset, offset) < reach2))
		{
			continue;
		}
		const auto direction = _bones.empty() ? point.world - _centre : NormaliseOrKeep(other._centre - point.world);
		std::optional<RayHit> hit;
		if (other._bones.empty())
		{
			hit = other.FaceBehind(point.world, direction);
		}
		else
		{
			hit = other.SkeletonHit(point.world, direction);
			if (hit.has_value())
			{
				// A skeleton is hit only between the body's centre and where the point will be
				const float along = openblack::x87::DotZyx(hit->point - _centre, direction) / point.aheadLength;
				if (along < 0.0f || point.aheadLength < along)
				{
					continue;
				}
			}
		}
		if (!hit.has_value())
		{
			continue;
		}
		++_contacts;
		point.contact = hit->point;
		point.other = &other;
		point.normal = hit->normal;
		const float depth = point.aheadLength - openblack::x87::DotZyx(hit->point - _centre, direction) / point.aheadLength;
		if (point.depth < depth)
		{
			point.depth = depth;
			if (!resting || !other.resting)
			{
				lastHit = &other;
				touched = true;
				other.lastHit = this;
				other.touched = true;
			}
		}
	}
}

void Body::ApplyContacts()
{
	justSetUp = false;
	_restCounter += k_RestCounterPerStep;
	if (_contacts == 0)
	{
		return;
	}
	for (auto& point : _points)
	{
		if (!(0.0f < point.depth))
		{
			point.anchor = point.contact;
			point.firstDepth = 0.0f;
			continue;
		}
		float stiffness = _stiffness;
		float damping = _damping;
		float friction = _friction;
		if (point.other != nullptr)
		{
			stiffness = std::min(_stiffness, point.other->_stiffness);
			damping = std::min(_damping, point.other->_damping);
			friction = std::min(_friction, point.other->_friction) * k_BodyFrictionFactor;
		}
		float push = 0.0f;
		if (point.firstDepth == 0.0f)
		{
			// The first step of a touch pushes by its depth alone
			point.firstDepth = point.depth;
			push = point.depth * stiffness;
		}
		else
		{
			push = stiffness * point.firstDepth +
			       (point.depth - point.firstDepth) * k_DampingFactor * (point.aheadLength / point.length) * damping;
		}
		push = std::max(push, 0.0f);
		const float maxFriction = push * friction;
		point.normal *= push;
		auto hold = (point.anchor - point.contact) * stiffness;
		const float holdSquared = openblack::x87::DotZyx(hold, hold);
		if (maxFriction * maxFriction < holdSquared)
		{
			// It slides: friction holds no harder than its limit, and the anchor follows the point. The game multiplies
			// by one over the stiffness.
			hold *= maxFriction / std::sqrt(holdSquared);
			point.anchor = point.contact + hold * (1.0f / stiffness);
		}
		point.normal += hold;
		const auto total = point.normal;
		_force += total;
		_torque += glm::cross(point.world - _aheadCentre, total);
		if (point.other != nullptr)
		{
			point.other->_force -= total;
			point.other->_torque += glm::cross(point.world - point.other->_centre, -total);
		}
	}
}

glm::vec3 Body::BodyOmega() const
{
	const auto local = glm::transpose(_axes) * angularMomentum;
	glm::vec3 omega(0.0f);
	// The game adds the three products of each part from the last row to the first
	for (int column = 0; column < 3; ++column)
	{
		omega[column] = (local[2] * _inverseInertia[2][column] + local[1] * _inverseInertia[1][column]) +
		                local[0] * _inverseInertia[0][column];
	}
	return omega;
}

glm::vec3 Body::AngularVelocity() const
{
	return _axes * BodyOmega();
}

void Body::SetAngularVelocityInBodyAxes(glm::vec3 omega)
{
	glm::vec3 local(0.0f);
	for (int column = 0; column < 3; ++column)
	{
		for (int row = 0; row < 3; ++row)
		{
			local[column] += omega[row] * _inertia[row][column];
		}
	}
	angularMomentum = _axes * local;
}

void Body::SetAngularVelocity(glm::vec3 omega)
{
	SetAngularVelocityInBodyAxes(glm::transpose(_axes) * omega);
}

StepResult Body::Integrate()
{
	if (!_dynamic || (resting && !touched))
	{
		return StepResult::None;
	}
	if (_centre.y < _radius * k_DeleteDepthRadii)
	{
		return StepResult::Delete;
	}

	angularMomentum += _torque * k_StepSeconds;
	angularMomentum *= _spinKeptPerStep;
	auto omega = BodyOmega();
	const float spin2 = openblack::x87::DotZyx(omega, omega);
	if (k_MaxSpin * k_MaxSpin < spin2 && (omega.x != 0.0f || omega.y != 0.0f || omega.z != 0.0f))
	{
		omega *= k_MaxSpin / std::sqrt(spin2);
	}
	const auto worldOmega = TurnedZyx(_axes, omega);

	velocity += _force * (k_StepSeconds / _mass);
	_speed = openblack::x87::LengthZyx(velocity);
	if (k_MaxSpeed < _speed)
	{
		velocity *= k_MaxSpeed / _speed;
		_speed = k_MaxSpeed;
	}

	float threshold = k_RestThreshold;
	if (resting)
	{
		threshold = k_RestingThreshold;
	}
	else if (k_RestCounterLoosens < _restCounter)
	{
		threshold = static_cast<float>(_restCounter) * k_RestLoosening;
	}
	if (_contacts != 0 && _speed < threshold && spin2 < threshold * threshold * 0.25f)
	{
		// The game multiplies the torque by one over the radius squared times the mass
		const auto turning = _torque * (1.0f / ((_radius * _radius) * _mass));
		if (openblack::x87::DotZyx(turning, turning) < threshold * threshold)
		{
			if (resting)
			{
				velocity = glm::vec3(0.0f);
				_speed = 0.0f;
				angularMomentum = glm::vec3(0.0f);
				return StepResult::None;
			}
			if (0 < _restCounter)
			{
				velocity = glm::vec3(0.0f);
				_speed = 0.0f;
				angularMomentum = glm::vec3(0.0f);
				return StepResult::Stopped;
			}
		}
	}

	const auto step = worldOmega * k_StepSeconds;
	const float angle = openblack::x87::LengthZyx(step);
	if (k_MinRotationStep < angle)
	{
		_axes = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, step / angle)) * _axes;
	}
	_centre += velocity * k_StepSeconds;
	return resting ? StepResult::Knocked : StepResult::Moved;
}

float Body::OverlapDepth(const Body& other, glm::vec3 direction) const
{
	float deepest = 0.0f;
	for (const auto& point : _points)
	{
		std::optional<glm::vec3> hit;
		if (other._bones.empty())
		{
			hit = other.AnyFaceBehind(point.world, direction);
		}
		else if (auto skeletonHit = other.SkeletonHit(point.world, direction))
		{
			hit = skeletonHit->point;
		}
		if (hit.has_value())
		{
			deepest = std::max(deepest, openblack::x87::DotZyx(point.world - *hit, direction));
		}
	}
	return deepest;
}

void Body::Raise(float height)
{
	_centre.y += height;
	SetUpPose(ObjectPose());
}

void Body::BeginTurn()
{
	_turnStartAxes = _axes;
	_turnStartCentre = _centre;
}

Pose Body::ObjectPose() const
{
	const auto scaled = _axes * _scale;
	return {.axes = scaled, .origin = _centre - scaled * _centreOfMass};
}

Pose Body::DrawPose(float turnFraction, bool animated) const
{
	glm::mat3 axes;
	for (int i = 0; i < 3; ++i)
	{
		axes[i] = NormaliseOrKeep(_turnStartAxes[i] + (_axes[i] - _turnStartAxes[i]) * turnFraction);
	}
	const auto centre = _turnStartCentre + (_centre - _turnStartCentre) * turnFraction;
	if (animated)
	{
		axes = QuarterTurned(axes);
	}
	const auto scaled = axes * _scale;
	return {.axes = scaled, .origin = centre - scaled * _centreOfMass};
}

glm::mat3 openblack::physics::QuarterTurned(const glm::mat3& axes)
{
	auto turned = axes;
	turned[0] = k_QuarterTurnCos * axes[0] + k_QuarterTurnSin * axes[2];
	turned[2] = k_QuarterTurnCos * axes[2] - k_QuarterTurnSin * axes[0];
	return turned;
}

glm::mat3 openblack::physics::QuarterTurnedBack(const glm::mat3& axes)
{
	auto turned = axes;
	turned[0] = k_QuarterTurnCos * axes[0] - k_QuarterTurnSin * axes[2];
	turned[2] = k_QuarterTurnCos * axes[2] + k_QuarterTurnSin * axes[0];
	return turned;
}
