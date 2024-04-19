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

#ifndef __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "soplex.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


using namespace soplex;

namespace bugger
{

   class SoplexParameters
   {

   public:

      int mode = -1;
      double limitspace = 1.0;
      bool set_dual_limit = false;
      bool set_prim_limit = false;
      bool set_iter_limit = false;
      bool set_time_limit = false;
   };

   class SoplexInterface : public SolverInterface
   {

   public:

      enum Limit : char
      {
         DUAL = 1,
         PRIM = 2,
         ITER = 3,
         TIME = 4
      };

   private:

      SoplexParameters& parameters;
      HashMap<String, char>& limits;
      SoPlex* soplex;
      NameSet colNames { };
      NameSet rowNames { };
      Vec<int> inds;

   public:

      explicit SoplexInterface(const Message& _msg, SoplexParameters& _parameters, HashMap<String, char>& _limits) :
                               SolverInterface(_msg), parameters(_parameters), limits(_limits)
      {
         soplex = new SoPlex();
      }

      void
      print_header( ) const override
      {
         soplex->printVersion();
      }

      bool
      has_setting(const String& name) const override
      {
         switch( (unsigned char)name.front() )
         {
         case 0:
            return (unsigned char)name.back() < SoPlex::BOOLPARAM_COUNT;
            break;
         case 1:
            return (unsigned char)name.back() < SoPlex::INTPARAM_COUNT;
            break;
         case 2:
            return (unsigned char)name.back() < SoPlex::REALPARAM_COUNT;
            break;
         default:
            return false;
         }
      }

