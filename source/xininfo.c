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
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/dpms.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NEAR(a,o,b) ((b) > (a)-(o) && (b) < (a)+(o))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))

// CLI: argument parsing
static int find_arg( const int argc, char * const argv[], const char * const key )
{
    int i;

    for ( i = 0; i < argc && strcasecmp( argv[i], key ); i++ );

    return i < argc ? i: -1;
}

// Monitor layout stuff.
typedef struct {
    int x,y;
    int w,h;
    char *name;
} MMB_Rectangle;

typedef struct {
    // Size of the total screen area.
    MMB_Rectangle base;

    // Number of monitors.
    int num_monitors;
    // List of monitors;
    MMB_Rectangle *monitors;

    // Mouse position
    MMB_Rectangle active_monitor;

} MMB_Screen;

// find active_monitor pointer location
static int pointer_get( Display *display, Window root, int *x, int *y )
{
    *x = 0;
    *y = 0;
    Window rr, cr;
    int rxr, ryr, wxr, wyr;
    unsigned int mr;

    if ( XQueryPointer( display, root, &rr, &cr, &rxr, &ryr, &wxr, &wyr, &mr ) ) {
        *x = rxr;
        *y = ryr;
        return 1;
    }

    return 0;
}
// retrieve a property of any type from a window
static int window_get_prop( Display *display, Window w, Atom prop, Atom *type, int *items, void *buffer, unsigned int bytes )
{
    Atom _type;

    if ( !type ) type = &_type;

    int _items;

    if ( !items ) items = &_items;

    int format;
    unsigned long nitems, nbytes;
    unsigned char *ret = NULL;
    memset( buffer, 0, bytes );

    if ( XGetWindowProperty( display, w, prop, 0, bytes/4, False, AnyPropertyType, type,
                             &format, &nitems, &nbytes, &ret ) == Success && ret && *type != None && format ) {
        if ( format ==  8 ) memmove( buffer, ret, MIN( bytes, nitems ) );

        if ( format == 16 ) memmove( buffer, ret, MIN( bytes, nitems * sizeof( short ) ) );

        if ( format == 32 ) memmove( buffer, ret, MIN( bytes, nitems * sizeof( long ) ) );

        *items = ( int )nitems;
        XFree( ret );
        return 1;
    }

    return 0;
}

static XWindowAttributes* window_get_attributes( Display *display,Window w )
{
    XWindowAttributes *cattr = malloc( sizeof( XWindowAttributes ) );

    if ( XGetWindowAttributes( display, w, cattr ) ) {
        return cattr;
    }

    return NULL;
}
// determine which monitor holds the active window, or failing that the active_monitor pointer
static void monitor_active( Display *display, MMB_Screen *mmb_screen )
{
    Screen* screen = DefaultScreenOfDisplay( display );
    Window root = RootWindow( display, XScreenNumberOfScreen( screen ) );

    Window id;
    Atom type;
    int count;

    Atom reqatom = XInternAtom ( display,"_NET_ACTIVE_WINDOW" , False );

    if ( window_get_prop( display, root, reqatom, &type, &count, &id, sizeof( Window ) )
         && type == XA_WINDOW && count > 0 ) {
        XWindowAttributes *attr = window_get_attributes( display, id );

        if ( attr != NULL ) {
            Window junkwin;
            int x,y;

            if ( XTranslateCoordinates ( display, id, attr->root,
                                         -attr->border_width,
                                         -attr->border_width,
                                         &x, &y, &junkwin ) == True ) {
                mmb_screen->active_monitor.x = x;
                mmb_screen->active_monitor.y = y;
            }

            free( attr );
            return;
        }
    }

    int x, y;

    if ( pointer_get( display, root, &x, &y ) ) {
        mmb_screen->active_monitor.x = x;
        mmb_screen->active_monitor.y = y;
        return;
    }
}
/**
 * @param display An X11 Display
 *
 * Create MMB_Screen that holds the monitor layout of display.
 *
 * @returns filled in MMB_Screen
 */
static MMB_Screen *mmb_screen_create( Display *display )
{
    // Create empty structure.
    MMB_Screen *retv = malloc( sizeof( *retv ) );
    memset( retv, 0, sizeof( *retv ) );

    Screen *screen = DefaultScreenOfDisplay( display );
    retv->base.w = WidthOfScreen( screen );
    retv->base.h = HeightOfScreen( screen );

    // Parse xinerama setup.
    if ( XineramaIsActive( display ) ) {
        XineramaScreenInfo *info = XineramaQueryScreens( display, &( retv->num_monitors ) );

        if ( info != NULL ) {
            retv->monitors = malloc( retv->num_monitors*sizeof( MMB_Rectangle ) );

            for ( int i = 0; i < retv->num_monitors; i++ ) {
                retv->monitors[i].x = info[i].x_org;
                retv->monitors[i].y = info[i].y_org;
                retv->monitors[i].w = info[i].width;
                retv->monitors[i].h = info[i].height;
                retv->monitors[i].name = NULL;
            }

            XFree( info );
        }
    }

    if ( retv->monitors == NULL ) {
        retv->num_monitors = 1;
        retv->monitors = malloc( 1*sizeof( MMB_Rectangle ) );
        retv->monitors[0] = retv->base;
    }

    monitor_active( display, retv );

    return retv;
}

