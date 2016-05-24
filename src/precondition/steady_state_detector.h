// StorScore
//
// Copyright (c) Microsoft Corporation
//
// All rights reserved. 
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#pragma once
#ifndef __STEADY_STATE_DETECTOR_H_
#define __STEADY_STATE_DETECTOR_H_

#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <iterator>

#include <boost/utility.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>

// STL expects iterators to behave sanely.  When operating on a range
// [_First, _Last), they quite reasonably expect that _First < _Last 
// will be true.  In order to build circular iterators that still meet
// expectations, we use the following approach from Stephan T Lavavej:
//
//  "...circular_iterator wraps index_, which can span the full
//  range of ptrdiff_t.  It then views a range [first, last)
//  (represented as [first_, first_ + size_)) as being infinitely
//  repeated over the full range of ptrdiff_t.  As a result, every
//  operation that has to be provided to iterator_facade trivially
//  manipulates index_, and only dereference() has to do some work."

template <typename RanIt>
class circular_iterator
    : public boost::iterator_facade<
        circular_iterator<RanIt>,
        typename std::iterator_traits<RanIt>::value_type,
        typename std::iterator_traits<RanIt>::iterator_category,
        typename std::iterator_traits<RanIt>::reference,
        typename std::iterator_traits<RanIt>::difference_type
    >
{
    public:

    typedef
        typename std::iterator_traits<RanIt>::reference
        reference;

    typedef 
        typename std::iterator_traits<RanIt>::difference_type
        difference_type;
 
    circular_iterator()
        : first_(), size_(0), index_(0) 
    {}

    circular_iterator(
        const RanIt first,
        const RanIt last,
        const difference_type index = 0 )
        : first_( first )
        , size_( distance( first, last ) )
        , index_( index )
    {}
    
    template <typename OtherRanIt>
    circular_iterator( circular_iterator<OtherRanIt> const& other )
        : first_( other.first_ )
        , size_( other.size_ )
        , index_( other.index_ )
    {}    

    private:

    friend class boost::iterator_core_access;    
    template <class> friend class circular_iterator;

    reference dereference() const
    {
        difference_type n = index_ % size_;

        if (n < 0) {
            n += size_;
        }

        return *next( first_, n );
    }

    bool equal( const circular_iterator& other ) const
    {
        return index_ == other.index_;
    }

    void increment()
    {
        ++index_;
    }

    void decrement()
    {
        --index_;
    }

    void advance( const difference_type n )
    {
        index_ += n;
    }

    difference_type distance_to( const circular_iterator& other ) const
    {
        return other.index_ - index_;
    }

    RanIt first_;
    difference_type size_;
    difference_type index_;
};

template< typename T >
class circular_buffer : boost::noncopyable
{
    public:

    typedef
        circular_iterator<
            typename std::vector<T>::iterator
        > iterator;
    
    typedef
        circular_iterator<
            typename std::vector<T>::const_iterator
        > const_iterator;

    private:

    std::vector<T> v_;
    iterator current_;
    
    public:
    
    circular_buffer( size_t n, const T& val )
        : v_( n, val )
        , current_( v_.begin(), v_.end() )
    {}

    T& current()
    {
        return *current_;
    }

    void advance()
    {
        ++current_;
    }
    
    iterator begin()
    {
        // skip current_, since it is in-progress
        return next( current_, 1 );
    }

    iterator end()
    {
        // moral equivalent to current_
        return next( current_, v_.size() );
    }

    const_iterator begin() const
    {
        // skip current_, since it is in-progress
        return next( current_, 1 );
    }

    const_iterator end() const
    {
        // moral equivalent to current_
        return next( current_, v_.size() );
    }
};

class SteadyStateDetector
{
    public:

    static const int DEFAULT_GATHER_SECONDS = 540; // 9 minutes
    static const int DEFAULT_DWELL_SECONDS = 60; // 1 minute
    static const double DEFAULT_SLOPE_TOLERANCE;

    private:

    int numValidBins_;
    int64_t nextBinStartTime_;
    
    bool possibleSteadyState_;
    int64_t dwellStart_;
   
    mutable bool changed_;

    const size_t GATHER_SECONDS;
    const size_t DWELL_SECONDS;
    const double SLOPE_TOLERANCE;

    const size_t NUM_BINS;
    const int64_t QPC_TICKS_PER_BIN;
  
    // N.B: this directly effects the frequency, and 
    // thus the CPU overhead, of the linear-regression.
    static const int BINS_PER_SECOND = 10;  // Each bin = 1/10th second

    circular_buffer<int64_t> data_;

    // Constants for linear regression
    const double SUM_X;
    const double SUM_SQ_X;
    const double VAR_X;
    const double STD_DEV_X;

    public:

    SteadyStateDetector( 
            size_t gather_sec = DEFAULT_GATHER_SECONDS,
            size_t dwell_sec = DEFAULT_DWELL_SECONDS,
            double slope_toler = DEFAULT_SLOPE_TOLERANCE )
        : numValidBins_( 0 )
        , possibleSteadyState_( false )
        , changed_( false )
        , GATHER_SECONDS( gather_sec )
        , DWELL_SECONDS( dwell_sec )
        , SLOPE_TOLERANCE( slope_toler )
        , NUM_BINS( GATHER_SECONDS * BINS_PER_SECOND )
        , QPC_TICKS_PER_BIN( QPC_TICKS_PER_SEC / BINS_PER_SECOND )
        , data_( NUM_BINS, 0 )
        , SUM_X(
                std::accumulate(
                    boost::counting_iterator<int64_t>( 1 ),
                    boost::counting_iterator<int64_t>( NUM_BINS ),
                    0.0 ) )
        , SUM_SQ_X(
                std::inner_product(
                    boost::counting_iterator<int64_t>( 1 ),
                    boost::counting_iterator<int64_t>( NUM_BINS ),
                    boost::counting_iterator<int64_t>( 1 ),
                    0.0 ) )
        , VAR_X( SUM_SQ_X - ( ( SUM_X * SUM_X ) / ( NUM_BINS - 1 ) ) )
        , STD_DEV_X( std::sqrt( VAR_X ) )
    {
        if( GATHER_SECONDS == 0 )
        {
            throw std::invalid_argument( "Non-zero window required" );
        }
    }

