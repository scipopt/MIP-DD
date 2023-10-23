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

#ifndef BUGGER_COEFFICIENT_VARIABLE_HPP_
#define BUGGER_COEFFICIENT_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/Status.hpp"

#if BUGGER_HAVE_SCIP
#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/sol.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"
#endif
namespace bugger
{

class ObjectiveModul : public BuggerModul
{
 public:
   ObjectiveModul() : BuggerModul()
   {
      this->setName( "objective" );
   }

   bool
   initialize( ) override
   {
      return false;
   }

   SCIP_Bool SCIPisObjectiveAdmissible(
         SCIP*                      scip,          /**< SCIP data structure */
         SCIP_VAR*                  var            /**< SCIP variable pointer */
   )
   {
      /* preserve restricted variables because they might be deleted anyway */
      return !SCIPisZero(scip, var->obj) && SCIPisLT(scip, var->data.original.origdom.lb, var->data.original.origdom.ub);
   }

   ModulStatus
   execute(Problem<double> &problem, Solution<double>& solution, bool solution_exists, const BuggerOptions &options, const Timer &timer) override
   {

      ModulStatus result = ModulStatus::kUnsuccesful;
//      SCIP* scip = iscip.getSCIP();
//      SCIP_VAR* batch;
//
//      int* inds;
//      int batchsize = 1;
//
//      SCIP_VAR **vars;
//      int nvars;
//      SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr);
//
//      if( options.nbatches > 0 )
//      {
//         batchsize = options.nbatches - 1;
//
//         for( int i = nvars - 1; i >= 0; --i )
//            if( SCIPisObjectiveAdmissible(scip, vars[i]) )
//               ++batchsize;
//         batchsize /= options.nbatches;
//      }
//
//      ( SCIPallocBufferArray(scip, &inds, batchsize) );
//      ( SCIPallocBufferArray(scip, &batch, batchsize) );
//      int nbatch = 0;
//
//
//      for( int i = nvars - 1; i >= 0; --i )
//      {
//         SCIP_VAR* var;
//
//         var = vars[i];
//
//         if( SCIPisObjectiveAdmissible(scip, var) )
//         {
//            inds[nbatch] = i;
//            batch[nbatch].obj = var->obj;
//
//            //TODO updating does not work in debug mode
//            if( iscip.exists_solution() )
//               SCIPsolUpdateVarObj(iscip.get_solution(), var, var->obj, 0);
//
//            var->obj = 0.0;
//            var->unchangedobj = 0.0;
//            ++nbatch;
//         }
//
//         if( nbatch >= 1 && ( nbatch >= batchsize || i <= 0 ) )
//         {
//            int j;
//
//            if( iscip.runSCIP() != Status::kSuccess )
//            {
//               // revert the change since they were not useful
//               for( j = nbatch - 1; j >= 0; --j )
//               {
//                  var = vars[inds[j]];
//                  var->obj = batch[j].obj;
//                  var->unchangedobj = batch[j].obj;
//                  //TODO check if this should revert the changes
//                  if( iscip.exists_solution())
//                     SCIPsolUpdateVarObj(iscip.get_solution(), var, 0, var->obj);
//               }
//            }
//            else
//            {
//               //TODO should we distinguish at this point?
//               nchgcoefs += nbatch;
//               result = ModulStatus::kSuccessful;
//            }
//
//            nbatch = 0;
//         }
//      }
//
//      SCIPfreeBufferArray(scip, &batch);
//      SCIPfreeBufferArray(scip, &inds);
      return result;
   }
};


} // namespace bugger

#endif
