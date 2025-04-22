#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/arrange.h"
#include "sway/sway_text_node.h"
#include "sway/input/seat.h"
#include "util.h"
#include <wayland-util.h>
#include "sway/input/keyboard.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "wlr/types/wlr_cursor.h"
#include "sway/ipc-server.h"

enum wlr_direction layout_to_wlr_direction(enum sway_layout_direction dir) {
	switch (dir) {
	case DIR_LEFT:
		return WLR_DIRECTION_LEFT;
	case DIR_RIGHT:
		return WLR_DIRECTION_RIGHT;
	case DIR_UP:
		return WLR_DIRECTION_UP;
	case DIR_DOWN:
		return WLR_DIRECTION_DOWN;
	case DIR_BEGIN:
		return WLR_DIRECTION_LEFT;
	case DIR_END:
		return WLR_DIRECTION_RIGHT;
	default:
		// In case of a user mistake
		return WLR_DIRECTION_RIGHT;
	}
}

double layout_get_default_width(struct sway_workspace *workspace) {
	struct sway_output *output = workspace->output;
	if (output->scroller_options.default_width > 0) {
		return output->scroller_options.default_width;
	}
	return config->layout_default_width;
}

double layout_get_default_height(struct sway_workspace *workspace) {
	struct sway_output *output = workspace->output;
	if (output->scroller_options.default_height > 0) {
		return output->scroller_options.default_height;
	}
	return config->layout_default_height;
}

list_t *layout_get_widths(struct sway_output *output) {
	if (output->scroller_options.widths) {
		return output->scroller_options.widths;
	}
	return config->layout_widths;
}

list_t *layout_get_heights(struct sway_output *output) {
	if (output->scroller_options.heights) {
		return output->scroller_options.heights;
	}
	return config->layout_heights;
}

void layout_init(struct sway_workspace *workspace) {
	layout_modifiers_init(workspace);
	workspace->layout.overview = false;
	workspace->layout.mem_scale = -1.0f;  // disabled
	ipc_event_scroller("new", workspace);
}

void layout_set_type(struct sway_workspace *workspace, enum sway_container_layout type) {
	workspace->layout.type = type;
}

enum sway_container_layout layout_get_type(struct sway_workspace *workspace) {
	return workspace->layout.type;
}

static void buffer_set_dest_size_iterator(struct sway_scene_buffer *buffer,
		int sx, int sy, void *user_data) {
	float *scale = user_data;
	struct sway_scene_surface *scene_surface = sway_scene_surface_try_from_buffer(buffer);
	struct wlr_surface *surface = scene_surface->surface;
	struct wlr_surface_state *state = &surface->current;

	int width = state->width;
	int height = state->height;

	sway_scene_buffer_set_dest_size(buffer, width * *scale, height * *scale);
}

static void recreate_buffers(struct sway_workspace *workspace) {
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	for (int i = 0; i < workspace->tiling->length; ++i) {
		const struct sway_container *con = workspace->tiling->items[i];
		for (int j = 0; j < con->pending.children->length; ++j) {
			struct sway_container *view = con->pending.children->items[j];
			float total_scale = scale;
			if (view_is_content_scaled(view->view)) {
				total_scale *= view_get_content_scale(view->view);
			}
			sway_scene_node_for_each_buffer(&view->content_tree->node,
				buffer_set_dest_size_iterator, &total_scale);
		}
	}
}

void layout_overview_recompute_scale(struct sway_workspace *workspace, int gaps) {
	double w = gaps, maxh = -DBL_MAX;
	for (int i = 0; i < workspace->tiling->length; ++i) {
		const struct sway_container *con = workspace->tiling->items[i];
		w += con->pending.width + 2 * gaps;
		double h = gaps;
		for (int j = 0; j < con->pending.children->length; ++j) {
			const struct sway_container *view = con->pending.children->items[j];
			h += view->pending.height + 2 * gaps;
		}
		if (h > maxh) {
			maxh = h;
		}
	}
	float scale = fmin(workspace->width / w, workspace->height / maxh);
	if (workspace->layers.tiling->node.scale != scale) {
		workspace->layers.tiling->node.scale = scale;
		node_set_dirty(&workspace->node);
		recreate_buffers(workspace);
	}
}

void layout_overview_toggle(struct sway_workspace *workspace) {
	if (workspace->layout.overview) {
		// Disable and restore old scale value
		workspace->layout.overview = false;
		workspace->layers.tiling->node.scale = workspace->layout.mem_scale;
		node_set_dirty(&workspace->node);
		recreate_buffers(workspace);
	} else {
		workspace->layout.mem_scale = layout_scale_get(workspace);
		workspace->layout.overview = true;
		node_set_dirty(&workspace->node);
	}
	ipc_event_scroller("overview", workspace);
}

bool layout_overview_enabled(struct sway_workspace *workspace) {
	return workspace->layout.overview;
}

void layout_scale_set(struct sway_workspace *workspace, float scale) {
	workspace->layers.tiling->node.scale = scale;
	node_set_dirty(&workspace->node);
	recreate_buffers(workspace);
	ipc_event_scroller("scale", workspace);
}

void layout_scale_reset(struct sway_workspace *workspace) {
	workspace->layers.tiling->node.scale = -1.0f;
	node_set_dirty(&workspace->node);
	recreate_buffers(workspace);
	ipc_event_scroller("scale", workspace);
}

float layout_scale_get(struct sway_workspace *workspace) {
	return workspace->layers.tiling->node.scale;
}

bool layout_scale_enabled(struct sway_workspace *workspace) {
	return workspace->layers.tiling->node.scale > 0.0f;
}

void layout_modifiers_init(struct sway_workspace *workspace) {
	struct sway_scroller *layout = &workspace->layout;
	if (workspace && workspace->output) {
		layout->type = output_get_default_layout(workspace->output);
	} else {
		if (config->default_layout != L_NONE) {
			layout->type = config->default_layout;
		} else {
			layout->type = L_HORIZ;
		}
	}
	layout->modifiers.reorder = REORDER_AUTO;
	layout->modifiers.mode = layout->type;
	layout->modifiers.insert = INSERT_AFTER;
	layout->modifiers.focus = true;
	layout->modifiers.center_horizontal = false;
	layout->modifiers.center_vertical = false;
}

void layout_modifiers_set_reorder(struct sway_workspace *workspace, enum sway_layout_reorder reorder) {
	workspace->layout.modifiers.reorder = reorder;
	ipc_event_scroller("reorder", workspace);
}

