//
// Graphics drawing routines for the Fast Light Tool Kit (FLTK).
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
//


#include "Fl_Emscripten_Graphics_Driver.H"
#include <FL/Enumerations.H>
#include <FL/Fl_Graphics_Driver.H>
#include <FL/Fl_Image.H>
#include <FL/platform.H>
#include <FL/fl_draw.H>
#include <emscripten.h>
#include <emscripten/val.h>
#include <vector>

using namespace emscripten;

extern unsigned int fl_cmap[256];

// We use a std vector and not Font Descriptors for simplicity.
// Emscripten depends on the C++ std library, so might as well use it!
static std::vector<Fl_Fontdesc> built_in_table = {
    {" Arial"},   {"BArial"},   {"IArial"},   {"PArial"},     {" Courier"}, {"BCourier"},
    {"ICourier"}, {"PCourier"}, {" Times"},   {"BTimes"},     {"ITimes"},   {"PTimes"},
    {" Symbol"},  {" Verdana"}, {"BVerdana"}, {" Wingdings"},
};

const char *style(const char *font) {
  char first = font[0];
  switch (first) {
    case ' ':
      return "";
    case 'P':
      return "bold italic";
    case 'B':
      return "bold";
    case 'I':
      return "italic";
    default:
      return "";
  }
}

FL_EXPORT Fl_Fontdesc *fl_fonts = built_in_table.data();


static const int dashes_flat[5][7] = {{-1, 0, 0, 0, 0, 0, 0},
                                      {3, 1, -1, 0, 0, 0, 0},
                                      {1, 1, -1, 0, 0, 0, 0},
                                      {3, 1, 1, 1, -1, 0, 0},
                                      {3, 1, 1, 1, 1, 1, -1}};
static const double dashes_cap[5][7] = {{-1, 0, 0, 0, 0, 0, 0},
                                        {2, 2, -1, 0, 0, 0, 0},
                                        {0.01, 1.99, -1, 0, 0, 0, 0},
                                        {2, 2, 0.01, 1.99, -1, 0, 0},
                                        {2, 2, 0.01, 1.99, 0.01, 1.99, -1}};

struct callback_data {
  const uchar *data;
  int D, LD;
};

static void draw_image_cb(void *data, int x, int y, int w, uchar *buf) {
  struct callback_data *cb_data;
  const uchar *curdata;

  cb_data = (struct callback_data *)data;
  int last = x + w;
  const size_t aD = abs(cb_data->D);
  curdata = cb_data->data + x * cb_data->D + y * cb_data->LD;
  for (; x < last; x++) {
    memcpy(buf, curdata, aD);
    buf += aD;
    curdata += cb_data->D;
  }
}

class EmClip {
public:
  int x, y, w, h;
  EmClip *prev;
};
EmClip *emclip_;

void formatFont(Fl_Font font, Fl_Fontsize sz, char *output) {
  const char *fontname = built_in_table[font].name;
  sprintf(output, "%s %dpx %s", style(fontname), sz, &fontname[1]);
}

// Drawing is done using a canvas 2d context.
Fl_Emscripten_Graphics_Driver::Fl_Emscripten_Graphics_Driver()
  : Fl_Graphics_Driver() {
  width_ = 0;
  line_style_ = 0;
  linecap_ = "butt";
  linejoin_ = "miter";
  what = NONE;
  ctxt = NULL;
  dummy_ctxt = NULL;
}

void Fl_Emscripten_Graphics_Driver::context(EM_VAL val) {
  if (dummy_ctxt) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.canvas = null;
        },
        dummy_ctxt);
    dummy_ctxt = NULL;
  }
  ctxt = val;
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
        ctx.lineWidth = 0;
        ctx.save();
      },
      ctxt);
}

EM_VAL Fl_Emscripten_Graphics_Driver::context() {
  return ctxt;
}

void Fl_Emscripten_Graphics_Driver::point(int x, int y) {
  rectf(x, y, 1, 1);
}

void Fl_Emscripten_Graphics_Driver::focus_rect(int x, int y, int w, int h) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.lineWidth = 1;
        ctx.lineCap = 'butt';
        ctx.lineJoin = 'miter';
        ctx.setLineDash([ 1, 1 ]);
        ctx.rect($1, $2, $3, $4);
        ctx.stroke();
        ctx.restore();
      },
      ctxt, x, y, w - 1, h - 1);
}
void Fl_Emscripten_Graphics_Driver::rect(int x, int y, int w, int h) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.rect($1, $2, $3, $4);
        ctx.stroke();
      },
      ctxt, x, y, w - 1, h - 1);
}

