#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <thread.h>
#include "dufus.h"

/* UI layout constants */
enum {
	/* Fixed UI element sizes */
	FOOTER_HEIGHT = 50,    /* Make footer much taller for better visibility */
	PADDING = 10,          /* Padding within elements */
	MARGIN = 4,            /* Margin around elements */
	SPLITTER_HEIGHT = 4,   /* Increased splitter height for better visibility */
	LISTITEM_HEIGHT = 50,  /* Make list items much taller for better text visibility */
	
	/* Visualization parameters */
	MINBOX = 20,           /* Minimum box size for rectangle in treemap */
	
	/* Text thresholds */
	LABEL_THRESHOLD = 40,  /* Minimum size to show text labels */
	
	/* UI states */
	NORMAL_STATE = 0,
	HELP_STATE = 1,
	
	/* Split ratio (percentage of height for treemap pane vs list pane) */
	SPLIT_RATIO = 60,      /* Treemap now uses 60% of available space */
	
	/* Visualization parameters */
	MAX_DEPTH_LEVEL = 8,   /* Maximum recursion depth to visualize differently */
	FRACTAL_PADDING = 2    /* Spacing between elements */
};

/* Version string */
char *VERSION = "0.2";

/* Colors */
Image *back;           /* Background color */
Image *footer_bg;      /* Footer background */
Image *dir_color;      /* Directory color in treemap */
Image *file_color;     /* File color in treemap */
Image *highlight;      /* Highlight color for selection */
Image *text_color;     /* Text color */
Image *border_color;   /* Border color */
Image *splitter_color; /* Splitter color */
Image *list_bg;        /* List background */
Image *list_sel_bg;    /* List selected item background */
Image *list_dir_bg;    /* List directory item background */
Image *depth_colors[8]; /* Array of colors for depth levels */

/* Pre-rendered icons */
Image *file_icon;      /* File icon image */
Image *dir_icon;       /* Directory icon image */

/* Global UI state */
Rectangle screenbounds;   /* Full screen bounds */
Rectangle footer_rect;    /* Footer rectangle */
Rectangle list_rect;      /* List view rectangle (upper pane) */
Rectangle treemap_rect;   /* Treemap visualization area (lower pane) */
int ui_state = NORMAL_STATE;
char status_message[256] = "Ready";
char current_path[1024] = ".";
char *argv0;
int scroll_offset = 0;    /* Scroll offset for list view (in items) */
int visible_items = 0;    /* Number of items visible in list view */
int selected_list_idx = -1; /* Selected item in list view */

/* FS data state */
FsNode *root = nil;       /* Root of file system tree */
FsNode *current = nil;    /* Current navigation point */

/* Global node counter for unique IDs */
int next_node_id = 1;

/* Pre-render a directory icon */
void
create_dir_icon(void)
{
	Rectangle r, tab;
	
	/* Create icon with transparent background */
	dir_icon = allocimage(display, Rect(0, 0, 16, 16), screen->chan, 0, DTransparent);
	if(dir_icon == nil)
		sysfatal("allocimage failed for dir_icon: %r");
	
	/* Main folder rectangle */
	r = Rect(0, 2, 16, 14);
	
	/* Tab on top */
	tab = Rect(2, 0, 8, 2);
	
	/* Draw the folder icon */
	draw(dir_icon, r, dir_color, nil, ZP);
	draw(dir_icon, tab, dir_color, nil, ZP);
	
	/* Draw borders */
	border(dir_icon, r, 1, border_color, ZP);
	border(dir_icon, tab, 1, border_color, ZP);
}

/* Pre-render a file icon */
void
create_file_icon(void)
{
	Rectangle r;
	Point pts[3];
	
	/* Create icon with transparent background */
	file_icon = allocimage(display, Rect(0, 0, 16, 16), screen->chan, 0, DTransparent);
	if(file_icon == nil)
		sysfatal("allocimage failed for file_icon: %r");
	
	/* Main document rectangle */
	r = Rect(0, 0, 14, 16);
	
	/* Draw the document icon */
	draw(file_icon, r, file_color, nil, ZP);
	
	/* Draw the folded corner */
	pts[0] = Pt(10, 0);
	pts[1] = Pt(14, 4);
	pts[2] = Pt(10, 4);
	fillpoly(file_icon, pts, 3, 1, border_color, ZP);
	
	/* Draw border */
	border(file_icon, r, 1, border_color, ZP);
	line(file_icon, Pt(10, 0), Pt(14, 4), 0, 0, 0, border_color, ZP);
}

/* Initialize UI colors */
void
init_colors(void)
{
	/* Create standard colors */
	back = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x1a1a1aFF);  /* Darker background */
	footer_bg = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x333333FF);  /* Dark gray footer */
	dir_color = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x4169E1FF);  /* Royal Blue for directories */
	file_color = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x87CEEBFF);  /* Sky Blue for files */
	highlight = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xFFD700FF);  /* Gold highlight */
	text_color = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xE0E0E0FF);  /* Light gray text */
	border_color = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x555555FF);  /* Medium gray border */
	splitter_color = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x444444FF);  /* Dark gray splitter */
	list_bg = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x2A2A2AFF);  /* Slightly lighter than background */
	list_sel_bg = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x3F3F3FFF);  /* Selected item background */
	list_dir_bg = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x2D2D4FFF);  /* Directory item background */
	
	/* Create depth-based visualization colors - gradient from cool to warm colors */
	depth_colors[0] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x4B0082FF);  /* Indigo */
	depth_colors[1] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x0000CDFF);  /* Medium Blue */
	depth_colors[2] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x1E90FFFF);  /* Dodger Blue */
	depth_colors[3] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x00CED1FF);  /* Dark Turquoise */
	depth_colors[4] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x00FF7FFF);  /* Spring Green */
	depth_colors[5] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xFFFF00FF);  /* Yellow */
	depth_colors[6] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xFF7F00FF);  /* Orange */
	depth_colors[7] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xFF0000FF);  /* Red */
	
	/* Create icons */
	create_file_icon();
	create_dir_icon();
}

