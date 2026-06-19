File src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx, function makeWindow:

`````````````````````````````````````````````````````````````````````````````
    checkSubwindowFrame(); // make sure subwindow doesn't leak outside parent
    // \@note: \@bug: NOT on FLTK main branch.  Must keep for fixing bug #1307
    if (can_expand_outside_parent_) parent->covered = true;
  } else { // a window without decoration
`````````````````````````````````````````````````````````````````````````````

=======================================================
File src/drivers/Wayland/Fl_Wayland_Screen_Driver.cxx, function wl_keyboard_key:

`````````````````````````````````````````````````````````````````````````````
  // \@note: \bug: !(sym >= FL_F && sym <= FL_F_Last) NOT ON FLTK main branch.  Fixes repetition of keys
  //               on mrv2
  if (event == FL_KEYDOWN && status == XKB_COMPOSE_NOTHING &&
      !(sym >= FL_Shift_L && sym <= FL_Alt_R) &&
      !(sym >= FL_F && sym <= FL_F_Last)) {
`````````````````````````````````````````````````````````````````````````````

======================================================

File src/Fl_Widget.cxx, function ~Fl_Widget


Fl_Widget::~Fl_Widget() {
// No Fl::Pen::unsubscribe(this);  // <-- crashes Windows ARM64 and AMD64 Debug
                                   //     builds.
}
