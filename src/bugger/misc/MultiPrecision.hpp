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

#ifndef _BUGGER_MISC_MULTIPRECISION_HPP_
#define _BUGGER_MISC_MULTIPRECISION_HPP_

#include "bugger/Config.hpp"

#include <boost/serialization/split_free.hpp>

#ifdef BUGGER_HAVE_FLOAT128
#include <boost/multiprecision/float128.hpp>
using Quad = boost::multiprecision::float128;
#elif defined( BUGGER_HAVE_GMP )
#include <boost/multiprecision/gmp.hpp>
using Quad = boost::multiprecision::number<boost::multiprecision::gmp_float<35>>;
BOOST_SERIALIZATION_SPLIT_FREE( papilo::Quad )
#else
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/serialization/nvp.hpp>
using Quad = boost::multiprecision::cpp_bin_float_quad;
#endif

#ifdef BUGGER_HAVE_GMP
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/gmp.hpp>
#include <boost/serialization/nvp.hpp>
// unfortunately the multiprecision gmp types do not provide an overload for
// serialization
using Integral = boost::multiprecision::mpq_int;
using Rational = boost::multiprecision::mpq_rational;
using Float100 = boost::multiprecision::mpf_float_100;
using Float500 = boost::multiprecision::mpf_float_500;
using Float1000 = boost::multiprecision::mpf_float_1000;
BOOST_SERIALIZATION_SPLIT_FREE( papilo::Rational )
BOOST_SERIALIZATION_SPLIT_FREE( papilo::Float100 )
BOOST_SERIALIZATION_SPLIT_FREE( papilo::Float500 )
BOOST_SERIALIZATION_SPLIT_FREE( papilo::Float1000 )
namespace boost
{
namespace serialization
{

template <class Archive>
void
save( Archive& ar, const papilo::Rational& num, const unsigned int version )
{
   boost::multiprecision::cpp_rational t( num );
   ar& t;
}

template <class Archive>
void
load( Archive& ar, papilo::Rational& num, const unsigned int version )
{
   boost::multiprecision::cpp_rational t;
   ar& t;
   num = papilo::Rational( t );
}

template <class Archive, unsigned M>
void
save( Archive& ar,
      const boost::multiprecision::number<boost::multiprecision::gmp_float<M>>&
          num,
      const unsigned int version )
{
   boost::multiprecision::number<boost::multiprecision::cpp_bin_float<M>> t(
       num );
   ar& t;
}

template <class Archive, unsigned M>
void
load( Archive& ar,
      boost::multiprecision::number<boost::multiprecision::gmp_float<M>>& num,
      const unsigned int version )
{
   boost::multiprecision::number<boost::multiprecision::cpp_bin_float<M>> t;
   ar& t;
   num =
       boost::multiprecision::number<boost::multiprecision::gmp_float<M>>( t );
}

} // namespace serialization
} // namespace boost
#else
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/serialization/nvp.hpp>
using Integral = boost::multiprecision::cpp_int;
using Rational = boost::multiprecision::cpp_rational;
using Float100 = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<100>>;
using Float500 = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<500>>;
using Float1000 = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<1000>>;
#endif

namespace bugger
{
   Rational round(const Rational& number)
   {
      Integral integer { number };
      Rational fraction { 2 * (number - integer) };

      if( fraction >= 1 )
         return integer + 1;
      else if( fraction <= -1 )
         return integer - 1;
      else
         return integer;
   }

   Rational floor(const Rational& number)
   {
      Integral integer { number };

      if( integer > number )
         return integer - 1;
      else
         return integer;
   }

   Rational ceil(const Rational& number)
   {
      Integral integer { number };

      if( integer < number )
         return integer + 1;
      else
         return integer;
   }

   bool isinf(const Rational& number)
   {
      return false;
   }
}

#endif
