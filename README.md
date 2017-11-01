# Hund
A terminal file manager.
#### Current features
- Vim-inspired command system
- Dynamic window size
- File type recognition (indicated by color and symbol)
- Follow symlinks
- Move/remove/copy/rename file/directory
- Hide/show hidden files
- Change file permissions/owner/group
- Create directory
- Find file in current directory
#### Planned features
- Rich error/warning/info log
- Open in external program, based on extension
- Rename multiple files at once (somewhat like ranger)
- UTF-8 support
- Task queue of some sort
- Fancy file sorting
- Configuration in a file
#### UI
Hund uses dual-panel view just like GNU Midnight Commander does. And it uses ncurses too.
#### Commands
- `qq` exit
- `ESC` reset command/get rid of error message
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
- `rn` rename highlighted file
- `mk` create directory
- `cd` open directory
- `/` find file in working directory
	- `(any characters)` enter name of searched file
	- `ESC` abort search and return to entry highlighted before searching
	- `ENTER` exit search and stay at found entry
- `ch` change file permissions
	- `qq` abort changes and quit chmod
	- `ch` apply changes and quit chmod
	- `co` change owner (prompts for owner login)
	- `cg` change group (prompts for group name)
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
