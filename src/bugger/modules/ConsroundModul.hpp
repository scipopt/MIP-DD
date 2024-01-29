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

#ifndef BUGGER_MODUL_CONSROUND_HPP_
#define BUGGER_MODUL_CONSROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class ConsRoundModul : public BuggerModul {
   public:
      explicit ConsRoundModul( const Message &_msg, const Num<double> &_num, std::shared_ptr<SolverFactory>& factory) : BuggerModul(factory) {
         this->setName("consround");
         this->msg = _msg;
         this->num = _num;

      }

      bool
      initialize( ) override {
         return false;
      }

      bool isConsroundAdmissible(const Problem<double>& problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         bool lhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf);
         bool rhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf);
         double lhs = problem.getConstraintMatrix( ).getLeftHandSides( )[ row ];
         double rhs = problem.getConstraintMatrix( ).getRightHandSides( )[ row ];
         if( ( lhsinf || rhsinf || !num.isZetaEq(lhs, rhs) ) && ( ( !lhsinf && !num.isZetaIntegral(lhs) ) || ( !rhsinf && !num.isZetaIntegral(rhs) ) ) )
            return true;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !num.isZetaIntegral(data.getValues( )[ index ]) )
               return true;
         return false;
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
            for( int i = 0; i < problem.getNRows( ); ++i )
               if( isConsroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<MatrixEntry<double>> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions_lhs { };
         Vec<std::pair<int, double>> applied_reductions_rhs { };
         Vec<MatrixEntry<double>> batches_coeff { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);

         int batch = 0;
         for( int row = 0; row < copy.getNRows( ); ++row )
         {
            if( isConsroundAdmissible(copy, row) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);
               double lhs = num.round(copy.getConstraintMatrix( ).getLeftHandSides( )[ row ]);
               double rhs = num.round(copy.getConstraintMatrix( ).getRightHandSides( )[ row ]);


               double activity = 0.0;

               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( solution.status == SolutionStatus::kFeasible )
                  {
                     if( !num.isZetaIntegral(data.getValues( )[ index ]) )
                     {
                        double new_coeff = num.round(data.getValues( )[ index ]);
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], new_coeff);
                        activity += solution.primal[ data.getIndices( )[ index ] ] * new_coeff;
                     }
                     else
                        activity += solution.primal[ data.getIndices( )[ index ] ] * data.getValues( )[ index ];
                  }
                  else
                  {
                     if( !num.isZetaIntegral(data.getValues( )[ index ]) )
                     {
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], num.round(data.getValues( )[ index ]));
                     }
                  }
               }

               if( solution.status == SolutionStatus::kFeasible )
               {
                  lhs = num.min(lhs, num.epsFloor(activity));
                  rhs = num.max(rhs, num.epsCeil(activity));
               }

               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
                     && !num.isZetaEq(lhs, copy.getConstraintMatrix().getLeftHandSides()[ row ]) )
               {
                  copy.getConstraintMatrix( ).modifyLeftHandSide(row, num, lhs);
                  batches_lhs.emplace_back(row, lhs);
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
                     && !num.isZetaEq(rhs, copy.getConstraintMatrix().getRightHandSides()[ row ]) )
               {
                  copy.getConstraintMatrix( ).modifyRightHandSide(row, num, rhs);
                  batches_rhs.emplace_back(row, rhs);
               }
               ++batch;
            }

            if( batch != 0 && ( batch >= batchsize || row >= copy.getNRows( ) - 1 ) )
            {
               auto solver = createSolver( );

               MatrixBuffer<double> matrixBuffer{ };
               for(auto entry: batches_coeff)
                  matrixBuffer.addEntry(entry.row, entry.col, entry.val);
               copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer);

               solver->doSetUp(copy,  settings, solution);
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  MatrixBuffer<double> matrixBuffer2{ };
                  for(auto entry: applied_entries)
                     matrixBuffer2.addEntry(entry.row, entry.col, entry.val);
                  copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer2);
                  for( const auto &item: applied_reductions_lhs )
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kLhsInf) )
                        copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, item.second );
                  for( const auto &item: applied_reductions_rhs )
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kRhsInf) )
                        copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, item.second );
               }
               else
               {
                  applied_reductions_lhs.insert(applied_reductions_lhs.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_reductions_rhs.insert(applied_reductions_rhs.end(), batches_rhs.begin(), batches_rhs.end());
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
               batch = 0;
            }
         }

         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if( applied_entries.empty() && applied_reductions_lhs.empty() || !applied_reductions_rhs.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_entries.size();
            nchgsides += applied_reductions_lhs.size() + applied_reductions_rhs.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
