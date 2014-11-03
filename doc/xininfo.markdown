# XININFO 1 xininfo

## NAME

xininfo - a tool to query the layout and size of each configured monitor.

## SYNOPSIS

**xininfo** [-monitor *id*] [-active-mon] [-mon-size] [ -mon-width ] [ -mon-height ] [ -mon-x ] 
[ -mon-y ] [ -mon-pos ] [ -num-mon ] [ -max-mon-width ] [ -max-mon-height ] [ -print ] [-help]



## DESCRIPTION

**xininfo** is an X11 utility to query the current layout and size of each configured monitor. It is
designed to be used by scripts.

## License

MIT/X11

## USAGE

**xininfo** accepts the following commands. Multiple commands can be concatenated.
All information returned is in number of pixels.

`monitor` *id*
    
    Set queried monitor to *id*. By default it uses the active monitor.

`active-mon` 

    Query the *id* of the active monitor.

    **Prints**: *id* 
    
`mon-size`

    Get the size of the monitor.

    **Prints**: *width* *height* 

`mon-width` 

    Get the width of the monitor:

    **Prints**: *width*

`mon-height` 

    Get the height of the monitor:

    **Prints**: *height*

`mon-x` 

    Get the x position of the monitor:

    **Prints**: *x*

`mon-y` 

    Get the y position of the monitor:

    **Prints**: *y*

`mon-pos` 

    Get the position of the monitor:

    **Prints**: *x* *y*

`num-mon` 

    Queries the number of configured monitors.

    **Prints**: *#monitors*

`max-mon-width` 

    Query width of the monitor with the most horizontal pixels

    **Prints**: *width*

`max-mon-height` 

    Query height of the monitor with the most vertical pixels

    **Prints**: *width*

`print` 

    Print out all information in a human readable format.

`-h`
`-help`

    Show the manpage of **xininfo**


## AUTHOR

Qball Cow <qball@gmpclient.org>
