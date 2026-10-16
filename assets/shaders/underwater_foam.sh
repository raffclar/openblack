#ifndef UNDERWATER_FOAM_SH
#define UNDERWATER_FOAM_SH

// EXPERIMENT (not for merge): the foam that builds up on the water where the waves break and decays over time. A field
// of texels round the place that the camera looks at keeps it (see underwater::FoamClock). Each texel keeps x: how
// thick the foam is (0 to 2), y: how old it is in seconds, z: how much air it holds (0 to 2), w: unused.

// xy: the corner of the foam field with the lowest x and z; z: 1 while the foam field shows; w: the side of the field
// in units
uniform vec4 u_uwFoam;

// Where a point of the water is in the foam field, 0 to 1 on each axis inside it
vec2 UwFoamUv(vec2 xz)
{
	return (xz - u_uwFoam.xy) / max(u_uwFoam.w, 1.0f);
}

// How much the foam field shows at a point of the field: 1 inside, and less near its sides, so its edge does not show
float UwFoamInside(vec2 uv)
{
	vec2 edge = min(uv, vec2_splat(1.0f) - uv);
	return smoothstep(0.0f, 0.06f, min(edge.x, edge.y)) * (u_uwFoam.z > 0.5f ? 1.0f : 0.0f);
}

#endif // UNDERWATER_FOAM_SH