/* Calculate and update all UI rectangles based on screen size */
void
calculate_layout(void)
{
	int content_height, list_height, treemap_height;
	
	/* Get current screen bounds */
	screenbounds = screen->r;
	
	/* Ensure minimum screen dimensions */
	if(Dx(screenbounds) < 200) screenbounds.max.x = screenbounds.min.x + 200;
	if(Dy(screenbounds) < 200) screenbounds.max.y = screenbounds.min.y + 200;
	
	/* Calculate footer rectangle (bottom of screen) at a FIXED position */
	footer_rect = Rect(
		screenbounds.min.x,
		screenbounds.max.y - FOOTER_HEIGHT,
		screenbounds.max.x,
		screenbounds.max.y
	);
	
	/* Calculate content area height (excluding footer) */
	content_height = Dy(screenbounds) - FOOTER_HEIGHT;
	
	/* Calculate heights for each pane based on split ratio */
	list_height = (content_height * (100 - SPLIT_RATIO)) / 100;
	
	/* Ensure minimum list height */
	if(list_height < LISTITEM_HEIGHT * 3)
		list_height = LISTITEM_HEIGHT * 3;
	
	/* Calculate treemap height based on remaining space */
	treemap_height = content_height - list_height - SPLITTER_HEIGHT;
	
	/* Ensure minimum treemap height */
	if(treemap_height < 100) {
		treemap_height = 100;
		/* Recalculate list height if needed */
		list_height = content_height - treemap_height - SPLITTER_HEIGHT;
	}
	
	/* Calculate list rectangle (upper pane) */
	list_rect = Rect(
		screenbounds.min.x,
		screenbounds.min.y,
		screenbounds.max.x,
		screenbounds.min.y + list_height
	);
	
	/* Calculate treemap rectangle (lower pane) */
	treemap_rect = Rect(
		screenbounds.min.x,
		screenbounds.min.y + list_height + SPLITTER_HEIGHT,
		screenbounds.max.x,
		screenbounds.min.y + list_height + SPLITTER_HEIGHT + treemap_height
	);
	
	/* Calculate visible items in list view - only count whole items that fit */
	visible_items = (list_rect.max.y - list_rect.min.y) / LISTITEM_HEIGHT;
	
	/* Ensure at least 1 visible item */
	if(visible_items < 1) 
		visible_items = 1;
}

/* Return the minimum of two integers */
int
min(int a, int b)
{
	return a < b ? a : b;
}

/* Draw the footer status bar */
void
draw_footer(void)
{
	char version_str[32];
	
	/* Draw footer background */
	draw(screen, footer_rect, footer_bg, nil, ZP);
	
	/* Draw top border of footer */
	Rectangle border = Rect(
		footer_rect.min.x, 
		footer_rect.min.y,
		footer_rect.max.x,
		footer_rect.min.y + 1
	);
	draw(screen, border, border_color, nil, ZP);
	
	/* Proper vertical centering for footer text */
	int text_y = footer_rect.min.y + (FOOTER_HEIGHT - font->height) / 2;
	
	/* Left-aligned status message */
	string(screen, Pt(footer_rect.min.x + PADDING, text_y), 
		text_color, ZP, font, status_message);
	
	/* Right-aligned version string */
	snprint(version_str, sizeof(version_str), "dufus %s", VERSION);
	string(screen, Pt(footer_rect.max.x - PADDING - stringwidth(font, version_str), text_y),
		text_color, ZP, font, version_str);
}

/* Draw the splitter between panes */
void
draw_splitter(void)
{
	Rectangle splitter = Rect(
		screenbounds.min.x,
		list_rect.max.y,
		screenbounds.max.x,
		list_rect.max.y + SPLITTER_HEIGHT
	);
	
	draw(screen, splitter, splitter_color, nil, ZP);
}

/* Draw a list item in the file list */
void
draw_list_item(FsNode *node, int index, int is_visible)
{
	char size_str[32];
	Rectangle item_rect;
	int y_pos;
	
	/* Skip if not visible */
	if(!is_visible)
		return;
	
	/* Calculate item rectangle */
	y_pos = list_rect.min.y + (index - scroll_offset + (current != root ? 1 : 0)) * LISTITEM_HEIGHT;
	item_rect = Rect(
		list_rect.min.x,
		y_pos,
		list_rect.max.x,
		y_pos + LISTITEM_HEIGHT
	);
	
	/* Skip items that would be drawn beyond the bottom of the list rect */
	if(item_rect.min.y >= list_rect.max.y || item_rect.max.y > list_rect.max.y)
		return;
	
	/* Draw background based on selection state and directory status */
	if(index == selected_list_idx) {
		draw(screen, item_rect, list_sel_bg, nil, ZP);
	} else if(node->isdir) {
		draw(screen, item_rect, list_dir_bg, nil, ZP);
	} else {
		draw(screen, item_rect, list_bg, nil, ZP);
	}
	
	/* Format size string */
	snprint(size_str, sizeof(size_str), "(%s)", format_size(node->size));
	
	/* Calculate vertical centers for better alignment */
	int item_center_y = y_pos + (LISTITEM_HEIGHT / 2);
	int text_y = item_center_y - (font->height / 2) + 2; /* Center text vertically with small adjustment */
	
	/* Draw appropriate icon */
	if(node->isdir) {
		/* Position for the icon */
		Point icon_pos = Pt(item_rect.min.x + PADDING, item_center_y - 8);
		draw(screen, rectaddpt(Rect(0, 0, 16, 16), icon_pos), dir_icon, nil, ZP);
	} else {
		/* Position for the icon */
		Point icon_pos = Pt(item_rect.min.x + PADDING, item_center_y - 8);
		draw(screen, rectaddpt(Rect(0, 0, 16, 16), icon_pos), file_icon, nil, ZP);
	}
	
	/* Draw file/directory name (with spacing for icon) */
	string(screen, Pt(item_rect.min.x + PADDING + 24, text_y), 
		text_color, ZP, font, node->name);
	
	/* Right-aligned size string */
	string(screen, Pt(item_rect.max.x - PADDING - stringwidth(font, size_str), text_y), 
		text_color, ZP, font, size_str);
	
	/* Draw bottom border */
	Rectangle border = Rect(
		item_rect.min.x,
		item_rect.max.y - 1,
		item_rect.max.x,
		item_rect.max.y
	);
	draw(screen, border, border_color, nil, ZP);
}

