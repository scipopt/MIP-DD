/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_INTERFACES_SCIPRATIONALINTERFACE_HPP__
#define __BUGGER_INTERFACES_SCIPRATIONALINTERFACE_HPP__

#include "ScipInterface.hpp"
#include "scip/struct_rational.h"


namespace bugger
{
   /* set a scip rational to the value of another type */
   template <typename REAL>
   SCIP_RETCODE
   SCIPratSetReal(
      SCIP_Rational*        res,                /**< the result */
      REAL                  real                /**< real to set from */
      )
   {
      assert(res != NULL);

      res->val = scip_rational::Rational(real);
      res->isinf = isinf(real);
      res->isfprepresentable = num_traits<REAL>::is_floating_point ? SCIP_ISFPREPRESENTABLE_TRUE : SCIP_ISFPREPRESENTABLE_UNKNOWN;

      return SCIP_OKAY;
   }

   /* EXPERIMENTAL: like exact SCIP, this is not ready yet, and must be used carefully */
   template <typename REAL>
   class ScipRationalInterface : public ScipInterface<REAL>
   {
   public:

      explicit ScipRationalInterface(const Message& _msg, const ScipParameters& _parameters,
                                     const HashMap<String, char>& _limits) :
                                     ScipInterface<REAL>(_msg, _parameters, _limits)
      { }

