#ifndef _SWAY_ANIMATION_H
#define _SWAY_ANIMATION_H
#include <stdint.h>
#include <stdbool.h>
#include "list.h"

/**
 * Animations.
 */

enum sway_animation_mode {
	ANIM_DISABLED,
	ANIM_DEFAULT,
	ANIM_WINDOW_OPEN,
	ANIM_WINDOW_MOVE,
	ANIM_WINDOW_SIZE,
};


// Animation callback
typedef void (*sway_animation_callback_func_t)(void *data);

struct sway_animation {
	struct wl_event_source *timer;
	uint32_t nsteps;
	uint32_t step;

	enum sway_animation_mode mode;
	sway_animation_callback_func_t callback_step;
	void *data_step;
	sway_animation_callback_func_t callback_end;
	void *data_end;
};


// Set the callbacks for the current animation, and start animating
//
// callback_begin is the function used to prepare anything the animation needs.
//   it will be called before the animation begins, just once, and only if the
//   animation is enabled. The function and data parameter can be NULL if not
//   needed.
//
// callback is the function that will be called at each step of the animation,
//   or once if the animation is disabled. This function cannot be NULL, though
//   its data parameter can be if not needed..
//
// callback_end is the function called when the animation ends. The function
//   and data can be NULL if not needed.
void animation_start(sway_animation_callback_func_t callback_begin, void *data_begin,
	sway_animation_callback_func_t callback, void *data,
	sway_animation_callback_func_t callback_end, void *data_end);

// Create a new animation. Call this before animation_start()
void animation_create(enum sway_animation_mode mode);

// Is an animation enabled?
bool animation_enabled();

// Get the current parameters for the active animation
void animation_get_values(double *t, double *x, double *y, double *offset_scale);

// Create a 3D animation curve
struct sway_animation_curve *create_animation_curve(bool enabled,
		uint32_t duration_ms, uint32_t var_order, list_t *var_points,
		double offset_scale, uint32_t off_order, list_t *off_points);
void destroy_animation_curve(struct sway_animation_curve *curve);

#endif
