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

   protected:

      const Problem<double>* model = nullptr;
      const Solution<double>* reference = nullptr;
      double value = std::numeric_limits<double>::signaling_NaN();

   public:

      const static char OKAY = 0;
      const static char DUALFAIL = 1;
      const static char PRIMALFAIL = 2;
      const static char OBJECTIVEFAIL = 3;

      SolverInterface( ) = default;

      const char
      check_dual_bound( const double &dual, const double &tolerance, const double &infinity )
      {
         assert(tolerance > 0.0);
         assert(tolerance < 0.5);
         assert(infinity > 1.0);

         if( dual < -infinity || dual > infinity || ( reference->status != SolutionStatus::kUnknown && (model->getObjective().sense ? dual - std::max(value, -infinity) : std::min(value, infinity) - dual) > tolerance ) )
            return DUALFAIL;

         return OKAY;
      }

      const char
      check_primal_solution( const Solution<double> &solution, const double &tolerance, const double &infinity )
      {
         assert(tolerance > 0.0);
         assert(tolerance < 0.5);
         assert(infinity > 1.0);

         if( solution.status == SolutionStatus::kUnknown )
            return OKAY;

         if( solution.status != SolutionStatus::kInfeasible )
         {
            assert(solution.primal.size() == model->getNCols());

            for( int col = 0; col < model->getNCols(); ++col )
            {
               if( model->getColFlags()[col].test( ColFlag::kFixed ) )
                  continue;

               double relax;

               if( model->getColFlags()[col].test( ColFlag::kLbInf ) )
                  relax = -infinity;
               else if( abs(model->getLowerBounds()[col]) < 1.0 )
                  relax = model->getLowerBounds()[col] - tolerance;
               else if( (abs(model->getLowerBounds()[col]) + 1.0) * tolerance > 1.0 )
                  relax = model->getLowerBounds()[col] - (1.0 - tolerance);
               else if( model->getLowerBounds()[col] < 0.0 )
                  relax = model->getLowerBounds()[col] * (1.0 + tolerance);
               else
                  relax = model->getLowerBounds()[col] * (1.0 - tolerance);
               if( solution.primal[col] < relax )
                  return PRIMALFAIL;

               if( model->getColFlags()[col].test( ColFlag::kUbInf ) )
                  relax = infinity;
               else if( abs(model->getUpperBounds()[col]) < 1.0 )
                  relax = model->getUpperBounds()[col] + tolerance;
               else if( (abs(model->getUpperBounds()[col]) + 1.0) * tolerance > 1.0 )
                  relax = model->getUpperBounds()[col] + (1.0 - tolerance);
               else if( model->getUpperBounds()[col] < 0.0 )
                  relax = model->getUpperBounds()[col] * (1.0 - tolerance);
               else
                  relax = model->getUpperBounds()[col] * (1.0 + tolerance);
               if( solution.primal[col] > relax )
                  return PRIMALFAIL;

               if( model->getColFlags()[col].test( ColFlag::kIntegral ) && abs(solution.primal[col] - rint(solution.primal[col])) > tolerance )
                  return PRIMALFAIL;
            }

            for( int row = 0; row < model->getNRows(); ++row )
            {
               if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                  continue;

               double activity = 0.0;
               auto coefficients = model->getConstraintMatrix().getRowCoefficients(row);
               for( int i = 0; i < coefficients.getLength(); ++i )
                  activity += coefficients.getValues()[i] * solution.primal[coefficients.getIndices()[i]];
               if( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) )
               {
                  double relax;

                  if( abs(model->getConstraintMatrix().getLeftHandSides()[row]) < 1.0 )
                     relax = model->getConstraintMatrix().getLeftHandSides()[row] - tolerance;
                  else if( (abs(model->getConstraintMatrix().getLeftHandSides()[row]) + 1.0) * tolerance > 1.0 )
                     relax = model->getConstraintMatrix().getLeftHandSides()[row] - (1.0 - tolerance);
                  else if( model->getConstraintMatrix().getLeftHandSides()[row] < 0.0 )
                     relax = model->getConstraintMatrix().getLeftHandSides()[row] * (1.0 + tolerance);
                  else
                     relax = model->getConstraintMatrix().getLeftHandSides()[row] * (1.0 - tolerance);
                  if( activity < relax )
                     return PRIMALFAIL;
               }
               if( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) )
               {
                  double relax;

                  if( abs(model->getConstraintMatrix().getRightHandSides()[row]) < 1.0 )
                     relax = model->getConstraintMatrix().getRightHandSides()[row] + tolerance;
                  else if( (abs(model->getConstraintMatrix().getRightHandSides()[row]) + 1.0) * tolerance > 1.0 )
                     relax = model->getConstraintMatrix().getRightHandSides()[row] + (1.0 - tolerance);
                  else if( model->getConstraintMatrix().getRightHandSides()[row] < 0.0 )
                     relax = model->getConstraintMatrix().getRightHandSides()[row] * (1.0 - tolerance);
                  else
                     relax = model->getConstraintMatrix().getRightHandSides()[row] * (1.0 + tolerance);
                  if( activity > relax )
                     return PRIMALFAIL;
               }
            }
         }

         if( solution.status == SolutionStatus::kUnbounded )
         {
            assert(solution.ray.size() == model->getNCols());

            double relax = 0.0;

            for( int col = 0; col < model->getNCols(); ++col )
               if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                  relax = std::max(relax, abs(solution.ray[col]));

            relax *= tolerance;

            for( int col = 0; col < model->getNCols(); ++col )
               if( !model->getColFlags()[col].test( ColFlag::kFixed )
                  && ( ( !model->getColFlags()[col].test( ColFlag::kLbInf ) && solution.ray[col] < -relax )
                    || ( !model->getColFlags()[col].test( ColFlag::kUbInf ) && solution.ray[col] > relax ) ) )
                  return PRIMALFAIL;

            for( int row = 0; row < model->getNRows(); ++row )
            {
               if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                  continue;

               double activity = 0.0;
               auto coefficients = model->getConstraintMatrix().getRowCoefficients(row);
               for( int i = 0; i < coefficients.getLength(); ++i )
                  activity += coefficients.getValues()[i] * solution.ray[coefficients.getIndices()[i]];
               if( ( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < -relax )
                || ( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) && activity > relax ) )
                  return PRIMALFAIL;
            }
         }

         return OKAY;
      }

      const char
      check_objective_value( const double &primal, const Solution<double> &solution, const double &tolerance, const double &infinity )
      {
         assert(tolerance > 0.0);
         assert(tolerance < 0.5);
         assert(infinity > 1.0);

         if( primal < -infinity || primal > infinity )
            return OBJECTIVEFAIL;

         if( solution.status == SolutionStatus::kUnknown )
            return OKAY;

         if( solution.status == SolutionStatus::kUnbounded )
         {
            assert(solution.ray.size() == model->getNCols());

            double relax = 0.0;
            double slope = 0.0;

            for( int col = 0; col < model->getNCols(); ++col )
            {
               if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
               {
                  relax = std::max(relax, abs(solution.ray[col]));
                  slope += model->getObjective().coefficients[col] * solution.ray[col];
               }
            }

            relax *= tolerance;

            if( (model->getObjective().sense ? -slope : slope) > relax )
               return OKAY;
         }

         double result;

         if( solution.status == SolutionStatus::kInfeasible )
            result = model->getObjective().sense ? infinity : -infinity;
         else
         {
            assert(solution.primal.size() == model->getNCols());

            result = model->getObjective().offset;

            for( int col = 0; col < model->getNCols(); ++col )
               if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                  result += model->getObjective().coefficients[col] * solution.primal[col];
         }

         if( (model->getObjective().sense ? std::min(result, infinity) - primal : primal - std::max(result, -infinity)) > tolerance )
            return OBJECTIVEFAIL;

         return OKAY;
      }

      /**
       * parse Settings
       * @param filename
       */
      virtual
      boost::optional<SolverSettings> parseSettings(const std::string &filename) = 0;

      /**
       * loads settings, problem, and solution
       * @param settings
       * @param problem
       * @param solution
       */
      virtual
      void doSetUp(const SolverSettings &settings, const Problem<double> &problem, const Solution<double> &solution) = 0;

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
      void writeInstance(const std::string &filename, const bool &writesettings) = 0;

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
