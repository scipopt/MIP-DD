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

#ifndef _BUGGER_MODUL_VARIABLE_HPP_
#define _BUGGER_MODUL_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#if BUGGER_HAVE_SCIP
#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"
#endif
namespace bugger
{

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

   SCIP_Bool SCIPisVariableAdmissible( SCIP *scip, SCIP_VAR *var )
   {
      /* keep restricted variables because they might be already fixed */
      return SCIPisLT(scip, var->data.original.origdom.lb, var->data.original.origdom.ub);
   }

   virtual ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) override
   {

      SCIP* scip = iscip.getSCIP();
      SCIP_VAR* batch;
      int* inds;
      int batchsize;
      int nbatch;
      int i;

      SCIP_VAR **vars;
      int nvars;
      SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr);

      if( options.nbatches <= 0 )
         batchsize = 1;
      else
      {
         batchsize = options.nbatches - 1;

         for( i = nvars - 1; i >= 0; --i )
            if( SCIPisVariableAdmissible(scip, vars[i]) )
               ++batchsize;

         batchsize /= options.nbatches;
      }

      ( SCIPallocBufferArray(scip, &inds, batchsize) );
      ( SCIPallocBufferArray(scip, &batch, batchsize) );
      nbatch = 0;

      for( i = nvars - 1; i >= 0; --i )
      {
         SCIP_VAR* var;

         var = vars[i];

         if( SCIPisVariableAdmissible(scip, var) )
         {
            SCIP_Real fixedval;

            inds[nbatch] = i;
            batch[nbatch].data.original.origdom.lb = var->data.original.origdom.lb;
            batch[nbatch].data.original.origdom.ub = var->data.original.origdom.ub;

            if( !iscip.exists_solution()  )
            {
               if( SCIPvarIsIntegral(var) )
                  fixedval = MAX(MIN(0.0, SCIPfloor(scip, var->data.original.origdom.ub)), SCIPceil(scip, var->data.original.origdom.lb));
               else
                  fixedval = MAX(MIN(0.0, var->data.original.origdom.ub), var->data.original.origdom.lb);
            }
            else
            {
               if( SCIPvarIsIntegral(var) )
                  fixedval = SCIPround(scip, SCIPgetSolVal(scip, iscip.get_solution(), var));
               else
                  fixedval = SCIPgetSolVal(scip, iscip.get_solution(), var);
            }

            var->data.original.origdom.lb = fixedval;
            var->data.original.origdom.ub = fixedval;
            ++nbatch;
         }

         if( nbatch >= 1 && ( nbatch >= batchsize || i <= 0 ) )
         {
            int j;

            if( iscip.runSCIP(iscip) == 0 )
            {
               for( j = nbatch - 1; j >= 0; --j )
               {
                  var = vars[inds[j]];
                  var->data.original.origdom.lb = batch[j].data.original.origdom.lb;
                  var->data.original.origdom.ub = batch[j].data.original.origdom.ub;
               }
            }
            else
            {
               //TODO: handle the return values
//               *nfixedvars += nbatch;
//               *result = SCIP_SUCCESS;
            }

            nbatch = 0;
         }
      }

      SCIPfreeBufferArray(scip, &batch);
      SCIPfreeBufferArray(scip, &inds);

      return ModulStatus::kDidNotRun;
   }
};


} // namespace bugger

#endif