enum sway_layout_reorder layout_modifiers_get_reorder(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.reorder;
}

void layout_modifiers_set_mode(struct sway_workspace *workspace, enum sway_container_layout mode) {
	workspace->layout.modifiers.mode = mode;
	ipc_event_scroller("mode", workspace);
}

enum sway_container_layout layout_modifiers_get_mode(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.mode;
}

void layout_modifiers_set_insert(struct sway_workspace *workspace, enum sway_layout_insert ins) {
	workspace->layout.modifiers.insert = ins;
	ipc_event_scroller("insert", workspace);
}

enum sway_layout_insert layout_modifiers_get_insert(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.insert;
}

void layout_modifiers_set_focus(struct sway_workspace *workspace, bool focus) {
	workspace->layout.modifiers.focus = focus;
	ipc_event_scroller("focus", workspace);
}

bool layout_modifiers_get_focus(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.focus;
}

void layout_modifiers_set_center_horizontal(struct sway_workspace *workspace, bool center) {
	workspace->layout.modifiers.center_horizontal = center;
	ipc_event_scroller("center_horizontal", workspace);
}

bool layout_modifiers_get_center_horizontal(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.center_horizontal;
}

void layout_modifiers_set_center_vertical(struct sway_workspace *workspace, bool center) {
	workspace->layout.modifiers.center_vertical = center;
	ipc_event_scroller("center_vertical", workspace);
}

bool layout_modifiers_get_center_vertical(struct sway_workspace *workspace) {
	return workspace->layout.modifiers.center_vertical;
}

static int layout_insert_compute_index(list_t *list, void *active, enum sway_layout_insert pos) {
	switch (pos) {
	case INSERT_BEFORE:
	case INSERT_AFTER:
	default: {
		int index = list_find(list, active);
		return pos == INSERT_AFTER ? index + 1 : index;
	}
	case INSERT_BEGINNING:
		return 0;
	case INSERT_END:
		return list->length;
	}
}

static void layout_container_add_view(struct sway_container *active, struct sway_container *view,
			enum sway_container_layout layout, enum sway_layout_insert pos) {
	struct sway_container *parent = active->pending.parent ? active->pending.parent : active;
	struct sway_container *ref = parent == active ? parent->pending.focused_inactive_child : active;
	int index = layout_insert_compute_index(parent->pending.children, ref, pos);
	container_insert_child(parent, view, index);
	view->width_fraction = parent->width_fraction;
	view->height_fraction = parent->height_fraction;
}

// Wraps a view container into a container with layout, and returns it.
static struct sway_container *layout_wrap_into_container(struct sway_container *child,
		enum sway_container_layout layout) {
	struct sway_container *cont = container_create(NULL);
	cont->pending.width = child->pending.width;
	cont->pending.height = child->pending.height;
	cont->width_fraction = child->width_fraction;
	cont->height_fraction = child->height_fraction;
	cont->pending.x = child->pending.x;
	cont->pending.y = child->pending.y;
	cont->pending.layout = layout;
	cont->free_size = child->free_size;

	list_add(cont->pending.children, child);
	child->pending.parent = cont;
	cont->pending.workspace = child->pending.workspace;
	container_update_representation(cont);
	node_set_dirty(&child->node);
	node_set_dirty(&cont->node);
	return cont;
}

static void layout_workspace_add_view(struct sway_workspace *workspace, struct sway_container *active, struct sway_container *view,
			enum sway_layout_insert pos) {
	int idx;
	if (!active) {
		idx = 0;
	} else if (active->pending.parent) {
		// active is in a container, need to insert at the level of its parent
		idx = layout_insert_compute_index(active->pending.workspace->tiling, active->pending.parent, pos);
	} else {
		idx = layout_insert_compute_index(active->pending.workspace->tiling, active, pos);
	}
	// Create a container and add view
	enum sway_container_layout mode = layout_get_type(workspace) == L_HORIZ ? L_VERT : L_HORIZ;
	struct sway_container *parent = layout_wrap_into_container(view, mode);
	// Set the container and view width/height
	if (view->width_fraction <= 0.0) {
		view->width_fraction = layout_get_default_width(workspace);
	}
	parent->width_fraction = view->width_fraction;
	if (view->height_fraction <= 0.0) {
		view->height_fraction = layout_get_default_height(workspace);
	}
	parent->height_fraction = view->height_fraction;
	// Insert the container
	workspace_insert_tiling_direct(workspace, parent, idx);
}

// Layout API
void layout_add_view(struct sway_workspace *workspace, struct sway_container *active, struct sway_container *view) {
	enum sway_layout_insert pos = layout_modifiers_get_insert(workspace);
	enum sway_container_layout mode = layout_modifiers_get_mode(workspace);
	if (layout_get_type(workspace) == mode || workspace->tiling->length == 0) {
		layout_workspace_add_view(workspace, active, view, pos);
	} else {
		layout_container_add_view(active, view, mode, pos);
	}
}

static void layout_workspace_add_container(struct sway_workspace *workspace, struct sway_container *active, struct sway_container *container,
			enum sway_layout_insert pos) {
	int idx;
	if (!active) {
		idx = 0;
	} else if (active->pending.parent) {
		// active is in a container, need to insert at the level of its parent
		idx = layout_insert_compute_index(active->pending.workspace->tiling, active->pending.parent, pos);
	} else {
		idx = layout_insert_compute_index(active->pending.workspace->tiling, active, pos);
	}
	// The container needs to be of the opposite type of the layout type, if not,
	// flip the container layout
	if (layout_get_type(workspace) == container->pending.layout) {
		container->pending.layout = container->pending.layout == L_HORIZ ? L_VERT : L_HORIZ;
	}
	// Insert the container
	workspace_insert_tiling_direct(workspace, container, idx);
}

