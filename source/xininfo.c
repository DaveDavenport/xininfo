/**
 * xininfo
 *
 * MIT/X11 License
 * Copyright (c) 2014-2015 Qball  Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <math.h>
#include <errno.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/randr.h>
#include <xcb/xinerama.h>
#include <xcb/dpms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/screensaver.h>

#define MAX( a, b )                          ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define MIN( a, b )                          ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define INTERSECT( x, y, x1, y1, w1, h1 )    ( ( ( ( x ) >= ( x1 ) ) && ( ( x ) < ( x1 + w1 ) ) ) && ( ( ( y ) >= ( y1 ) ) && ( ( y ) < ( y1 + h1 ) ) ) )

#define TRUE     1
#define FALSE    0

xcb_connection_t      *connection = NULL;
xcb_screen_t          *screen     = NULL;
xcb_ewmh_connection_t ewmh;
int                   screen_nbr = 0;
int                   show_all   = FALSE;

typedef struct
{
    int    w, h;
    double rate;
} MMB_Mode;
// Monitor layout stuff.
typedef struct
{
    int      enabled;
    int      x, y;
    int      w, h;
    char     *name;
    int      primary;
    MMB_Mode *modes;
    int      modes_len;
} MMB_Rectangle;

typedef struct
{
    // Size of the total screen area.
    MMB_Rectangle base;

    // Number of monitors.
    int           num_monitors;
    // List of monitors;
    MMB_Rectangle **monitors;

    // Mouse position
    MMB_Rectangle active_monitor;
} MMB_Screen;

// find active_monitor pointer location
void x11_build_monitor_layout ( MMB_Screen *mmc );

static int pointer_get ( MMB_Screen *screen, xcb_window_t root )
{
    xcb_query_pointer_cookie_t c  = xcb_query_pointer ( connection, root );
    xcb_query_pointer_reply_t  *r = xcb_query_pointer_reply ( connection, c, NULL );
    if ( r ) {
        screen->active_monitor.x = r->root_x;
        screen->active_monitor.y = r->root_y;
        free ( r );
        return TRUE;
    }

    return FALSE;
}
static int mmb_screen_get_current_desktop ( MMB_Screen *screen )
{
    unsigned int              current_desktop = 0;
    xcb_get_property_cookie_t gcdc;
    gcdc = xcb_ewmh_get_current_desktop ( &ewmh, screen_nbr );
    if  ( xcb_ewmh_get_current_desktop_reply ( &ewmh, gcdc, &current_desktop, NULL ) ) {
        xcb_get_property_cookie_t             c = xcb_ewmh_get_desktop_viewport ( &ewmh, screen_nbr );
        xcb_ewmh_get_desktop_viewport_reply_t vp;
        if ( xcb_ewmh_get_desktop_viewport_reply ( &ewmh, c, &vp, NULL ) ) {
            if ( current_desktop < vp.desktop_viewport_len ) {
                screen->active_monitor.x = vp.desktop_viewport[current_desktop].x;
                screen->active_monitor.y = vp.desktop_viewport[current_desktop].y;
                xcb_ewmh_get_desktop_viewport_reply_wipe ( &vp );
                return TRUE;
            }
            xcb_ewmh_get_desktop_viewport_reply_wipe ( &vp );
        }
    }
    return FALSE;
}

/**
 * @param display An X11 Display
 *
 * Create MMB_Screen that holds the monitor layout of display.
 *
 * @returns filled in MMB_Screen
 */
static MMB_Screen *mmb_screen_create ( int screen_nbr )
{
    // Create empty structure.
    MMB_Screen *retv = malloc ( sizeof ( *retv ) );
    memset ( retv, 0, sizeof ( *retv ) );

    screen       = xcb_aux_get_screen ( connection, screen_nbr );
    retv->base.w = screen->width_in_pixels;
    retv->base.h = screen->height_in_pixels;

    x11_build_monitor_layout ( retv );
    // monitor_active
    if ( !mmb_screen_get_current_desktop ( retv ) ) {
        if ( pointer_get ( retv, screen->root ) ) {
            fprintf ( stderr, "Failed to find monitor\n" );
        }
    }

    return retv;
}
/**
 * Create monitor based on output id
 */
