#include <map>

#include <Entities/Components/Component.h>
#include <Entities/Components/Villager.h>
#include <Entities/Components/Model.h>
#include <Entities/Components/Marker.h>

namespace OpenBlack {
class Camera;
}

namespace OpenBlack {
namespace Graphics {
class ShaderManager;
}
}

namespace OpenBlack
{
namespace Entities
{
class EntityManager
{
public:
	void AddVillager(EntityId id, Villager villager, Model model, Marker marker);
	void DebugCreateEntities(float x, float y, float z);
	void DrawModels(const Camera& camera, ShaderManager& shaderManager);
	void Update();
private:
	class Entity
	{
	public:
		EntityId id;
		bool active = true;
	};

	std::map<EntityId, Entity> _entities;
	std::map<EntityId, Villager> _villagerComponents;
	std::map<EntityId, Component> _buildingComponents;
	std::map<EntityId, Model> _modelComponents;
	std::map<EntityId, Marker> _markerComponents;
	void RemoveInactive();
	void UpdateVillagers();
	void UpdateBuildings();
};
} // namespace Entities
} // namespace OpenBlack
