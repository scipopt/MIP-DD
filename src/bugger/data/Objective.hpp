/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
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

#ifndef _BUGGER_CORE_OBJECTIVE_HPP_
#define _BUGGER_CORE_OBJECTIVE_HPP_

#include "bugger/misc/Vec.hpp"

namespace bugger
{

/// type to store an objective function
template <typename REAL>
struct Objective
{
   /// dense vector of objective coefficients
   Vec<REAL> coefficients;

   /// offset of objective function
   REAL offset;

   /// sense of objective function
   bool sense;

   template <typename Archive>
   void
   serialize( Archive& ar, const unsigned int version )
   {
      ar& coefficients;
      ar& offset;
      ar& sense;
   }
};

} // namespace bugger

#endif
