## cbase16

> a blazing fast base16 colorscheme builder

## Usage

cbase16 \[command\] \[options\]

Command:

- update: fetch all necessary sources for building
- build: generate colorscheme templates
- help: display usage message

Options:

- -s: only build specified schemes
- -t: only build specified templates
- -o: set output directory

## File Structure

After running `update`, the file structure for the directory under which the
builder is ran will be structured as follows:

- `/sources.yaml` -- Holds a list of source repositories for schemes and templates
- `/sources/schemes/list.yaml` -- Holds a list of scheme repositories
- `/sources/templates/list.yaml` -- Holds a list of template repositories
- `/schemes/[name]/*.yaml` -- Scheme files
- `/templates/[name]/templates/*.mustache` -- Template files
- `/templates/[name]/templates/config.yaml` -- Template configuration file

After running `build`, the generated colorscheme templates will be in the
`output` directory by default unless specified otherwise under the current
running directory.

## Dependencies

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
in your `$PATH`. For example, you could also include this directory in `$PATH` by
adding the following in your profile:

``` sh
PATH=$PATH:path/to/cbase16
```

To get zsh completion working, place the `_cbase16` completion script in your
fpath. For example, you could add the following to your ~/.zshrc to include the
directory that contains the `_cbase16` file in your `$fpath`:

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
