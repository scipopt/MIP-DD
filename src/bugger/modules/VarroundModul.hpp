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

#ifndef BUGGER_MODUL_VARROUND_HPP_
#define BUGGER_MODUL_VARROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"

#if BUGGER_HAVE_SCIP

#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"

#endif

namespace bugger {

   class VarroundModul : public BuggerModul {
   public:
      VarroundModul( ) : BuggerModul( ) {
         this->setName("varround");
      }

      bool
      initialize( ) override {
         return false;
      }

      SCIP_Bool SCIPisVarroundAdmissible( SCIP_VAR* var )
      {
         /* leave sparkling or fixed variables */
         return rint(var->obj) != var->obj || ( var->data.original.origdom.lb != var->data.original.origdom.ub && ( rint(var->data.original.origdom.lb) != var->data.original.origdom.lb || rint(var->data.original.origdom.ub) != var->data.original.origdom.ub ) );
      }

      ModulStatus
      execute(ScipInterface &iscip, const BuggerOptions &options, const Timer &timer) override {

         SCIP *scip = iscip.getSCIP( );
         SCIP_VAR *batch;
         int *inds;
         int batchsize;
         int nbatch;
         ModulStatus result = ModulStatus::kUnsuccesful;

         SCIP_VAR **vars;
         int nvars;
         SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr);

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int i = 0; i < nvars; ++i )
               if( SCIPisVarroundAdmissible(vars[i]) )
                  ++batchsize;

            batchsize /= options.nbatches;
         }

         ( SCIPallocBufferArray(scip, &inds, batchsize) );
         ( SCIPallocBufferArray(scip, &batch, batchsize) );
         nbatch = 0;

         for( int i = 0; i < nvars; ++i )
         {
            SCIP_VAR* var;

            var = vars[i];

            if( SCIPisVarroundAdmissible(var) )
            {
               inds[nbatch] = i;
               batch[nbatch].obj = var->obj;
               batch[nbatch].data.original.origdom.lb = var->data.original.origdom.lb;
               batch[nbatch].data.original.origdom.ub = var->data.original.origdom.ub;
               var->obj = rint(var->obj);
               var->unchangedobj = var->obj;
               var->data.original.origdom.lb = rint(var->data.original.origdom.lb);
               var->data.original.origdom.ub = rint(var->data.original.origdom.ub);

               if( iscip.exists_solution() )
               {
                  SCIP_Real solval;

                  solval = SCIPgetSolVal(scip, iscip.get_solution(), var);
                  //TODO: please check if this is correct
//                  presoldata->solution->obj -= batch[nbatch].obj * solval;
//                  presoldata->solution->obj += var->obj * solval;
                  SCIPsolUpdateVarObj(iscip.get_solution(), var, batch[nbatch].obj, var->obj);
                  var->data.original.origdom.lb = MIN(var->data.original.origdom.lb, SCIPfloor(scip, solval));
                  var->data.original.origdom.ub = MAX(var->data.original.origdom.ub, SCIPceil(scip, solval));
               }

               ++nbatch;
            }

            if( nbatch >= 1 && ( nbatch >= batchsize || i >= nvars - 1 ) )
            {
               if( iscip.runSCIP() != Status::kSuccess )
               {
                  for( int j = nbatch - 1; j >= 0; --j )
                  {
                     var = vars[inds[j]];
                     //TODO check also here
                     if( iscip.exists_solution() )
                        SCIPsolUpdateVarObj(iscip.get_solution(), var, var->obj, batch[nbatch].obj);
                     var->obj = batch[j].obj;
                     var->unchangedobj = batch[j].obj;
                     var->data.original.origdom.lb = batch[j].data.original.origdom.lb;
                     var->data.original.origdom.ub = batch[j].data.original.origdom.ub;
                  }
               }
               else
               {
                  for( int j = nbatch - 1; j >= 0; --j )
                  {
                     if( rint(batch[j].obj) != batch[j].obj )
                        nchgcoefs++;

                     if( rint(batch[j].data.original.origdom.lb) != batch[j].data.original.origdom.lb )
                        nchgcoefs++;

                     if( rint(batch[j].data.original.origdom.ub) != batch[j].data.original.origdom.ub )
                        nchgcoefs++;
                  }
                  result = ModulStatus::kSuccessful;
               }

               nbatch = 0;
            }
         }

         SCIPfreeBufferArray(scip, &batch);
         SCIPfreeBufferArray(scip, &inds);
         return result;
      }
   };


} // namespace bugger

#endif