      void
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) override
      {
         this->adjustment = &settings;
         this->model = &problem;
         this->reference = &solution;
         int ncols = this->model->getNCols( );
         int nrows = this->model->getNRows( );
         bool solution_exists = solution.status == SolutionStatus::kFeasible && solution.primal.size() == ncols;
         const auto& varNames = this->model->getVariableNames( );
         const auto& consNames = this->model->getConstraintNames( );
         const auto& domains = this->model->getVariableDomains( );
         const auto& obj = this->model->getObjective( );
         const auto& consMatrix = this->model->getConstraintMatrix( );
         const auto& lhs_values = consMatrix.getLeftHandSides( );
         const auto& rhs_values = consMatrix.getRightHandSides( );
         const auto& cflags = this->model->getColFlags( );
         const auto& rflags = this->model->getRowFlags( );
         const auto& rtypes = this->model->getConstraintTypes( );
         SCIP_Rational lower;
         SCIP_Rational upper;
         SCIP_Rational objval;

         this->set_parameters( );
         SCIP_CALL_ABORT(SCIPcreateProbBasic(this->scip, this->model->getName( ).c_str()));
         SCIP_CALL_ABORT(SCIPratSetReal(&objval, obj.offset));
         SCIP_CALL_ABORT(SCIPaddOrigObjoffsetExact(this->scip, &objval));
         SCIP_CALL_ABORT(SCIPsetObjsense(this->scip, obj.sense ? SCIP_OBJSENSE_MINIMIZE : SCIP_OBJSENSE_MAXIMIZE));
         this->vars.resize(ncols);
         if( solution_exists )
            this->value = this->model->getPrimalObjective(solution);
         else if( this->reference->status == SolutionStatus::kUnbounded )
            this->value = obj.sense ? -SCIPinfinity(this->scip) : SCIPinfinity(this->scip);
         else if( this->reference->status == SolutionStatus::kInfeasible )
            this->value = obj.sense ? SCIPinfinity(this->scip) : -SCIPinfinity(this->scip);

         for( int col = 0; col < ncols; ++col )
         {
            this->vars[col] = NULL;
            if( cflags[col].test(ColFlag::kFixed) )
               continue;
            SCIP_VAR* var;
            SCIP_VARTYPE type;
            SCIP_CALL_ABORT(SCIPratSetReal(&lower, cflags[col].test(ColFlag::kLbInf)
                                                   ? REAL(-SCIPinfinity(this->scip))
                                                   : domains.lower_bounds[col]));
            SCIP_CALL_ABORT(SCIPratSetReal(&upper, cflags[col].test(ColFlag::kUbInf)
                                                   ? REAL(SCIPinfinity(this->scip))
                                                   : domains.upper_bounds[col]));
            SCIP_CALL_ABORT(SCIPratSetReal(&objval, obj.coefficients[col]));
            assert(!cflags[col].test(ColFlag::kInactive) || lower.val == upper.val);
            if( cflags[col].test(ColFlag::kIntegral) )
            {
               if( lower.val >= 0 && upper.val <= 1 )
                  type = SCIP_VARTYPE_BINARY;
               else
                  type = SCIP_VARTYPE_INTEGER;
            }
            else if( cflags[col].test(ColFlag::kImplInt) )
               type = SCIP_VARTYPE_IMPLINT;
            else
               type = SCIP_VARTYPE_CONTINUOUS;
            SCIP_CALL_ABORT(SCIPcreateVarBasic(this->scip, &var, varNames[col].c_str(), 0.0, 0.0, 0.0, type));
            SCIP_CALL_ABORT(SCIPaddVarExactData(this->scip, var, &lower, &upper, &objval));
            this->vars[col] = var;
            SCIP_CALL_ABORT(SCIPaddVar(this->scip, var));
            SCIP_CALL_ABORT(SCIPreleaseVar(this->scip, &var));
         }

         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Rational> consvals(ncols);
         Vec<SCIP_Rational*> consvalsptrs(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[row].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[row].test(RowFlag::kLhsInf) || !rflags[row].test(RowFlag::kRhsInf));
            //TODO: Setup special constraints
            assert(rtypes[row] == ConstraintType::kLinear);
            const auto& rowvec = consMatrix.getRowCoefficients(row);
            const auto& rowinds = rowvec.getIndices( );
            const auto& rowvals = rowvec.getValues( );
            int nrowcols = rowvec.getLength( );
            SCIP_CONS* cons;
            SCIP_CALL_ABORT(SCIPratSetReal(&lower, rflags[row].test(RowFlag::kLhsInf)
                                                   ? REAL(-SCIPinfinity(this->scip))
                                                   : lhs_values[row]));
            SCIP_CALL_ABORT(SCIPratSetReal(&upper, rflags[row].test(RowFlag::kRhsInf)
                                                   ? REAL(SCIPinfinity(this->scip))
                                                   : rhs_values[row]));
            for( int i = 0; i < nrowcols; ++i )
            {
               assert(!cflags[rowinds[i]].test(ColFlag::kFixed));
               assert(rowvals[i] != 0);
               consvars[i] = this->vars[rowinds[i]];
               SCIP_CALL_ABORT(SCIPratSetReal(&consvals[i], rowvals[i]));
               consvalsptrs[i] = &consvals[i];
            }
            SCIP_CALL_ABORT(SCIPcreateConsBasicExactLinear(this->scip, &cons, consNames[row].c_str(), nrowcols,
                  consvars.data( ), consvalsptrs.data( ), &lower, &upper));
            SCIP_CALL_ABORT(SCIPaddCons(this->scip, cons));
            SCIP_CALL_ABORT(SCIPreleaseCons(this->scip, &cons));
         }

