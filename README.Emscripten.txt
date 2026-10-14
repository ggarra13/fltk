README.Emscripten.txt - Emscripten Platform Support for FLTK
------------------------------------------------------


Contents
========

 1   Introduction

 2   Emscripten Support for FLTK
   2.1    Configuration
   2.2    Known Limitations


1 Introduction
==============

This is an experimental support for Emscripten. Emscripten is a toolchain which targets WebAssembly. It utilizes LLVM to generate wasm binaries which can run on the browser.
The official Emscripten website explains how you can download and install the SDK (EMSDK).


2 Emscripten Support for FLTK
=============================
Building your FLTK application with Emscripten should allow your application to run on any web browser which supports wasm.
The drawing is done using canvas 2d context calls. Windows are enclosed in a DIV element which allows them to be moved in the screen. If this is not desirable, you can disable your windows borders.

 2.1 Configuration
------------------

Emscripten provides a CMake wrapper called emcmake, and alternatively a CMake toolchain file. Either can be used to configure FLTK.

Using emcmake:
emcmake cmake -Bbin -GNinja -DCMAKE_BUILD_TYPE=Release -DFLTK_BUILD_TEST=OFF -DFLTK_USE_PTHREADS=OFF -DFLTK_BUILD_FLUID=OFF -DFLTK_BUILD_FLTK_OPTIONS=OFF -DFLTK_BACKEND_WAYLAND=OFF -DFLTK_BACKEND_X11=OFF

Using the toolchain file:
cmake -Bbin -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DFLTK_BUILD_TEST=OFF -DFLTK_USE_PTHREADS=OFF -DFLTK_BUILD_FLUID=OFF -DFLTK_BUILD_FLTK_OPTIONS=OFF

Then the project can be built using `cmake --build bin` and installed using `cmake --install bin`. Installation will install FLTK in ${EMSDK}/upstream/emscripten/cache/sysroot/.

Notice we disable building any executables which are bundled with the FLTK repo, that's because generating a wasm executable binary requires certain additional properties.
These properties are set in your CMakeLists.txt file. For example, specifying that Emscripten should generate an html file or that it should support offscreen canvases.

An example of setting these properties for your project:
```
if(EMSCRIPTEN)
    add_executable(index main.cpp)
    target_link_libraries(index PRIVATE fltk::fltk embind)
    set_target_properties(index PROPERTIES SUFFIX .html LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sOFFSCREENCANVAS_SUPPORT=1 -sSINGLE_FILE --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html")
else()
    add_executable(MyApplication main.cpp)
    target_link_libraries(MyApplication PRIVATE fltk::fltk)
endif()
```

The SINGLE_FILE shell argument allows building your app into a single html file. This is useful during development, since it doesn't require running a server.

Notice how our binary name is set to index, this is optional however it allows running the binary by simply accessing the root '/' of our site.
We also tell the Emscripten toolchain to use a shell html file which we call shell_minimal.html and place in our CMakeLists.txt directory.
The content of this shell file has to be valid HTML5 and contain a placeholder for the generated script. A minimal example:
```
<!doctype html>
<html lang="en-us">
  <head>
    <meta charset="utf-8">
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  </head>
  <body>
    {{{ SCRIPT }}}
  </body>
</html>
```
If you don't provide one, Emscripten will still generate an html file which contains its logo and other elements which you might not want/need.

 2.2 Known Limitations
----------------------

1- Asyncify reentrancy: FLTK uses reentrant calls to Fl::wait(), a feature not supported by emscripten for the time being:
https://emscripten.org/docs/porting/asyncify.html#reentrancy

That means that dialogs and menu windows (those spawned by Fl_Menu_Bar, Fl_Choice etc) might work but are prone to failure.
You would have to work around this limitation.

2- Multithreading: Notice how we configured FLTK using `-DFLTK_USE_PTHREADS=OFF`. 
Threads are supported by Emscripten via web workers. However the data representation requires a SharedArrayBuffer, support for which was disabled in major browsers in 2019 following the Spectre attack.

3- File system access: Emscripten supports a virtual file system in which local files can be embedded and deployed with your application, this virtual filesystem can be accessed with Fl_File_Chooser. Also files loaded into the application using `--preload file` can be accessed directly using their path and passed to fopen or to FLTK functions expecting a path. 
You can also get access to the local filesystem using the browser's file chooser via Fl_Native_File_Chooser with certain limiation since Web applications are sandboxed from the client's machines. Currently this works only on chrome and bing since they support the local file access api.
You can use fl_read_to_string and fl_read_to_binary utility functions to read a file in the browser from the client via the Fl_Native_File_Chooser. The fl_read_to_binary returns a uchar pointer and can be passed to FLTK functions expecting data such as image constructors. 

To enable the virtual file system, you can use the --preload-file directive in your CMake script:
```
set_target_properties(index PROPERTIES SUFFIX .html LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sOFFSCREENCANVAS_SUPPORT=1 --bind --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html" --preload-file ../images@.)
```
More information can be found in the emscripten documentation.

4- Virtual keyboard: Support on iOS is non-existent for non input HTMLElement. Since FLTK's input and text widgets are drawn on canvas, these wouldn't show a virtual keyboard on iOS.

5- OpenGL support: Emscripten translates OpenGL calls to WebGL, however these are limited to a subset supported also by GLES. Even when enabling Emscripten's LEGACY_GL_EMULATION, some calls aren't supported like glPushAttrib, glPopAttrib, glCopyPixels and some others.
It's also not possible to embed an Fl_GL_Window inside another window since it's not possible to embed a canvas element inside another.
Potentially Fl_GL_Window can be supported by removing legacy functions and by being the main window (i.e not embedded).