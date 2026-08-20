/* constraints.c — obstacle push, world clamp, CSR agent pairs. */

#include <string.h>
#include "constraints.h"
#include "obstacle.h"
#include "vec2.h"

static int cg_count[CONSTR_GRID_CELLS];
static int cg_start[CONSTR_GRID_CELLS + 1];
static int cg_list[MAX_AGENTS];

static inline int cg_cell_xy(float x, float y, int *out_cx, int *out_cy)
{
	int cx = (int)((x + WORLD_HALF) / CONSTR_CELL_SIZE);
	int cy = (int)((y + WORLD_HALF) / CONSTR_CELL_SIZE);
	if (cx < 0)
		cx = 0;
	else if (cx >= CONSTR_GRID_DIM)
		cx = CONSTR_GRID_DIM - 1;
	if (cy < 0)
		cy = 0;
	else if (cy >= CONSTR_GRID_DIM)
		cy = CONSTR_GRID_DIM - 1;
	if (out_cx)
		*out_cx = cx;
	if (out_cy)
		*out_cy = cy;
	return cy * CONSTR_GRID_DIM + cx;
}

static void build_constraint_grid(const Simulation *s)
{
	int N = s->n_agents;
	memset(cg_count, 0, sizeof(cg_count));
	for (int i = 0; i < N; i++)
		cg_count[cg_cell_xy(s->agents[i].pos.x, s->agents[i].pos.y, NULL, NULL)]++;
	cg_start[0] = 0;
	for (int c = 0; c < CONSTR_GRID_CELLS; c++)
		cg_start[c + 1] = cg_start[c] + cg_count[c];
	memset(cg_count, 0, sizeof(cg_count));
	for (int i = 0; i < N; i++)
	{
		int c = cg_cell_xy(s->agents[i].pos.x, s->agents[i].pos.y, NULL, NULL);
		cg_list[cg_start[c] + cg_count[c]++] = i;
	}
}

static inline void resolve_obstacle_collisions(Agent *a, Simulation *s)
{
	for (int o = 0; o < s->n_obstacles; o++)
	{
		const Obstacle *ob = &s->obstacles[o];
		float dx = a->pos.x - ob->x;
		float dy = a->pos.y - ob->y;
		float d2 = dx * dx + dy * dy;
		float min_d = obstacle_hard_min(ob->radius);
		if (d2 >= min_d * min_d)
			continue;

		float d = sqrtf(d2);
		if (d < 1e-6f)
		{
			dx = min_d;
			d = min_d;
		}
		float nx = dx / d, ny = dy / d;
		float pen = min_d - d;
		a->pos.x += nx * (pen * CONTACT_PUSH_GAIN + CONTACT_PUSH_BIAS);
		a->pos.y += ny * (pen * CONTACT_PUSH_GAIN + CONTACT_PUSH_BIAS);
		float vdot = a->vel.x * nx + a->vel.y * ny;
		if (vdot < 0.0f)
		{
			a->vel.x -= (1.0f + RESTITUTION) * vdot * nx;
			a->vel.y -= (1.0f + RESTITUTION) * vdot * ny;
			a->vel = vec2_clamp(a->vel, MAX_SPEED);
		}
	}
}

static inline void clamp_to_world(Agent *a)
{
	float half = WORLD_HALF - AGENT_RADIUS;
	if (a->pos.x > half)
	{
		a->pos.x = half;
		if (a->vel.x > 0)
			a->vel.x = 0;
	}
	if (a->pos.x < -half)
	{
		a->pos.x = -half;
		if (a->vel.x < 0)
			a->vel.x = 0;
	}
	if (a->pos.y > half)
	{
		a->pos.y = half;
		if (a->vel.y > 0)
			a->vel.y = 0;
	}
	if (a->pos.y < -half)
	{
		a->pos.y = -half;
		if (a->vel.y < 0)
			a->vel.y = 0;
	}
}

static inline void resolve_agent_pair(Agent *a, Agent *b, float min_dist)
{
	float ddx = a->pos.x - b->pos.x;
	float ddy = a->pos.y - b->pos.y;
	float d2 = ddx * ddx + ddy * ddy;
	if (d2 >= min_dist * min_dist || d2 < 1e-12f)
		return;

	float d = sqrtf(d2);
	float nx = ddx / d, ny = ddy / d;
	float push = (min_dist - d + CONTACT_PAIR_SLOP) * 0.5f;
	a->pos.x += nx * push;
	a->pos.y += ny * push;
	b->pos.x -= nx * push;
	b->pos.y -= ny * push;

	float vrel_n = (a->vel.x - b->vel.x) * nx + (a->vel.y - b->vel.y) * ny;
	if (vrel_n < 0.0f)
	{
		float impulse = (1.0f + RESTITUTION) * vrel_n * 0.5f;
		a->vel.x -= impulse * nx;
		a->vel.y -= impulse * ny;
		b->vel.x += impulse * nx;
		b->vel.y += impulse * ny;
		a->vel = vec2_clamp(a->vel, MAX_SPEED);
		b->vel = vec2_clamp(b->vel, MAX_SPEED);
	}
}

void resolve_constraints(Simulation *s)
{
	int N = s->n_agents;
	const float min_agent_dist = 2.0f * AGENT_RADIUS;

	for (int pass = 0; pass < N_PASSES; pass++)
	{
		for (int i = 0; i < N; i++)
		{
			resolve_obstacle_collisions(&s->agents[i], s);
			clamp_to_world(&s->agents[i]);
		}
		build_constraint_grid(s);
		for (int i = 0; i < N; i++)
		{
			Agent *a = &s->agents[i];
			int cx, cy;
			cg_cell_xy(a->pos.x, a->pos.y, &cx, &cy);
			for (int dy = -1; dy <= 1; dy++)
			{
				int ny = cy + dy;
				if (ny < 0 || ny >= CONSTR_GRID_DIM)
					continue;
				for (int dx = -1; dx <= 1; dx++)
				{
					int nx = cx + dx;
					if (nx < 0 || nx >= CONSTR_GRID_DIM)
						continue;
					int cell = ny * CONSTR_GRID_DIM + nx;
					for (int k = cg_start[cell]; k < cg_start[cell + 1]; k++)
					{
						int j = cg_list[k];
						if (j > i)
							resolve_agent_pair(a, &s->agents[j], min_agent_dist);
					}
				}
			}
		}
	}
	for (int i = 0; i < N; i++)
		clamp_to_world(&s->agents[i]);
}