    void trackCompletion()
    {
        int64_t now = qpc(); 
    
        if( numValidBins_ == 0 )
        {
            // Initialize on 1st call
            nextBinStartTime_ = now + QPC_TICKS_PER_BIN;
            numValidBins_ = 1;
        }

        while( now >= nextBinStartTime_ )
        {
            data_.advance();
            changed_ = true;

            data_.current() = 0;
       
            numValidBins_ = 
                std::min<int64_t>( numValidBins_ + 1, NUM_BINS );

            nextBinStartTime_ += QPC_TICKS_PER_BIN;
        }
            
        data_.current()++;
        
        if( full() )
        {
            double slope;
            double rSquared;

            std::tie( slope, rSquared ) = getLinearFit();

            // ISSUE_REVIEW: should we also have an R^2 tolerance?
            // An R^2 close to 1.0 indicates a good fit, but ours
            // are often terrible... 0.01 or worse.
            if( abs( slope ) <= SLOPE_TOLERANCE )
            {
                if( possibleSteadyState_ == false )
                {
                    dwellStart_ = qpc();
                }

                possibleSteadyState_ = true;
            }
            else
            {
                possibleSteadyState_ = false;
            }
        }
    }
    
    std::string getProgressMessage() const
    {
        if( done() )
        {
            return "steady-state achieved";
        }
        else if( !full() )
        {
            double percentFull = 
                static_cast<double>(numValidBins_) / NUM_BINS * 100;
            
            std::ostringstream msg;

            msg << "gathering data "
                << std::setiosflags( std::ios::fixed )
                << std::setprecision( 1 )
                << percentFull << "%";
            
            return msg.str();
        }
        else if( possibleSteadyState_ )
        {
            int secondsInSteadyState = secondsSince( dwellStart_ );

            double dwellPercent = 
                static_cast<double>( secondsInSteadyState ) / 
                DWELL_SECONDS * 100;
           
            dwellPercent = std::min( dwellPercent, 100.0 );

            double slope;
            double rSquared;

            std::tie( slope, rSquared ) = getLinearFit();

            std::ostringstream msg;
            
            msg << "dwelling "
                << std::setiosflags( std::ios::fixed )
                << std::setprecision( 1 )
                << dwellPercent
                << "%, slope "
                << std::setprecision( 4 )
                << slope;
                //<< ", R^2 "
                //<< rSquared;

            return msg.str();
        }
        else
        {
            double slope;
            double rSquared;

            std::tie( slope, rSquared ) = getLinearFit();

            std::ostringstream msg;
                
            msg << "awaiting steady-state, slope "
                << std::setiosflags( std::ios::fixed )
                << std::setprecision( 4 )
                << slope;
                //<< ", R^2 "
                //<< rSquared;
            
            return msg.str();
        }
    }
  
    bool done() const
    {
        if( !full() ) return false;

        if( possibleSteadyState_ )
        {
            int secondsInSteadyState = secondsSince( dwellStart_ );

            if( secondsInSteadyState >= DWELL_SECONDS )
            {
                return true;
            }
        }

        return false;
    }

    private:
    
    bool full() const
    {
        if( numValidBins_ < NUM_BINS ) return false;

        return true;
    }
   
    // N.B. 
    // It's critical that we perform this linear regression without
    // becoming CPU-limited.  We need to remain IO bound so we are
    // actually measuring the storage device. --MarkSan
    std::tuple<double, double> getLinearFit() const
    {
        if( !full() )
        {
            throw std::runtime_error( "called too soon" );
        }

        static double currentSlope = 0;
        static double currentRSquared = 0;

        if( !changed_ )
        {
            return std::make_pair( currentSlope, currentRSquared );
        }

        const double sum_y =
            std::accumulate( data_.begin(), data_.end(), 0.0 );
        
        const double sum_xy =
            std::inner_product(
                boost::counting_iterator<int64_t>( 1 ),
                boost::counting_iterator<int64_t>( NUM_BINS ),
                data_.begin(),
                0.0
            );
     
        const double covar_xy = 
            sum_xy - ( ( SUM_X * sum_y ) / ( NUM_BINS - 1 ) );
        
        const double sum_sq_y =
            std::inner_product(
                data_.begin(),
                data_.end(),
                data_.begin(),
                0.0
            );

        const double var_y =
            sum_sq_y - ( ( sum_y * sum_y ) / ( NUM_BINS - 1 ) );

        const double std_dev_y = std::sqrt( var_y );

        const double corr = covar_xy / ( STD_DEV_X * std_dev_y );
        
        currentSlope = covar_xy / VAR_X;
        currentRSquared = corr * corr;
        
        //double currentIntercept =
        //    ( sum_y - currentSlope * SUM_X ) / NUM_BINS - 1;

        changed_ = false;
        
        return std::make_pair( currentSlope, currentRSquared );
    }
};

const double SteadyStateDetector::DEFAULT_SLOPE_TOLERANCE = 0.001; 

#endif // __STEADY_STATE_DETECTOR_H_
