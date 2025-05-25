#include "sway/desktop/animation.h"
#include "sway/server.h"
#include "log.h"
#include "util.h"
#include <wayland-server-core.h>


#define NDIM 2

// Size of lookup table
#define NINTERVALS 100

struct bezier_curve {
	uint32_t n;
	double *b[NDIM];
	double u[NINTERVALS + 1];
};

struct sway_animation_curve {
	bool enabled;
	uint32_t duration_ms;
	double offset_scale;
	struct bezier_curve var;
	struct bezier_curve off;
};

static struct sway_animation animation = {
	.timer = NULL,
	.step = 0,
	.mode = ANIM_DEFAULT,
};

static uint32_t comb_n_i(uint32_t n, uint32_t i) {
	if (i > n) {
		return 0;
	}
	int comb = 1;
	for (uint32_t j = n; j > i; --j) {
		comb *= j;
	}
	for (uint32_t j = n - i; j > 1; --j) {
		comb /= j;
	}
	return comb;
}

static double bernstein(uint32_t n, uint32_t i, double t) {
	if (n == 0) {
		return 1.0;
	}
	double B = comb_n_i(n, i) * pow(t, i) * pow(1.0 - t, n - i);
	return B;
}

static void bezier(struct bezier_curve *curve, double t, double (*B)[NDIM]) {
	for (uint32_t d = 0; d < NDIM; ++d) {
		(*B)[d] = 0.0;
	}
	for (uint32_t i = 0; i <= curve->n; ++i) {
		double b = bernstein(curve->n, i, t);
		for (uint32_t d = 0; d < NDIM; ++d) {
			(*B)[d] += curve->b[d][i] * b;
		}
	}
}

static void create_lookup(struct bezier_curve *curve) {
	double B0[NDIM], length = 0.0;
	for (int i = 0; i < NDIM; ++i) {
		B0[i] = 0.0;
	}
	double *T = (double *) malloc(sizeof(double) * (NINTERVALS + 1));
	for (int i = 0; i < NINTERVALS + 1; ++i) {
		double u = (double) i / NINTERVALS;
		double B[NDIM];
		bezier(curve, u, &B);
		double t = 0.0;
		for (int d = 0; d < NDIM; ++d) {
			t += (B[d] - B0[d]) * (B[d] - B0[d]);
		}
		T[i] = sqrt(t);
		length += T[i];
		for (int d = 0; d < NDIM; ++d) {
			B0[d] = B[d];
		}
	}
	// Fill U
	int last = 0;
	double len0 = 0.0, len1 = 0.0;
	double u0 = 0.0;
	for (int i = 0; i < NINTERVALS + 1; ++i) {
		double t = i * length / NINTERVALS;
		while (t > len1) {
			len0 = len1;
			u0 = (double) last / NINTERVALS;
			len1 += T[++last];
		}
		// Interpolate
		if (last == 0) {
			curve->u[i] = 0;
		} else {
			double u1 = (double) last / NINTERVALS;
			double k = (t - len0) / (len1 - len0);
			curve->u[i] = (1.0 - k) * u0 + k * u1;
		}
	}
	free(T);
}

static int timer_callback(void *data) {
	struct sway_animation *animation = data;
	++animation->step;
	if (animation->step <= animation->nsteps) {
		if (animation->callback_step) {
			animation->callback_step(animation->data_step);
		}
		wl_event_source_timer_update(animation->timer, config->animations.frequency_ms);
	} else {
		// This is where we set the one in config if disabled or it not, default
		animation->mode = ANIM_DEFAULT;
		if (animation->callback_end) {
			animation->callback_end(animation->data_end);
		}
	}
	return 0;
}

static uint32_t animation_curve_get_duration_ms(struct sway_animation_curve *curve) {
	if (!curve) {
		curve = config->animations.anim_default;
	}
	if (!curve) {
		return 0;
	}
	return curve->duration_ms;
}