void layout_move_container_to_workspace(struct sway_container *container, struct sway_workspace *workspace) {
	struct sway_workspace *old_ws = container->pending.workspace;
	// Remove container from old_ws, updating the active if any
	// If we are in layout mode equal to the layout type, we move the whole container,
	// otherwise, we extract the view container and add it to the other workspace,
	// inserting it in the active container (if any), or creating a new one
	enum sway_container_layout mode = layout_modifiers_get_mode(old_ws);
	struct sway_container *active = workspace->current.focused_inactive_child;
	if (layout_get_type(old_ws) == mode) {
		// Move the whole container
		struct sway_container *con = container->view ? container->pending.parent : container;
		int old_idx = list_find(old_ws->tiling, con);
		list_del(old_ws->tiling, old_idx);
		// Update active container in old_ws
		int old_active = old_idx == 0 ? 0 : old_idx - 1;
		old_ws->current.focused_inactive_child = old_ws->tiling->length > 0 ? old_ws->tiling->items[old_active] : NULL;
		node_set_dirty(&old_ws->node);
		// Find insertion point
		enum sway_layout_insert pos = layout_modifiers_get_insert(workspace);
		layout_workspace_add_container(workspace, active, con, pos);
		workspace->current.focused_inactive_child = con;
		node_set_dirty(&con->node);
	} else {
		// Move only the view
		// Extract the view from the parent container
		struct sway_container *parent = container->pending.parent;
		list_t *siblings = parent->pending.children;
		list_del(siblings, list_find(siblings, container));
		container_update_representation(parent);
		node_set_dirty(&parent->node);
		container_reap_empty(parent);
		layout_add_view(workspace, active, container);
		workspace->current.focused_inactive_child = container;
		node_set_dirty(&container->node);
	}
	workspace_update_representation(workspace);
	node_set_dirty(&workspace->node);
}

// Dragging
static void drag_parent_container_to_workspace(struct sway_container *container, struct sway_workspace *workspace) {
	if (container->view) {
		return;
	}
	struct sway_workspace *old_workspace = container->pending.workspace;
	// top level container
	int old_idx = list_find(old_workspace->tiling, container);
	if (old_idx >= 0) {
		// Only if moving a parent container, not if moving a view wrapped into a new container (still not in the tiling list)
		list_del(old_workspace->tiling, old_idx);
		// Update active container
		int old_active = old_idx == 0 ? 0 : old_idx - 1;
		old_workspace->current.focused_inactive_child = old_workspace->tiling->length > 0 ? old_workspace->tiling->items[old_active] : NULL;
	}
	//node_set_dirty(&old_workspace->node);
	if (old_workspace != workspace) {
		arrange_workspace(old_workspace);
	}
	// Find insertion point
	enum sway_layout_insert pos = layout_modifiers_get_insert(workspace);
	struct sway_container *active = workspace->current.focused_inactive_child;
	layout_workspace_add_container(workspace, active, container, pos);
}

// This function inserts
// container into workspace according to the current insertion mode.
// If the node is a top level container, it relocates it. If the node is a view
// container, it is first extracted from its parent container (if it has other
// siblings), and relocated to its own container.
void layout_drag_container_to_workspace(struct sway_container *container, struct sway_workspace *workspace) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	enum sway_container_layout layout = layout_get_type(old_workspace);
	enum sway_container_layout mode = layout_modifiers_get_mode(old_workspace);
	if (mode == layout) {
		container = container->pending.parent;
		drag_parent_container_to_workspace(container, workspace);
	} else {
		// Extract the view from the parent container if needed
		struct sway_container *parent = container->pending.parent;
		list_t *siblings = parent->pending.children;
		if (siblings->length > 1) {
			list_del(siblings, list_find(siblings, container));
			container_update_representation(parent);
			node_set_dirty(&parent->node);
			// Create a new container for extracted child
			parent = layout_wrap_into_container(container, parent->pending.layout);
		}
		node_set_dirty(&parent->node);
		drag_parent_container_to_workspace(parent, workspace);
	}
}

static void remove_active_parent_container_from_workspace(int idx, struct sway_workspace *workspace) {
	list_del(workspace->tiling, idx);
	// Update active container
	int active = idx == 0 ? 0 : idx - 1;
	workspace->current.focused_inactive_child = workspace->tiling->length > 0 ? workspace->tiling->items[active] : NULL;
	workspace_update_representation(workspace);
	node_set_dirty(&workspace->node);
}

static void move_container_before(struct sway_container *container, struct sway_container *target,
		struct sway_workspace *workspace) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	int cidx = list_find(old_workspace->tiling, container);
	int tidx = list_find(workspace->tiling, target);
	if (cidx < 0 || tidx < 0) {
		// Shouldn't happen, but just in case
		return;
	}
	if (old_workspace != workspace) {
		remove_active_parent_container_from_workspace(cidx, old_workspace);
		workspace_insert_tiling_direct(workspace, container, tidx);
		return;
	}
	// Move container
	if (cidx == tidx - 1) {
		// Correct position
		return;
	}
	if (cidx < tidx) {
		list_move_to(workspace->tiling, tidx - 1, container);
	} else {
		list_move_to(workspace->tiling, tidx, container);
	}
}

static void move_container_after(struct sway_container *container, struct sway_container *target,
		struct sway_workspace *workspace) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	int cidx = list_find(old_workspace->tiling, container);
	int tidx = list_find(workspace->tiling, target);
	if (cidx < 0 || tidx < 0) {
		// Shouldn't happen, but just in case
		return;
	}
	if (old_workspace != workspace) {
		remove_active_parent_container_from_workspace(cidx, old_workspace);
		workspace_insert_tiling_direct(workspace, container, tidx + 1);
		return;
	}
	// Move container
	if (cidx == tidx + 1) {
		// Correct position
		return;
	}
	if (cidx < tidx) {
		list_move_to(workspace->tiling, tidx, container);
	} else {
		list_move_to(workspace->tiling, tidx + 1, container);
	}
}

static void swap_containers(struct sway_container *container, struct sway_container *target,
		struct sway_workspace *workspace) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	int cidx = list_find(old_workspace->tiling, container);
	int tidx = list_find(workspace->tiling, target);
	if (cidx < 0 || tidx < 0) {
		// Shouldn't happen, but just in case
		return;
	}
	if (old_workspace != workspace) {
		remove_active_parent_container_from_workspace(cidx, old_workspace);
		remove_active_parent_container_from_workspace(tidx, workspace);
		workspace_insert_tiling_direct(old_workspace, target, cidx);
		workspace_insert_tiling_direct(workspace, container, tidx);
		return;
	}
	// Move container
	if (cidx == tidx) {
		// Correct position
		return;
	}
	list_swap(workspace->tiling, cidx, tidx);
}

static void insert_children_before(struct sway_container *container, struct sway_container *target) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	int cidx = list_find(old_workspace->tiling, container);
	remove_active_parent_container_from_workspace(cidx, old_workspace);
	list_t *children = container->pending.children;
	while (children->length > 0) {
		struct sway_container *con = children->items[children->length - 1];
		con->pending.parent = target;
		con->pending.workspace = target->pending.workspace;
		list_insert(target->pending.children, 0, con);
		list_del(children, children->length - 1);
	}
	container_reap_empty(container);
	container_update_representation(target);
	node_set_dirty(&target->node);
}

