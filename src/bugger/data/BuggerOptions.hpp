/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/* This program is free software: you can redistribute it and/or modify      */
/* it under the terms of the GNU Lesser General Public License as published  */
/* by the Free Software Foundation, either version 3 of the License, or      */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Lesser General Public License for more details.                       */
/*                                                                           */
/* You should have received a copy of the GNU Lesser General Public License  */
/* along with this program.  If not, see <https://www.gnu.org/licenses/>.    */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _BUGGER_CORE_PRESOLVE_OPTIONS_HPP_
#define _BUGGER_CORE_PRESOLVE_OPTIONS_HPP_

#include "bugger/misc/ParameterSet.hpp"
#include <type_traits>

namespace bugger
{

struct BuggerOptions
{

   int threads = 0;

   int initround = 0;

   int initstage = 0;

   int maxrounds = -1;

   int maxstages = -1;

   int nbatches = 0;

   double epsilon = 1e-9;

   double zeta = 0;

   double feastol = 1e-6;

   double tlim = std::numeric_limits<double>::max();

   Vec<int> passcodes = {};

   std::string debug_filename = "";

public:

   void
   addParameters( ParameterSet& paramSet )
   {
      paramSet.addParameter( "tlim", "bugger time limit", tlim, 0.0 );
      paramSet.addParameter( "initround", "initial bugger round or -1 for last round", initround, -1 );
      paramSet.addParameter( "initstage", "initial bugger stage or -1 for last stage", initstage, -1 );
      paramSet.addParameter( "maxrounds", "the maximum number of bugger rounds or -1 for no limit", maxrounds, -1 );
      paramSet.addParameter( "maxstages", " maximum number of bugger stages or -1 for number of modules", maxstages, -1 );
      paramSet.addParameter( "nbatches", "the maximum number of batches or 0 for singleton batches", nbatches, 0 );
      paramSet.addParameter( "threads", "maximal number of threads to use (0: automatic)", threads, 0 );
      paramSet.addParameter( "numerics.feastol", "the feasibility tolerance", feastol, 0.0, 1e-1 );
      paramSet.addParameter( "numerics.epsilon", "epsilon tolerance to consider two values numerically equal", epsilon, 0.0, 1e-1 );
      paramSet.addParameter( "numerics.zeta", "zeta tolerance to consider two values exactly equal", zeta, 0.0, 1e-1 );
      paramSet.addParameter( "passcodes", "list of ignored return codes (string separated by blanks) example: [passcodes = -1 -2]", passcodes );
      paramSet.addParameter( "debug_filename", "if not empty, current instance is written to this file before every solve (default = "") ]", debug_filename );
   }

};

} // namespace bugger

#endif
