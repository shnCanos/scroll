#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include "sway/tree/container.h"
#include <wlr/util/edges.h>

struct sway_scroller_output_options {
	enum sway_container_layout type;
	double default_width;
	double default_height;
	list_t *widths;
	list_t *heights;
};

enum sway_layout_reorder {
	REORDER_AUTO,
	REORDER_LAZY,
};

enum sway_layout_direction {
	DIR_INVALID,
	DIR_LEFT,
	DIR_RIGHT,
	DIR_UP,
	DIR_DOWN,
	DIR_BEGIN,
	DIR_END,
	DIR_CENTER,
	DIR_MIDDLE,
};

enum sway_layout_fit_group {
	FIT_ACTIVE,
	FIT_VISIBLE,
	FIT_ALL,
	FIT_TOEND,
	FIT_TOBEG,
};

enum sway_layout_admit_direction {
	ADMIT_LEFT,
	ADMIT_RIGHT
};

enum sway_layout_insert {
	INSERT_BEFORE,
	INSERT_AFTER,
	INSERT_BEGINNING,
	INSERT_END
};

enum sway_layout_pin {
	PIN_BEGINNING,
	PIN_END
};

struct sway_scroller {
	enum sway_container_layout type;

	struct {
		enum sway_layout_reorder reorder;
		enum sway_container_layout mode;
		enum sway_layout_insert insert;
		bool focus;
		bool center_horizontal;
		bool center_vertical;
	} modifiers;

	bool overview;
	float mem_scale; // Stores the current workspace scale when calling overview, so it can be restored later
	struct sway_container *fullscreen;  // stores the full screen container when calling overview, so it can be restored later

	struct {
		struct sway_container *container;
		enum sway_layout_pin pos;
	} pin;
};

// Global functions
enum wlr_direction layout_to_wlr_direction(enum sway_layout_direction dir);

double layout_get_default_width(struct sway_workspace *workspace);
double layout_get_default_height(struct sway_workspace *workspace);
list_t *layout_get_widths(struct sway_output *output);
list_t *layout_get_heights(struct sway_output *output);

// Functions Specific to layout handling
void layout_init(struct sway_workspace *workspace);
void layout_set_type(struct sway_workspace *workspace, enum sway_container_layout type);
enum sway_container_layout layout_get_type(struct sway_workspace *workspace);

// Toggle overview will only trigger a workspace arrangement where it will call
// layout_overview_recompute_scale()
void layout_overview_toggle(struct sway_workspace *workspace);
// This can be called by the layout to recalculate the overview scale every
// transaction that re-arranges the workspace. It will set it too
void layout_overview_recompute_scale(struct sway_workspace *workspace, int gaps);
// Return true if overview is on
bool layout_overview_enabled(struct sway_workspace *workspace);

void layout_scale_set(struct sway_workspace *workspace, float scale);
void layout_scale_reset(struct sway_workspace *workspace);
float layout_scale_get(struct sway_workspace *workspace);
bool layout_scale_enabled(struct sway_workspace *workspace);

// Mode modifiers
void layout_modifiers_init(struct sway_workspace *workspace);
void layout_modifiers_set_mode(struct sway_workspace *workspace, enum sway_container_layout mode);
void layout_modifiers_set_insert(struct sway_workspace *workspace, enum sway_layout_insert ins);
void layout_modifiers_set_focus(struct sway_workspace *workspace, bool focus);
void layout_modifiers_set_center_horizontal(struct sway_workspace *workspace, bool center);
void layout_modifiers_set_center_vertical(struct sway_workspace *workspace, bool center);
void layout_modifiers_set_reorder(struct sway_workspace *workspace, enum sway_layout_reorder reorder);

enum sway_container_layout layout_modifiers_get_mode(struct sway_workspace *workspace);
enum sway_layout_insert layout_modifiers_get_insert(struct sway_workspace *workspace);
bool layout_modifiers_get_focus(struct sway_workspace *workspace);
bool layout_modifiers_get_center_horizontal(struct sway_workspace *workspace);
bool layout_modifiers_get_center_vertical(struct sway_workspace *workspace);
enum sway_layout_reorder layout_modifiers_get_reorder(struct sway_workspace *workspace);

// Layout API
void layout_add_view(struct sway_workspace *workspace, struct sway_container *active, struct sway_container *view);
void layout_move_container_to_workspace(struct sway_container *container, struct sway_workspace *workspace);
void layout_drag_container_to_workspace(struct sway_container *container, struct sway_workspace *workspace);
void layout_drag_container_to_container(struct sway_container *container, struct sway_container *target,
		struct sway_workspace *workspace, enum wlr_edges edge);

bool layout_move_container(struct sway_container *container, enum sway_layout_direction dir, bool nomode);

void layout_toggle_pin(struct sway_scroller *layout);

void layout_jump();

// Gestures
// Begin scrolling swipe gesture
void layout_scroll_begin(struct sway_seat *seat);
// Update scrolling swipe gesture
void layout_scroll_update(struct sway_seat *seat, double dx, double dy);
// Finish scrolling swipe and return true if scrolling, else false
bool layout_scroll_end(struct sway_seat *seat);

// Pin

// Returns true if a pin is enabled for the workspace. Instead of chasing the pinned
// container to make sure the pin is valid (view unmapped, container changing
// workspace etc.), we verify the pin is still valid using this function in the
// very few places we need to check.
bool layout_pin_enabled(struct sway_workspace *workspace);
void layout_pin_set(struct sway_workspace *workspace, struct sway_container *container, enum sway_layout_pin pos);
// Remove a pin if it exists for the container, otherwise do nothing
void layout_pin_remove(struct sway_workspace *workspace, struct sway_container *container);
struct sway_container *layout_pin_get_container(struct sway_workspace *workspace);
enum sway_layout_pin layout_pin_get_position(struct sway_workspace *workspace);

#endif // _SWAY_LAYOUT_H
