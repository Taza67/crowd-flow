/* cpu_opt.c — SoA + CSR spatial grid. Twin of cpu_naive.c. */

#include <string.h>
#include "cpu_opt.h"
#include "steering.h"

#ifdef __GNUC__
#define PREFETCH(addr) __builtin_prefetch((addr), 0, 1)
#else
#define PREFETCH(addr)
#endif

typedef struct
{
	float px[MAX_AGENTS], py[MAX_AGENTS];
	float vx[MAX_AGENTS], vy[MAX_AGENTS];
	int gi[MAX_AGENTS];
} AgentSoa;

static AgentSoa soa;
static float new_px[MAX_AGENTS], new_py[MAX_AGENTS];
static float new_vx[MAX_AGENTS], new_vy[MAX_AGENTS];
static int g_count[GRID_CELLS];
static int g_start[GRID_CELLS + 1];
static int g_list[MAX_AGENTS];

static void soa_from_aos(const Simulation *s)
{
	for (int i = 0; i < s->n_agents; i++)
	{
		soa.px[i] = s->agents[i].pos.x;
		soa.py[i] = s->agents[i].pos.y;
		soa.vx[i] = s->agents[i].vel.x;
		soa.vy[i] = s->agents[i].vel.y;
		soa.gi[i] = s->agents[i].goal_idx;
	}
}

static void soa_to_aos(Simulation *s)
{
	for (int i = 0; i < s->n_agents; i++)
	{
		s->agents[i].pos.x = soa.px[i];
		s->agents[i].pos.y = soa.py[i];
		s->agents[i].vel.x = soa.vx[i];
		s->agents[i].vel.y = soa.vy[i];
		s->agents[i].goal_idx = soa.gi[i];
	}
}

static void build_grid(int N)
{
	memset(g_count, 0, sizeof(g_count));
	for (int i = 0; i < N; i++)
		g_count[steering_grid_cell(soa.px[i], soa.py[i])]++;
	g_start[0] = 0;
	for (int c = 0; c < GRID_CELLS; c++)
		g_start[c + 1] = g_start[c] + g_count[c];
	memset(g_count, 0, sizeof(g_count));
	for (int i = 0; i < N; i++)
	{
		int c = steering_grid_cell(soa.px[i], soa.py[i]);
		g_list[g_start[c] + g_count[c]++] = i;
	}
}

static Vec2 boid_forces(int i, float ax, float ay, float avx, float avy)
{
	float sep_x = 0, sep_y = 0, aln_x = 0, aln_y = 0, coh_x = 0, coh_y = 0;
	int n_nbr = 0, n_sep = 0;
	const float R2 = PERCEPTION_RADIUS * PERCEPTION_RADIUS;
	const float S2 = SEPARATION_DIST * SEPARATION_DIST;

	int cx = steering_grid_coord(ax), cy = steering_grid_coord(ay);
	for (int dy = -1; dy <= 1; dy++)
	{
		int ny = cy + dy;
		if (ny < 0 || ny >= GRID_DIM)
			continue;
		for (int dx = -1; dx <= 1; dx++)
		{
			int nx = cx + dx;
			if (nx < 0 || nx >= GRID_DIM)
				continue;
			int cell = ny * GRID_DIM + nx;
			int begin = g_start[cell], end = g_start[cell + 1];
			if (begin < end)
				PREFETCH(&soa.px[g_list[begin]]);
			for (int k = begin; k < end; k++)
			{
				int j = g_list[k];
				if (j == i)
					continue;
				steering_accum_neighbour(ax, ay, soa.px[j], soa.py[j], soa.vx[j], soa.vy[j],
										 R2, S2, &sep_x, &sep_y, &n_sep,
										 &aln_x, &aln_y, &coh_x, &coh_y, &n_nbr);
			}
		}
	}
	return steering_flock(sep_x, sep_y, n_sep, aln_x, aln_y, coh_x, coh_y, n_nbr,
						  ax, ay, avx, avy);
}

void cpu_opt_run_step(Simulation *s)
{
	int N = s->n_agents;
	soa_from_aos(s);
	build_grid(N);

	for (int i = 0; i < N; i++)
	{
		float ax = soa.px[i], ay = soa.py[i];
		float avx = soa.vx[i], avy = soa.vy[i];
		const Goal *g = &s->goals[soa.gi[i]];
		Vec2 acc = steering_forces(boid_forces(i, ax, ay, avx, avy), ax, ay, g->x, g->y,
								   s->obstacles, s->n_obstacles);
		steering_integrate(ax, ay, avx, avy, acc, &new_px[i], &new_py[i], &new_vx[i], &new_vy[i]);
	}
	for (int i = 0; i < N; i++)
	{
		soa.px[i] = new_px[i];
		soa.py[i] = new_py[i];
		soa.vx[i] = new_vx[i];
		soa.vy[i] = new_vy[i];
	}
	soa_to_aos(s);
	simulation_finish_step(s);
}
