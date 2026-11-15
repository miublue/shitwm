#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shitwm.h"
#include "config.h"

/* XXX: also keep track of drawing order so titlebars don't get mapped above other windows. */

static Display *display;
static Window root;
static int screen_w, screen_h, cur_ws = 0, topgap = BAR_ON_BOTTOM? 0 : BAR_HEIGHT;
static struct workspace workspaces[MAX_WORKSPACES] = {0};
static XColor border_normal, border_select;
static XftColor text_normal, text_select;
static XftFont *font;
static XButtonEvent button_event;
static XWindowAttributes hover_attr;
static FILE *info_file;

#define CURWS workspaces[cur_ws]
#define WSWIN(wn) CURWS.list[wn]

static void (*events[LASTEvent])(XEvent *ev) = {
    [ConfigureRequest] = configure_request,
    [MapRequest]       = map_request,
    [UnmapNotify]      = unmap_notify,
    [DestroyNotify]    = destroy_notify,
    [MotionNotify]     = motion_notify,
    [EnterNotify]      = enter_notify,
    [KeyPress]         = key_press,
    [ButtonPress]      = button_press,
    [ButtonRelease]    = button_release,
};

void exec(const union arg arg) {
    if (fork() != 0) return;
    if (display) close(ConnectionNumber(display));
    setsid();
    execvp((char*)arg.com[0], (char**)arg.com);
}

void win_switch(const union arg arg) {
    if (CURWS.size < 2 || WSWIN(CURWS.cur).is_full) return;
    const int n = CURWS.cur + arg.i, sz = (int)CURWS.size;
    win_focus(n < 0? sz-1 : n >= sz? 0 : n);
}

void win_full(const union arg arg) {
    (void)arg;
    if (CURWS.size == 0) return;
    XRaiseWindow(display, WSWIN(CURWS.cur).wn);
    if ((WSWIN(CURWS.cur).is_full = !WSWIN(CURWS.cur).is_full))
        XMoveResizeWindow(display, WSWIN(CURWS.cur).wn, -BORDER_SIZE, -BORDER_SIZE, screen_w, screen_h);
    update_clients();
}

void win_center(const union arg arg) {
    (void)arg;
    if (CURWS.size == 0 || button_event.subwindow != None) return;
    struct client *c = &WSWIN(CURWS.cur);
    c->rect.x = (screen_w/2) - (c->rect.w/2);
    c->rect.y = (screen_h/2) - (c->rect.h/2) + TITLEBAR_HEIGHT/2 + topgap;
    if (c->is_full) return;
    update_clients();
}

void win_monocle(const union arg arg) {
    (void)arg;
    if (CURWS.size == 0 || button_event.subwindow != None || WSWIN(CURWS.cur).is_full) return;
    struct client *c = &WSWIN(CURWS.cur);
    const int bs=BORDER_SIZE*2;
    if ((c->is_mono = !c->is_mono)) {
        c->prev = c->rect;
        c->rect = (struct rect) {
            .x = 0,
            .y = topgap+TITLEBAR_HEIGHT,
            .w = screen_w-bs,
            .h = screen_h-BAR_HEIGHT-TITLEBAR_HEIGHT-bs
        };
    } else c->rect = c->prev;
    update_clients();
}

void win_kill(const union arg arg) {
    (void)arg;
    if (CURWS.size == 0) return;
    XKillClient(display, WSWIN(CURWS.cur).wn);
}

void win_to_ws(const union arg arg) {
    if (arg.i == cur_ws || CURWS.size == 0) return;
    int ws = cur_ws;
    struct client c = WSWIN(CURWS.cur);
    titlebar_del(CURWS.cur);
    XUnmapWindow(display, c.wn);
    cur_ws = arg.i;
    win_add(c.wn);
    struct client *w = &WSWIN(CURWS.size-1);
    w->is_mono = c.is_mono, w->rect = c.rect;
    win_focus(CURWS.size-1);
    cur_ws = ws;
    win_del(CURWS.cur);
    update_clients();
}