      boost::optional<SolverSettings>
      parseSettings(const String& filename) const override
      {
         if( !filename.empty() && !soplex->loadSettingsFile(filename.c_str()) )
            return boost::none;

         // include objective limits
         if( parameters.mode != -1 )
         {
            parameters.set_dual_limit = false;
            parameters.set_prim_limit = false;
         }
         else
         {
            if( parameters.set_dual_limit )
            {
               String name { 2, (char)(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_UPPER : SoPlex::OBJLIMIT_LOWER) };
               if( has_setting(name) )
                  limits[name] = SoplexInterface::DUAL;
               else
               {
                  msg.info("Dual limit disabled.\n");
                  parameters.set_dual_limit = false;
               }
            }
            if( parameters.set_prim_limit )
            {
               String name { 2, (char)(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_LOWER : SoPlex::OBJLIMIT_UPPER) };
               if( has_setting(name) )
                  limits[name] = SoplexInterface::PRIM;
               else
               {
                  msg.info("Primal limit disabled.\n");
                  parameters.set_prim_limit = false;
               }
            }
         }

         Vec<std::pair<String, bool>> bool_settings;
         Vec<std::pair<String, int>> int_settings;
         Vec<std::pair<String, long>> long_settings;
         Vec<std::pair<String, double>> double_settings;
         Vec<std::pair<String, char>> char_settings;
         Vec<std::pair<String, String>> string_settings;
         Vec<std::pair<String, long long>> limit_settings;

         for( int i = 0; i < SoPlex::BOOLPARAM_COUNT; ++i )
         {
            String name { 0, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
               case ITER:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               bool_settings.emplace_back( name, soplex->boolParam(SoPlex::BoolParam(i)) );
         }

         for( int i = 0; i < SoPlex::INTPARAM_COUNT; ++i )
         {
            String name { 1, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case ITER:
                  limit_settings.emplace_back( name, soplex->intParam(SoPlex::IntParam(i)) );
                  break;
               case DUAL:
               case PRIM:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               int_settings.emplace_back( name, soplex->intParam(SoPlex::IntParam(i)) );
         }

         for( int i = 0; i < SoPlex::REALPARAM_COUNT; ++i )
         {
            String name { 2, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
                  break;
               case TIME:
                  limit_settings.emplace_back( name, std::min(soplex->realParam(SoPlex::RealParam(i)), (double)LLONG_MAX) );
                  break;
               case ITER:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               double_settings.emplace_back( name, soplex->realParam(SoPlex::RealParam(i)) );
         }

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings, limit_settings);
      }

      void
      doSetUp(SolverSettings& settings, const Problem<double>& problem, const Solution<double>& solution) override
      {
         setup(settings, problem, solution);
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override
      {
         char retcode = SPxSolver::ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;
         if( msg.getVerbosityLevel() < VerbosityLevel::kDetailed )
            soplex->setIntParam(SoPlex::VERBOSITY, SoPlex::VERBOSITY_ERROR);
         // optimize
         if( parameters.mode == -1 )
            retcode = soplex->optimize();

         if( retcode > SPxSolver::NOT_INIT )
         {
            // translate solver status
            switch( retcode )
            {
               case SPxSolver::SINGULAR:
               case SPxSolver::NO_PROBLEM:
               case SPxSolver::REGULAR:
               case SPxSolver::RUNNING:
               case SPxSolver::UNKNOWN:
               case SPxSolver::OPTIMAL_UNSCALED_VIOLATIONS:
                  solverstatus = SolverStatus::kUnknown;
                  break;
               case SPxSolver::ABORT_CYCLING:
                  solverstatus = SolverStatus::kTerminate;
                  break;
               case SPxSolver::ABORT_TIME:
                  solverstatus = SolverStatus::kTimeLimit;
                  break;
               case SPxSolver::ABORT_ITER:
                  solverstatus = SolverStatus::kLimit;
                  break;
               case SPxSolver::ABORT_VALUE:
                  solverstatus = SolverStatus::kGapLimit;
                  break;
               case SPxSolver::OPTIMAL:
                  solverstatus = SolverStatus::kOptimal;
                  break;
               case SPxSolver::UNBOUNDED:
                  solverstatus = SolverStatus::kUnbounded;
                  break;
               case SPxSolver::INFEASIBLE:
                  solverstatus = SolverStatus::kInfeasible;
                  break;
               case SPxSolver::INForUNBD:
                  solverstatus = SolverStatus::kInfeasibleOrUnbounded;
                  break;
            }

            // reset return code
            retcode = OKAY;

            if( parameters.mode == -1 )
            {
               // retrieve enabled checks
               bool dual = true;
               bool primal = true;
               bool objective = true;

               for( int passcode: passcodes )
               {
                  switch( passcode )
                  {
                  case DUALFAIL:
                     dual = false;
                     break;
                  case PRIMALFAIL:
                     primal = false;
                     break;
                  case OBJECTIVEFAIL:
                     objective = false;
                     break;
                  }
               }

               // declare primal solution
               Vec<Solution<double>> solution;

               // check dual by reference solution objective
               if( retcode == OKAY && dual && soplex->isDualFeasible() )
                  retcode = check_dual_bound( soplex->objValueReal(), std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );

               // check primal by generated solution values
               if( retcode == OKAY && ( primal && soplex->isPrimalFeasible() || objective ) )
               {
                  solution.resize(1);
                  soplex->getPrimalReal(solution[0].primal.data(), solution[0].primal.size());
                  solution[0].status = SolutionStatus::kFeasible;
                  solution[0].primal.resize(inds.size());

                  for( int col = solution[0].primal.size() - 1, var = soplex->numCols(); col >= var; --col )
                     solution[0].primal[col] = model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : solution[0].primal[--var];

                  if( soplex->hasPrimalRay() )
                  {
                     soplex->getPrimalRayReal(solution[0].ray.data(), solution[0].ray.size());
                     solution[0].status = SolutionStatus::kUnbounded;
                     solution[0].ray.resize(inds.size());

                     for( int col = solution[0].ray.size() - 1, var = soplex->numCols(); col >= var; --col )
                        solution[0].ray[col] = model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : solution[0].ray[--var];
                  }

                  if( primal )
                     retcode = check_primal_solution( solution, std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );
               }

               // check objective by best solution evaluation
               if( retcode == OKAY && objective )
               {
                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = check_objective_value( soplex->objValueReal(), solution[0], std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );
               }
            }
         }
         else
         {
            // shift retcodes so that all errors have negative values
            retcode -= SPxSolver::NOT_INIT + 1;
         }
         // interpret certain passcodes as OKAY based on the user preferences
         for( int passcode: passcodes )
         {
            if( passcode == retcode )
            {
               retcode = OKAY;
               break;
            }
         }
         // restrict limit settings
         if( retcode != OKAY )
         {
            const auto& limitsettings = adjustment->getLimitSettings( );
            for( int index = 0; index < limitsettings.size( ); ++index )
            {
               if( limitsettings[index].second < 0 || limitsettings[index].second > 1 )
               {
                  double bound;
                  const char* name;
                  switch( limits.find(limitsettings[index].first)->second )
                  {
                  case ITER:
                     // assumes last iteration is finished
                     name = soplex->settings().intParam.name[(unsigned char)limitsettings[index].first.back()].data();
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * soplex->numIterations(), 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case TIME:
                     // sensitive to processor speed variability
                     name = soplex->settings().realParam.name[(unsigned char)limitsettings[index].first.back()].data();
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * soplex->solveTime(), 1.0));
                     if( bound > LLONG_MAX )
                        continue;
                     else
                        break;
                  case DUAL:
                  case PRIM:
                  default:
                     SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
                  }
                  if( limitsettings[index].second < 0 || bound < limitsettings[index].second )
                  {
                     msg.info("\t\t{} = {}\n", name, (long long)bound);
                     adjustment->setLimitSettings(index, bound);
                  }
               }
            }
         }
         return { retcode, solverstatus };
      }

      long long
      getSolvingEffort( ) override
      {
         if( soplex->status() != SPxSolver::NOT_INIT )
            return soplex->numIterations();
         else
            return -1;
      }

      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<double>>>
      readInstance(const String& settings_filename, const String& problem_filename) override
      {
         auto settings = parseSettings(settings_filename);
         if( !soplex->readFile(problem_filename.c_str(), &rowNames, &colNames) )
            return { settings, boost::none };
         ProblemBuilder<double> builder;

         //TODO: Set problem name

         // set objective offset
         builder.setObjOffset(soplex->realParam(SoPlex::OBJ_OFFSET));
         // set objective sense
         builder.setObjSense(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = soplex->numCols();
         int nrows = soplex->numRows();
         int nnz = soplex->numNonzeros();
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int i = 0; i < ncols; ++i )
         {
            double lb = soplex->lowerReal(i);
            double ub = soplex->upperReal(i);
            builder.setColLb(i, lb);
            builder.setColUb(i, ub);
            builder.setColLbInf(i, -lb >= soplex->realParam(SoPlex::INFTY));
            builder.setColUbInf(i, ub >= soplex->realParam(SoPlex::INFTY));
            builder.setObj(i, soplex->objReal(i));
            builder.setColName(i, colNames[i]);
         }

         // set up rows
         builder.setNumRows(nrows);
         Vec<int> consinds(ncols);
         Vec<double> consvals(ncols);
         for( int i = 0; i < nrows; ++i )
         {
            double lhs = soplex->lhsReal(i);
            double rhs = soplex->rhsReal(i);
            builder.setRowLhs(i, lhs);
            builder.setRowRhs(i, rhs);
            builder.setRowLhsInf(i, -lhs >= soplex->realParam(SoPlex::INFTY));
            builder.setRowRhsInf(i, rhs >= soplex->realParam(SoPlex::INFTY));
            DSVector cons;
            soplex->getRowVectorReal(i, cons);
            for( int j = 0; j < cons.size(); ++j )
            {
               consinds[ j ] = cons.index(j);
               consvals[ j ] = cons.value(j);
            }
            builder.addRowEntries(i, cons.size(), consinds.data(), consvals.data());
            builder.setRowName(i, rowNames[i]);
         }

         return { settings, builder.build() };
      }

      bool
      writeInstance(const String& filename, const bool& writesettings) override
      {
         if( writesettings || limits.size() >= 1 )
            soplex->saveSettingsFile((filename + ".set").c_str(), true);
         return soplex->writeFile((filename + ".mps").c_str(), &rowNames, &colNames);
      };

      ~SoplexInterface( ) override
      {
         delete soplex;
      }

   private:

      void
      setup(SolverSettings& settings, const Problem<double>& problem, const Solution<double>& solution)
      {
         adjustment = &settings;
         model = &problem;
         reference = &solution;
         bool solution_exists = reference->status == SolutionStatus::kFeasible;
         int ncols = model->getNCols( );
         int nrows = model->getNRows( );
         const auto& varNames = model->getVariableNames( );
         const auto& consNames = model->getConstraintNames( );
         const auto& domains = model->getVariableDomains( );
         const auto& obj = model->getObjective( );
         const auto& consMatrix = model->getConstraintMatrix( );
         const auto& lhs_values = consMatrix.getLeftHandSides( );
         const auto& rhs_values = consMatrix.getRightHandSides( );
         const auto& rflags = model->getRowFlags( );

         //TODO: Get problem name

         set_parameters( );
         soplex->setRealParam(SoPlex::OBJ_OFFSET, obj.offset);
         soplex->setIntParam(SoPlex::OBJSENSE, obj.sense ? SoPlex::OBJSENSE_MINIMIZE : SoPlex::OBJSENSE_MAXIMIZE);
         colNames.reMax(model->getNCols( ));
         rowNames.reMax(model->getNRows( ));
         inds.resize(model->getNCols( ));
         if( solution_exists )
            value = obj.offset;
         else if( reference->status == SolutionStatus::kUnbounded )
            value = obj.sense ? -soplex->realParam(SoPlex::INFTY) : soplex->realParam(SoPlex::INFTY);
         else if( reference->status == SolutionStatus::kInfeasible )
            value = obj.sense ? soplex->realParam(SoPlex::INFTY) : -soplex->realParam(SoPlex::INFTY);

         for( int col = 0; col < ncols; ++col )
         {
            if( domains.flags[ col ].test(ColFlag::kFixed) )
               inds[col] = -1;
            else
            {
               LPCol var { };
               double lb = domains.flags[ col ].test(ColFlag::kLbInf)
                           ? -soplex->realParam(SoPlex::INFTY)
                           : domains.lower_bounds[ col ];
               double ub = domains.flags[ col ].test(ColFlag::kUbInf)
                           ? soplex->realParam(SoPlex::INFTY)
                           : domains.upper_bounds[ col ];
               assert(!domains.flags[ col ].test(ColFlag::kInactive) || ( lb == ub ));
               var.setLower(lb);
               var.setUpper(ub);
               var.setObj(obj.coefficients[ col ]);
               inds[col] = soplex->numCols();
               soplex->addColReal(var);
               colNames.add(varNames[col].c_str());
               if( solution_exists )
                  value += obj.coefficients[ col ] * reference->primal[ col ];
            }
         }

         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[ row ].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[ row ].test(RowFlag::kLhsInf) || !rflags[ row ].test(RowFlag::kRhsInf));

            auto rowvec = consMatrix.getRowCoefficients(row);
            const int* rowinds = rowvec.getIndices( );
            const double* rowvals = rowvec.getValues( );
            DSVector cons(rowvec.getLength( ));
            for( int k = 0; k < rowvec.getLength( ); ++k )
            {
               assert(!model->getColFlags( )[ rowinds[ k ] ].test(ColFlag::kFixed));
               assert(rowvals[ k ] != 0.0);
               cons.add(inds[ rowinds[ k ] ], rowvals[ k ]);
            }
            soplex->addRowReal(LPRow(
                  rflags[ row ].test(RowFlag::kLhsInf) ? -soplex->realParam(SoPlex::INFTY) : lhs_values[ row ],
                  cons,
                  rflags[ row ].test(RowFlag::kRhsInf) ? soplex->realParam(SoPlex::INFTY) : rhs_values[ row ]));
            rowNames.add(consNames[row].c_str());
         }

