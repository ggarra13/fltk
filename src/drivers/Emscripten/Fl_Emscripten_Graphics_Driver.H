//
// Definition of classes  Fl_Graphics_Driver
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2024 by Bill Spitzak and others.
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

#ifndef FL_EMSCRIPTEN_GRAPHICS_DRIVER_H
#define FL_EMSCRIPTEN_GRAPHICS_DRIVER_H

#include <FL/Fl_Graphics_Driver.H>
#include <emscripten/val.h>

class Fl_Emscripten_Graphics_Driver : public Fl_Graphics_Driver {
  emscripten::EM_VAL dummy_ctxt;
  int width_;
  int line_style_;
  const char *linecap_;
  const char *linejoin_;
  char linedash_[256];

protected:
  emscripten::EM_VAL ctxt;

public:
  Fl_Emscripten_Graphics_Driver();
  void context(emscripten::EM_VAL val);
  emscripten::EM_VAL context();
  void scale(float f) FL_OVERRIDE;
  void restore_scale(float f) FL_OVERRIDE;
  void point(int x, int y) FL_OVERRIDE;
  void focus_rect(int x, int y, int w, int h) FL_OVERRIDE;
  void rect(int x, int y, int w, int h) FL_OVERRIDE;
  void rectf(int x, int y, int w, int h) FL_OVERRIDE; // implementation of drawing methods
  void color(Fl_Color c) FL_OVERRIDE;
  void color(uchar r, uchar g, uchar b) FL_OVERRIDE;
  Fl_Color color() FL_OVERRIDE;

  Fl_Font set_fonts(const char *name) FL_OVERRIDE;
  void init_built_in_fonts();

  void push_clip(int x, int y, int w, int h) FL_OVERRIDE;
  void push_no_clip() FL_OVERRIDE;
  void pop_clip() FL_OVERRIDE;
  int clip_box(int x, int y, int w, int h, int &X, int &Y, int &W, int &H) FL_OVERRIDE;
  int not_clipped(int x, int y, int w, int h) FL_OVERRIDE;

  void line_style(int style, int width = 0, char *dashes = 0) FL_OVERRIDE;

  void line(int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void line(int x0, int y0, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void draw(const char *str, int nChars, int x, int y) FL_OVERRIDE;
  double width(const char *str, int nChars) FL_OVERRIDE;

  void draw(const char *str, int nChars, float x, float y) FL_OVERRIDE;
  void draw(int angle, const char *str, int nChars, int x, int y) FL_OVERRIDE;
  void rtl_draw(const char *str, int n, int x, int y) FL_OVERRIDE;
  int height() FL_OVERRIDE;

  void concat();
  void reconcat();
  void circle(double x, double y, double r) FL_OVERRIDE;
  void arc(double x, double y, double r, double start, double end) FL_OVERRIDE;
  void arc(int x, int y, int w, int h, double a1, double a2) FL_OVERRIDE;
  void pie(int x, int y, int w, int h, double a1, double a2) FL_OVERRIDE;

  void xyline(int x, int y, int x1) FL_OVERRIDE;
  void xyline(int x, int y, int x1, int y2) FL_OVERRIDE;
  void xyline(int x, int y, int x1, int y2, int x3) FL_OVERRIDE;
  void yxline(int x, int y, int y1) FL_OVERRIDE;
  void yxline(int x, int y, int y1, int x2) FL_OVERRIDE;
  void yxline(int x, int y, int y1, int x2, int y3) FL_OVERRIDE;
  void begin_points() FL_OVERRIDE;
  void begin_line() FL_OVERRIDE;
  void begin_loop() FL_OVERRIDE;
  void begin_polygon() FL_OVERRIDE;
  void begin_complex_polygon() FL_OVERRIDE;
  void end_complex_polygon() FL_OVERRIDE;
  void end_points() FL_OVERRIDE;
  void end_line() FL_OVERRIDE;
  void end_loop() FL_OVERRIDE;
  void end_polygon() FL_OVERRIDE;
  void vertex(double x, double y) FL_OVERRIDE;
  void transformed_vertex(double x, double y) FL_OVERRIDE;
  void loop(int x0, int y0, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) FL_OVERRIDE;
  void polygon(int x0, int y0, int x1, int y1, int x2, int y2) FL_OVERRIDE;
  void polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) FL_OVERRIDE;
  void curve(double x, double y, double x1, double y1, double x2, double y2, double x3,
             double y3) FL_OVERRIDE;

  void font(Fl_Font fnum, Fl_Fontsize s) FL_OVERRIDE;
  const char *font_name(int num) FL_OVERRIDE;
  void font_name(int num, const char *name) FL_OVERRIDE;

  void draw_image(const uchar *data, int ix, int iy, int iw, int ih, int D, int LD) FL_OVERRIDE;
  void draw_image_mono(const uchar *d, int x, int y, int w, int h, int delta = 1,
                       int ld = 0) FL_OVERRIDE;
  void draw_image(Fl_Draw_Image_Cb call, void *data, int x, int y, int w, int h,
                  int delta = 3) FL_OVERRIDE;
  void draw_image_mono(Fl_Draw_Image_Cb call, void *data, int x, int y, int w, int h,
                       int delta = 1) FL_OVERRIDE;
  void draw_fixed(Fl_RGB_Image *rgb, int XP, int YP, int WP, int HP, int cx, int cy) FL_OVERRIDE;
  void draw_fixed(Fl_Bitmap *bm, int XP, int YP, int WP, int HP, int cx, int cy) FL_OVERRIDE;
  void draw_fixed(Fl_Pixmap *rgb, int XP, int YP, int WP, int HP, int cx, int cy) FL_OVERRIDE;

  void copy_offscreen(int x, int y, int w, int h, Fl_Offscreen pixmap, int srcx,
                      int srcy) FL_OVERRIDE;

  void ps_translate(int x, int y);
  void ps_untranslate(void);
};

#endif