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

#ifndef __BUGGER_DATA_BUGGERPARAMETERS_HPP__
#define __BUGGER_DATA_BUGGERPARAMETERS_HPP__

#include "bugger/misc/ParameterSet.hpp"


namespace bugger {

   struct BuggerParameters
   {
      int mode = -1;
      int nbatches = 0;
      int initround = 0;
      int initstage = 0;
      int maxrounds = -1;
      int maxstages = -1;
      double tlim = std::numeric_limits<double>::max();
      double feastol = 1e-6;
      double epsilon = 1e-9;
      double zeta = 0.0;
      Vec<int> passcodes = {};
      String debug_filename = "";

   public:

      void
      addParameters( ParameterSet& paramSet )
      {
         paramSet.addParameter( "mode", "selective bugger mode (-1: reproduce and reduce, 0: only reproduce, 1: only reduce)", mode, -1, 1 );
         paramSet.addParameter( "nbatches", "maximum number of batches or 0 for singleton batches", nbatches, 0 );
         paramSet.addParameter( "initround", "initial bugger round or -1 for last round", initround, -1 );
         paramSet.addParameter( "initstage", "initial bugger stage or -1 for last stage", initstage, -1 );
         paramSet.addParameter( "maxrounds", "maximum number of bugger rounds or -1 for no limit", maxrounds, -1 );
         paramSet.addParameter( "maxstages", "maximum number of bugger stages or -1 for number of modules", maxstages, -1 );
         paramSet.addParameter( "tlim", "bugger time limit", tlim, 0.0 );
         paramSet.addParameter( "numerics.feastol", "feasibility tolerance to consider constraints satisfied", feastol, 0.0, 1e-1 );
         paramSet.addParameter( "numerics.epsilon", "epsilon tolerance to consider two values numerically equal", epsilon, 0.0, 1e-1 );
         paramSet.addParameter( "numerics.zeta", "zeta tolerance to consider two values exactly equal", zeta, 0.0, 1e-1 );
         paramSet.addParameter( "passcodes", "ignored return codes separated by blanks (example: 2 3)", passcodes );
         paramSet.addParameter( "debug_filename", "if not empty, current instance is written to this file before every solve", debug_filename );
      }
   };

} // namespace bugger

#endif
