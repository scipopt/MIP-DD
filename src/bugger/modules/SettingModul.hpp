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
#include "bugger/interfaces/Status.hpp"

namespace bugger {

   class SettingModul : public BuggerModul {
   public:
      SettingModul(const Message &_msg) : BuggerModul( ) {
         this->setName("setting");
         this->msg = _msg;
      }

      bool
      initialize( ) override {
         return false;
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;
         return result;
         auto solver = createSolver( );
         solver->parseParameters( );
         //TODO: think of a abortion criteria
         while( true )
         {

            solver->modify_parameters(options.nbatches);
            solver->doSetUp(problem, solution_exists, solution);
            //TODO: add batches for solving
            if( solver->run(msg) != Status::kFail )
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
   };


} // namespace bugger

#endif
