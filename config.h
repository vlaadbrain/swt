/* See LICENSE file for copyright and license details. */

/* appearance */
static const char font[]            = "-*-terminus-medium-r-*-*-14-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#073642";
static const char normbgcolor[]     = "#002936";
static const char normfgcolor[]     = "#93a1a1";
static const char selbordercolor[]  = "#cb4b16";
static const char selbgcolor[]      = "#073642";
static const char selfgcolor[]      = "#93a1a1";

#define MODKEY ControlMask
static Key keys[] = { \
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_q,      killclient,     { 0 } },
};
