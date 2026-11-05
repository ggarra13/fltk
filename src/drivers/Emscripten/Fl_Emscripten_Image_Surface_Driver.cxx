//
// Draw-to-image code for the Fast Light Tool Kit (FLTK).
//
// Copyright 2024 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//


#include "Fl_Emscripten_Image_Surface_Driver.H"
#include "Fl_Emscripten_Graphics_Driver.H"
#include <FL/Fl_Graphics_Driver.H>
#include <emscripten.h>
#include <emscripten/val.h>
#include <vector>

using namespace emscripten;

Fl_Emscripten_Image_Surface_Driver::Fl_Emscripten_Image_Surface_Driver(int w, int h, int highres,
                                                                       Fl_Offscreen off)
  : Fl_Image_Surface_Driver(w, h, highres, off) {
  // Modern browsers support offscreen canvas, however Emscripten needs to be told to enable such
  // support. This is done by compiling the main application with `-s OFFSCREENCANVAS_SUPPORT=1`
  offscreen = off ? off
                  : (Fl_Offscreen)EM_ASM_PTR(
                        {
                          let canvas = new OffscreenCanvas($0, $1);
                          let ctx = canvas.getContext("2d");
                          ctx.lineWidth = 0;
                          return Emval.toHandle(ctx);
                        },
                        w, h);
  Fl_Display_Device::display_device();
  driver(Fl_Graphics_Driver::newMainGraphicsDriver());
}
void Fl_Emscripten_Image_Surface_Driver::set_current() {
  ((Fl_Emscripten_Graphics_Driver *)fl_graphics_driver)->context((EM_VAL)offscreen);
  pre_window = fl_window;
  fl_window = 0;
}
void Fl_Emscripten_Image_Surface_Driver::translate(int x, int y) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.translate($1, $2);
      },
      (EM_VAL)offscreen, x, y);
}
void Fl_Emscripten_Image_Surface_Driver::untranslate() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
      (EM_VAL)offscreen);
}

Fl_RGB_Image *Fl_Emscripten_Image_Surface_Driver::image() {
  val ctx = val::take_ownership((EM_VAL)offscreen);
  val imagedata = ctx.call<val>("getImageData", 0, 0, width, height)["data"];
  std::vector<uchar> vec = vecFromJSArray<uchar>(imagedata);
  Fl_RGB_Image *image = new Fl_RGB_Image(vec.data(), width, height, 4);
  image->alloc_array = 1;
  return image;
}

void Fl_Emscripten_Image_Surface_Driver::end_current() {
  fl_window = (Window)pre_window;
  Fl_Surface_Device::end_current();
}