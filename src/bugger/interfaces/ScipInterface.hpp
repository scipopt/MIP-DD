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

#ifndef _BUGGER_INTERFACES_SCIP_INTERFACE_HPP_
#define _BUGGER_INTERFACES_SCIP_INTERFACE_HPP_

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "bugger/data/Problem.hpp"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"

namespace bugger {


   class ScipInterface {
   private:
      SCIP *scip;

   public:
      ScipInterface( ) : scip(nullptr) {
         if( SCIPcreate(&scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      SCIP*
      getSCIP()
      {
         return scip;
      }


      void
      parse(const std::string& filename)
      {
         assert(!filename.empty());
         SCIP_CALL_ABORT(SCIPincludeDefaultPlugins(scip));
         SCIP_CALL_ABORT(SCIPreadProb(scip, filename.c_str(), NULL));
      }

      void
      read_parameters(const std::string& scip_settings_file)
      {
         if(!scip_settings_file.empty())
            SCIP_CALL_ABORT( SCIPreadParams( scip, scip_settings_file.c_str() ) );
      }

      void
      read_solution(const std::string& solution_file)
      {
         if(!solution_file.empty())
            SCIP_CALL_ABORT( SCIPreadSol( scip, solution_file.c_str() ) );
      }

      void
      solve( ) {
         SCIP_RETCODE returncode = SCIPsolve(scip);
         assert(returncode == SCIP_OKAY);
      };


      ~ScipInterface( ) {
         if( scip != nullptr )
         {
            SCIP_RETCODE retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }
   };

} // namespace bugger

#endif