         if( solution_exists )
         {
            if( this->parameters.cutoffrelax >= 0.0 && this->parameters.cutoffrelax < SCIPinfinity(this->scip) )
               //TODO: Set rational limit
               SCIP_CALL_ABORT(SCIPsetObjlimit(this->scip, max(min(SCIP_Real(obj.sense ? this->parameters.cutoffrelax : -this->parameters.cutoffrelax) + SCIP_Real(this->value), SCIPinfinity(this->scip)), -SCIPinfinity(this->scip))));
            if( this->parameters.set_dual_limit || this->parameters.set_prim_limit )
            {
               for( const auto& pair : this->limits )
               {
                  switch( pair.second )
                  {
                  case DUAL:
                     SCIP_CALL_ABORT(SCIPsetRealParam(this->scip, pair.first.c_str(), SCIP_Real(this->relax( this->value, obj.sense, 2 * SCIPsumepsilon(this->scip), SCIPinfinity(this->scip) ))));
                     break;
                  case PRIM:
                     SCIP_CALL_ABORT(SCIPsetRealParam(this->scip, pair.first.c_str(), SCIP_Real(this->value)));
                     break;
                  }
               }
            }
         }
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override
      {
         char retcode = SCIP_ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;

         if( this->msg.getVerbosityLevel() < VerbosityLevel::kDetailed )
            SCIPsetMessagehdlrQuiet(this->scip, TRUE);
         else
            SCIPsetIntParam(this->scip, ScipParameters::VERB.c_str(), 5);

         // optimize
         if( this->parameters.mode == -1 )
            retcode = SCIPsolve(this->scip);
         // count
         else
         {
            retcode = SCIPsetParamsCountsols(this->scip);
            if( retcode == SCIP_OKAY )
               retcode = SCIPcount(this->scip);
         }

         if( retcode == SCIP_OKAY )
         {
            SCIP_Rational lower;
            SCIP_Rational upper;
            SCIP_Rational objval;

            // reset return code
            retcode = SolverRetcode::OKAY;

            if( this->parameters.mode == -1 )
            {
               // retrieve enabled checks
               bool dual = true;
               bool primal = true;
               bool objective = true;

               for( int passcode: passcodes )
               {
                  switch( passcode )
                  {
                  case SolverRetcode::DUALFAIL:
                     dual = false;
                     break;
                  case SolverRetcode::PRIMALFAIL:
                     primal = false;
                     break;
                  case SolverRetcode::OBJECTIVEFAIL:
                     objective = false;
                     break;
                  }
               }

               // declare primal solution
               Vec<Solution<REAL>> solution;
               SCIP_SOL** sols = SCIPgetSols(this->scip);
               int nsols = SCIPgetNSols(this->scip);

               // check dual by reference solution objective
               if( retcode == SolverRetcode::OKAY && dual )
               {
                  SCIPgetDualboundExact(this->scip, &lower);
                  retcode = this->check_dual_bound( REAL(lower.val), REAL(SCIPsumepsilon(this->scip)), REAL(SCIPinfinity(this->scip)) );
               }

               // check primal by generated solution values
               if( retcode == SolverRetcode::OKAY )
               {
                  if( nsols >= 1 )
                  {
                     solution.resize(primal ? nsols : objective ? 1 : 0);

                     for( int i = solution.size() - 1; i >= 0; --i )
                        translateSolution(sols[i], i == 0 && SCIPhasPrimalRay(this->scip), *this->model, solution[i]);

                     if( primal )
                        retcode = this->check_primal_solution( solution, REAL(SCIPsumepsilon(this->scip)), REAL(SCIPinfinity(this->scip)) );
                  }
                  else if( nsols != 0 && primal )
                     retcode = SolverRetcode::PRIMALFAIL;
               }

               // check objective by best solution evaluation
               if( retcode == SolverRetcode::OKAY && objective )
               {
                  // instead of primal bound use solution objective if no ray or infinity value if objective limit
                  SCIPgetPrimalboundExact(this->scip, &upper);
                  REAL bound { upper.val };

                  if( abs(bound) == REAL(SCIPinfinity(this->scip)) && solution.size() >= 1 && solution[0].status == SolutionStatus::kFeasible )
                  {
                     SCIPgetSolOrigObjExact(this->scip, sols[0], &objval);
                     bound = REAL(objval.val);
                  }
                  else if( this->parameters.cutoffrelax >= 0.0 && this->parameters.cutoffrelax < SCIPinfinity(this->scip) )
                  {
                     if( this->model->getObjective( ).sense )
                     {
                        //TODO: Get rational limit
                        if( SCIP_Real(upper.val) >= SCIP_Real(this->relax( SCIP_Real(this->parameters.cutoffrelax) + SCIP_Real(this->value), false, SCIPepsilon(this->scip), SCIPinfinity(this->scip) )) )
                        {
                           if( solution.size() >= 1 )
                           {
                              SCIPgetSolOrigObjExact(this->scip, sols[0], &objval);
                              bound = REAL(objval.val);
                           }
                           else
                              bound = REAL(SCIPinfinity(this->scip));
                        }
                     }
                     else
                     {
                        //TODO: Get rational limit
                        if( SCIP_Real(upper.val) <= SCIP_Real(this->relax( SCIP_Real(-this->parameters.cutoffrelax) + SCIP_Real(this->value), true, SCIPepsilon(this->scip), SCIPinfinity(this->scip) )) )
                        {
                           if( solution.size() >= 1 )
                           {
                              SCIPgetSolOrigObjExact(this->scip, sols[0], &objval);
                              bound = REAL(objval.val);
                           }
                           else
                              bound = REAL(-SCIPinfinity(this->scip));
                        }
                     }
                  }

                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = this->check_objective_value( bound, solution[0], REAL(SCIPsumepsilon(this->scip)), REAL(SCIPinfinity(this->scip)) );
               }
            }
            else
            {
               // check count by primal solution existence
               if( retcode == SolverRetcode::OKAY )
               {
                  SCIP_Bool valid;
                  long long count = SCIPgetNCountedSols(this->scip, &valid);

                  SCIPgetDualboundExact(this->scip, &lower);
                  SCIPgetPrimalboundExact(this->scip, &upper);
                  retcode = this->check_count_number( REAL(lower.val), REAL(upper.val), valid ? count : -1, REAL(SCIPinfinity(this->scip)) );
               }
            }

            // translate solver status
            switch( SCIPgetStatus(this->scip) )
            {
            case SCIP_STATUS_UNKNOWN:
               solverstatus = SolverStatus::kUnknown;
               break;
            case SCIP_STATUS_USERINTERRUPT:
               solverstatus = SolverStatus::kInterrupt;
               break;
#if SCIP_APIVERSION >= 22
            case SCIP_STATUS_TERMINATE:
               solverstatus = SolverStatus::kTerminate;
               break;
#endif
            case SCIP_STATUS_NODELIMIT:
               solverstatus = SolverStatus::kNodeLimit;
               break;
            case SCIP_STATUS_TOTALNODELIMIT:
               solverstatus = SolverStatus::kTotalNodeLimit;
               break;
            case SCIP_STATUS_STALLNODELIMIT:
               solverstatus = SolverStatus::kStallNodeLimit;
               break;
            case SCIP_STATUS_TIMELIMIT:
               solverstatus = SolverStatus::kTimeLimit;
               break;
            case SCIP_STATUS_MEMLIMIT:
               solverstatus = SolverStatus::kMemLimit;
               break;
            case SCIP_STATUS_GAPLIMIT:
               solverstatus = SolverStatus::kGapLimit;
               break;
#if SCIP_APIVERSION >= 115
            case SCIP_STATUS_PRIMALLIMIT:
               solverstatus = SolverStatus::kPrimalLimit;
               break;
            case SCIP_STATUS_DUALLIMIT:
               solverstatus = SolverStatus::kDualLimit;
               break;
#endif
            case SCIP_STATUS_SOLLIMIT:
               solverstatus = SolverStatus::kSolLimit;
               break;
            case SCIP_STATUS_BESTSOLLIMIT:
               solverstatus = SolverStatus::kBestSolLimit;
               break;
            case SCIP_STATUS_RESTARTLIMIT:
               solverstatus = SolverStatus::kRestartLimit;
               break;
            case SCIP_STATUS_OPTIMAL:
               solverstatus = SolverStatus::kOptimal;
               break;
            case SCIP_STATUS_INFEASIBLE:
               solverstatus = SolverStatus::kInfeasible;
               break;
            case SCIP_STATUS_UNBOUNDED:
               solverstatus = SolverStatus::kUnbounded;
               break;
            case SCIP_STATUS_INFORUNBD:
               solverstatus = SolverStatus::kInfeasibleOrUnbounded;
               break;
            }

            // only do certificate stuff if problem was not solved in presolving
            if( retcode == SolverRetcode::OKAY && this->parameters.certificate && SCIPgetNNodes(this->scip) > 0 )
            {
               SCIPfreeTransform(this->scip);

               if( system("viprcomp certificate.vipr >/dev/null") )
                  retcode = SolverRetcode::COMPLETIONFAIL;
               else if( system("viprchk certificate_complete.vipr >/dev/null") )
                  retcode = SolverRetcode::CERTIFICATIONFAIL;
            }
         }
         else
         {
            // shift retcodes so that all errors have negative values
            --retcode;
         }
         // interpret certain passcodes as OKAY based on the user preferences
         for( int passcode: passcodes )
         {
            if( passcode == retcode )
            {
               retcode = SolverRetcode::OKAY;
               break;
            }
         }
         // restrict limit settings
         if( retcode != SolverRetcode::OKAY )
         {
            const auto& limitsettings = this->adjustment->getLimitSettings( );
            switch( SCIPgetStage(this->scip) )
            {
            case SCIP_STAGE_TRANSFORMING:
            case SCIP_STAGE_TRANSFORMED:
            case SCIP_STAGE_INITPRESOLVE:
            case SCIP_STAGE_PRESOLVING:
            case SCIP_STAGE_EXITPRESOLVE:
            case SCIP_STAGE_PRESOLVED:
            case SCIP_STAGE_INITSOLVE:
            case SCIP_STAGE_SOLVING:
            case SCIP_STAGE_SOLVED:
            case SCIP_STAGE_EXITSOLVE:
               for( int index = 0; index < limitsettings.size( ); ++index )
               {
                  if( limitsettings[index].second < 0 || limitsettings[index].second > 1 )
                  {
                     double bound;
                     switch( this->limits.find(limitsettings[index].first)->second )
                     {
                     case BEST:
                        // incremented to continue after finding the last best solution
                        bound = ceil(max((1.0 + this->parameters.limitspace) * SCIPgetNBestSolsFound(this->scip) + 1.0, 1.0));
                        if( bound > INT_MAX )
                           continue;
                        else
                           break;
                     case SOLU:
                        // incremented to continue after finding the last solution
                        bound = ceil(max((1.0 + this->parameters.limitspace) * SCIPgetNSolsFound(this->scip) + 1.0, 1.0));
                        if( bound > INT_MAX )
                           continue;
                        else
                           break;
                     case REST:
                        // decremented from runs to restarts
                        bound = ceil(max((1.0 + this->parameters.limitspace) * (SCIPgetNRuns(this->scip) - 1.0), 1.0));
                        if( bound > INT_MAX )
                           continue;
                        else
                           break;
                     case TOTA:
                        // assumes last node is processed
                        bound = ceil(max((1.0 + this->parameters.limitspace) * SCIPgetNTotalNodes(this->scip), 1.0));
                        if( bound > LONG_MAX )
                           continue;
                        else
                           break;
                     case TIME:
                        // sensitive to processor speed variability
                        bound = ceil(max((1.0 + this->parameters.limitspace) * SCIPgetSolvingTime(this->scip), 1.0));
                        if( bound > LLONG_MAX )
                           continue;
                        else
                           break;
                     case DUAL:
                     case PRIM:
                     default:
                        SCIPerrorMessage("unknown limit type\n");
                        assert(false);
                        continue;
                     }
                     if( limitsettings[index].second < 0 || bound < limitsettings[index].second )
                     {
                        this->msg.info("\t\t{} = {}\n", limitsettings[index].first, (long long)bound);
                        this->adjustment->setLimitSettings(index, bound);
                     }
                  }
               }
               break;
            case SCIP_STAGE_INIT:
            case SCIP_STAGE_PROBLEM:
            case SCIP_STAGE_FREETRANS:
            case SCIP_STAGE_FREE:
            default:
               break;
            }
         }
         return { retcode, solverstatus };
      }