         if( solution_exists && ( parameters.set_dual_limit || parameters.set_prim_limit ) )
         {
            for( const auto& pair : limits )
            {
               switch( pair.second )
               {
               case DUAL:
                  soplex->setRealParam(SoPlex::RealParam(pair.first.back()), relax( value, obj.sense, 2.0 * std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) ));
                  break;
               case PRIM:
                  soplex->setRealParam(SoPlex::RealParam(pair.first.back()), value);
                  break;
               }
            }
         }
      }

      void
      set_parameters( ) const
      {
         for( const auto& pair : adjustment->getBoolSettings( ) )
            soplex->setBoolParam(SoPlex::BoolParam(pair.first.back()), pair.second);
         for( const auto& pair : adjustment->getIntSettings( ) )
            soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second);
         for( const auto& pair : adjustment->getDoubleSettings( ) )
            soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second);
         for( const auto& pair : adjustment->getLimitSettings( ) )
         {
            switch( limits.find(pair.first)->second )
            {
            case ITER:
               soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second);
               break;
            case TIME:
               soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second);
               break;
            case DUAL:
            case PRIM:
            default:
               SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
            }
         }
      }
   };

   class SoplexFactory : public SolverFactory
   {

   private:

      SoplexParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
         parameterset.addParameter("soplex.mode", "solve soplex mode (-1: optimize)", parameters.mode, -1, -1);
         parameterset.addParameter("soplex.limitspace", "relative margin when restricting limits or -1 for no restriction", parameters.limitspace, -1.0);
         parameterset.addParameter("soplex.setduallimit", "terminate when dual solution is better than reference solution (affecting)", parameters.set_dual_limit);
         parameterset.addParameter("soplex.setprimlimit", "terminate when prim solution is as good as reference solution (affecting)", parameters.set_prim_limit);
         parameterset.addParameter("soplex.setiterlimit", "restrict number of iterations automatically (effortbounding)", parameters.set_iter_limit);
         parameterset.addParameter("soplex.settimelimit", "restrict time automatically (unreproducible)", parameters.set_time_limit);
         //TODO: Restrict monotonous limits by default
      }

      std::unique_ptr<SolverInterface>
      create_solver(const Message& msg) override
      {
         auto soplex = std::unique_ptr<SolverInterface>( new SoplexInterface( msg, parameters, limits ) );
         if( initial )
         {
            // objective limits will be included in parseSettings() where sense is revealed
            if( parameters.limitspace < 0.0 )
            {
               parameters.set_iter_limit = false;
               parameters.set_time_limit = false;
            }
            else
            {
               if( parameters.set_iter_limit )
               {
                  String name { 1, (char)SoPlex::ITERLIMIT };
                  if( soplex->has_setting(name) )
                     limits[name] = SoplexInterface::ITER;
                  else
                  {
                     msg.info("Iteration limit disabled.\n");
                     parameters.set_iter_limit = false;
                  }
               }
               if( parameters.set_time_limit )
               {
                  String name { 2, (char)SoPlex::TIMELIMIT };
                  if( soplex->has_setting(name) )
                     limits[name] = SoplexInterface::TIME;
                  else
                  {
                     msg.info("Time limit disabled.\n");
                     parameters.set_time_limit = false;
                  }
               }
            }
            initial = false;
         }
         return soplex;
      }
   };

   std::shared_ptr<SolverFactory>
   load_solver_factory( )
   {
      return std::shared_ptr<SolverFactory>(new SoplexFactory( ));
   }

} // namespace bugger

#endif
