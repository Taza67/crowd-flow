/* obstacle.h — circular obstacles. */

#ifndef OBSTACLE_H
#define OBSTACLE_H

#include "constants.h"

typedef struct
{
	float x, y;
	float radius;
} Obstacle;

static inline float obstacle_hard_min(float radius)
{
	return radius + AGENT_RADIUS + CONTACT_SLOP;
}

static inline int inside_obstacle(float x, float y, const Obstacle *obs, int n_obs)
{
	for (int o = 0; o < n_obs; o++)
	{
		float dx = x - obs[o].x, dy = y - obs[o].y;
		float md = obstacle_hard_min(obs[o].radius);
		if (dx * dx + dy * dy < md * md)
			return 1;
	}
	return 0;
}

#endif