static MMB_Rectangle * x11_get_monitor_from_output ( xcb_randr_output_t out, xcb_randr_mode_info_t *modes, int modes_len )
{
    xcb_randr_get_output_info_reply_t  *op_reply;
    xcb_randr_get_crtc_info_reply_t    *crtc_reply;
    xcb_randr_get_output_info_cookie_t it = xcb_randr_get_output_info ( connection, out, XCB_CURRENT_TIME );
    op_reply = xcb_randr_get_output_info_reply ( connection, it, NULL );

    if ( op_reply->num_modes == 0 ) {
        // No monitor attached.
        free ( op_reply );
        return NULL;
    }
    MMB_Rectangle *retv = malloc ( sizeof ( MMB_Rectangle ) );
    memset ( retv, '\0', sizeof ( MMB_Rectangle ) );
    if ( op_reply->crtc != XCB_NONE ) {
        xcb_randr_get_crtc_info_cookie_t ct = xcb_randr_get_crtc_info ( connection, op_reply->crtc, XCB_CURRENT_TIME );
        crtc_reply = xcb_randr_get_crtc_info_reply ( connection, ct, NULL );
        if ( crtc_reply ) {
            retv->enabled = TRUE;
            retv->x       = crtc_reply->x;
            retv->y       = crtc_reply->y;
            retv->w       = crtc_reply->width;
            retv->h       = crtc_reply->height;
            free ( crtc_reply );
        }
    }

    retv->modes_len = op_reply->num_modes;
    retv->modes     = malloc ( sizeof ( MMB_Mode ) * op_reply->num_modes );
    xcb_randr_mode_t *modesr = xcb_randr_get_output_info_modes ( op_reply );
    for ( int i = 0; i < op_reply->num_modes; i++ ) {
        for ( int j = 0; j < modes_len; j++ ) {
            if ( modesr[i] == modes[j].id ) {
                retv->modes[i].w    = modes[j].width;
                retv->modes[i].h    = modes[j].height;
                retv->modes[i].rate = modes[j].dot_clock / (double) ( modes[j].htotal * modes[j].vtotal );
            }
        }
    }

    char *tname    = (char *) xcb_randr_get_output_info_name ( op_reply );
    int  tname_len = xcb_randr_get_output_info_name_length ( op_reply );

    retv->name = malloc ( ( tname_len + 1 ) * sizeof ( char ) );
    memcpy ( retv->name, tname, tname_len );
    retv->name[tname_len] = '\0';
    free ( op_reply );
    return retv;
}

static void x11_build_monitor_layout_xinerama ( MMB_Screen *mmc )
{
    xcb_xinerama_query_screens_cookie_t screens_cookie = xcb_xinerama_query_screens_unchecked ( connection );

    xcb_xinerama_query_screens_reply_t  *screens_reply = xcb_xinerama_query_screens_reply ( connection,
                                                                                            screens_cookie,
                                                                                            NULL
                                                                                            );

    xcb_xinerama_screen_info_iterator_t screens_iterator = xcb_xinerama_query_screens_screen_info_iterator (
        screens_reply
        );

    for (; screens_iterator.rem > 0; xcb_xinerama_screen_info_next ( &screens_iterator ) ) {
        mmc->monitors = realloc ( mmc->monitors, sizeof ( MMB_Rectangle* ) * ( mmc->num_monitors + 1 ) );
        MMB_Rectangle *w = malloc ( sizeof ( MMB_Rectangle ) );
        memset ( w, '\0', sizeof ( MMB_Rectangle ) );
        w->x                             = screens_iterator.data->x_org;
        w->y                             = screens_iterator.data->y_org;
        w->w                             = screens_iterator.data->width;
        w->h                             = screens_iterator.data->height;
        mmc->monitors[mmc->num_monitors] = w;
        mmc->num_monitors++;
    }

    free ( screens_reply );
}
static int x11_is_extension_present ( const char *extension )
{
    xcb_query_extension_cookie_t randr_cookie = xcb_query_extension ( connection, strlen ( extension ), extension );
    xcb_query_extension_reply_t  *randr_reply = xcb_query_extension_reply ( connection, randr_cookie, NULL );
    int                          present      = randr_reply->present;
    free ( randr_reply );
    return present;
}

