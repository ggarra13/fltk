//
// Rectangle drawing routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2020 by Bill Spitzak and others.
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


/**
 \file Fl_Vulkan_Graphics_Driver_rect.cxx
 \brief Vulkan specific line and polygon drawing with integer coordinates.
 */

#include <config.h>
#include "Fl_Vulkan_Graphics_Driver.H"
#include <FL/vk.h>
#include <FL/Fl_Vk_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl.H>
#include <FL/math.h>

// --- line and polygon drawing with integer coordinates

void Fl_Vulkan_Graphics_Driver::point(int x, int y) {
  if (line_width_ == 1.0f) {
    vkBegin(VK_POINTS);
    vkVertex2f(x+0.5f, y+0.5f);
    vkEnd();
  } else {
    float offset = line_width_ / 2.0f;
    float xx = x+0.5f, yy = y+0.5f;
    vkRectf(xx-offset, yy-offset, xx+offset, yy+offset);
  }
}

void Fl_Vulkan_Graphics_Driver::rect(int x, int y, int w, int h) {
  float offset = line_width_ / 2.0f;
  float xx = x+0.5f, yy = y+0.5f;
  float rr = x+w-0.5f, bb = y+h-0.5f;
  vkRectf(xx-offset, yy-offset, rr+offset, yy+offset);
  vkRectf(xx-offset, bb-offset, rr+offset, bb+offset);
  vkRectf(xx-offset, yy-offset, xx+offset, bb+offset);
  vkRectf(rr-offset, yy-offset, rr+offset, bb+offset);
}

void Fl_Vulkan_Graphics_Driver::rectf(int x, int y, int w, int h) {
  if (w<=0 || h<=0) return;
  vkRectf((VKfloat)x, (VKfloat)y, (VKfloat)(x+w), (VKfloat)(y+h));
}

void Fl_Vulkan_Graphics_Driver::line(int x, int y, int x1, int y1) {
  if (x==x1 && y==y1) return;
  if (x==x1) {
    yxline(x, y, y1);
    return;
  }
  if (y==y1) {
    xyline(x, y, x1);
    return;
  }
  float xx = x+0.5f, xx1 = x1+0.5f;
  float yy = y+0.5f, yy1 = y1+0.5f;
  if (line_width_==1.0f) {
    vkBegin(VK_LINE_STRIP);
    vkVertex2f(xx, yy);
    vkVertex2f(xx1, yy1);
    vkEnd();
  } else {
    float dx = xx1-xx, dy = yy1-yy;
    float len = sqrtf(dx*dx+dy*dy);
    dx = dx/len*line_width_*0.5f;
    dy = dy/len*line_width_*0.5f;

    vkBegin(VK_TRIANVKE_STRIP);
    vkVertex2f(xx-dy, yy+dx);
    vkVertex2f(xx+dy, yy-dx);
    vkVertex2f(xx1-dy, yy1+dx);
    vkVertex2f(xx1+dy, yy1-dx);
    vkEnd();
  }
}

void Fl_Vulkan_Graphics_Driver::line(int x, int y, int x1, int y1, int x2, int y2) {
  // TODO: no corner types (miter) yet
  line(x, y, x1, y1);
  line(x1, y1, x2, y2);
}

void Fl_Vulkan_Graphics_Driver::xyline(int x, int y, int x1) {
  float offset = line_width_ / 2.0f;
  float xx = (float)x, yy = y+0.5f, rr = x1+1.0f;
  vkRectf(xx, yy-offset, rr, yy+offset);
}

void Fl_Vulkan_Graphics_Driver::xyline(int x, int y, int x1, int y2) {
  float offset = line_width_ / 2.0f;
  float xx = (float)x, yy = y+0.5f, rr = x1+0.5f, bb = y2+1.0f;
  vkRectf(xx, yy-offset, rr+offset, yy+offset);
  vkRectf(rr-offset, yy+offset, rr+offset, bb);
}

void Fl_Vulkan_Graphics_Driver::xyline(int x, int y, int x1, int y2, int x3) {
  float offset = line_width_ / 2.0f;
  float xx = (float)x, yy = y+0.5f, xx1 = x1+0.5f, rr = x3+1.0f, bb = y2+0.5f;
  vkRectf(xx, yy-offset, xx1+offset, yy+offset);
  vkRectf(xx1-offset, yy+offset, xx1+offset, bb+offset);
  vkRectf(xx1+offset, bb-offset, rr, bb+offset);
}

