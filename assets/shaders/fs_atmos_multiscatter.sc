$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "atmosphere.sh"

// EXPERIMENT (not for merge): the table of the light that the air scatters more than once (atmosphere::
// MultipleScatteringAt, Hillaire 2020). Across: the cosine of the light from -1 to 1; down: the height from the ground
// to the top of the air. Each texel sums the second scattering from 64 directions round its point, with the light from
// the ground, and then the scatterings after it as a series. The table is made again only when the air's dust changes.

// xy: the size of the texture; zw: unused
uniform vec4 u_atmosTarget;

#define MULTIPLE_DIRECTIONS 64
#define MULTIPLE_STEPS 20
#define MULTIPLE_GROUND_ALBEDO 0.1f

void main()
{
	vec2 texel = floor(gl_FragCoord.xy);
	float lightMu = -1.0f + 2.0f * texel.x / (u_atmosTarget.x - 1.0f);
	float altitude = (ATMOS_TOP_RADIUS - ATMOS_GROUND_RADIUS) * texel.y / (u_atmosTarget.y - 1.0f);
	float radius = ATMOS_GROUND_RADIUS + clamp(altitude, 1e-3f, ATMOS_TOP_RADIUS - ATMOS_GROUND_RADIUS - 1e-3f);
	vec3 eye = vec3(0.0f, radius, 0.0f);
	vec3 light = vec3(sqrt(max(1.0f - lightMu * lightMu, 0.0f)), lightMu, 0.0f);
	// The directions are on a Fibonacci spiral, so that they cover the ball evenly
	float golden = ATMOS_PI * (3.0f - sqrt(5.0f));
	vec3 second = vec3_splat(0.0f);
	vec3 share = vec3_splat(0.0f);
	for (int i = 0; i < MULTIPLE_DIRECTIONS; ++i)
	{
		float y = 1.0f - 2.0f * (float(i) + 0.5f) / float(MULTIPLE_DIRECTIONS);
		float across = sqrt(max(1.0f - y * y, 0.0f));
		float angle = golden * float(i);
		vec3 direction = vec3(across * cos(angle), y, across * sin(angle));
		bool ground = AtmosHitsGround(radius, y);
		float rayLength = ground ? AtmosDistanceToGround(radius, y) : AtmosDistanceToTop(radius, y);
		float stride = rayLength / float(MULTIPLE_STEPS);
		vec3 throughput = vec3_splat(1.0f);
		for (int k = 0; k < MULTIPLE_STEPS; ++k)
		{
			vec3 p = eye + direction * ((float(k) + 0.5f) * stride);
			float r = length(p);
			float h = r - ATMOS_GROUND_RADIUS;
			vec4 scatteringParts = AtmosScattering(h);
			vec3 scattering = scatteringParts.rgb + vec3_splat(scatteringParts.a);
			vec3 extinction = max(AtmosExtinction(h), vec3_splat(1e-7f));
			vec3 dimmed = exp(-extinction * stride);
			vec3 part = throughput * (vec3_splat(1.0f) - dimmed) / extinction;
			second += part * scattering * AtmosTransmittance(h, dot(p / r, light)) / (4.0f * ATMOS_PI);
			share += part * scattering;
			throughput *= dimmed;
		}
		// The ground sends back some of the sun's light that reaches it
		if (ground)
		{
			vec3 normal = normalize(eye + direction * rayLength);
			float lit = dot(normal, light);
			second += throughput * AtmosTransmittance(0.0f, lit) * max(lit, 0.0f) * MULTIPLE_GROUND_ALBEDO / ATMOS_PI;
		}
	}
	second /= float(MULTIPLE_DIRECTIONS);
	share /= float(MULTIPLE_DIRECTIONS);
	// Each scattering after the second gives the same share of the one before it: a series
	gl_FragColor = vec4(second / max(vec3_splat(1.0f) - share, vec3_splat(1e-3f)), 1.0f);
}
