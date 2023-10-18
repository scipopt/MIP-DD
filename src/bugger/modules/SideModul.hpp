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

#ifndef BUGGER_SIDE_VARIABLE_HPP_
#define BUGGER_SIDE_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#if BUGGER_HAVE_SCIP
#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"
#include "scip/cons_linear.h"
#endif
namespace bugger
{

class SideModul : public BuggerModul
{
 public:
   SideModul() : BuggerModul()
   {
      this->setName( "side" );
   }

   bool
   initialize( ) override
   {
      return false;
   }


   SCIP_Bool SCIPisSideAdmissible(
         SCIP*                      scip,          /**< SCIP data structure */
         SCIP_CONS*                 cons           /**< SCIP constraint pointer */
   )
   {
      SCIP_CONSDATALINEAR consdata;

      (void)SCIPconsGetDataLinear(cons, &consdata);

      /* preserve restricted constraints because they might be already fixed */
      return SCIPconsIsChecked(cons) && strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), "linear") == 0
             && SCIPisLT(scip, consdata.lhs, consdata.rhs);
   }


   virtual ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) override
   {
      SCIP* scip = iscip.getSCIP();
      SCIP_CONS** conss = SCIPgetOrigConss(scip);
      int nconss = SCIPgetNOrigConss(scip);
      SCIP_CONSDATALINEAR* batch;
      int* inds;
      ModulStatus result = ModulStatus::kUnsuccesful;

      int batchsize = 1;
      if( options.nbatches > 0 )
      {
         batchsize = options.nbatches - 1;

         for( int i = nconss - 1; i >= 0; --i )
            if( SCIPisSideAdmissible(scip, conss[i]) )
               ++batchsize;

         batchsize /= options.nbatches;
      }

      ( SCIPallocBufferArray(scip, &inds, batchsize) );
      ( SCIPallocBufferArray(scip, &batch, batchsize) );
      int nbatch = 0;

      for( int i = nconss - 1; i >= 0; --i )
      {
         SCIP_CONS* cons;

         cons = conss[i];

         if( SCIPisSideAdmissible(scip, cons) )
         {
            SCIP_CONSDATALINEAR consdata;
            SCIP_VAR** vars;
            SCIP_Real* vals;
            SCIP_Real fixedval;
            SCIP_Bool integral;
            int nvals;
            int j;

            ( SCIPconsGetDataLinear(cons, &consdata) );
            inds[nbatch] = i;
            batch[nbatch].lhs = consdata.lhs;
            batch[nbatch].rhs = consdata.rhs;
            vars = SCIPgetVarsLinear(scip, cons);
            vals = SCIPgetValsLinear(scip, cons);
            nvals = SCIPgetNVarsLinear(scip, cons);
            integral = TRUE;

            for( j = 0; j < nvals; ++j )
            {
               if( !SCIPvarIsIntegral(vars[j]) || !SCIPisIntegral(scip, vals[j]) )
               {
                  integral = FALSE;

                  break;
               }
            }

            if( iscip.get_solution() == nullptr )
            {
               if( integral )
                  fixedval = MAX(MIN(0.0, SCIPfloor(scip, consdata.rhs)), SCIPceil(scip, consdata.lhs));
               else
                  fixedval = MAX(MIN(0.0, consdata.rhs), consdata.lhs);
            }
            else
            {
               if( integral )
                  fixedval = SCIPround(scip, SCIPgetActivityLinear(scip, cons, iscip.get_solution()));
               else
                  fixedval = SCIPgetActivityLinear(scip, cons, iscip.get_solution());
            }

            consdata.lhs = fixedval;
            consdata.rhs = fixedval;
            ( SCIPconsSetDataLinear(cons, &consdata) );
            ++nbatch;
         }

         if( nbatch >= 1 && ( nbatch >= batchsize || i <= 0 ) )
         {
            int j;

            if( iscip.runSCIP() == 0 )
            {
               for( j = nbatch - 1; j >= 0; --j )
               {
                  SCIP_CONSDATALINEAR consdata;

                  cons = conss[inds[j]];
                  ( SCIPconsGetDataLinear(cons, &consdata) );
                  consdata.lhs = batch[j].lhs;
                  consdata.rhs = batch[j].rhs;
                  ( SCIPconsSetDataLinear(cons, &consdata) );
               }
            }
            else
            {
               nchgsides += nbatch;
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
