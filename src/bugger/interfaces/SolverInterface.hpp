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

#ifndef _BUGGER_INTERFACES_SOLVER_INTERFACE_HPP_
#define _BUGGER_INTERFACES_SOLVER_INTERFACE_HPP_


#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "bugger/data/Problem.hpp"
#include "bugger/data/Solution.hpp"
#include "bugger/data/SolverSettings.hpp"

namespace bugger {

   class SolverInterface {

   public:

      const static char OKAY = 0;
      const static char DUALFAIL = 1;

      SolverInterface( ) = default;

      /**
       * parse Settings
       * @param filename
       */
      virtual
      SolverSettings parseSettings(const std::string &filename) = 0;

      /**
       * loads settings, problem, and solution
       * @param settings
       * @param problem
       * @param solution
       */
      virtual
      void doSetUp(const SolverSettings &settings, const Problem<double> &problem, Solution<double> &solution) = 0;

      virtual
      std::pair<char, SolverStatus> solve(const Vec<int>& passcodes) = 0;

      /**
       * read setting-problem pair from files
       * @param settings_filename
       * @param problem_filename
       */
      virtual
      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<double>>> readInstance(const std::string &settings_filename, const std::string &problem_filename)
      {
         return { boost::none, boost::none };
      };

      /**
       * write setting-problem pair to files
       * @param filename
       * @param settings
       * @param problem
       * @param writesettings
       */
      virtual
      void writeInstance(const std::string &filename, const SolverSettings &settings, const Problem<double> &problem, const bool &writesettings) = 0;

      virtual ~SolverInterface() = default;
   };

   class SolverFactory
   {
   public:

      virtual std::unique_ptr<SolverInterface>
      create_solver( ) const = 0;

      virtual void
      add_parameters( ParameterSet& parameter ) {}

      virtual ~SolverFactory() {}
   };


} // namespace bugger

#endif
