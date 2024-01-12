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

   public:

      SettingModul( const Message &_msg, const Num<double> &_num, const SolverStatus& _status, const SolverSettings& _target_solver_settings) : BuggerModul() {
         this->setName("setting");
         this->msg = _msg;
         this->num = _num;
         this->originalSolverStatus = _status;
         target_solver_settings = _target_solver_settings;
      }

      bool
      initialize( ) override {
         return false;
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution, bool solution_exists,
              const BuggerOptions &options, const Timer &timer) override {

         SolverSettings copy = SolverSettings(settings);

         Vec<std::pair<int, bool>> applied_bool { };
         Vec<std::pair<int, int>> applied_int { };
         Vec<std::pair<int, long>> applied_long { };
         Vec<std::pair<int, double>> applied_double { };
         Vec<std::pair<int, char>> applied_char { };
         Vec<std::pair<int, std::string>> applied_string { };

         Vec<std::pair<int, bool>> batches_bool { };
         Vec<std::pair<int, int>> batches_int { };
         Vec<std::pair<int, long>> batches_long { };
         Vec<std::pair<int, double>> batches_double { };
         Vec<std::pair<int, char>> batches_char { };
         Vec<std::pair<int, std::string>> batches_string { };
         int batches = 0;
         int batchsize = 1;

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
            for( int i = 0; i < target_solver_settings.getLongSettings().size(); i++)
            {
               assert(target_solver_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
               if(target_solver_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_solver_settings.getDoubleSettings().size(); i++)
            {
               assert(target_solver_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
               if(target_solver_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
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
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         batches_bool.reserve(batchsize);
         batches_int.reserve(batchsize);
         batches_long.reserve(batchsize);
         batches_double.reserve(batchsize);
         batches_char.reserve(batchsize);
         batches_string.reserve(batchsize);
         bool admissible = false;

         for( int i = 0; i < target_solver_settings.getBoolSettings().size(); i++)
         {
            assert(target_solver_settings.getBoolSettings()[i].first == settings.getBoolSettings()[i].first);
            if(target_solver_settings.getBoolSettings()[i].second != copy.getBoolSettings()[i].second)
            {
               copy.setBoolSettings(i, target_solver_settings.getBoolSettings( )[ i ].second);
               batches_bool.emplace_back(i, target_solver_settings.getBoolSettings( )[ i ].second);
               batches++;
               admissible = true;
            }

            if( batches != 0 && ( batches >= batchsize || ( i + 1 == target_solver_settings.getBoolSettings().size()
                                                           && target_solver_settings.getIntSettings().empty()
                                                           && target_solver_settings.getLongSettings().empty()
                                                           && target_solver_settings.getDoubleSettings().empty()
                                                           && target_solver_settings.getCharSettings().empty()
                                                           && target_solver_settings.getStringSettings().empty() ) ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
               }
               batches_bool.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_solver_settings.getIntSettings().size(); i++)
         {
            assert(target_solver_settings.getIntSettings()[i].first == settings.getIntSettings()[i].first);
            if(target_solver_settings.getIntSettings()[i].second != settings.getIntSettings()[i].second)
            {
               copy.setIntSettings(i, target_solver_settings.getIntSettings( )[ i ].second);
               batches_int.emplace_back(i, target_solver_settings.getIntSettings( )[ i ].second);
               batches++;
               admissible = true;
            }

            if( batches != 0 && ( batches >= batchsize || ( i + 1 == target_solver_settings.getIntSettings().size()
                                                           && target_solver_settings.getLongSettings().empty()
                                                           && target_solver_settings.getDoubleSettings().empty()
                                                           && target_solver_settings.getCharSettings().empty()
                                                           && target_solver_settings.getStringSettings().empty() ) ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
                  applied_int.insert(applied_int.end(), batches_int.begin(), batches_int.end());
               }
               batches_bool.clear();
               batches_int.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_solver_settings.getLongSettings().size(); i++)
         {
            assert(target_solver_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
            if(target_solver_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
            {
               copy.setLongSettings(i, target_solver_settings.getLongSettings( )[ i ].second);
               batches_long.emplace_back(i, target_solver_settings.getLongSettings( )[ i ].second);
               batches++;
               admissible = true;
            }

            if( batches != 0 && ( batches >= batchsize || ( i + 1 == target_solver_settings.getLongSettings().size()
                                                           && target_solver_settings.getDoubleSettings().empty()
                                                           && target_solver_settings.getCharSettings().empty()
                                                           && target_solver_settings.getStringSettings().empty() ) ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
                  applied_int.insert(applied_int.end(), batches_int.begin(), batches_int.end());
                  applied_long.insert(applied_long.end(), batches_long.begin(), batches_long.end());
               }
               batches_bool.clear();
               batches_int.clear();
               batches_long.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_solver_settings.getDoubleSettings().size(); i++)
         {
            assert(target_solver_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
            if(target_solver_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
            {
               copy.setDoubleSettings(i, target_solver_settings.getDoubleSettings( )[ i ].second);
               batches_double.emplace_back(i, target_solver_settings.getDoubleSettings( )[ i ].second);
               batches++;
               admissible = true;
            }

            if( batches != 0 && ( batches >= batchsize || ( i + 1 == target_solver_settings.getDoubleSettings().size()
                                                           && target_solver_settings.getCharSettings().empty()
                                                           && target_solver_settings.getStringSettings().empty() ) ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
                  applied_int.insert(applied_int.end(), batches_int.begin(), batches_int.end());
                  applied_long.insert(applied_long.end(), batches_long.begin(), batches_long.end());
                  applied_double.insert(applied_double.end(), batches_double.begin(), batches_double.end());
               }
               batches_bool.clear();
               batches_int.clear();
               batches_long.clear();
               batches_double.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_solver_settings.getCharSettings().size(); i++)
         {
            assert(target_solver_settings.getCharSettings()[i].first == settings.getCharSettings()[i].first);
            if(target_solver_settings.getCharSettings()[i].second != settings.getCharSettings()[i].second)
            {
               copy.setCharSettings(i, target_solver_settings.getCharSettings( )[ i ].second);
               batches++;
               admissible = true;
               batches_char.emplace_back(i, target_solver_settings.getCharSettings( )[ i ].second);
            }

            if( batches != 0 && ( batches >= batchsize || ( i + 1 == target_solver_settings.getCharSettings().size()
                                                           && target_solver_settings.getStringSettings().empty() ) ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
                  applied_int.insert(applied_int.end(), batches_int.begin(), batches_int.end());
                  applied_long.insert(applied_long.end(), batches_long.begin(), batches_long.end());
                  applied_double.insert(applied_double.end(), batches_double.begin(), batches_double.end());
                  applied_char.insert(applied_char.end(), batches_char.begin(), batches_char.end());
               }
               batches_bool.clear();
               batches_int.clear();
               batches_long.clear();
               batches_double.clear();
               batches_char.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_solver_settings.getStringSettings().size(); i++)
         {
            assert(target_solver_settings.getStringSettings()[i].first == settings.getStringSettings()[i].first);
            if(target_solver_settings.getStringSettings()[i].second != settings.getStringSettings()[i].second)
            {
               copy.setStringSettings(i, target_solver_settings.getStringSettings( )[ i ].second);
               batches++;
               admissible = true;
               batches_string.emplace_back(i, target_solver_settings.getStringSettings( )[ i ].second);

            }

            if( batches != 0 && ( batches >= batchsize || i + 1 == target_solver_settings.getStringSettings().size() ) )
            {
               auto solver = createSolver();
               solver->doSetUp(problem, copy, solution_exists, solution);
               if( call_solver(solver.get(), msg, originalSolverStatus, settings)!= BuggerStatus::kReproduced)
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
                  applied_int.insert(applied_int.end(), batches_int.begin(), batches_int.end());
                  applied_long.insert(applied_long.end(), batches_long.begin(), batches_long.end());
                  applied_double.insert(applied_double.end(), batches_double.begin(), batches_double.end());
                  applied_char.insert(applied_char.end(), batches_char.begin(), batches_char.end());
                  applied_string.insert(applied_string.end(), batches_string.begin(), batches_string.end());
               }
               batches_bool.clear();
               batches_int.clear();
               batches_long.clear();
               batches_double.clear();
               batches_char.clear();
               batches_string.clear();
               batches = 0;
            }
         }

         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if(applied_bool.empty() && applied_int.empty() && applied_long.empty() && applied_double.empty() && applied_char.empty() && applied_string.empty())
            return ModulStatus::kUnsuccesful;
         settings = copy;
         nchgsettings += applied_bool.size() + applied_int.size() + applied_long.size() + applied_double.size() + applied_char.size() + applied_string.size();
         return ModulStatus::kSuccessful;
      }

   private:

      SolverSettings
      reset(SolverSettings &settings, const Vec <std::pair<int, bool>>& applied_bool, const Vec <std::pair<int, int>>& applied_int,
            const Vec <std::pair<int, long>>& applied_long, const Vec <std::pair<int, double>>& applied_double,
            const Vec <std::pair<int, char>>& applied_char, const Vec <std::pair<int, std::string>>& applied_string) {
         auto reset = SolverSettings(settings);
         for( const auto &item: applied_bool )
            reset.setBoolSettings(item.first, item.second);
         for( const auto &item: applied_int )
            reset.setIntSettings(item.first, item.second);
         for( const auto &item: applied_long )
            reset.setLongSettings(item.first, item.second);
         for( const auto &item: applied_double )
            reset.setDoubleSettings(item.first, item.second);
         for( const auto &item: applied_char )
            reset.setCharSettings(item.first, item.second);
         for( const auto &item: applied_string)
            reset.setStringSettings(item.first, item.second);
         return reset;
      }

   };


} // namespace bugger

#endif
