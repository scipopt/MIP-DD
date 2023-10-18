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

#ifndef BUGGER_MODUL_SETTING_HPP_
#define BUGGER_MODUL_SETTING_HPP_

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

class SettingModul : public BuggerModul
{
 public:
   SettingModul() : BuggerModul()
   {
      this->setName( "setting" );
   }

   bool
   initialize( ) override
   {
      return false;
   }

   SCIP_Bool SCIPisSettingAdmissible( SCIP_PARAM* param )
   {
      /* keep reading and writing settings because input and output is not monitored */
      return ( param->isfixed || !SCIPparamIsDefault(param) )
             && strncmp("display/", SCIPparamGetName(param), 8) != 0 && strncmp("limits/", SCIPparamGetName(param), 7) != 0
             && strncmp("reading/", SCIPparamGetName(param), 8) != 0 && strncmp("write/", SCIPparamGetName(param), 6) != 0;
   }

   ModulStatus
   execute( ScipInterface& iscip, const BuggerOptions& options, const Timer& timer ) override
   {

      SCIP* scip = iscip.getSCIP();
      SCIP_PARAM* batch;
      ModulStatus result = ModulStatus::kUnsuccesful;

      int* inds;
      int nparams = SCIPgetNParams(scip);
      SCIP_PARAM** params= SCIPgetParams(scip);
      int batchsize = 1;

      if( options.nbatches > 0 )
      {
         batchsize = options.nbatches - 1;

         for( int i = 0; i < nparams; ++i )
            if( SCIPisSettingAdmissible(params[i]) )
               ++batchsize;

         batchsize /= options.nbatches;
      }

      ( SCIPallocBufferArray(scip, &inds, batchsize) );
      ( SCIPallocBufferArray(scip, &batch, batchsize) );
      int nbatch = 0;

      for( int  i = 0; i < nparams; ++i )
      {
         SCIP_PARAM* param;

         param = params[i];

         if( SCIPisSettingAdmissible(param) )
         {
            inds[nbatch] = i;
            batch[nbatch].isfixed = param->isfixed;
            param->isfixed = FALSE;

            switch( SCIPparamGetType(param) )
            {
               case SCIP_PARAMTYPE_BOOL:
               {
                  SCIP_Bool* ptrbool;
                  ptrbool = (param->data.boolparam.valueptr == nullptr ? &param->data.boolparam.curvalue : param->data.boolparam.valueptr);
                  batch[nbatch].data.boolparam.curvalue = *ptrbool;
                  *ptrbool = param->data.boolparam.defaultvalue;
                  break;
               }
               case SCIP_PARAMTYPE_INT:
               {
                  int* ptrint;
                  ptrint = (param->data.intparam.valueptr == nullptr ? &param->data.intparam.curvalue : param->data.intparam.valueptr);
                  batch[nbatch].data.intparam.curvalue = *ptrint;
                  *ptrint = param->data.intparam.defaultvalue;
                  break;
               }
               case SCIP_PARAMTYPE_LONGINT:
               {
                  SCIP_Longint* ptrlongint;
                  ptrlongint = (param->data.longintparam.valueptr == nullptr ? &param->data.longintparam.curvalue : param->data.longintparam.valueptr);
                  batch[nbatch].data.longintparam.curvalue = *ptrlongint;
                  *ptrlongint = param->data.longintparam.defaultvalue;
                  break;
               }
               case SCIP_PARAMTYPE_REAL:
               {
                  SCIP_Real* ptrreal;
                  ptrreal = (param->data.realparam.valueptr == nullptr ? &param->data.realparam.curvalue : param->data.realparam.valueptr);
                  batch[nbatch].data.realparam.curvalue = *ptrreal;
                  *ptrreal = param->data.realparam.defaultvalue;
                  break;
               }
               case SCIP_PARAMTYPE_CHAR:
               {
                  char* ptrchar;
                  ptrchar = (param->data.charparam.valueptr == nullptr ? &param->data.charparam.curvalue : param->data.charparam.valueptr);
                  batch[nbatch].data.charparam.curvalue = *ptrchar;
                  *ptrchar = param->data.charparam.defaultvalue;
                  break;
               }
               case SCIP_PARAMTYPE_STRING:
               {
                  char** ptrstring;
                  ptrstring = (param->data.stringparam.valueptr == nullptr ? &param->data.stringparam.curvalue : param->data.stringparam.valueptr);
                  batch[nbatch].data.stringparam.curvalue = *ptrstring;
                  ( BMSduplicateMemoryArray(ptrstring, param->data.stringparam.defaultvalue, strlen(param->data.stringparam.defaultvalue)+1) );
                  break;
               }
               default:
                  SCIPerrorMessage("unknown parameter type\n");
                  return ModulStatus::kUnsuccesful;
            }

            ++nbatch;
         }

         if( nbatch >= 1 && ( nbatch >= batchsize || i >= nparams - 1 ) )
         {
            int j;

            if( iscip.runSCIP( ) == 0 )
            {
               for( j = nbatch - 1; j >= 0; --j )
               {
                  param = params[inds[j]];
                  param->isfixed = batch[j].isfixed;

                  switch( SCIPparamGetType(param) )
                  {
                     case SCIP_PARAMTYPE_BOOL:
                        *(param->data.boolparam.valueptr == nullptr ? &param->data.boolparam.curvalue : param->data.boolparam.valueptr) = batch[j].data.boolparam.curvalue;
                        break;
                     case SCIP_PARAMTYPE_INT:
                        *(param->data.intparam.valueptr == nullptr ? &param->data.intparam.curvalue : param->data.intparam.valueptr) = batch[j].data.intparam.curvalue;
                        break;
                     case SCIP_PARAMTYPE_LONGINT:
                        *(param->data.longintparam.valueptr == nullptr ? &param->data.longintparam.curvalue : param->data.longintparam.valueptr) = batch[j].data.longintparam.curvalue;
                        break;
                     case SCIP_PARAMTYPE_REAL:
                        *(param->data.realparam.valueptr == nullptr ? &param->data.realparam.curvalue : param->data.realparam.valueptr) = batch[j].data.realparam.curvalue;
                        break;
                     case SCIP_PARAMTYPE_CHAR:
                        *(param->data.charparam.valueptr == nullptr ? &param->data.charparam.curvalue : param->data.charparam.valueptr) = batch[j].data.charparam.curvalue;
                        break;
                     case SCIP_PARAMTYPE_STRING:
                     {
                        char** ptrstring;
                        ptrstring = (param->data.stringparam.valueptr == nullptr ? &param->data.stringparam.curvalue : param->data.stringparam.valueptr);
                        BMSfreeMemoryArrayNull(ptrstring);
                        *ptrstring = batch[j].data.stringparam.curvalue;
                        break;
                     }
                     default:
                        SCIPerrorMessage("unknown parameter type\n");
                        return ModulStatus::kUnsuccesful;
                  }
               }
            }
            else
            {
               for( j = nbatch - 1; j >= 0; --j )
               {
                  switch( SCIPparamGetType(params[inds[j]]) )
                  {
                     case SCIP_PARAMTYPE_BOOL:
                     case SCIP_PARAMTYPE_INT:
                     case SCIP_PARAMTYPE_LONGINT:
                     case SCIP_PARAMTYPE_REAL:
                     case SCIP_PARAMTYPE_CHAR:
                     case SCIP_PARAMTYPE_STRING:
                        BMSfreeMemoryArrayNull(&batch[j].data.stringparam.curvalue);
                        break;
                     default:
                        SCIPerrorMessage("unknown parameter type\n");
                        return ModulStatus::kUnsuccesful;
                  }
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
