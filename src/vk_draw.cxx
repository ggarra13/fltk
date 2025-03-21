//
// OpenGL text drawing support routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
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

// Functions from <FL/gl.h>
// See also Fl_Vk_Window and vk_start.cxx

/* Note about implementing VK text support for a new platform

 1) if the VK_EXT_texture_rectangle (a.k.a. GL_ARB_texture_rectangle) GL extension
 is available, no platform-specific code is needed, besides support for fl_draw() and Fl_Image_Surface for the platform.

 2) if the GL_EXT_texture_rectangle GL extension is not available,
 a rudimentary support through GLUT is obtained without any platform-specific code.

 3) A more elaborate support can be obtained implementing
 get_list(), vk_bitmap_font() and draw_string_legacy() for the platform's Fl_XXX_Vk_Window_Driver.
 */

#include <config.h>

#if HAVE_VK || defined(FL_DOXYGEN)

#include <FL/Fl.H>
#include <FL/vk.h>
#include <FL/vk_draw.H>
#include <FL/fl_draw.H>
#include <FL/math.h> // for ceil()
#include "Fl_Vk_Window_Driver.H"
#include <FL/Fl_Image_Surface.H>
#include <stdlib.h>



/** Returns the current font's height */
int   vk_height() {return fl_height();}
/** Returns the current font's descent */
int   vk_descent() {return fl_descent();}
/** Returns the width of the string in the current fnt */
double vk_width(const char* s) {return fl_width(s);}
/** Returns the width of n characters of the string in the current font */
double vk_width(const char* s, int n) {return fl_width(s,n);}
/** Returns the width of the character in the current font */
double vk_width(uchar c) {return fl_width(c);}

static Fl_Font_Descriptor *vk_fontsize;
static int has_texture_rectangle = 0;

extern float vk_start_scale; // in vk_start.cxx

/**
  Sets the current Vulkan font to the same font as calling fl_font().
 \see Fl::draw_GL_text_with_textures(int val)
  */
void  vk_font(int fontid, int size) {
  static bool once = true;
  if (once) {
    once = false;
    has_texture_rectangle = true;
  }
  fl_font(fontid, size);
  Fl_Font_Descriptor *fl_fontsize = fl_graphics_driver->font_descriptor();
  vk_fontsize = fl_fontsize;
}

void vk_remove_displaylist_fonts()
{
  // clear variables used mostly in fl_font
  fl_graphics_driver->font(0, 0);

  for (int j = 0 ; j < FL_FREE_FONT ; ++j)
  {
    Fl_Font_Descriptor *prevDesc = 0L, *nextDesc = 0L;
    Fl_Font_Descriptor *&firstDesc = *Fl_Vk_Window_Driver::global()->fontnum_to_fontdescriptor(j);
    for (Fl_Font_Descriptor *desc = firstDesc; desc; desc = nextDesc)
    {
      nextDesc = desc->next;
      if(desc->listbase) {
        // remove descriptor from a single-linked list
        if(desc == firstDesc) {
          firstDesc = desc->next;
        } else if (prevDesc) {
          // prevDesc should not be NULL, but this test will make static analysis shut up
          prevDesc->next = desc->next;
        }
        // It would be nice if this next line was in a destructor somewhere
        // glDeleteLists(desc->listbase, Fl_Vk_Window_Driver::global()->genlistsize());
        delete desc;
      } else {
        prevDesc = desc;
      }
    }
  }
}


/**
  Draws an array of n characters of the string in the current font at the current position.
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str, int n) {
  if (n > 0) {
    if (has_texture_rectangle)  Fl_Vk_Window_Driver::draw_string_with_texture(str, n);
  }
}


/**
  Draws n characters of the string in the current font at the given position
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str, int n, int x, int y) {
  // glRasterPos2i(x, y);
  vk_draw(str, n);
}

/**
  Draws n characters of the string in the current font at the given position
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str, int n, float x, float y) {
  // glRasterPos2f(x, y);
  vk_draw(str, n);
}

/**
  Draws a nul-terminated string in the current font at the current position
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str) {
  vk_draw(str, (int)strlen(str));
}

/**
  Draws a nul-terminated string in the current font at the given position
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str, int x, int y) {
  vk_draw(str, (int)strlen(str), x, y);
}

/**
  Draws a nul-terminated string in the current font at the given position
 \see  vk_texture_pile_height(int)
  */
