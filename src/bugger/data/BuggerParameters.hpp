/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_DATA_BUGGERPARAMETERS_HPP__
#define __BUGGER_DATA_BUGGERPARAMETERS_HPP__

#include "bugger/misc/ParameterSet.hpp"


namespace bugger {

   struct BuggerParameters
   {
      int mode = -1;
      long long expenditure = -1;
      long long nbatches = 2;
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
         paramSet.addParameter( "expenditure", "calculate the number of batches by ceiled division of the solving effort defined in the solver interface (-1: use original, 0: keep batches)", expenditure, -1 );
         paramSet.addParameter( "nbatches", "maximum number of batches or 0 for singleton batches", nbatches, 0 );
         paramSet.addParameter( "initround", "initial bugger round or -1 for last round", initround, -1 );
         paramSet.addParameter( "initstage", "initial bugger stage or -1 for last stage", initstage, -1 );
         paramSet.addParameter( "maxrounds", "maximum number of bugger rounds or -1 for no limit", maxrounds, -1 );
         paramSet.addParameter( "maxstages", "maximum number of bugger stages or -1 for number of modifiers", maxstages, -1 );
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
