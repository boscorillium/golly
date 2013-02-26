# Import any icons for the current rule so user can edit them.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2013.

import golly as g
from glife import getminbox, rect, pattern
from glife.text import make_text
from time import time
from colorsys import hsv_to_rgb
import os

iconinfo31x31 = []
iconinfo15x15 = []
iconinfo7x7 = []
iconcolors = []     # list of (r,g,b) colors used in ALL icon sizes
colorstate = {}     # dictionary mapping (r,g,b) colors to cell states

# --------------------------------------------------------------------

def parse_hex(hexstr):
    # parse a string like "FF00EE" or "FFFF0000EEEE"
    R, G, B = (0, 0, 0)
    if len(hexstr) == 6:
        R = int(hexstr[0:2],16)
        G = int(hexstr[2:4],16)
        B = int(hexstr[4:6],16)
    elif len(hexstr) == 12:
        # only use upper 2 hex digits
        R = int(hexstr[0:2],16)
        G = int(hexstr[4:6],16)
        B = int(hexstr[8:10],16)
    else:
        g.warn("Bad hex string: " + hexstr)
    return (R,G,B)

# --------------------------------------------------------------------

def import_icons(rulename):
    global iconinfo31x31, iconinfo15x15, iconinfo7x7, iconcolors
    
    # replace any illegal filename chars with underscores
    rulename = rulename.replace("/","_").replace("\\","_")
    
    rulepath = g.getdir("rules") + rulename + ".rule"
    if not os.path.isfile(rulepath):
        rulepath = g.getdir("app") + "Rules/" + rulename + ".rule"
        if not os.path.isfile(rulepath):
            g.warn("There is no .rule file for this rule: " + rulename)
            return []
    
    try:
        rulefile = open(rulepath,"r")
    except:
        g.exit("Failed to open .rule file: " + rulepath)
    
    foundicons = False
    xpmcount = 0
    width = 0
    height = 0
    num_colors = 0
    chars_per_pixel = 0
    iconinfo = []
    colordict = {}
    
    for line in rulefile:
        if line.startswith("@ICONS"):
            foundicons = True
        elif foundicons and line.startswith("XPM"):
            xpmcount = 1
            iconinfo = []
            colordict = {}
        elif xpmcount > 0 and line[0] == "\"":
            # extract the stuff inside double quotes, ignoring anything after 2nd one
            line, sep, extra = line.lstrip("\"").partition("\"")
            if xpmcount == 1:
                # parse "width height num_colors chars_per_pixel"
                header = line.split()
                width = int(header[0])
                height = int(header[1])
                num_colors = int(header[2])
                chars_per_pixel = int(header[3])
                iconinfo.append(width)
                iconinfo.append(height)
                iconinfo.append(num_colors)
                iconinfo.append(chars_per_pixel)
            
            elif xpmcount > 1 and xpmcount <= 1 + num_colors:
                # parse color index line like "A c #FFFFFF" or "AB c #FF009900BB00"
                key, c, hexrgb = line.split()
                rgb = parse_hex(hexrgb.lstrip("#"))
                if not rgb in iconcolors:
                    iconcolors.append(rgb)
                colordict[key] = rgb
                if xpmcount == 1 + num_colors:
                    iconinfo.append(colordict)
            
            elif xpmcount <= 1 + num_colors + height:
                # simply append pixel data in line like "......AAA......"
                iconinfo.append(line)
            
            if xpmcount == 1 + num_colors + height:
                if width == 31: iconinfo31x31 = iconinfo
                if width == 15: iconinfo15x15 = iconinfo
                if width == 7: iconinfo7x7 = iconinfo
                xpmcount = 0    # skip any extra lines
            else:
                xpmcount += 1
        elif foundicons and line.startswith("@"):
            # start of another section
            break

    rulefile.close()

# --------------------------------------------------------------------

def draw_line(x1, y1, x2, y2, state = 1):
    # draw a line of cells in given state from x1,y1 to x2,y2
    g.setcell(x1, y1, state)
    if x1 == x2 and y1 == y2: return
    
    dx = x2 - x1
    ax = abs(dx) * 2
    sx = 1
    if dx < 0: sx = -1
    dy = y2 - y1
    ay = abs(dy) * 2
    sy = 1
    if dy < 0: sy = -1
    
    if ax > ay:
        d = ay - (ax / 2)
        while x1 != x2:
            g.setcell(x1, y1, state)
            if d >= 0:
                y1 += sy
                d -= ax
            x1 += sx
            d += ay
    else:
        d = ax - (ay / 2)
        while y1 != y2:
            g.setcell(x1, y1, state)
            if d >= 0:
                x1 += sx
                d -= ay
            y1 += sy
            d += ax
    
    g.setcell(x2, y2, state)

# --------------------------------------------------------------------

