

/*
 Copyright (C) 2000, 2001, 2002 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it under the
 terms of the QuantLib license.  You should have received a copy of the
 license along with this program; if not, please email ferdinando@ametrano.net
 The license is also available online at http://quantlib.org/html/license.html

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/
/*! \file impliciteuler.hpp
    \brief implicit Euler scheme for finite difference methods

    \fullpath
    ql/FiniteDifferences/%impliciteuler.hpp
*/

// $Id$

#ifndef quantlib_backward_euler_h
#define quantlib_backward_euler_h

#include <ql/FiniteDifferences/finitedifferencemodel.hpp>

namespace QuantLib {

    namespace FiniteDifferences {

        //! Backward Euler scheme for finite difference methods
        /*! See sect. \ref findiff for details on the method.

            In this implementation, the passed operator must be derived
            from either TimeConstantOperator or TimeDependentOperator.
            Also, it must implement at least the following interface:

            \code
            typedef ... arrayType;

            // copy constructor/assignment
            // (these will be provided by the compiler if none is defined)
            Operator(const Operator&);
            Operator& operator=(const Operator&);

            // inspectors
            Size size();

            // modifiers
            void setTime(Time t);

            // operator interface
            arrayType solveFor(const arrayType&);
            static Operator identity(Size size);

            // operator algebra
            Operator operator*(double, const Operator&);
            Operator operator+(const Operator&, const Operator&);
            \endcode
        */
        template <class Operator>
        class ImplicitEuler {
            friend class FiniteDifferenceModel<ImplicitEuler<Operator> >;
          private:
            // typedefs
            typedef typename Operator::arrayType arrayType;
            typedef Operator operatorType;
            // constructors
            ImplicitEuler(const Operator& L)
            : L_(L), I_(Operator::identity(L.size())), dt_(0.0) {}
            void step(arrayType& a, Time t);
            void setStep(Time dt) {
                dt_ = dt;
                implicitPart_ = I_+dt_*L_;
            }
            Operator L_;
            Operator I_;
            Operator implicitPart_;
            Time dt_;
        };

        // inline definitions

        template <class Operator>
        inline void ImplicitEuler<Operator>::step(arrayType& a, Time t) {
            if (L_.isTimeDependent()) {
                L_.setTime(t-dt_);
                implicitPart_ = I_+dt_*L_;
            }
            a = implicitPart_.solveFor(a);
        }

    }

}


#endif
