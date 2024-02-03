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

#ifndef _BUGGER_CORE_SOLUTION_HPP_
#define _BUGGER_CORE_SOLUTION_HPP_

#include "bugger/misc/Vec.hpp"

namespace bugger
{

enum class SolutionStatus
{
   kInfeasible,
   kUnbounded,
   kFeasible,
   kUnknown,
};

template <typename REAL>
class Solution
{
 public:
   SolutionStatus status;
   Vec<REAL> primal;
   Vec<REAL> ray;

   explicit Solution() : status( SolutionStatus::kUnknown ) {}

   Solution(const SolutionStatus& _status) : status( _status ) {}

   Solution(const Vec<REAL>& _primal) : status( SolutionStatus::kFeasible ), primal( _primal ) {}
};

} // namespace bugger

#endif
