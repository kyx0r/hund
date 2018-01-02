# Hund
A minimalistic terminal file manager.
#### Features
- Vim-inspired hotkeys
- MC-inspired, minimalistic UI
- UTF-8 support
- Move/remove/copy selected files
- Rename multiple files/directories at once
- Create multiple directories at once
- Calculate volume of a directory
- Hide/show hidden files
- Change permissions/owner/group of a single file
- Find file in current directory (find as you type)
- Sort by name/date/size ascending/descending
- Pause/resume/abort copy/move/remove operation
- Visible progress of copy/move/remove operation
- Licensed under GPL-3.0
#### Environment Variables
| Variable | Usage              | Default |
| -------- | ------------------ | ------- |
| $VISUAL  | text editor        | $EDITOR |
| $EDITOR  | backup for $VISUAL | vi      |
| $PAGER   | pager              | less    |
#### Planned features
- Chmod mask
- Fancy file sorting
- Filter files
- Configuration in a file
- Color schemes
- ACLs (at least detection)
- Integrated terminal
- Bookmarks
- Extract archive
#### Architecture/OS Support
- x86-64 + Arch Linux - Developed on, can be expected to work *best*
- x86-64 + FreeBSD - gcc's only problem is that it does not detect ctrl keys, clang does not detect any keys at all lol
- aarch64 (Raspberry PI 3) + Arch Linux ARM 64-bit - works just fine
#### Dependencies & Requirements
- ncurses
- gcc (preferably) or clang
- GNUMake
- glibc
#### Commands
Type `?` while in hund for help.
#### License
Hund is licensed under GPL-3.0.
Type `?` while in hund for copyright notice.
#### Name
_Hund_ is a German word for _dog_.