void Fl_Emscripten_Graphics_Driver::rectf(int x, int y, int w, int h) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.fillRect($1 - 0.5, $2 - 0.5, $3, $4);
      },
      ctxt, x, y, w, h);
}

void Fl_Emscripten_Graphics_Driver::color(Fl_Color i) {
  Fl_Graphics_Driver::color(i);
  uchar r, g, b;
  if (i & 0xFFFFFF00) {
    // translate rgb colors into color index
    r = i >> 24;
    g = i >> 16;
    b = i >> 8;
  } else {
    // translate index into rgb:
    unsigned c = fl_cmap[i];
    c = c ^ 0x000000ff; // trick to restore the color's correct alpha value
    r = c >> 24;
    g = c >> 16;
    b = c >> 8;
  }
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        const col = `rgb(${$1} ${$2} ${$3})`;
        ctx.fillStyle = col;
        ctx.strokeStyle = col;
      },
      ctxt, r, g, b);
}

void Fl_Emscripten_Graphics_Driver::color(uchar r, uchar g, uchar b) {
  Fl_Graphics_Driver::color(fl_rgb_color(r, g, b));
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        const col = `rgb(${$1} ${$2} ${$3})`;
        ctx.fillStyle = col;
        ctx.strokeStyle = col;
      },
      ctxt, r, g, b);
}

Fl_Color Fl_Emscripten_Graphics_Driver::color() {
  return Fl_Graphics_Driver::color();
}

int Fl_Emscripten_Graphics_Driver::clip_box(int x, int y, int w, int h, int &X, int &Y, int &W,
                                            int &H) {
  if (!emclip_) {
    X = x;
    Y = y;
    W = w;
    H = h;
    return 0;
  }
  if (emclip_->w < 0) {
    X = x;
    Y = y;
    W = w;
    H = h;
    return 1;
  }
  int ret = 0;
  if (x > (X = emclip_->x)) {
    X = x;
    ret = 1;
  }
  if (y > (Y = emclip_->y)) {
    Y = y;
    ret = 1;
  }
  if ((x + w) < (emclip_->x + emclip_->w)) {
    W = x + w - X;

    ret = 1;

  } else
    W = emclip_->x + emclip_->w - X;
  if (W < 0) {
    W = 0;
    return 1;
  }
  if ((y + h) < (emclip_->y + emclip_->h)) {
    H = y + h - Y;
    ret = 1;
  } else
    H = emclip_->y + emclip_->h - Y;
  if (H < 0) {
    W = 0;
    H = 0;
    return 1;
  }
  return ret;
}


void Fl_Emscripten_Graphics_Driver::push_clip(int x, int y, int w, int h) {
  EmClip *c = new EmClip();
  clip_box(x, y, w, h, c->x, c->y, c->w, c->h);
  c->prev = emclip_;
  emclip_ = c;
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.rect($1, $2, $3, $4);
        ctx.clip();
      },
      ctxt, emclip_->x - 0.5, emclip_->y - 0.5, emclip_->w, emclip_->h);
}

void Fl_Emscripten_Graphics_Driver::push_no_clip() {
  EmClip *c = new EmClip();
  c->prev = emclip_;
  emclip_ = c;
  emclip_->x = emclip_->y = emclip_->w = emclip_->h = -1;
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.restore();
      },
      ctxt);
}

void Fl_Emscripten_Graphics_Driver::pop_clip() {
  if (!emclip_)
    return;
  EmClip *c = emclip_;
  emclip_ = emclip_->prev;
  delete c;
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
      },
      ctxt);
}

