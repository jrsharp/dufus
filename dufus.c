#include "dufus.h"

enum {
	MARGIN = 20,      /* Margin around visualization */
	MINBOX = 40,      /* Minimum box size for visualization */
	STATUSHEIGHT = 30 /* Status bar height */
};

/* Colors */
Image *back;    /* Background */
Image *high;    /* Highlight */
Image *bord;    /* Border */
Image *text;    /* Text */
Image *file_color;  /* Files */
Image *dir_color;   /* Directories */
Image *accent;      /* Accent */

/* Global variables */
int mode = NORMAL;
FsNode *root = nil;
FsNode *current = nil;
struct Image *screen;
struct Font *font;
struct Display *display;
Rectangle maprect;
Point viewport = {0, 0};  /* Current viewport offset for panning */
Point pan_start = {0, 0};  /* Starting point for panning */
int panning = 0;           /* Flag to indicate if we're panning the viewport */
char *argv0;
char status_message[256] = "Ready";

/* Initialize colors */
void
initcolors(void)
{
	back = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xF0F0F0FF);  /* Light gray background */
	high = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xB0E0FFFF);  /* Light blue for highlights */
	bord = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x333333FF);  /* Dark gray for borders */
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);  /* Black for text */
	file_color = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x8090C0FF);  /* Blue-gray for files */
	dir_color = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x90C080FF);  /* Green-gray for directories */
	accent = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xE04040FF);  /* Red accent for selection */
	
	if(back == nil || high == nil || bord == nil || text == nil || 
	   file_color == nil || dir_color == nil || accent == nil)
		sysfatal("allocimage failed");
}

/* Setup drawing environment */
void
setupdraw(void)
{
	if(initdraw(nil, nil, "dufus") < 0)
		sysfatal("initdraw failed: %r");
	
	display = display;
	screen = screen;
	font = font;
	maprect = screen->r;
	
	einit(Emouse | Ekeyboard);
	
	initcolors();
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
	node->selected = 0;
	node->color = isdir ? dir_color : file_color;
	
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
	
	for(i = 0; i < ndirents; i++) {
		/* Skip "." and ".." */
		if(strcmp(dirents[i].name, ".") == 0 || strcmp(dirents[i].name, "..") == 0)
			continue;
			
		snprint(fullpath, sizeof(fullpath), "%s/%s", path, dirents[i].name);
		
		int isdir = (dirents[i].qid.type & QTDIR);
		u64int size = dirents[i].length;
		
		FsNode *child = create_fsnode(dirents[i].name, fullpath, size, isdir, parent);
		add_child(parent, child);
		
		/* Recursively scan subdirectories */
		if(isdir)
			scan_directory(fullpath, child);
		
		total += size;
	}
	
	/* Update parent's size to include all children */
	parent->size = total;
	
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

/* Format a size value in a human-readable way */
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
	
	/* Force redraw of status bar */
	draw_status_bar();
	flushimage(display, 1);
}

/* Layout nodes using a treemap algorithm */
void
layout_nodes(FsNode *node, Rectangle avail)
{
	int i;
	double total, acc;
	int x, y, w, h;
	int horizontal;
	Rectangle r;
	
	if(node == nil || node->nchildren == 0)
		return;
		
	/* Copy available rectangle for this node */
	node->bounds = avail;
	
	/* Calculate total size of all children */
	total = 0;
	for(i = 0; i < node->nchildren; i++)
		total += (double)node->children[i]->size;
		
	/* Choose layout direction (horizontal or vertical) based on rectangle aspect ratio */
	horizontal = (Dx(avail) >= Dy(avail));
	
	/* Position all children */
	x = avail.min.x;
	y = avail.min.y;
	w = Dx(avail);
	h = Dy(avail);
	
	acc = 0;
	for(i = 0; i < node->nchildren; i++) {
		FsNode *child = node->children[i];
		double ratio = (total > 0) ? (double)child->size / total : 0;
		
		if(horizontal) {
			int width = (i == node->nchildren - 1) ? 
				avail.max.x - x : /* use remaining space for last node */
				(int)(ratio * w);
				
			if(width < MINBOX) width = MINBOX;
			
			r = Rect(x, y, x + width, y + h);
			x += width;
		} else {
			int height = (i == node->nchildren - 1) ? 
				avail.max.y - y : /* use remaining space for last node */
				(int)(ratio * h);
				
			if(height < MINBOX) height = MINBOX;
			
			r = Rect(x, y, x + w, y + height);
			y += height;
		}
		
		child->bounds = r;
		acc += ratio;
		
		/* Recursively layout children if this is a directory */
		if(child->isdir && child->nchildren > 0) {
			/* Inset the rectangle to add a small margin */
			Rectangle inset = insetrect(r, 2);
			layout_nodes(child, inset);
		}
	}
}

/* Draw a single file system node */
void
draw_fsnode(FsNode *node)
{
	Rectangle r = node->bounds;
	char label[256];
	Point p;
	
	/* Draw rectangle filled with node color */
	if(node->selected)
		draw(screen, r, accent, nil, ZP);
	else
		draw(screen, r, node->color, nil, ZP);
		
	/* Draw border */
	border(screen, r, 1, bord, ZP);
	
	/* Draw label if box is big enough */
	if(Dx(r) >= 50 && Dy(r) >= 20) {
		snprint(label, sizeof(label), "%s (%s)", node->name, format_size(node->size));
		p = Pt(r.min.x + 5, r.min.y + Dy(r)/2);
		string(screen, p, text, ZP, font, label);
	}
}

