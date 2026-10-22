#ifndef INSTANCE_SH
#define INSTANCE_SH

// Moves a point or a direction by an instance's matrix. The vertex shaders put the four instance vectors into the
// matrix by index: in GLSL an index gives a column, in the other shader languages it gives a row. Thus the order of
// the product changes with the language, so that each vector of the instance is a column of the transform.

#if BGFX_SHADER_LANGUAGE_GLSL
vec3 InstanceMul(mat3 _mtx, vec3 _vec) { return mul(_mtx, _vec); }
vec4 InstanceMul(mat4 _mtx, vec4 _vec) { return mul(_mtx, _vec); }
#else
vec3 InstanceMul(mat3 _mtx, vec3 _vec) { return mul(_vec, _mtx); }
vec4 InstanceMul(mat4 _mtx, vec4 _vec) { return mul(_vec, _mtx); }
#endif // BGFX_SHADER_LANGUAGE_GLSL

#endif // INSTANCE_SH