void Fl_Emscripten_Graphics_Driver::line_style(int style, int width, char *dashes) {
  line_style_ = style;
  if (dashes) {
    if (dashes != linedash_)
      strcpy(linedash_, dashes);

  } else
    linedash_[0] = 0;
  if (width == 0)
    width = 1;
  width_ = width;
  int cap_part = style & 0xF00;
  if (cap_part == FL_CAP_SQUARE)
    linecap_ = "square";
  else if (cap_part == FL_CAP_ROUND)
    linecap_ = "round";
  else
    linecap_ = "butt";
  int join_part = style & 0xF000;
  if (join_part == FL_JOIN_BEVEL)
    linejoin_ = "bevel";
  else if (join_part == FL_JOIN_MITER)
    linejoin_ = "miter";
  else if (join_part == FL_JOIN_ROUND)
    linejoin_ = "round";
  else
    linejoin_ = "miter";


  double *ddashes = NULL;
  int l = 0;
  if (dashes && *dashes) {
    ddashes = new double[strlen(dashes)];
    while (dashes[l]) {
      ddashes[l] = dashes[l];
      l++;
    }
  } else if (style & 0xff) {
    ddashes = new double[6];
    if (style & 0x200) { // round and square caps, dash length need to be adjusted
      const double *dt = dashes_cap[style & 0xff];
      while (*dt >= 0) {
        ddashes[l++] = width * (*dt);
        dt++;
      }
    } else {
      const int *ds = dashes_flat[style & 0xff];
      while (*ds >= 0) {
        ddashes[l++] = width * (*ds);
        ds++;
      }
    }
  }
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.lineWidth = $1;
        ctx.lineCap = UTF8ToString($2);
        ctx.lineJoin = UTF8ToString($3);
        ctx.setLineDash(new Float64Array(HEAPF64.buffer, $4, $5));
      },
      ctxt, width_, linecap_, linejoin_, ddashes, l);
}

void Fl_Emscripten_Graphics_Driver::line(int x1, int y1, int x2, int y2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.stroke();
      },
      ctxt, x1, y1, x2, y2);
}

void Fl_Emscripten_Graphics_Driver::line(int x0, int y0, int x1, int y1, int x2, int y2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $6);
        ctx.stroke();
      },
      ctxt, x0, y0, x1, y1, x2, y2);
}

void Fl_Emscripten_Graphics_Driver::draw(const char *str, int nChars, int x, int y) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.translate($1 - 2, $2 - 2);
        ctx.textAlign = 'left';
        ctx.fillText(UTF8ToString($3, $4), 0, 0);
        ctx.restore();
      },
      ctxt, x, y, str, nChars);
}

double Fl_Emscripten_Graphics_Driver::width(const char *str, int nChars) {
  return EM_ASM_DOUBLE(
      {
        let ctx = Emval.toValue($0);
        let metrics = ctx.measureText(UTF8ToString($1, $2));
        return metrics.width;
      },
      ctxt, str, nChars);
}

void Fl_Emscripten_Graphics_Driver::draw(const char *str, int n, float x, float y) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.translate($1, $2 - 2.0);
        ctx.textAlign = 'left';
        ctx.fillText(UTF8ToString($3, $4), 0, 0);
        ctx.restore();
      },
      ctxt, x, y, str, n);
}


void Fl_Emscripten_Graphics_Driver::draw(int rotation, const char *str, int n, int x, int y) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.translate($1, $2 - 2.0);
        ctx.rotate(-1 * $5 * Math.PI / 180);
        ctx.textAlign = 'left';
        ctx.fillText(UTF8ToString($3, $4), 0, 0);
        ctx.restore();
      },
      ctxt, x, y, str, n, rotation);
}

void Fl_Emscripten_Graphics_Driver::rtl_draw(const char *str, int n, int x, int y) {
  int w = (int)width(str, n);
  draw(str, n, x - w, y);
}

int Fl_Emscripten_Graphics_Driver::height() {
  return size();
}


void Fl_Emscripten_Graphics_Driver::concat() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.transform($1, $2, $3, $4, $5, $6);
      },
      ctxt, m.a, m.b, m.c, m.d, m.x, m.y);
}

int invert_matrix(const Fl_Graphics_Driver::matrix &m, Fl_Graphics_Driver::matrix &newmatrix) {
  int det = m.a * m.d - m.b * m.c;
  if (!det)
    return -1;
  double invDet = 1 / det;
  newmatrix.a = m.d * invDet;
  newmatrix.b = -m.b * invDet;
  newmatrix.c = -m.c * invDet;
  newmatrix.d = m.a * invDet;
  newmatrix.x = (m.c * m.y - m.d * m.x) * invDet;
  newmatrix.y = (m.b * m.x - m.a * m.y) * invDet;
  return 0;
}

