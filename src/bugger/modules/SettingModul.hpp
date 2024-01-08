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
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {



   class SettingModul : public BuggerModul {

//      SolverSettings target_solver_settings;

   public:

      SettingModul( const Message &_msg, const Num<double> &_num) : BuggerModul() {
         this->setName("setting");
         this->msg = _msg;
         this->num = _num;
//         target_solver_settings = SolverSettings{};
      }

      bool
      initialize( ) override {
         return false;
      }

      void
      set_target_settings(SolverSettings _target_solver_settings)
      {
//         target_solver_settings = _target_solver_settings;
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const SolverSettings& settings, const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;
         return result;
         auto solver = createSolver( );
         //TODO: think of a abortion criteria
         while( true )
         {

            solver->modify_parameters(options.nbatches);
            solver->doSetUp(problem, solution_exists, solution);
            //TODO: add batches for solving
            if( solver->run(msg,originalSolverStatus, settings) != BuggerStatus::kFail )
            {
               //TODO: ignore new parameter file
            }
            else
            {
               //TODO: push back together
               //store new parameter file
               result = ModulStatus::kSuccessful;
            }
         }

         return result;
      }

//      void modify_parameters(int nbatches) override {
//         SCIP_PARAM *batch;
//
//         int *inds;
//         int nparams = SCIPgetNParams(scip);
//         SCIP_PARAM **params = SCIPgetParams(scip);
//         int batchsize = 1;
//
//         if( nbatches > 0 )
//         {
//            batchsize = nbatches - 1;
//
//            for( int i = 0; i < nparams; ++i )
//               if( isSettingAdmissible(params[ i ]))
//                  ++batchsize;
//
//            batchsize /= nbatches;
//         }
//
//         (SCIPallocBufferArray(scip, &inds, batchsize));
//         (SCIPallocBufferArray(scip, &batch, batchsize));
//         int nbatch = 0;
//
//         for( int i = 0; i < nparams; ++i )
//         {
//            SCIP_PARAM *param;
//
//            param = params[ i ];
//
//            if( isSettingAdmissible(param))
//            {
//               inds[ nbatch ] = i;
//               batch[ nbatch ].isfixed = param->isfixed;
//               param->isfixed = FALSE;
//
//               switch( SCIPparamGetType(param))
//               {
//                  case SCIP_PARAMTYPE_BOOL:
//                  {
//                     SCIP_Bool *ptrbool;
//                     ptrbool = ( param->data.boolparam.valueptr == nullptr ? &param->data.boolparam.curvalue
//                                                                           : param->data.boolparam.valueptr );
//                     batch[ nbatch ].data.boolparam.curvalue = *ptrbool;
//                     *ptrbool = param->data.boolparam.defaultvalue;
//                     break;
//                  }
//                  case SCIP_PARAMTYPE_INT:
//                  {
//                     int *ptrint;
//                     ptrint = ( param->data.intparam.valueptr == nullptr ? &param->data.intparam.curvalue
//                                                                         : param->data.intparam.valueptr );
//                     batch[ nbatch ].data.intparam.curvalue = *ptrint;
//                     *ptrint = param->data.intparam.defaultvalue;
//                     break;
//                  }
//                  case SCIP_PARAMTYPE_LONGINT:
//                  {
//                     SCIP_Longint *ptrlongint;
//                     ptrlongint = ( param->data.longintparam.valueptr == nullptr ? &param->data.longintparam.curvalue
//                                                                                 : param->data.longintparam.valueptr );
//                     batch[ nbatch ].data.longintparam.curvalue = *ptrlongint;
//                     *ptrlongint = param->data.longintparam.defaultvalue;
//                     break;
//                  }
//                  case SCIP_PARAMTYPE_REAL:
//                  {
//                     SCIP_Real *ptrreal;
//                     ptrreal = ( param->data.realparam.valueptr == nullptr ? &param->data.realparam.curvalue
//                                                                           : param->data.realparam.valueptr );
//                     batch[ nbatch ].data.realparam.curvalue = *ptrreal;
//                     *ptrreal = param->data.realparam.defaultvalue;
//                     break;
//                  }
//                  case SCIP_PARAMTYPE_CHAR:
//                  {
//                     char *ptrchar;
//                     ptrchar = ( param->data.charparam.valueptr == nullptr ? &param->data.charparam.curvalue
//                                                                           : param->data.charparam.valueptr );
//                     batch[ nbatch ].data.charparam.curvalue = *ptrchar;
//                     *ptrchar = param->data.charparam.defaultvalue;
//                     break;
//                  }
//                  case SCIP_PARAMTYPE_STRING:
//                  {
//                     char **ptrstring;
//                     ptrstring = ( param->data.stringparam.valueptr == nullptr ? &param->data.stringparam.curvalue
//                                                                               : param->data.stringparam.valueptr );
//                     batch[ nbatch ].data.stringparam.curvalue = *ptrstring;
//                     (BMSduplicateMemoryArray(ptrstring, param->data.stringparam.defaultvalue,
//                                              strlen(param->data.stringparam.defaultvalue) + 1));
//                     break;
//                  }
//                  default:
//                     SCIPerrorMessage("unknown parameter type\n");
//               }
//
//               ++nbatch;
//            }
//
//         }
//
//      }
   };


} // namespace bugger

#endif
