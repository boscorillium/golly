                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

                        / ***/
#ifndef _WXSTATUS_H_
#define _WXSTATUS_H_

// Define a child window for status bar at top of main frame:

class StatusBar : public wxWindow
{
public:
    StatusBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~StatusBar();

   // erase 2nd line of status bar
   void ClearMessage();

   // display message on 2nd line of status bar
   void DisplayMessage(const char *s);

   // beep and display message on 2nd line of status bar
   void ErrorMessage(const char *s);

   // set message string without displaying it (until next update)
   void SetMessage(const char *s);

   // XY location needs to be updated
   void UpdateXYLocation();

   // check location of mouse and update XY location if necessary
   void CheckMouseLocation(bool active);

   // convert given number to string suitable for display
   const char* Stringify(double d);
   const char* Stringify(const bigint &b);
   
   // return current delay (in millisecs)
   int GetCurrentDelay();

   wxFont* GetStatusFont() { return statusfont; }
   int GetTextAscent() { return textascent; }

   // status bar height (STATUS_HT if visible, 0 if not visible)
   int statusht;
   
private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnEraseBackground(wxEraseEvent& event);

   bool ClickInScaleBox(int x, int y);
   bool ClickInStepBox(int x, int y);
   void SetStatusFont(wxDC &dc);
   void DisplayText(wxDC &dc, const char *s, wxCoord x, wxCoord y);
   void DrawStatusBar(wxDC &dc, wxRect &updaterect);

   #ifndef __WXMAC__
      wxBitmap *statbitmap;      // status bar bitmap
      int statbitmapwd;          // width of status bar bitmap
      int statbitmapht;          // height of status bar bitmap
   #endif
   
   int h_gen;                    // horizontal position of "Generation"
   int h_pop;                    // horizontal position of "Population"
   int h_scale;                  // horizontal position of "Scale"
   int h_step;                   // horizontal position of "Step"
   int h_xy;                     // horizontal position of "XY"
   int textascent;               // vertical adjustment used in DrawText calls
   char statusmsg[256];          // for messages on 2nd line
   bigint currx, curry;          // cursor location in cell coords
   bool showxy;                  // show cursor's XY location?
   wxFont *statusfont;           // status bar font
   
   wxBrush *brush_qlife;         // for background if not hashing
   wxBrush *brush_hlife;         // for background if hashing
};

const int STATUS_HT = 32;        // status bar height (enough for 2 lines)

#endif