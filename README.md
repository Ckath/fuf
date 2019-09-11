# fuf
Fairly Usable Filebrowser
[![videodemo](https://xn--z7x.xn--6frz82g/files/fuf_demo.png)](https://streamable.com/iur7v)

another cli filebrowser made just for myself with the following ideas:
- async loading of directories and previews that you can cancel
- more vim movement keybinds
- handling of opening and previewing files done by external scripts for easy configuration
- more respect for `LS_COLORS`
- no fancy file edit operations, at most drop to shell
- no fancy in depth configuration besides the scripts
- minimize hanging due to unneeded disk io

## keybinds
### vim - work mostly as expected
`j`,`k`,`l`,`h`,`H`,`M`,`L`,`G`,`gg`,`/`,`n`,`N`,`q`,`^u`,`^d`,`^f`,`^b`,`^y`,`^e`,`zz`,`zb`,`zt`
### non vim
`s`: sort (+`a`/`A` alphabetical, +`s`/`S` size based, +`t`/`T` time based)\
`r`: reload\
`R`: force redraw of ui

## development
current bugs, features and enhancements in the works are kept track of in [issues](https://github.com/Ckath/fuf/issues), refer to this to see what's happening at the moment. PRs are always welcome, but make an issue first to discuss any new features or bugs.

## setup
### install
#### arch
install either [fuf](https://aur.archlinux.org/packages/fuf) or [fuf-git](https://aur.archlinux.org/packages/fuf-git)
#### manual
`sudo make install`\
\* ncurses, pthreads and a posix system is required to compile fuf
### uninstall
`sudo make uninstall`
### configuration
`mkdir ~/.config/fuf && cp /usr/lib/fuf/* ~/.config/fuf`\
grab the default scripts and move them to local copies, you can also choose symlink xdg-open or other open managers to ~/.config/fuf/open at this point. the fuf preview and open executables/scripts fully handle opening and previewing files, this means you can customize the way this is handled completely within these scripts.

## non issues/wontfix
- preview showing `no preview configured`\
you are lacking a program used to generate previews, change it in the script or install them
- image previews not working\
w3m is used to display the (generated) images, make sure your terminal supports this
- some file is not opening with fuf\
either the open script isn't setup to open this filetype or you dont have any of the programs associated with the filetype installed. install the programs or edit/replace the open handler
- the directory sizes are wrong\
this is done on purpose, sizes are displayed as they are given by stat, calculating size of directory contents is too costly
- no colors anywhere\
set your `LS_COLORS`, ls defaults back to an unfindable default, fuf only reads the `LS_COLORS` variable
- no mouse support\
this will never be added, it annoys me
- problems with programs using terminal as ui\
these are whitelisted in fuf in the `CLI_PROGRAMS` definition in fuf.c, make a pr or issue for one to be added
- issues related to scripts remain after update that should fix them\
since fuf allows you use your own versions of the scripts in ~/.config/fuf its up to yourself to keep up with patching these with the latest changes from the /usr/lib/fuf supplied ones.

## inspirations
- [ranger](https://github.com/ranger/ranger): the cli fm that annoyed me to the point of writing my own
- [cfiles](https://github.com/mananapr/cfiles): C cli fm that made me appreciate the idea of async loading

## excuses
you might notice the syntax and style is slightly 'unique' in places of this source. this has several reasons:
- most if not all of this was created late at night under influence
- it's primarily a small personal project with realistically myself maintaining 99%
- I do this shit for fun
