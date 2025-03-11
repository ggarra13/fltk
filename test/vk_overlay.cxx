//
// OpenGL overlay test program for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
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

#include <config.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/math.h>

#if !HAVE_VK
#include <FL/Fl_Box.H>
class shape_window : public Fl_Box {
public:
  int sides;
  shape_window(int x,int y,int w,int h,const char *l=0)
    :Fl_Box(FL_DOWN_BOX,x,y,w,h,l){
      label("This demo does\nnot work without GL");
  }
};
#else
#include <FL/gl.h>
#include <FL/Fl_Vk_Window.H>

class shape_window : public Fl_Vk_Window {
  void draw() FL_OVERRIDE;
//  void draw_overlay() FL_OVERRIDE;
public:
  int sides;
  int overlay_sides;
  shape_window(int x,int y,int w,int h,const char *l=0);
};

shape_window::shape_window(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
  sides = overlay_sides = 3;
}

void shape_window::draw() {
// the valid() property may be used to avoid reinitializing your
// GL transformation for each redraw:
  if (!valid()) {
    valid(1);
  }
}

#endif

// when you change the data, as in this callback, you must call redraw():
void sides_cb(Fl_Widget *o, void *p) {
  shape_window *sw = (shape_window *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->redraw();
}

#if HAVE_VK
void overlay_sides_cb(Fl_Widget *o, void *p) {
  shape_window *sw = (shape_window *)p;
  sw->overlay_sides = int(((Fl_Slider *)o)->value());
  //sw->redraw_overlay();
}
#endif
#include <stdio.h>
int main(int argc, char **argv) {

  Fl::use_high_res_GL(1);
  Fl_Window window(300, 370);

  shape_window sw(10, 75, window.w()-20, window.h()-90);
//sw.mode(FL_RGB);
  window.resizable(&sw);

  Fl_Hor_Slider slider(60, 5, window.w()-70, 30, "Sides:");
  slider.align(FL_ALIGN_LEFT);
  slider.callback(sides_cb,&sw);
  slider.value(sw.sides);
  slider.step(1);
  slider.bounds(3,40);

  Fl_Hor_Slider oslider(60, 40, window.w()-70, 30, "Overlay:");
  oslider.align(FL_ALIGN_LEFT);
#if HAVE_GL
  oslider.callback(overlay_sides_cb,&sw);
  oslider.value(sw.overlay_sides);
#endif
  oslider.step(1);
  oslider.bounds(3,40);

  window.end();
  window.show(argc,argv);
#if HAVE_VK
  printf("Can do overlay = %d\n", sw.can_do_overlay());
  sw.show();
  //sw.redraw_overlay();
#else
  sw.show();
#endif

  return Fl::run();
}
