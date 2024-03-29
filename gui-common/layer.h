/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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

#ifndef _LAYER_H_
#define _LAYER_H_

#include "bigint.h"         // for bigint class
#include "viewport.h"       // for viewport class

#include "utils.h"          // for gColor, gRect
#include "algos.h"          // for algo_type, gBitmapPtr
#include "select.h"         // for Selection class

class UndoRedo;

// export from script.h if we ever support scripting!!!
extern bool inscript;

enum TouchModes { drawmode, pickmode, selectmode, movemode };

// Golly can support multiple layers.  Each layer is a separate universe
// (unless cloned) with its own algorithm, rule, viewport, pattern title,
// selection, undo/redo history, etc.

class Layer {
public:
    Layer();
    ~Layer();

    // if this is a cloned layer then cloneid is > 0 and all the other clones
    // have the same cloneid
    int cloneid;

    lifealgo* algo;               // this layer's universe (shared by clones)
    algo_type algtype;            // type of universe (index into algoinfo)
    bool hyperspeed;              // use acceleration while generating?
    bool showhashinfo;            // show hashing info?
    bool autofit;                 // auto fit pattern while generating?
    bool dirty;                   // user has modified pattern?
    bool savedirty;               // state of dirty flag before drawing/script change
    bool stayclean;               // script has reset dirty flag?
    int currbase;                 // current base step
    int currexpo;                 // current step exponent
    int drawingstate;             // current drawing state
    TouchModes touchmode;         // current touch mode (drawing, selecting, etc)
    UndoRedo* undoredo;           // undo/redo history (shared by clones)

    // each layer (cloned or not) has its own viewport for displaying patterns;
    // note that we use a pointer to the viewport to allow temporary switching
    // in wxrender.cpp (eg. in DrawStackedLayers)
    viewport* view;

    // WARNING: this string is used to remember the current rule when
    // switching to another layer; to determine the current rule at any
    // time, use currlayer->algo->getrule()
    std::string rule;

    Selection currsel;            // current selection
    Selection savesel;            // for saving/restoring selection

    bigint originx;               // X origin offset
    bigint originy;               // Y origin offset

    std::string currfile;         // full path of current pattern file
    std::string currname;         // name used for Pattern=...

    // for saving and restoring starting pattern
    algo_type startalgo;          // starting algorithm
    bool savestart;               // need to save starting pattern?
    bool startdirty;              // starting state of dirty flag
    std::string startfile;        // file for saving starting pattern
    std::string startname;        // starting currname
    std::string startrule;        // starting rule
    bigint startgen;              // starting generation (>= 0)
    bigint startx, starty;        // starting location
    int startbase;                // starting base step
    int startexpo;                // starting step exponent
    int startmag;                 // starting scale
    Selection startsel;           // starting selection

    // temporary file used to restore starting pattern or to show comments;
    // each non-cloned layer uses a different temporary file
    std::string tempstart;

    // used when tilelayers is true
    //!!! PatternView* tilewin;         // tile window
    gRect tilerect;               // tile window's size and position

    // color scheme for this layer
    gColor fromrgb;               // start of gradient
    gColor torgb;                 // end of gradient
    unsigned char cellr[256];     // red components for states 0..255
    unsigned char cellg[256];     // green components for states 0..255
    unsigned char cellb[256];     // blue components for states 0..255

    // icons for this layer
    gBitmapPtr* icons7x7;         // icons for scale 1:8
    gBitmapPtr* icons15x15;       // icons for scale 1:16
    gBitmapPtr* icons31x31;       // icons for scale 1:32

    // texture data for rendering icons
    unsigned char** textures7x7;        // texture data for 7x7 icons
    unsigned char** textures15x15;      // texture data for 15x15 icons
    unsigned char** textures31x31;      // texture data for 31x31 icons

    bool multicoloricons;         // are icons multi-colored? (grayscale if not)

    // used if the layer has a timeline (see wxtimeline.cpp)
    int currframe;                // current frame in timeline
    int autoplay;                 // +ve = play forwards, -ve = play backwards, 0 = stop
    int tlspeed;                  // controls speed at which frames are played
    long lastframe;               // time (in msecs) when last frame was displayed
};

