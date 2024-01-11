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
      explicit ConsRoundModul( const Message &_msg, const Num<double> &_num, const SolverStatus& _status) : BuggerModul() {
         this->setName("consround");
         this->msg = _msg;
         this->num = _num;
         this->originalSolverStatus = _status;
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
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         auto copy = Problem<double>(problem);
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions_lhs { };
         Vec<std::pair<int, double>> applied_reductions_rhs { };
         MatrixBuffer<double> batches_coeff { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
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

         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);
         bool admissible = false;

         for( int row = 0; row < copy.getNRows( ); ++row )
         {
            if( isConsroundAdmissible(copy, row) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);
               double lhs = num.round(copy.getConstraintMatrix( ).getLeftHandSides( )[ row ]);
               double rhs = num.round(copy.getConstraintMatrix( ).getRightHandSides( )[ row ]);

               for( int index = 0; index < data.getLength( ); ++index )
                  if( !num.isZetaIntegral(data.getValues( )[ index ]) )
                     batches_coeff.addEntry(row, data.getIndices( )[ index ], num.round(data.getValues( )[ index ]));

               //TODO: Change row only
               copy.getConstraintMatrix( ).changeCoefficients(batches_coeff);

               if( solution_exists )
               {
                  data = copy.getConstraintMatrix( ).getRowCoefficients(row);
                  double activity = get_linear_activity(data, solution);

                  lhs = num.min(lhs, num.epsFloor(activity));
                  rhs = num.max(rhs, num.epsCeil(activity));
               }

               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                  copy.getConstraintMatrix( ).modifyLeftHandSide( row, num, lhs );
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                  copy.getConstraintMatrix( ).modifyRightHandSide( row, num, rhs );
               batches_lhs.push_back({ row, lhs });
               batches_rhs.push_back({ row, rhs });
            }

            if( !batches_lhs.empty() && ( batches_lhs.size() >= batchsize || row >= copy.getNRows( ) - 1 ) )
            {
               auto solver = createSolver( );
               solver->doSetUp(copy,  settings, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus, settings) != BuggerStatus::kFail)
               {
                  copy = Problem<double>(problem);
                  copy.getConstraintMatrix( ).changeCoefficients(applied_entries);
                  for( const auto &item: applied_reductions_lhs )
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kLhsInf) )
                        copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, item.second );
                  for( const auto &item: applied_reductions_rhs )
                     if( !copy.getRowFlags( )[ item.first ].test(RowFlag::kRhsInf) )
                        copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, item.second );
               }
               else
               {
                  SmallVec<int, 32> buffer;
                  const MatrixEntry<double> *iter = batches_coeff.template begin<true>(buffer);
                  while( iter != batches_coeff.end( ) )
                  {
                     applied_entries.addEntry(iter->row, iter->col, iter->val);
                     iter = batches_coeff.template next<true>( buffer );
                  }
                  applied_reductions_lhs.insert(applied_reductions_lhs.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_reductions_rhs.insert(applied_reductions_rhs.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
            }
         }

         if(!admissible)
            return ModulStatus::kNotAdmissible;
         if( applied_reductions_lhs.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_entries.getNnz();
            nchgsides += applied_reductions_lhs.size() + applied_reductions_rhs.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