void ws_switch(const union arg arg) {
    if (arg.i == cur_ws) return;
    for (size_t i = 0; i < CURWS.size; ++i) {
        titlebar_del(i);
        XUnmapWindow(display, WSWIN(i).wn);
    }
    cur_ws = arg.i;
    for (size_t i = 0; i < CURWS.size; ++i) {
        titlebar_add(i);
        XMapWindow(display, WSWIN(i).wn);
    }
    win_focus(CURWS.cur);
    update_clients();
}

static void configure_request(XEvent *ev) {
    XConfigureRequestEvent *cr = &ev->xconfigurerequest;
    XConfigureWindow(display, cr->window, cr->value_mask, &(XWindowChanges) {
        .x = cr->x,
        .y = cr->y,
        .width = cr->width,
        .height = cr->height,
        .sibling = cr->above,
        .stack_mode = cr->detail,
    });
    update_clients();
}

static void map_request(XEvent *ev) {
    Window wn = ev->xmaprequest.window;
    if (wn == None || client_from_window(wn) != -1) return;
    XSelectInput(display, wn, StructureNotifyMask|EnterWindowMask);
    win_add(wn);
    XSetWindowBorderWidth(display, wn, BORDER_SIZE);
    XSetWindowBorder(display, wn, border_normal.pixel);
    titlebar_add(CURWS.size-1);
    XMapWindow(display, wn);
    win_focus(CURWS.size-1);
    win_center((const union arg){0});
    update_clients();
}

static void unmap_notify(XEvent *ev) {
    win_del(client_from_window(ev->xunmap.window));
    update_clients();
}

static void destroy_notify(XEvent *ev) {
    int cur = cur_ws;
    for (cur_ws = 0; cur_ws < MAX_WORKSPACES; ++cur_ws)
        win_del(client_from_window(ev->xdestroywindow.window));
    cur_ws = cur;
    update_clients();
}

static void motion_notify(XEvent *ev) {
    int c = client_from_window(button_event.subwindow);
    if (c == -1 || WSWIN(c).is_full) return;
    WSWIN(c).is_mono = 0;
    win_focus(c);
    const int xdiff = ev->xbutton.x_root - button_event.x_root;
    const int ydiff = ev->xbutton.y_root - button_event.y_root;
    XMoveResizeWindow(display, button_event.subwindow,
        hover_attr.x + (button_event.button==1 ? xdiff : 0),
        hover_attr.y + (button_event.button==1 ? ydiff : 0),
        MAX(50, hover_attr.width  + (button_event.button==3 ? xdiff : 0)),
        MAX(50, hover_attr.height + (button_event.button==3 ? ydiff : 0)));
    set_client_size(c);
    update_clients();
}

static void enter_notify(XEvent *ev) {
    if (button_event.subwindow != None || !CURWS.size || WSWIN(CURWS.cur).is_full) return;
    while (XCheckTypedEvent(display, EnterNotify, ev));
    if (!FOCUS_ON_HOVER) return;
    int c = client_from_window(ev->xcrossing.window);
    if (c != -1) {
        win_focus(c);
        return;
    }
    // these values aren't optional, i'm forced to allocate and free this array
    Window parent, _root, *_children; uint32_t _num_children;
    XQueryTree(display, ev->xcrossing.window, &_root, &parent, &_children, &_num_children);
    if (_num_children) XFree(_children);
    // if window doesn't exist, try focusing its parent
    win_focus(client_from_window(parent));
}

static void key_press(XEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);
    for (size_t i = 0; i < LEN(keys); ++i) {
        if (keys[i].keysym == keysym && mod_clean(keys[i].mod) == mod_clean(ev->xkey.state))
            keys[i].fun(keys[i].arg);
    }
}

static void button_press(XEvent *ev) {
    if (ev->xbutton.subwindow == None || !CURWS.size || WSWIN(CURWS.cur).is_full) return;
    int c = client_from_window(ev->xbutton.subwindow);
    if (c == -1 || WSWIN(c).is_full) return;
    set_client_size(c);
    win_focus(c);
    XGetWindowAttributes(display, WSWIN(c).wn, &hover_attr);
    button_event = ev->xbutton, button_event.subwindow = WSWIN(c).wn;
}

static void button_release(XEvent *ev) {
    (void)ev;
    button_event.subwindow = None;
}