void x11_build_monitor_layout ( MMB_Screen *mmc )
{
    // If RANDR is not available, try Xinerama
    if ( !x11_is_extension_present ( "RANDR" ) ) {
        // Check if xinerama is available.
        if ( x11_is_extension_present ( "XINERAMA" ) ) {
            x11_build_monitor_layout_xinerama ( mmc );
            return;
        }
        fprintf ( stderr, "No RANDR or Xinerama available for getting monitor layout." );
        return;
    }

    xcb_randr_get_screen_resources_current_reply_t  *res_reply;
    xcb_randr_get_screen_resources_current_cookie_t src;
    src       = xcb_randr_get_screen_resources_current ( connection, screen->root );
    res_reply = xcb_randr_get_screen_resources_current_reply ( connection, src, NULL );
    if ( !res_reply ) {
        return;  //just report error
    }
    int                   mon_num   = xcb_randr_get_screen_resources_current_outputs_length ( res_reply );
    xcb_randr_output_t    *ops      = xcb_randr_get_screen_resources_current_outputs ( res_reply );
    xcb_randr_mode_info_t *modes    = xcb_randr_get_screen_resources_current_modes ( res_reply );
    int                   modes_len = xcb_randr_get_screen_resources_current_modes_length ( res_reply );

    // Get primary.
    xcb_randr_get_output_primary_cookie_t pc      = xcb_randr_get_output_primary ( connection, screen->root );
    xcb_randr_get_output_primary_reply_t  *pc_rep = xcb_randr_get_output_primary_reply ( connection, pc, NULL );

    for ( int i = mon_num - 1; i >= 0; i-- ) {
        MMB_Rectangle *w = x11_get_monitor_from_output ( ops[i], modes, modes_len );
        if ( w ) {
            mmc->monitors                    = realloc ( mmc->monitors, ( mmc->num_monitors + 1 ) * sizeof ( MMB_Rectangle* ) );
            mmc->monitors[mmc->num_monitors] = w;
            if ( pc_rep && pc_rep->output == ops[i] ) {
                w->primary = TRUE;
            }
            mmc->num_monitors++;
        }
    }
    // If exists, free primary output reply.
    if ( pc_rep ) {
        free ( pc_rep );
    }
    free ( res_reply );
}

/**
 * @param screen a Pointer to the MMB_Screen pointer to free.
 *
 * Free MMB_Screen object and set pointer to NULL.
 */
static void mmb_screen_free ( MMB_Screen **screen )
{
    if ( screen == NULL || *screen == NULL ) {
        return;
    }

    if ( ( *screen )->active_monitor.name ) {
        free ( ( *screen )->active_monitor.name );
    }

    for ( int i = 0; i < ( *screen )->num_monitors; i++ ) {
        if ( ( *screen )->monitors[i]->name ) {
            free ( ( *screen )->monitors[i]->name );
            if ( ( *screen )->monitors[i]->modes_len > 0 ) {
                free (  ( *screen )->monitors[i]->modes );
            }
            free ( ( *screen )->monitors[i] );
        }
    }

    if ( ( *screen )->monitors != NULL ) {
        free ( ( *screen )->monitors );
    }

    free ( *screen );
    screen = NULL;
}

static int mmb_screen_get_active_monitor ( MMB_Screen *screen )
{
    for ( int i = 0; i < screen->num_monitors; i++ ) {
        if ( INTERSECT ( screen->active_monitor.x, screen->active_monitor.y,
                         screen->monitors[i]->x,
                         screen->monitors[i]->y,
                         screen->monitors[i]->w,
                         screen->monitors[i]->h ) ) {
            return i;
        }
    }

    return 0;
}

static void mmb_screen_print ( MMB_Screen *screen )
{
    printf ( "Total size:    %d %d\n", screen->base.w, screen->base.h );
    printf ( "Num. monitors: %d\n", screen->num_monitors );

    for ( int i = 0; i < screen->num_monitors; i++ ) {
        printf ( "               %01d: %d %d -> %d %d (%s) %s\n",
                 i,
                 screen->monitors[i]->x,
                 screen->monitors[i]->y,
                 screen->monitors[i]->w,
                 screen->monitors[i]->h,
                 screen->monitors[i]->name,
                 screen->monitors[i]->enabled ? "" : "(disabled)"
                 );
    }

    int active_monitor = mmb_screen_get_active_monitor ( screen );
    printf ( "Active mon:    %d\n", active_monitor );
    printf ( "               %d-%d\n", screen->active_monitor.x, screen->active_monitor.y );
}

