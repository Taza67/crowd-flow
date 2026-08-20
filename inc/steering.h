/* steering.h — flock, goal, obstacles, Euler. Shared by CPU and CUDA. */

#ifndef STEERING_H
#define STEERING_H

#include "constants.h"
#include "obstacle.h"
#include "vec2.h"

CF_INLINE int steering_grid_coord(float val)
{
	int g = (int)((val + WORLD_HALF) / CELL_SIZE);
	if (g < 0)
		return 0;
	if (g >= GRID_DIM)
		return GRID_DIM - 1;
	return g;
}

CF_INLINE int steering_grid_cell(float x, float y)
{
	return steering_grid_coord(y) * GRID_DIM + steering_grid_coord(x);
}

CF_INLINE void steering_accum_neighbour(float ax, float ay, float bx, float by, float bvx, float bvy,
										float R2, float S2,
										float *sep_x, float *sep_y, int *n_sep,
										float *aln_x, float *aln_y, float *coh_x, float *coh_y, int *n_nbr)
{
	float dx = ax - bx, dy = ay - by;
	float d2 = dx * dx + dy * dy;
	if (d2 > R2)
		return;
	*coh_x += bx;
	*coh_y += by;
	*aln_x += bvx;
	*aln_y += bvy;
	(*n_nbr)++;
	if (d2 < S2 && d2 > 1e-8f)
	{
		float d = sqrtf(d2);
		float w = (SEPARATION_DIST - d) / SEPARATION_DIST / d;
		*sep_x += w * dx;
		*sep_y += w * dy;
		(*n_sep)++;
	}
}

CF_INLINE Vec2 steering_flock(float sep_x, float sep_y, int n_sep,
							  float aln_x, float aln_y, float coh_x, float coh_y, int n_nbr,
							  float ax, float ay, float avx, float avy)
{
	Vec2 force = {0.f, 0.f};
	if (n_sep > 0)
		force = vec2_add(force, vec2_mul(vec2_norm((Vec2){sep_x, sep_y}), W_SEPARATION));
	if (n_nbr > 0)
	{
		float inv = 1.f / n_nbr;
		Vec2 af = vec2_norm((Vec2){aln_x * inv - avx, aln_y * inv - avy});
		Vec2 cf = vec2_norm((Vec2){coh_x * inv - ax, coh_y * inv - ay});
		force = vec2_add(force, vec2_mul(af, W_ALIGNMENT));
		force = vec2_add(force, vec2_mul(cf, W_COHESION));
	}
	return force;
}

CF_INLINE int steering_clamp_goal(int gi, int n_goals)
{
	if (n_goals <= 0 || gi < 0 || gi >= n_goals)
		return 0;
	return gi;
}

CF_INLINE Vec2 steering_goal_force(float ax, float ay, float gx, float gy)
{
	return vec2_mul(vec2_norm((Vec2){gx - ax, gy - ay}), W_GOAL);
}

CF_INLINE int steering_goal_reached(float ax, float ay, float gx, float gy)
{
	return vec2_dist((Vec2){ax, ay}, (Vec2){gx, gy}) < GOAL_REACH_DIST;
}

CF_INLINE void steering_add_obstacle(float ax, float ay, float ox, float oy, float radius,
									 float *fx, float *fy)
{
	float dx = ax - ox, dy = ay - oy;
	float d = sqrtf(dx * dx + dy * dy);
	float clearance = radius + AGENT_RADIUS + OBSTACLE_CLEARANCE;
	if (d < clearance && d > 1e-6f)
	{
		float t = (clearance - d) / clearance;
		float w = t * t * t;
		*fx += W_OBSTACLE * w * dx / d;
		*fy += W_OBSTACLE * w * dy / d;
	}
}

CF_INLINE Vec2 steering_obstacle_repulsion(float x, float y, const Obstacle *obs, int n_obs)
{
	Vec2 force = {0.f, 0.f};
	for (int o = 0; o < n_obs; o++)
		steering_add_obstacle(x, y, obs[o].x, obs[o].y, obs[o].radius, &force.x, &force.y);
	return force;
}

CF_INLINE Vec2 steering_forces(Vec2 flock, float ax, float ay, float gx, float gy,
							   const Obstacle *obs, int n_obs)
{
	Vec2 acc = vec2_add(flock, steering_goal_force(ax, ay, gx, gy));
	return vec2_add(acc, steering_obstacle_repulsion(ax, ay, obs, n_obs));
}

CF_INLINE void steering_integrate(float ax, float ay, float avx, float avy, Vec2 acc,
								  float *npx, float *npy, float *nvx, float *nvy)
{
	acc = vec2_clamp(acc, MAX_FORCE);
	Vec2 nv = vec2_clamp((Vec2){avx + acc.x * DT, avy + acc.y * DT}, MAX_SPEED);
	*nvx = nv.x;
	*nvy = nv.y;
	*npx = ax + nv.x * DT;
	*npy = ay + nv.y * DT;
}

#endif