static void insert_children_after(struct sway_container *container, struct sway_container *target) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	int cidx = list_find(old_workspace->tiling, container);
	remove_active_parent_container_from_workspace(cidx, old_workspace);
	list_t *children = container->pending.children;
	while (children->length > 0) {
		struct sway_container *con = children->items[0];
		con->pending.parent = target;
		con->pending.workspace = target->pending.workspace;
		list_add(target->pending.children, con);
		list_del(children, 0);
	}
	container_reap_empty(container);
	container_update_representation(target);
	node_set_dirty(&target->node);
}

static struct sway_container *extract_view(struct sway_container *view) {
	struct sway_container *parent = view->pending.parent;
	list_t *siblings = parent->pending.children;
	int idx = list_find(siblings, view);
	list_del(siblings, idx);
	if (parent->pending.focused_inactive_child == view && siblings->length > 0) {
		parent->pending.focused_inactive_child = siblings->items[idx >= siblings->length ? idx - 1 : idx];
	}
	container_update_representation(parent);
	node_set_dirty(&parent->node);
	container_reap_empty(parent);
	return view;
}

// Extract view container from parent and insert into target's parent, before target,
// possibly removing container's parent if empty
static void insert_view_before(struct sway_container *container, struct sway_container *target) {
	container = extract_view(container);
	container->pending.workspace = target->pending.workspace;
	struct sway_container *parent = target->pending.parent;
	int tidx = list_find(parent->pending.children, target);
	list_insert(parent->pending.children, tidx, container);
	container->pending.parent = parent;
	parent->pending.focused_inactive_child = container;
	container_update_representation(parent);
	node_set_dirty(&parent->node);
}
//
// Extract view container from parent and insert into target's parent, after target,
// possibly removing container's parent if empty
static void insert_view_after(struct sway_container *container, struct sway_container *target) {
	container = extract_view(container);
	container->pending.workspace = target->pending.workspace;
	struct sway_container *parent = target->pending.parent;
	int tidx = list_find(parent->pending.children, target);
	list_insert(parent->pending.children, tidx + 1, container);
	container->pending.parent = parent;
	parent->pending.focused_inactive_child = container;
	container_update_representation(parent);
	node_set_dirty(&parent->node);
}

static void extract_view_and_insert_before(struct sway_container *container,
			struct sway_container *reference, struct sway_workspace *workspace) {
	container = extract_view(container);
	container->pending.workspace = workspace;
	container = layout_wrap_into_container(container, layout_get_type(workspace) == L_HORIZ ? L_VERT : L_HORIZ);
	int idx = list_find(workspace->tiling, reference);
	list_insert(workspace->tiling, idx, container);
	node_set_dirty(&container->node);
}

static void extract_view_and_insert_after(struct sway_container *container,
			struct sway_container *reference, struct sway_workspace *workspace) {
	container = extract_view(container);
	container->pending.workspace = workspace;
	container = layout_wrap_into_container(container, layout_get_type(workspace) == L_HORIZ ? L_VERT : L_HORIZ);
	int idx = list_find(workspace->tiling, reference);
	list_insert(workspace->tiling, idx + 1, container);
	node_set_dirty(&container->node);
}

// Swap views in possibly different containers of possibly different workspaces
static void	swap_views(struct sway_container *container, struct sway_container *target) {
	struct sway_container *cparent = container->pending.parent;
	struct sway_container *tparent = target->pending.parent;
	int cidx = list_find(cparent->pending.children, container);
	int tidx = list_find(tparent->pending.children, target);
	if (cparent == tparent) {
		list_swap(cparent->pending.children, cidx, tidx);
		container_update_representation(cparent);
		node_set_dirty(&cparent->node);
		return;
	}
	struct sway_workspace *cworkspace = container->pending.workspace;
	struct sway_workspace *tworkspace = target->pending.workspace;
	list_del(cparent->pending.children, cidx);
	list_del(tparent->pending.children, tidx);
	list_insert(cparent->pending.children, cidx, target);
	list_insert(tparent->pending.children, tidx, container);
	container->pending.parent = tparent;
	target->pending.parent = cparent;
	target->pending.workspace = cworkspace;
	container->pending.workspace = tworkspace;
	if (cparent->pending.focused_inactive_child == container) {
		cparent->pending.focused_inactive_child = target;
	}
	if (tparent->pending.focused_inactive_child == target) {
		tparent->pending.focused_inactive_child = container;
	}
	container_update_representation(cparent);
	node_set_dirty(&cparent->node);
	container_update_representation(tparent);
	node_set_dirty(&tparent->node);
}

