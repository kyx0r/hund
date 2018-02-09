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
- Not _too_ big; ~4K SLOC
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
- ACLs (at least detection)
- Configuration in a file
- Color schemes
- Extract archive
- Bookmarks
- Integrated terminal
#### Architecture/OS Support
- x86-64 + Arch Linux - Developed on. Can be expected to work *best*
	- Alpine Linux works too
- x86-64 + FreeBSD - works!
- aarch64 (Raspberry PI 3) + Arch Linux ARM 64-bit - works just fine
#### Dependencies & Requirements
- c99-compatible C compiler (gcc and clang were tested)
- libc of choice (glibc and musl were tested)
- make
#### Commands
Type `?` while in hund for help.
#### License
Hund is licensed under GPL-3.0.
Type `?` while in hund for copyright notice.
#### Name
_Hund_ is a German word for _dog_.
