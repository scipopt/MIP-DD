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
   private:
      SolverSettings target_solver_settings ;
      bool initialized = false;
   public:
      SettingModul( const Message &_msg, const Num<double> &_num) : BuggerModul() {
         this->setName("setting");
         this->msg = _msg;
         this->num = _num;

      }

      bool
      initialize( ) override {
         return false;
      }


      virtual void
      set_target_settings(SolverSettings& _target_solver_settings) override
      {
         target_solver_settings = _target_solver_settings;
         initialized = true;
      }


      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const SolverSettings& settings, const Timer &timer) override {

         if(!initialized)
            return ModulStatus::kUnsuccesful;
         ModulStatus result = ModulStatus::kUnsuccesful;
         SolverSettings copy = SolverSettings(settings);

         Vec<int> batches { };
         int batchsize = 1;

         auto solver = createSolver( );

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = 0; i < target_solver_settings.getBoolSettings().size(); i++)
            {
               assert(target_solver_settings.getBoolSettings()[i].first == settings.getBoolSettings()[i].first);
               if(target_solver_settings.getBoolSettings()[i].second != settings.getBoolSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getIntSettings().size(); i++)
            {
               assert(target_solver_settings.getIntSettings()[i].first == settings.getIntSettings()[i].first);
               if(target_solver_settings.getIntSettings()[i].second != settings.getIntSettings()[i].second)
               ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getDoubleSettings().size(); i++)
            {
               assert(target_solver_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
               if(target_solver_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getLongSettings().size(); i++)
            {
               assert(target_solver_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
               if(target_solver_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getCharSettings().size(); i++)
            {
               assert(target_solver_settings.getCharSettings()[i].first == settings.getCharSettings()[i].first);
               if(target_solver_settings.getCharSettings()[i].second != settings.getCharSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getStringSettings().size(); i++)
            {
               assert(target_solver_settings.getStringSettings()[i].first == settings.getStringSettings()[i].first);
               if(target_solver_settings.getStringSettings()[i].second != settings.getStringSettings()[i].second)
                  ++batchsize;
            }
            batchsize /= options.nbatches;
         }

         return ModulStatus::kUnsuccesful;


         batches.reserve(batchsize);
         bool admissible = false;

         for( int i = 0; i < target_solver_settings.getBoolSettings().size(); i++)
         {
            assert(target_solver_settings.getBoolSettings()[i].first == settings.getBoolSettings()[i].first);
            if(target_solver_settings.getBoolSettings()[i].second != copy.getBoolSettings()[i].second)
            {
               //TODO: overwrite
//               copy.getBoolSettings( )[ i ] = target_solver_settings.getBoolSettings( )[ i ];
            }
            if( !batches.empty() && ( batches.size() >= batchsize ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus, copy) == BuggerStatus::kSuccess )
               {
                  copy = SolverSettings(settings);
                  //TODO restore
//                  for( const auto &item: applied_reductions )
//                     copy.getObjective( ).coefficients[ item ] = 0.0;
               }
               else
               {
                  //TODO:
               }
               batches.clear();
            }
         }
         for( int i = 0; i < target_solver_settings.getIntSettings().size(); i++)
         {
            assert(target_solver_settings.getIntSettings()[i].first == settings.getIntSettings()[i].first);
            if(target_solver_settings.getIntSettings()[i].second != settings.getIntSettings()[i].second)
               ++batchsize;
         }
         for( int i = 0; i < target_solver_settings.getDoubleSettings().size(); i++)
         {
            assert(target_solver_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
            if(target_solver_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
               ++batchsize;
         }
         for( int i = 0; i < target_solver_settings.getLongSettings().size(); i++)
         {
            assert(target_solver_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
            if(target_solver_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
               ++batchsize;
         }
         for( int i = 0; i < target_solver_settings.getCharSettings().size(); i++)
         {
            assert(target_solver_settings.getCharSettings()[i].first == settings.getCharSettings()[i].first);
            if(target_solver_settings.getCharSettings()[i].second != settings.getCharSettings()[i].second)
               ++batchsize;
         }
         for( int i = 0; i < target_solver_settings.getStringSettings().size(); i++)
         {
            assert(target_solver_settings.getStringSettings()[i].first == settings.getStringSettings()[i].first);
            if(target_solver_settings.getStringSettings()[i].second != settings.getStringSettings()[i].second)
               ++batchsize;
         }

         if(!admissible)
            return ModulStatus::kDidNotRun;
//         if( applied_reductions.empty() )
//            return ModulStatus::kUnsuccesful;
//         else
//         {
//            problem = copy;
//            nchgcoefs += applied_reductions.size();
//            return ModulStatus::kSuccessful;
//         }
      }

   };


} // namespace bugger

#endif
