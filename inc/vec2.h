/* vec2.h — 2D vectors. */

#ifndef VEC2_H
#define VEC2_H

#include <math.h>
#include "cf_compat.h"

typedef struct
{
	float x, y;
} Vec2;

CF_INLINE Vec2 vec2_add(Vec2 a, Vec2 b)
{
	return (Vec2){a.x + b.x, a.y + b.y};
}

CF_INLINE Vec2 vec2_mul(Vec2 v, float s)
{
	return (Vec2){v.x * s, v.y * s};
}

CF_INLINE float vec2_len2(Vec2 v)
{
	return v.x * v.x + v.y * v.y;
}

CF_INLINE float vec2_len(Vec2 v)
{
	return sqrtf(vec2_len2(v));
}

CF_INLINE Vec2 vec2_norm(Vec2 v)
{
	float l = vec2_len(v);
	if (l < 1e-6f)
		return (Vec2){0.f, 0.f};
	return (Vec2){v.x / l, v.y / l};
}

CF_INLINE Vec2 vec2_clamp(Vec2 v, float max_len)
{
	float l = vec2_len(v);
	if (l > max_len && l > 1e-6f)
		return vec2_mul(v, max_len / l);
	return v;
}

CF_INLINE float vec2_dist2(Vec2 a, Vec2 b)
{
	float dx = a.x - b.x, dy = a.y - b.y;
	return dx * dx + dy * dy;
}

CF_INLINE float vec2_dist(Vec2 a, Vec2 b)
{
	return sqrtf(vec2_dist2(a, b));
}

#endif
