File src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx, function makeWindow:

`````````````````````````````````````````````````````````````````````````````
    checkSubwindowFrame(); // make sure subwindow doesn't leak outside parent
    // \@note: \@bug: NOT on FLTK main branch.  Must keep for fixing bug #1307
    if (can_expand_outside_parent_) parent->covered = true;
  } else { // a window without decoration
`````````````````````````````````````````````````````````````````````````````
