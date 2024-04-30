/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
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

#ifndef __BUGGER_MODULE_SETTING_HPP__
#define __BUGGER_MODULE_SETTING_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger
{
   class SettingModul : public BuggerModul
   {
   public:

      SolverSettings target_settings;

      explicit SettingModul(const Message& _msg, const Num<double>& _num, const BuggerParameters& _parameters,
                            std::shared_ptr<SolverFactory>& _factory) : BuggerModul(_msg, _num, _parameters, _factory)
      {
         this->setName("setting");
      }

   private:

      ModulStatus
      execute(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution) override
      {
         int batchsize = 1;

         if( parameters.nbatches > 0 )
         {
            batchsize = parameters.nbatches - 1;
            for( int i = 0; i < target_settings.getBoolSettings().size(); i++)
            {
               assert(target_settings.getBoolSettings()[i].first == settings.getBoolSettings()[i].first);
               if( target_settings.getBoolSettings()[i].second != settings.getBoolSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_settings.getIntSettings().size(); i++)
            {
               assert(target_settings.getIntSettings()[i].first == settings.getIntSettings()[i].first);
               if( target_settings.getIntSettings()[i].second != settings.getIntSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_settings.getLongSettings().size(); i++)
            {
               assert(target_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
               if( target_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_settings.getDoubleSettings().size(); i++)
            {
               assert(target_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
               if( target_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_settings.getCharSettings().size(); i++)
            {
               assert(target_settings.getCharSettings()[i].first == settings.getCharSettings()[i].first);
               if( target_settings.getCharSettings()[i].second != settings.getCharSettings()[i].second)
                  ++batchsize;
            }
            for( int i = 0; i < target_settings.getStringSettings().size(); i++)
            {
               assert(target_settings.getStringSettings()[i].first == settings.getStringSettings()[i].first);
               if( target_settings.getStringSettings()[i].second != settings.getStringSettings()[i].second)
                  ++batchsize;
            }
            if( batchsize == parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= parameters.nbatches;
         }

         bool admissible = false;
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
         batches_bool.reserve(batchsize);
         batches_int.reserve(batchsize);
         batches_long.reserve(batchsize);
         batches_double.reserve(batchsize);
         batches_char.reserve(batchsize);
         batches_string.reserve(batchsize);
         int batches = 0;

         for( int i = 0; i < target_settings.getBoolSettings().size(); ++i )
         {
            assert(target_settings.getBoolSettings()[i].first == settings.getBoolSettings()[i].first);
            if( target_settings.getBoolSettings()[i].second != copy.getBoolSettings()[i].second)
            {
               admissible = true;
               copy.setBoolSettings(i, target_settings.getBoolSettings( )[ i ].second);
               batches_bool.emplace_back(i, target_settings.getBoolSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || ( i + 1 == target_settings.getBoolSettings().size()
                                                            && target_settings.getIntSettings().empty()
                                                            && target_settings.getLongSettings().empty()
                                                            && target_settings.getDoubleSettings().empty()
                                                            && target_settings.getCharSettings().empty()
                                                            && target_settings.getStringSettings().empty() ) ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
                  copy = reset(settings, applied_bool, applied_int, applied_long, applied_double, applied_char, applied_string);
               else
               {
                  applied_bool.insert(applied_bool.end(), batches_bool.begin(), batches_bool.end());
               }
               batches_bool.clear();
               batches = 0;
            }
         }
         for( int i = 0; i < target_settings.getIntSettings().size(); ++i )
         {
            assert(target_settings.getIntSettings()[i].first == settings.getIntSettings()[i].first);
            if( target_settings.getIntSettings()[i].second != settings.getIntSettings()[i].second)
            {
               admissible = true;
               copy.setIntSettings(i, target_settings.getIntSettings( )[ i ].second);
               batches_int.emplace_back(i, target_settings.getIntSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || ( i + 1 == target_settings.getIntSettings().size()
                                                            && target_settings.getLongSettings().empty()
                                                            && target_settings.getDoubleSettings().empty()
                                                            && target_settings.getCharSettings().empty()
                                                            && target_settings.getStringSettings().empty() ) ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
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
         for( int i = 0; i < target_settings.getLongSettings().size(); ++i )
         {
            assert(target_settings.getLongSettings()[i].first == settings.getLongSettings()[i].first);
            if( target_settings.getLongSettings()[i].second != settings.getLongSettings()[i].second)
            {
               admissible = true;
               copy.setLongSettings(i, target_settings.getLongSettings( )[ i ].second);
               batches_long.emplace_back(i, target_settings.getLongSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || ( i + 1 == target_settings.getLongSettings().size()
                                                            && target_settings.getDoubleSettings().empty()
                                                            && target_settings.getCharSettings().empty()
                                                            && target_settings.getStringSettings().empty() ) ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
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
         for( int i = 0; i < target_settings.getDoubleSettings().size(); ++i )
         {
            assert(target_settings.getDoubleSettings()[i].first == settings.getDoubleSettings()[i].first);
            if( target_settings.getDoubleSettings()[i].second != settings.getDoubleSettings()[i].second)
            {
               admissible = true;
               copy.setDoubleSettings(i, target_settings.getDoubleSettings( )[ i ].second);
               batches_double.emplace_back(i, target_settings.getDoubleSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || ( i + 1 == target_settings.getDoubleSettings().size()
                                                            && target_settings.getCharSettings().empty()
                                                            && target_settings.getStringSettings().empty() ) ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
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
         for( int i = 0; i < target_settings.getCharSettings().size(); ++i )
         {
            assert(target_settings.getCharSettings()[i].first == settings.getCharSettings()[i].first);
            if( target_settings.getCharSettings()[i].second != settings.getCharSettings()[i].second)
            {
               admissible = true;
               copy.setCharSettings(i, target_settings.getCharSettings( )[ i ].second);
               batches_char.emplace_back(i, target_settings.getCharSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || ( i + 1 == target_settings.getCharSettings().size()
                                                            && target_settings.getStringSettings().empty() ) ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
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
         for( int i = 0; i < target_settings.getStringSettings().size(); ++i )
         {
            assert(target_settings.getStringSettings()[i].first == settings.getStringSettings()[i].first);
            if( target_settings.getStringSettings()[i].second != settings.getStringSettings()[i].second)
            {
               admissible = true;
               copy.setStringSettings(i, target_settings.getStringSettings( )[ i ].second);
               batches_string.emplace_back(i, target_settings.getStringSettings( )[ i ].second);
               ++batches;
            }

            if( batches >= 1 && ( batches >= batchsize || i + 1 == target_settings.getStringSettings().size() ) )
            {
               if( call_solver(copy, problem, solution) == BuggerStatus::kOkay )
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

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_bool.empty() && applied_int.empty() && applied_long.empty() && applied_double.empty() && applied_char.empty() && applied_string.empty() )
            return ModulStatus::kUnsuccesful;
         settings = copy;
         nchgsettings += applied_bool.size() + applied_int.size() + applied_long.size() + applied_double.size() + applied_char.size() + applied_string.size();
         return ModulStatus::kSuccessful;
      }

      SolverSettings
      reset(const SolverSettings& settings,
            const Vec<std::pair<int, bool>>& applied_bool, const Vec<std::pair<int, int>>& applied_int,
            const Vec<std::pair<int, long>>& applied_long, const Vec<std::pair<int, double>>& applied_double,
            const Vec<std::pair<int, char>>& applied_char, const Vec<std::pair<int, std::string>>& applied_string) const
      {
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
         for( const auto &item: applied_string )
            reset.setStringSettings(item.first, item.second);
         return reset;
      }
   };

} // namespace bugger

#endif
