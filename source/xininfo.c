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

#define MAX( a, b )                                ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define MIN( a, b )                                ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define NEAR( a, o, b )                            ( ( b ) > ( a ) - ( o ) && ( b ) < ( a ) + ( o ) )
#define OVERLAP( a, b, c, d )                      ( ( ( a ) == ( c ) && ( b ) == ( d ) ) || MIN ( ( a ) + ( b ), ( c ) + ( d ) ) - MAX ( ( a ), ( c ) ) > 0 )
#define INTERSECT( x, y, w, h, x1, y1, w1, h1 )    ( OVERLAP ( ( x ), ( w ), ( x1 ), ( w1 ) ) && OVERLAP ( ( y ), ( h ), ( y1 ), ( h1 ) ) )

#define TRUE     1
#define FALSE    0

xcb_connection_t      *connection = NULL;
xcb_screen_t          *screen     = NULL;
xcb_ewmh_connection_t ewmh;
int                   screen_nbr = 0;

// Monitor layout stuff.
typedef struct
{
    int  x, y;
    int  w, h;
    char *name;
    int  primary;
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
static MMB_Rectangle * x11_get_monitor_from_output ( xcb_randr_output_t out )
{
    xcb_randr_get_output_info_reply_t  *op_reply;
    xcb_randr_get_crtc_info_reply_t    *crtc_reply;
    xcb_randr_get_output_info_cookie_t it = xcb_randr_get_output_info ( connection, out, XCB_CURRENT_TIME );
    op_reply = xcb_randr_get_output_info_reply ( connection, it, NULL );
    if ( op_reply->crtc == XCB_NONE ) {
        free ( op_reply );
        return NULL;
    }
    xcb_randr_get_crtc_info_cookie_t ct = xcb_randr_get_crtc_info ( connection, op_reply->crtc, XCB_CURRENT_TIME );
    crtc_reply = xcb_randr_get_crtc_info_reply ( connection, ct, NULL );
    if ( !crtc_reply ) {
        free ( op_reply );
        return NULL;
    }
    MMB_Rectangle *retv = malloc ( sizeof ( MMB_Rectangle ) );
    memset ( retv, '\0', sizeof ( MMB_Rectangle ) );
    retv->x = crtc_reply->x;
    retv->y = crtc_reply->y;
    retv->w = crtc_reply->width;
    retv->h = crtc_reply->height;

    char *tname    = (char *) xcb_randr_get_output_info_name ( op_reply );
    int  tname_len = xcb_randr_get_output_info_name_length ( op_reply );

    retv->name = malloc ( ( tname_len + 1 ) * sizeof ( char ) );
    memcpy ( retv->name, tname, tname_len );

    free ( crtc_reply );
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
    int                mon_num = xcb_randr_get_screen_resources_current_outputs_length ( res_reply );
    xcb_randr_output_t *ops    = xcb_randr_get_screen_resources_current_outputs ( res_reply );

    // Get primary.
    xcb_randr_get_output_primary_cookie_t pc      = xcb_randr_get_output_primary ( connection, screen->root );
    xcb_randr_get_output_primary_reply_t  *pc_rep = xcb_randr_get_output_primary_reply ( connection, pc, NULL );

    for ( int i = mon_num - 1; i >= 0; i-- ) {
        MMB_Rectangle *w = x11_get_monitor_from_output ( ops[i] );
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
        if ( INTERSECT ( screen->active_monitor.x, screen->active_monitor.y, 1, 1,
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
        printf ( "               %01d: %d %d -> %d %d\n",
                 i,
                 screen->monitors[i]->x,
                 screen->monitors[i]->y,
                 screen->monitors[i]->w,
                 screen->monitors[i]->h
                 );
    }

    int active_monitor = mmb_screen_get_active_monitor ( screen );
    printf ( "Active mon:    %d\n", active_monitor );
    printf ( "               %d-%d\n", screen->active_monitor.x, screen->active_monitor.y );
}

static void screensaver ( void )
{
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
static void screensaver_print ( void )
{
    printf ( "screensaver:     " );
    screensaver ();
}

static void dpms_state ( void )
{
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

static void dpms_print ( void )
{
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
        .description = ""
    },
    {
        .handle      = "-mon-width",
        .n_args      = 0,
        .callback    = print_mon_width,
        .description = ""
    },
    {
        .handle      = "-mon-height",
        .n_args      = 0,
        .callback    = print_mon_height,
        .description = ""
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
    printf ( "Command line arguments:\n" );
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
    if ( strcmp ( argv[0], "-mon-width" ) == 0 ) {
        printf ( "%i\n", mmb_screen->monitors[monitor_pos]->w );
    }
    else if ( strcmp ( argv[0], "-mon-height" ) == 0 ) {
        printf ( "%i\n", mmb_screen->monitors[monitor_pos]->h );
    }
    else if ( strcmp ( argv[0], "-mon-x" ) == 0 ) {
        printf ( "%i\n", mmb_screen->monitors[monitor_pos]->x );
    }
    else if ( strcmp ( argv[0], "-mon-y" ) == 0 ) {
        printf ( "%i\n", mmb_screen->monitors[monitor_pos]->y );
    }
    else if ( strcmp ( argv[0], "-mon-pos" ) == 0 ) {
        printf ( "%i %i\n", mmb_screen->monitors[monitor_pos]->x, mmb_screen->monitors[monitor_pos]->y );
    }
    else if ( strcmp ( argv[0], "-num-mon" ) == 0 ) {
        printf ( "%u\n", mmb_screen->num_monitors );
    }
    else if ( strcmp ( argv[0], "-name" ) == 0 ) {
        if ( mmb_screen->monitors[monitor_pos]->name ) {
            printf ( "%s\n", mmb_screen->monitors[monitor_pos]->name );
        }
        else {
            printf ( "unknown\n" );
        }
    }
    else if ( strcmp ( argv[0], "--max-mon-width" ) == 0 ||
              strcmp ( argv[0], "-max-mon-width" ) == 0 ) {
        int maxw = 0;

        for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
            maxw = MAX ( maxw, mmb_screen->monitors[i]->w );
        }

        printf ( "%i\n", maxw );
    }
    else if ( strcmp ( argv[0], "--max-mon-height" ) == 0 ||
              strcmp ( argv[0], "-max-mon-height" ) == 0 ) {
        int maxh = 0;

        for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
            maxh = MAX ( maxh, mmb_screen->monitors[i]->h );
        }

        printf ( "%i\n", maxh );
    }
    else if ( strcmp ( argv[0], "-print" ) == 0 ) {
        mmb_screen_print ( mmb_screen );
    }
    else if ( strcmp ( argv[0], "-dpms" ) == 0 ) {
        dpms_print ( );
    }
    else if ( strcmp ( argv[0], "-dpms-monitor-state" ) == 0 ) {
        dpms_state ( );
    }
    else if ( strcmp ( argv[0], "-screensaver" ) == 0 ) {
        screensaver_print ( );
    }
    else if ( strcmp ( argv[0], "-screensaver-state" ) == 0 ) {
        screensaver ( );
    }

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