static void grab_input(void) {
    const uint32_t mask = ButtonPressMask|ButtonReleaseMask|PointerMotionMask;
    KeyCode code;
    XUngrabKey(display, AnyKey, AnyModifier, root);
    XUngrabButton(display, AnyButton, AnyModifier, root);
    for (size_t i = 0; i < LEN(keys); ++i) {
        if ((code = XKeysymToKeycode(display, keys[i].keysym)))
            XGrabKey(display, code, keys[i].mod, root, 1, GrabModeAsync, GrabModeAsync);
    }
    XGrabButton(display, AnyButton, MOD, root, 1, mask, GrabModeAsync, GrabModeAsync, None, None);
}

static void titlebar_add(int w) {
    if (w < 0 || CURWS.size == 0) return;
    struct client *c = &WSWIN(w);
#if TITLEBAR_HEIGHT > 0
    c->titlebar = XCreateSimpleWindow(display, root,
        c->rect.x, c->rect.y - TITLEBAR_HEIGHT, c->rect.w, TITLEBAR_HEIGHT,
        0, 0, border_normal.pixel);
    XSetWindowAttributes attr = { .override_redirect = True };
    XChangeWindowAttributes(display, c->titlebar, CWOverrideRedirect, &attr);
    XMapWindow(display, c->titlebar);
#else
    c->titlebar = 0;
#endif
}

static void titlebar_del(int w) {
    if (w < 0 || CURWS.size == 0 || WSWIN(w).titlebar == 0) return;
    XUnmapWindow(display, WSWIN(w).titlebar);
    XDestroyWindow(display, WSWIN(w).titlebar);
    WSWIN(w).titlebar = 0;
}

static void win_add(Window w) {
    if (w == None) return;
    if (CURWS.size) WSWIN(CURWS.cur).is_full = 0;
    assert(CURWS.size < MAX_WINDOWS);
    struct client client = { .wn = w, .is_full = 0, .is_mono = 0 };
    CURWS.list[CURWS.size++] = client;
    set_client_size(CURWS.size-1);
}

static void win_del(int w) {
    if (w < 0 || CURWS.size == 0) return;
    titlebar_del(w);
    WSWIN(w).is_full = 0;
    CURWS.size--;
    for (size_t i = w; i < CURWS.size; ++i) CURWS.list[i] = CURWS.list[i+1];
    win_focus(MIN(CURWS.prev, CURWS.size-1));
}

static void win_focus(int w) {
    if (button_event.subwindow != None) return;
    if (w < 0 || CURWS.size == 0) {
        XSetInputFocus(display, root, RevertToParent, CurrentTime);
        CURWS.cur = CURWS.prev = 0;
        return;
    }
    if (WSWIN(CURWS.cur).is_full) {
        XRaiseWindow(display, WSWIN(CURWS.cur).wn);
        return;
    }
    CURWS.prev = CURWS.cur, CURWS.cur = w;
    struct client *c = &WSWIN(CURWS.cur), *p = &WSWIN(CURWS.prev);
    if (c->titlebar) XRaiseWindow(display, c->titlebar);
    XRaiseWindow(display, c->wn);
    XSetInputFocus(display, c->wn, RevertToParent, CurrentTime);
    XSetWindowBorder(display, p->wn, border_normal.pixel);
    XSetWindowBorder(display, c->wn, border_select.pixel);
    update_clients();
}

static void draw_client_title(struct client *c, int is_focused) {
    if (!c->titlebar) return;
    Colormap cmap = DefaultColormap(display, DefaultScreen(display));
    Visual *visual = DefaultVisual(display, DefaultScreen(display));
    XftColor color = is_focused? text_select : text_normal;
    XftDraw *draw = XftDrawCreate(display, c->titlebar, visual, cmap);
    XGlyphInfo extents;
    XTextProperty prop;

    XSetWindowBackground(display, c->titlebar, is_focused? border_select.pixel : border_normal.pixel);
    XClearWindow(display, c->titlebar);

    if (XGetWMName(display, c->wn, &prop)) {
        const FcChar8 *title = (const FcChar8*)prop.value;
        const int len = strlen((const char*)title);
        XftTextExtentsUtf8(display, font, title, len, &extents);
        const int xoff = TITLE_ON_MIDDLE? (c->rect.w/2) - (extents.width/2) : BORDER_SIZE;
        const int yoff = (TITLEBAR_HEIGHT / 2) + (extents.height / 2);
        XftDrawStringUtf8(draw, &color, font, xoff, yoff, title, len);
        XFree(prop.value);
    }
    XftDrawDestroy(draw);
}