static void screensaver ( char **argv )
{
    (void) ( argv );
    if ( !x11_is_extension_present ( "MIT-SCREEN-SAVER" ) ) {
        printf ( "unavailable\n" );
        return;
    }
    xcb_screensaver_query_info_cookie_t c  = xcb_screensaver_query_info ( connection, screen->root );
    xcb_screensaver_query_info_reply_t  *r = xcb_screensaver_query_info_reply ( connection, c, NULL );
    if ( r ) {
        switch ( r->state )
        {
        case XCB_SCREENSAVER_STATE_OFF:
            printf ( "off\n" );
            break;
        case XCB_SCREENSAVER_STATE_ON:
            printf ( "on\n" );
            break;
        case XCB_SCREENSAVER_STATE_CYCLE:
            printf ( "cycle\n" );
            break;
        default:
            printf ( "disabled\n" );
            break;
        }
        free ( r );
    }
    else{
        printf ( "n\\a\n" );
    }
}
static void screensaver_print ( char **argv )
{
    (void) ( argv );
    printf ( "screensaver:     " );
    screensaver ( NULL );
}

static void dpms_state ( char **argv )
{
    (void ) ( argv );
    xcb_dpms_capable_cookie_t c  = xcb_dpms_capable ( connection );
    xcb_dpms_capable_reply_t  *r = xcb_dpms_capable_reply ( connection, c, NULL );

    if ( r && r->capable == 0 ) {
        printf ( "incapable\n" );
        free ( r );
        return;
    }
    free ( r );
    xcb_dpms_info_cookie_t ic  = xcb_dpms_info ( connection );
    xcb_dpms_info_reply_t  *ir = xcb_dpms_info_reply ( connection, ic, NULL );
    if ( ir ) {
        if ( ir->state ) {
            switch ( ir->power_level )
            {
            case XCB_DPMS_DPMS_MODE_ON:
                printf ( "on\n" );
                break;
            case XCB_DPMS_DPMS_MODE_STANDBY:
                printf ( "standby\n" );
                break;
            case XCB_DPMS_DPMS_MODE_SUSPEND:
                printf ( "suspend\n" );
                break;
            case XCB_DPMS_DPMS_MODE_OFF:
                printf ( "off\n" );
                break;
            default:
                break;
            }
        }
        else {
            printf ( "disabled\n" );
        }

        free ( ir );
    }
}

static void dpms_print ( char ** argv )
{
    (void ) ( argv );
    xcb_dpms_capable_cookie_t c  = xcb_dpms_capable ( connection );
    xcb_dpms_capable_reply_t  *r = xcb_dpms_capable_reply ( connection, c, NULL );

    if ( r && r->capable == 0 ) {
        printf ( "dpms:          incapable\n" );
        free ( r );
        return;
    }
    free ( r );
    xcb_dpms_info_cookie_t ic  = xcb_dpms_info ( connection );
    xcb_dpms_info_reply_t  *ir = xcb_dpms_info_reply ( connection, ic, NULL );
    if ( ir ) {
        if ( ir->state ) {
            printf ( "dpms:          capable\nstate:         " );
            switch ( ir->power_level )
            {
            case XCB_DPMS_DPMS_MODE_ON:
                printf ( "on\n" );
                break;
            case XCB_DPMS_DPMS_MODE_STANDBY:
                printf ( "standby\n" );
                break;
            case XCB_DPMS_DPMS_MODE_SUSPEND:
                printf ( "suspend\n" );
                break;
            case XCB_DPMS_DPMS_MODE_OFF:
                printf ( "off\n" );
                break;
            default:
                break;
            }
        }
        else {
            printf ( "dpms: disabled\n" );
        }

        free ( ir );
    }
}

static int           monitor_pos   = 0;
static MMB_Screen    *mmb_screen   = NULL;
static MMB_Rectangle *selected_mon = NULL;

