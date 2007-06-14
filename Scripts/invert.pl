# Invert all cell states in the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), May 2007.

use strict;
use Time::HiRes qw ( time );

my @rect = g_getselrect();
g_exit("There is no selection.") if @rect == 0;
my $x = $rect[0];
my $y = $rect[1];
my $wd = $rect[2];
my $ht = $rect[3];

my $oldsecs = time;

for (my $row = $y; $row < $y + $ht; $row++) {
   # if large selection then give some indication of progress
   my $newsecs = time;
   if ($newsecs - $oldsecs >= 1.0) {
      $oldsecs = $newsecs;
      g_update();
   }

   # also allow keyboard interaction
   g_dokey( g_getkey() );

   for (my $col = $x; $col < $x + $wd; $col++) {
      g_setcell($col, $row, 1 - g_getcell($col, $row));
   }
}

g_fitsel() if !g_visrect(@rect);