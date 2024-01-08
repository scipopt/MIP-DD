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

#ifndef _BUGGER_CORE_SOLVER_SETTINGS_HPP_
#define _BUGGER_CORE_SOLVER_SETTINGS_HPP_

#include "bugger/misc/Vec.hpp"

namespace bugger {

   class SolverSettings {

   private:
      Vec<std::pair<std::string, bool>> bool_settings;
      Vec<std::pair<std::string, int>> int_settings;
      Vec<std::pair<std::string, long>> long_settings;
      Vec<std::pair<std::string, double>> double_settings;
      Vec<std::pair<std::string, char>> char_settings;
      Vec<std::pair<std::string, std::string>> string_settings;

   public:

      SolverSettings()            :
            bool_settings( { } ),
            int_settings( { } ),
            long_settings( { }),
            double_settings( { }),
            char_settings( { }),
            string_settings( { } ) {
      };

      SolverSettings(Vec<std::pair<std::string, bool>> &_bool_settings,
                     Vec<std::pair<std::string, int>> &_int_settings,
                     Vec<std::pair<std::string, long>> &_long_settings,
                     Vec<std::pair<std::string, double>> &_double_settings,
                     Vec<std::pair<std::string, char>> &_char_settings,
                     Vec<std::pair<std::string, std::string>> &_string_settings
      )
            :
            bool_settings(_bool_settings),
            int_settings(_int_settings),
            long_settings(_long_settings),
            double_settings(_double_settings),
            char_settings(_char_settings),
            string_settings(_string_settings) {
      };

      const Vec<std::pair<std::string, bool>> &getBoolSettings( ) const {
         return bool_settings;
      }

      const Vec<std::pair<std::string, int>> &getIntSettings( ) const {
         return int_settings;
      }

      const Vec<std::pair<std::string, long>> &getLongSettings( ) const {
         return long_settings;
      }

      const Vec<std::pair<std::string, double>> &getDoubleSettings( ) const {
         return double_settings;
      }

      const Vec<std::pair<std::string, char>> &getCharSettings( ) const {
         return char_settings;
      }

      const Vec<std::pair<std::string, std::string>> &getStringSettings( ) const {
         return string_settings;
      }
   };

} // namespace bugger

#endif