/* Draw the file list (upper pane) */
void
draw_file_list(void)
{
	int i, count;
	
	/* Fill list area with background */
	draw(screen, list_rect, list_bg, nil, ZP);
	
	/* Draw border around list */
	border(screen, list_rect, 1, border_color, ZP);
	
	/* Return if no data */
	if(current == nil) {
		/* Draw "No data" message with proper vertical centering */
		int text_y = list_rect.min.y + (Dy(list_rect) - font->height) / 2;
		string(screen, Pt(list_rect.min.x + PADDING, text_y), 
			text_color, ZP, font, "No data loaded. Press 'o' to open a directory.");
		return;
	}
	
	/* Draw parent directory entry if not at root */
	if(current != root) {
		Rectangle parent_rect = Rect(
			list_rect.min.x,
			list_rect.min.y,
			list_rect.max.x,
			list_rect.min.y + LISTITEM_HEIGHT
		);
		
		/* Draw background */
		if(selected_list_idx == -2) /* Special case for ".." */
			draw(screen, parent_rect, list_sel_bg, nil, ZP);
		else
			draw(screen, parent_rect, list_dir_bg, nil, ZP);
		
		/* Calculate vertical centers for better alignment */
		int item_center_y = list_rect.min.y + (LISTITEM_HEIGHT / 2);
		int text_y = item_center_y - (font->height / 2) + 2; /* Center text vertically with small adjustment */
		
		/* Draw folder icon for parent directory */
		Point icon_pos = Pt(parent_rect.min.x + PADDING, item_center_y - 8);
		draw(screen, rectaddpt(Rect(0, 0, 16, 16), icon_pos), dir_icon, nil, ZP);
		
		/* Draw parent directory label */
		string(screen, Pt(parent_rect.min.x + PADDING + 24, text_y), 
			text_color, ZP, font, "..");
		
		/* Draw bottom border */
		Rectangle border = Rect(
			parent_rect.min.x,
			parent_rect.max.y - 1,
			parent_rect.max.x,
			parent_rect.max.y
		);
		draw(screen, border, border_color, nil, ZP);
	}
	
	/* Draw list items */
	count = current->nchildren;
	for(i = 0; i < count; i++) {
		/* Check if item is visible within the viewport - account for parent directory */
		int has_parent = (current != root) ? 1 : 0;
		int is_visible = (i >= scroll_offset && i < scroll_offset + visible_items - has_parent);
		if(is_visible)
			draw_list_item(current->children[i], i, is_visible);
	}
}

/* Enhanced treemap layout algorithm - designed to show recursive patterns */
void
layout_treemap(FsNode *node, Rectangle avail, int depth)
{
	double total_size;
	double aspect;
	int i;
	
	if(node == nil || node->nchildren == 0 || depth > MAX_DEPTH_LEVEL)
		return;
	
	/* Ensure minimum rectangle size */
	if(Dx(avail) < MINBOX || Dy(avail) < MINBOX)
		return;
	
	/* Store the available rectangle as this node's bounds */
	node->bounds = avail;
	
	/* Calculate total size of all children */
	total_size = 0;
	for(i = 0; i < node->nchildren; i++)
		total_size += (double)node->children[i]->size;
	
	/* If total size is zero, give equal space to all */
	if(total_size <= 0) {
		total_size = node->nchildren;
		for(i = 0; i < node->nchildren; i++)
			node->children[i]->size = 1;
	}
	
	/* Select layout pattern based on aspect ratio */
	aspect = (double)Dx(avail) / Dy(avail);
	
	/* Simple layout based on aspect ratio */
	if(aspect >= 1.0) {
		/* Horizontal layout for wide rectangles */
		layout_horizontal(node, avail, total_size);
	} else {
		/* Vertical layout for tall rectangles */
		layout_vertical(node, avail, total_size);
	}
	
	/* Recursively layout children with proper padding, but only up to a certain depth
	 * to avoid going too deep and making the visualization too complex */
	if(depth < 2) {  /* We show the top few levels in full detail */
		for(i = 0; i < node->nchildren; i++) {
			FsNode *child = node->children[i];
			if(child->isdir && child->nchildren > 0) {
				Rectangle inset = insetrect(child->bounds, MARGIN);
				if(Dx(inset) > MINBOX && Dy(inset) > MINBOX) {
					layout_treemap(child, inset, depth + 1);
				}
			}
		}
	} else {
		/* For deeper levels, only process larger nodes to avoid visual clutter */
		int limit = 0; /* Start with root level count */
		
		/* Progressively increase the size threshold at deeper levels */
		double size_threshold = 0;
		if(depth == 2) size_threshold = total_size * 0.05; /* 5% of parent's size */
		else if(depth == 3) size_threshold = total_size * 0.10; /* 10% of parent's size */
		else if(depth == 4) size_threshold = total_size * 0.15; /* 15% of parent's size */
		else size_threshold = total_size * 0.20; /* 20% of parent's size */
		
		for(i = 0; i < node->nchildren && limit < 5; i++) {
			FsNode *child = node->children[i];
			if(child->isdir && child->nchildren > 0 && child->size >= size_threshold) {
				Rectangle inset = insetrect(child->bounds, MARGIN);
				if(Dx(inset) > MINBOX && Dy(inset) > MINBOX) {
					layout_treemap(child, inset, depth + 1);
					limit++;
				}
			}
		}
	}
}