static void update_clients(void) {
    desktop_info();
    if (!CURWS.size || WSWIN(CURWS.cur).is_full) return;
    XEvent ev;
    for (size_t i = CURWS.size; i--;) {
        struct client *c = &WSWIN(i);
        if (c->wn == None) {
            win_del(i);
            continue;
        }
        XMoveResizeWindow(display, c->wn, c->rect.x, c->rect.y, c->rect.w, c->rect.h);
        if (c->titlebar) {
            XMoveResizeWindow(display, c->titlebar, c->rect.x, c->rect.y-TITLEBAR_HEIGHT, c->rect.w+(BORDER_SIZE*2), TITLEBAR_HEIGHT);
            draw_client_title(c, i == CURWS.cur);
        }
    }
    XSync(display, False);
    while (XCheckTypedEvent(display, EnterNotify, &ev));
}

static int client_from_window(Window wn) {
    for (size_t i = 0; i < CURWS.size; ++i) {
        if (WSWIN(i).wn == wn || (WSWIN(i).titlebar && WSWIN(i).titlebar == wn))
            return (int)i;
    }
    return -1;
}

static void set_client_size(int w) {
    if (w < 0 || CURWS.size == 0 || w >= (int)CURWS.size) return;
    struct client *c = &WSWIN(w);
    XWindowAttributes attr;
    XGetWindowAttributes(display, c->wn, &attr);
    c->rect = (struct rect) { attr.x, attr.y, attr.width, attr.height };
}

static void desktop_info(void) {
    struct client c = CURWS.size? WSWIN(CURWS.cur) : (struct client) {0};
    rewind(info_file);
    fprintf(info_file, "%d %d %d %lu %d\n", MAX_WORKSPACES, cur_ws+1, CURWS.size, c.wn, c.is_full);
    fflush(info_file);
}

void usage(const char *prg, int err) {
    printf("usage: %s [-v]\n", prg);
    exit(err);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-v")) { printf("%s %s\n", argv[0], VERSION); return 0; }
        else if (!strcmp(argv[i], "-h")) usage(argv[0], 0);
        else usage(argv[0], 1);
    }
    if (!(info_file = fopen(INFO_FILE, "w+"))) {
        fprintf(stderr, "error: failed to open file '%s'\n", INFO_FILE);
        return 1;
    }
    if (!(display = XOpenDisplay(0))) {
        fprintf(stderr, "error: failed to open display\n");
        return 1;
    }
    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);
    system(INIT_SCRIPT);

    int s = DefaultScreen(display);
    root = RootWindow(display, s), screen_w = XDisplayWidth(display, s), screen_h = XDisplayHeight(display, s);
    Colormap cmap = DefaultColormap(display, s);
    Visual *visual = DefaultVisual(display, s);
    font = XftFontOpenName(display, s, TITLEBAR_FONT);

    XAllocNamedColor(display, cmap, BORDER_NORMAL, &border_normal, &border_normal);
    XAllocNamedColor(display, cmap, BORDER_SELECT, &border_select, &border_select);
    border_normal.pixel |= 0xff << 24, border_select.pixel |= 0xff << 24;
    XftColorAllocName(display, visual, cmap, TEXT_NORMAL, &text_normal);
    XftColorAllocName(display, visual, cmap, TEXT_SELECT, &text_select);
    XSelectInput(display, root, SubstructureRedirectMask);
    XDefineCursor(display, root, XCreateFontCursor(display, 68));
    grab_input();
    desktop_info();
    XEvent ev;
    for (;;) {
        XNextEvent(display, &ev);
        if (events[ev.type]) events[ev.type](&ev);
    }

    XCloseDisplay(display);
    fclose(info_file);
    return 0;
}