void vk_draw(const char* str, float x, float y) {
  vk_draw(str, (int)strlen(str), x, y);
}

static void vk_draw_invert(const char* str, int n, int x, int y) {
  // glRasterPos2i(x, -y);
  vk_draw(str, n);
}

/**
  Draws a string formatted into a box, with newlines and tabs expanded,
  other control characters changed to ^X. and aligned with the edges or
  center. Exactly the same output as fl_draw().
  */
void vk_draw(
  const char* str,      // the (multi-line) string
  int x, int y, int w, int h,   // bounding box
  Fl_Align align)
{
  fl_draw(str, x, -y-h, w, h, align, vk_draw_invert, NULL, 0);
}

/** Measure how wide and tall the string will be when drawn by the vk_draw() function */
void vk_measure(const char* str, int& x, int& y) {
  fl_measure(str,x,y,0);
}

/**
  Outlines the given rectangle with the current color.
  If Fl_Vk_Window::ortho() has been called, then the rectangle will
  exactly fill the given pixel rectangle.
  */
void vk_rect(int x, int y, int w, int h) {
  if (w < 0) {w = -w; x = x-w;}
  if (h < 0) {h = -h; y = y-h;}
}

/**
  Fills the given rectangle with the current color.
  \see vk_rect(int x, int y, int w, int h)
  */
void vk_rectf(int x,int y,int w,int h) {
  // glRecti(x,y,x+w,y+h);
}

void vk_draw_image(const uchar* b, int x, int y, int w, int h, int d, int ld) {
  if (!ld) ld = w*d;
  // GLint row_length;
  // glGetIntegerv(VK_UNPACK_ROW_LENGTH, &row_length); // get current row length
  // glPixelStorei(VK_UNPACK_ROW_LENGTH, ld/d);
  // glRasterPos2i(x,y);
  // glDrawPixels(w,h,d<4?VK_RGB:VK_RGBA,VK_UNSIGNED_BYTE,(const ulong*)b);
  // glPixelStorei(VK_UNPACK_ROW_LENGTH, row_length); // restore row length
}


/**
  Sets the curent Vulkan color to an FLTK color.

  For color-index modes it will use fl_xpixel(c), which is only
  right if the window uses the default colormap!
  */
void vk_color(Fl_Color i) {
    //if (Fl_Vk_Window_Driver::global()->overlay_color(i)) return;
  uchar red, green, blue;
  Fl::get_color(i, red, green, blue);
  // glColor3ub(red, green, blue);
}


#if ! defined(FL_DOXYGEN) // do not want too much of the vk_texture_fifo internals in the documentation

/* Implement the vk_texture_fifo mechanism:
 Strings to be drawn are memorized in a fifo pile (which max size can
 be increased calling vk_texture_pile_height(int)).
 Each pile element contains the string, the font, the GUI scale, and
 an image of the string in the form of a GL texture.
 Strings are drawn in 2 steps:
    1) compute the texture for that string if it was not computed before;
    2) draw the texture using the current GL color.
*/

// manages a fifo pile of pre-computed string textures
class vk_texture_fifo {
  friend class Fl_Vk_Window_Driver;
private:
  typedef struct { // information for a pre-computed texture
      //Vkuint texName; // its name
      char *utf8; //its text
      Fl_Font_Descriptor *fdesc; // its font
      float scale; // scaling factor of the GUI
      int str_len; // the length of the utf8 text
  } data;
  data *fifo; // array of pile elements
  int size_; // pile height
  int current; // the oldest texture to have entered the pile
  int last; // pile top
  int textures_generated; // true after glGenTextures has been called
  void display_texture(int rank);
  int compute_texture(const char* str, int n);
  int already_known(const char *str, int n);
public:
  vk_texture_fifo(int max = 100); // 100 = default height of texture pile
  inline int size(void) {return size_; }
  ~vk_texture_fifo(void);
};

vk_texture_fifo::vk_texture_fifo(int max)
{
  size_ = max;
  last = current = -1;
  textures_generated = 0;
  fifo = (data*)calloc(size_, sizeof(data));
}

