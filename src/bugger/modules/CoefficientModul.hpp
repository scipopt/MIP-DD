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
#include "bugger/interfaces/Status.hpp"

namespace bugger {

   class CoefficientModul : public BuggerModul {
   public:
      CoefficientModul(const Message &_msg) : BuggerModul( ) {
         this->setName("coefficient");
         this->msg = _msg;
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
            if( is_lb_smaller_than_ub(domains, var))
               return true;
         }
         return false;
      }

      bool is_lb_smaller_than_ub(const VariableDomains<double> &domains, int var) const {
         return domains.flags[ var ].test(ColFlag::kUbInf) || domains.flags[ var ].test(ColFlag::kLbInf)
                || num.isLT(domains.lower_bounds[ var ], domains.upper_bounds[ var ]);
      }


      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

//         auto copy = Problem<double>(problem);
//         Vec<std::pair<int, double>> applied_reductions{};
//         Vec<std::pair<int, double>> batches{};
//         int batchsize = 1;
//
//         if( options.nbatches > 0 )
//         {
//            batchsize = options.nbatches - 1;
//
//            for( int i = problem.getNRows() - 1; i >= 0; --i )
//               if( SCIPisCoefficientAdmissible(problem, i) )
//                  ++batchsize;
//            batchsize /= options.nbatches;
//         }
//
//         int nbatch = 0;
//
//
//         for( int row = copy.getNRows() ; row >= 0; --row )
//         {
//            if( SCIPisCoefficientAdmissible(copy, row) )
//            {
//
//               auto data = problem.getConstraintMatrix().getRowCoefficients(row);
//               for( int j = 0, batch[nbatch].nvars = 0; j < consdata.nvars; ++j )
//               if( is_lb_smaller_than_ub(copy.getVariableDomains()) )
//                  ++batch[nbatch].nvars;
//
//               inds[nbatch][0] = row;
//               batch[nbatch].lhs = consdata.lhs;
//               batch[nbatch].rhs = consdata.rhs;
//
//
//
//               for( int j = data.getLength() - 1, k = 0; k < batch[nbatch].nvars; --j )
//               {
//                  //@Dominik du hattest hier SCIPisGE(lb, ub), was nicht so viel Sinn macht?
//                  if( SCIPisGE(scip, consdata.vars[j]->data.original.origdom.lb, consdata.vars[j]->data.original.origdom.ub) )
//                  {
//                     SCIP_Real fixedval;
//
//                     inds[nbatch][k + 1] = j;
//                     batch[nbatch].vars[k] = consdata.vars[j];
//                     batch[nbatch].vals[k] = consdata.vals[j];
//
//                     if( solution_exists )
//                     {
//                        if( SCIPvarIsIntegral(consdata.vars[j]) )
//                           fixedval = MAX(MIN(0.0, SCIPfloor(scip, consdata.vars[j]->data.original.origdom.ub)), SCIPceil(scip, consdata.vars[j]->data.original.origdom.lb));
//                        else
//                           fixedval = MAX(MIN(0.0, consdata.vars[j]->data.original.origdom.ub), consdata.vars[j]->data.original.origdom.lb);
//                     }
//                     else
//                     {
//                        if( SCIPvarIsIntegral(consdata.vars[j]) )
//                           fixedval = SCIPround(scip, SCIPgetSolVal(scip, iscip.get_solution(), consdata.vars[j]));
//                        else
//                           fixedval = SCIPgetSolVal(scip, iscip.get_solution(), consdata.vars[j]);
//                     }
//
//                     if( !SCIPisInfinity(scip, -consdata.lhs) )
//                        consdata.lhs -= consdata.vals[j] * fixedval;
//
//                     if( !SCIPisInfinity(scip, consdata.rhs) )
//                        consdata.rhs -= consdata.vals[j] * fixedval;
//
//                     --consdata.nvars;
//                     consdata.vars[j] = consdata.vars[consdata.nvars];
//                     consdata.vals[j] = consdata.vals[consdata.nvars];
//                     ++k;
//                  }
//               }
//
//               ( SCIPconsSetDataLinear(cons, &consdata) );
//               ++nbatch;
//            }
//
//            if( nbatch >= 1 && ( nbatch >= batchsize || row <= 0 ) )
//            {
//               if( iscip.runSCIP() != Status::kSuccess )
//               {
//                  for( int j = nbatch - 1; j >= 0; --j )
//                  {
//                     SCIP_CONSDATALINEAR consdata;
//
//                     cons = conss[inds[j][0]];
//                     ( SCIPconsGetDataLinear(cons, &consdata) );
//                     consdata.lhs = batch[j].lhs;
//                     consdata.rhs = batch[j].rhs;
//
//                     for( int k = batch[j].nvars - 1; k >= 0; --k )
//                     {
//                        consdata.vars[inds[j][k + 1]] = batch[j].vars[k];
//                        consdata.vals[inds[j][k + 1]] = batch[j].vals[k];
//                        ++consdata.nvars;
//                     }
//
//                     ( SCIPconsSetDataLinear(cons, &consdata) );
//                  }
//               }
//               else
//               {
//                  for( int j = 0; j < nbatch; ++j )
//                  {
//                     for( int k = 0; k < batch[j].nvars; ++k )
//                     {
//                        ( SCIPreleaseVar(scip, &batch[j].vars[k]) );
//                        nchgcoefs++;
//                     }
//                  }
//                  result = ModulStatus::kSuccessful;
//               }
//
//               for( int j = nbatch - 1; j >= 0; --j )
//               {
//                  SCIPfreeBufferArray(scip, &batch[j].vals);
//                  SCIPfreeBufferArray(scip, &batch[j].vars);
//                  SCIPfreeBufferArray(scip, &inds[j]);
//               }
//
//               nbatch = 0;
//            }
//         }
//
//         SCIPfreeBufferArray(scip, &batch);
//         SCIPfreeBufferArray(scip, &inds);
         return result;
      }
   };


} // namespace bugger

#endif
