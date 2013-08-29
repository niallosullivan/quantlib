/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2013 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/


#include <ql/experimental/models/gaussian1dmodel.hpp>

namespace QuantLib {

    const Real Gaussian1dModel::forwardRate(const Date& fixing, const Date& referenceDate, const Real y,
                                  boost::shared_ptr<IborIndex> iborIdx) const {

        QL_REQUIRE(iborIdx != NULL,"no ibor index given");

        Handle<YieldTermStructure> yts = iborIdx->forwardingTermStructure(); // might be empty, then use model curve

        Date valueDate = iborIdx->valueDate(fixing);
        Date endDate = iborIdx->fixingCalendar().advance(valueDate,iborIdx->tenor(),
                                iborIdx->businessDayConvention(),iborIdx->endOfMonth()); 
        // FIXME Here we should use the calculation date calendar ?
        Real dcf = iborIdx->dayCounter().yearFraction(valueDate,endDate);

        return ( zerobond(valueDate,referenceDate,y,yts) - zerobond(endDate,referenceDate,y,yts) ) / 
            (dcf * zerobond(endDate,referenceDate,y,yts));

    }

    const Real Gaussian1dModel::swapRate(const Date& fixing, const Period& tenor, const Date& referenceDate, 
                                        const Real y, boost::shared_ptr<SwapIndex> swapIdx) const {

        QL_REQUIRE(swapIdx != NULL,"no swap index given");

        Handle<YieldTermStructure> ytsf = swapIdx->iborIndex()->forwardingTermStructure();
        Handle<YieldTermStructure> ytsd = swapIdx->discountingTermStructure(); // either might be empty, then use model curve

        SwapIndex tmpIdx = SwapIndex(swapIdx->familyName(), tenor, swapIdx->fixingDays(),
                                 swapIdx->currency(), swapIdx->fixingCalendar(), swapIdx->fixedLegTenor(),
                                 swapIdx->fixedLegConvention(), swapIdx->dayCounter(), swapIdx->iborIndex());
        boost::shared_ptr<VanillaSwap> underlying = tmpIdx.underlyingSwap(fixing);
        Schedule sched = underlying->fixedSchedule();
        Real annuity = swapAnnuity(fixing,tenor,referenceDate,y,swapIdx);
        Rate floatleg;
        if(ytsf.empty() && ytsd.empty()) { // simple 100-formula can be used only in one curve setup
            floatleg = ( zerobond(sched.dates().front(),referenceDate,y) - 
                         zerobond(sched.calendar().adjust(sched.dates().back(),
                                                          underlying->paymentConvention()),referenceDate,y));
        }
        else {
			Schedule floatSched = underlying->floatingSchedule();
			Real floatLeg=0.0;
			for(Size i=1; i<floatSched.size(); i++) {
				floatLeg += ( zerobond( floatSched[i-1], referenceDate, y, ytsf ) / 
                              zerobond ( floatSched[i], referenceDate, y, ytsf ) - 1.0 ) *
                    zerobond( floatSched.calendar().adjust(floatSched[i], underlying->paymentConvention()), 
                              referenceDate, y , ytsd );
			}
        }
        return floatleg / annuity;

    }

   	const Real Gaussian1dModel::swapAnnuity(const Date& fixing, const Period& tenor, const Date& referenceDate, 
                                           const Real y, boost::shared_ptr<SwapIndex> swapIdx) const {
        
        QL_REQUIRE(swapIdx != NULL,"no swap index given");

        Handle<YieldTermStructure> ytsd = swapIdx->discountingTermStructure(); // might be empty, then use model curve

		SwapIndex tmpIdx = SwapIndex(swapIdx->familyName(), tenor, swapIdx->fixingDays(),
                                 swapIdx->currency(), swapIdx->fixingCalendar(), swapIdx->fixedLegTenor(),
                                 swapIdx->fixedLegConvention(), swapIdx->dayCounter(), swapIdx->iborIndex());
		boost::shared_ptr<VanillaSwap> underlying = tmpIdx.underlyingSwap(fixing);
		Schedule sched = underlying->fixedSchedule();

		Real annuity=0.0;
		for(unsigned int j=1; j<sched.size(); j++) {
			annuity += zerobond(sched.calendar().adjust(sched.date(j),underlying->paymentConvention()),
                                referenceDate,y,ytsd) * 
				swapIdx->dayCounter().yearFraction( sched.date(j-1) , sched.date(j) );
		}
		return annuity;

	}

