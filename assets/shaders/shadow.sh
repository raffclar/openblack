#ifndef SHADOW_SH
#define SHADOW_SH

// The projected shadows' texture coordinates (graphics::shadow_list, wiki: rendering.md, "Sombras proyectadas"). The
// texture is the CPU's 32 x 32 alpha n / 15 with the fade baked in and an empty outer ring, sampled with CLAMP
// (the shadow material's wrap mode), so outside the box it gives 0.

// The land: u = (Lx + (x - Lx) t' - x0) / (x1 - x0), v the same in z.
// light: xyz the light, w t' (one per shadow); box: x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)
vec2 LandShadowUv(vec2 xz, vec4 light, vec4 box)
{
	return (light.xz + (xz - light.xz) * light.w - box.xy) * box.zw;
}

// The objects: straight down, u = (x - x0) / (x1 - x0), v the same in z
vec2 ObjectShadowUv(vec2 xz, vec4 box)
{
	return (xz - box.xy) * box.zw;
}

// The code 0x400 of a vertex (the land and the objects alike): k = x d.x + z d.z below the shadow's least k
// (d = the caster minus the light in x, z). 1 kept, 0 coded. cull: x, y d.x, d.z, z the least k
float ShadowKept(vec2 xz, vec4 cull)
{
	return xz.x * cull.x + xz.y * cull.y < cull.z ? 0.0f : 1.0f;
}

#endif // SHADOW_SH
