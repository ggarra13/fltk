//
// Copy-to-clipboard code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2024 by Bill Spitzak and others.
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

#include "Fl_Emscripten_Copy_Surface_Driver.H"
#include <FL/Fl_Image_Surface.H>
#include "Fl_Emscripten_Graphics_Driver.H"
#include <emscripten.h>
#include <emscripten/val.h>

Fl_Emscripten_Copy_Surface_Driver::Fl_Emscripten_Copy_Surface_Driver(int w, int h)
  : Fl_Copy_Surface_Driver(w, h) {
  driver(new Fl_Emscripten_Graphics_Driver());
  img_surf = new Fl_Image_Surface(w, h);
  driver(img_surf->driver());
}

Fl_Emscripten_Copy_Surface_Driver::~Fl_Emscripten_Copy_Surface_Driver() {
  // Storing the canvas to an image/png makes it easier to retrieve.
  // The png is drawn again to the canvas and is restored as RGB image.
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.canvas.toBlob(
            function(blob) {
              navigator.clipboard.write([new ClipboardItem({"image/png" : blob})]);
            },
            "image/png");
      },
      (emscripten::EM_VAL)img_surf->offscreen());
  delete img_surf;
  driver(NULL);
}


void Fl_Emscripten_Copy_Surface_Driver::set_current() {
  Fl_Surface_Device::set_current();
  ((Fl_Emscripten_Graphics_Driver *)driver())->context((emscripten::EM_VAL)img_surf->offscreen());
}

void Fl_Emscripten_Copy_Surface_Driver::translate(int x, int y) {
  ((Fl_Emscripten_Graphics_Driver *)driver())->ps_translate(x, y);
}


void Fl_Emscripten_Copy_Surface_Driver::untranslate() {
  ((Fl_Emscripten_Graphics_Driver *)driver())->ps_untranslate();
}