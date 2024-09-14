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

#ifndef __BUGGER_INTERFACES_SCIPINTERFACE_HPP__
#define __BUGGER_INTERFACES_SCIPINTERFACE_HPP__

#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


namespace bugger
{
   enum ScipLimit : char
   {
      DUAL = 1,
      PRIM = 2,
      BEST = 3,
      SOLU = 4,
      REST = 5,
      TOTA = 6,
      TIME = 7,
   };

   class ScipParameters
   {
   public:

      static const String VERB;
      static const String EXAC;
      static const String CERT;

      int arithmetic = 0;
      int mode = -1;
      int certificate = 0;
      double cutoffrelax = -1.0;
      double limitspace = 1.0;
      bool set_dual_limit = true;
      bool set_prim_limit = true;
      bool set_best_limit = true;
      bool set_solu_limit = true;
      bool set_rest_limit = true;
      bool set_tota_limit = true;
      bool set_time_limit = false;
   };

   const String ScipParameters::VERB { "display/verblevel" };
   const String ScipParameters::EXAC { "exact/enabled" };
   const String ScipParameters::CERT { "certificate/filename" };

   template <typename REAL>
   class ScipInterface : public SolverInterface<REAL>
   {
   protected:

      const ScipParameters& parameters;
      const HashMap<String, char>& limits;
      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars { };

   public:

