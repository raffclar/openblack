#ifndef CLOUDS_SHADOW_SH
#define CLOUDS_SHADOW_SH

// EXPERIMENT (not for merge): the shadow of the volumetric clouds, seen from above round the eye (see
// fs_clouds_shadow.sc)

SAMPLER2D(s_cloudShadow, 11);
// xy: where the map starts; z: the units that it covers, 0 while the clouds are off; w: how dark the shadow is
uniform vec4 u_cloudShadow;

// The share of the sun's or the moon's light that comes through the clouds to a point across the land, 1 with no cloud
float CloudShadeAt(vec2 xz)
{
	if (u_cloudShadow.z <= 0.0f)
	{
		return 1.0f;
	}
	vec2 uv = (xz - u_cloudShadow.xy) / u_cloudShadow.z;
	if (uv.x < 0.0f || uv.y < 0.0f || uv.x > 1.0f || uv.y > 1.0f)
	{
		return 1.0f;
	}
	return texture2DLod(s_cloudShadow, uv, 0.0f).r;
}

#endif // CLOUDS_SHADOW_SH
