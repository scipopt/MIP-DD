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

#ifndef BUGGER_MODUL_VARROUND_HPP_
#define BUGGER_MODUL_VARROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"

namespace bugger {

   class VarroundModul : public BuggerModul {
   public:
      VarroundModul( const Message &_msg, const Num<double> &_num, std::shared_ptr<SolverFactory>& factory) : BuggerModul(factory) {
         this->setName("varround");
         this->msg = _msg;
         this->num = _num;

      }

      bool
      initialize( ) override {
         return false;
      }

      bool isVarroundAdmissible(const Problem<double>& problem, int col) {
         if( problem.getColFlags( )[ col ].test(ColFlag::kFixed) )
            return false;
         if( !num.isZetaIntegral(problem.getObjective( ).coefficients[ col ]) )
            return true;
         bool lbinf = problem.getColFlags( )[ col ].test(ColFlag::kLbInf);
         bool ubinf = problem.getColFlags( )[ col ].test(ColFlag::kUbInf);
         double lb = problem.getLowerBounds( )[ col ];
         double ub = problem.getUpperBounds( )[ col ];
         return ( lbinf || ubinf || !num.isZetaEq(lb, ub) ) && ( ( !lbinf && !num.isZetaIntegral(lb) ) || ( !ubinf && !num.isZetaIntegral(ub) ) );
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution,
              const BuggerOptions &options, const Timer &timer) override {

         if( solution.status == SolutionStatus::kInfeasible || solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = 0; i < problem.getNCols( ); ++i )
               if( isVarroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_objectives { };
         Vec<std::pair<int, double>> applied_lowers { };
         Vec<std::pair<int, double>> applied_uppers { };
         Vec<std::pair<int, double>> batches_obj { };
         Vec<std::pair<int, double>> batches_lb { };
         Vec<std::pair<int, double>> batches_ub { };
         batches_obj.reserve(batchsize);
         batches_lb.reserve(batchsize);
         batches_ub.reserve(batchsize);
         int batch = 0;

         for( int col = 0; col < copy.getNCols( ); ++col )
         {
            if( isVarroundAdmissible(copy, col) )
            {
               admissible = true;
               double lb = num.round(copy.getLowerBounds( )[ col ]);
               double ub = num.round(copy.getUpperBounds( )[ col ]);
               if( solution.status == SolutionStatus::kFeasible )
               {
                  double value = solution.primal[ col ];
                  lb = num.min(lb, num.epsFloor(value));
                  ub = num.max(ub, num.epsCeil(value));
               }
               if( !num.isZetaIntegral(copy.getObjective( ).coefficients[ col ]) )
               {
                  double obj = num.round(copy.getObjective( ).coefficients[ col ]);
                  copy.getObjective( ).coefficients[ col ] = obj;
                  batches_obj.emplace_back(col, obj);
               }
               if( !copy.getColFlags( )[ col ].test(ColFlag::kLbInf) && !num.isZetaEq(copy.getLowerBounds( )[ col ], lb) )
               {
                  copy.getLowerBounds( )[ col ] = lb;
                  batches_lb.emplace_back(col, lb);
               }
               if( !copy.getColFlags( )[ col ].test(ColFlag::kUbInf) && !num.isZetaEq(copy.getUpperBounds( )[ col ], ub) )
               {
                  copy.getUpperBounds( )[ col ] = ub;
                  batches_ub.emplace_back(col, ub);
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || col >= copy.getNCols( ) - 1 ) )
            {
               auto solver = createSolver();
               solver->doSetUp(settings, copy, solution);
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_objectives )
                     copy.getObjective( ).coefficients[ item.first ] = item.second;
                  for( const auto &item: applied_lowers )
                     copy.getLowerBounds( )[ item.first ] = item.second;
                  for( const auto &item: applied_uppers )
                     copy.getUpperBounds( )[ item.first ] = item.second;
               }
               else
               {
                  applied_objectives.insert(applied_objectives.end(), batches_obj.begin(), batches_obj.end());
                  applied_lowers.insert(applied_lowers.end(), batches_lb.begin(), batches_lb.end());
                  applied_uppers.insert(applied_uppers.end(), batches_ub.begin(), batches_ub.end());
               }
               batches_obj.clear();
               batches_lb.clear();
               batches_ub.clear();
               batch = 0;
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_objectives.empty() && applied_lowers.empty() && applied_uppers.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         nchgcoefs += applied_objectives.size() + applied_lowers.size() + applied_uppers.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