/* Layout nodes in a horizontal pattern */
void
layout_horizontal(FsNode *node, Rectangle avail, double total_size)
{
	int i;
	int pos;
	int remaining;
	int width;
	FsNode *child;
	Rectangle r;
	
	pos = avail.min.x;
	remaining = Dx(avail);
	
	/* Use threshold to ensure minimum rectangle sizes */
	int min_width = MINBOX;
	int available_children = node->nchildren;
	
	/* If we can't fit all children with minimum size, limit how many we show */
	if(remaining < available_children * min_width) {
		available_children = remaining / min_width;
		if(available_children < 1)
			available_children = 1;
	}
	
	for(i = 0; i < node->nchildren; i++) {
		child = node->children[i];
		
		/* Skip tiny nodes if we're short on space */
		if(i >= available_children) {
			child->bounds = Rect(0, 0, 0, 0); /* Zero-sized rectangle */
			continue;
		}
		
		/* Last child gets remaining space, others get proportional space */
		if(i == available_children - 1) {
			width = remaining;
		} else {
			width = (int)((double)child->size / total_size * Dx(avail));
			if(width < min_width)
				width = min_width;
			if(width > remaining - (available_children - i - 1) * min_width)
				width = remaining - (available_children - i - 1) * min_width;
		}
		
		/* Ensure width is positive */
		if(width <= 0)
			width = 1;
		
		/* Create the rectangle for this child */
		r = Rect(pos, avail.min.y, pos + width, avail.max.y);
		
		/* Ensure rectangle is within parent bounds */
		if(r.max.x > avail.max.x)
			r.max.x = avail.max.x;
		
		child->bounds = r;
		
		/* Update position and remaining space */
		pos += width;
		remaining -= width;
		
		/* Stop if we run out of space */
		if(remaining <= 0)
			break;
	}
}

/* Layout nodes in a vertical pattern */
void
layout_vertical(FsNode *node, Rectangle avail, double total_size)
{
	int i;
	int pos;
	int remaining;
	int height;
	FsNode *child;
	Rectangle r;
	
	pos = avail.min.y;
	remaining = Dy(avail);
	
	/* Use threshold to ensure minimum rectangle sizes */
	int min_height = MINBOX;
	int available_children = node->nchildren;
	
	/* If we can't fit all children with minimum size, limit how many we show */
	if(remaining < available_children * min_height) {
		available_children = remaining / min_height;
		if(available_children < 1)
			available_children = 1;
	}
	
	for(i = 0; i < node->nchildren; i++) {
		child = node->children[i];
		
		/* Skip tiny nodes if we're short on space */
		if(i >= available_children) {
			child->bounds = Rect(0, 0, 0, 0); /* Zero-sized rectangle */
			continue;
		}
		
		/* Last child gets remaining space, others get proportional space */
		if(i == available_children - 1) {
			height = remaining;
		} else {
			height = (int)((double)child->size / total_size * Dy(avail));
			if(height < min_height)
				height = min_height;
			if(height > remaining - (available_children - i - 1) * min_height)
				height = remaining - (available_children - i - 1) * min_height;
		}
		
		/* Ensure height is positive */
		if(height <= 0)
			height = 1;
		
		/* Create the rectangle for this child */
		r = Rect(avail.min.x, pos, avail.max.x, pos + height);
		
		/* Ensure rectangle is within parent bounds */
		if(r.max.y > avail.max.y)
			r.max.y = avail.max.y;
		
		child->bounds = r;
		
		/* Update position and remaining space */
		pos += height;
		remaining -= height;
		
		/* Stop if we run out of space */
		if(remaining <= 0)
			break;
	}
}

