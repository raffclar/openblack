#include <Entities/EntityID.h>

#pragma once

namespace OpenBlack
{
namespace Entities
{
class Component
{
public:
	explicit Component(EntityId entityId):
		_entity(entityId)
	{
	}

	EntityId getEntityId()
	{
		return _entity;
	}

private:
	EntityId _entity;
};
} // namespace Entities
} // namespace OpenBlack
