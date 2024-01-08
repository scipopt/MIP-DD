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

#ifndef BUGGER_SIDE_VARIABLE_HPP_
#define BUGGER_SIDE_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class SideModul : public BuggerModul {
   public:
      SideModul(const std::string& _setting, const Message &_msg, const Num<double> &_num) : BuggerModul(_setting) {
         this->setName("side");
         this->msg = _msg;
         this->num = _num;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isSideAdmissable(const Problem<double>& problem, int row) const
      {
         return !problem.getRowFlags( )[ row ].test(RowFlag::kRedundant)
           && ( problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
             || problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
             || !num.isZetaEq(problem.getConstraintMatrix( ).getLeftHandSides( )[ row ], problem.getConstraintMatrix( ).getRightHandSides( )[ row ]) );
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const SolverSettings& settings, const Timer &timer) override {

         auto copy = Problem<double>(problem);
         ConstraintMatrix<double>& matrix = copy.getConstraintMatrix( );
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<std::pair<int, double>> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int row = problem.getNRows( ) - 1; row >= 0; --row )
               if( isSideAdmissable(problem, row) )
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         batches.reserve(batchsize);
         bool admissible = false;

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isSideAdmissable(copy, row) )
            {
               admissible = true;
               auto data = matrix.getRowCoefficients(row);
               bool integral = true;
               double fixedval;

               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kIntegral) || !num.isEpsIntegral(data.getValues( )[ index ]) )
                  {
                     integral = false;
                     break;
                  }
               }

               if( solution_exists )
               {
                  fixedval = get_linear_activity(data, solution);
                  if( integral )
                     fixedval = num.round(fixedval);
               }
               else
               {
                  fixedval = 0.0;
                  if( integral )
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = num.min(fixedval, num.epsFloor(matrix.getRightHandSides( )[ row ]));
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = num.max(fixedval, num.epsCeil(matrix.getLeftHandSides( )[ row ]));
                  }
                  else
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = num.min(fixedval, matrix.getRightHandSides( )[ row ]);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = num.max(fixedval, matrix.getLeftHandSides( )[ row ]);
                  }
               }

               matrix.modifyLeftHandSide( row, num, fixedval );
               matrix.modifyRightHandSide( row, num, fixedval );
               batches.push_back({ row, fixedval });
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               auto solver = createSolver();
//               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus, settings) == BuggerStatus::kSuccess )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                  {
                     matrix.modifyLeftHandSide( item.first, num, item.second );
                     matrix.modifyRightHandSide( item.first, num, item.second );
                  }
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }
         if(!admissible)
            return ModulStatus::kDidNotRun;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgsides += 2 * applied_reductions.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
