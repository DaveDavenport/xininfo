# Installation guide

## DEPENDENCY

### For building:

* C compiler that supports the c99 standard. (gcc or clang)
* make
* autoconf
* automake (1.11.3 or up)
* pkg-config
* Developer packages of the external libraries

### External libraries

* libglib2.0 >= 2.40
* libxcb (sometimes split, you need libxcb, libxcb-xkb and libxcb-randr libxcb-xinerama)
* xcb-util
* xcb-util-wm (sometimes split as libxcb-ewmh and libxcb-icccm libxcb-dpms)
* xcb-util-xrm [new module, can be found here](https://github.com/Airblader/xcb-util-xrm/)

On debian based systems, the developer packages are in the form of: `<package>-dev` on rpm based
`<package>-devel`.

## Install from a release

Check dependencies and configure build system:

```
./configure
```

Build xininfo:

```
make
```

The actual install, execute as root (if needed):

```
make install
```


## Install a checkout from git

The GitHub Pages version of these directions may be out of date.  Please use
[INSTALL.md from the online repo][master-install] or your local repository.

[master-install]: https://github.com/DaveDavenport/xininfo/blob/master/INSTALL.md#install-a-checkout-from-git

Generate build system:

```
autoreconf -i
```

Create a build directory:

```
mkdir build
```

Enter build directory:

```
cd build
```

Check dependencies and configure build system:

```
../configure
```

Build xininfo:

```
make
```

The actual install, execute as root (if needed):

```
make install
```


## Options for configure

When you run the configure step there are several you can configure. (To see the full list type
`./configure --help` ).

The most useful one to set the installation prefix:

```
./configure --prefix=<installation path>
```

f.e.

```
./configure --prefix=/usr/
```

### Install locally

or to install locally:

```
./configure --prefix=${HOME}/.local/
```


## Options for make

When you run make you can tweak the build process a little.

### Verbose output

Show the commands called:

```
make V=1
```

### Debug build

Compile with debug symbols and no optimization

```
make CFLAGS="-O0 -g3" clean xininfo 
```