/* Draw a single file system node in the treemap */
void
draw_node(FsNode *node, int highlight_it)
{
	Rectangle r;
	int depth;
	FsNode *parent;
	char count[32];
	char size_str[32];
	char display_name[256];
	int text_y;
	int border_thickness;
	int max_text_width;
	
	if(node == nil)
		return;
	
	/* Get the node's bounds */
	r = node->bounds;
	
	/* Skip if rectangle is too small, invalid, or zero-sized */
	if(Dx(r) <= 0 || Dy(r) <= 0)
		return;
	
	/* Calculate recursive depth for coloring */
	depth = 0;
	parent = node->parent;
	while(parent != nil && depth < MAX_DEPTH_LEVEL - 1) {
		depth++;
		parent = parent->parent;
	}
	
	/* Draw filled rectangle with appropriate color */
	if(highlight_it) {
		/* For highlighted items, draw background in highlight color */
		draw(screen, r, highlight, nil, ZP);
	} else if(node->isdir) {
		/* Use depth-based color gradient for directories */
		draw(screen, r, depth_colors[depth % 8], nil, ZP);
	} else {
		draw(screen, r, file_color, nil, ZP);
	}
	
	/* Determine border thickness based on item type and highlight state */
	if(highlight_it) {
		/* Thicker border for highlighted items */
		border_thickness = 3;
	} else if(node->isdir) {
		/* Slightly thicker border for directories to make them more distinct */
		border_thickness = 2;
	} else {
		/* Normal border for files */
		border_thickness = 1;
	}
	
	/* Draw appropriate border */
	border(screen, r, border_thickness, border_color, ZP);
	
	/* Add an inner border for highlighted items for extra emphasis */
	if(highlight_it) {
		Rectangle inner = insetrect(r, border_thickness);
		if(Dx(inner) > 6 && Dy(inner) > 6) {
			border(screen, inner, 1, highlight, ZP);
		}
	}
	
	/* Calculate maximum text width to fit inside the rectangle */
	max_text_width = Dx(r) - 2*(border_thickness + 3);
	if(max_text_width <= 0)
		return;  /* No space for text */
	
	/* Only draw text if rectangle is large enough */
	if(Dx(r) > LABEL_THRESHOLD && Dy(r) > LABEL_THRESHOLD) {
		/* Truncate filename if needed and add ellipsis */
		strecpy(display_name, display_name+sizeof(display_name), node->name);
		truncate_string(display_name, font, max_text_width);
		
		/* Draw filename for large enough rectangles */
		text_y = r.min.y + border_thickness + 3;
		if(text_y + font->height <= r.max.y - 3) {
			string(screen, Pt(r.min.x + border_thickness + 3, text_y), text_color, ZP, font, display_name);
			
			/* If it's a directory, show child count */
			if(node->isdir) {
				text_y += font->height + 2;
				snprint(count, sizeof(count), "%d items", node->nchildren);
				truncate_string(count, font, max_text_width);
				
				if(text_y + font->height <= r.max.y - 3) {
					string(screen, Pt(r.min.x + border_thickness + 3, text_y), text_color, ZP, font, count);
					
					/* Show size information */
					text_y += font->height + 2;
					snprint(size_str, sizeof(size_str), "%s", format_size(node->size));
					truncate_string(size_str, font, max_text_width);
					
					/* Only draw size if we have room */
					if(text_y + font->height <= r.max.y - 3) {
						string(screen, Pt(r.min.x + border_thickness + 3, text_y), text_color, ZP, font, size_str);
					}
				}
			} else {
				/* For files, just show size after name */
				text_y += font->height + 2;
				snprint(size_str, sizeof(size_str), "%s", format_size(node->size));
				truncate_string(size_str, font, max_text_width);
				
				/* Only draw size if we have room */
				if(text_y + font->height <= r.max.y - 3) {
					string(screen, Pt(r.min.x + border_thickness + 3, text_y), text_color, ZP, font, size_str);
				}
			}
		}
	} else if(node->isdir && Dx(r) > MINBOX*2 && Dy(r) > MINBOX*2) {
		/* For medium-sized directories, just show item count */
		snprint(count, sizeof(count), "%d", node->nchildren);
		truncate_string(count, font, max_text_width);
		text_y = r.min.y + (Dy(r) - font->height)/2;
		
		/* Only draw if it fits vertically */
		if(text_y >= r.min.y && text_y + font->height <= r.max.y) {
			string(screen, Pt(r.min.x + border_thickness + 3, text_y), text_color, ZP, font, count);
		}
	}
}

/* Truncate a string to fit the given width, adding ellipsis if needed */
void
truncate_string(char *s, Font *f, int max_width)
{
	int len, width;
	char ellipsis[] = "...";
	int ellipsis_width;
	
	if(s == nil || s[0] == '\0')
		return;
	
	width = stringwidth(f, s);
	if(width <= max_width)
		return;  /* String fits, no truncation needed */
	
	ellipsis_width = stringwidth(f, ellipsis);
	
	/* If we can't even fit ellipsis, just truncate to empty string */
	if(max_width < ellipsis_width) {
		s[0] = '\0';
		return;
	}
	
	/* Try removing characters until string with ellipsis fits */
	len = strlen(s);
	while(len > 0) {
		s[--len] = '\0';
		width = stringwidth(f, s) + ellipsis_width;
		if(width <= max_width) {
			strcat(s, ellipsis);
			return;
		}
	}
}

/* Draw the treemap visualization */
void
draw_treemap(void)
{
	int i;
	Rectangle full_treemap;
	
	/* Fill treemap area with background */
	draw(screen, treemap_rect, back, nil, ZP);
	
	/* Draw border around treemap */
	border(screen, treemap_rect, 1, border_color, ZP);
	
	/* Return if no data */
	if(current == nil) {
		/* Draw "No data" message */
		int text_y = treemap_rect.min.y + (Dy(treemap_rect) / 2) + (font->height / 3);
		string(screen, 
			Pt(treemap_rect.min.x + PADDING, text_y), 
			text_color, ZP, font, "No data loaded. Press 'o' to open a directory.");
		return;
	}
	
	/* Calculate full treemap area with small margin */
	full_treemap = insetrect(treemap_rect, MARGIN);
	
	/* For an empty directory, just show a message */
	if(current->nchildren == 0) {
		/* Draw message if no children */
		int text_y = treemap_rect.min.y + (Dy(treemap_rect) / 2) + (font->height / 3);
		string(screen, 
			Pt(treemap_rect.min.x + PADDING, text_y), 
			text_color, ZP, font, "Empty directory");
		return;
	}
	
	/* Layout the current directory's children directly within the treemap */
	layout_treemap(current, full_treemap, 0);
	
	/* First, draw all direct children of the current directory */
	for(i = 0; i < current->nchildren; i++) {
		FsNode *child = current->children[i];
		int highlight_this = (i == selected_list_idx);
		draw_node(child, highlight_this);
	}
	
	/* Then, recursively draw the contents of each child */
	for(i = 0; i < current->nchildren; i++) {
		FsNode *child = current->children[i];
		if(child->isdir && child->nchildren > 0) {
			/* Skip drawing this recursive node if it's the selected item 
			 * (we already drew it with highlight) */
			if(i != selected_list_idx) {
				draw_node_recursive_contents(child);
			}
		}
	}
}

