# Kivy with Gstreamer OpenGL test

This is a test of using Gstreamer's OpenGL support with the Kivy framework.

Currently only supports X11 on Linux, but could potentially support every platform supported by Kivy.

### Requirements

* Python 3
* My [Kivy fork](https://github.com/hunternet93/kivy)
* SDL2 (with headers)
* Gstreamer 1.6 or later (with headers)
* gst-plugins-bad (with headers)
* Python GObject Introspection module

### Usage

First, build `glstuff.so`:

    gcc -c -Wall -Werror -fpic `pkg-config --cflags gstreamer-gl-1.0` glstuff.c
    gcc -shared -o glstuff.so -lSDL2 `pkg-config --libs gstreamer-gl-1.0` glstuff.o

Next, run `kivy-gstgl-test.py`:

    python3 kivy-gstgl-test.py

Kivy will open and display a test pattern.

## Bugs

Lots! But two noticible ones:

* A `TypeError` is printed to console every frame. This is because I don't understand C or ctypes.
* The program segfaults on exit. Something's up with the GL-context cleanup code, I don't understand it well enough to debug it.
