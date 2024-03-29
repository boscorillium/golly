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

#include "bigint.h"
#include "lifealgo.h"

#include "utils.h"      // for Fatal, Beep
#include "prefs.h"      // for mindelay, maxdelay, etc
#include "algos.h"      // for algoinfo
#include "layer.h"      // for currlayer
#include "view.h"       // for nopattupdate, widescreen
#include "status.h"

#ifdef ANDROID_GUI
    #include "jnicalls.h"   // for UpdateStatus, GetRuleName
#endif
#ifdef IOS_GUI
    #import "PatternViewController.h"   // for UpdateStatus
    #import "RuleViewController.h"      // for GetRuleName
#endif

// -----------------------------------------------------------------------------

std::string status1;    // top line
std::string status2;    // middle line
std::string status3;    // bottom line

// prefixes used when widescreen is true:
const char* large_gen_prefix =   "Generation=";
const char* large_algo_prefix =  "    Algorithm=";
const char* large_rule_prefix =  "    Rule=";
const char* large_pop_prefix =   "    Population=";
const char* large_scale_prefix = "    Scale=";
const char* large_step_prefix =  "    ";
const char* large_xy_prefix =    "    XY=";

// prefixes used when widescreen is false:
const char* small_gen_prefix =   "Gen=";
const char* small_algo_prefix =  "   Algo=";
const char* small_rule_prefix =  "   Rule=";
const char* small_pop_prefix =   "   Pop=";
const char* small_scale_prefix = "   Scale=";
const char* small_step_prefix =  "   ";
const char* small_xy_prefix =    "   XY=";

// -----------------------------------------------------------------------------

void UpdateStatusLines()
{
    std::string rule = currlayer->algo->getrule();

    status1 = "Pattern=";
    if (currlayer->dirty) {
        // display asterisk to indicate pattern has been modified
        status1 += "*";
    }
    status1 += currlayer->currname;
    status1 += widescreen ? large_algo_prefix : small_algo_prefix;
    status1 += GetAlgoName(currlayer->algtype);
    status1 += widescreen ? large_rule_prefix : small_rule_prefix;
    status1 += rule;

    // show rule name if one exists and is not same as rule
    // (best NOT to remove any suffix like ":T100,200" in case we allow
    // users to name "B3/S23:T100,200" as "Life on torus")
    std::string rulename = GetRuleName(rule);
    if (!rulename.empty() && rulename != rule) {
        status1 += " [";
        status1 += rulename;
        status1 += "]";
    }

    char scalestr[32];
    int mag = currlayer->view->getmag();
    if (mag < 0) {
        sprintf(scalestr, "2^%d:1", -mag);
    } else {
        sprintf(scalestr, "1:%d", 1 << mag);
    }

    char stepstr[32];
    if (currlayer->currexpo < 0) {
        // show delay in secs
        sprintf(stepstr, "Delay=%gs", (double)GetCurrentDelay() / 1000.0);
    } else {
        sprintf(stepstr, "Step=%d^%d", currlayer->currbase, currlayer->currexpo);
    }

    status2 = widescreen ? large_gen_prefix : small_gen_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        status2 += Stringify(currlayer->algo->getGeneration());
    }
    status2 += widescreen ? large_pop_prefix : small_pop_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        bigint popcount = currlayer->algo->getPopulation();
        if (popcount.sign() < 0) {
            // getPopulation returns -1 if it can't be calculated
            status2 += "?";
        } else {
            status2 += Stringify(popcount);
        }
    }
    status2 += widescreen ? large_scale_prefix : small_scale_prefix;
    status2 += scalestr;
    status2 += widescreen ? large_step_prefix : small_step_prefix;
    status2 += stepstr;
    status2 += widescreen ? large_xy_prefix : small_xy_prefix;
    status2 += Stringify(currlayer->view->x);
    status2 += " ";
    status2 += Stringify(currlayer->view->y);
}

// -----------------------------------------------------------------------------

void ClearMessage()
{
    if (status3.length() == 0) return;    // no need to clear message

    status3.clear();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void DisplayMessage(const char* s)
{
    status3 = s;
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void ErrorMessage(const char* s)
{
    Beep();
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

void SetMessage(const char* s)
{
    // set message string without displaying it
    status3 = s;
}

// -----------------------------------------------------------------------------

int GetCurrentDelay()
{
    int gendelay = mindelay * (1 << (-currlayer->currexpo - 1));
    if (gendelay > maxdelay) gendelay = maxdelay;
    return gendelay;
}

// -----------------------------------------------------------------------------

char* Stringify(const bigint& b)
{
    static char buf[32];
    char* p = buf;
    double d = b.todouble();
    if ( fabs(d) > 1000000000.0 ) {
        // use e notation for abs value > 10^9 (agrees with min & max_coord)
        sprintf(p, "%g", d);
    } else {
        // show exact value with commas inserted for readability
        if ( d < 0 ) {
            d = - d;
            *p++ = '-';
        }
        sprintf(p, "%.f", d);
        int len = strlen(p);
        int commas = ((len + 2) / 3) - 1;
        int dest = len + commas;
        int src = len;
        p[dest] = 0;
        while (commas > 0) {
            p[--dest] = p[--src];
            p[--dest] = p[--src];
            p[--dest] = p[--src];
            p[--dest] = ',';
            commas--;
        }
        if ( p[-1] == '-' ) p--;
    }
    return p;
}
