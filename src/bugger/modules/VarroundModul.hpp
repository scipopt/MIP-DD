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

#ifndef BUGGER_MODUL_VARROUND_HPP_
#define BUGGER_MODUL_VARROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"

namespace bugger {

   class VarroundModul : public BuggerModul {
   public:
      VarroundModul( const Message& _msg, const Num<double> &_num ) : BuggerModul( ) {
         this->setName("varround");
         this->msg = _msg;
         this->num = _num;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isBoundFractional(const Problem<double> &problem, int var) {
         double lb = problem.getLowerBounds( )[ var ];
         double ub = problem.getUpperBounds( )[ var ];
         if( !problem.getColFlags( )[ var ].test(ColFlag::kLbInf)
          && !problem.getColFlags( )[ var ].test(ColFlag::kUbInf)
          && num.isZetaEq(lb, ub) )
            return false;
         return ( !problem.getColFlags( )[ var ].test(ColFlag::kLbInf) && !num.isZetaIntegral(lb) )
             || ( !problem.getColFlags( )[ var ].test(ColFlag::kUbInf) && !num.isZetaIntegral(ub) );
      }

      bool isVarroundAdmissible(const Problem<double> &problem, int var) {
         if( problem.getColFlags( )[ var ].test(ColFlag::kFixed) )
            return false;
         if( isBoundFractional(problem, var) )
            return true;
         return !num.isZetaIntegral(problem.getObjective( ).coefficients[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_lb { };
         Vec<std::pair<int, double>> applied_ub { };
         Vec<std::pair<int, double>> applied_obj { };
         Vec<std::pair<int, double>> batches_lb { };
         Vec<std::pair<int, double>> batches_ub { };
         Vec<std::pair<int, double>> batches_obj { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = 0; i < problem.getNCols( ); ++i )
               if( isVarroundAdmissible(problem, i) )
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         batches_lb.reserve(batchsize);
         batches_ub.reserve(batchsize);
         batches_obj.reserve(batchsize);

         for( int var = 0; var < copy.getNCols( ); ++var )
         {
            if( isVarroundAdmissible(copy, var) )
            {
               double lb = num.round(copy.getLowerBounds( )[ var ]);
               double ub = num.round(copy.getUpperBounds( )[ var ]);

               if( solution_exists )
               {
                  double value = solution.primal[ var ];

                  lb = num.min(lb, num.epsFloor(value));
                  ub = num.max(ub, num.epsCeil(value));
               }

               if( !copy.getColFlags( )[ var ].test(ColFlag::kLbInf) )
               {
                  copy.getLowerBounds( )[ var ] = lb;
                  batches_lb.push_back({ var, lb });
               }
               if( !copy.getColFlags( )[ var ].test(ColFlag::kUbInf) )
               {
                  copy.getUpperBounds( )[ var ] = ub;
                  batches_ub.push_back({ var, lb });
               }
               copy.getObjective( ).coefficients[ var ] = num.round(copy.getObjective( ).coefficients[ var ]);
               batches_obj.push_back({ var, copy.getObjective( ).coefficients[ var ] });
            }

            if( !batches_obj.empty() && ( batches_obj.size() >= batchsize || var >= copy.getNCols( ) - 1 ) )
            {
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus) == BuggerStatus::kSuccess )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_lb )
                     copy.getLowerBounds( )[ item.first ] = item.second;
                  for( const auto &item: applied_ub )
                     copy.getUpperBounds( )[ item.first ] = item.second;
                  for( const auto &item: applied_obj )
                     copy.getObjective( ).coefficients[ item.first ] = item.second;
               }
               else
               {
                  applied_lb.insert(applied_lb.end(), batches_lb.begin(), batches_lb.end());
                  applied_ub.insert(applied_ub.end(), batches_ub.begin(), batches_ub.end());
                  applied_obj.insert(applied_obj.end(), batches_obj.begin(), batches_obj.end());
               }
               batches_lb.clear();
               batches_ub.clear();
               batches_obj.clear();
            }
         }

         if( applied_obj.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_lb.size() + applied_ub.size() + applied_obj.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
