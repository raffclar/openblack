$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "atmosphere.sh"

// EXPERIMENT (not for merge): the texture of the light that gets through the air from each height to the top, along
// each angle (see atmosphere::Transmittance). It is made again only when the air's dust changes.

// xy: the size of the texture; zw: unused
uniform vec4 u_atmosTarget;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_atmosTarget.xy;
	// The height and the angle at this texel (atmosphere::FromTransmittanceUv)
	float horizon = sqrt(ATMOS_TOP_RADIUS * ATMOS_TOP_RADIUS - ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS);
	float rho = horizon * uv.y;
	float radius = sqrt(rho * rho + ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS);
	float dMin = ATMOS_TOP_RADIUS - radius;
	float dMax = rho + horizon;
	float d = dMin + uv.x * (dMax - dMin);
	float mu = d <= 0.0f ? 1.0f : clamp((horizon * horizon - rho * rho - d * d) / (2.0f * radius * d), -1.0f, 1.0f);

	float rayLength = AtmosDistanceToTop(radius, mu);
	float stride = rayLength / 40.0f;
	vec3 depth = vec3_splat(0.0f);
	for (int i = 0; i < 40; ++i)
	{
		float t = (float(i) + 0.5f) * stride;
		float r = sqrt(radius * radius + t * t + 2.0f * radius * mu * t);
		depth += AtmosExtinction(r - ATMOS_GROUND_RADIUS) * stride;
	}
	gl_FragColor = vec4(exp(-depth), 1.0f);
}
