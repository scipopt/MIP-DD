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

#ifndef BUGGER_MODUL_CONSROUND_HPP_
#define BUGGER_MODUL_CONSROUND_HPP_

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

class ConsRoundModul : public BuggerModul
{
 public:
   ConsRoundModul() : BuggerModul()
   {
      this->setName( "consround" );
   }

   bool
   initialize( ) override
   {
      return false;
   }

   SCIP_Bool SCIPisConsroundAdmissible(
         SCIP_CONS*                 cons           /**< SCIP constraint pointer */
   )
   {
      SCIP_CONSDATALINEAR consdata;
      int i;

      if( !SCIPconsIsChecked(cons) || strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), "linear") != 0 )
         return FALSE;

      (void)SCIPconsGetDataLinear(cons, &consdata);

      if( consdata.lhs != consdata.rhs && ( rint(consdata.lhs) != consdata.lhs || rint(consdata.rhs) != consdata.rhs ) )
         return TRUE;

      for( i = 0; i < consdata.nvars; ++i )
         if( consdata.vals[i] == 0.0 || rint(consdata.vals[i]) != consdata.vals[i] )
            return TRUE;

      /* leave sparkling or fixed constraints */
      return FALSE;
   }


   ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) override
   {

      SCIP* scip = iscip.getSCIP();
      ModulStatus result = ModulStatus::kUnsuccesful;
      SCIP_CONS**conss = SCIPgetOrigConss(scip);
      int nconss = SCIPgetNOrigConss(scip);
      SCIP_CONSDATALINEAR* batch;
      int* inds;

      int batchsize = 1;
      if( options.nbatches > 0 )
      {
         batchsize = options.nbatches - 1;

         for( int i = 0; i < nconss; ++i )
            if( SCIPisConsroundAdmissible(conss[i]) )
               ++batchsize;

         batchsize /= options.nbatches;
      }

      ( SCIPallocBufferArray(scip, &inds, batchsize) );
      ( SCIPallocBufferArray(scip, &batch, batchsize) );
      int nbatch = 0;

      for( int i = 0; i < nconss; ++i )
      {
         SCIP_CONS* cons;

         cons = conss[i];

         if( SCIPisConsroundAdmissible(cons) )
         {
            SCIP_CONSDATALINEAR consdata;
            int j;

            ( SCIPconsGetDataLinear(cons, &consdata) );
            ( SCIPallocBufferArray(scip, &batch[nbatch].vars, consdata.nvars) );
            ( SCIPallocBufferArray(scip, &batch[nbatch].vals, consdata.nvars) );
            inds[nbatch] = i;
            batch[nbatch].lhs = consdata.lhs;
            batch[nbatch].rhs = consdata.rhs;
            batch[nbatch].nvars = consdata.nvars;

            for( j = 0, consdata.nvars = 0; j < batch[nbatch].nvars; ++j )
            {
               batch[nbatch].vars[j] = consdata.vars[j];
               batch[nbatch].vals[j] = consdata.vals[j];
               consdata.vals[consdata.nvars] = rint(consdata.vals[j]);

               if( consdata.vals[consdata.nvars] != 0.0 )
                  consdata.vars[consdata.nvars++] = consdata.vars[j];
            }

            consdata.lhs = rint(consdata.lhs);
            consdata.rhs = rint(consdata.rhs);

            if( iscip.get_solution() != NULL )
            {
               SCIP_Real activity;

               ( SCIPconsSetDataLinear(cons, &consdata) );
               activity = SCIPgetActivityLinear(scip, cons, iscip.get_solution());
               consdata.lhs = MIN(consdata.lhs, SCIPfloor(scip, activity));
               consdata.rhs = MAX(consdata.rhs, SCIPceil(scip, activity));
            }

            ( SCIPconsSetDataLinear(cons, &consdata) );
            ++nbatch;
         }

         if( nbatch >= 1 && ( nbatch >= batchsize || i >= nconss - 1 ) )
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

                  for( consdata.nvars = 0; consdata.nvars < batch[j].nvars; ++consdata.nvars )
                  {
                     consdata.vars[consdata.nvars] = batch[j].vars[consdata.nvars];
                     consdata.vals[consdata.nvars] = batch[j].vals[consdata.nvars];
                  }

                  ( SCIPconsSetDataLinear(cons, &consdata) );
               }
            }
            else
            {
               for( j = nbatch - 1; j >= 0; --j )
               {
                  int k;

                  if( rint(batch[j].lhs) != batch[j].lhs )
                     nchgcoefs++;

                  if( rint(batch[j].rhs) != batch[j].rhs )
                     nchgcoefs++;

                  for( k = batch[j].nvars - 1; k >= 0; --k )
                  {
                     SCIP_Real val;

                     val = rint(batch[j].vals[k]);

                     if( batch[j].vals[k] == 0.0 || val != batch[j].vals[k] )
                     {
                        nchgcoefs++;

                        if( val == 0.0 )
                           ( SCIPreleaseVar(scip, &batch[j].vars[k]) );
                     }
                  }
               }

               result = ModulStatus::kSuccessful;
            }

            for( j = nbatch - 1; j >= 0; --j )
            {
               SCIPfreeBufferArray(scip, &batch[j].vals);
               SCIPfreeBufferArray(scip, &batch[j].vars);
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