/**
 * @param screen a Pointer to the MMB_Screen pointer to free.
 *
 * Free MMB_Screen object and set pointer to NULL.
 */
static void mmb_screen_free( MMB_Screen **screen )
{
    if ( screen == NULL || *screen == NULL ) return;

    if ( ( *screen )->active_monitor.name ) {
        free( ( *screen )->active_monitor.name );
    }

    for ( int i = 0; i < ( *screen )->num_monitors; i++ ) {
        if ( ( *screen )->monitors[i].name ) {
            free( ( *screen )->monitors[i].name );
        }
    }

    if ( ( *screen )->monitors != NULL ) {
        free( ( *screen )->monitors );
    }

    free( *screen );
    screen = NULL;
}

static int mmb_screen_get_active_monitor( const MMB_Screen *screen )
{
    for ( int i =0; i < screen->num_monitors; i++ ) {
        if ( INTERSECT( screen->active_monitor.x, screen->active_monitor.y, 1,1,
                        screen->monitors[i].x,
                        screen->monitors[i].y,
                        screen->monitors[i].w,
                        screen->monitors[i].h ) ) {
            return i;
        }
    }

    return 0;
}

static void mmb_screen_print( const MMB_Screen *screen )
{
    printf( "Total size:    %d %d\n", screen->base.w, screen->base.h );
    printf( "Num. monitors: %d\n", screen->num_monitors );

    for ( int i =0; i < screen->num_monitors; i++ ) {
        printf( "               %01d: %d %d -> %d %d\n",
                i,
                screen->monitors[i].x,
                screen->monitors[i].y,
                screen->monitors[i].w,
                screen->monitors[i].h
              );
    }

    int active_monitor = mmb_screen_get_active_monitor( screen );
    printf( "Active mon:    %d\n", active_monitor );
    printf( "               %d-%d\n", screen->active_monitor.x, screen->active_monitor.y );
}


// X error handler
static int ( *xerror )( Display *, XErrorEvent * );
static int X11_oops( Display *display, XErrorEvent *ee )
{
    if ( ee->error_code == BadWindow
         || ( ee->request_code == X_GrabButton && ee->error_code == BadAccess )
         || ( ee->request_code == X_GrabKey && ee->error_code == BadAccess )
       ) return 0;

    fprintf( stderr, "error: request code=%d, error code=%d\n", ee->request_code, ee->error_code );
    return xerror( display, ee );
}

static void help ()
{
    int code = execlp ( "man", "man", MANPAGE_PATH, NULL );

    if ( code == -1 ) {
        fprintf ( stderr, "Failed to execute man: %s\n", strerror ( errno ) );
    }
}

static void info( const MMB_Screen *screen, Display *display )
{
    Window root = RootWindow( display, 0 );
    XRRScreenResources *rs = XRRGetScreenResources ( display, root );

    if ( rs != NULL ) {
        for ( int i = 0; i < rs->noutput; i++ ) {
            XRROutputInfo *info = XRRGetOutputInfo ( display, rs, rs->outputs[i] );

            if ( info ) {
                if ( info->connection == RR_Connected ) {
                    // Check if there is a crtc assigned to this.
                    if ( info->crtc == None )  continue;

                    XRRCrtcInfo *ci = XRRGetCrtcInfo( display, rs, info->crtc );

                    for ( int m =0; m < screen->num_monitors; m++ ) {
                        if ( INTERSECT( ci->x, ci->y, 1,1,
                                        screen->monitors[m].x,
                                        screen->monitors[m].y,
                                        screen->monitors[m].w,
                                        screen->monitors[m].h ) ) {
                            screen->monitors[m].name = strdup( info->name );
                        }
                    }

                    XRRFreeCrtcInfo( ci );
                }

                XRRFreeOutputInfo( info );
            }
        }

        XRRFreeScreenResources( rs );
    }

}

static void dpms_state ( Display *display )
{
    BOOL state;
    CARD16 pl;
    if(DPMSInfo(display, &pl,  &state)) {
        if(state) {
            switch(pl) {
                case DPMSModeOn:
                    printf("on\n");
                    break;
                case DPMSModeStandby:
                    printf("standby\n");
                    break;
                case DPMSModeSuspend:
                    printf("suspend\n");
                    break;
                case DPMSModeOff:
                    printf("off\n");
                    break;
                default:
                    printf("n/a");
            }
        } else {
            printf("n/a\n");
        }
    }else {
        // No DPMS available.
        printf("n/a\n");
    }
}

