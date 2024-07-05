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

#ifndef _BUGGER_MISC_PARAMETER_SET_HPP_
#define _BUGGER_MISC_PARAMETER_SET_HPP_

#include "bugger/misc/Alloc.hpp"
#include "bugger/misc/String.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/fmt.hpp"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/variant.hpp>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>

namespace bugger
{

class ParameterSet
{
 private:
   template <typename T>
   struct NumericalOption
   {
      T* storage;
      T min;
      T max;

      void
      set( T val )
      {
         if( val < min || val > max )
            throw std::out_of_range(
                "tried to set invalid value for numerical option" );

         *storage = val;
      }

      template <typename ValType>
      void
      set( ValType val )
      {
         throw std::domain_error(
             "tried to set invalid value for numerical option" );
      }

      void
      parse( const char* val )
      {
         T parsedval;
         try
         {
            parsedval = boost::lexical_cast<T>( val );
         }
         catch( ... )
         {
            throw std::invalid_argument( "could not parse given option" );
         }
         set( parsedval );
      }

      template <typename OutputIt>
      void
      print( OutputIt out, const String& key, const String& desc ) const
      {
         if( std::is_integral<T>::value )
            fmt::format_to( out, "# {}  [Integer: [{},{}]]\n{} = {}\n", desc,
                            min, max, key, *storage );
         else
            fmt::format_to( out, "# {}  [Numerical: [{},{}]]\n{} = {}\n", desc,
                            boost::lexical_cast<String>( min ),
                            boost::lexical_cast<String>( max ), key,
                            boost::lexical_cast<String>( *storage ) );
      }
   };

   struct CategoricalOption
   {
      char* storage;
      Vec<char> possibleOptions;

      void
      set( char val )
      {
         if( std::find( possibleOptions.begin(), possibleOptions.end(), val ) ==
             possibleOptions.end() )
            throw std::out_of_range(
                "tried to set invalid value for categorical option" );

         *storage = val;
      }

      template <typename ValType>
      void
      set( const ValType& )
      {
         throw std::domain_error(
             "tried to set invalid value for categorical option" );
      }

      void
      parse( const char* val )
      {
         if( val[0] == '\0' )
            throw std::invalid_argument( "could not parse given option" );

         set( val[0] );
      }

      template <typename OutputIt>
      void
      print( OutputIt out, const String& key, const String& desc ) const
      {
         assert( possibleOptions.size() > 0 );

         fmt::format_to( out, "# {}  [Categorical: {{", desc );
         std::size_t last = possibleOptions.size() - 1;

         for( std::size_t i = 0; i < last; ++i )
            fmt::format_to( out, "{}, ", possibleOptions[i] );
         fmt::format_to( out, "{}", possibleOptions[last] );

         fmt::format_to( out, "}}]\n{} = {}\n", key, *storage );
      }
   };

   struct StringOption
   {
      String* storage;

      void
      set( const char* val )
      {
         *storage = String( val );
      }

      void
      parse( const char* val )
      {
         set( val );
      }

      void
      set( String val )
      {
         *storage = val;
      }

      template <typename ValType>
      void
      set( const ValType& )
      {
         throw std::domain_error(
             "tried to set invalid value for string option" );
      }

      template <typename OutputIt>
      void
      print( OutputIt out, const String& key, const String& desc ) const
      {
         fmt::format_to( out, "# {}  [String]\n{} = {}\n", desc, key,
                         *storage );
      }
   };

   struct BoolOption
   {
      bool* storage;

      void
      set( bool val )
      {
         *storage = val;
      }

      template <typename ValType>
      void
      set( const ValType& )
      {
         throw std::domain_error(
             "tried to set invalid value for bool option" );
      }

      void
      parse( const char* val )
      {
         bool parsedval;
         try
         {
            parsedval = boost::lexical_cast<bool>( val );
         }
         catch( ... )
         {
            throw std::invalid_argument( "could not parse given option" );
         }
         set( parsedval );
      }

      template <typename OutputIt>
      void
      print( OutputIt out, const String& key, const String& desc ) const
      {
         fmt::format_to( out, "# {}  [Boolean: {{0,1}}]\n{} = {}\n", desc, key,
                         *storage ? '1' : '0' );
      }
   };

   struct IntVectorOption
   {
      Vec<int>* storage;

      void
      set( Vec<int> val )
      {
         *storage = val;
      }

      template <typename ValType>
      void
      set( const ValType& )
      {
         throw std::domain_error(
               "tried to set invalid value for integer vector option" );
      }

      void
      parse( const char* val )
      {
         Vec<int> parsedval;
         std::string string_representation(val);
         try
         {
            Vec<std::string> s = split(string_representation);
            for(const auto& item: s)
               parsedval.push_back(std::stoi(item));
         }
         catch( ... )
         {
            throw std::invalid_argument( "could not parse given option" );
         }
         set( parsedval );
      }