void Fl_Vulkan_Graphics_Driver::yxline(int x, int y, int y1) {
  float offset = line_width_ / 2.0f;
  float xx = x+0.5f, yy = (float)y, bb = y1+1.0f;
  vkRectf(xx-offset, yy, xx+offset, bb);
}

void Fl_Vulkan_Graphics_Driver::yxline(int x, int y, int y1, int x2) {
  float offset = line_width_ / 2.0f;
  float xx = x+0.5f, yy = (float)y, rr = x2+1.0f, bb = y1+0.5f;
  vkRectf(xx-offset, yy, xx+offset, bb+offset);
  vkRectf(xx+offset, bb-offset, rr, bb+offset);
}

void Fl_Vulkan_Graphics_Driver::yxline(int x, int y, int y1, int x2, int y3) {
  float offset = line_width_ / 2.0f;
  float xx = x+0.5f, yy = (float)y, yy1 = y1+0.5f, rr = x2+0.5f, bb = y3+1.0f;
  vkRectf(xx-offset, yy, xx+offset, yy1+offset);
  vkRectf(xx+offset, yy1-offset, rr+offset, yy1+offset);
  vkRectf(rr-offset, yy1+offset, rr+offset, bb);
}

void Fl_Vulkan_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2) {
  vkBegin(VK_LINE_LOOP);
  vkVertex2i(x0, y0);
  vkVertex2i(x1, y1);
  vkVertex2i(x2, y2);
  vkEnd();
}

void Fl_Vulkan_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  vkBegin(VK_LINE_LOOP);
  vkVertex2i(x0, y0);
  vkVertex2i(x1, y1);
  vkVertex2i(x2, y2);
  vkVertex2i(x3, y3);
  vkEnd();
}

void Fl_Vulkan_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2) {
  vkBegin(VK_POLYGON);
  vkVertex2i(x0, y0);
  vkVertex2i(x1, y1);
  vkVertex2i(x2, y2);
  vkEnd();
}

void Fl_Vulkan_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
  vkBegin(VK_POLYGON);
  vkVertex2i(x0, y0);
  vkVertex2i(x1, y1);
  vkVertex2i(x2, y2);
  vkVertex2i(x3, y3);
  vkEnd();
}

void Fl_Vulkan_Graphics_Driver::focus_rect(int x, int y, int w, int h) {
  float width = line_width_;
  int stipple = line_stipple_;
  line_style(FL_DOT, 1);
  vkBegin(VK_LINE_LOOP);
  vkVertex2f(x+0.5f, y+0.5f);
  vkVertex2f(x+w+0.5f, y+0.5f);
  vkVertex2f(x+w+0.5f, y+h+0.5f);
  vkVertex2f(x+0.5f, y+h+0.5f);
  vkEnd();
  line_style(stipple, (int)width);
}


// -----------------------------------------------------------------------------

static int vk_min(int a, int b) { return (a<b) ? a : b; }
static int vk_max(int a, int b) { return (a>b) ? a : b; }

enum {
  kStateFull, // Region is the full window
  kStateRect, // Region is a rectanvke
  kStateEmpty // Region is an empty space
};

typedef struct Fl_Vk_Region {
  int x, y, w, h;
  int vk_x, vk_y, vk_w, vk_h;
  char state;
  void set(int inX, int inY, int inW, int inH) {
    if (inW<=0 || inH<=0) {
      state = kStateEmpty;
      x = inX; y = inY; w = 1; h = 1; // or 0?
    } else {
      x = inX; y = inY; w = inW; h = inH;
      state = kStateRect;
    }
    Fl_Vk_Window *win = Fl_Vk_Window::current()->as_vk_window();
    if (win) {
      float scale = win->pixels_per_unit();
      vk_x = int(x*scale);
      vk_y = int((win->h()-h-y+1)*scale);
      vk_w = int((w-1)*scale);
      vk_h = int((h-1)*scale);
      if (inX<=0 && inY<=0 && inX+inW>win->w() && inY+inH>=win->h()) {
        state = kStateFull;
      }
    } else {
      state = kStateFull;
    }
  }
  void set_full() { state = kStateFull; }
  void set_empty() { state = kStateEmpty; }
  void set_intersect(int inX, int inY, int inW, int inH, Fl_Vk_Region &g) {
    if (g.state==kStateFull) {
      set(inX, inY, inW, inH);
    } else if (g.state==kStateEmpty) {
      set_empty();
    } else {
      int rx = vk_max(inX, g.x);
      int ry = vk_max(inY, g.y);
      int rr = vk_min(inX+inW, g.x+g.w);
      int rb = vk_max(inY+inH, g.y+g.h);
      set(rx, ry, rr-rx, rb-ry);
    }
  }
  void apply() {
    if (state==kStateFull) {
      vkDisable(VK_SCISSOR_TEST);
    } else {
      vkScissor(vk_x, vk_y, vk_w, vk_h);
      vkEnable(VK_SCISSOR_TEST);
    }
  }
} Fl_Vk_Region;