void Fl_Emscripten_Graphics_Driver::reconcat() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.resetTransform();
      },
      ctxt);
}

void Fl_Emscripten_Graphics_Driver::circle(double x, double y, double r) {
  if (what == NONE) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.save();
        },
        ctxt);
    concat();
    EM_ASM(
        {
          let x = $1;
          let y = $2;
          let r = $3;
          let ctx = Emval.toValue($0);
          ctx.arc(x, y, r, 0, 2 * Math.PI);
          ctx.stroke();
        },
        ctxt, x, y, r);
    reconcat();
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.restore();
        },
        ctxt);
  } else {
    EM_ASM(
        {
          let x = $1;
          let y = $2;
          let r = $3;
          let ctx = Emval.toValue($0);
          ctx.arc(x, y, r, 0, 2 * Math.PI);
        },
        ctxt, x, y, r);
  }
}

void Fl_Emscripten_Graphics_Driver::arc(double x, double y, double r, double start, double end) {
  if (what == NONE)
    return;
  if (gap_ == 1) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.beginPath();
        },
        ctxt);
  }
  gap_ = 0;
  EM_ASM(
      {
        let x = $1;
        let y = $2;
        let r = $3;
        let start = $4;
        let end = $5;
        let ctx = Emval.toValue($0);
        if (start > end) {
          ctx.arc(x, y, r, -start * Math.PI / 180, -end * Math.PI / 180);
        } else {
          ctx.arc(x, y, r, -start * Math.PI / 180, -end * Math.PI / 180, true);
        }
      },
      ctxt, x, y, r, start, end);
}

void Fl_Emscripten_Graphics_Driver::arc(int x, int y, int w, int h, double a1, double a2) {
  if (w <= 1 || h <= 1)
    return;
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  begin_line();
  EM_ASM(
      {
        let x = $1;
        let y = $2;
        let w = $3;
        let h = $4;
        let a1 = $5;
        let a2 = $6;
        let ctx = Emval.toValue($0);
        ctx.translate(x + w / 2.0 - 0.5, y + h / 2.0 - 0.5);
        ctx.scale((w - 1) / 2.0, (h - 1) / 2.0);
      },
      ctxt, x, y, w, h, a1, a2);
  arc(0, 0, 1, a2, a1);
  EM_ASM(
      {
        let x = $1;
        let y = $2;
        let w = $3;
        let h = $4;
        let a1 = $5;
        let a2 = $6;
        let ctx = Emval.toValue($0);
        ctx.scale(2.0 / (w - 1), 2.0 / (h - 1));
        ctx.translate(-x - w / 2.0 + 0.5, -y - h / 2.0 + 0.5);
      },
      ctxt, x, y, w, h, a1, a2);
  end_line();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
      },
      ctxt);
}

void Fl_Emscripten_Graphics_Driver::pie(int x, int y, int w, int h, double a1, double a2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  begin_polygon();
  EM_ASM(
      {
        let x = $1;
        let y = $2;
        let w = $3;
        let h = $4;
        let ctx = Emval.toValue($0);
        ctx.translate(x + w / 2.0 - 0.5, y + h / 2.0 - 0.5);
        ctx.scale(w / 2.0, h / 2.0);
      },
      ctxt, x, y, w, h);
  vertex(0, 0);
  arc(0.0, 0.0, 1, a2, a1);
  end_polygon();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
      },
      ctxt);
}


void Fl_Emscripten_Graphics_Driver::xyline(int x, int y, int x1) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $2);
        ctx.stroke();
      },
      ctxt, x, y, x1);
}

void Fl_Emscripten_Graphics_Driver::xyline(int x, int y, int x1, int y2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $2);
        ctx.lineTo($3, $4);
        ctx.stroke();
      },
      ctxt, x, y, x1, y2);
}

void Fl_Emscripten_Graphics_Driver::xyline(int x, int y, int x1, int y2, int x3) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $4);
        ctx.stroke();
      },
      ctxt, x, y, x1, y2, x3);
}

void Fl_Emscripten_Graphics_Driver::yxline(int x, int y, int y1) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($1, $3);
        ctx.stroke();
      },
      ctxt, x, y, y1);
}

void Fl_Emscripten_Graphics_Driver::yxline(int x, int y, int y1, int x2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($1, $3);
        ctx.lineTo($4, $3);
        ctx.stroke();
      },
      ctxt, x, y, y1, x2);
}