    const Real Gaussian1dModel::zerobondOption(const Option::Type& type, const Date& expiry, const Date& valueDate, 
                                              const Date& maturity, const Rate strike, const Date& referenceDate, 
                                              const Real y, const Handle<YieldTermStructure>& yts,
                                              const Real yStdDevs,
                                              const Size yGridPoints,
                                              const bool extrapolatePayoff,
                                              const bool flatPayoffExtrapolation) const {

        Time fixingTime = termStructure()->timeFromReference(expiry);
        Time referenceTime = referenceDate == Null<Date>() ? 0.0 : termStructure()->timeFromReference(referenceDate);

        Array yg = yGrid(yStdDevs,yGridPoints,fixingTime,referenceTime,y);
        Array z = yGrid(yStdDevs,yGridPoints);

        Array p(yg.size());

        for(Size i=0;i<yg.size();i++) {
            Real expValDsc = zerobond(valueDate,expiry,yg[i],yts);
            Real discount = zerobond(maturity,expiry,yg[i],yts) / expValDsc;
            p[i] = std::max((type == Option::Call ? 1.0 : -1.0) * (discount-strike), 0.0 ) / 
                numeraire(fixingTime,yg[i],yts) * expValDsc;
        }

        CubicInterpolation payoff(z.begin(),z.end(),p.begin(),CubicInterpolation::Spline,true,CubicInterpolation::Lagrange,
                                  0.0,CubicInterpolation::Lagrange,0.0);

        Real price = 0.0;
        for(Size i=0;i<z.size()-1;i++) {
            price += gaussianShiftedPolynomialIntegral( 0.0, payoff.cCoefficients()[i], payoff.bCoefficients()[i], 
                                                        payoff.aCoefficients()[i], p[i], z[i], z[i], z[i+1] );
        }
        if(extrapolatePayoff) {
            if(flatPayoffExtrapolation) {
                price += gaussianShiftedPolynomialIntegral( 0.0, 0.0, 0.0, 0.0, p[z.size()-2], z[z.size()-2], 
                                                            z[z.size()-1], 100.0 );
                price += gaussianShiftedPolynomialIntegral( 0.0, 0.0, 0.0, 0.0, p[0], z[0], -100.0 , z[0] );
            }
            else {
                if(type == Option::Call) price += gaussianShiftedPolynomialIntegral( 0.0, payoff.cCoefficients()[z.size()-2], 
                            payoff.bCoefficients()[z.size()-2], payoff.aCoefficients()[z.size()-2], 
                            p[z.size()-2], z[z.size()-2], z[z.size()-1], 100.0 );
                if(type == Option::Put) price += gaussianShiftedPolynomialIntegral( 0.0, payoff.cCoefficients()[0], 
                            payoff.bCoefficients()[0], payoff.aCoefficients()[0], p[0], z[0], -100.0 , z[0] );
            }
        }

        return numeraire(referenceTime,y,yts) * price;

    }

    const Real Gaussian1dModel::gaussianPolynomialIntegral(const Real a, const Real b, const Real c, const Real d, 
                                                                 const Real e, const Real y0, const Real y1,
                                                                 const bool useNtl) {
#ifdef MF_ENABLE_NTL
        if(useNtl) {
        const boost::math::ntl::RR aa=4.0*a, ba=2.0*M_SQRT2*b, ca=2.0*c, da=M_SQRT2*d;
        const boost::math::ntl::RR x0=y0*M_SQRT1_2, x1=y1*M_SQRT1_2;
        const boost::math::ntl::RR res = (0.125*(3.0*aa+2.0*ca+4.0*e)*boost::math::erf(x1)-1.0/(4.0*M_SQRTPI)*
            exp(-x1*x1)*(2.0*aa*x1*x1*x1+3.0*aa*x1+2.0*ba*(x1*x1+1.0)+2.0*ca*x1+2.0*da))-
            (0.125*(3.0*aa+2.0*ca+4.0*e)*boost::math::erf(x0)-1.0/(4.0*M_SQRTPI)*exp(-x0*x0)*
            (2.0*aa*x0*x0*x0+3.0*aa*x0+2.0*ba*(x0*x0+1.0)+2.0*ca*x0+2.0*da));
        return NTL::to_double(res.value());
    }
#endif
        const Real aa=4.0*a, ba=2.0*M_SQRT2*b, ca=2.0*c, da=M_SQRT2*d;
        const Real x0=y0*M_SQRT1_2, x1=y1*M_SQRT1_2;
        return (0.125*(3.0*aa+2.0*ca+4.0*e)*boost::math::erf(x1)-1.0/(4.0*M_SQRTPI)*exp(-x1*x1)*
                (2.0*aa*x1*x1*x1+3.0*aa*x1+2.0*ba*(x1*x1+1.0)+2.0*ca*x1+2.0*da))-
            (0.125*(3.0*aa+2.0*ca+4.0*e)*boost::math::erf(x0)-1.0/(4.0*M_SQRTPI)*exp(-x0*x0)*
             (2.0*aa*x0*x0*x0+3.0*aa*x0+2.0*ba*(x0*x0+1.0)+2.0*ca*x0+2.0*da));
    }

    const Real Gaussian1dModel::gaussianShiftedPolynomialIntegral(const Real a, const Real b, const Real c, 
            const Real d, const Real e, const Real h, const Real x0, const Real x1, const bool useNtl) {
        return gaussianPolynomialIntegral(a,-4.0*a*h+b,6.0*a*h*h-3.0*b*h+c,-4*a*h*h*h+3.0*b*h*h-2.0*c*h+d,
            a*h*h*h*h-b*h*h*h+c*h*h-d*h+e,x0,x1,useNtl);
    }

    const Disposable<Array> Gaussian1dModel::yGrid(const Real stdDevs, const int gridPoints, const Real T, 
                                                    const Real t, const Real y) const {

        QL_REQUIRE(stateProcess_ != NULL,"state process not set");

        Array result(2*gridPoints+1,0.0);

        Real stdDev_0_t = stateProcess_->stdDeviation(0.0,0.0,t);
        // we use that the standard deviation is independent of $x$ here !
        Real stdDev_0_T = stateProcess_->stdDeviation(0.0,0.0,T);
        Real stdDev_t_T = stateProcess_->stdDeviation(t,0.0,T-t);

        Real h = stdDevs / ((Real)gridPoints);

        for(int j=-gridPoints;j<=gridPoints;j++) {
            result[j+gridPoints] = (y*stdDev_0_t + stdDev_t_T*((Real)j)*h) / stdDev_0_T;
        }

        return result;

    }





}