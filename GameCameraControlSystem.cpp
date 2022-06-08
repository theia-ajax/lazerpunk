#include "GameCameraControlSystem.h"

#include "components.h"

void GameCameraControlSystem::SnapFocusToFollow(Entity cameraEntity) const
{
	auto [transform, view, camera] = GetArchetype(cameraEntity);

	if (!camera.followTarget)
		return;

	const auto& [position, _, __] = GetWorld().GetComponent<Transform>(camera.followTarget);

	transform.position = position - view.extents / 2 / view.scale;
	view.center = position;
}

void GameCameraControlSystem::Update(const GameTime& time) const
{
	for (Entity entity : entities)
	{
		auto [transform, view, camera] = GetArchetype(entity);

		if (camera.followTarget)
		{
			const auto& targetTransform = GetWorld().GetComponent<Transform>(camera.followTarget);

			auto [dx, dy] = targetTransform.position - view.center;

			if (dx < camera.followBounds.Left())
				transform.position.x += dx - camera.followBounds.Left();
			if (dx > camera.followBounds.Right())
				transform.position.x += dx - camera.followBounds.Right();
			if (dy < camera.followBounds.Top())
				transform.position.y += dy - camera.followBounds.Top();
			if (dy > camera.followBounds.Bottom())
				transform.position.y += dy - camera.followBounds.Bottom();
		}

		if (camera.clampViewMap)
		{
			const auto& gameMap = map::Get(camera.clampViewMap);
			Bounds2D viewBounds = gameMap.worldBounds;
			viewBounds.max = viewBounds.max - camera_view::WorldExtents(view);
			transform.position = viewBounds.ClampPoint(transform.position);
		}
	}
}
