/* Application modes */
enum {
	APP_NORMAL = 0,
	APP_NAVIGATING = 1,
	APP_ZOOMING = 2
};

/* File size multipliers */
#define KB 1024ULL
#define MB (KB*1024ULL)
#define GB (MB*1024ULL)

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
	Image *color;      /* Display color */
	int id;            /* Unique ID for this node */
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

/* Pre-rendered icons */
extern Image *file_icon;   /* File icon image */
extern Image *dir_icon;    /* Directory icon image */

/* Depth-based visualization colors */
extern Image *depth_colors[8];

/* Utility functions */
int min(int a, int b);
char* format_size(u64int size);
void truncate_string(char *s, Font *f, int max_width);

/* Drawing functions */
void setup_draw(void);
void init_colors(void);
void calculate_layout(void);
void draw_header(void);
void draw_footer(void);
void draw_treemap(void);
void draw_node(FsNode *node, int highlight_it);
void draw_node_recursive_contents(FsNode *node);
void draw_ui(void);
void layout_treemap(FsNode *node, Rectangle avail, int depth);
void layout_horizontal(FsNode *node, Rectangle avail, double total_size);
void layout_vertical(FsNode *node, Rectangle avail, double total_size);

/* File system operations */
FsNode* create_fsnode(char *name, char *path, u64int size, int isdir, FsNode *parent);
void add_child(FsNode *parent, FsNode *child);
void scan_directory(char *path, FsNode *parent);
void clear_fsnode(FsNode *node);
void sort_nodes_by_size(FsNode *parent);
void open_directory(char *path);

/* Navigation functions */
void navigate(Rune key);
void handle_key(Rune key);
void navigate_up(void);
void navigate_to_selected(void);
void update_status(char *fmt, ...);

/* Event handling */
void eresized(int new);

/* Utility */
void usage(void);
FsNode* find_node_at_point(FsNode *node, Point p);
int find_list_item_at_point(Point p);