static void set_monitor ( char **argv )
{
    monitor_pos = atoi ( argv[1] );

    if ( !( monitor_pos >= 0 && monitor_pos < mmb_screen->num_monitors ) ) {
        fprintf ( stderr, "Invalid monitor: %d (0 <= %d < %d failed)\n",
                  monitor_pos,
                  monitor_pos,
                  mmb_screen->num_monitors );
        // Cleanup
        exit ( EXIT_FAILURE );
    }
    selected_mon = mmb_screen->monitors[monitor_pos];
}
static void print_active_mon ( char **argv )
{
    (void ) ( argv );
    int active_mon = mmb_screen_get_active_monitor ( mmb_screen );
    printf ( "%d\n", active_mon );
}
static void print_mon_size ( char **argv )
{
    (void ) ( argv );
    printf ( "%i %i\n", selected_mon->w, selected_mon->h );
}
static void print_mon_width ( char **argv )
{
    (void ) ( argv );
    printf ( "%d\n", selected_mon->w );
}
static void print_mon_height ( char **argv )
{
    (void ) ( argv );
    printf ( "%d\n", selected_mon->h );
}
static void print_mon_x ( char **argv )
{
    (void ) ( argv );
    printf ( "%d\n", selected_mon->x );
}
static void print_mon_y ( char **argv )
{
    (void ) ( argv );
    printf ( "%d\n", selected_mon->y );
}

static void print_mon_pos ( char **argv )
{
    (void ) ( argv );
    printf ( "%i %i\n", selected_mon->x, selected_mon->y );
}
static void print_max_mon_width ( char **argv )
{
    (void ) ( argv );
    int maxw = 0;

    for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
        maxw = MAX ( maxw, mmb_screen->monitors[i]->w );
    }

    printf ( "%i\n", maxw );
}
static void print_max_mon_height ( char **argv )
{
    (void ) ( argv );
    int maxh = 0;

    for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
        maxh = MAX ( maxh, mmb_screen->monitors[i]->h );
    }

    printf ( "%i\n", maxh );
}
static void print_mon_name ( char **argv )
{
    (void ) ( argv );
    if ( mmb_screen->monitors[monitor_pos]->name ) {
        printf ( "%s\n", mmb_screen->monitors[monitor_pos]->name );
    }
    else {
        printf ( "unknown\n" );
    }
}
static void print_num_mon ( char **argv )
{
    (void ) ( argv );
    printf ( "%u\n", mmb_screen->num_monitors );
}
static void print ( char **argv )
{
    (void ) ( argv );
    mmb_screen_print ( mmb_screen );
}

static void print_mon_modes  ( char ** argv )
{
    (void ) ( argv );
    for ( int i = 0; i < selected_mon->modes_len; i++ ) {
        printf ( "%d %d @ %.2f\n", selected_mon->modes[i].w, selected_mon->modes[i].h, selected_mon->modes[i].rate );
    }
}
static void print_help ( char ** );
typedef struct _CmdOptions
{
    const char *handle;
    const int  n_args;
    void ( *callback )( char **start );
    const char *description;
} CmdOptions;

static const CmdOptions options[] = {
    {
        .handle      = "-monitor",
        .n_args      = 1,
        .callback    = set_monitor,
        .description = "Select monitor by id. By default it uses the active monitor as indicated by the window manager."
    },
    {
        .handle      = "-active-mon",
        .n_args      = 0,
        .callback    = print_active_mon,
        .description = "Print the monitor id indicated by the window manager to hold the focus."
    },
    {
        .handle      = "-mon-size",
        .n_args      = 0,
        .callback    = print_mon_size,
        .description = "Get the size of the selected monitor."
    },
    {
        .handle      = "-mon-width",
        .n_args      = 0,
        .callback    = print_mon_width,
        .description = "Get the width of the selected monitor."
    },
    {
        .handle      = "-max-mon-width",
        .n_args      = 0,
        .callback    = print_max_mon_width,
        .description = "Get largest monitor width."
    },
    {
        .handle      = "-max-mon-height",
        .n_args      = 0,
        .callback    = print_max_mon_height,
        .description = "Get the largest monitor height."
    },
    {
        .handle      = "-mon-height",
        .n_args      = 0,
        .callback    = print_mon_height,
        .description = "Get the selected monitor height."
    },
    {
        .handle      = "-mon-x",
        .n_args      = 0,
        .callback    = print_mon_x,
        .description = "Get the selected monitor horizontal (x) position."
    },
    {
        .handle      = "-mon-y",
        .n_args      = 0,
        .callback    = print_mon_y,
        .description = "Get the selected monitor vertical (y) position."
    },
    {
        .handle      = "-mon-pos",
        .n_args      = 0,
        .callback    = print_mon_pos,
        .description = "Get the selected monitor position (x y)."
    },
    {
        .handle      = "-num-mon",
        .n_args      = 0,
        .callback    = print_num_mon,
        .description = "Get the number of enabled monitors."
    },
    {
        .handle      = "-dpms",
        .n_args      = 0,
        .callback    = dpms_print,
        .description = "Get the dpms state."
    },
    {
        .handle      = "-dpms-state",
        .n_args      = 0,
        .callback    = dpms_state,
        .description = "Get the dpms state (parsable)."
    },
    {
        .handle      = "-screensaver",
        .n_args      = 0,
        .callback    = screensaver_print,
        .description = "Get the screensaver state."
    },
    {
        .handle      = "-screensaver-state",
        .n_args      = 0,
        .callback    = screensaver,
        .description = "Get the screensaver state (parsable)"
    },
    {
        .handle      = "-print",
        .n_args      = 0,
        .callback    = print,
        .description = "Print monitor(s) layout."
    },
    {
        .handle      = "-name",
        .n_args      = 0,
        .callback    = print_mon_name,
        .description = "Print monitor name."
    },
    {
        .handle      = "-modes",
        .n_args      = 0,
        .callback    = print_mon_modes,
        .description = "Print monitors supported modes."
    },

    {
        .handle      = "-h",
        .n_args      = 0,
        .callback    = print_help,
        .description = "Print this help message."
    },
};
const unsigned int      num_options = sizeof ( options ) / sizeof ( CmdOptions );

