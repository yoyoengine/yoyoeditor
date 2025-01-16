
# TODO

## Stuff needed for windows port

- sys/wait stuff for the editor build thread
- likely dirent for the tricks panel, but you already ported a windows header in yoyoengine
- zenity file picker
- libcurl welcome panel

editor cleanup project:
nk_bool returns bool if changed, used to set unsaved etc
macros to track unsaved for field props
split up the stinky code into many files
refactor
