#pragma once
#include <Entities/Components/Component.h>

namespace OpenBlack
{
namespace Entities
{
class Marker: Component
{
  public:
	explicit Marker(EntityId entityId, float x, float y, float z):
	    Component(entityId), x(x), y(y), z(z)
	{
	}
	const float x;
	const float y;
	const float z;
};
} // namespace Entities
} // namespace OpenBlack
