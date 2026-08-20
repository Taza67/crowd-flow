/* cpu_naive.c — O(N²) neighbour scan. Twin of cpu_opt.c. */

#include "cpu_naive.h"
#include "steering.h"

static Vec2 boid_forces(const Agent *a, const Simulation *s, int N)
{
	float sep_x = 0, sep_y = 0, aln_x = 0, aln_y = 0, coh_x = 0, coh_y = 0;
	int n_nbr = 0, n_sep = 0;
	const float R2 = PERCEPTION_RADIUS * PERCEPTION_RADIUS;
	const float S2 = SEPARATION_DIST * SEPARATION_DIST;

	for (int j = 0; j < N; j++)
	{
		if (a == &s->agents[j])
			continue;
		steering_accum_neighbour(a->pos.x, a->pos.y,
								 s->agents[j].pos.x, s->agents[j].pos.y,
								 s->agents[j].vel.x, s->agents[j].vel.y,
								 R2, S2, &sep_x, &sep_y, &n_sep,
								 &aln_x, &aln_y, &coh_x, &coh_y, &n_nbr);
	}
	return steering_flock(sep_x, sep_y, n_sep, aln_x, aln_y, coh_x, coh_y, n_nbr,
						  a->pos.x, a->pos.y, a->vel.x, a->vel.y);
}

void cpu_naive_run_step(Simulation *s)
{
	int N = s->n_agents;
	static Vec2 new_vel[MAX_AGENTS];
	static Vec2 new_pos[MAX_AGENTS];

	for (int i = 0; i < N; i++)
	{
		Agent *a = &s->agents[i];
		const Goal *g = &s->goals[a->goal_idx];
		Vec2 acc = steering_forces(boid_forces(a, s, N), a->pos.x, a->pos.y, g->x, g->y,
								   s->obstacles, s->n_obstacles);
		steering_integrate(a->pos.x, a->pos.y, a->vel.x, a->vel.y, acc,
						   &new_pos[i].x, &new_pos[i].y, &new_vel[i].x, &new_vel[i].y);
	}
	for (int i = 0; i < N; i++)
	{
		s->agents[i].pos = new_pos[i];
		s->agents[i].vel = new_vel[i];
	}
	simulation_finish_step(s);
}