// Move a container into/near target according to edge
// 1. If container is a top level container, if edge is parallel to layout type,
// insert every children of container into target, and remove container.
// 2. If container is a top level container and edge is not parallel to layout
// type, move container to the new position.
// 3. If container is a view container:
// 3.1 if edge is parallel to layout type, extract container from its parent
// and insert it into target, possibly removing the old parent if empty
// 3.2 if edge is not parallel, extract container from its parent, create a new
// parent container and move it to the correct location.
//
void layout_drag_container_to_container(struct sway_container *container, struct sway_container *target,
		struct sway_workspace *workspace, enum wlr_edges edge) {
	struct sway_workspace *old_workspace = container->pending.workspace;
	bool move_parent = layout_get_type(old_workspace) == layout_modifiers_get_mode(old_workspace);
	enum sway_container_layout layout = layout_get_type(old_workspace);
	if (move_parent) {
		// Get parent (top level container)
		container = container->pending.parent;
		if (target->view) {
			target = target->pending.parent;
		}
		if (container == target) {
			return;
		}
		if (layout == L_HORIZ) {
			switch (edge) {
			case WLR_EDGE_TOP:
				// Parallel
				insert_children_before(container, target);
				break;
			case WLR_EDGE_BOTTOM:
				// Parallel
				insert_children_after(container, target);
				break;
			case WLR_EDGE_LEFT:
				// Move container
				move_container_before(container, target, workspace);
				break;
			case WLR_EDGE_RIGHT:
				// Move container
				move_container_after(container, target, workspace);
				break;
			case WLR_EDGE_NONE:
				// Swap containers
				swap_containers(container, target, workspace);
				break;
			}
		} else {
			switch (edge) {
			case WLR_EDGE_TOP:
				// Move container
				move_container_before(container, target, workspace);
				break;
			case WLR_EDGE_BOTTOM:
				// Move container
				move_container_after(container, target, workspace);
				break;
			case WLR_EDGE_LEFT:
				// Parallel
				insert_children_before(container, target);
				break;
			case WLR_EDGE_RIGHT:
				// Parallel
				move_container_after(container, target, workspace);
				break;
			case WLR_EDGE_NONE:
				// Swap containers
				swap_containers(container, target, workspace);
				break;
			}
		}
	} else {
		if (container == target) {
			return;
		}
		// Check if parent can be a top level container...???
		if (layout == L_HORIZ) {
			switch (edge) {
			case WLR_EDGE_TOP:
				// Parallel
				insert_view_before(container, target);
				break;
			case WLR_EDGE_BOTTOM:
				// Parallel
				insert_view_after(container, target);
				break;
			case WLR_EDGE_LEFT:
				// Move container
				extract_view_and_insert_before(container, target->pending.parent, workspace);
				break;
			case WLR_EDGE_RIGHT:
				// Move container
				extract_view_and_insert_after(container, target->pending.parent, workspace);
				break;
			case WLR_EDGE_NONE:
				// Swap views
				swap_views(container, target);
				break;
			}
		} else {
			switch (edge) {
			case WLR_EDGE_TOP:
				// Move container
				extract_view_and_insert_before(container, target->pending.parent, workspace);
				break;
			case WLR_EDGE_BOTTOM:
				// Move container
				extract_view_and_insert_after(container, target->pending.parent, workspace);
				break;
			case WLR_EDGE_LEFT:
				// Parallel
				insert_view_before(container, target);
				break;
			case WLR_EDGE_RIGHT:
				// Parallel
				insert_view_after(container, target);
				break;
			case WLR_EDGE_NONE:
				// Swap views
				swap_views(container, target);
				break;
			}
		}
	}
	arrange_workspace(workspace);
	workspace_update_representation(workspace);
	node_set_dirty(&workspace->node);
}

void layout_cycle_width(struct sway_scroller *layout, int step) {

}

void layout_cycle_height(struct sway_scroller *layout, int step) {

}

void layout_set_width(struct sway_scroller *layout, double fraction) {

}

void layout_set_height(struct sway_scroller *layout, double fraction) {

}

void layout_fit_width(struct sway_scroller *layout, enum sway_layout_fit_group group) {

}

void layout_fit_height(struct sway_scroller *layout, enum sway_layout_fit_group group) {

}
#if 0
static bool layout_is_parallel(enum sway_container_layout layout,
		enum sway_layout_direction dir) {
	switch (layout) {
	case L_HORIZ:
		return dir == DIR_LEFT || dir == DIR_RIGHT || dir == DIR_BEGIN || dir == DIR_END;
	case L_VERT:
		return dir == DIR_UP || dir == DIR_DOWN || dir == DIR_BEGIN || dir == DIR_END;
	default:
		return false;
	}
}
#endif

static int layout_direction_compute_index(list_t *list, void *item,
		enum sway_layout_direction dir, bool detach) {
	int index;
	switch (dir) {
	case DIR_LEFT:
	case DIR_UP:
	case DIR_RIGHT:
	case DIR_DOWN:
	default: {
		index = list_find(list, item);
		return dir == DIR_RIGHT || dir == DIR_DOWN ? index + 1 : detach ? index : index - 1;
	}
	case DIR_BEGIN:
		return 0;
	case DIR_END:
		return list->length - 1;
	}
}

static bool need_to_detach(enum sway_container_layout parent_layout, enum sway_layout_direction dir) {
	if (dir == DIR_LEFT || dir == DIR_RIGHT) {
		if (parent_layout == L_HORIZ) {
			return false;
		}
	} else if (dir == DIR_UP || dir == DIR_DOWN) {
		if (parent_layout == L_VERT) {
			return false;
		}
	} else if (dir == DIR_BEGIN || dir == DIR_END) {
		return false;
	}
	return true;
}

// Detaches a container from its parent container and wraps it into a container
static struct sway_container *layer_container_detach(struct sway_container *child) {
	struct sway_container *parent = child->pending.parent;
	list_t *siblings = parent->pending.children;
	list_del(siblings, list_find(siblings, child));
	struct sway_container *new_parent = layout_wrap_into_container(child, parent->pending.layout);
	container_update_representation(parent);
	node_set_dirty(&parent->node);
	return new_parent;
}

// Inserts a top level orphan container (view) into a container
static void layout_insert_into_container(struct sway_container *parent, struct sway_container *child, int idx) {
	list_insert(parent->pending.children, idx, child);
	child->pending.parent = parent;
	child->pending.workspace = parent->pending.workspace;
	container_update_representation(parent);
	node_set_dirty(&child->node);
	node_set_dirty(&parent->node);
}

static bool layout_move_container_nomode(struct sway_container *container, enum sway_layout_direction dir) {
	// Don't allow containers to move out of their fullscreen or floating parent
	if (container->pending.fullscreen_mode || container_is_floating(container)) {
		return false;
	}

	struct sway_container *parent = container->pending.parent;
	enum sway_container_layout parent_layout = container_parent_layout(container);
	struct sway_workspace *workspace = container->pending.workspace;

	if (parent->pending.children->length > 1) {
		// If container has siblings, detach and insert at the top level using dir.
		list_t *children;
		int new_index;
		if (need_to_detach(parent_layout, dir)) {
			// Here, moving to the left should return parent's index (parent will be afer)
			// Moving to the right should return parent's index + 1
			new_index = max(layout_direction_compute_index(workspace->tiling, parent, dir, true), 0);
			container = layer_container_detach(container);
			children = workspace->tiling;
			list_insert(children, new_index, container);
			node_set_dirty(&workspace->node);
		} else {
			children = parent->pending.children;
			new_index = layout_direction_compute_index(children, container, dir, false);
			if (new_index < 0 || new_index >= children->length) {
				return false;
			}
			list_move_to(children, new_index, container);
			node_set_dirty(&parent->node);
		}
	} else {
		// If beginning or end, move parent container to location
		// Compute neighbor
		int neighbor_index = layout_direction_compute_index(workspace->tiling, parent, dir, false);
		if (neighbor_index < 0 || neighbor_index >= workspace->tiling->length) {
			return false;
		}
		struct sway_container *new_parent = workspace->tiling->items[neighbor_index];
		if (new_parent == parent) {
			return false;
		}
		int index = list_find(workspace->tiling, parent);
		if (dir == DIR_BEGIN || dir == DIR_END) {
			list_swap(workspace->tiling, neighbor_index, index);
		} else {
			list_del(workspace->tiling, index);
			// Remove container from old parent
			list_del(parent->pending.children, 0);
			// Insert container into neighbor
			enum sway_layout_insert pos = layout_modifiers_get_insert(workspace);
			struct sway_container *active = new_parent->current.focused_inactive_child;
			int new_index = layout_insert_compute_index(new_parent->pending.children, active, pos);
			layout_insert_into_container(new_parent, container, new_index);
			// Delete old parent container
			container_reap_empty(parent);
		}
		node_set_dirty(&workspace->node);
	}
	return true;
}

