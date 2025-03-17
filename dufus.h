/* Application modes */
enum {
	NORMAL = 0,
	NAVIGATING = 1,
	ZOOMING = 2
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

/* Function prototypes */
void setup_draw(void);
void init_colors(void);
void calculate_layout(void);
void draw_header(void);
void draw_footer(void);
void draw_treemap(void);
void draw_node(FsNode *node, int highlight_it);
void draw_ui(void);
void layout_treemap(FsNode *node, Rectangle avail);

void scan_directory(char *path, FsNode *parent);
FsNode* create_fsnode(char *name, char *path, u64int size, int isdir, FsNode *parent);
void add_child(FsNode *parent, FsNode *child);
void clear_fsnode(FsNode *node);
void open_directory(char *path);

void navigate(Rune key);
void handle_key(Rune key);
void update_status(char *fmt, ...);
void eresized(int new);
void usage(void);

FsNode* find_node_at_point(FsNode *node, Point p);
char* format_size(u64int size);

/* File system operations */
FsNode* create_fsnode(char *name, char *path, u64int size, int isdir, FsNode *parent);
void add_child(FsNode *parent, FsNode *child);
void scan_directory(char *path, FsNode *parent);
void clear_fsnode(FsNode *node);
void sort_nodes_by_size(FsNode *parent);
void open_directory(char *path);

/* Navigation functions */
void navigate_up(void);
void navigate_to_selected(void);
int find_list_item_at_point(Point p);

/* UI functions */
char* format_size(u64int size);
