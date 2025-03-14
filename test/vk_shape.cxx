//
// Tiny Vulkan demo program for the Fast Light Tool Kit (FLTK).
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
#include <FL/platform.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/math.h>

#if HAVE_VK

#include <FL/Fl_Vk_Window.H>

class vk_shape_window : public Fl_Vk_Window {
    void draw() FL_OVERRIDE;
public:
    int sides;
    vk_shape_window(int x,int y,int w,int h,const char *l=0);
    vk_shape_window(int w,int h,const char *l=0);

};

vk_shape_window::vk_shape_window(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA);
  sides = 3;
}

vk_shape_window::vk_shape_window(int w,int h,const char *l) :
Fl_Vk_Window(w,h,l) {
  sides = 3;
}

void vk_shape_window::draw() {
    if (!shown() || w() <= 0 || h() <= 0) return;
    printf("Fl_Vk_Window::draw called\n");

    // Background color
    m_clearColor = { 0.0, 0.0, 1.0, 1.0 };
    
    draw_begin();

    // Draw the triangle
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(m_draw_cmd, VERTEX_BUFFER_BIND_ID, 1,
                           &m_vertices.buf, offsets);

    vkCmdDraw(m_draw_cmd, 3, 1, 0, 0);

    Fl_Window::draw();
    
    draw_end();
}

#else

#include <FL/Fl_Box.H>
class vk_shape_window : public Fl_Box {
public:
  int sides;
  vk_shape_window(int x,int y,int w,int h,const char *l=0)
    :Fl_Box(FL_DOWN_BOX,x,y,w,h,l){
      label("This demo does\nnot work without Vulkan");
  }
};

#endif

// when you change the data, as in this callback, you must call redraw():
void sides_cb(Fl_Widget *o, void *p) {
  vk_shape_window *sw = (vk_shape_window *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->redraw();
}

int main(int argc, char **argv) {
    Fl::use_high_res_VK(1);

#ifdef _WIN32
    char* term = fl_getenv("TERM");
    if (!term || strlen(term) == 0)
    {
        BOOL ok = AttachConsole(ATTACH_PARENT_PROCESS);
        if (ok)
        {
            freopen("conout$", "w", stdout);
            freopen("conout$", "w", stderr);
        }
    }
#endif
  
#if 1
    Fl_Window window(300, 330);
  
// the shape window could be it's own window, but here we make it
// a child window:
        
    vk_shape_window sw(10, 10, 280, 280);

// // make it resize:
    //  window.size_range(300,330,0,0,1,1,1);
// add a knob to control it:
    Fl_Hor_Slider slider(50, 295, window.w()-60, 30, "Sides:");
    slider.align(FL_ALIGN_LEFT);
    slider.step(1);
    slider.bounds(3,40);

    window.resizable(&sw);
    slider.value(sw.sides);
    slider.callback(sides_cb,&sw);
    window.end();
    window.show(argc,argv);
#else
    vk_shape_window sw(300, 330, "VK Window");
    sw.resizable(&sw);
    sw.show();
#endif
    return Fl::run();
}