static uint32_t animation_get_duration_ms() {
	if (!config->animations.enabled) {
		return 0;
	}
	switch (animation.mode) {
	case ANIM_DEFAULT:
		return animation_curve_get_duration_ms(config->animations.anim_default);
	case ANIM_WINDOW_OPEN:
		return animation_curve_get_duration_ms(config->animations.window_open);
	case ANIM_WINDOW_MOVE:
		return animation_curve_get_duration_ms(config->animations.window_move);
	case ANIM_WINDOW_SIZE:
		return animation_curve_get_duration_ms(config->animations.window_size);
	case ANIM_DISABLED:
	default:
		return 0;
	}
}

// Set the callback for the current animation, and start animating
void animation_start(sway_animation_callback_func_t callback_begin, void *data_begin,
		sway_animation_callback_func_t callback, void *data,
		sway_animation_callback_func_t callback_end, void *data_end) {
	if (animation_enabled()) {
		animation.nsteps = max(1, animation_get_duration_ms() / config->animations.frequency_ms);
		animation.step = 1;
		if (callback_begin) {
			callback_begin(data_begin);
		}
		callback(data);
		if (animation.timer) {
			wl_event_source_remove(animation.timer);
		}
		animation.callback_step = callback;
		animation.data_step = data;
		animation.callback_end = callback_end;
		animation.data_end = data_end;
		animation.timer = wl_event_loop_add_timer(server.wl_event_loop,
			timer_callback, &animation);
		if (animation.timer) {
			wl_event_source_timer_update(animation.timer, config->animations.frequency_ms);
		} else {
			sway_log_errno(SWAY_ERROR, "Unable to create animation timer");
		}
	} else {
		callback(data);
	}
}

// Create a new animation. Call this before animation_start()
void animation_create(enum sway_animation_mode mode) {
	animation.mode = mode;
}

static bool animation_curve_enabled(struct sway_animation_curve *curve) {
	if (!curve) {
		curve = config->animations.anim_default;
	}
	if (!curve) {
		return false;
	}
	return curve->enabled;
}

// Is an animation enabled?
bool animation_enabled() {
	if (!config->animations.enabled) {
		return false;
	}
	switch (animation.mode) {
	case ANIM_DEFAULT:
		return animation_curve_enabled(config->animations.anim_default);
	case ANIM_WINDOW_OPEN:
		return animation_curve_enabled(config->animations.window_open);
	case ANIM_WINDOW_MOVE:
		return animation_curve_enabled(config->animations.window_move);
	case ANIM_WINDOW_SIZE:
		return animation_curve_enabled(config->animations.window_size);
	case ANIM_DISABLED:
	default:
		return false;
	}
}

static void lookup_xy(struct bezier_curve *curve, double t, double *x, double *y) {
	// Interpolate in the lookup table
	double t0 = floor(t * NINTERVALS);
	double t1 = ceil(t * NINTERVALS);
	double u;
	if (t0 != t1) {
		double u0 = curve->u[(uint32_t) t0];
		double u1 = curve->u[(uint32_t) t1];
		double k = (t * NINTERVALS - t0) / (t1 - t0);
		u = (1.0 - k) * u0 + k * u1;
	} else {
		u = curve->u[(uint32_t) t0];
	}
	double B[NDIM];
	// I could create another lookup table for B
	bezier(curve, u, &B);
	*x = B[0]; *y = B[1];
}

static void animation_curve_get_values(struct sway_animation_curve *curve, double u,
		double *t, double *x, double *y, double *scale) {
	if (!curve) {
		curve = config->animations.anim_default;
	}
	if (!curve || u >= 1.0) {
		*t = 1.0;
		*x = 1.0; *y = 0.0;
		*scale = 0.0;
		return;
	}
	double t_off;
	if (curve->var.n > 0) {
		lookup_xy(&curve->var, u, &t_off, t);
	} else {
		*t = t_off = u;
	}

	// Now use t_off to get offset
	if (t_off >= 1.0) {
		*x = 1.0; *y = 0.0;
		*scale = 0.0;
		return;
	}
	if (curve->off.n > 0) {
		lookup_xy(&curve->off, t_off, x, y);
		*scale = curve->offset_scale;
	} else {
		*x = t_off; *y = 0.0;
		*scale = 0.0;
	}
}