      std::tuple<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>, boost::optional<Solution<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename, const String& solution_filename) override
      {
         auto parsed_settings = this->parseSettings(settings_filename);
         if( !parsed_settings )
            return { boost::none, boost::none, boost::none };
         SolverSettings settings { parsed_settings.get() };
         if( SCIPreadProb(this->scip, problem_filename.c_str(), NULL) != SCIP_OKAY )
            return { settings, boost::none, boost::none };
         ProblemBuilder<REAL> builder;
         SCIP_Bool success = TRUE;
         SCIP_Rational* lower;
         SCIP_Rational* upper;
         SCIP_Rational* objval;

         // set problem name
         builder.setProblemName(SCIPgetProbName(this->scip));
         // set objective offset
         objval = SCIPgetOrigObjoffsetExact(this->scip);
         builder.setObjOffset(REAL(objval->val));
         // set objective sense
         builder.setObjSense(SCIPgetObjsense(this->scip) == SCIP_OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = SCIPgetNVars(this->scip);
         int nrows = SCIPgetNConss(this->scip);
         int nnz = 0;
         this->vars.clear();
         this->vars.insert(this->vars.end(), SCIPgetVars(this->scip), SCIPgetVars(this->scip) + ncols);
         SCIP_CONS** conss = SCIPgetConss(this->scip);
         for( int row = 0; row < nrows; ++row )
         {
            int nrowcols = 0;
            SCIPgetConsNVars(this->scip, conss[row], &nrowcols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            nnz += nrowcols;
         }
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int col = 0; col < ncols; ++col )
         {
            SCIP_VAR* var = this->vars[col];
            SCIP_VARTYPE vartype = SCIPvarGetType(var);
            lower = SCIPvarGetLbGlobalExact(var);
            upper = SCIPvarGetUbGlobalExact(var);
            objval = SCIPvarGetObjExact(var);
            builder.setColLb(col, REAL(lower->val));
            builder.setColUb(col, REAL(upper->val));
            builder.setColLbInf(col, SCIPisInfinity(this->scip, SCIP_Real(-lower->val)));
            builder.setColUbInf(col, SCIPisInfinity(this->scip, SCIP_Real(upper->val)));
            builder.setColIntegral(col, vartype == SCIP_VARTYPE_BINARY || vartype == SCIP_VARTYPE_INTEGER);
            builder.setColImplInt(col, vartype != SCIP_VARTYPE_CONTINUOUS);
            builder.setObj(col, REAL(objval->val));
            builder.setColName(col, SCIPvarGetName(var));
         }

         // set up rows
         builder.setNumRows(nrows);
         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Rational*> consvals(ncols);
         Vec<int> rowinds(ncols);
         Vec<REAL> rowvals(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            //TODO: Store special constraints
            SCIP_CONS* cons = conss[row];
            REAL lhs { SCIPconsGetLhsExact(this->scip, cons, &success)->val };
            if( !success )
               return { settings, boost::none, boost::none };
            REAL rhs { SCIPconsGetRhsExact(this->scip, cons, &success)->val };
            if( !success )
               return { settings, boost::none, boost::none };
            int nrowcols = 0;
            SCIPgetConsNVars(this->scip, cons, &nrowcols, &success);
            SCIPgetConsVars(this->scip, cons, consvars.data(), ncols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            SCIPgetConsValsExact(this->scip, cons, consvals.data(), ncols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            for( int i = 0; i < nrowcols; ++i )
            {
               rowvals[i] = REAL(consvals[i]->val);
               if( SCIPvarIsNegated(consvars[i]) )
               {
                  consvars[i] = SCIPvarGetNegatedVar(consvars[i]);
                  rowvals[i] *= -1;
                  if( !SCIPisInfinity(this->scip, SCIP_Real(-lhs)) )
                     lhs += rowvals[i];
                  if( !SCIPisInfinity(this->scip, SCIP_Real(rhs)) )
                     rhs += rowvals[i];
               }
               rowinds[i] = SCIPvarGetProbindex(consvars[i]);
            }
            builder.setRowLhs(row, lhs);
            builder.setRowRhs(row, rhs);
            builder.setRowLhsInf(row, SCIPisInfinity(this->scip, SCIP_Real(-lhs)));
            builder.setRowRhsInf(row, SCIPisInfinity(this->scip, SCIP_Real(rhs)));
            builder.addRowEntries(row, nrowcols, rowinds.data(), rowvals.data());
            builder.setRowName(row, SCIPconsGetName(cons));
         }

         Problem<REAL> problem { builder.build() };
         Solution<REAL> solution { };
         if( !solution_filename.empty() )
         {
            SCIP_SOL* sol = NULL;
            SCIP_Bool error = TRUE;
            if( success && SCIPcreateSolExact(this->scip, &sol, NULL) != SCIP_OKAY )
            {
               sol = NULL;
               success = FALSE;
            }
            if( success && ( SCIPreadSolFile(this->scip, solution_filename.c_str(), sol, FALSE, NULL, &error) != SCIP_OKAY || error ) )
               success = FALSE;
            if( success )
               translateSolution(sol, FALSE, problem, solution);
            if( sol != NULL )
               SCIPfreeSol(this->scip, &sol);
            if( !success )
               return { settings, problem, boost::none };
         }

         return { settings, problem, solution };
      }

      std::tuple<bool, bool, bool>
      writeInstance(const String& filename, const bool& writesettings, const bool& writesolution) const override
      {
         bool successsettings = ( !writesettings && this->limits.size() == 0 ) || SCIPwriteParams(this->scip, (filename + ".set").c_str(), FALSE, TRUE) == SCIP_OKAY;
         bool successproblem = SCIPwriteOrigProblem(this->scip, (filename + ".cip").c_str(), NULL, FALSE) == SCIP_OKAY;
         bool successsolution = true;

         if( writesolution && this->reference->primal.size() == this->model->getNCols() )
         {
            SCIP_SOL* sol = NULL;
            FILE* file = NULL;
            SCIP_Rational solval;
            if( successsolution && SCIPcreateSolExact(this->scip, &sol, NULL) != SCIP_OKAY )
            {
               sol = NULL;
               successsolution = false;
            }
            for( int col = 0; successsolution && col < this->reference->primal.size(); ++col )
            {
               if( !this->model->getColFlags()[col].test(ColFlag::kFixed) && ( SCIPratSetReal(&solval, this->reference->primal[col]) != SCIP_OKAY || SCIPsetSolValExact(this->scip, sol, this->vars[col], &solval) != SCIP_OKAY ) )
                  successsolution = false;
            }
            if( successsolution && ( (file = fopen((filename + ".sol").c_str(), "w")) == NULL || SCIPprintSol(this->scip, sol, file, FALSE) != SCIP_OKAY ) )
               successsolution = false;
            if( file != NULL )
               fclose(file);
            if( sol != NULL )
               SCIPfreeSol(this->scip, &sol);
         }

         return { successsettings, successproblem, successsolution };
      }

      ~ScipRationalInterface( ) override = default;

   private:

      void
      translateSolution(SCIP_SOL* const sol, SCIP_Bool ray, const Problem<REAL>& problem, Solution<REAL>& solution) const
      {
         SCIP_Rational solval;
         solution.status = ray ? SolutionStatus::kUnbounded : SolutionStatus::kFeasible;
         solution.primal.resize(problem.getNCols());
         for( int col = 0; col < solution.primal.size(); ++col )
         {
            if( problem.getColFlags()[col].test(ColFlag::kFixed) )
               solution.primal[col] = std::numeric_limits<REAL>::signaling_NaN();
            else
            {
               SCIPgetSolValExact(this->scip, sol, this->vars[col], &solval);
               solution.primal[col] = REAL(solval.val);
            }
         }
         if( ray )
         {
            solution.ray.resize(problem.getNCols());
            for( int col = 0; col < solution.ray.size(); ++col )
               //TODO: Get rational ray
               solution.ray[col] = problem.getColFlags()[col].test(ColFlag::kFixed) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(SCIPgetPrimalRayVal(this->scip, this->vars[col]));
         }
      }
   };

} // namespace bugger

#endif