/* Draw just the children of a node, not the node itself */
void
draw_node_recursive_contents(FsNode *node)
{
	int i;
	
	if(node == nil || !node->isdir || node->nchildren == 0)
		return;
	
	/* Draw all children */
	for(i = 0; i < node->nchildren; i++) {
		FsNode *child = node->children[i];
		draw_node(child, 0);
		
		/* Recursively draw grandchildren */
		if(child->isdir && child->nchildren > 0) {
			draw_node_recursive_contents(child);
		}
	}
}

/* Draw the entire UI - reorganize to ensure footer is drawn last */
void
draw_ui(void)
{
	/* Clear screen */
	draw(screen, screenbounds, back, nil, ZP);
	
	/* Draw main UI components - order matters! */
	draw_file_list();
	draw_splitter();
	draw_treemap();
	
	/* Draw footer LAST to ensure it's on top */
	draw_footer();
	
	/* If in help state, draw help overlay */
	if(ui_state == HELP_STATE) {
		/* Draw help panel */
		Rectangle help_rect = insetrect(screenbounds, 50);
		draw(screen, help_rect, back, nil, ZP);
		border(screen, help_rect, 2, border_color, ZP);
		
		/* Draw title and version */
		Point p = Pt(help_rect.min.x + 10, help_rect.min.y + 20);
		char title[64];
		snprint(title, sizeof(title), "Dufus %s - Help", VERSION);
		string(screen, p, text_color, ZP, font, title);
		
		/* Draw help text with increased spacing between lines */
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "q - Quit");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "o - Open directory");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "u - Go up to parent");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "r - Return to root");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "? - Toggle help display");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "h - Go left/up to parent (vim-style)");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "j - Move down (vim-style)");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "k - Move up (vim-style)");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "l - Go right/enter directory (vim-style)");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "↑/↓ - Navigate list");
		p.y += 25; /* Increased spacing */
		string(screen, p, text_color, ZP, font, "Enter - Navigate into selected directory");
	}
	
	/* Flush image to screen */
	flushimage(display, 1);
}

/* Initialize drawing environment */
void
setup_draw(void)
{
	if(initdraw(nil, nil, "dufus - disk usage analyzer") < 0)
		sysfatal("initdraw failed: %r");
	
	einit(Emouse | Ekeyboard);
	
	calculate_layout();
	init_colors();
}

/* Format size in a human-readable way */
char*
format_size(u64int size)
{
	static char buf[32];
	
	if(size < KB)
		snprint(buf, sizeof(buf), "%lldB", size);
	else if(size < MB)
		snprint(buf, sizeof(buf), "%.1fKB", (double)size / KB);
	else if(size < GB)
		snprint(buf, sizeof(buf), "%.1fMB", (double)size / MB);
	else
		snprint(buf, sizeof(buf), "%.1fGB", (double)size / GB);
	
	return buf;
}

/* Update status message */
void
update_status(char *fmt, ...)
{
	va_list arg;
	
	va_start(arg, fmt);
	vsnprint(status_message, sizeof(status_message), fmt, arg);
	va_end(arg);
}

/* Create a new filesystem node */
FsNode*
create_fsnode(char *name, char *path, u64int size, int isdir, FsNode *parent)
{
	FsNode *node = mallocz(sizeof(FsNode), 1);
	if(node == nil)
		sysfatal("malloc failed: %r");
	
	strecpy(node->name, node->name+sizeof(node->name), name);
	strecpy(node->path, node->path+sizeof(node->path), path);
	node->size = size;
	node->isdir = isdir;
	node->parent = parent;
	node->children = nil;
	node->nchildren = 0;
	node->maxchildren = 0;
	node->color = isdir ? dir_color : file_color;
	node->id = next_node_id++;
	
	return node;
}

/* Add a child to a node */
void
add_child(FsNode *parent, FsNode *child)
{
	if(parent == nil || child == nil)
		return;
	
	if(parent->nchildren >= parent->maxchildren) {
		int newmax = parent->maxchildren == 0 ? 16 : parent->maxchildren * 2;
		FsNode **newchildren = realloc(parent->children, newmax * sizeof(FsNode*));
		if(newchildren == nil)
			sysfatal("realloc failed: %r");
		
		parent->children = newchildren;
		parent->maxchildren = newmax;
	}
	
	parent->children[parent->nchildren++] = child;
}

/* Sort nodes by size (largest first) */
void
sort_nodes_by_size(FsNode *parent)
{
	int i, j;
	FsNode *temp;
	
	if(parent == nil || parent->nchildren <= 1)
		return;
	
	/* Simple bubble sort */
	for(i = 0; i < parent->nchildren - 1; i++) {
		for(j = 0; j < parent->nchildren - i - 1; j++) {
			if(parent->children[j]->size < parent->children[j+1]->size) {
				temp = parent->children[j];
				parent->children[j] = parent->children[j+1];
				parent->children[j+1] = temp;
			}
		}
	}
}