static int vk_rstackptr = 0;
static const int vk_region_stack_max = FL_REGION_STACK_SIZE - 1;
static Fl_Vk_Region vk_rstack[FL_REGION_STACK_SIZE];

/*
 Intersect the given rect with the current rect, push the result on the stack,
 and apply the new clipping area.
 */
void Fl_Vulkan_Graphics_Driver::push_clip(int x, int y, int w, int h) {
  if (vk_rstackptr==vk_region_stack_max) {
    Fl::warning("Fl_Vulkan_Graphics_Driver::push_clip: clip stack overflow!\n");
    return;
  }
  if (vk_rstackptr==0) {
    vk_rstack[vk_rstackptr].set(x, y, w, h);
  } else {
    vk_rstack[vk_rstackptr].set_intersect(x, y, w, h, vk_rstack[vk_rstackptr-1]);
  }
  vk_rstack[vk_rstackptr].apply();
  vk_rstackptr++;
}

/*
 Remove the current clipping area and apply the previous one on the stack.
 */
void Fl_Vulkan_Graphics_Driver::pop_clip() {
  if (vk_rstackptr==0) {
    vkDisable(VK_SCISSOR_TEST);
    Fl::warning("Fl_Vulkan_Graphics_Driver::pop_clip: clip stack underflow!\n");
    return;
  }
  vk_rstackptr--;
  restore_clip();
}

/*
 Push a full area onton the stack, so no clipping will take place.
 */
void Fl_Vulkan_Graphics_Driver::push_no_clip() {
  if (vk_rstackptr==vk_region_stack_max) {
    Fl::warning("Fl_Vulkan_Graphics_Driver::push_no_clip: clip stack overflow!\n");
    return;
  }
  vk_rstack[vk_rstackptr].set_full();
  vk_rstack[vk_rstackptr].apply();
  vk_rstackptr++;
}

/*
 We don't know the format of clip regions of the default driver, so return NULL.
 */
Fl_Region Fl_Vulkan_Graphics_Driver::clip_region() {
  return NULL;
}

/*
 We don't know the format of clip regions of the default driver, so do the best
 we can.
 */
void Fl_Vulkan_Graphics_Driver::clip_region(Fl_Region r) {
  if (r==NULL) {
    vkDisable(VK_SCISSOR_TEST);
  } else {
    restore_clip();
  }
}

/*
 Apply the current clipping rect.
 */
void Fl_Vulkan_Graphics_Driver::restore_clip() {
  if (vk_rstackptr==0) {
    vkDisable(VK_SCISSOR_TEST);
  } else {
    vk_rstack[vk_rstackptr-1].apply();
  }
}

/*
 Does the rectanvke intersect the current clip region?
 0 = regions don't intersect, nothing to draw
 1 = region is fully inside current clipping region
 2 = region is partially inside current clipping region
 */
int Fl_Vulkan_Graphics_Driver::not_clipped(int x, int y, int w, int h) {
  if (vk_rstackptr==0)
    return 1;
  Fl_Vk_Region &g = vk_rstack[vk_rstackptr-1];
  if (g.state==kStateFull)
    return 1;
  if (g.state==kStateEmpty)
    return 0;
  int r = x+w, b = y + h;
  int gr = g.x+g.w, gb = g.y+g.h;
  if (r<=g.x || x>=gr || b<=g.y || y>=gb) return 0;
  if (x>=g.x && y>=g.y && r<=gr && b<=gb) return 1;
  return 2;
}

/*
 Calculate the intersection of the given rect and the clipping area.
 Return 0 if the result did not change.
 */
int Fl_Vulkan_Graphics_Driver::clip_box(int x, int y, int w, int h, int &X, int &Y, int &W, int &H) {
  X = x; Y = y; W = w; H = h;
  if (vk_rstackptr==0)
    return 0;
  Fl_Vk_Region &g = vk_rstack[vk_rstackptr-1];
  if (g.state==kStateFull)
    return 0;
  int r = x+w, b = y + h;
  int gr = g.x+g.w, gb = g.y+g.h;
  X = vk_max(x, g.x);
  Y = vk_max(y, g.y);
  W = vk_min(r, gr) - X;
  H = vk_min(b, gb) - Y;
  return (x!=X || y!=Y || w!=W || h!=H);
}
