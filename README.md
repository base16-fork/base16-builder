## cbase16

> a blazing fast base16 colorscheme builder

## Usage

cbase16 \[command\]

Command:

- update: fetch all necessary sources for building
- build: generate colorscheme templates
- help: display usage message

## Dependency

- libgit2
- yaml-cpp

## Installation

To compile, run:

``` sh
$ make
```

To install, run:

``` sh
$ make install
```

To uninstall, run:

``` sh
$ make uninstall
```

## Manual Installation

After compiling the program by running `make`, place the executable `cbase16`
in your $PATH. For example, you could also include this directory in $PATH by
adding the following in your profile:

``` sh
PATH=$PATH:path/to/cbase16
```

To get zsh completion working, place the `_cbase16` completion script in your
fpath. For example, you could add the following to your ~/.zshrc to include the
directory that contains the `_cbase16` file in your $fpath:

``` sh
fpath=(path/to/cbase16/completion/zsh $fpath)
```

To get bash completion working, source the `cbase16.bash` completion script.
For example, add the following to your ~/.bashrc:

``` sh
. path/to/cbase16/completion/bash/cbase16.bash
```

Reload your shell and the completion script should be working

To get access to the manpage, either place `cbase16.1` in your $MANPATH or
include this directory in the $MANPATH by adding the following in your profile:

``` sh
MANPATH=$MANPATH:path/to/cbase16
```