/* Recursively scan a directory and build the tree */
void
scan_directory(char *path, FsNode *parent)
{
	Dir *dirents;
	int ndirents, i;
	u64int total = 0;
	char fullpath[1024];
	int fd;
	
	fd = open(path, OREAD);
	if(fd < 0) {
		fprint(2, "open failed for %s: %r\n", path);
		return;
	}
	
	ndirents = dirread(fd, &dirents);
	close(fd);
	
	if(ndirents < 0) {
		fprint(2, "dirread failed for %s: %r\n", path);
		return;
	}
	
	/* Initialize parent size to zero - will accumulate total */
	parent->size = 0;
	
	/* First pass: create nodes for all entries */
	for(i = 0; i < ndirents; i++) {
		/* Skip "." and ".." */
		if(strcmp(dirents[i].name, ".") == 0 || strcmp(dirents[i].name, "..") == 0)
			continue;
		
		snprint(fullpath, sizeof(fullpath), "%s/%s", path, dirents[i].name);
		
		int isdir = (dirents[i].qid.type & QTDIR);
		u64int size = dirents[i].length;
		
		FsNode *child = create_fsnode(dirents[i].name, fullpath, size, isdir, parent);
		add_child(parent, child);
	}
	
	/* Sort children by size (provisional) to process larger ones first */
	sort_nodes_by_size(parent);
	
	/* Second pass: scan subdirectories and update sizes */
	for(i = 0; i < parent->nchildren; i++) {
		FsNode *child = parent->children[i];
		if(child->isdir) {
			/* Recursively scan subdirectories */
			scan_directory(child->path, child);
			/* Update the running total with this subdirectory's size */
			total += child->size;
		} else {
			/* For files, just add the file size */
			total += child->size;
		}
		
		/* Update status periodically to show progress */
		if(i % 10 == 0) {
			update_status("Scanning %s... (%d/%d)", path, i, parent->nchildren);
			draw_footer();
			flushimage(display, 1);
		}
	}
	
	/* Update parent's size to include all children */
	parent->size = total;
	
	/* Re-sort children by size after all calculations */
	sort_nodes_by_size(parent);
	
	free(dirents);
}

/* Free all resources for a node and its children */
void
clear_fsnode(FsNode *node)
{
	int i;
	
	if(node == nil)
		return;
	
	for(i = 0; i < node->nchildren; i++)
		clear_fsnode(node->children[i]);
	
	free(node->children);
	free(node);
}

/* Find a node at a given point in the treemap */
FsNode*
find_node_at_point(FsNode *node, Point p)
{
	int i;
	FsNode *result;
	
	if(node == nil || !ptinrect(p, treemap_rect))
		return nil;
	
	/* First check if the point is within this node's bounds */
	if(ptinrect(p, node->bounds)) {
		/* Check children first - we want the most specific/innermost node */
		for(i = 0; i < node->nchildren; i++) {
			result = find_node_at_point(node->children[i], p);
			if(result != nil)
				return result;
		}
		
		/* If no child contains the point, return this node */
		return node;
	}
	
	/* Point not in this node */
	return nil;
}

/* Find the list item index at the given point */
int
find_list_item_at_point(Point p)
{
	int index;
	int offset = (current != root) ? 1 : 0; /* Adjust for parent directory entry */
	
	if(!ptinrect(p, list_rect))
		return -1;
	
	/* Check if it's the parent directory item */
	if(current != root && p.y < list_rect.min.y + LISTITEM_HEIGHT)
		return -2; /* Special case for ".." */
	
	/* Calculate index based on Y position, accounting for parent directory offset */
	index = ((p.y - list_rect.min.y) / LISTITEM_HEIGHT) - offset + scroll_offset;
	
	/* Check if index is valid */
	if(current != nil && index >= 0 && index < current->nchildren)
		return index;
	
	return -1;
}

/* Open and analyze a directory */
void
open_directory(char *path)
{
	/* Clean up existing data if any */
	if(root != nil) {
		clear_fsnode(root);
		root = nil;
		current = nil;
	}
	
	/* Store current path */
	strecpy(current_path, current_path+sizeof(current_path), path);
	
	/* Create root node and scan directory */
	root = create_fsnode(path, path, 0, 1, nil);
	current = root;
	scan_directory(path, root);
	
	/* Reset UI state */
	scroll_offset = 0;
	selected_list_idx = current->nchildren > 0 ? 0 : -1;
	
	/* Update status with size information */
	update_status("Current: %s (%s)", path, format_size(root->size));
}

/* Navigate to selected directory */
void
navigate_to_selected(void)
{
	if(current == nil || selected_list_idx < 0 || selected_list_idx >= current->nchildren)
		return;
	
	FsNode *selected = current->children[selected_list_idx];
	
	if(selected->isdir) {
		current = selected;
		scroll_offset = 0;
		selected_list_idx = current->nchildren > 0 ? 0 : -1;
		update_status("Current: %s (%s)", current->name, format_size(current->size));
	}
}

/* Navigate up to parent directory */
void
navigate_up(void)
{
	if(current != nil && current->parent != nil) {
		current = current->parent;
		scroll_offset = 0;
		selected_list_idx = current->nchildren > 0 ? 0 : -1;
		update_status("Current: %s (%s)", current->name, format_size(current->size));
	}
}

