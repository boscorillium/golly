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
#ifndef RULELOADERALGO_H
#define RULELOADERALGO_H
#include "ghashbase.h"
#include "ruletable_algo.h"
#include "ruletreealgo.h"
/**
 *   This algorithm loads rule data from external files.
 */
class ruleloaderalgo : public ghashbase {

public:

    ruleloaderalgo();
    virtual ~ruleloaderalgo();
    virtual state slowcalc(state nw, state n, state ne, state w, state c,
                           state e, state sw, state s, state se);
    virtual const char* setrule(const char* s);
    virtual const char* getrule();
    virtual const char* DefaultRule();
    virtual int NumCellStates();
    static void doInitializeAlgoInfo(staticAlgoInfo &);

protected:
    
    ruletable_algo* LocalRuleTable;      // local instance of RuleTable algo
    ruletreealgo* LocalRuleTree;         // local instance of RuleTree algo

    enum RuleTypes {TABLE, TREE} rule_type;
    
    void SetAlgoVariables(RuleTypes ruletype);
    const char* LoadTableOrTree(FILE* rulefile, const char* rule);
};
#endif