void Fl_Emscripten_Graphics_Driver::yxline(int x, int y, int y1, int x2, int y3) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($1, $3);
        ctx.lineTo($4, $3);
        ctx.lineTo($4, $5);
        ctx.stroke();
      },
      ctxt, x, y, y1, x2, y3);
}


void Fl_Emscripten_Graphics_Driver::begin_points() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  concat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
      },
      ctxt);
  gap_ = 1;
  what = POINTS;
}

void Fl_Emscripten_Graphics_Driver::begin_line() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  concat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
      },
      ctxt);
  gap_ = 1;
  what = LINE;
}

void Fl_Emscripten_Graphics_Driver::begin_loop() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  concat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
      },
      ctxt);
  gap_ = 1;
  what = LOOP;
}

void Fl_Emscripten_Graphics_Driver::begin_polygon() {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
      },
      ctxt);
  concat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.beginPath();
      },
      ctxt);
  gap_ = 1;
  what = POLYGON;
}

void Fl_Emscripten_Graphics_Driver::scale(float f) {
  Fl_Graphics_Driver::scale(f);
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
        ctx.save();
        ctx.scale($1, $1);
        ctx.translate(0.5, 0.5);
      },
      ctxt, f);
  // line_style(0);
}

void Fl_Emscripten_Graphics_Driver::restore_scale(float s) {
  // if (s != 1.f && Fl_Display_Device::display_device()->is_current()) {
  //   Fl::screen_driver()->scale(0, s);
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.scale($1, $1);
      },
      ctxt, s);
  // }
}

void Fl_Emscripten_Graphics_Driver::begin_complex_polygon() {
  begin_polygon();
  gap_ = 0;
}

void Fl_Emscripten_Graphics_Driver::end_complex_polygon() {
  end_polygon();
}

void Fl_Emscripten_Graphics_Driver::end_points() {
  end_line();
}

void Fl_Emscripten_Graphics_Driver::end_line() {
  gap_ = 1;
  reconcat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.stroke();
        ctx.restore();
      },
      ctxt);
  what = NONE;
}

void Fl_Emscripten_Graphics_Driver::end_loop() {
  gap_ = 1;
  reconcat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.closePath();
        ctx.stroke();
        ctx.restore();
      },
      ctxt);
  what = NONE;
}

void Fl_Emscripten_Graphics_Driver::end_polygon() {
  gap_ = 1;
  reconcat();
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
      },
      ctxt);
  what = NONE;
}


void Fl_Emscripten_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $6);
        ctx.closePath();
        ctx.stroke();
        ctx.restore();
      },
      ctxt, x0, y0, x1, y1, x2, y2);
}

void Fl_Emscripten_Graphics_Driver::loop(int x0, int y0, int x1, int y1, int x2, int y2, int x3,
                                         int y3) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $6);
        ctx.lineTo($7, $8);
        ctx.closePath();
        ctx.stroke();
        ctx.restore();
      },
      ctxt, x0, y0, x1, y1, x2, y2, x3, y3);
}

void Fl_Emscripten_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $6);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
      },
      ctxt, x0, y0, x1, y1, x2, y2);
}

void Fl_Emscripten_Graphics_Driver::polygon(int x0, int y0, int x1, int y1, int x2, int y2, int x3,
                                            int y3) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.beginPath();
        ctx.moveTo($1, $2);
        ctx.lineTo($3, $4);
        ctx.lineTo($5, $6);
        ctx.lineTo($7, $8);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
      },
      ctxt, x0, y0, x1, y1, x2, y2, x3, y3);
}


void Fl_Emscripten_Graphics_Driver::vertex(double x, double y) {
  if (what == POINTS) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.moveTo($1, $2);
          ctx.rect($1 - 0.5, $2 - 0.5, 1, 1);
          ctx.fill();
        },
        ctxt, x, y);
    gap_ = 1;
    return;
  }
  if (gap_) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.moveTo($1, $2);
        },
        ctxt, x, y);
    gap_ = 0;
  } else {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.lineTo($1, $2);
        },
        ctxt, x, y);
  }
}

