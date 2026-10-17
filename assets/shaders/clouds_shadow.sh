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
	// The shadow fades out towards the edge of its map, so the edge does not show
	vec2 fromMiddle = abs(uv * 2.0f - 1.0f);
	float inside = 1.0f - smoothstep(0.8f, 1.0f, max(fromMiddle.x, fromMiddle.y));
	return mix(1.0f, texture2DLod(s_cloudShadow, uv, 0.0f).r, inside);
}

// The lowest point of the clouds' layer, as the map counts it (the bottom of the layer less the lowest drop of a rain
// cloud, see clouds.sh), and the height over it where most of the cloud ends (the top of a cumulus)
#define CLOUD_SHADOW_BOTTOM 100.0f
#define CLOUD_SHADOW_FADE 720.0f

// The share of the light that comes through the clouds to a point in the air. The map holds the shadow on the sea, so
// the point takes the shadow of the place where the light's ray through the point meets the sea. The map turns the
// light's ray up to 0.2 at the least, and this function does the same. Over the bottom of the layer, less cloud is
// between the point and the light, so the shadow fades. Thus the shadow goes along the light's rays, not straight
// down, and the air over the clouds has no shadow.
float CloudShadeInAir(vec3 p, vec3 towards, float sea)
{
	vec3 ray = towards;
	ray.y = max(ray.y, 0.2f);
	ray = normalize(ray);
	vec2 onSea = p.xz - ray.xz / ray.y * max(p.y - sea, 0.0f);
	float overBottom = smoothstep(0.0f, CLOUD_SHADOW_FADE, p.y - CLOUD_SHADOW_BOTTOM);
	return mix(CloudShadeAt(onSea), 1.0f, overBottom);
}

#endif // CLOUDS_SHADOW_SH