/* Draw the status bar */
void
draw_status_bar(void)
{
	Rectangle statusrect = Rect(maprect.min.x, maprect.max.y - STATUSHEIGHT, 
	                            maprect.max.x, maprect.max.y);
	
	draw(screen, statusrect, back, nil, ZP);
	border(screen, statusrect, 1, bord, ZP);
	
	Point p = Pt(statusrect.min.x + 10, statusrect.min.y + STATUSHEIGHT/2);
	string(screen, p, text, ZP, font, status_message);
}

/* Draw the entire interface */
void
draw_interface(void)
{
	Rectangle datarect;
	
	/* Clear screen */
	draw(screen, screen->r, back, nil, ZP);
	
	/* Create rectangle for data visualization (leaving room for status bar) */
	datarect = Rect(maprect.min.x, maprect.min.y, 
	                maprect.max.x, maprect.max.y - STATUSHEIGHT);
	
	/* If we have data, layout and draw nodes */
	if(root != nil) {
		/* Layout nodes starting from root */
		layout_nodes(root, insetrect(datarect, MARGIN));
		
		/* Draw all nodes */
		if(current != nil) {
			int i;
			draw_fsnode(current);
			for(i = 0; i < current->nchildren; i++)
				draw_fsnode(current->children[i]);
		}
	} else {
		/* Display message if no data */
		Point p = Pt((datarect.min.x + datarect.max.x)/2 - 100, 
		             (datarect.min.y + datarect.max.y)/2);
		string(screen, p, text, ZP, font, "No data loaded. Press 'o' to open a directory.");
	}
	
	/* Draw status bar */
	draw_status_bar();
	
	/* Flush changes to screen */
	flushimage(display, 1);
}

/* Find a node at a given point */
FsNode*
find_node_at_point(FsNode *node, Point p)
{
	int i;
	FsNode *found = nil;
	
	if(node == nil)
		return nil;
		
	/* Check children first (front-to-back) */
	for(i = 0; i < node->nchildren; i++) {
		found = find_node_at_point(node->children[i], p);
		if(found != nil)
			return found;
	}
	
	/* Then check this node */
	if(ptinrect(p, node->bounds))
		return node;
		
	return nil;
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
			if(root != nil) {
				clear_fsnode(root);
				root = nil;
				current = nil;
			}
			root = create_fsnode("root", path, 0, 1, nil);
			current = root;
			scan_directory(path, root);
			update_status("Loaded directory: %s", path);
			draw_interface();
		}
		break;
	case 'u':
		/* Go up to parent */
		if(current != nil && current->parent != nil) {
			current = current->parent;
			update_status("Current: %s (%s)", current->name, format_size(current->size));
			draw_interface();
		}
		break;
	case 'r':
		/* Return to root */
		if(root != nil) {
			current = root;
			update_status("Returned to root");
			draw_interface();
		}
		break;
	}
}

/* Switch application mode */
void
switchmode(int newmode)
{
	mode = newmode;
	switch(mode) {
	case NORMAL:
		update_status("Mode: Normal");
		break;
	case NAVIGATING:
		update_status("Mode: Navigating");
		break;
	case ZOOMING:
		update_status("Mode: Zooming");
		break;
	}
}

/* Handle keyboard input */
void
handlekey(Rune key, Event *ev)
{
	switch(mode) {
	case NORMAL:
		navigate(key);
		break;
	case NAVIGATING:
	case ZOOMING:
		/* Handle specific mode keys */
		switch(key) {
		case Kesc:
			switchmode(NORMAL);
			break;
		default:
			navigate(key);
			break;
		}
		break;
	}
}

/* Handle window resize */
void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
		
	maprect = screen->r;
	draw_interface();
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
		
	setupdraw();
	
	/* Initialize and scan the root directory */
	root = create_fsnode("root", path, 0, 1, nil);
	current = root;
	scan_directory(path, root);
	update_status("Loaded directory: %s", path);
	
	/* Main event loop */
	for(;;) {
		draw_interface();
		
		/* Wait for an event */
		switch(event(&ev)) {
		case Emouse:
			/* Handle mouse events */
			if(ev.mouse.buttons & 1) {
				/* Left click: select node */
				FsNode *clicked = find_node_at_point(current, ev.mouse.xy);
				if(clicked != nil) {
					/* Deselect previously selected node if any */
					if(current->selected)
						current->selected = 0;
						
					/* Select the clicked node */
					clicked->selected = 1;
					
					/* If it's a directory with children, make it the current directory */
					if(clicked->isdir && clicked->nchildren > 0) {
						current = clicked;
						update_status("Current: %s (%s)", current->name, format_size(current->size));
					} else {
						update_status("Selected: %s (%s)", clicked->name, format_size(clicked->size));
					}
					
					draw_interface();
				}
			}
			break;
			
		case Ekeyboard:
			handlekey(ev.kbdc, &ev);
			break;
		}
	}
} 