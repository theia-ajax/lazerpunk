#include "PlayerControlSystem.h"

#include "components.h"

void PlayerControlSystem::Update(const GameTime& time) const
{
	using namespace math;
	using namespace vec2;

	for (Entity entity : entities)
	{
		auto& input = GetWorld().GetComponent<GameInput>(entity);
		auto& transform = GetWorld().GetComponent<Transform>(entity);
		auto& [facing] = GetWorld().GetComponent<Facing>(entity);
		auto& [velocity] = GetWorld().GetComponent<Velocity>(entity);
		auto& control = GetWorld().GetComponent<PlayerControl>(entity);

		control.velocity = input.moveInput * 10.0f;

		if (input.requestDash)
		{
			input.requestDash = false;
			if (Length(control.dashVelocity) < 0.1f)
			{
				control.dashVelocity = DirectionVector(facing) * 25.0f;
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

		velocity = control.velocity + control.dashVelocity;

		if (input.direction != Direction::Invalid)
		{
			facing = input.direction;

			float posRefComp = transform.position.y;
			float velRefComp = velocity.x;
			float* velTargetComp = &velocity.y;

			if (IsDirectionVert(input.direction))
			{
				posRefComp = transform.position.x;
				velRefComp = velocity.y;
				velTargetComp = &velocity.x;
			}

			float current = posRefComp;
			float moveMag = abs(velRefComp) / 4;
			float target = round((current - 0.5f) * 2) / 2 + 0.5f;
			float deltaDiff = (target - current) / time.dt();
			*velTargetComp += min(moveMag, abs(deltaDiff)) * Sign(deltaDiff);
		}
	}
}
