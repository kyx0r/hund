# Hund
A terminal file manager.
#### Goals
I want to learn new things. If you find Hund useful, it's a huge plus.
#### Current features
- Vim-inspired command system
- Resize window
- File type recognition (indicated by color and symbol)
- Follow symlinks
- Move/remove/copy file/directory
- Create directory
- Find file
#### UI
Hund uses dual-panel view just like GNU Midnight Commander does. And it uses ncurses too.
#### Be careful
Hund is very young at the moment and it can mess your files up if you trust it too much.
#### Commands
- `qq` exit
- `j` entry down
- `k` entry up
- `u` or `d` up directory
- `i` or `e` enter directory
- `gg` go to the first file in directory
- `G` go to the last file in directory
- `TAB` change active view
- `cp` copy highlighted file from active view to the other view
- `mv` move highlighted file from active view to the other view
- `rm` remove highlighted file
- `rr` rescan active view
- `mk` create directory
- `/` find file in working directory
	- `(any characters)` enter name of searched file
	- `ESC` abort search and return to entry highlighted before searching
	- `ENTER` exit search and stay at found entry
- `ESC` reset command
- `ch` change file permissions
	- `qq` abort changes and quit chmod
	- `ch` apply changes and quit chmod
	- `ui` toggle set user id on execution
	- `gi` toggle set group id on execution
	- `os` toggle sticky bit
	- `ur` toggle user read
	- `uw` toggle user write
	- `ux` toggle user execute/search
	- `gr` toggle group read
	- `gw` toggle group write
	- `gx` toggle group execute
	- `or` toggle other read
	- `ow` toggle other write
	- `ox` toggle other execute
#### Name
_Hund_ is a german word for _dog_.