void Fl_Emscripten_Graphics_Driver::curve(double x, double y, double x1, double y1, double x2,
                                          double y2, double x3, double y3) {
  if (what == NONE)
    return;
  if (what == POINTS)
    Fl_Graphics_Driver::curve(x, y, x1, y1, x2, y2, x3, y3);
  else {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          if ($1) {
            ctx.moveTo($2, $3);
          } else {
            ctx.lineTo($2, $3);
          }
          ctx.bezierCurveTo($4, $5, $6, $7, $8, $9);
        },
        ctxt, gap_, x, y, x1, y1, x2, y2, x3, y3);
    gap_ = 0;
  }
}


void Fl_Emscripten_Graphics_Driver::transformed_vertex(double x, double y) {
  if (what == POINTS) {
    EM_ASM(
        {
          let ctx = Emval.toValue($0);
          ctx.moveTo($1, $2);
        },
        ctxt, x, y);
    point(x, y);
    gap_ = 1;
  } else {
    reconcat();
    if (gap_) {
      EM_ASM(
          {
            let ctx = Emval.toValue($0);
            ctx.moveTo($1, $2);
          },
          ctxt, x, y);
      gap_ = 0;
    } else {
      EM_ASM(
          {
            let ctx = Emval.toValue($0);
            ctx.lineTo($1, $2);
          },
          ctxt, x, y);
    }
    concat();
  }
}


int Fl_Emscripten_Graphics_Driver::not_clipped(int x, int y, int w, int h) {
  if (!emclip_)
    return 1;
  if (emclip_->w < 0)
    return 1;
  int X = 0, Y = 0, W = 0, H = 0;
  clip_box(x, y, w, h, X, Y, W, H);
  if (W)
    return 1;
  return 0;
}


static int font_name_process(const char *name, char &face) {
  face = ' ';
  if (strncmp(name, "bold italic", strlen("bold italic")) == 0)
    face = 'P';
  else if (strncmp(name, "bold", strlen("bold")) == 0)
    face = 'B';
  else if (strncmp(name, "italic", strlen("italic")) == 0)
    face = 'I';
  int l = strlen(name);
  return l;
}


typedef int (*sort_f_type)(const void *aa, const void *bb);

// TODO: to use when local font access api becomes widely available
static int font_sort(Fl_Fontdesc *fa, Fl_Fontdesc *fb) {
  char face_a, face_b;
  int la = font_name_process(fa->name, face_a);
  int lb = font_name_process(fb->name, face_b);
  int c = strncasecmp(fa->name, fb->name, la >= lb ? lb : la);
  return (c == 0 ? face_a - face_b : c);
}

// Only chrome supports local fonts access api for now.
// Also due to privacy considerations, a local-fonts permission policy needs to be added to the
// response header:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Permissions-Policy/local-fonts
Fl_Font Fl_Emscripten_Graphics_Driver::set_fonts(const char *name) {
  init_built_in_fonts();
  // clang-format off
  bool has_query_fonts_api = EM_ASM_INT({
    let has_api = false;
    if ("queryLocalFonts" in window) {
      navigator.permissions.query({ name: "local-fonts" }).then((result) => {
      if (result.state === "granted" || result.state === "prompt") {
        has_api = true;
      }});
    }
    return has_api;
  });
  // clang-format on
  if (!has_query_fonts_api) {
    Fl::set_font((Fl_Font)(FL_FREE_FONT + 1), name);
    built_in_table.push_back({name});
    return FL_FREE_FONT + 1;
  } else {
    val window = val::global("window");
    val availablefonts = window.call<val>("queryLocalFonts").await();
    std::vector<val> vec = vecFromJSArray<val>(availablefonts);
    int count = 0;
    for (const val &font : vec) {
      std::string familyname0 = font["family"].as<std::string>();
      int lfont = familyname0.size() + 2;
      const char *familyname = familyname0.c_str();
      char *fname = new char[lfont];
      snprintf(fname, lfont, " %s", familyname);
      char *regular = strdup(fname);
      Fl::set_font((Fl_Font)(count++ + FL_FREE_FONT), regular);
      built_in_table.push_back({regular});

      snprintf(fname, lfont, "B%s", familyname);
      char *bold = strdup(fname);
      Fl::set_font((Fl_Font)(count++ + FL_FREE_FONT), bold);
      built_in_table.push_back({bold});

      snprintf(fname, lfont, "I%s", familyname);
      char *italic = strdup(fname);
      Fl::set_font((Fl_Font)(count++ + FL_FREE_FONT), italic);
      built_in_table.push_back({italic});

      snprintf(fname, lfont, "P%s", familyname);
      char *bi = strdup(fname);
      Fl::set_font((Fl_Font)(count++ + FL_FREE_FONT), bi);
      // The returned fonts are already sorted.
      built_in_table.push_back({bi});
      delete[] fname;
    }
    return FL_FREE_FONT + count;
  }
}