static void dpms_print ( Display *display )
{
    BOOL state;
    CARD16 pl;
    if(DPMSInfo(display, &pl,  &state)) {
        printf("dpms: enabled\ndpms state: ");
        if(state) {
            switch(pl) {
                case DPMSModeOn:
                    printf("on\n");
                    break;
                case DPMSModeStandby:
                    printf("standby\n");
                    break;
                case DPMSModeSuspend:
                    printf("suspend\n");
                    break;
                case DPMSModeOff:
                    printf("off\n");
                    break;
                default:
                    printf("n/a");
            }
        } else {
            printf("dpms: disabled\n");
        }
    }else {
        // No DPMS available.
        printf("n/a\n");
    }
}

static int monitor_pos = 0;
static MMB_Screen *mmb_screen = NULL;

/**
 *  Function to handle arguments.
 */
static int handle_arg( Display *display, int argc, char **argv )
{
    if ( argc > 0 && strcmp( argv[0],"-monitor" ) == 0 ) {
        monitor_pos = atoi( argv[1] );

        if ( !( monitor_pos >= 0 && monitor_pos < mmb_screen->num_monitors ) ) {
            fprintf( stderr, "Invalid monitor: %d (0 <= %d < %d failed)\n",
                     monitor_pos,
                     monitor_pos,
                     mmb_screen->num_monitors );
            // Cleanup
            mmb_screen_free( &mmb_screen );
            XCloseDisplay( display );
            exit( EXIT_FAILURE );
        }

        return 1;
    } else if ( strcmp( argv[0], "-active-mon" ) == 0 ) {
        unsigned int mon = mmb_screen_get_active_monitor( mmb_screen );
        printf( "%d\n", mon );
    } else if ( strcmp( argv[0], "-mon-size" ) == 0 ) {
        printf( "%i %i\n", mmb_screen->monitors[monitor_pos].w,mmb_screen->monitors[monitor_pos].h );
    } else if ( strcmp( argv[0], "-mon-width" ) == 0 ) {
        printf( "%i\n", mmb_screen->monitors[monitor_pos].w );
    } else if ( strcmp( argv[0], "-mon-height" ) == 0 ) {
        printf( "%i\n", mmb_screen->monitors[monitor_pos].h );
    } else if ( strcmp( argv[0], "-mon-x" ) == 0 ) {
        printf( "%i\n", mmb_screen->monitors[monitor_pos].x );
    } else if ( strcmp( argv[0], "-mon-y" ) == 0 ) {
        printf( "%i\n", mmb_screen->monitors[monitor_pos].y );
    } else if ( strcmp( argv[0], "-mon-pos" ) == 0 ) {
        printf( "%i %i\n", mmb_screen->monitors[monitor_pos].x,mmb_screen->monitors[monitor_pos].y );
    } else if ( strcmp( argv[0], "-num-mon" ) == 0 ) {
        printf( "%u\n", mmb_screen->num_monitors );
    } else if ( strcmp( argv[0], "-name" ) == 0 ) {
        if ( mmb_screen->monitors[monitor_pos].name ) {
            printf( "%s\n", mmb_screen->monitors[monitor_pos].name );
        } else {
            printf( "unknown\n" );
        }
    } else if ( strcmp( argv[0], "--max-mon-width" ) == 0 ||
                strcmp( argv[0], "-max-mon-width" ) == 0 ) {
        int maxw = 0;

        for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
            maxw = MAX( maxw, mmb_screen->monitors[i].w );
        }

        printf( "%i\n", maxw );
    } else if ( strcmp( argv[0], "--max-mon-height" ) == 0 ||
                strcmp( argv[0], "-max-mon-height" ) == 0 ) {
        int maxh = 0;

        for ( int i = 0; i < mmb_screen->num_monitors; i++ ) {
            maxh = MAX( maxh, mmb_screen->monitors[i].h );
        }

        printf( "%i\n", maxh );
    } else if ( strcmp( argv[0], "-print" ) == 0 ) {
        mmb_screen_print( mmb_screen );
    } else if ( strcmp ( argv[0], "-dpms" ) == 0 ) {
        dpms_print(display);
    } else if ( strcmp ( argv[0], "-dpms-monitor-state" ) == 0 ) {
        dpms_state(display);
    }

    return 0;
}

int main ( int argc, char **argv )
{
    Display *display;

    if ( find_arg( argc, argv, "-h" ) >= 0 || find_arg( argc, argv, "-help" ) >= 0 ) {
        help();
        return 0;
    }


    // Get DISPLAY
    const char *display_str= getenv( "DISPLAY" );

    if ( !( display = XOpenDisplay( display_str ) ) ) {
        fprintf( stderr, "cannot open display!\n" );
        return EXIT_FAILURE;
    }

    // Setup error handler.
    XSync( display, False );
    xerror = XSetErrorHandler( X11_oops );
    XSync( display, False );

    // Get monitor layout. (xinerama aware)
    mmb_screen = mmb_screen_create( display );

    // Update
    info( mmb_screen, display );

    monitor_pos = mmb_screen_get_active_monitor( mmb_screen );

    for ( int ac = 1; ac < argc; ac++ ) {
        //
        ac+=handle_arg( display,argc-ac, &argv[ac] );
    }

    // Cleanup
    mmb_screen_free( &mmb_screen );
    XCloseDisplay( display );
}
