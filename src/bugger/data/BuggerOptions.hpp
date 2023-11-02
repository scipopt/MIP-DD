/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
/*                     fuer Informationstechnik Berlin                       */
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

   int nrounds = -1;

   int nstages = -1;

   int nbatches = 0;

   double tlim = std::numeric_limits<double>::max();

   void
   addParameters( ParameterSet& paramSet )
   {
      paramSet.addParameter( "tlim", "bugger time limit", tlim, 0.0 );
      paramSet.addParameter( "initround", "initial bugger round", initround, 0 );
      paramSet.addParameter( "initstage", "initial bugger stage", initstage, 0 );
      paramSet.addParameter( "nrounds", "the maximum number of bugger rounds or -1 for no limit", nrounds, -1 );
      paramSet.addParameter( "nbatches", "the maximum number of batches or -1 for no limit", nbatches, -1 );
      paramSet.addParameter( "nstages", " maximum number of bugger stages or -1 for number of included bugger modules", nstages, -1 );
      paramSet.addParameter( "threads", "maximal number of threads to use (0: automatic)", threads, 0 );
   }

   bool
   runs_sequential() const
   {
      return threads == 1;
   }

};

} // namespace bugger

#endif