vk_texture_fifo::~vk_texture_fifo()
{
  for (int i = 0; i < size_; i++) {
    if (fifo[i].utf8) free(fifo[i].utf8);
    // if (textures_generated) glDeleteTextures(1, &fifo[i].texName);
  }
  free(fifo);
}

// returns rank of pre-computed texture for a string if it exists
int vk_texture_fifo::already_known(const char *str, int n)
{
  int rank;
  for ( rank = 0; rank <= last; rank++) {
    if ((fifo[rank].str_len == n) &&
        (fifo[rank].fdesc == vk_fontsize) &&
        (fifo[rank].scale == Fl_Vk_Window_Driver::vk_scale) &&
        (memcmp(str, fifo[rank].utf8, n) == 0)) {
      return rank;
    }
  }
  return -1; // means no texture exists yet for that string
}

static vk_texture_fifo *vk_fifo = NULL; // points to the texture pile class instance


// Cross-platform implementation of the texture mechanism for text rendering
// using textures with the alpha channel only.

// displays a pre-computed texture on the GL scene
void vk_texture_fifo::display_texture(int rank)
{
//   // VK_TRANSFORM_BIT for VK_PROJECTION and VK_MODELVIEW
//   // VK_ENABLE_BIT for VK_DEPTH_TEST, VK_LIGHTING
//   // VK_TEXTURE_BIT for VK_TEXTURE_RECTANGLE_ARB
//   // VK_COLOR_BUFFER_BIT for VK_BLEND and glBlendFunc,
//   glPushAttrib(VK_TRANSFORM_BIT | VK_ENABLE_BIT | VK_TEXTURE_BIT | VK_COLOR_BUFFER_BIT);
//   //setup matrices
//   glMatrixMode (VK_PROJECTION);
//   glPushMatrix();
//   glLoadIdentity ();
//   glMatrixMode (VK_MODELVIEW);
//   glPushMatrix();
//   glLoadIdentity ();
//   float winw = Fl_Vk_Window_Driver::vk_scale * Fl_Window::current()->w();
//   float winh = Fl_Vk_Window_Driver::vk_scale * Fl_Window::current()->h();
//   glDisable (VK_DEPTH_TEST); // ensure text is not removed by depth buffer test.
//   glEnable (VK_BLEND); // for text fading
//   glBlendFunc(VK_SRC_ALPHA, VK_ONE_MINUS_SRC_ALPHA);
//   glDisable(VK_LIGHTING);
//   GLfloat pos[4];
//   glGetFloatv(VK_CURRENT_RASTER_POSITION, pos);
//   if (vk_start_scale != 1) { // using vk_start() / vk_finish()
//     pos[0] /= vk_start_scale;
//     pos[1] /= vk_start_scale;
//   }

//   float R = 2;
//   glScalef (R/winw, R/winh, 1.0f);
//   glTranslatef (-winw/R, -winh/R, 0.0f);
//   glEnable (VK_TEXTURE_RECTANGLE_ARB);
//   glBindTexture (VK_TEXTURE_RECTANGLE_ARB, fifo[rank].texName);
//   GLint width, height;
//   glGetTexLevelParameteriv(VK_TEXTURE_RECTANGLE_ARB, 0, VK_TEXTURE_WIDTH, &width);
//   glGetTexLevelParameteriv(VK_TEXTURE_RECTANGLE_ARB, 0, VK_TEXTURE_HEIGHT, &height);
//   //write the texture on screen
//   glBegin (VK_QUADS);
//   float ox = pos[0];
//   float oy = pos[1] + height - Fl_Vk_Window_Driver::vk_scale * fl_descent();
//   glTexCoord2f (0.0f, 0.0f); // draw lower left in world coordinates
//   glVertex2f (ox, oy);
//   glTexCoord2f (0.0f, (GLfloat)height); // draw upper left in world coordinates
//   glVertex2f (ox, oy - height);
//   glTexCoord2f ((GLfloat)width, (GLfloat)height); // draw upper right in world coordinates
//   glVertex2f (ox + width, oy - height);
//   glTexCoord2f ((GLfloat)width, 0.0f); // draw lower right in world coordinates
//   glVertex2f (ox + width, oy);
//   glEnd ();

//   // reset original matrices
//   glPopMatrix(); // VK_MODELVIEW
//   glMatrixMode (VK_PROJECTION);
//   glPopMatrix();
//   glPopAttrib(); // VK_TRANSFORM_BIT | VK_ENABLE_BIT | VK_TEXTURE_BIT | VK_COLOR_BUFFER_BIT
// #if HAVE_VK_GLU_H
//   //set the raster position to end of string
//   pos[0] += width;
//   GLdouble modelmat[16];
//   glGetDoublev (VK_MODELVIEW_MATRIX, modelmat);
//   GLdouble projmat[16];
//   glGetDoublev (VK_PROJECTION_MATRIX, projmat);
//   GLdouble objX, objY, objZ;
//   GLint viewport[4];
//   glGetIntegerv (VK_VIEWPORT, viewport);
//   gluUnProject(pos[0], pos[1], pos[2], modelmat, projmat, viewport, &objX, &objY, &objZ);

//   if (vk_start_scale != 1) { // using vk_start() / vk_finish()
//     objX *= vk_start_scale;
//     objY *= vk_start_scale;
//   }
//   glRasterPos2d(objX, objY);
//#endif // HAVE_VK_GLU_H
} // display_texture