void Fl_Emscripten_Graphics_Driver::init_built_in_fonts() {
  static int i = 0;
  if (!i) {
    while (i < FL_FREE_FONT) {
      i++;
      Fl::set_font((Fl_Font)i - 1, fl_fonts[i - 1].name);
    }
  }
}

void Fl_Emscripten_Graphics_Driver::font(Fl_Font fnum, Fl_Fontsize s) {
  if (!ctxt) {
    // We create a dummy context to get font measurements when no canvas context is available
    ctxt = (EM_VAL)EM_ASM_PTR({
      let canvas = new OffscreenCanvas(100, 100);
      let ctx = canvas.getContext("2d");
      ctx.lineWidth = 0;
      return Emval.toHandle(ctx);
    });
    dummy_ctxt = ctxt;
  }
  if (s == 0)
    return;
  if (fnum == -1) {
    Fl_Graphics_Driver::font(0, 0);
    return;
  }
  Fl_Graphics_Driver::font(fnum, s);
  char fontstr[32] = {0};
  formatFont(fnum, s, fontstr);
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.font = UTF8ToString($1);
      },
      ctxt, fontstr);
}

const char *Fl_Emscripten_Graphics_Driver::font_name(int num) {
  return built_in_table[num].name;
}


void Fl_Emscripten_Graphics_Driver::font_name(int num, const char *name) {
  built_in_table[num].name = name;
}

// The canvas only understands RGB images of depth 4 (RGBA8888), that means everything needs to be converted to it.
// Can't think of another solution!
void Fl_Emscripten_Graphics_Driver::draw_fixed(Fl_RGB_Image *rgb, int XP, int YP, int WP, int HP,
                                               int cx, int cy) {
  int X, Y, W, H;
  // Don't draw an empty image...
  if (!rgb->d() || !rgb->array) {
    Fl_Graphics_Driver::draw_empty(rgb, XP, YP);
    return;
  }
  if (start_image(rgb, XP, YP, WP, HP, cx, cy, X, Y, W, H)) {
    return;
  }
  const int sz = rgb->data_w() * rgb->data_h() * rgb->d();
  std::vector<uchar> image_data;
  image_data.reserve(rgb->data_w() * rgb->data_h() * 4);
  switch (rgb->d()) {
    case 4: {
      for (int i = 0; i < sz; i++) {
        image_data.push_back(rgb->array[i]);
      }
      break;
    }
    case 3: {
      for (int i = 0; i < sz; i += 3) {
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i + 1]);
        image_data.push_back(rgb->array[i + 2]);
        image_data.push_back(255);
      }
      break;
    }
    case 2: {
      for (int i = 0; i < sz; i += 2) {
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i + 1]);
      }
      break;
    }
    case 1: {
      for (int i = 0; i < sz; i++) {
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i]);
        image_data.push_back(rgb->array[i]);
        image_data.push_back(255);
      }
      break;
    }
  }
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        let idata = new ImageData(new Uint8ClampedArray(HEAPF64.buffer, $1, $2), $3, $4);
        ctx.save();
        ctx.rect($5, $6, $9, $10);
        ctx.clip();
        ctx.putImageData(idata, $5, $6, $7, $8, $9, $10);
        ctx.restore();
      },
      ctxt, image_data.data(), image_data.size(), rgb->data_w(), rgb->data_h(), XP, YP, cx, cy, WP,
      HP);
}


void Fl_Emscripten_Graphics_Driver::draw_image(Fl_Draw_Image_Cb call, void *data, int ix, int iy,
                                               int iw, int ih, int D) {
  uchar *array = new uchar[iw * D * ih];
  for (int l = 0; l < ih; l++) {
    call(data, 0, l, iw, array + l * D * iw);
    if (D % 2 == 0)
      for (int i = 0; i < iw; i++) {
        *(array + l * D * iw + i * D + D - 1) = 0xff;
      }
  }
  Fl_RGB_Image *rgb = new Fl_RGB_Image(array, iw, ih, D);
  rgb->alloc_array = 1;
  draw_rgb(rgb, ix, iy, iw, ih, 0, 0);
  delete rgb;
}

