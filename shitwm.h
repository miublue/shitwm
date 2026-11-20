#ifndef __SHITWM_H
#define __SHITWM_H
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>

// That's 100 windows per workspace. If you even open that many, you may have a serious problem.
#define MAX_WINDOWS 100
#define MAX_WORKSPACES 10

// This is the file to dump WM information, such as current workspace,
// is current window fullscreen, etc. See function `desktop_info`.
#define INFO_FILE "/tmp/shitwm.info"
#define VERSION "0.0.1"

#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define LEN(x) (sizeof(x) / sizeof((x)[0]))
#define mod_clean(mask) (mask & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

struct rect {
    int x, y, w, h;
};

struct client {
    Window wn, titlebar;
    int is_full, is_mono;
    struct rect rect, prev;
};

struct workspace {
    uint32_t cur, prev, size;
    struct client list[MAX_WINDOWS];
};

union arg { const char **com; const int i; };

struct key {
    unsigned int mod;
    KeySym keysym;
    void (*fun)(const union arg arg);
    const union arg arg;
};

void exec(const union arg arg);
void win_switch(const union arg arg);
void win_full(const union arg arg);
void win_center(const union arg arg);
void win_monocle(const union arg arg);
void win_kill(const union arg arg);
void win_to_ws(const union arg arg);
void ws_switch(const union arg arg);

static void configure_request(XEvent *ev);
static void map_request(XEvent *ev);
static void unmap_notify(XEvent *ev);
static void destroy_notify(XEvent *ev);
static void enter_notify(XEvent *ev);
static void key_press(XEvent *ev);
static void button_press(XEvent *ev);

static void grab_input(void);
static void titlebar_add(int w);
static void titlebar_del(int w);
static void win_add(Window w);
static void win_del(int w);
static void win_focus(int w);
static void update_clients(void);
static int client_from_window(Window w);
static void set_client_size(int w);
static void desktop_info(void);

static int xerror(struct _XDisplay *_d, XErrorEvent *_e) { (void)_d, (void)_e; return 0; }
#endif
