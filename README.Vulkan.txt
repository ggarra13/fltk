README.Wayland.txt - Wayland Platform Support for FLTK
------------------------------------------------------


Contents
========

 1   Introduction

 2   Vulkan Support for FLTK
   2.1    Enabling Vulkan
   2.2    Configuration
   2.3    Known Limitations

 3   Platform Specific Notes
   3.1    Wayland
   3.2    X11
   3.3    macOS
   3.4    Windows


1 Introduction
==============

Version 1.5 of the FLTK library introduces support of drivers for the FLTK API
for Vulkan development. It requires a Vulkan-equipped OS, namely Win64 with new drivers, macOS with MoltenVK, or Linux with Khrono's Vulkan.
The code has currently been tested on Windows, X11 and macOS.

2 Vulkan Support for FLTK
=========================


 2.1 Enabling Wayland
---------------------

Programs using X11 specific functions may need to disable the automatic
detection of Wayland at runtime so they fall back to X11 only.

Note 2: When building a user project with CMake 3.4 or higher, i.e. using

  cmake_minimum_required (VERSION 3.4)

or any higher (minimum) CMake version users need to use at least one of
the following techniques:

-D FLTK_USE_VK=ON
-D Vulkan_INCLUDE_DIR=/usr/local/Vulkan-Headers
-D Vulkan_LIBRARY=/usr/lib/x86_64-linux-gnu/libvulkan.so.1


 2.2 Configuration
------------------

On Linux and FreeBSD systems equipped with the adequate software packages
(see section 3 below), the default building procedure produces a Vulkan/X11
hybrid library. On systems lacking all or part of Vulkan-required packages,
the default building procedure produces a X11-based library.

Use "-D FLTK_BACKEND_WAYLAND=OFF" with CMake to build FLTK for the X11
library when the default would build for Vulkan.

CMake option FLTK_BACKEND_X11=OFF can be used to produce a Vulkan-only
library which can be useful, e.g., when cross-compiling for systems that
lack X11 headers and libraries.

The FLTK Vulkan platform uses a library called libdecor which handles window decorations
(i.e., titlebars, shade). On very recent Linux distributions (e.g., Debian trixie)
libdecor is available as Linux packages (libdecor-0-dev and libdecor-0-plugin-1-gtk).
FLTK requires version 0.2.0 or more recent of these packages.
When libdecor is not available or not recent enough, FLTK uses a copy of libdecor
bundled in the FLTK source code.
FLTK equipped with libdecor supports both the client-side decoration mode (CSD) and the
server-side decoration mode (SSD) as determined by the active Vulkan compositor.
Mutter (gnome's Vulkan compositor) and Weston use CSD mode, KWin and Sway use SSD mode.
Furthermore, setting environment variable LIBDECOR_FORCE_CSD to 1 will make FLTK use CSD
mode even if the compositor would have selected SSD mode.

 2.3 Known Limitations
----------------------

* A deliberate design trait of Vulkan makes application windows ignorant of their exact
placement on screen. It's possible, though, to position a popup window relatively to
another window. This allows FLTK to properly position menu and tooltip windows. But
Fl_Window::position() has no effect on other top-level windows.

* With Vulkan, there is no way to know if a window is currently minimized, nor is there any
way to programmatically unset minimization of a window. Consequently, Fl_Window::show() of
a minimized window has no effect.

* Although the FLTK API to read from and write to the system clipboard is fully functional,
it's currently not possible for an app to be notified of changes to the content of
the system clipboard, that is, Fl::add_clipboard_notify() has no effect.

* Copying data to the clipboard is best done when the app has focus. Any copy operation
performed when the app did not get the focus yet does not change the clipboard. A copy
operation performed when the app has lost the focus is successful only if the type of
the copied data, that is, text or image, is the same as the last data type copied when
the app had the focus.

* Narrow windows with a titlebar are silently forced to be wide enough
for the titlebar to display window buttons and a few letters of the title.

* Text input methods are known to work well for Chinese and Japanese.
Feedback for other writing systems would be helpful.

* Using OpenGL inside Vulkan windows doesn't seem to work on RaspberryPi hardware,
although it works inside X11 windows on the same hardware.

* Drag-and-drop initiation from a subwindow doesn't work under the KDE/Plasma desktop.
That is most probably a KWin bug because no such problem occurs with 3 other
Vulkan compositors (Mutter, Weston, Sway). A workaround is proposed in issue #997
of the FLTK github repository (https://github.com/fltk/fltk/issues/997).

3 Platform Specific Notes
=========================

The following are notes about building FLTK for the Vulkan platform
on the various supported Linux distributions/OS.

3.1 Debian and Derivatives (like Ubuntu, Mint, RaspberryPiOS)
-------------------------------------------------------------

Under Debian, the Vulkan platform requires version 11 (a.k.a. Bullseye) or more recent.
Under Ubuntu, the Vulkan platform requires version 20.04 (focal fossa) or more recent.


3.2 Fedora
----------

The Vulkan platform is known to work with Fedora version 35 or more recent.

These packages are necessary to build the FLTK library, in addition to
package groups listed in section 2.2 of file README.Unix.txt :

- wayland-devel
- wayland-protocols-devel
- cairo-devel
- libxkbcommon-devel
- pango-devel
- mesa-libGLU-devel
- dbus-devel     <== recommended to query current cursor theme
- libdecor-devel <== recommended, draws window titlebars
- gtk3-devel     <== highly recommended if libdecor-devel is not installed
- glew-devel     <== necessary to use OpenGL version 3 or above
- cmake          <== if you plan to build with CMake
- cmake-gui      <== if you plan to use the GUI of CMake

Package installation command: sudo yum install <package-name ...>


3.3 FreeBSD
-----------

The Vulkan platform is known to work with FreeBSD version 13.1 and the Sway compositor.

These packages are necessary to build the FLTK library and use the Sway compositor:

git autoconf pkgconf xorg urwfonts gnome glew seatd sway dmenu-wayland dmenu evdev-proto

Package installation command: sudo pkg install <package-name ...>
