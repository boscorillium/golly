                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/file.h"       // for wxFile
#include "wx/filename.h"   // for wxFileName
#include "wx/menuitem.h"   // for SetText
#include "wx/clipbrd.h"    // for wxTheClipboard
#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/image.h"      // for wxImage

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"  // for writepattern, pattern_format

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for SavePrefs, etc
#include "wxrule.h"        // for GetRuleName
#include "wxinfo.h"        // for GetInfoFrame
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for SetSelectionColor
#include "wxscript.h"      // for IsScript, RunScript, inscript
#include "wxmain.h"        // for MainFrame, etc

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>                      // for OpaqueWindowPtr, etc
   #include "wx/mac/corefoundation/cfstring.h"     // for wxMacCFStringHolder
#endif

// File menu functions:

// -----------------------------------------------------------------------------

void MainFrame::MySetTitle(const wxString& title)
{
   #ifdef __WXMAC__
      // avoid wxMac's SetTitle call -- it causes an undesirable window refresh
      SetWindowTitleWithCFString((OpaqueWindowPtr*)this->MacGetWindowRef(),
                                 wxMacCFStringHolder(title, wxFONTENCODING_DEFAULT));
   #else
      SetTitle(title);
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::SetWindowTitle(const wxString& filename)
{
   wxString wtitle;
   if ( !filename.IsEmpty() ) {
      // remember current file name
      currname = filename;
   }
   wxString rule = GetRuleName( wxString(curralgo->getrule(),wxConvLocal) );
   #ifdef __WXMAC__
      wtitle.Printf(_("%s [%s]"), currname.c_str(), rule.c_str());
   #else
      wtitle.Printf(_("%s [%s] - Golly"), currname.c_str(), rule.c_str());
   #endif
   // better to truncate a really long title???!!!
   MySetTitle(wtitle);
}

// -----------------------------------------------------------------------------

void MainFrame::SetGenIncrement()
{
   if (warp > 0) {
      bigint inc = 1;
      // WARNING: if this code changes then also change StatusBar::DrawStatusBar
      if (hashing) {
         // set inc to hbasestep^warp
         int i = warp;
         while (i > 0) { inc.mul_smallint(hbasestep); i--; }
      } else {
         // set inc to qbasestep^warp
         int i = warp;
         while (i > 0) { inc.mul_smallint(qbasestep); i--; }
      }
      curralgo->setIncrement(inc);
   } else {
      curralgo->setIncrement(1);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::CreateUniverse()
{
   // first delete old universe if it exists
   if (curralgo) delete curralgo;

   if (hashing) {
      curralgo = new hlifealgo();
      curralgo->setMaxMemory(maxhashmem);
   } else {
      curralgo = new qlifealgo();
   }

   // curralgo->step() will call wx_poll::checkevents()
   curralgo->setpoll(wxGetApp().Poller());

   // increment has been reset to 1 but that's probably not always desirable
   // so set increment using current warp value
   SetGenIncrement();
}

// -----------------------------------------------------------------------------

const wxString B0message = _("Hashing has been turned off due to B0-not-S8 rule.");

void MainFrame::NewPattern(const wxString& title)
{
   if (generating) return;
   savestart = false;
   currfile.Clear();
   startgen = 0;
   warp = 0;
   CreateUniverse();

   if (initrule[0]) {
      // this is the first call of NewPattern when app starts
      const char *err = curralgo->setrule(initrule);
      if (err) Warning(wxString(err,wxConvLocal));
      if (global_liferules.hasB0notS8 && hashing) {
         hashing = false;
         statusptr->SetMessage(B0message);
         CreateUniverse();
      }
      initrule[0] = 0;        // don't use it again
   }

   if (newremovesel) viewptr->NoSelection();
   if (newcurs) currcurs = newcurs;
   viewptr->SetPosMag(bigint::zero, bigint::zero, newmag);

   // best to restore true origin
   if (viewptr->originx != bigint::zero || viewptr->originy != bigint::zero) {
      viewptr->originy = 0;
      viewptr->originx = 0;
      statusptr->SetMessage(origin_restored);
   }

   // window title will also show curralgo->getrule()
   SetWindowTitle(title);

   UpdateEverything();
}

// -----------------------------------------------------------------------------

bool MainFrame::LoadImage()
{
   wxString ext = currfile.AfterLast(wxT('.'));
   
   // supported extensions match image handlers added in GollyApp::OnInit()
   if ( ext.IsSameAs(wxT("bmp"),false) ||
        ext.IsSameAs(wxT("gif"),false) ||
        ext.IsSameAs(wxT("png"),false) ||
        ext.IsSameAs(wxT("tif"),false) ||
        ext.IsSameAs(wxT("tiff"),false) ) {
      wxImage image;
      if ( image.LoadFile(currfile) ) {
         curralgo->setrule("B3/S23");
         
         unsigned char maskr, maskg, maskb;
         bool hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
         int wd = image.GetWidth();
         int ht = image.GetHeight();
         unsigned char *idata = image.GetData();
         int x, y;
         for (y = 0; y < ht; y++)
            for (x = 0; x < wd; x++) {
               long pos = (y * wd + x) * 3;
               unsigned char r = idata[pos];
               unsigned char g = idata[pos+1];
               unsigned char b = idata[pos+2];
               if ( hasmask && r == maskr && g == maskg && b == maskb ) {
                  // treat transparent pixel as a dead cell
               } else if ( r < 255 || g < 255 || b < 255 ) {
                  // treat non-white pixel as a live cell
                  curralgo->setcell(x, y, 1);
               }
            }
         
         curralgo->endofpattern();
      } else {
         Warning(_("Could not load image from file!"));
      }
      return true;
   } else {
      return false;
   }
}

// -----------------------------------------------------------------------------

void MainFrame::LoadPattern(const wxString& newtitle)
{
   // don't use initrule in future NewPattern calls
   initrule[0] = 0;
   if (!newtitle.IsEmpty()) {
      savestart = false;
      warp = 0;
      if (GetInfoFrame()) {
         // comments will no longer be relevant so close info window
         GetInfoFrame()->Close(true);
      }
   }
   if (!showbanner) statusptr->ClearMessage();

   // set this flag BEFORE UpdateStatus() call so we see gen=0 and pop=0;
   // in particular, it avoids getPopulation being called which would
   // slow down hlife pattern loading
   viewptr->nopattupdate = true;
   
   // update all of status bar so we don't see different colored lines;
   // on Mac, DrawView also gets called if there are pending updates
   UpdateStatus();
   CreateUniverse();

   if (!newtitle.IsEmpty()) {
      // show new file name in window title but no rule (which readpattern can change);
      // nicer if user can see file name while loading a very large pattern
      MySetTitle(_("Loading ") + newtitle);
   }

   if (LoadImage()) {
      viewptr->nopattupdate = false;
   } else {
      const char *err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      if (err && strcmp(err,cannotreadhash) == 0 && !hashing) {
         hashing = true;
         statusptr->SetMessage(_("Hashing has been turned on for macrocell format."));
         // update all of status bar so we don't see different colored lines
         UpdateStatus();
         CreateUniverse();
         err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      } else if (global_liferules.hasB0notS8 && hashing && !newtitle.IsEmpty()) {
         hashing = false;
         statusptr->SetMessage(B0message);
         // update all of status bar so we don't see different colored lines
         UpdateStatus();
         CreateUniverse();
         err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      }
      viewptr->nopattupdate = false;
      if (err) Warning(wxString(err,wxConvLocal));
   }

   if (!newtitle.IsEmpty()) {
      // show full window title after readpattern has set rule
      SetWindowTitle(newtitle);
      if (openremovesel) viewptr->NoSelection();
      if (opencurs) currcurs = opencurs;
      viewptr->FitInView(1);
      startgen = curralgo->getGeneration();     // might be > 0
      UpdateEverything();
      showbanner = false;
   } else {
      // ResetPattern sets rule, window title, scale and location
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ResetPattern()
{
   if (generating || curralgo->getGeneration() == startgen) return;
   
   if (curralgo->getGeneration() < startgen) {
      // if this happens then startgen logic is wrong
      Warning(_("Current gen < starting gen!"));
      return;
   }
   
   if (startfile.IsEmpty() && currfile.IsEmpty()) {
      // if this happens then savestart logic is wrong
      Warning(_("Starting pattern cannot be restored!"));
      return;
   }
   
   // restore pattern and settings saved by SaveStartingPattern;
   // first restore step size, hashing option and starting pattern
   warp = startwarp;
   hashing = starthash;

   wxString oldfile;
   if ( !startfile.IsEmpty() ) {
      // temporarily change currfile to startfile
      oldfile = currfile;
      currfile = startfile;
   }
   
   // restore starting pattern from currfile;
   // pass in empty string so savestart, warp and currcurs won't change
   LoadPattern(wxEmptyString);
   // gen count has been reset to startgen
   
   if ( !startfile.IsEmpty() ) {
      // restore currfile
      currfile = oldfile;
      savestart = true;       // should not be necessary, but play safe
   }
   
   // now restore rule, window title, scale and location
   curralgo->setrule(startrule.mb_str(wxConvLocal));
   SetWindowTitle(wxEmptyString);
   viewptr->SetPosMag(startx, starty, startmag);
   UpdateEverything();
}

// -----------------------------------------------------------------------------

wxString MainFrame::GetBaseName(const wxString& fullpath)
{
   // extract basename from given path
   return fullpath.AfterLast(wxFILE_SEP_PATH);
}

// -----------------------------------------------------------------------------

void MainFrame::SetCurrentFile(const wxString& path)
{
   #ifdef __WXMAC__
      // copy given path to currfile but as decomposed UTF8 so fopen will work
      #if wxCHECK_VERSION(2, 7, 0)
         currfile = wxString(path.fn_str(), wxConvLocal);
      #else
         // wxMac 2.6.x or older (but conversion doesn't always work on OS 10.4)
         currfile = wxString(path.wc_str(wxConvLocal), wxConvUTF8);
      #endif
   #else
      currfile = path;
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::OpenFile(const wxString& path, bool remember)
{
   if ( IsScript(path) ) {
      // execute script
      if (remember) AddRecentScript(path);
      RunScript(path);
   } else {
      // load pattern
      SetCurrentFile(path);
      if (remember) AddRecentPattern(path);
      LoadPattern( GetBaseName(path) );
   }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentPattern(const wxString& path)
{
   // put given path at start of patternSubMenu
   int id = patternSubMenu->FindItem(path);
   if ( id == wxNOT_FOUND ) {
      if ( numpatterns < maxpatterns ) {
         // add new path
         numpatterns++;
         id = GetID_OPEN_RECENT() + numpatterns;
         patternSubMenu->Insert(numpatterns - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem *item = patternSubMenu->FindItemByPosition(maxpatterns - 1);
         item->SetText(path);
         id = GetID_OPEN_RECENT() + maxpatterns;
      }
   }
   // path exists in patternSubMenu 
   if ( id > GetID_OPEN_RECENT() + 1 ) {
      // move path to start of menu (item IDs don't change)
      wxMenuItem *item;
      while ( id > GetID_OPEN_RECENT() + 1 ) {
         wxMenuItem *previtem = patternSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         item = patternSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = patternSubMenu->FindItem(id);
      item->SetText(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentScript(const wxString& path)
{
   // put given path at start of scriptSubMenu
   int id = scriptSubMenu->FindItem(path);
   if ( id == wxNOT_FOUND ) {
      if ( numscripts < maxscripts ) {
         // add new path
         numscripts++;
         id = GetID_RUN_RECENT() + numscripts;
         scriptSubMenu->Insert(numscripts - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem *item = scriptSubMenu->FindItemByPosition(maxscripts - 1);
         item->SetText(path);
         id = GetID_RUN_RECENT() + maxscripts;
      }
   }
   // path exists in scriptSubMenu 
   if ( id > GetID_RUN_RECENT() + 1 ) {
      // move path to start of menu (item IDs don't change)
      wxMenuItem *item;
      while ( id > GetID_RUN_RECENT() + 1 ) {
         wxMenuItem *previtem = scriptSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         item = scriptSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = scriptSubMenu->FindItem(id);
      item->SetText(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenPattern()
{
   if (generating) return;

   wxString filetypes = _("All files (*)|*");
   filetypes +=         _("|RLE (*.rle)|*.rle");
   filetypes +=         _("|Macrocell (*.mc)|*.mc");
   filetypes +=         _("|Life 1.05/1.06 (*.lif)|*.lif");
   filetypes +=         _("|dblife (*.l)|*.l");
   filetypes +=         _("|Gzip (*.gz)|*.gz");
   filetypes +=         _("|BMP (*.bmp)|*.bmp");
   filetypes +=         _("|GIF (*.gif)|*.gif");
   filetypes +=         _("|PNG (*.png)|*.png");
   filetypes +=         _("|TIFF (*.tiff;*.tif)|*.tiff;*.tif");
   
   wxFileDialog opendlg(this, _("Choose a pattern file"),
                        opensavedir, wxEmptyString, filetypes,
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      opensavedir = fullpath.GetPath();
      SetCurrentFile( opendlg.GetPath() );
      AddRecentPattern( opendlg.GetPath() );
      LoadPattern( opendlg.GetFilename() );
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenScript()
{
   if (generating) return;

   wxFileDialog opendlg(this, _("Choose a Python script"),
                        rundir, wxEmptyString,
                        _("Python script (*.py)|*.py"),
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      rundir = fullpath.GetPath();
      AddRecentScript( opendlg.GetPath() );
      RunScript( opendlg.GetPath() );
   }
}

// -----------------------------------------------------------------------------

bool MainFrame::CopyTextToClipboard(wxString &text)
{
   bool result = true;
   #ifdef __WXX11__
      // no global clipboard support on X11 so we save data in a file
      wxFile tmpfile(clipfile, wxFile::write);
      if ( tmpfile.IsOpened() ) {
         size_t textlen = text.Length();
         if ( tmpfile.Write( text.c_str(), textlen ) < textlen ) {
            Warning(_("Could not write all data to clipboard file!"));
            result = false;
         }
         tmpfile.Close();
      } else {
         Warning(_("Could not create clipboard file!"));
         result = false;
      }
   #else
      if (wxTheClipboard->Open()) {
         if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
            Warning(_("Could not copy text to clipboard!"));
            result = false;
         }
         wxTheClipboard->Close();
      } else {
         Warning(_("Could not open clipboard!"));
         result = false;
      }
   #endif
   return result;
}

// -----------------------------------------------------------------------------

bool MainFrame::GetTextFromClipboard(wxTextDataObject *textdata)
{
   bool gotdata = false;
   
   if ( wxTheClipboard->Open() ) {
      if ( wxTheClipboard->IsSupported( wxDF_TEXT ) ) {
         gotdata = wxTheClipboard->GetData( *textdata );
         if (!gotdata) {
            statusptr->ErrorMessage(_("Could not get clipboard text!"));
         }

      } else if ( wxTheClipboard->IsSupported( wxDF_BITMAP ) ) {
         wxBitmapDataObject bmapdata;
         gotdata = wxTheClipboard->GetData( bmapdata );
         if (gotdata) {
            // convert bitmap data to text data
            wxString str;
            wxBitmap bmap = bmapdata.GetBitmap();
            wxImage image = bmap.ConvertToImage();
            if (image.Ok()) {
               /* there doesn't seem to be any mask or alpha info, at least on Mac
               if (bmap.GetMask() != NULL) Warning("Bitmap has mask!");
               if (image.HasMask()) Warning("Image has mask!");
               if (image.HasAlpha()) Warning("Image has alpha!");
               */
               int wd = image.GetWidth();
               int ht = image.GetHeight();
               unsigned char *idata = image.GetData();
               int x, y;
               for (y = 0; y < ht; y++) {
                  for (x = 0; x < wd; x++) {
                     long pos = (y * wd + x) * 3;
                     if ( idata[pos] < 255 || idata[pos+1] < 255 || idata[pos+2] < 255 ) {
                        // non-white pixel is a live cell
                        str += 'o';
                     } else {
                        // white pixel is a dead cell
                        str += '.';
                     }
                  }
                  str += '\n';
               }
               textdata->SetText(str);
            } else {
               statusptr->ErrorMessage(_("Could not convert clipboard bitmap!"));
               gotdata = false;
            }
         } else {
            statusptr->ErrorMessage(_("Could not get clipboard bitmap!"));
         }

      } else {
         #ifdef __WXX11__
            statusptr->ErrorMessage(_("Sorry, but there is no clipboard support for X11."));
            // do X11 apps like xlife or fontforge have clipboard support???!!!
         #else
            statusptr->ErrorMessage(_("No data in clipboard."));
         #endif
      }
      wxTheClipboard->Close();

   } else {
      statusptr->ErrorMessage(_("Could not open clipboard!"));
   }
   
   return gotdata;
}

// -----------------------------------------------------------------------------

void MainFrame::OpenClipboard()
{
   if (generating) return;
   // load and view pattern data stored in clipboard
   #ifdef __WXX11__
      // on X11 the clipboard data is in non-temporary clipfile, so copy
      // clipfile to tempstart (for use by ResetPattern and ShowPatternInfo)
      if ( wxCopyFile(clipfile, tempstart, true) ) {
         currfile = tempstart;
         LoadPattern(_("clipboard"));
      } else {
         statusptr->ErrorMessage(_("Could not copy clipfile!"));
      }
   #else
      wxTextDataObject data;
      if (GetTextFromClipboard(&data)) {
         // copy clipboard data to tempstart so we can handle all formats
         // supported by readpattern
         wxFile outfile(tempstart, wxFile::write);
         if ( outfile.IsOpened() ) {
            outfile.Write( data.GetText() );
            outfile.Close();
            currfile = tempstart;
            LoadPattern(_("clipboard"));
            // do NOT delete tempstart -- it can be reloaded by ResetPattern
            // or used by ShowPatternInfo
         } else {
            statusptr->ErrorMessage(_("Could not create tempstart file!"));
         }
      }
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::RunClipboard()
{
   if (generating) return;
   // run script stored in clipboard
   wxTextDataObject data;
   if (GetTextFromClipboard(&data)) {
      // copy clipboard data to scriptfile
      wxFile outfile(scriptfile, wxFile::write);
      if ( outfile.IsOpened() ) {
         outfile.Write( data.GetText() );
         outfile.Close();
         RunScript(scriptfile);
      } else {
         statusptr->ErrorMessage(_("Could not create script file!"));
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentPattern(int id)
{
   wxMenuItem *item = patternSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      SetCurrentFile(path);
      AddRecentPattern(path);
      LoadPattern(GetBaseName(path));
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentScript(int id)
{
   wxMenuItem *item = scriptSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      AddRecentScript(path);
      RunScript(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ClearRecentPatterns()
{
   while (numpatterns > 0) {
      patternSubMenu->Delete( patternSubMenu->FindItemByPosition(0) );
      numpatterns--;
   }
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) mbar->Enable(GetID_OPEN_RECENT(), false);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearRecentScripts()
{
   while (numscripts > 0) {
      scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(0) );
      numscripts--;
   }
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) mbar->Enable(GetID_RUN_RECENT(), false);
}

// -----------------------------------------------------------------------------

const char *MainFrame::WritePattern(const wxString& path,
                                    pattern_format format,
                                    int top, int left, int bottom, int right)
{
   #if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0)
      // use decomposed UTF8 so fopen will work
      const char *err = writepattern(path.fn_str(),
                        *curralgo, format, top, left, bottom, right);
   #else
      const char *err = writepattern(path.mb_str(wxConvLocal),
                        *curralgo, format, top, left, bottom, right);
   #endif
   return err;
}

// -----------------------------------------------------------------------------

void MainFrame::SavePattern()
{
   if (generating) return;
   
   wxString filetypes;
   int RLEindex, L105index, MCindex;
   
   // initially all formats are not allowed (use any -ve number)
   RLEindex = L105index = MCindex = -1;
   
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   
   wxString RLEstring;
   if (savexrle)
      RLEstring = _("Extended RLE (*.rle)|*.rle");
   else
      RLEstring = _("RLE (*.rle)|*.rle");
   
   if (hashing) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         // too big so only allow saving as MC file
         itop = ileft = ibottom = iright = 0;
         filetypes = _("Macrocell (*.mc)|*.mc");
         MCindex = 0;
      } else {
         // allow saving as RLE/MC file
         itop = top.toint();
         ileft = left.toint();
         ibottom = bottom.toint();
         iright = right.toint();
         filetypes = RLEstring;
         RLEindex = 0;
         filetypes += _("|Macrocell (*.mc)|*.mc");
         MCindex = 1;
      }
   } else {
      // allow saving file only if pattern is small enough
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
         return;
      }   
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
      filetypes = RLEstring;
      RLEindex = 0;
      /* Life 1.05 format not yet implemented!!!
      filetypes += _("|Life 1.05 (*.lif)|*.lif");
      L105index = 1;
      */
   }

   wxFileDialog savedlg( this, _("Save pattern"),
                         opensavedir, wxEmptyString, filetypes,
                         wxSAVE | wxOVERWRITE_PROMPT );

   if ( savedlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( savedlg.GetPath() );
      opensavedir = fullpath.GetPath();
      wxString ext = fullpath.GetExt();
      pattern_format format;
      // if user supplied a known extension then use that format if it is
      // allowed, otherwise use current format specified in filter menu
      if ( ext.IsSameAs(wxT("rle"),false) && RLEindex >= 0 ) {
         format = savexrle ? XRLE_format : RLE_format;
      /* Life 1.05 format not yet implemented!!!
      } else if ( ext.IsSameAs("lif",false) && L105index >= 0 ) {
         format = L105_format;
      */
      } else if ( ext.IsSameAs(wxT("mc"),false) && MCindex >= 0 ) {
         format = MC_format;
      } else if ( savedlg.GetFilterIndex() == RLEindex ) {
         format = savexrle ? XRLE_format : RLE_format;
      } else if ( savedlg.GetFilterIndex() == L105index ) {
         format = L105_format;
      } else if ( savedlg.GetFilterIndex() == MCindex ) {
         format = MC_format;
      } else {
         statusptr->ErrorMessage(_("Bug in SavePattern!"));
         return;
      }
      SetCurrentFile( savedlg.GetPath() );
      AddRecentPattern( savedlg.GetPath() );
      SetWindowTitle( savedlg.GetFilename() );
      const char *err = WritePattern(savedlg.GetPath(), format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
      } else {
         statusptr->DisplayMessage(_("Pattern saved in file."));
         if ( curralgo->getGeneration() == startgen ) {
            // no need to save starting pattern (ResetPattern can load currfile)
            savestart = false;
         }
      }
   }
}

// -----------------------------------------------------------------------------

// called by script command to save current pattern to given file
wxString MainFrame::SaveFile(const wxString& path, const wxString& format, bool remember)
{
   // check that given format is valid and allowed
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   
   pattern_format pattfmt;
   if ( format.IsSameAs(wxT("rle"),false) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         return _("Pattern is too big to save as RLE.");
      }   
      pattfmt = savexrle ? XRLE_format : RLE_format;
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
   } else if ( format.IsSameAs(wxT("mc"),false) ) {
      if (!hashing) {
         return _("Macrocell format is only allowed if hashing.");
      }
      pattfmt = MC_format;
      // writepattern will ignore itop, ileft, ibottom, iright
      itop = ileft = ibottom = iright = 0;
   } else {
      return _("Unknown pattern format.");
   }   
   
   SetCurrentFile(path);
   if (remember) AddRecentPattern(path);
   SetWindowTitle( GetBaseName(path) );
   const char *err = WritePattern(path, pattfmt, itop, ileft, ibottom, iright);
   if (!err) {
      if ( curralgo->getGeneration() == startgen ) {
         // no need to save starting pattern (ResetPattern can load currfile)
         savestart = false;
      }
   }
   return wxString(err, wxConvLocal);
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleShowPatterns()
{
   showpatterns = !showpatterns;
   if (showpatterns && showscripts) {
      showscripts = false;
      splitwin->Unsplit(scriptctrl);
      splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         dirwinwd = splitwin->GetSashPosition();
         splitwin->Unsplit(patternctrl);
      } else {
         splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
      }
      // resize viewport
      viewptr->SetViewSize();
      viewptr->SetFocus();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleShowScripts()
{
   showscripts = !showscripts;
   if (showscripts && showpatterns) {
      showpatterns = false;
      splitwin->Unsplit(patternctrl);
      splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         dirwinwd = splitwin->GetSashPosition();
         splitwin->Unsplit(scriptctrl);
      } else {
         splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
      }
      // resize viewport
      viewptr->SetViewSize();
      viewptr->SetFocus();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ChangePatternDir()
{
   // wxMac bug: 3rd parameter seems to be ignored!!!
   wxDirDialog dirdlg(this, _("Choose a new pattern folder"),
                      patterndir, wxDD_NEW_DIR_BUTTON);
   if ( dirdlg.ShowModal() == wxID_OK ) {
      wxString newdir = dirdlg.GetPath();
      if ( patterndir != newdir ) {
         patterndir = newdir;
         if ( showpatterns ) {
            // show new pattern directory
            SimplifyTree(patterndir, patternctrl->GetTreeCtrl(), patternctrl->GetRootId());
         }
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ChangeScriptDir()
{
   // wxMac bug: 3rd parameter seems to be ignored!!!
   wxDirDialog dirdlg(this, _("Choose a new script folder"),
                      scriptdir, wxDD_NEW_DIR_BUTTON);
   if ( dirdlg.ShowModal() == wxID_OK ) {
      wxString newdir = dirdlg.GetPath();
      if ( scriptdir != newdir ) {
         scriptdir = newdir;
         if ( showscripts ) {
            // show new script directory
            SimplifyTree(scriptdir, scriptctrl->GetTreeCtrl(), scriptctrl->GetRootId());
         }
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::SetMinimumWarp()
{
   // set minwarp depending on mindelay and maxdelay
   minwarp = 0;
   if (mindelay > 0) {
      int d = mindelay;
      minwarp--;
      while (d < maxdelay) {
         d *= 2;
         minwarp--;
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::UpdateWarp()
{
   SetMinimumWarp();
   if (warp < minwarp) {
      warp = minwarp;
      curralgo->setIncrement(1);    // warp is <= 0
   } else if (warp > 0) {
      SetGenIncrement();            // in case qbasestep/hbasestep changed
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ShowPrefsDialog()
{
   if (inscript || generating || viewptr->waitingforclick) return;
   
   if (ChangePrefs()) {
      // user hit OK button
   
      // selection color may have changed
      SetSelectionColor();

      // if maxpatterns was reduced then we may need to remove some paths
      while (numpatterns > maxpatterns) {
         numpatterns--;
         patternSubMenu->Delete( patternSubMenu->FindItemByPosition(numpatterns) );
      }

      // if maxscripts was reduced then we may need to remove some paths
      while (numscripts > maxscripts) {
         numscripts--;
         scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(numscripts) );
      }
      
      // randomfill might have changed
      SetRandomFillPercentage();
      
      // if mindelay/maxdelay changed then may need to change minwarp and warp
      UpdateWarp();
      
      // we currently don't allow user to edit prefs while generating,
      // but in case that changes:
      if (generating && warp < 0) {
         whentosee = 0;                // best to see immediately
      }
      
      // maxhashmem might have changed
      if (hashing) curralgo->setMaxMemory(maxhashmem);
      
      SavePrefs();
      UpdateEverything();
   }
}