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

#ifndef BUGGER_MODUL_FIXING_HPP_
#define BUGGER_MODUL_FIXING_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/Status.hpp"
#if BUGGER_HAVE_SCIP
#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"
#endif
namespace bugger
{

class FixingModul : public BuggerModul
{
 public:
   FixingModul( const Message& _msg ) : BuggerModul()
   {
      this->setName( "fixing" );
      this->msg = _msg;
   }

   bool
   initialize( ) override
   {
      return false;
   }

   bool SCIPisFixingAdmissible(
         SCIP*                      scip,          /**< SCIP data structure */
         SCIP_VAR*                  var            /**< SCIP variable pointer */
   )
   {
      /* leave clean variables */
      return SCIPisGE(scip, var->data.original.origdom.lb, var->data.original.origdom.ub);
   }


   ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) override
   {

      SCIP* scip = iscip.getSCIP();
      ModulStatus result = ModulStatus::kUnsuccesful;
      SCIP_CONS** conss = SCIPgetOrigConss(scip);
      int nconss = SCIPgetNOrigConss(scip);
      int* inds;

      SCIP_VAR** batch;

      SCIP_VAR **vars;
      int nvars;
      SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr);

      bool* constrained;
      ( SCIPallocCleanBufferArray(scip, &constrained, nvars) );



      for( int  i = 0; i < nconss; ++i )
      {
         SCIP_VAR** consvars;
         SCIP_CONS* cons;
         bool success;
         int nconsvars;

         cons = conss[i];
         ( SCIPgetConsNVars(scip, cons, &nconsvars, &success) );

         if( !success )
            continue;

         ( SCIPallocBufferArray(scip, &consvars, nconsvars) );
         ( SCIPgetConsVars(scip, cons, consvars, nconsvars, &success) );

         if( success )
         {
            int j;

            for( j = 0; j < nconsvars; ++j )
               constrained[consvars[j]->probindex] = TRUE;
         }

         SCIPfreeBufferArray(scip, &consvars);
      }

      int batchsize = 1;
      if( options.nbatches > 0 )
      {
         batchsize = options.nbatches - 1;

         for( int i = nvars - 1; i >= 0; --i )
            if( !constrained[i] && SCIPisFixingAdmissible(scip, vars[i]) )
               ++batchsize;

         batchsize /= options.nbatches;
      }

      ( SCIPallocBufferArray(scip, &inds, batchsize) );
      ( SCIPallocBufferArray(scip, &batch, batchsize) );
      int nbatch = 0;

//      if( iscip.get_solution() != NULL )
//         obj = presoldata->solution->obj;

      for( int i = nvars - 1; i >= 0; --i )
      {
         SCIP_VAR* var;

         var = vars[i];

         if( constrained[i] )
            constrained[i] = FALSE;
         else if( SCIPisFixingAdmissible(scip, var) )
         {
            inds[nbatch] = i;
            batch[nbatch] = var;

//            if( iscip.get_solution() != NULL )
//               iscip.get_solution()->obj -= var->obj * SCIPgetSolVal(scip, iscip.get_solution(), var);

            vars[i] = vars[--nvars];
            vars[i]->probindex = i;
            var->probindex = -1;

            switch( SCIPvarGetType(var) )
            {
               //TODO: how to implement that from outside
               case SCIP_VARTYPE_BINARY:
                  --scip->origprob->nbinvars;
                  break;
               case SCIP_VARTYPE_INTEGER:
                  --scip->origprob->nintvars;
                  break;
               case SCIP_VARTYPE_IMPLINT:
                  --scip->origprob->nimplvars;
                  break;
               case SCIP_VARTYPE_CONTINUOUS:
                  --scip->origprob->ncontvars;
                  break;
               default:
                  SCIPerrorMessage("unknown variable type\n");
                  return ModulStatus::kUnsuccesful;
            }

            ++nbatch;
         }

         if( nbatch >= 1 && ( nbatch >= batchsize || i <= 0 ) )
         {
            if( iscip.runSCIP() != Status::kSuccess )
            {
               for( int j = nbatch - 1; j >= 0; --j )
               {
                  var = batch[j];
                  scip->origprob->vars[inds[j]] = var;
                  var->probindex = inds[j];
                  scip->origprob->vars[scip->origprob->nvars]->probindex = scip->origprob->nvars;
                  ++scip->origprob->nvars;

                  switch( SCIPvarGetType(var) )
                  {
                     case SCIP_VARTYPE_BINARY:
                        ++scip->origprob->nbinvars;
                        break;
                     case SCIP_VARTYPE_INTEGER:
                        ++scip->origprob->nintvars;
                        break;
                     case SCIP_VARTYPE_IMPLINT:
                        ++scip->origprob->nimplvars;
                        break;
                     case SCIP_VARTYPE_CONTINUOUS:
                        ++scip->origprob->ncontvars;
                        break;
                     default:
                        assert(false);
                        SCIPerrorMessage("unknown variable type\n");
                        return ModulStatus::kUnsuccesful;
                  }
               }

            }
            else
            {
               for( int j = 0; j < nbatch; ++j )
               {
                  ( SCIPreleaseVar(scip, &batch[j]) );
                  naggrvars++;
               }

               result = ModulStatus::kSuccessful;
            }

            nbatch = 0;
         }
      }

      SCIPfreeBufferArray(scip, &batch);
      SCIPfreeBufferArray(scip, &inds);
      SCIPfreeCleanBufferArray(scip, &constrained);
      return result;
   }
};


} // namespace bugger

#endif