static void print_help ( char **argv )
{
    (void ) ( argv );
    printf ( "xinfino usage:\n" );
    printf ( "       xininfo [-option ....]\n" );
    printf ( "\n" );
    printf ( "Command line options:\n" );
    for ( unsigned int i = 0; i < num_options; i++ ) {
        printf ( " %*s %s -  %s\n", 20, options[i].handle, options[i].n_args > 0 ? "{arguments}" : "           ", options[i].description );
    }
    printf ( "\n" );
    printf ( "These arguments can be chained, e.g. xininfo -monitor 1 -mon-size -monitor 2 -mon-size.\n" );
    printf ( "Will print first the size of monitor 1 then monitor 2.\n" );
}
/**
 *  Function to handle arguments.
 */
static int handle_arg ( int argc, char **argv )
{
    for ( unsigned int i = 0; i < num_options; i++ ) {
        if ( strcmp ( options[i].handle, argv[0] ) == 0 ) {
            if ( argc > options[i].n_args ) {
                options[i].callback ( argv );
                return options[i].n_args;
            }
            else {
                fprintf ( stderr, "Option: %s requires %d arguments.\n", options[i].handle, options[i].n_args );
                exit ( EXIT_FAILURE );
            }
        }
    }
    fprintf ( stderr, "Commandline option: '%s' not found.\n", argv[0] );
    return 0;
}

static void cleanup ( void )
{
    // Cleanup
    mmb_screen_free ( &mmb_screen );
    xcb_ewmh_connection_wipe ( &( ewmh ) );
    xcb_disconnect ( connection );
}

int main ( int argc, char **argv )
{
    atexit ( cleanup );

    // Get DISPLAY
    const char *display_str = getenv ( "DISPLAY" );
    connection = xcb_connect ( display_str, &screen_nbr );
    if ( xcb_connection_has_error ( connection ) ) {
        fprintf ( stderr, "Failed to open display: %s", display_str );
        return EXIT_FAILURE;
    }
    xcb_intern_atom_cookie_t *ac     = xcb_ewmh_init_atoms ( connection, &ewmh );
    xcb_generic_error_t      *errors = NULL;
    xcb_ewmh_init_atoms_replies ( &ewmh, ac, &errors );
    if ( errors ) {
        fprintf ( stderr, "Failed to create EWMH atoms\n" );
        free ( errors );
    }

    // Get monitor layout. (xinerama aware)
    mmb_screen = mmb_screen_create ( screen_nbr );

    if ( mmb_screen->num_monitors == 0 ) {
        fprintf ( stderr, "No monitor found.\n" );
        return EXIT_FAILURE;
    }

    monitor_pos  = mmb_screen_get_active_monitor ( mmb_screen );
    selected_mon = mmb_screen->monitors[monitor_pos];

    for ( int ac = 1; ac < argc; ac++ ) {
        //
        ac += handle_arg ( argc - ac, &argv[ac] );
    }
    return EXIT_SUCCESS;
}
