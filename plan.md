# Spectogram to visualize music

buffer length of 1024 corresponds to 47 fps at 48000 Hz sampling -- totally acceptable

to reserch:
* SLD2 resizable window
* SDL2 coordinate system rescaling with changing size

# TODO

- [x] Create window
- [x] Make window re-sizable
- [x] make visualization function
- [x] for smooth video, record shorter buffers and "interpolate"
- [ ] Check how I can better set default input source for capture device I've created. 
Not a big deal since I can change it with _pavucontrol_.
