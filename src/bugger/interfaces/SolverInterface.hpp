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

#ifndef _BUGGER_INTERFACES_SOLVER_INTERFACE_HPP_
#define _BUGGER_INTERFACES_SOLVER_INTERFACE_HPP_


#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "bugger/data/Problem.hpp"
#include "bugger/data/Solution.hpp"

namespace bugger {

   class SolverInterface {


   public:
      SolverInterface( ) { }

      virtual void
      doSetUp(const Problem<double> &problem, bool solution_exits, const Solution<double> sol) = 0;

      virtual
      Status run(const Message &msg) = 0;

      void
      modify_parameters(){}

      void parseParameters( ) {

      }
   };

} // namespace bugger

#endif
