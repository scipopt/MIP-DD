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

#ifndef _EXACT_CORE_NODE_INFEASIBLE_HPP_
#define _EXACT_CORE_NODE_INFEASIBLE_HPP_

#include "exact/misc/Vec.hpp"

namespace exact
{


class Assumption
{
public:

   int index;
   double value;
   bool is_lower;

   Assumption( int _index, double _value, bool _is_lower )
   : index( _index ), value(_value), is_lower(_is_lower)
   {
   }
};

class FarkasProof
{
   SparseVectorView<double> farkas_row;
   SparseVectorView<double> farkas_var;
public:
   FarkasProof( SparseVectorView<double> _farkas_row, SparseVectorView<double> _farkas_var )
         : farkas_row( _farkas_row ), farkas_var(_farkas_var)
   {
   }
};


class NodeInfeasible
{
   Vec<Assumption> assumptions;
public:

   NodeInfeasible( Vec<Assumption> _assumptions )
       : assumptions( _assumptions )
   {
   }

};

} // namespace exact

#endif