/* Handle keyboard navigation */
void
navigate(Rune key)
{
	switch(key) {
	case 'q':
		/* Quit */
		exits(nil);
		break;
		
	case 'o':
		/* Open directory dialog */
		{
			char path[1024] = ".";
			open_directory(path);
			draw_ui();
		}
		break;
		
	case 'u':
	case 'h': /* vim-style: left / also go up to parent */
		/* Go up to parent */
		navigate_up();
		draw_ui();
		break;
		
	case 'r':
		/* Return to root */
		if(root != nil) {
			current = root;
			scroll_offset = 0;
			selected_list_idx = current->nchildren > 0 ? 0 : -1;
			update_status("Returned to root");
			draw_ui();
		}
		break;
		
	case '?': /* Alternative trigger for help screen */
		/* Toggle help */
		ui_state = (ui_state == NORMAL_STATE) ? HELP_STATE : NORMAL_STATE;
		draw_ui();
		break;
		
	case 'k': /* vim-style: up */
	case Kup:
		/* Move selection up */
		if(current != nil && current->nchildren > 0) {
			selected_list_idx--;
			
			/* Handle parent directory special case */
			if(selected_list_idx < 0) {
				if(current != root)
					selected_list_idx = -2; /* ".." special case */
				else
					selected_list_idx = 0;
			}
			
			/* Adjust scroll if necessary */
			if(selected_list_idx >= 0 && selected_list_idx < scroll_offset)
				scroll_offset = selected_list_idx;
			
			draw_ui();
		}
		break;
		
	case 'j': /* vim-style: down */
	case Kdown:
		/* Move selection down */
		if(current != nil) {
			/* Handle parent directory special case */
			if(selected_list_idx == -2)
				selected_list_idx = 0;
			else
				selected_list_idx++;
			
			if(selected_list_idx >= current->nchildren)
				selected_list_idx = current->nchildren - 1;
			
			/* Adjust scroll if necessary */
			if(selected_list_idx >= scroll_offset + visible_items)
				scroll_offset = selected_list_idx - visible_items + 1;
			
			draw_ui();
		}
		break;
		
	case '\n':
	case 'l': /* vim-style: right */
	case Kright:
		/* Navigate into selected directory */
		if(selected_list_idx == -2) /* ".." special case */
			navigate_up();
		else
			navigate_to_selected();
		draw_ui();
		break;
		
	case Kleft:
		/* Navigate up */
		navigate_up();
		draw_ui();
		break;
	}
}

/* Handle keyboard input */
void
handle_key(Rune key)
{
	/* In any state, ESC returns to normal mode */
	if(key == Kesc) {
		ui_state = NORMAL_STATE;
		draw_ui();
		return;
	}
	
	/* Otherwise, handle based on current state */
	switch(ui_state) {
	case NORMAL_STATE:
		navigate(key);
		break;
		
	case HELP_STATE:
		/* Any key dismisses help */
		ui_state = NORMAL_STATE;
		draw_ui();
		break;
	}
}

/* Handle window resize */
void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	
	calculate_layout();
	draw_ui();
}

/* Display usage information */
void
usage(void)
{
	fprint(2, "usage: dufus [directory]\n");
	exits("usage");
}

/* Main function */
void
main(int argc, char *argv[])
{
	char *path = ".";
	Event ev;
	
	argv0 = argv[0];
	
	ARGBEGIN {
	default:
		usage();
	} ARGEND;
	
	if(argc > 1)
		usage();
	
	if(argc == 1)
		path = argv[0];
	
	setup_draw();
	
	/* Initialize and scan the root directory */
	open_directory(path);
	
	/* Draw the UI immediately */
	draw_ui();
	
	/* Main event loop */
	for(;;) {
		/* Wait for an event */
		switch(event(&ev)) {
		case Emouse:
			/* Handle mouse events */
			if(ev.mouse.buttons & 1) {
				/* Left click in list view */
				int list_idx = find_list_item_at_point(ev.mouse.xy);
				if(list_idx >= -2) {
					/* Update selection */
					selected_list_idx = list_idx;
					
					/* Handle click on ".." */
					if(list_idx == -2) {
						navigate_up();
					}
					/* Handle double-click to navigate */
					else if(list_idx >= 0 && current->children[list_idx]->isdir) {
						/* TODO: proper double-click detection */
						navigate_to_selected();
					}
					draw_ui();
				}
				
				/* Left click in treemap view */
				else if(ptinrect(ev.mouse.xy, treemap_rect)) {
					FsNode *clicked = find_node_at_point(current, ev.mouse.xy);
					if(clicked != nil) {
						/* If clicked node is a direct child of current, select it */
						if(clicked->parent == current) {
							/* Find index of clicked node in current's children */
							int i;
							for(i = 0; i < current->nchildren; i++) {
								if(current->children[i] == clicked) {
									selected_list_idx = i;
									
									/* Adjust scroll to show selected item */
									if(selected_list_idx < scroll_offset)
										scroll_offset = selected_list_idx;
									else if(selected_list_idx >= scroll_offset + visible_items)
										scroll_offset = selected_list_idx - visible_items + 1;
									
									break;
								}
							}
						}
						/* If it's a descendant, navigate to its parent directory */
						else if(clicked->parent != nil) {
							FsNode *target = clicked;
							int i;
							
							/* Navigate to the parent of the clicked node */
							current = clicked->parent;
							
							/* Find the index of the clicked node in its parent's children */
							for(i = 0; i < current->nchildren; i++) {
								if(current->children[i] == target) {
									selected_list_idx = i;
									break;
								}
							}
							
							/* Adjust scroll to show selected item */
							if(selected_list_idx < scroll_offset)
								scroll_offset = selected_list_idx;
							else if(selected_list_idx >= scroll_offset + visible_items)
								scroll_offset = selected_list_idx - visible_items + 1;
							
							/* Update status message */
							update_status("Current: %s (%s)", current->path, format_size(current->size));
						}
						
						draw_ui();
					}
				}
			}
			break;
			
		case Ekeyboard:
			handle_key(ev.kbdc);
			break;
		}
	}
} 