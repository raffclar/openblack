#include "EntityManager.h"


#include <3D/Camera.h>
#include <3D/MeshPack.h>
#include <Game.h>
#include <Graphics/ShaderManager.h>
#include <vector>
#include <Entities/Components/Transform.h>
#include <Entities/Components/Model.h>
#include <Entities/Components/Villager.h>

namespace OpenBlack::Entities
{

void EntityManager::DebugCreateEntities(float x, float y, float z)
{
	auto entity = _registry.create();
	_registry.assign<Transform>(entity, x, y, z, 2.5f, 180.0f, 111.0f, 0.0f);
	uint32_t meshId = 0;
	_registry.assign<Model>(entity, meshId);
	uint32_t health = 100;
	uint32_t age = 18;
	uint32_t hunger = 100;
	_registry.assign<Villager>(entity, health, age, hunger);
}

void EntityManager::DrawModels(const Camera& camera, ShaderManager& shaderManager)
{
	auto meshPack = Game::instance()->GetMeshPack();
	auto view = _registry.view<Model, Transform>();

	for (auto entity : view)
	{
		auto& model = view.get<Model>(entity);
		auto& position = view.get<Transform>(entity);

		auto _modelPosition = glm::vec3(position.x, position.y, position.z);
		auto _modelRotation = glm::vec3(180.0f, 111.0f, 0.0f);
		auto _modelScale    = glm::vec3(2.5f);

		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix           = glm::translate(modelMatrix, _modelPosition);

		modelMatrix = glm::rotate(modelMatrix, glm::radians(_modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(_modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(_modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		modelMatrix = glm::scale(modelMatrix, _modelScale);

		ShaderProgram* objectShader = shaderManager.GetShader("SkinnedMesh");
		objectShader->Bind();
		objectShader->SetUniformValue("u_viewProjection", camera.GetViewProjectionMatrix());
		objectShader->SetUniformValue("u_modelTransform", modelMatrix);

		meshPack.models[model.meshId]->Draw(objectShader);
	}
}

} // namespace OpenBlack::Entities