const int MAX_LAYERS = 10;      // maximum number of layers
extern int numlayers;           // number of existing layers
extern int numclones;           // number of cloned layers
extern int currindex;           // index of current layer (0..numlayers-1)
extern Layer* currlayer;        // pointer to current layer

Layer* GetLayer(int index);
// Return a pointer to the layer specified by the given index.

void AddLayer();
// Add a new layer (with an empty universe) and make it the current layer.
// The first call creates the initial layer.  Later calls insert the
// new layer immediately after the old current layer.  The new layer
// inherits the same type of universe, the same rule, the same scale
// and location, and the same touch mode.

void CloneLayer();
// Like AddLayer but shares the same universe and undo/redo history
// as the current layer.  All the current layer's settings are duplicated
// and most will be kept synchronized; ie. changing one clone changes all
// the others, but each cloned layer has a separate viewport so the same
// pattern can be viewed at different scales and locations.

void SyncClones();
// If the current layer is a clone then this call ensures the universe
// and settings in its other clones are correctly synchronized.

void DuplicateLayer();
// Like AddLayer but makes a copy of the current layer's pattern.
// Also duplicates all the current settings but, unlike a cloned layer,
// the settings are not kept synchronized.

void DeleteLayer();
// Delete the current layer.  The current layer changes to the previous
// index, unless layer 0 was deleted, in which case the current layer
// is the new layer at index 0.

void DeleteOtherLayers();
// Delete all layers except the current layer.

void SetLayer(int index);
// Set the current layer using the given index (0..numlayers-1).

void MoveLayer(int fromindex, int toindex);
// Move the layer at fromindex to toindex and make it the current layer.
// This is called by MoveLayerDialog and the movelayer script command.

void MoveLayerDialog();
// Bring up a dialog that allows the user to move the current layer
// to a new index (which becomes the current layer).

void NameLayerDialog();
// Bring up a dialog that allows the user to change the name of the
// current layer.

void MarkLayerDirty();
// Set dirty flag in current layer and any of its clones.
// Also prefix pattern title and layer name(s) with an asterisk.

void MarkLayerClean(const char* title);
// Reset dirty flag in current layer and any of its clones.
// Also set pattern title, removing asterisk from it and layer name(s).

void ToggleSyncViews();
// Toggle the syncviews flag.  When true, every layer uses the same
// scale and location as the current layer.

void ToggleSyncModes();
// Toggle the syncmodes flag.  When true, every layer uses the same
// touch mode as the current layer.

void ToggleStackLayers();
// Toggle the stacklayers flag.  When true, the rendering code displays
// all layers using the same scale and location as the current layer.
// Layer 0 is drawn first (100% opaque), then all the other layers
// are drawn as overlays using an opacity set by the user.  The colors
// for the live cells in each layer can be set via the Prefs dialog.

void ToggleTileLayers();
// Toggle the tilelayers flag.  When true, the rendering code displays
// all layers in tiled sub-windows.

bool CanSwitchLayer(int index);
// Return true if the user can switch to the given layer.

void SwitchToClickedTile(int index);
// If allowed, change current layer to clicked tile.

void ResizeLayers(int wd, int ht);
// Resize the viewport in all layers.

bool RestoreRule(const char* rule);
// Try to set the current layer's rule to a previously known rule.
// If this succeeds return true, but if it fails then warn the user,
// switch to the current algorithm's default rule, and return false.
// The latter can happen if the given rule's table/tree file was
// deleted or was edited and some sort of error introduced.

//??? move following color stuff into colors.*

void CreateColorGradient();
// Create a color gradient for the current layer using
// currlayer->fromrgb and currlayer->torgb.

void UpdateCloneColors();
// If current layer has clones then update their colors.

void UpdateLayerColors();
// Update the cell colors and icons for the current layer (and its clones)
// according to the current algo and rule.  Must be called very soon
// after any algo/rule change, and before the viewport is updated.

void InvertCellColors();
// Invert the cell colors in all layers, including the dead cell color,
// grid lines, and the colors in all icon bitmaps.

void SetLayerColors();
// Open a dialog to change the current layer's colors.

#endif