static bool layout_move_container_mode(struct sway_container *container, enum sway_layout_direction dir) {
	// Look for a suitable ancestor of the container to move within
	struct sway_container *current = container;

	while (current) {
		// Don't allow containers to move out of their fullscreen or floating parent
		if (current->pending.fullscreen_mode || container_is_floating(current)) {
			return false;
		}

		bool can_move = false;
		int desired;
		int idx = container_sibling_index(current);
		enum sway_container_layout parent_layout = current->pending.parent ?
			container_parent_layout(current) : layout_get_type(current->pending.workspace);
		list_t *siblings = container_get_siblings(current);

		if (dir == DIR_LEFT || dir == DIR_RIGHT) {
			if (parent_layout == L_HORIZ) {
				can_move = true;
				desired = idx + (dir == DIR_LEFT ? -1 : 1);
			}
		} else if (dir == DIR_UP || dir == DIR_DOWN) {
			if (parent_layout == L_VERT) {
				can_move = true;
				desired = idx + (dir == DIR_UP ? -1 : 1);
			}
		} else if (dir == DIR_BEGIN || dir == DIR_END) {
			// We move the container within this container if the layout mode is the
			// same as the container's
			struct sway_workspace *workspace = config->handler_context.workspace;
			enum sway_container_layout mode = layout_modifiers_get_mode(workspace);
			if (mode == parent_layout) {
				can_move = true;
				desired = dir == DIR_BEGIN ? 0 : siblings->length - 1;
			}
		}
		
		if (can_move && siblings->length > 1) {
			if (desired >= 0 && desired < siblings->length) {
				list_del(siblings, idx);
				list_insert(siblings, desired, current);
				if (current->pending.parent) {
					node_set_dirty(&current->pending.parent->node);
					node_set_dirty(&current->node);
				} else {
					node_set_dirty(&current->pending.workspace->node);
				}
				return true;
			}
		}

		current = current->pending.parent;
	}

	return false;
}

bool layout_move_container(struct sway_container *container, enum sway_layout_direction dir, bool nomode) {
	if (nomode) {
		return layout_move_container_nomode(container, dir);
	} else {
		return layout_move_container_mode(container, dir);
	}
}

void layout_toggle_pin(struct sway_scroller *layout) {

}

static void container_toggle_jump_decoration(struct sway_container *con, char *text) {
	if (!text) {
		if (con->jump.text) {
			sway_scene_node_destroy(con->jump.text->node);
			con->jump.text = NULL;
		}
		return;
	} else if (!con->jump.text) {
		con->jump.text = sway_text_node_create(con->jump.tree,
			text, config->jump_labels_color, false);
	} else {
		sway_text_node_set_text(con->jump.text, text);
	}
	sway_text_node_set_background(con->jump.text, config->jump_labels_background);
	double jscale = config->jump_labels_scale;
	struct sway_workspace *workspace = con->pending.workspace;
	float wscale = workspace && layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	float scale = fmin(con->pending.width / con->jump.text->width, con->pending.height / con->jump.text->height);
	sway_text_node_scale(con->jump.text, jscale * scale * wscale);
	int x = 0.5 * wscale * (con->pending.width - con->jump.text->width * jscale * scale);
	int y = 0.5 * wscale * (con->pending.height - con->jump.text->height * jscale * scale);
	sway_scene_node_set_position(&con->jump.tree->node, x, y);
	sway_scene_node_set_enabled(&con->jump.tree->node, true);
	node_set_dirty(&con->node);
}

struct jump_data {
	list_t *workspaces;
	bool jumping;
	uint32_t keys_pressed;
	uint32_t nkeys;
	uint32_t window_number;
	uint32_t nwindows;
};

static void jump_handle_keyboard_key(struct sway_keyboard *keyboard, struct wlr_keyboard_key_event *event, void *data);

static void override_keyboard(bool override, void *data) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_seat_device *seat_device, *next;
	wl_list_for_each_safe(seat_device, next, &seat->devices, link) {
		struct sway_keyboard *keyboard = seat_device->keyboard;
		if (keyboard == NULL) {
			continue;
		}
		if (override) {
			sway_keyboard_set_keypress_cb(keyboard, jump_handle_keyboard_key, data);
		} else {
			sway_keyboard_set_keypress_cb(keyboard, NULL, NULL);
		}
	}
	struct sway_keyboard_group *keyboard_group, *next_kg;
	wl_list_for_each_safe(keyboard_group, next_kg, &seat->keyboard_groups, link) {
		if (override) {
			sway_keyboard_set_keypress_cb(keyboard_group->seat_device->keyboard, jump_handle_keyboard_key, data);
		} else {
			sway_keyboard_set_keypress_cb(keyboard_group->seat_device->keyboard, NULL, NULL);
		}
	}
}

static char *generate_label(uint32_t i, const char *keys, uint32_t nkeys) {
	int ksize = strlen(keys);
	char *label = malloc((nkeys + 1) * sizeof(char));
	for (uint32_t n = 0, div = i; n < nkeys; ++n) {
		uint32_t rem = div % ksize;
		label[nkeys - 1 -n] = keys[rem];
		div = div / ksize;
	}
	label[nkeys] = 0x0;
	return label;
}