// Get the current parameters for the active animation
void animation_get_values(double *t, double *x, double *y, double *off_scale) {
	if (!animation_enabled()) {
		*t = 1.0; *x = 1.0, *y = 0.0, *off_scale = 0.0;
		return;
	}
	double u = animation.step / (double)animation.nsteps;
	switch (animation.mode) {
	case ANIM_DEFAULT:
		animation_curve_get_values(config->animations.anim_default, u, t, x, y, off_scale);
		break;
	case ANIM_WINDOW_OPEN:
		animation_curve_get_values(config->animations.window_open, u, t, x, y, off_scale);
		break;
	case ANIM_WINDOW_MOVE:
		animation_curve_get_values(config->animations.window_move, u, t, x, y, off_scale);
		break;
	case ANIM_WINDOW_SIZE:
		animation_curve_get_values(config->animations.window_size, u, t, x, y, off_scale);
		break;
	default:
		*t = 1.0; *x = 1.0, *y = 0.0, *off_scale = 0.0;
		break;
	}
}

static void create_bezier(struct bezier_curve *curve, uint32_t order, list_t *points,
		double end[NDIM]) {
	if (points && points->length > 0) {
		curve->n = order;

		for (int d = 0; d < NDIM; ++d) {
		    curve->b[d] = (double *) malloc(sizeof(double) * (curve->n + 1));
			// Set starting point (0, 0,...)
			curve->b[d][0] = 0.0;
		}

		for (uint32_t i = 1, idx = 0; i < curve->n; ++i) {
			for (int d = 0; d < NDIM; ++d) {
				double *x = points->items[idx++];
				curve->b[d][i] = *x;
			}
		}
		// Set end points
		for (int d = 0; d < NDIM; ++d) {
			curve->b[d][curve->n] = end[d];
		}
		create_lookup(curve);
	} else {
		// Use linear parameter
		curve->n = 0;
	}
}

struct sway_animation_curve *create_animation_curve(bool enabled,
		uint32_t duration_ms, uint32_t var_order, list_t *var_points,
		double offset_scale, uint32_t off_order, list_t *off_points) {
	if (enabled && var_points && (uint32_t) var_points->length != NDIM * (var_order - 1)) {
		sway_log(SWAY_ERROR, "Animation curve mismatch: var curve provided %d points, need %d for curve of order %d",
			var_points->length, NDIM * (var_order - 1), var_order);
		return NULL;
	}
	if (enabled && off_points && (uint32_t) off_points->length != NDIM * (off_order - 1)) {
		sway_log(SWAY_ERROR, "Animation curve mismatch: off curve provided %d points, need %d for curve of order %d",
			off_points->length, NDIM * (off_order - 1), off_order);
		return NULL;
	}
	struct sway_animation_curve *curve = (struct sway_animation_curve *) malloc(sizeof(struct sway_animation_curve));
	curve->enabled = enabled;
	curve->duration_ms = duration_ms;
	curve->offset_scale = offset_scale;

	double end_var[2] = { 1.0, 1.0 };
	create_bezier(&curve->var, var_order, var_points, end_var);
	double end_off[2] = { 1.0, 0.0 };
	create_bezier(&curve->off, off_order, off_points, end_off);

	return curve;
}

void destroy_animation_curve(struct sway_animation_curve *curve) {
	if (!curve) {
		return;
	}
	for (int i = 0; i < NDIM; ++i) {
		if (curve->var.n > 0) {
			free(curve->var.b[i]);
		}
		if (curve->off.n > 0) {
			free(curve->off.b[i]);
		}
	}
	free(curve);
}
