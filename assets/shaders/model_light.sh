#ifndef MODEL_LIGHT_SH
#define MODEL_LIGHT_SH

// The GPU side of src/Graphics/ModelLight.h: the one per-vertex model light of the original
// (on the CPU over D3DTLVERTEX), its single point light (placed every frame by
// the game) and its ambient = 90. Wiki: rendering-objects.md, "Luz de los modelos". The CPU and this one
// must stay the same: the rounding of I (to nearest, halves to even), the >> 8 of the factor and the truncation of each
// channel. Every model of the original goes through this integer rule: the float formula of the D3D T&L path
// (c (90 + 166 N.L) / 256) is unreachable in this build, because the start-up code sets a flag that keeps the D3D
// set-up from ever setting "Support Hardware T'n'L", which the hardware T&L draw needs
// (the wiki has the details).

uniform vec4 u_modelLight; // xyz: the light's position in the world, w: ambient 0..255 = 90

// I = round(255 (n . l)): rounded to the nearest, halves to even (the FPU's default mode). The
// store to a float32 before it is not worth copying. `truncate` is the truncating variant of the same rule
// (used by some other draws of the original), which cuts towards zero instead.
float ModelLightI(vec3 nLocal, vec3 lLocal, bool truncate)
{
	float lit = 255.0f * dot(nLocal, lLocal);
	if (truncate)
	{
		return lit < 0.0f ? -floor(-lit) : floor(lit);
	}
	float intensity = floor(lit + 0.5f);
	if (intensity - lit == 0.5f && mod(intensity, 2.0f) != 0.0f)
	{
		intensity -= 1.0f;
	}
	return intensity;
}

// f = I < 0 ? amb : amb + ((255 - amb) I >> 8), so with amb = 90 it is 90 or 90..254: never the
// full 256 of a float formula
float ModelLightFactor(float intensity, float ambient)
{
	return intensity < 0.0f ? ambient : ambient + floor((255.0f - ambient) * intensity / 256.0f);
}

// The diffuse of each channel of a 0..255 colour, (c f) >> 8 truncated. The alpha is untouched:
// the caller keeps it.
vec3 ModelLightDiffuse(vec3 c255, float factor)
{
	return floor(c255 * factor / 256.0f);
}

// The light in the mesh's own space, normalised: the object's inverse matrix (the rigid path) and the per-bone
// B^-1 (W2C Lpos) of the boned one (each bone matrix inverted over the light in camera space), which come to the
// same thing if the bone matrices go all the way to the camera (B = W2C Obj Bc), so that the camera cancels
// out (inferred: they are projected with no other matrix). The direction is taken from the ORIGIN of the
// bone (or of the object), not from the vertex, and it meets the raw local normal (not rotated, not normalised).
// `axisX/Y/Z` and `origin` are that space's axes and origin in the world; the inverse is the general one
// (adjugate / determinant), so the adjugate is enough here: 1 / det goes away with the
// normalisation and only its sign is kept, for the mirrored matrices.
vec3 ModelLightLocal(vec3 axisX, vec3 axisY, vec3 axisZ, vec3 origin, vec3 lightPos)
{
	vec3 toLight = lightPos - origin;
	vec3 adjugateRow0 = cross(axisY, axisZ);
	vec3 adjugateRow1 = cross(axisZ, axisX);
	vec3 adjugateRow2 = cross(axisX, axisY);
	float determinant = dot(axisX, adjugateRow0);
	vec3 local = vec3(dot(adjugateRow0, toLight), dot(adjugateRow1, toLight), dot(adjugateRow2, toLight));
	local *= determinant < 0.0f ? -1.0f : 1.0f;
	// a flattened matrix (a scale of 0) would leave 0 / 0 here; the original's inverse square root has its own
	// guard for it, and with no direction the vertex only takes the ambient
	float lengthSquared = dot(local, local);
	return lengthSquared > 0.0f ? local / sqrt(lengthSquared) : vec3(0.0f, 0.0f, 0.0f);
}

#endif // MODEL_LIGHT_SH