      ScipInterface(const Message& _msg, const ScipParameters& _parameters, const HashMap<String, char>& _limits) :
                    SolverInterface<REAL>(_msg), parameters(_parameters), limits(_limits)
      {
         if( SCIPcreate(&scip) != SCIP_OKAY || SCIPincludeDefaultPlugins(scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      void
      print_header( ) const override
      {
         SCIPprintVersion(scip, nullptr);

         auto names = SCIPgetExternalCodeNames(scip);
         auto description = SCIPgetExternalCodeDescriptions(scip);
         int length = SCIPgetNExternalCodes(scip);

         for( int i = 0; i < length; ++i )
            this->msg.info("\t{:20} {}\n", names[i], description[i]);
      }

      bool
      has_setting(const String& name) const override
      {
         return SCIPgetParam(scip, name.c_str()) != nullptr;
      }

      boost::optional<SolverSettings>
      parseSettings(const String& filename) const override
      {
         bool success = filename.empty() || SCIPreadParams(scip, filename.c_str()) == SCIP_OKAY;

         set_arithmetic( );

         if( !success )
            return boost::none;

         Vec<std::pair<String, bool>> bool_settings;
         Vec<std::pair<String, int>> int_settings;
         Vec<std::pair<String, long>> long_settings;
         Vec<std::pair<String, double>> double_settings;
         Vec<std::pair<String, char>> char_settings;
         Vec<std::pair<String, String>> string_settings;
         Vec<std::pair<String, long long>> limit_settings;
         SCIP_PARAM** params = SCIPgetParams(scip);
         int nparams = SCIPgetNParams(scip);

         for( int i = 0; i < nparams; ++i )
         {
            SCIP_PARAM* param = params[i];
            String name { param->name };
            if( name == ScipParameters::VERB
             || name == ScipParameters::EXAC
             || name == ScipParameters::CERT )
               continue;
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
                  break;
               case BEST:
               case SOLU:
               case REST:
                  limit_settings.emplace_back( name, param->data.intparam.valueptr == nullptr
                                                     ? param->data.intparam.curvalue
                                                     : *param->data.intparam.valueptr );
                  break;
               case TOTA:
                  limit_settings.emplace_back( name, param->data.longintparam.valueptr == nullptr
                                                     ? param->data.longintparam.curvalue
                                                     : *param->data.longintparam.valueptr );
                  break;
               case TIME:
                  limit_settings.emplace_back( name, min(ceil(param->data.realparam.valueptr == nullptr
                                                              ? param->data.realparam.curvalue
                                                              : *param->data.realparam.valueptr), SCIP_Real(LLONG_MAX)) );
                  break;
               default:
                  SCIPerrorMessage("unknown limit type\n");
                  assert(false);
               }
            }
            else
            {
               switch( param->paramtype )
               {
               case SCIP_PARAMTYPE_BOOL:
                  bool_settings.emplace_back( name, param->data.boolparam.valueptr == nullptr
                                                    ? param->data.boolparam.curvalue
                                                    : *param->data.boolparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_INT:
                  int_settings.emplace_back( name, param->data.intparam.valueptr == nullptr
                                                   ? param->data.intparam.curvalue
                                                   : *param->data.intparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_LONGINT:
                  long_settings.emplace_back( name, param->data.longintparam.valueptr == nullptr
                                                    ? param->data.longintparam.curvalue
                                                    : *param->data.longintparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_REAL:
                  double_settings.emplace_back( name, param->data.realparam.valueptr == nullptr
                                                      ? param->data.realparam.curvalue
                                                      : *param->data.realparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_CHAR:
                  char_settings.emplace_back( name, param->data.charparam.valueptr == nullptr
                                                    ? param->data.charparam.curvalue
                                                    : *param->data.charparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_STRING:
                  string_settings.emplace_back( name, param->data.stringparam.valueptr == nullptr
                                                      ? param->data.stringparam.curvalue
                                                      : *param->data.stringparam.valueptr );
                  break;
               default:
                  SCIPerrorMessage("unknown setting type\n");
                  assert(false);
               }
            }
         }

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings, limit_settings);
      }

      long long
      getSolvingEffort( ) const override
      {
         switch( SCIPgetStage(scip) )
         {
         case SCIP_STAGE_PRESOLVING:
         case SCIP_STAGE_PRESOLVED:
         case SCIP_STAGE_SOLVING:
         case SCIP_STAGE_SOLVED:
            return SCIPgetNLPIterations(scip);
         case SCIP_STAGE_INIT:
         case SCIP_STAGE_PROBLEM:
         case SCIP_STAGE_TRANSFORMING:
         case SCIP_STAGE_TRANSFORMED:
         case SCIP_STAGE_INITPRESOLVE:
         case SCIP_STAGE_EXITPRESOLVE:
         case SCIP_STAGE_INITSOLVE:
         case SCIP_STAGE_EXITSOLVE:
         case SCIP_STAGE_FREETRANS:
         case SCIP_STAGE_FREE:
         default:
            return -1;
         }
      }

      virtual
      ~ScipInterface( ) override
      {
         if( scip != nullptr )
         {
            switch( SCIPgetStage(scip) )
            {
            case SCIP_STAGE_INITSOLVE:
            case SCIP_STAGE_EXITSOLVE:
            case SCIP_STAGE_FREETRANS:
            case SCIP_STAGE_FREE:
               // avoid errors in stages above
               scip = nullptr;
            case SCIP_STAGE_INIT:
            case SCIP_STAGE_PROBLEM:
            case SCIP_STAGE_TRANSFORMING:
            case SCIP_STAGE_TRANSFORMED:
            case SCIP_STAGE_INITPRESOLVE:
            case SCIP_STAGE_PRESOLVING:
            case SCIP_STAGE_PRESOLVED:
            case SCIP_STAGE_EXITPRESOLVE:
            case SCIP_STAGE_SOLVING:
            case SCIP_STAGE_SOLVED:
            default:
               if( scip == nullptr || SCIPfree(&scip) != SCIP_OKAY )
                  this->msg.warn("could not free SCIP\n");
               break;
            }
         }
      }

   protected:

      void
      set_parameters( ) const
      {
         for( const auto& pair : this->adjustment->getBoolSettings( ) )
            SCIP_CALL_ABORT(SCIPsetBoolParam(scip, pair.first.c_str(), pair.second));
         for( const auto& pair : this->adjustment->getIntSettings( ) )
            SCIP_CALL_ABORT(SCIPsetIntParam(scip, pair.first.c_str(), pair.second));
         for( const auto& pair : this->adjustment->getLongSettings( ) )
            SCIP_CALL_ABORT(SCIPsetLongintParam(scip, pair.first.c_str(), pair.second));
         for( const auto& pair : this->adjustment->getDoubleSettings( ) )
            SCIP_CALL_ABORT(SCIPsetRealParam(scip, pair.first.c_str(), pair.second));
         for( const auto& pair : this->adjustment->getCharSettings( ) )
            SCIP_CALL_ABORT(SCIPsetCharParam(scip, pair.first.c_str(), pair.second));
         for( const auto& pair : this->adjustment->getStringSettings( ) )
            SCIP_CALL_ABORT(SCIPsetStringParam(scip, pair.first.c_str(), pair.second.c_str()));

         for( const auto& pair : this->adjustment->getLimitSettings( ) )
         {
            switch( limits.find(pair.first)->second )
            {
            case BEST:
            case SOLU:
            case REST:
               SCIP_CALL_ABORT(SCIPsetIntParam(scip, pair.first.c_str(), pair.second));
               break;
            case TOTA:
               SCIP_CALL_ABORT(SCIPsetLongintParam(scip, pair.first.c_str(), pair.second));
               break;
            case TIME:
               SCIP_CALL_ABORT(SCIPsetRealParam(scip, pair.first.c_str(), pair.second));
               break;
            case DUAL:
            case PRIM:
            default:
               SCIPerrorMessage("unknown limit type\n");
               assert(false);
            }
         }

         set_arithmetic( );
      }

   private:

      void
      set_arithmetic( ) const
      {
#ifdef SCIP_WITH_EXACTSOLVE
         switch( parameters.arithmetic )
         {
         case 0:
            SCIP_CALL_ABORT(SCIPsetBoolParam(scip, ScipParameters::EXAC.c_str(), FALSE));
            break;
         case 1:
            SCIP_CALL_ABORT(SCIPsetBoolParam(scip, ScipParameters::EXAC.c_str(), TRUE));
            break;
         default:
            SCIPerrorMessage("unknown solver arithmetic\n");
            assert(false);
         }

         SCIP_CALL_ABORT(SCIPsetStringParam(scip, ScipParameters::CERT.c_str(), parameters.certificate ? "certificate.vipr" : ""));
#endif
      }
   };

} // namespace bugger

#include "ScipRealInterface.hpp"
#ifdef SCIP_WITH_EXACTSOLVE
#include "ScipRationalInterface.hpp"
#endif


namespace bugger
{
   template <typename REAL>
   class ScipFactory : public SolverFactory<REAL>
   {
   private:

      ScipParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
#ifndef SCIP_WITH_EXACTSOLVE
         parameterset.addParameter("scip.arithmetic", "arithmetic scip type (0: double)", parameters.arithmetic, 0, 0);
#else
         parameterset.addParameter("scip.arithmetic", "arithmetic scip type (0: double, 1: rational)", parameters.arithmetic, 0, 1);
#endif
         parameterset.addParameter("scip.mode", "solve scip mode (-1: optimize, 0: count)", parameters.mode, -1, 0);
#ifndef SCIP_WITH_EXACTSOLVE
         parameterset.addParameter("scip.certificate", "check vipr certificate", parameters.certificate, 0, 0);
#else
         parameterset.addParameter("scip.certificate", "check vipr certificate", parameters.certificate, 0, 1);
#endif
         parameterset.addParameter("scip.cutoffrelax", "absolute margin for limiting objective or -1 for no limitation", parameters.cutoffrelax, -1.0);
         parameterset.addParameter("scip.limitspace", "relative margin when restricting limits or -1 for no restriction", parameters.limitspace, -1.0);
         parameterset.addParameter("scip.setduallimit", "terminate when dual bound is better than reference solution", parameters.set_dual_limit);
         parameterset.addParameter("scip.setprimlimit", "terminate when prim bound is as good as reference solution", parameters.set_prim_limit);
         parameterset.addParameter("scip.setbestlimit", "restrict best number of solutions automatically", parameters.set_best_limit);
         parameterset.addParameter("scip.setsolulimit", "restrict total number of solutions automatically", parameters.set_solu_limit);
         parameterset.addParameter("scip.setrestlimit", "restrict number of restarts automatically", parameters.set_rest_limit);
         parameterset.addParameter("scip.settotalimit", "restrict total number of nodes automatically", parameters.set_tota_limit);
         parameterset.addParameter("scip.settimelimit", "restrict time automatically (unreproducible)", parameters.set_time_limit);
         // run and stalling number of nodes, memory, and gap are unrestrictable because they are not monotonously increasing
      }

      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) override
      {
         std::unique_ptr<SolverInterface<REAL>> scip;
         switch( parameters.arithmetic )
         {
         case 0:
            scip = std::unique_ptr<SolverInterface<REAL>>( new ScipRealInterface<REAL>( msg, parameters, limits ) );
            break;
#ifdef SCIP_WITH_EXACTSOLVE
         case 1:
            scip = std::unique_ptr<SolverInterface<REAL>>( new ScipRationalInterface<REAL>( msg, parameters, limits ) );
            break;
#endif
         default:
            throw std::runtime_error("unknown solver arithmetic");
         }
         if( initial )
         {
            String name;
            if( parameters.mode != -1 )
            {
               parameters.set_dual_limit = false;
               parameters.set_prim_limit = false;
               parameters.set_best_limit = false;
               parameters.set_solu_limit = false;
               parameters.set_rest_limit = false;
            }
            else
            {
               if( parameters.set_dual_limit )
               {
                  if( scip->has_setting(name = "limits/dual") || scip->has_setting(name = "limits/proofstop") )
                     limits[name] = DUAL;
                  else
                  {
                     msg.info("Dual limit disabled.\n");
                     parameters.set_dual_limit = false;
                  }
               }
               if( parameters.set_prim_limit )
               {
                  if( scip->has_setting(name = "limits/primal") || scip->has_setting(name = "limits/objectivestop") )
                     limits[name] = PRIM;
                  else
                  {
                     msg.info("Primal limit disabled.\n");
                     parameters.set_prim_limit = false;
                  }
               }
               if( parameters.limitspace < 0.0 )
               {
                  parameters.set_best_limit = false;
                  parameters.set_solu_limit = false;
                  parameters.set_rest_limit = false;
               }
               else
               {
                  if( parameters.set_best_limit )
                  {
                     if( scip->has_setting(name = "limits/bestsol") )
                        limits[name] = BEST;
                     else
                     {
                        msg.info("Bestsolution limit disabled.\n");
                        parameters.set_best_limit = false;
                     }
                  }
                  if( parameters.set_solu_limit )
                  {
                     if( scip->has_setting(name = "limits/solutions") )
                        limits[name] = SOLU;
                     else
                     {
                        msg.info("Solution limit disabled.\n");
                        parameters.set_solu_limit = false;
                     }
                  }
                  if( parameters.set_rest_limit )
                  {
                     if( scip->has_setting(name = "limits/restarts") )
                        limits[name] = REST;
                     else
                     {
                        msg.info("Restart limit disabled.\n");
                        parameters.set_rest_limit = false;
                     }
                  }
               }
            }
            if( parameters.limitspace < 0.0 )
            {
               parameters.set_tota_limit = false;
               parameters.set_time_limit = false;
            }
            else
            {
               if( parameters.set_tota_limit )
               {
                  if( scip->has_setting(name = "limits/totalnodes") )
                     limits[name] = TOTA;
                  else
                  {
                     msg.info("Totalnode limit disabled.\n");
                     parameters.set_tota_limit = false;
                  }
               }
               if( parameters.set_time_limit )
               {
                  if( scip->has_setting(name = "limits/time") )
                     limits[name] = TIME;
                  else
                  {
                     msg.info("Time limit disabled.\n");
                     parameters.set_time_limit = false;
                  }
               }
            }
            initial = false;
         }
         return scip;
      }
   };

   template <typename REAL>
   std::shared_ptr<SolverFactory<REAL>>
   load_solver_factory( )
   {
      return std::shared_ptr<SolverFactory<REAL>>( new ScipFactory<REAL>( ) );
   }

} // namespace bugger

#endif