// pre-computes a string texture
int vk_texture_fifo::compute_texture(const char* str, int n)
{
  current = (current + 1) % size_;
  if (current > last) last = current;
  if ( fifo[current].utf8 ) free(fifo[current].utf8);
  fifo[current].utf8 = (char *)malloc(n + 1);
  memcpy(fifo[current].utf8, str, n);
  fifo[current].utf8[n] = 0;
  fifo[current].str_len = n; // record length of text in utf8
  Fl_Fontsize fs = fl_size();
  float s = fl_graphics_driver->scale();
  fl_graphics_driver->Fl_Graphics_Driver::scale(1); // temporarily remove scaling factor
  fl_font(fl_font(), int(fs * Fl_Vk_Window_Driver::vk_scale)); // the font size to use in the GL scene
  int w = (int)ceil( fl_width(fifo[current].utf8, n) );
  w = ((w + 3) / 4) * 4; // make w a multiple of 4
  int h = fl_height();
  fl_graphics_driver->Fl_Graphics_Driver::scale(s); // re-install scaling factor
  fl_font(fl_font(), fs);
  fs = int(fs * Fl_Vk_Window_Driver::vk_scale);
  fifo[current].scale = Fl_Vk_Window_Driver::vk_scale;
  fifo[current].fdesc = vk_fontsize;
  char *alpha_buf = Fl_Vk_Window_Driver::global()->alpha_mask_for_string(str, n, w, h, fs);

  // // save GL parameters VK_UNPACK_ROW_LENGTH and VK_UNPACK_ALIGNMENT
  // GLint row_length, alignment;
  // glGetIntegerv(VK_UNPACK_ROW_LENGTH, &row_length);
  // glGetIntegerv(VK_UNPACK_ALIGNMENT, &alignment);

  // // put the bitmap in an alpha-component-only texture
  // glPushAttrib(VK_TEXTURE_BIT);
  // glBindTexture (VK_TEXTURE_RECTANGLE_ARB, fifo[current].texName);
  // glTexParameteri(VK_TEXTURE_RECTANGLE_ARB, VK_TEXTURE_MIN_FILTER, VK_LINEAR);
  // glPixelStorei(VK_UNPACK_ROW_LENGTH, w);
  // glPixelStorei(VK_UNPACK_ALIGNMENT, 4);
  // // VK_ALPHA8 is defined in GL/gl.h of X11 and of MinGW32 and of MinGW64 and of Vulkan.framework for MacOS
  // glTexImage2D(VK_TEXTURE_RECTANGLE_ARB, 0, VK_ALPHA8, w, h, 0, VK_ALPHA, VK_UNSIGNED_BYTE, alpha_buf);
  // /* For the record: texture construction if an alpha-only-texture is not possible
  //  glTexImage2D(VK_TEXTURE_RECTANGLE_ARB, 0, VK_RGBA8, w, h, 0, VK_BGRA_EXT, VK_UNSIGNED_INT_8_8_8_8_REV, rgba_buf);
  //  and also, replace VK_SRC_ALPHA by VK_ONE in glBlendFunc() call of display_texture()
  //  */
  delete[] alpha_buf; // free the buffer now we have copied it into the GL texture
  // glPopAttrib();
  // // restore saved GL parameters
  // glPixelStorei(VK_UNPACK_ROW_LENGTH, row_length);
  // glPixelStorei(VK_UNPACK_ALIGNMENT, alignment);
  return current;
}

