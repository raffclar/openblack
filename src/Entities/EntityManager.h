#include <map>

#include <Entities/Components/Component.h>

namespace OpenBlack
{
namespace Entities
{
class EntityManager
{
  public:
	void Update();
  private:
	class Entity
	{
	  public:
		EntityId id;
		bool active = true;
	};

	std::map<EntityId, Entity> _entities;
	std::map<EntityId, Component> _villagerComponents;
	std::map<EntityId, Component> _buildingComponents;
	void RemoveInactive();
	void UpdateVillagers();
	void UpdateBuildings();
};
} // namespace Entities
} // namespace OpenBlack
