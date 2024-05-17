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

#ifndef __BUGGER_INTERFACES_SOLVERINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOLVERINTERFACE_HPP__

#include "bugger/data/SolverSettings.hpp"
#include "bugger/data/Problem.hpp"
#include "bugger/data/Solution.hpp"
#include "bugger/misc/Timer.hpp"
#include "bugger/interfaces/SolverStatus.hpp"


namespace bugger
{
   enum SolverRetcode : char
   {
      OKAY          = 0,
      DUALFAIL      = 1,
      PRIMALFAIL    = 2,
      OBJECTIVEFAIL = 3
   };

   template <typename REAL>
   class SolverInterface
   {
   protected:

      const Message& msg;
      SolverSettings* adjustment = nullptr;
      const Problem<REAL>* model = nullptr;
      const Solution<REAL>* reference = nullptr;
      REAL value { std::numeric_limits<REAL>::signaling_NaN() };

   public:

      SolverInterface(const Message& _msg) : msg(_msg) { }

      /**
       * prints the header of the used solver
       */
      virtual
      void
      print_header() const = 0;

      /**
       * reports whether given setting is available
       * @param name
       */
      virtual
      bool
      has_setting(const String& name) const = 0;

      /**
       * parse Settings
       * @param filename
       */
      virtual
      boost::optional<SolverSettings>
      parseSettings(const String& filename) const = 0;

      /**
       * loads settings, problem, and solution
       * @param settings
       * @param problem
       * @param solution
       */
      virtual
      void
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) = 0;