#endif  // ! defined(FL_DOXYGEN)

/**
 Returns the current maximum height of the pile of pre-computed string textures.
 The default value is 100
 \see Fl::draw_VK_text_with_textures(int)
 */
int vk_texture_pile_height(void)
{
  if (! vk_fifo) vk_fifo = new vk_texture_fifo();
  return vk_fifo->size();
}

/** To call after VK operations that may invalidate textures used to draw text in VK scenes
 (e.g., switch between FL_DOUBLE / FL_SINGLE modes).
 */
void vk_texture_reset()
{
  if (vk_fifo) vk_texture_pile_height(vk_texture_pile_height());
}


/**
 Changes the maximum height of the pile of pre-computed string textures

 Strings that are often re-displayed can be processed much faster if
 this pile is set high enough to hold all of them.
 \param max Maximum height of the texture pile
 \see Fl::draw_VK_text_with_textures(int)
*/
void vk_texture_pile_height(int max)
{
  if (vk_fifo) delete vk_fifo;
  vk_fifo = new vk_texture_fifo(max);
}


/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */

/** draws a utf8 string using a Vulkan texture */
void Fl_Vk_Window_Driver::draw_string_with_texture(const char* str, int n)
{
  // Check if the raster pos is valid.
  // If it's not, then drawing it results in undefined behaviour (#1006, #1007).
  // VKint valid;
  // vkGetIntegerv(VK_CURRENT_RASTER_POSITION_VALID, &valid);
  // if (!valid) return;
  Fl_Vk_Window *gwin = Fl_Window::current()->as_vk_window();
  vk_scale = (gwin ? gwin->pixels_per_unit() : 1);
  if (!vk_fifo) vk_fifo = new vk_texture_fifo();
  if (!vk_fifo->textures_generated) {
      // \@todo:
    // if (has_texture_rectangle) for (int i = 0; i < vk_fifo->size_; i++) vkGenTextures(1, &(vk_fifo->fifo[i].texName));
    vk_fifo->textures_generated = 1;
  }
  int index = vk_fifo->already_known(str, n);
  if (index == -1) {
    index = vk_fifo->compute_texture(str, n);
  }
  vk_fifo->display_texture(index);
}


char *Fl_Vk_Window_Driver::alpha_mask_for_string(const char *str, int n, int w, int h, Fl_Fontsize fs)
{
  // write str to a bitmap that is just big enough
  // create an Fl_Image_Surface object
  Fl_Image_Surface *image_surface = new Fl_Image_Surface(w, h);
  Fl_Font fnt = fl_font(); // get the current font
  // direct all further graphics requests to the image
  Fl_Surface_Device::push_current(image_surface);
  // fill the background with black, which we will interpret as transparent
  fl_color(0,0,0);
  fl_rectf(0, 0, w, h);
  // set up the text colour as white, which we will interpret as opaque
  fl_color(255,255,255);
  // Fix the font scaling
  fl_font (fnt, fs); // resize "fltk" font to current GL view scaling
  int desc = fl_descent();
  // Render the text to the buffer
  fl_draw(str, n, 0, h - desc);
  // get the resulting image
  Fl_RGB_Image* image = image_surface->image();
  // direct graphics requests back to previous state
  Fl_Surface_Device::pop_current();
  delete image_surface;
  // This gives us an RGB rendering of the text. We build an alpha channel from that.
  char *alpha_buf = new char [w * h];
  for (int idx = 0; idx < w * h; ++idx)
  { // Fake up the alpha component using the green component's value
    alpha_buf[idx] = image->array[idx * 3 + 1];
  }
  delete image;
  return alpha_buf;
}



/**
 \}
 \endcond
 */

#endif // HAVE_VK || defined(FL_DOXYGEN)
