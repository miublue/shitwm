#ifndef __CONFIG_H
#define __CONFIG_H
#include <X11/Xlib.h>
#include <X11/XF86keysym.h>

#define INIT_SCRIPT "$HOME/.shitwmrc &"
#define TITLE_ON_MIDDLE 1
#define TITLEBAR_HEIGHT 18
#define TITLEBAR_FONT "Ioskeley Mono:size=12"
#define BAR_HEIGHT 30
#define BAR_ON_BOTTOM 0
#define BORDER_SIZE 2
#define BORDER_NORMAL "#000000"
#define BORDER_SELECT "#ff5faf"
#define TEXT_NORMAL   "#bcbcbc"
#define TEXT_SELECT   "#000000"
#define MOD Mod4Mask

const char *terminal[]  = { "lterm", 0 };
const char *launcher[]  = { "dmenu_run", 0 };

const char *volup[]     = { "vol", "5%+", 0 };
const char *voldown[]   = { "vol", "5%-", 0 };
const char *volmute[]   = { "vol", "toggle", 0 };

#define DESKTOPCHANGE(key, ws) \
    {MOD, key, ws_switch, {.i = ws}}, \
    {MOD|ShiftMask, key, win_to_ws, {.i = ws}}

static struct key keys[] = {
    {MOD,           XK_Return,               exec,         {.com = terminal}},
    {MOD,           XK_d,                    exec,         {.com = launcher}},

    {MOD,           XK_Tab,                  win_switch,   {.i   = +1}},
    {MOD|ShiftMask, XK_Tab,                  win_switch,   {.i   = -1}},

    {MOD,           XK_f,                    win_full,     {0}},
    {MOD,           XK_c,                    win_center,   {0}},
    {MOD,           XK_space,                win_monocle,  {0}},
    {MOD,           XK_q,                    win_kill,     {0}},

    DESKTOPCHANGE(XK_1, 0), DESKTOPCHANGE(XK_2, 1), DESKTOPCHANGE(XK_3, 2),
    DESKTOPCHANGE(XK_4, 3), DESKTOPCHANGE(XK_5, 4), DESKTOPCHANGE(XK_6, 5),
    DESKTOPCHANGE(XK_7, 6), DESKTOPCHANGE(XK_8, 7), DESKTOPCHANGE(XK_9, 8),
    DESKTOPCHANGE(XK_0, 9),
};

#endif