      /**
       * solves the instance
       * @param passcodes suppress certain codes (referring to the returned values) -> these are not considered as bugs
       * @return a pair<char, SolverStatus>: Negative values in the char are reserved for solver internal errors while the remaining ones are declared in SolverInterface::Retcode. The SolverStatus primarily serves to be printed in the log holding the solution status of the solve, for example infeasible, unbounded, optimal, or specific limits reached.
       */
      virtual
      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) = 0;

      /**
       * provides measure for the solving effort to adapt batch number
       * @return a long long int: Non-negative value proportional to effort of the solve or -1 if unknown
       */
      virtual
      long long
      getSolvingEffort( ) const
      {
         return -1;
      }

      /**
       * read setting-problem pair from files
       * @param settings_filename
       * @param problem_filename
       */
      virtual
      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename) = 0;

      /**
       * write stored setting-problem pair to files
       * @param filename
       * @param writesettings
       */
      virtual
      bool
      writeInstance(const String& filename, const bool& writesettings) const = 0;

      virtual
      ~SolverInterface() = default;

   protected:

      REAL
      get_primal_activity(const SparseVectorView<REAL>& data, const Solution<REAL>& solution) const
      {
         StableSum<REAL> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add(data.getValues( )[ i ] * solution.primal[ data.getIndices( )[ i ] ]);
         return sum.get( );
      }

      REAL
      get_ray_activity(const SparseVectorView<REAL>& data, const Solution<REAL>& solution) const
      {
         StableSum<REAL> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add(data.getValues( )[ i ] * solution.ray[ data.getIndices( )[ i ] ]);
         return sum.get( );
      }

      REAL
      get_primal_objective(const Solution<REAL>& solution) const
      {
         StableSum<REAL> sum { model->getObjective().offset };
         for( int i = 0; i < model->getNCols(); ++i )
            if( !model->getColFlags()[i].test( ColFlag::kFixed ) )
               sum.add(model->getObjective().coefficients[i] * solution.primal[i]);
         return sum.get( );
      }

      REAL
      get_ray_objective(const Solution<REAL>& solution) const
      {
         StableSum<REAL> sum;
         for( int i = 0; i < model->getNCols(); ++i )
            if( !model->getColFlags()[i].test( ColFlag::kFixed ) )
               sum.add(model->getObjective().coefficients[i] * solution.ray[i]);
         return sum.get( );
      }

      REAL
      relax(const REAL& bound, const bool& increase, const REAL& tolerance, const REAL& infinity) const
      {
         assert(tolerance > 0);
         assert(tolerance * 2 < 1);
         assert(infinity > 1);

         if( bound <= -infinity )
            return -infinity;
         else if( bound >= infinity )
            return infinity;
         else if( abs(bound) < 1 )
            return bound + (increase ? tolerance : -tolerance);
         else if( (abs(bound) + 1) * tolerance > 1 )
            return bound + (increase ? REAL(1 - tolerance) : REAL(tolerance - 1));
         else if( bound < 0 )
            return bound * (1 + (increase ? -tolerance : tolerance));
         else
            return bound * (1 + (increase ? tolerance : -tolerance));
      }

      char
      check_dual_bound(const REAL& dual, const REAL& tolerance, const REAL& infinity) const
      {
         if( abs(dual) > infinity )
         {
            msg.detailed( "\tDual beyond infinity ({:<3} > {:<3})\n", abs(dual), infinity );
            return DUALFAIL;
         }

         if( reference->status == SolutionStatus::kUnknown )
            return OKAY;

         if( model->getObjective().sense )
         {
            if( dual > relax( value, true, tolerance, infinity ) )
            {
               msg.detailed( "\tDual above reference ({:<3} > {:<3})\n", dual, value );
               return DUALFAIL;
            }
         }
         else
         {
            if( dual < relax( value, false, tolerance, infinity ) )
            {
               msg.detailed( "\tDual below reference ({:<3} < {:<3})\n", dual, value );
               return DUALFAIL;
            }
         }

         return OKAY;
      }

      char
      check_primal_solution(const Vec<Solution<REAL>>& solution, const REAL& tolerance, const REAL& infinity) const
      {
         for( int i = solution.size() - 1; i >= 0; --i )
         {
            if( solution[i].status == SolutionStatus::kUnknown )
               continue;

            if( solution[i].status != SolutionStatus::kInfeasible )
            {
               assert(solution[i].primal.size() == model->getNCols());

               for( int col = 0; col < model->getNCols(); ++col )
               {
                  if( model->getColFlags()[col].test( ColFlag::kFixed ) )
                     continue;

                  if( solution[i].primal[col] < relax( model->getColFlags()[col].test( ColFlag::kLbInf ) ? -infinity : model->getLowerBounds()[col], false, tolerance, infinity )
                   || solution[i].primal[col] > relax( model->getColFlags()[col].test( ColFlag::kUbInf ) ?  infinity : model->getUpperBounds()[col], true,  tolerance, infinity )
                   || ( model->getColFlags()[col].test( ColFlag::kIntegral ) && abs(solution[i].primal[col] - round(solution[i].primal[col])) > tolerance ) )
                  {
                     msg.detailed( "\tColumn {:<3} outside domain (value {:<3}) in solution {:<3}\n", model->getVariableNames()[col], solution[i].primal[col], i );
                     return PRIMALFAIL;
                  }
               }

               for( int row = 0; row < model->getNRows(); ++row )
               {
                  if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                     continue;

                  REAL activity { get_primal_activity(model->getConstraintMatrix().getRowCoefficients(row), solution[i]) };

                  if( ( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < relax( model->getConstraintMatrix().getLeftHandSides()[row],  false, tolerance, infinity ) )
                   || ( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) && activity > relax( model->getConstraintMatrix().getRightHandSides()[row], true,  tolerance, infinity ) ) )
                  {
                     msg.detailed( "\tRow {:<3} outside range (activity {:<3}) in solution {:<3}\n", model->getConstraintNames()[row], activity, i );
                     return PRIMALFAIL;
                  }
               }
            }

            if( solution[i].status == SolutionStatus::kUnbounded )
            {
               assert(solution[i].ray.size() == model->getNCols());

               REAL scale { };

               for( int col = 0; col < model->getNCols(); ++col )
                  if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                     scale = max(scale, abs(solution[i].ray[col]));

               scale *= tolerance;

               for( int col = 0; col < model->getNCols(); ++col )
               {
                  if( !model->getColFlags()[col].test( ColFlag::kFixed )
                      && ( ( !model->getColFlags()[col].test( ColFlag::kLbInf ) && solution[i].ray[col] < -scale )
                        || ( !model->getColFlags()[col].test( ColFlag::kUbInf ) && solution[i].ray[col] >  scale ) ) )
                  {
                     msg.detailed( "\tColumn {:<3} escaped domain (rayval {:<3}) in solution {:<3}\n", model->getVariableNames()[col], solution[i].ray[col], i );
                     return PRIMALFAIL;
                  }
               }

               for( int row = 0; row < model->getNRows(); ++row )
               {
                  if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                     continue;

                  REAL activity { get_ray_activity(model->getConstraintMatrix().getRowCoefficients(row), solution[i]) };

                  if( ( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < -scale )
                   || ( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) && activity >  scale ) )
                  {
                     msg.detailed( "\tRow {:<3} escaped range (rayact {:<3}) in solution {:<3}\n", model->getConstraintNames()[row], activity, i );
                     return PRIMALFAIL;
                  }
               }
            }
         }

         return OKAY;
      }

      char
      check_objective_value(const REAL& primal, const Solution<REAL>& solution, const REAL& tolerance, const REAL& infinity) const
      {
         if( abs(primal) > infinity )
         {
            msg.detailed( "\tPrimal beyond infinity ({:<3} > {:<3})\n", abs(primal), infinity );
            return OBJECTIVEFAIL;
         }

         if( solution.status == SolutionStatus::kUnknown )
            return OKAY;

         if( solution.status == SolutionStatus::kUnbounded )
         {
            assert(solution.ray.size() == model->getNCols());

            REAL slope { get_ray_objective(solution) };
            REAL scale { };

            for( int col = 0; col < model->getNCols(); ++col )
               if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                  scale = max(scale, abs(solution.ray[col]));

            scale *= tolerance;

            if( (model->getObjective().sense ? -slope : slope) > scale )
               return OKAY;
         }

         REAL result;

         if( solution.status == SolutionStatus::kInfeasible )
            result = model->getObjective().sense ? infinity : -infinity;
         else
         {
            assert(solution.primal.size() == model->getNCols());

            result = get_primal_objective(solution);
         }

         if( model->getObjective().sense )
         {
            if( primal < relax( result, false, tolerance, infinity ) )
            {
               msg.detailed( "\tPrimal below reference ({:<3} < {:<3})\n", primal, result );
               return OBJECTIVEFAIL;
            }
         }
         else
         {
            if( primal > relax( result, true, tolerance, infinity ) )
            {
               msg.detailed( "\tPrimal above reference ({:<3} > {:<3})\n", primal, result );
               return OBJECTIVEFAIL;
            }
         }

         return OKAY;
      }

      char
      check_count_number(const REAL& dual, const REAL& primal, const long long& count, const REAL& infinity) const
      {
         assert(infinity > 1);

         if( abs(dual) > infinity || (model->getObjective().sense ? primal : -primal) != infinity || count < -1 )
         {
            msg.detailed( "\tResult not consistent (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
            return OBJECTIVEFAIL;
         }

         if( reference->status == SolutionStatus::kUnknown )
            return OKAY;

         if( reference->status == SolutionStatus::kInfeasible )
         {
            if( count != 0 )
            {
               msg.detailed( "\tInfeasibility not respected (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
               return PRIMALFAIL;
            }
         }
         else if( abs(value) < infinity && (model->getObjective().sense ? dual : -dual) == infinity )
         {
            if( count == 0 )
            {
               msg.detailed( "\tFeasibility not respected (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
               return DUALFAIL;
            }
         }

         return OKAY;
      }
   };

   template <typename REAL>
   class SolverFactory
   {
   public:

      virtual
      void
      addParameters(ParameterSet& parameterset) = 0;

      virtual
      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) = 0;

      virtual
      ~SolverFactory() = default;
   };

} // namespace bugger

#endif