      std::vector<std::string> split(std::string const &input) {
         std::istringstream buffer(input);
         Vec<std::string> ret((std::istream_iterator<std::string>(buffer)),
                              std::istream_iterator<std::string>());
         return ret;
      }


      template <typename OutputIt>
      void
      print( OutputIt out, const String& key, const String& desc ) const
      {
         std::string str(storage->begin(), storage->end());
         fmt::format_to( out, "# {}  [String of integers separated by blanks]\n{} = {}\n", desc, key, str );
      }
   };

   struct Parameter
   {
      String description;
      boost::variant<StringOption,
                     BoolOption,
                     NumericalOption<int>,
                     NumericalOption<unsigned int>,
                     NumericalOption<long long>,
                     NumericalOption<double>,
                     IntVectorOption,
                     CategoricalOption>
          value;
   };

   template <typename ValType>
   struct SetParameterVisitor : public boost::static_visitor<>
   {
      ValType val;

      template <typename T>
      SetParameterVisitor( T&& _val ) : val( _val )
      {
      }

      template <typename OptionType>
      void
      operator()( OptionType& option ) const
      {
         option.set( val );
      }
   };

   struct ParseParameterVisitor : public boost::static_visitor<>
   {
      const char* val;

      ParseParameterVisitor( const char* val_ ) : val( val_ ) {}

      template <typename OptionType>
      void
      operator()( OptionType& option ) const
      {
         option.parse( val );
      }
   };

   template <typename OutputIt>
   struct PrintParameterVisitor : public boost::static_visitor<>
   {
      OutputIt out;
      const String& key;
      const String& desc;

      PrintParameterVisitor( OutputIt _out, const String& _key,
                             const String& _desc )
          : out( _out ), key( _key ), desc( _desc )
      {
      }

      template <typename OptionType>
      void
      operator()( OptionType& option ) const
      {
         option.print( out, key, desc );
      }
   };

   std::map<String, Parameter, std::less<String>,
            Allocator<std::pair<const String, Parameter>>>
       parameters;

 public:
   void
   addParameter( const char* key, const char* description, String& val )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description, StringOption{ &val } } );
   }

   void
   addParameter( const char* key, const char* description, bool& val )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description, BoolOption{ &val } } );
   }

   void
   addParameter( const char* key, const char* description, Vec<int>& val )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description, IntVectorOption{ &val } } );
   }

   void
   addParameter( const char* key, const char* description, int& val,
                 int min = std::numeric_limits<int>::min(),
                 int max = std::numeric_limits<int>::max() )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description, NumericalOption<int>{
                                                           &val, min, max } } );
   }

   void
   addParameter( const char* key, const char* description, unsigned int& val,
                 unsigned int min = std::numeric_limits<unsigned int>::min(),
                 unsigned int max = std::numeric_limits<unsigned int>::max() )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace(
          key, Parameter{ description,
                          NumericalOption<unsigned int>{ &val, min, max } } );
   }

   void
   addParameter( const char* key, const char* description, long long& val,
                 long long min = std::numeric_limits<long long>::min(),
                 long long max = std::numeric_limits<long long>::max() )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description,
                          NumericalOption<long long>{ &val, min, max } } );
   }

   void
   addParameter( const char* key, const char* description, double& val,
                 double min = std::numeric_limits<double>::min(),
                 double max = std::numeric_limits<double>::max() )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace( key, Parameter{ description, NumericalOption<double>{
                                                           &val, min, max } } );
   }

   void
   addParameter( const char* key, const char* description, char& val,
                 Vec<char> options )
   {
      if( parameters.count( key ) != 0 )
         throw std::invalid_argument(
               "tried to add parameter that already exists" );

      parameters.emplace(
          key, Parameter{ description,
                          CategoricalOption{ &val, std::move( options ) } } );
   }

   template <typename T>
   void
   setParameter( const char* key, T val )
   {
      if( parameters.count( key ) == 0 )
         throw std::invalid_argument(
               "tried to set parameter that does not exist" );

      SetParameterVisitor<T> visitor( val );
      boost::apply_visitor( visitor, parameters[key].value );
   }

   void
   parseParameter( const char* key, const char* val )
   {
      if( parameters.count( key ) == 0 )
         throw std::invalid_argument(
               "tried to set parameter that does not exist" );

      ParseParameterVisitor visitor( val );
      boost::apply_visitor( visitor, parameters[key].value );
   }

   template <typename OutputIt>
   void
   printParams( OutputIt out )
   {
      bool first = true;
      for( const auto& param : parameters )
      {
         if( first )
            first = false;
         else
            fmt::format_to( out, "\n" );

         PrintParameterVisitor<OutputIt> visitor( out, param.first,
                                                  param.second.description );
         boost::apply_visitor( visitor, param.second.value );
      }
   }
};

} // namespace bugger

#endif
