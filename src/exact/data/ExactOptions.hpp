/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    PaPILO --- Parallel Presolve for Integer and Linear Optimization       */
/*                                                                           */
/* Copyright (C) 2020-2023 Konrad-Zuse-Zentrum                               */
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

#ifndef _EXACT_CORE_PRESOLVE_OPTIONS_HPP_
#define _EXACT_CORE_PRESOLVE_OPTIONS_HPP_

#include "exact/misc/ParameterSet.hpp"
#include <type_traits>

namespace exact
{

struct ExactOptions
{

   int threads = 0;

   unsigned int randomseed = 0;

   double tlim = std::numeric_limits<double>::max();

   void
   addParameters( ParameterSet& paramSet )
   {

      paramSet.addParameter( "presolve.randomseed", "random seed value",
                             randomseed );
      paramSet.addParameter( "presolve.tlim", "time limit for presolve", tlim,
                             0.0 );
      paramSet.addParameter( "presolve.threads",
                             "maximal number of threads to use (0: automatic)",
                             threads, 0 );
   }

   bool
   runs_sequential() const
   {
      return threads == 1;
   }

};

} // namespace exact

#endif
