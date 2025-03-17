#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <thread.h>

/* Application modes */
enum {
	NORMAL = 0,
	NAVIGATING = 1,
	ZOOMING = 2
};

/* File size multipliers */
enum {
	KB = 1024,
	MB = 1024*1024,
	GB = 1024*1024*1024
};

/* Structure for file/directory information */
typedef struct FsNode {
	char name[256];
	char path[1024];
	u64int size;
	int isdir;
	struct FsNode *parent;
	struct FsNode **children;
	int nchildren;
	int maxchildren;
	Rectangle bounds;  /* Display rectangle */
	int selected;      /* Selection state */
	Image *color;      /* Display color */
} FsNode;

/* Global variables */
extern int mode;
extern FsNode *root;
extern FsNode *current;
extern struct Image *screen;
extern struct Font *font;
extern struct Display *display;
extern Rectangle maprect;
extern Point viewport;
extern Point pan_start;
extern int panning;

/* Colors */
extern Image *back;    /* Background */
extern Image *high;    /* Highlight */
extern Image *bord;    /* Border */
extern Image *text;    /* Text */
extern Image *file_color;  /* Files */
extern Image *dir_color;   /* Directories */
extern Image *accent;      /* Accent */

/* Function prototypes */
void setupdraw(void);
void initcolors(void);
void scan_directory(char *path, FsNode *parent);
FsNode* create_fsnode(char *name, char *path, u64int size, int isdir, FsNode *parent);
void add_child(FsNode *parent, FsNode *child);
void clear_fsnode(FsNode *node);
void layout_nodes(FsNode *node, Rectangle avail);
void draw_interface(void);
void draw_fsnode(FsNode *node);
void draw_status_bar(void);
void navigate(Rune key);
void switchmode(int newmode);
void handlekey(Rune key, Event *ev);
void resdraw(void);
void eresized(int new);
void usage(void);
FsNode* find_node_at_point(FsNode *node, Point p);
char* format_size(u64int size);
void update_status(char *fmt, ...);
