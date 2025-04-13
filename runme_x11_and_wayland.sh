#!/usr/bin/env bash

mkdir -p build_x11_and_wayland
cd build_x11_and_wayland

if [[ -z "$VULKAN_SDK" ]]; then
    export VULKAN_SDK=/usr/
fi

cmake .. \
      -G Ninja \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_EXAMPLES=OFF \
      -D FLTK_BUILD_FLUID=OFF \
      -D FLTK_BUILD_FORMS=OFF \
      -D FLTK_BUILD_GL=OFF \
      -D FLTK_BUILD_VK=ON \
      -D FLTK_BUILD_PDF_DOCS=OFF \
      -D FLTK_BUILD_HTML_DOCS=OFF \
      -D FLTK_BUILD_OPTIONS=OFF \
      -D FLTK_BUILD_SHARED_LIBS=ON \
      -D FLTK_BUILD_TEST=ON \
      -D FLTK_GRAPHICS_CAIRO=OFF \
      -D FLTK_OPTION_CAIRO_EXT=OFF \
      -D FLTK_OPTION_CAIRO_WINDOW=OFF \
      -D FLTK_OPTION_PRINT_SUPPORT=OFF \
      -D FLTK_OPTION_STD=ON \
      -D FLTK_OPTION_SVG=OFF \
      -D FLTK_USE_PANGO=OFF \
      -D FLTK_USE_PTHREADS=OFF \
      -D FLTK_USE_SYSTEM_LIBJPEG=OFF \
      -D FLTK_USE_SYSTEM_LIBPNG=OFF \
      -D FLTK_USE_SYSTEM_ZLIB=OFF \
      -D FLTK_USE_SYSTEM_LIBDECOR=OFF \
      -D FLTK_USE_XCURSOR=OFF \
      -D FLTK_USE_XFIXES=OFF \
      -D FLTK_USE_XFT=OFF \
      -D FLTK_USE_XINERAMA=OFF \
      -D FLTK_USE_XRENDER=OFF \
      -D FLTK_USE_LIBDECOR_GTK=OFF \
      -D FLTK_BACKEND_WAYLAND=ON \
      -D FLTK_BACKEND_X11=ON \
      -D GLU_LIB="" \
      -D LIB_GL="" \
      -D LIB_MesaGL="" \
      -D OPENGL_INCLUDE_DIR="" \
      -D X11_xcb_xcb_INCLUDE_PATH=""

ninja && bin/test/vk_shape-shared && bin/test/vk_shape_textured-shared && bin/test/vk_cube-shared