static void jump_handle_keyboard_key(struct sway_keyboard *keyboard,
		struct wlr_keyboard_key_event *event, void *data) {
	struct jump_data *jump_data = data;

	uint32_t keycode = event->keycode + 8; // Because to xkbcommon it's +8 from libinput
	const xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->wlr->xkb_state, keycode);

	if (event->state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}
	// Check if key is valid, otherwise exit
	bool valid = false;
	for (uint32_t i = 0; i < strlen(config->jump_labels_keys); ++i) {
		char keyname[2] = { config->jump_labels_keys[i], 0x0 };
		xkb_keysym_t key = xkb_keysym_from_name(keyname, XKB_KEYSYM_NO_FLAGS);
		if (key && key == keysym) {
			jump_data->window_number = jump_data->window_number * strlen(config->jump_labels_keys) + i;
			valid = true;
			break;
		}
	}
	bool focus = false;
	if (valid) {
		jump_data->keys_pressed++;
		if (jump_data->keys_pressed == jump_data->nkeys) {
			if (jump_data->window_number < jump_data->nwindows) {
				focus = true;
			}
		} else {
			return;
		}
	}

	// Finished, remove decorations
	for (int w = 0; w < jump_data->workspaces->length; ++w) {
		struct sway_workspace *workspace = jump_data->workspaces->items[w];
		for (int i = 0; i < workspace->tiling->length; ++i) {
			struct sway_container *container = workspace->tiling->items[i];
			for (int j = 0; j < container->pending.children->length; ++j) {
				struct sway_container *view = container->pending.children->items[j];
				container_toggle_jump_decoration(view, NULL);
			}
		}
		// Restore original view
		layout_overview_toggle(workspace);
	}
	transaction_commit_dirty();

	if (focus) {
		uint32_t n = 0;
		for (int w = 0; w < jump_data->workspaces->length; ++w) {
			struct sway_workspace *workspace = jump_data->workspaces->items[w];
			for (int i = 0; i < workspace->tiling->length; ++i) {
				struct sway_container *container = workspace->tiling->items[i];
				if (n + container->pending.children->length > jump_data->window_number) {
					struct sway_container *view = container->pending.children->items[jump_data->window_number - n];
					struct sway_seat *seat = input_manager_current_seat();
					seat_set_focus_container(seat, view);
					seat_consider_warp_to_focus(seat);
					goto cleanup;
				} else {
					n += container->pending.children->length;
				}
			}
		}
	}

cleanup:
	list_free(jump_data->workspaces);
	free(jump_data);
	transaction_commit_dirty();
	override_keyboard(false, NULL);
}

void layout_jump() {
	struct jump_data *jump_data = calloc(1, sizeof(struct jump_data));
	jump_data->workspaces = create_list();

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		struct sway_workspace *workspace = output->current.active_workspace;
		if (workspace->tiling->length > 0) {
			list_add(jump_data->workspaces, workspace);
		}
	}
	if (jump_data->workspaces->length == 0) {
		list_free(jump_data->workspaces);
		free(jump_data);
		return;
	}

	uint32_t nwindows = 0;
	for (int i = 0; i < jump_data->workspaces->length; ++i) {
		struct sway_workspace *workspace = jump_data->workspaces->items[i];
		if (!layout_overview_enabled(workspace)) {
			layout_overview_toggle(workspace);
		}
		for (int i = 0; i < workspace->tiling->length; ++i) {
			struct sway_container *container = workspace->tiling->items[i];
			nwindows += container->pending.children->length;
		}
	}
	transaction_commit_dirty();
	uint32_t nkeys = nwindows == 1 ? 1 : ceil(log10(nwindows) / log10(strlen(config->jump_labels_keys)));
	jump_data->nwindows = nwindows;
	jump_data->nkeys = nkeys;

	for (int i = 0, n = 0; i < jump_data->workspaces->length; ++i) {
		struct sway_workspace *workspace = jump_data->workspaces->items[i];
		for (int i = 0; i < workspace->tiling->length; ++i) {
			struct sway_container *container = workspace->tiling->items[i];
			for (int j = 0; j < container->pending.children->length; ++j) {
				struct sway_container *view = container->pending.children->items[j];
				char *label = generate_label(n++, config->jump_labels_keys, nkeys);
				container_toggle_jump_decoration(view, label);
				free(label);
			}
		}
	}
	override_keyboard(true, jump_data);
}

// When the workspace is scaled, offsets are not valid to check cursor or bounds,
// because offsets and sizes are not scaled. We need to re-compute them using the
// active offset as the only ground truth, with everything else relative to it.

static struct sway_container *get_mouse_container(struct sway_seat *seat) {
	struct sway_workspace *workspace = seat->workspace;
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	struct sway_container *container = workspace->current.focused_inactive_child;
	enum sway_container_layout layout = layout_get_type(workspace);
	int active_idx = list_find(workspace->tiling, container);
	if (layout == L_HORIZ) {
		double offset = container->pending.x;
		for (int i = active_idx; i < workspace->tiling->length; ++i) {
			struct sway_container *con = workspace->tiling->items[i];
			double x0 = offset - scale * workspace->gaps_inner;
			double x1 = x0 + scale * (con->pending.width + 2.0 * workspace->gaps_inner);
			if (seat->cursor->cursor->x >= x0 && seat->cursor->cursor->x < x1) {
				return con;
			}
			offset = x1;
		}
		offset = container->pending.x;
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *con = workspace->tiling->items[i];
			double x1 = offset - scale * workspace->gaps_inner;
			double x0 = x1 - scale * (con->pending.width + 2.0 * workspace->gaps_inner);
			if (seat->cursor->cursor->x >= x0 && seat->cursor->cursor->x < x1) {
				return con;
			}
			offset = x0;
		}
	} else {
		double offset = container->pending.y;
		for (int i = active_idx; i < workspace->tiling->length; ++i) {
			struct sway_container *con = workspace->tiling->items[i];
			double y0 = offset - scale * workspace->gaps_inner;
			double y1 = y0 + scale * (con->pending.height + 2.0 * workspace->gaps_inner);
			if (seat->cursor->cursor->y >= y0 && seat->cursor->cursor->y < y1) {
				return con;
			}
			offset = y1;
		}
		offset = container->pending.y;
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *con = workspace->tiling->items[i];
			double y1 = offset - scale * workspace->gaps_inner;
			double y0 = y1 - scale * (con->pending.height + 2.0 * workspace->gaps_inner);
			if (seat->cursor->cursor->y >= y0 && seat->cursor->cursor->y < y1) {
				return con;
			}
			offset = y0;
		}
	}
	return container;
}

static void scroll_workspace(struct sway_workspace *workspace, double dx, double dy) {
	if (layout_get_type(workspace) == L_HORIZ) {
		for (int i = 0; i < workspace->tiling->length; ++i) {
			struct sway_container *con = workspace->tiling->items[i];
			con->pending.x += dx;
		}
	} else {
		for (int i = 0; i < workspace->tiling->length; ++i) {
			struct sway_container *con = workspace->tiling->items[i];
			con->pending.y += dy;
		}
	}
	node_set_dirty(&workspace->node);
	transaction_commit_dirty();
}

