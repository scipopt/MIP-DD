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

#ifndef BUGGER_MODUL_COEFFICIENT_HPP_
#define BUGGER_MODUL_COEFFICIENT_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/Status.hpp"
#if BUGGER_HAVE_SCIP
#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"
#endif
namespace bugger
{

class CoefficientModul : public BuggerModul
{
 public:
   CoefficientModul() : BuggerModul()
   {
      this->setName( "coefficient" );
   }

   bool
   initialize( ) override
   {
      return false;
   }

   static
   SCIP_Bool SCIPisCoefficientAdmissible(
         SCIP*                      scip,          /**< SCIP data structure */
         SCIP_CONS*                 cons           /**< SCIP constraint pointer */
   )
   {
      SCIP_CONSDATALINEAR consdata;
      int i;

      if( !SCIPconsIsChecked(cons) || strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), "linear") != 0 )
         return FALSE;

      (void)SCIPconsGetDataLinear(cons, &consdata);

      for( i = 0; i < consdata.nvars; ++i )
         if( SCIPisGE(scip, consdata.vars[i]->data.original.origdom.lb, consdata.vars[i]->data.original.origdom.ub) )
            return true;

      /* leave clean constraints */
      return false;
   }


   ModulStatus
   execute(Problem<double> &problem, Solution<double>& solution, bool solution_exists, const BuggerOptions &options, const Timer &timer) override
   {

      ModulStatus result = ModulStatus::kUnsuccesful;
//      SCIP* scip = iscip.getSCIP();
//      SCIP_CONS** conss = SCIPgetOrigConss(scip);
//      SCIP_CONSDATALINEAR* batch;
//
//      int** inds;
//      int nconss = SCIPgetNOrigConss(scip);
//      int batchsize = 1;
//      if( options.nbatches > 0 )
//      {
//         batchsize = options.nbatches - 1;
//
//         for( int i = nconss - 1; i >= 0; --i )
//            if( SCIPisCoefficientAdmissible(scip, conss[i]) )
//               ++batchsize;
//         batchsize /= options.nbatches;
//      }
//
//      ( SCIPallocBufferArray(scip, &inds, batchsize) );
//      ( SCIPallocBufferArray(scip, &batch, batchsize) );
//      int nbatch = 0;
//
//      for( int  i = nconss - 1; i >= 0; --i )
//      {
//         SCIP_CONS* cons;
//
//         cons = conss[i];
//
//         if( SCIPisCoefficientAdmissible(scip, cons) )
//         {
//            SCIP_CONSDATALINEAR consdata;
//            int j;
//            int k;
//
//            ( SCIPconsGetDataLinear(cons, &consdata) );
//
//            for( j = 0, batch[nbatch].nvars = 0; j < consdata.nvars; ++j )
//               if( SCIPisGE(scip, consdata.vars[j]->data.original.origdom.lb, consdata.vars[j]->data.original.origdom.ub) )
//                  ++batch[nbatch].nvars;
//
//            ( SCIPallocBufferArray(scip, &inds[nbatch], batch[nbatch].nvars + 1) );
//            ( SCIPallocBufferArray(scip, &batch[nbatch].vars, batch[nbatch].nvars) );
//            ( SCIPallocBufferArray(scip, &batch[nbatch].vals, batch[nbatch].nvars) );
//            inds[nbatch][0] = i;
//            batch[nbatch].lhs = consdata.lhs;
//            batch[nbatch].rhs = consdata.rhs;
//
//            for( j = consdata.nvars - 1, k = 0; k < batch[nbatch].nvars; --j )
//            {
//               if( SCIPisGE(scip, consdata.vars[j]->data.original.origdom.lb, consdata.vars[j]->data.original.origdom.ub) )
//               {
//                  SCIP_Real fixedval;
//
//                  inds[nbatch][k + 1] = j;
//                  batch[nbatch].vars[k] = consdata.vars[j];
//                  batch[nbatch].vals[k] = consdata.vals[j];
//
//                  if( iscip.exists_solution() )
//                  {
//                     if( SCIPvarIsIntegral(consdata.vars[j]) )
//                        fixedval = MAX(MIN(0.0, SCIPfloor(scip, consdata.vars[j]->data.original.origdom.ub)), SCIPceil(scip, consdata.vars[j]->data.original.origdom.lb));
//                     else
//                        fixedval = MAX(MIN(0.0, consdata.vars[j]->data.original.origdom.ub), consdata.vars[j]->data.original.origdom.lb);
//                  }
//                  else
//                  {
//                     if( SCIPvarIsIntegral(consdata.vars[j]) )
//                        fixedval = SCIPround(scip, SCIPgetSolVal(scip, iscip.get_solution(), consdata.vars[j]));
//                     else
//                        fixedval = SCIPgetSolVal(scip, iscip.get_solution(), consdata.vars[j]);
//                  }
//
//                  if( !SCIPisInfinity(scip, -consdata.lhs) )
//                     consdata.lhs -= consdata.vals[j] * fixedval;
//
//                  if( !SCIPisInfinity(scip, consdata.rhs) )
//                     consdata.rhs -= consdata.vals[j] * fixedval;
//
//                  --consdata.nvars;
//                  consdata.vars[j] = consdata.vars[consdata.nvars];
//                  consdata.vals[j] = consdata.vals[consdata.nvars];
//                  ++k;
//               }
//            }
//
//            ( SCIPconsSetDataLinear(cons, &consdata) );
//            ++nbatch;
//         }
//
//         if( nbatch >= 1 && ( nbatch >= batchsize || i <= 0 ) )
//         {
//            int j;
//            int k;
//
//            if( iscip.runSCIP() != Status::kSuccess )
//            {
//               for( j = nbatch - 1; j >= 0; --j )
//               {
//                  SCIP_CONSDATALINEAR consdata;
//
//                  cons = conss[inds[j][0]];
//                  ( SCIPconsGetDataLinear(cons, &consdata) );
//                  consdata.lhs = batch[j].lhs;
//                  consdata.rhs = batch[j].rhs;
//
//                  for( k = batch[j].nvars - 1; k >= 0; --k )
//                  {
//                     consdata.vars[inds[j][k + 1]] = batch[j].vars[k];
//                     consdata.vals[inds[j][k + 1]] = batch[j].vals[k];
//                     ++consdata.nvars;
//                  }
//
//                  ( SCIPconsSetDataLinear(cons, &consdata) );
//               }
//            }
//            else
//            {
//               for( j = 0; j < nbatch; ++j )
//               {
//                  for( k = 0; k < batch[j].nvars; ++k )
//                  {
//                     ( SCIPreleaseVar(scip, &batch[j].vars[k]) );
//                     nchgcoefs++;
//                  }
//               }
//               result = ModulStatus::kSuccessful;
//            }
//
//            for( j = nbatch - 1; j >= 0; --j )
//            {
//               SCIPfreeBufferArray(scip, &batch[j].vals);
//               SCIPfreeBufferArray(scip, &batch[j].vars);
//               SCIPfreeBufferArray(scip, &inds[j]);
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
