/* See LICENSE file for copyright and license details. */

/* appearance */
static const char font[]            = "-*-terminus-medium-r-*-*-14-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#444444";
static const char normbgcolor[]     = "#222222";
static const char normfgcolor[]     = "#bbbbbb";
static const char selbordercolor[]  = "#005577";
static const char selbgcolor[]      = "#005577";
static const char selfgcolor[]      = "#eeeeee";

static const int bordersize = 1;  /* widget border spacing in pixels */

#define MODKEY ControlMask
static Key keys[] = { \
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_q,      quit,           { 0 } },
	{ MODKEY,                       XK_c,      closewindow,    { 0 } },
	{ MODKEY,                       XK_j,      toggleselect,   { .i = +1 } },
	{ MODKEY,                       XK_k,      toggleselect,   { .i = -1 } },
};
