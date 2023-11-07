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

#ifndef BUGGER_MODUL_COEFFICIENT_HPP_
#define BUGGER_MODUL_COEFFICIENT_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class CoefficientModul : public BuggerModul {
   public:
      CoefficientModul(const Message &_msg, const Num<double> &_num) : BuggerModul( ) {
         this->setName("coefficient");
         this->msg = _msg;
         this->num = _num;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isCoefficientAdmissible(const Problem<double> problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant))
            return false;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         const VariableDomains<double> &domains = problem.getVariableDomains( );
         for( int i = 0; i < data.getLength( ); ++i )
         {
            int var = data.getIndices( )[ i ];
            if( is_lb_ge_than_ub(domains, var))
               return true;
         }
         return false;
      }

      bool is_lb_ge_than_ub(const VariableDomains<double> &domains, int var) const {
         return domains.flags[ var ].test(ColFlag::kUbInf) || domains.flags[ var ].test(ColFlag::kLbInf)
                || num.isZetaGE(domains.lower_bounds[ var ], domains.upper_bounds[ var ]);
      }


      //TODO: here seems to be a bug so that there is infinity loop
      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto copy = Problem<double>(problem);
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, int >> applied_reductions_lhs { };
         Vec<std::pair<int, int>> applied_reductions_rhs { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         MatrixBuffer<double> batches_coeff { };

         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isCoefficientAdmissible(problem, i))
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         int nbatch = 0;
         batches_rhs.reserve(batchsize);
         batches_lhs.reserve(batchsize);
         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {

            if( isCoefficientAdmissible(copy, row))
            {
               auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);

               // add abort criteria
               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int var = data.getIndices( )[ index ];
                  if( is_lb_ge_than_ub(copy.getVariableDomains( ), var))
                  {
                     SCIP_Real fixedval;

                     if( !solution_exists )
                     {
                        if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral))
                           fixedval = MAX(MIN(0.0, num.zetaFloor(copy.getUpperBounds( )[ var ])),
                                          num.zetaCeil(copy.getLowerBounds( )[ var ]));
                        else
                           fixedval = MAX(MIN(0.0, copy.getUpperBounds( )[ var ]), copy.getLowerBounds( )[ var ]);
                     }
                     else
                     {
                        if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral))
                           fixedval = num.round(solution.primal[ var ]);
                        else
                           fixedval = solution.primal[ var ];
                     }

                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf))
                     {
                        copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] -= data.getValues( )[ index ] * fixedval;
                        batches_rhs.push_back({ row, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] });
                     }
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf))
                     {
                        copy.getConstraintMatrix( ).getRightHandSides( )[ row ] -=
                              data.getValues( )[ index ] * fixedval;
                        batches_rhs.push_back({ row, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] });

                     }
                     batches_coeff.addEntry(row, var, fixedval);
                  }
               }
               ++nbatch;
            }
            if( nbatch == 0 )
               continue;

            if( nbatch >= 1 && ( nbatch >= batchsize || row >= copy.getNRows( ) - 1 ))
            {
               if(!batches_coeff.empty())
                  copy.getConstraintMatrix().changeCoefficients(batches_coeff);
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg,originalSolverStatus) != BuggerStatus::kFail )
               {
                  copy = Problem<double>(problem);
                  SmallVec<int, 32> buffer;
                  if( !applied_entries.empty( ))
                     copy.getConstraintMatrix( ).changeCoefficients(applied_entries);
                  for( const auto &item: applied_reductions_lhs )
                     copy.getConstraintMatrix( ).getLeftHandSides( )[ item.first ] = item.second;
                  for( const auto &item: applied_reductions_rhs )
                     copy.getConstraintMatrix( ).getRightHandSides( )[ item.first ] = item.second;
               }

               else
               {
                  for( const auto &item: batches_lhs )
                     applied_reductions_lhs.emplace_back(item);
                  for( const auto &item: batches_rhs )
                     applied_reductions_rhs.emplace_back(item);
                  nchgsides += batches_lhs.size( ) + batches_rhs.size( );
                  SmallVec<int, 32> buffer;
                  const MatrixEntry<double> *iter = batches_coeff.template begin<true>(buffer);
                  if( !batches_coeff.empty())
                     while( iter != batches_coeff.end( ))
                     {
                        applied_entries.addEntry(iter->row, iter->col, iter->val);
                        iter = batches_coeff.template next<true>( buffer );
                        nchgcoefs++;
                     }
                  result = ModulStatus::kSuccessful;
               }
               nbatch = 0;
               batches_rhs.clear( );
               batches_lhs.clear( );
               batches_coeff.clear( );
               batches_rhs.reserve(batchsize);
               batches_lhs.reserve(batchsize);
            }
         }
         problem = Problem<double>(copy);
         return result;
      }
   };


} // namespace bugger

#endif