void Fl_Emscripten_Graphics_Driver::draw_image(const uchar *data, int ix, int iy, int iw, int ih,
                                               int D, int LD) {
  if (abs(D) < 3) { // mono
    draw_image_mono(data, ix, iy, iw, ih, D, LD);
    return;
  }
  struct callback_data cb_data;
  if (!LD)
    LD = iw * abs(D);
  if (D < 0)
    data += iw * abs(D);
  cb_data.data = data;
  cb_data.D = D;
  cb_data.LD = LD;
  Fl_Emscripten_Graphics_Driver::draw_image(draw_image_cb, &cb_data, ix, iy, iw, ih, abs(D));
}

void Fl_Emscripten_Graphics_Driver::draw_image_mono(const uchar *data, int ix, int iy, int iw,
                                                    int ih, int D, int LD) {
  struct callback_data cb_data;
  const size_t aD = abs(D);
  if (!LD)
    LD = iw * aD;
  if (D < 0)
    data += iw * aD;
  cb_data.data = data;
  cb_data.D = D;
  cb_data.LD = LD;
  draw_image(draw_image_cb, &cb_data, ix, iy, iw, ih, abs(D));
}

void Fl_Emscripten_Graphics_Driver::draw_image_mono(Fl_Draw_Image_Cb call, void *data, int ix,
                                                    int iy, int iw, int ih, int D) {
  draw_image(call, data, ix, iy, iw, ih, D);
}

void Fl_Emscripten_Graphics_Driver::draw_fixed(Fl_Bitmap *bm, int XP, int YP, int WP, int HP,
                                               int cx, int cy) {
  uchar R, G, B;
  Fl::get_color(fl_color(), R, G, B);
  uchar *data = new uchar[bm->data_w() * bm->data_h() * 4];
  memset(data, 0, bm->data_w() * bm->data_h() * 4);
  Fl_RGB_Image *rgb = new Fl_RGB_Image(data, bm->data_w(), bm->data_h(), 4);
  rgb->alloc_array = 1;
  int rowBytes = (bm->data_w() + 7) >> 3;
  for (int j = 0; j < bm->data_h(); j++) {
    const uchar *p = bm->array + j * rowBytes;
    for (int i = 0; i < rowBytes; i++) {
      uchar q = *p;
      int last = bm->data_w() - 8 * i;
      if (last > 8)
        last = 8;
      for (int k = 0; k < last; k++) {
        if (q & 1) {
          uchar *r = (uchar *)rgb->array + j * bm->data_w() * 4 + i * 8 * 4 + k * 4;
          *r++ = R;
          *r++ = G;
          *r++ = B;
          *r = ~0;
        }
        q >>= 1;
      }
      p++;
    }
  }
  draw_rgb(rgb, XP, YP, WP, HP, cx, cy);
  delete rgb;
}

void Fl_Emscripten_Graphics_Driver::draw_fixed(Fl_Pixmap *rgb, int XP, int YP, int WP, int HP,
                                               int cx, int cy) {
  Fl_RGB_Image *img = new Fl_RGB_Image(rgb);
  img->alloc_array = 1;
  draw_rgb(img, XP, YP, WP, HP, cx, cy);
  delete img;
}

void Fl_Emscripten_Graphics_Driver::copy_offscreen(int x, int y, int w, int h, Fl_Offscreen pixmap,
                                                   int srcx, int srcy) {
  EM_VAL ctx = (EM_VAL)pixmap;
  EM_ASM(
      {
        let ctxt = Emval.toValue($0);
        let ctx = Emval.toValue($1);
        let cvs = ctx.canvas;
        ctxt.drawImage(cvs, $6, $7, $4, $5, $2, $3, $4, $5);
      },
      ctxt, ctx, x, y, w, h, srcx, srcy);
}

void Fl_Emscripten_Graphics_Driver::ps_translate(int x, int y) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.save();
        ctx.translate($1, $2);
        ctx.save();
      },
      ctxt, x, y);
}

void Fl_Emscripten_Graphics_Driver::ps_untranslate(void) {
  EM_ASM(
      {
        let ctx = Emval.toValue($0);
        ctx.restore();
        ctx.restore();
      },
      ctxt);
}