static void scroll_container(struct sway_container *container, double dx, double dy) {
	if (container->pending.layout == L_HORIZ) {
		list_t *children = container->pending.children;
		for (int i = 0; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
			con->pending.x += dx;
		}
	} else {
		list_t *children = container->pending.children;
		for (int i = 0; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
			con->pending.y += dy;
		}
	}
	node_set_dirty(&container->node);
	transaction_commit_dirty();
}

// Gestures
void layout_scroll_begin(struct sway_seat *seat) {
	seat->workspace->gesture.scrolling = true;
	seat->workspace->gesture.dx = 0.0;
	seat->workspace->gesture.dy = 0.0;
}

void layout_scroll_update(struct sway_seat *seat, double dx, double dy) {
	struct sway_workspace *workspace = seat->workspace;
	if (workspace->tiling->length == 0) {
		return;
	}
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	dx *= config->gesture_scroll_sentitivity * scale;
	dy *= config->gesture_scroll_sentitivity * scale;

	enum sway_layout_direction scrolling_direction;
	if (fabs(dx) > fabs(dy)) {
		scrolling_direction = dx > 0.0 ? DIR_RIGHT : DIR_LEFT;
		workspace->gesture.dx += dx;
	} else {
		scrolling_direction = dy > 0.0 ? DIR_DOWN : DIR_UP; 
		workspace->gesture.dy += dy;
	}

	switch (scrolling_direction) {
	case DIR_UP:
	case DIR_DOWN:
		if (layout_get_type(workspace) == L_HORIZ) {
			scroll_container(get_mouse_container(seat), dx, dy);
		} else {
			scroll_workspace(workspace, dx, dy);
		}
		break;
	case DIR_LEFT:
	case DIR_RIGHT:
		if (layout_get_type(workspace) == L_HORIZ) {
			scroll_workspace(workspace, dx, dy);
		} else {
			scroll_container(get_mouse_container(seat), dx, dy);
		}
		break;
	default:
		break;
	}
}

static void scroll_end_horizontal(struct sway_seat *seat, list_t *children, int active_idx,
		enum sway_layout_direction scrolling_direction) {
	struct sway_container *active = children->items[active_idx];
	struct sway_workspace *workspace = active->pending.workspace;
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	if (scrolling_direction == DIR_LEFT) {
		struct sway_container *new_active = children->items[children->length - 1];
		double offset = active->pending.x + scale * (active->pending.width + 2.0 * workspace->gaps_inner);
		// Take the first after active that has its left edge in the viewport
		for (int i = active_idx + 1; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
			double x0 = offset;
			if (x0 > 0 && x0 < workspace->width) {
				new_active = con;
				break;
			}
			offset += scale * (con->pending.width + 2.0 * workspace->gaps_inner);
		}
		seat_set_focus_container(seat, new_active);
    } else if (scrolling_direction == DIR_RIGHT) {
		struct sway_container *new_active = children->items[0];
		double offset = active->pending.x - 2.0 * scale * workspace->gaps_inner;
		// Take the first before active that has its right edge in the viewport
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *con = children->items[i];
			double x1 = offset;
			if (x1 > 0 && x1 < workspace->width) {
				new_active = con;
				break;
			}
			offset -= scale * (con->pending.width + 2.0 * workspace->gaps_inner);
		}
		seat_set_focus_container(seat, new_active);
	}
}

static void scroll_end_vertical(struct sway_seat *seat, list_t *children, int active_idx,
		enum sway_layout_direction scrolling_direction) {
	struct sway_container *active = children->items[active_idx];
	struct sway_workspace *workspace = active->pending.workspace;
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	if (scrolling_direction == DIR_UP) {
		struct sway_container *new_active = children->items[children->length - 1];
		double offset = active->pending.y + scale * (active->pending.height + 2.0 * workspace->gaps_inner);
		// Take the first after active that has its top edge in the viewport
		for (int i = active_idx + 1; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
			double y0 = offset;
			if (y0 > 0 && y0 < workspace->height) {
				new_active = con;
				break;
			}
			offset += scale * (con->pending.height + 2.0 * workspace->gaps_inner);
		}
		seat_set_focus_container(seat, new_active);
    } else if (scrolling_direction == DIR_DOWN) {
		struct sway_container *new_active = children->items[0];
		double offset = active->pending.y - 2.0 * scale * workspace->gaps_inner;
		// Take the first before active that has its bottom edge in the viewport
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *con = children->items[i];
			double y1 = offset;
			if (y1 > 0 && y1 < workspace->height) {
				new_active = con;
				break;
			}
			offset -= scale * (con->pending.height + 2.0 * workspace->gaps_inner);
		}
		seat_set_focus_container(seat, new_active);
	}
}

bool layout_scroll_end(struct sway_seat *seat) {
	struct sway_workspace *workspace = seat->workspace;
	if (!workspace->gesture.scrolling) {
		return false;
	}
	workspace->gesture.scrolling = false;
	enum sway_layout_direction scrolling_direction;
	enum sway_container_layout layout = layout_get_type(workspace);
	if (fabs(workspace->gesture.dx) > fabs(workspace->gesture.dy)) {
		scrolling_direction = workspace->gesture.dx > 0.0 ? DIR_RIGHT : DIR_LEFT;
		if (layout == L_HORIZ) {
			struct sway_container *active = workspace->current.focused_inactive_child;
			int active_idx = max(list_find(workspace->tiling, active), 0);
			scroll_end_horizontal(seat, workspace->tiling, active_idx, scrolling_direction);
		} else {
			struct sway_container *container = get_mouse_container(seat);
			struct sway_container *active = container->current.focused_inactive_child;
			int active_idx = max(list_find(container->pending.children, active), 0);
			scroll_end_horizontal(seat, container->pending.children, active_idx, scrolling_direction);			
		}
	} else {
		scrolling_direction = workspace->gesture.dy > 0.0 ? DIR_DOWN : DIR_UP; 
		if (layout == L_HORIZ) {
			struct sway_container *container = get_mouse_container(seat);
			struct sway_container *active = container->current.focused_inactive_child;
			int active_idx = max(list_find(container->pending.children, active), 0);
			scroll_end_vertical(seat, container->pending.children, active_idx, scrolling_direction);			
		} else {
			struct sway_container *active = workspace->current.focused_inactive_child;
			int active_idx = max(list_find(workspace->tiling, active), 0);
			scroll_end_vertical(seat, workspace->tiling, active_idx, scrolling_direction);
		}
	}
	arrange_workspace(workspace);
	transaction_commit_dirty();
	return true;
}
