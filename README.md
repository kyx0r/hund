# Hund
A minimalistic terminal file manager.
#### Features
- Vim-inspired hotkeys
- Dual-panel, Vim-like, minimalistic UI
- UTF-8 + wide character support
- Move/remove/copy/rename/chmod/chown selected files
- Recursive chmod with set/unset masks
- Find file in current directory (find as you type)
- Multiple key sorting
- Can be statically linked
- Marks
- Licensed under GPL-3.0
#### Environment Variables (which Hund uses)
| Variable | Usage              | Default |
| -------- | ------------------ | ------- |
| $VISUAL  | text editor        | $EDITOR |
| $EDITOR  | backup for $VISUAL | vi      |
| $PAGER   | pager              | less    |
#### Planned features
- Calculate volume of directories
- Filter files
- atime, mtime, ctime
- ACLs (at least detection)
- Configuration in a file
- Color schemes
- man page
- Packaging for some popular distros + installation scripts
- Columns (size, date, permissions, owner, group, date) (???)
- Integrated terminal (???)
#### Architecture/OS Support
- x86-64
	- Arch Linux - works
	- Alpine Linux - works
	- FreeBSD - works
	- OpenBSD - works
- aarch64 (Raspberry PI 3)
	- Arch Linux ARM 64-bit - works
#### Dependencies & Requirements
- c99-compatible C compiler (`gcc`, `clang`, `tcc` were tested)
	- For `tcc` you need to overwrite CFLAGS. "tcc: error: invalid option -- '--std=c99'"
- libc of choice (`glibc` and `musl` were tested)
- `make`
- A pager (`less` by default)
- A text editor (`vi` by default)
#### Commands
Type `?` while in hund for help.
#### License
Hund is licensed under GPL-3.0.
Type `?` while in hund for copyright notice.
#### Name
_Hund_ is a German word for _dog_.
