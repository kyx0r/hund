# Hund
A terminal file manager.
#### Current features
- Vim-inspired command system
- UTF-8 support
- Symlink aware
- Move/remove/copy/rename file/directory
- Hide/show hidden files
- Change file permissions/owner/group
- Create directory
- Find file in current directory
#### Planned features
- Open in external program, based on extension
- Rename multiple files at once (somewhat like ranger)
- Select files and perform operations on them
- Task queue of some sort
- Fancy file sorting
- Configuration in a file
#### UI
Hund uses dual-panel view just like GNU Midnight Commander does. And it uses ncurses too.
#### Commands
- `FILE VIEW`
	- `qq` exit
	- `?` display help screen
	- `ESC` reset command/get rid of error message
	- `^N` or `j` entry down
	- `^P` or `k` entry up
	- `u` or `d` up directory
	- `i` or `e` enter directory
	- `o` open in external program (right now, everything is opened with less)
	- `gg` go to the first file in directory
	- `G` go to the last file in directory
	- `TAB` change active view
	- `cp` copy highlighted file from active view to the other view (opens `PROMPT`)
	- `mv` move highlighted file from active view to the other view
	- `rm` remove highlighted file
	- `rr` rescan active view
	- `rn` rename highlighted file (opens `PROMPT`)
	- `mk` create directory (opens `PROMPT`)
	- `cd` open directory
	- `/` find file in working directory (opens `PROMPT`)
		- aborting search returns to entry highlighted before searching
		- applying search leaves selection at found entry
	- `ch` change file permissions (opens `CHMOD`)
- `CHMOD`
	- `qq` abort changes and quit `CHMOD`
	- `ch` apply changes and quit `CHMOD`
	- `co` change owner (opens `PROMPT`)
	- `cg` change group (opens `PROMPT`)
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
- `PROMPT`
	- `^J` or `ENTER` accept input and quit `PROMPT`
	- `^[` or `ESC` abort input and quit `PROMPT`
	- `^H` or `BACKSPACE` backspace (delete before cursor)
	- `^D` or `DELETE` delete after cursor
	- `^A` or `HOME` place cursor at the begining of input
	- `^E` or `END` place cursor at the end of input
	- `^F` or `ARROW RIGHT` go right
	- `^B` or `ARROW LEFT` go left
	- `^U` clear whole input
	- `^K` clear input after cursor
#### Name
_Hund_ is a german word for _dog_.
