

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
/*! \file scheduler.cpp
    \brief date scheduler

    \fullpath
    ql/%scheduler.cpp
*/

// $Id$

#include <ql/scheduler.hpp>
#include <ql/dataformatters.hpp>

namespace QuantLib {

    Scheduler::Scheduler(const Calendar& calendar,
      const Date& startDate, const Date& endDate, int frequency,
      RollingConvention rollingConvention, bool isAdjusted,
      const Date& stubDate)
    : calendar_(calendar), startDate_(startDate), endDate_(endDate),
      frequency_(frequency), rollingConvention_(rollingConvention),
      isAdjusted_(isAdjusted), stubDate_(stubDate), lastIsRegular_(true) {
        // sanity checks
        QL_REQUIRE(startDate_ != Date(), "null start date");
        QL_REQUIRE(endDate_ != Date(),   "null end date");
        QL_REQUIRE(startDate_ < endDate_,
            "start date (" +
                DateFormatter::toString(startDate_) +
                ") later than end date (" +
                DateFormatter::toString(endDate_) +
                ")");
        if (stubDate_ != Date()) {
            QL_REQUIRE(stubDate_ > startDate_,
                "stub date (" +
                    DateFormatter::toString(stubDate_) +
                    ") later than start date (" +
                    DateFormatter::toString(startDate_) + ")");
            QL_REQUIRE(!calendar_.isHoliday(stubDate_) ||
                       !isEndOfMonth(stubDate_),
                "stub date (" +
                    DateFormatter::toString(stubDate_) +
                    ") is holiday and end of month for " +
                    calendar_.name() + " calendar");
        } else {
            QL_REQUIRE(!calendar_.isHoliday(startDate_) ||
                       !isEndOfMonth(startDate_),
                "start date (" +
                    DateFormatter::toString(startDate_) +
                    ") is holiday and end of month for " +
                    calendar_.name() + " calendar");
        }
        QL_REQUIRE(12%frequency_ == 0,
            "frequency (" +
            IntegerFormatter::toString(frequency_) +
            " per year) does not correspond to a whole number of months");

        // calculations
        Date seed = startDate_;
        Date last = (isAdjusted_ ?
                     calendar_.roll(endDate_,rollingConvention_) :
                     endDate_);
        // add start date
        dates_.push_back(startDate_);

        // add stub date if given
        if (stubDate_ != Date()) {
            seed = stubDate_;
            dates_.push_back(isAdjusted_ ?
                             calendar_.roll(stubDate_) :
                             stubDate_);
        }

        // add subsequent dates
        int periods = 1, months = 12/frequency_;
        while (true) {
            Date temp = seed.plus(periods*months,Months);
            if (isAdjusted_)
                temp = calendar_.roll(temp,rollingConvention_);
            dates_.push_back(temp);
            // check exit condition
            if (temp >= last)
                break;
            else
                periods++;
        }

        // possibly correct last inserted date
        if (dates_.back() > last) {
            dates_.back() = last;
            lastIsRegular_ = false;
        }

        // possibly collapse last two dates
        int N = dates_.size();
        if (calendar_.roll(dates_[N-2],rollingConvention_) ==
            calendar_.roll(dates_[N-1],rollingConvention_)) {
                dates_[N-2] = dates_[N-1];
                dates_.pop_back();
                lastIsRegular_ = true;
        }

        // done
    }

    bool Scheduler::isRegular(Size i) const {
        if (i == 1)
            return (stubDate_ == Date());
        else if (i == size()-1)
            return lastIsRegular_;
        else
            return true;
    }

    bool Scheduler::isEndOfMonth(const Date& d) const {
        return (d.month() != calendar_.roll(d+1).month());
    }

}
