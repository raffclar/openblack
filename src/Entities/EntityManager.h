#include <entt/entt.hpp>

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
	void DebugCreateEntities(float x, float y, float z);
	void DrawModels(const Camera& camera, Graphics::ShaderManager& shaderManager);
	void Update();
private:
	entt::registry _registry;
};

} // namespace Entities
} // namespace OpenBlack
