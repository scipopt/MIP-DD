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

#ifndef _BUGGER_MODUL_VARIABLE_HPP_
#define _BUGGER_MODUL_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"

namespace bugger
{

template <typename REAL>
class VariableModul : public BuggerModul
{
 public:
   VariableModul() : BuggerModul()
   {
      this->setName( "variable" );
   }

   bool
   initialize( ) override
   {
      return false;
   }

   virtual ModulStatus
   execute( const Timer& timer ) override
   {

      return ModulStatus::kDidNotRun;
   }
};


} // namespace bugger

#endif