def color_text(string, extrastate):
    t = make_text(string, "mono")
    bbox = getminbox(t)
    # convert two-state pattern to multi-state and set state to extrastate
    mlist = []
    tlist = list(t)
    for i in xrange(0, len(tlist), 2):
        mlist.append(tlist[i])
        mlist.append(tlist[i+1])
        mlist.append(extrastate)
    if len(mlist) % 2 == 0: mlist.append(0)
    p = pattern(mlist)
    return p, bbox.wd, bbox.ht

# --------------------------------------------------------------------

def init_colors():
    # start with all colors used in the imported icons
    global iconcolors, colorstate
    if len(iconcolors) > 240: g.exit("The imported icons use too many colors!")
    s = 1
    for rgb in iconcolors:
        if rgb == (255,255,255) or rgb == (0,0,0):
            # white and black will be added below
            pass
        else:
            R,G,B = rgb
            g.setcolors([s,R,G,B])
            colorstate[rgb] = s
            s += 1
    
    # add white to black
    g.setcolors([s, 255,255,255])    # white
    colorstate[(255,255,255)] = s
    s += 1
    g.setcolors([s, 191,191,191])    # light gray
    s += 1
    g.setcolors([s, 127,127,127])    # gray
    gray = s
    s += 1
    g.setcolors([s,  63, 63, 63])    # dark gray
    s += 1
    g.setcolors([s,   0,  0,  0])    # black
    colorstate[(0,0,0)] = s
    s += 1
    
    # add rainbow colors in various shades (bright, pale, dark)
    for hue in xrange(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 1.0, 1.0)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    for hue in xrange(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 0.5, 1.0)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    for hue in xrange(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 1.0, 0.5)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    
    # return the 50% gray state (used for drawing boxes and text)
    return gray

# --------------------------------------------------------------------

def draw_icon_boxes(numicons, linestate):
    for i in xrange(numicons):
        x = -1 + i*32
        y = -1
        
        # draw boxes for 31x31 icons
        draw_line(x, y, x, y+32, linestate)
        draw_line(x, y, x+32, y, linestate)
        draw_line(x+32, y, x+32, y+32, linestate)
        draw_line(x, y+32, x+32, y+32, linestate)
        
        # draw boxes for 15x15 icons
        draw_line(x, y+32, x, y+32+16, linestate)
        draw_line(x, y+32+16, x+16, y+32+16, linestate)
        draw_line(x+16, y+32, x+16, y+32+16, linestate)
        
        # draw boxes for 7x7 icons
        draw_line(x, y+32+16, x, y+32+16+8, linestate)
        draw_line(x, y+32+16+8, x+8, y+32+16+8, linestate)
        draw_line(x+8, y+32+16, x+8, y+32+16+8, linestate)
        
        # show state number above top row of icons
        t, twd, tht = color_text(str(i+1), linestate)
        t.put(x + 32/2 - twd/2, y - 2 - tht)

# --------------------------------------------------------------------

def draw_icons(iconinfo, deadrgb):
    global colorstate
    if len(iconinfo) == 0: return
    width = iconinfo[0]
    height = iconinfo[1]
    num_colors = iconinfo[2]
    chars_per_pixel = iconinfo[3]
    colordict = iconinfo[4]
    pos = 5
    numicons = height/width
    for i in xrange(numicons):
        x = i*32
        y = 0
        if width == 15: y = 32
        if width == 7: y = 48
        for row in xrange(width):
            pxls = iconinfo[pos]
            pos += 1
            for col in xrange(width):
                offset = col*chars_per_pixel
                key = pxls[offset : offset + chars_per_pixel]
                if not key in colordict:
                    g.exit("Unexpected key in icon data: " + key)
                rgb = colordict[key]
                if rgb != deadrgb:
                    g.setcell(x+col, y+row, colorstate[rgb])

# --------------------------------------------------------------------

# check that a layer is available
if g.numlayers() == g.maxlayers():
    g.exit("You need to delete a layer.")

iconlayer = g.addlayer()
rulename, sep, suffix = g.getrule().partition(":")
g.new("imported icons for " + rulename)
livestates = g.numstates() - 1
deadcolor = g.getcolors(0)
deadrgb = (deadcolor[1], deadcolor[2], deadcolor[3])

# search for rulename.rule and import any icon data
import_icons(rulename)

# switch to a Generations rule so we can have lots of colors
g.setrule("//256")
g.setcolors(deadcolor)
graystate = init_colors()

draw_icon_boxes(livestates, graystate)
draw_icons(iconinfo31x31, deadrgb)
draw_icons(iconinfo15x15, deadrgb)
draw_icons(iconinfo7x7, deadrgb)

g.fit()
g.setoption("showicons",False)
g.show("Edit the icons and then run icon-exporter.py.")
