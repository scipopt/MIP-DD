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

#ifndef __BUGGER_DATA_SOLVERSETTINGS_HPP__
#define __BUGGER_DATA_SOLVERSETTINGS_HPP__

#include "bugger/misc/Vec.hpp"

namespace bugger {

   class SolverSettings {

   private:

      Vec<std::pair<String, bool>> bool_settings;
      Vec<std::pair<String, int>> int_settings;
      Vec<std::pair<String, long>> long_settings;
      Vec<std::pair<String, double>> double_settings;
      Vec<std::pair<String, char>> char_settings;
      Vec<std::pair<String, String>> string_settings;
      Vec<std::pair<String, long long>> limit_settings;

   public:

      SolverSettings() :
         bool_settings( ),
         int_settings( ),
         long_settings( ),
         double_settings( ),
         char_settings( ),
         string_settings( ),
         limit_settings( )
      { };

      SolverSettings(Vec<std::pair<String, bool>>& _bool_settings,
                     Vec<std::pair<String, int>>& _int_settings,
                     Vec<std::pair<String, long>>& _long_settings,
                     Vec<std::pair<String, double>>& _double_settings,
                     Vec<std::pair<String, char>>& _char_settings,
                     Vec<std::pair<String, String>>& _string_settings,
                     Vec<std::pair<String, long long>>& _limit_settings
      ) :
         bool_settings(_bool_settings),
         int_settings(_int_settings),
         long_settings(_long_settings),
         double_settings(_double_settings),
         char_settings(_char_settings),
         string_settings(_string_settings),
         limit_settings(_limit_settings)
      { };

      const Vec<std::pair<String, bool>>& getBoolSettings( ) const {
         return bool_settings;
      }

      const Vec<std::pair<String, int>>& getIntSettings( ) const {
         return int_settings;
      }

      const Vec<std::pair<String, long>>& getLongSettings( ) const {
         return long_settings;
      }

      const Vec<std::pair<String, double>>& getDoubleSettings( ) const {
         return double_settings;
      }

      const Vec<std::pair<String, char>>& getCharSettings( ) const {
         return char_settings;
      }

      const Vec<std::pair<String, String>>& getStringSettings( ) const {
         return string_settings;
      }

      const Vec<std::pair<String, long long>>& getLimitSettings( ) const {
         return limit_settings;
      }

      void setBoolSettings(int index, bool value) {
         bool_settings[ index ].second = value;
      }

      void setIntSettings(int index, int value) {
         int_settings[ index ].second = value;
      }

      void setLongSettings(int index, long value) {
         long_settings[ index ].second = value;
      }

      void setDoubleSettings(int index, double value) {
         double_settings[ index ].second = value;
      }

      void setCharSettings(int index, char value) {
         char_settings[ index ].second = value;
      }

      void setStringSettings(int index, String value) {
         string_settings[ index ].second = value;
      }

      void setLimitSettings(int index, long long value) {
         limit_settings[ index ].second = value;
      }
   };

} // namespace bugger

#endif
