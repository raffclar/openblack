$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#define ATMOS_MULTIPLE_TABLE
#include "atmosphere.sh"

// EXPERIMENT (not for merge): the texture of the sky round the eye, each frame (see atmosphere::SkyRadiance). Each
// texel is a direction: down the texture from the zenith to the horizon (the middle row) and the ground, and across it
// from towards the light to away from it. The light that the air scatters is summed along the direction's ray, for a
// light of strength 1.

// xy: the size of the texture; zw: unused
uniform vec4 u_atmosTarget;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_atmosTarget.xy;
	float altitude = max(u_atmosParams.z, 1e-3f);
	float radius = ATMOS_GROUND_RADIUS + altitude;
	// The direction at this texel (atmosphere::FromSkyViewUv)
	float beta = acos(clamp(sqrt(radius * radius - ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS) / radius, -1.0f, 1.0f));
	float zenithHorizon = ATMOS_PI - beta;
	float viewAngle = 0.0f;
	if (uv.y < 0.5f)
	{
		float coord = 1.0f - 2.0f * uv.y;
		viewAngle = zenithHorizon * (1.0f - coord * coord);
	}
	else
	{
		float coord = uv.y * 2.0f - 1.0f;
		viewAngle = zenithHorizon + beta * coord * coord;
	}
	float lightCos = -(uv.x * uv.x * 2.0f - 1.0f);
	float viewMu = cos(viewAngle);
	float across = sqrt(max(1.0f - viewMu * viewMu, 0.0f));
	vec3 view = vec3(across * lightCos, viewMu, across * sqrt(max(1.0f - lightCos * lightCos, 0.0f)));
	float lightY = clamp(u_atmosLight.y, -1.0f, 1.0f);
	vec3 towardsLight = vec3(sqrt(max(1.0f - lightY * lightY, 0.0f)), lightY, 0.0f);

	// From over the top of the air the ray starts where it comes into the air, and a ray past the air has no light
	float rayStart = 0.0f;
	if (radius > ATMOS_TOP_RADIUS)
	{
		float disc = radius * radius * (viewMu * viewMu - 1.0f) + ATMOS_TOP_RADIUS * ATMOS_TOP_RADIUS;
		if (disc < 0.0f || viewMu >= 0.0f)
		{
			gl_FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			return;
		}
		rayStart = -radius * viewMu - sqrt(disc);
	}
	float rayEnd = AtmosHitsGround(radius, viewMu) ? AtmosDistanceToGround(radius, viewMu) : AtmosDistanceToTop(radius, viewMu);
	float stride = max(rayEnd - rayStart, 0.0f) / 30.0f;
	vec3 eye = vec3(0.0f, radius, 0.0f) + view * rayStart;
	float cosAngle = dot(view, towardsLight);
	vec3 light = vec3_splat(0.0f);
	vec3 through = vec3_splat(1.0f);
	for (int i = 0; i < 30; ++i)
	{
		float t = (float(i) + 0.5f) * stride;
		vec3 p = eye + view * t;
		float r = length(p);
		float h = r - ATMOS_GROUND_RADIUS;
		float lightMu = dot(p / r, towardsLight);
		vec3 scattered = AtmosScatterAt(h, lightMu, cosAngle, 1.0f);
		vec3 extinction = max(AtmosExtinction(h), vec3_splat(1e-7f));
		vec3 dimmed = exp(-extinction * stride);
		light += through * scattered * (1.0f - dimmed) / extinction;
		through *= dimmed;
	}
	gl_FragColor = vec4(light, 1.0f);
}
