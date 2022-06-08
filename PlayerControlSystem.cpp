#include "PlayerControlSystem.h"

#include "components.h"

void PlayerControlSystem::Update(const GameTime& time) const
{
	using namespace math;
	using namespace vec2;

	for (Entity entity : entities)
	{
		auto [input, transform, facing, velocity, control] = GetArchetype(entity);

		control.velocity = input.moveInput * 10.0f;

		if (input.requestDash)
		{
			input.requestDash = false;
			if (Length(control.dashVelocity) < 0.1f)
			{
				control.dashVelocity = DirectionVector(facing.facing) * 25.0f;
			}
		}

		float dashMag = Length(control.dashVelocity);
		if (float newDashMag = math::Damp(dashMag, 0.0f, 5.0f, time.dt()); newDashMag < control.minDashThreshold)
		{
			control.dashVelocity = vec2::Zero;
		}
		else
		{
			control.dashVelocity = Normalize(control.dashVelocity) * newDashMag;
		}

		Vec2 newVelocity = control.velocity + control.dashVelocity;

		if (input.direction != Direction::Invalid)
		{
			facing.facing = input.direction;

			float posRefComp = transform.position.y;
			float velRefComp = input.moveInput.x;
			float* velTargetComp = &newVelocity.y;

			if (IsDirectionVert(input.direction))
			{
				posRefComp = transform.position.x;
				velRefComp = input.moveInput.y;
				velTargetComp = &newVelocity.x;
			}

			float current = posRefComp;
			float moveMag = abs(velRefComp) / 16;
			float target = round((current - 0.5f) * 2) / 2 + 0.5f;
			float deltaDiff = (target - current);
			*velTargetComp += min(moveMag, abs(deltaDiff)) * Sign(deltaDiff) / time.dt();
		}

		velocity.velocity = newVelocity;
	}
}
