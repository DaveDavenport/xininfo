# XININFO 1 xininfo

## NAME

xininfo - a tool to query the layout and size of each configured monitor.

## SYNOPSIS

**xininfo** 
[ -monitor *id*] 
[ -active-mon] 
[ -mon-size] 
[ -mon-width ] 
[ -max-mon-width ] 
[ -mon-height ] 
[ -max-mon-height ] 
[ -mon-x ] 
[ -mon-y ] 
[ -mon-pos ] 
[ -num-mon ] 
[ -dpms ]
[ -dpms-state ]
[ -screensaver ]
[ -screensaver-state ]
[ -print ] 
[ -name ]
[ -modes ]
[ -h ]



## DESCRIPTION

**xininfo** is an X11 utility to query the current layout and size of each configured monitor. It is
designed to be used by scripts.

## License

MIT/X11

## USAGE

**xininfo** accepts the following commands. Multiple commands can be concatenated.
All information returned is in number of pixels.

`-monitor` *id*
    
Set queried monitor to *id*. By default it uses the active monitor.

`-active-mon` 

Query the *id* of the active monitor.

**Prints**: *id* 
    
`-mon-size`

Get the size of the monitor.

**Prints**: *width* *height* 

`-mon-width` 

Get the width of the monitor:

**Prints**: *width*

`-max-mon-width` 

Query width of the monitor with the most horizontal pixels

**Prints**: *width*

`-mon-height` 

Get the height of the monitor:

**Prints**: *height*

`-max-mon-height` 

Query height of the monitor with the most vertical pixels

**Prints**: *width*

`-mon-x` 

Get the x position of the monitor:

**Prints**: *x*

`-mon-y` 

Get the y position of the monitor:

**Prints**: *y*

`-mon-pos` 

Get the position of the monitor:

**Prints**: *x* *y*

`-num-mon` 

Queries the number of configured monitors.

**Prints**: *#monitors*

`-dpms`

Prints the current dpms state human readable.

`-dpms-state`

Prints the current dpms state easy parsable.

`-screensaver`

Prints the current screensaver state human readable.

`-screensaver-state`

Prints the current screensaver state easy parsable.


`-print` 

Print out all information in a human readable format.

`-name` 
    
Print the xrandr name of the monitor.

`-modes`

Print the supported modes (resolution and refresh rate) for the selected monitor.

`-h`

Show the manpage of **xininfo**


## AUTHOR

Qball Cow <qball@gmpclient.org>
