//
// Implementation of Emscripten Browser Screen interface
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

#include "Fl_Emscripten_Screen_Driver.H"
#include "../Unix/Fl_Unix_System_Driver.H"
#include <emscripten.h>
#include <emscripten/val.h>

static char *fl_selection_buffer[2];
static int fl_selection_length[2];
static const char *fl_selection_type[2];
static int fl_selection_buffer_length[2];
static char fl_i_own_selection[2] = {0, 0};
static struct wl_data_offer *fl_selection_offer = NULL;
static const char *fl_selection_offer_type = NULL;

using namespace emscripten;

extern void fl_fix_focus();

Fl_Emscripten_Screen_Driver::Fl_Emscripten_Screen_Driver()
  : Fl_Screen_Driver() {
  Fl_Display_Device::display_device();
  // we only deal with 1 screen.
  num_screens = 0;
}

void Fl_Emscripten_Screen_Driver::init() {
  open_display();
}

int Fl_Emscripten_Screen_Driver::x() {
  return 0;
}
int Fl_Emscripten_Screen_Driver::y() {
  return 0;
}
int Fl_Emscripten_Screen_Driver::w() {
  val screen = val::global("screen");
  return screen["availWidth"].as<int>();
}
int Fl_Emscripten_Screen_Driver::h() {
  val screen = val::global("screen");
  return screen["availHeight"].as<int>();
}


void Fl_Emscripten_Screen_Driver::screen_work_area(int &X, int &Y, int &W, int &H, int n) {
  if (num_screens < 0)
    init();
  if (n < 0 || n >= num_screens)
    n = 0;
  if (n == 0) { // for the main screen, these return the work area
    X = Fl::x();
    Y = Fl::y();
    W = Fl::w();
    H = Fl::h();
  } else { // for other screens, work area is full screen,
    screen_xywh(X, Y, W, H, n);
  }
}


void Fl_Emscripten_Screen_Driver::screen_xywh(int &X, int &Y, int &W, int &H, int n) {
  if (num_screens < 0)
    init();

  if ((n < 0) || (n >= num_screens))
    n = 0;

  if (num_screens > 0) {
    float s = 1;
    X = 0;
    Y = 0;
    W = w();
    H = h();
  }
}

int Fl_Emscripten_Screen_Driver::get_mouse(int &x, int &y) {
  x = Fl::e_x_root;
  y = Fl::e_y_root;
  return 0;
}

int Fl_Emscripten_Screen_Driver::compose(int &del) {
  int condition;
  unsigned char ascii = (unsigned char)Fl::e_text[0];
  condition = (Fl::e_state & (FL_ALT | FL_META | FL_CTRL)) && !(ascii & 128);
  if (condition) {
    del = 0;
    return 0;
  } // this stuff is to be treated as a function key
  del = Fl::compose_state;
  Fl::compose_state = 0;
  // Only insert non-control characters:
  if ((!Fl::compose_state) && !(ascii & ~31 && ascii != 127)) {
    return 0;
  }
  return 1;
}

// This is possible with the clipboard access api which is supported by most modern browsers
void Fl_Emscripten_Screen_Driver::copy(const char *stuff, int len, int clipboard,
                                       const char *type) {
  if (!stuff || len < 0)
    return;

  if (clipboard >= 2)
    clipboard = 1; // Only on X11 do multiple clipboards make sense.

  if (len + 1 > fl_selection_buffer_length[clipboard]) {
    delete[] fl_selection_buffer[clipboard];
    fl_selection_buffer[clipboard] = new char[len + 100];
    fl_selection_buffer_length[clipboard] = len + 100;
  }
  memcpy(fl_selection_buffer[clipboard], stuff, len);
  fl_selection_buffer[clipboard][len] = 0; // needed for direct paste
  fl_selection_length[clipboard] = len;
  fl_i_own_selection[clipboard] = 1;
  fl_selection_type[clipboard] = Fl::clipboard_plain_text;
  if (clipboard == 1) {
    // clang-format off
    EM_ASM({
      navigator.clipboard.writeText(UTF8ToString($0)).then(
        () => {
          /* clipboard successfully set */
        },
        () => {
          /* clipboard write failed */
        },
      );
    }, fl_selection_buffer[1]);
    // clang-format on
  }
}

// Here we get stored images from the clilpboard, blit them on an offscreen canvas and get them as RGB images.
// It's more convenient than returning a PNG image and having to depend on Fl_PNG_Image for core code.
// clang-format off
EM_ASYNC_JS(EM_VAL, get_clipboard_image, (), {
  const itemList = await navigator.clipboard.read();
  let imageType;
  
  // Find the first item that is an image
  const item = itemList.find(item => item.types.some(type => type.startsWith('image/')));

  if (item) {
    // Get the blob of the image
    const imageBlob = await item.getType(imageType = item.types.find(type => type.startsWith('image/')));
    
    // Create an ImageBitmap from the blob
    const imageBitmap = await createImageBitmap(imageBlob);
    
    // Create a canvas to draw the image onto
    const canvas = new OffscreenCanvas(imageBitmap.width, imageBitmap.height);
    const context = canvas.getContext('2d');
    
    // Draw the image onto the canvas
    context.drawImage(imageBitmap, 0, 0);

    // Extract the ImageData from the canvas
    const imageData = context.getImageData(0, 0, canvas.width, canvas.height);
    return imageData;
  } else {
    return null;
  }
});
// clang-format on

// This is possible with the clipboard access api which is supported by most modern browsers
void Fl_Emscripten_Screen_Driver::paste(Fl_Widget &receiver, int clipboard, const char *type) {
  if (type[0] == 0)
    type = Fl::clipboard_plain_text;
  if (clipboard) {
    Fl::e_clipboard_type = "";
    if (strcmp(type, Fl::clipboard_plain_text) == 0) {
      val navigator = val::global("navigator");
      std::string clipText =
          navigator["clipboard"].call<val>("readText").await().as<std::string>();
      size_t len = clipText.size();
      Fl::e_text = clipText.data();
      Fl::e_length = len;
      fl_selection_buffer[clipboard][len - 1] = 0;
      fl_selection_length[1] = len;
      Fl::e_clipboard_type = Fl::clipboard_plain_text;
      receiver.handle(FL_PASTE);
      Fl::e_text = 0;
      return;
    } else if (strcmp(type, Fl::clipboard_image) == 0) {
      val imagedata = val::take_ownership(get_clipboard_image()).await();
      int width = imagedata["width"].as<int>();
      int height = imagedata["height"].as<int>();
      const std::vector<uchar> vec = vecFromJSArray<uchar>(imagedata);
      Fl_RGB_Image *image = new Fl_RGB_Image(vec.data(), width, height, 4);
      image->alloc_array = 1;
      Fl::e_clipboard_data = image;
      Fl::e_clipboard_type = Fl::clipboard_image;
      if (Fl::e_clipboard_data) {
        int done = receiver.handle(FL_PASTE);
        Fl::e_clipboard_type = "";
        if (done == 0) {
          delete (Fl_Image *)Fl::e_clipboard_data;
          Fl::e_clipboard_data = NULL;
        }
      }
      return;
    } else
      fl_selection_length[1] = 0;
  }
}

void Fl_Emscripten_Screen_Driver::grab(Fl_Window *win) {
  if (win) {
    if (!Fl::grab()) {
    }
    Fl::grab_ = win;
  } else {
    if (Fl::grab()) {
      Fl::grab_ = 0;
      fl_fix_focus();
    }
  }
}