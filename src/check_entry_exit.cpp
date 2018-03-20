#include "check_entry_exit.h"
#include "bitcoin.h"
#include "result.h"
#include "parameters.h"
#include <sstream>
#include <iomanip>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>


bool equal(double a, double b)
{
    double epsilon = std::numeric_limits<double>::epsilon();
    return ((a-b) > -1.0*epsilon) && ((a-b) <epsilon); 
}


// Not sure to understand what's going on here ;-)
template <typename T>
static typename std::iterator_traits<T>::value_type compute_sd(T first, const T &last) {
  using namespace std;
  typedef typename iterator_traits<T>::value_type value_type;
  
  auto n  = distance(first, last);
  auto mu = accumulate(first, last, value_type()) / n;
  auto squareSum = inner_product(first, last, first, value_type());
  return sqrt(squareSum / n - mu * mu);
}

// Returns a double as a string '##.##%'
std::string percToStr(double perc) {
  std::ostringstream s;
  if (perc >= 0.0) s << " ";
  s << std::fixed << std::setprecision(2) << perc * 100.0 << "%";
  return s.str();
}

bool checkEntry(Bitcoin* btcLong, Bitcoin* btcShort, Result& res, Parameters& params) {
  // Only check the arbitrage opportunities when the btcShort exchange support short
  if (!btcShort->getHasShort()) 
      return false;

  // Gets the prices and computes the spread
  double priceLong = btcLong->getAsk();
  double priceShort = btcShort->getBid();
  // If the prices are null we return a null spread
  // to avoid false opportunities
  if (priceLong > 0.0 && priceShort > 0.0) {
    res.spreadIn = (priceShort - priceLong) / priceLong;
  } else {
    res.spreadIn = 0.0;
    return false;
  }
  int longId = btcLong->getId();
  int shortId = btcShort->getId();

  // We update the max and min spread if necessary
  res.maxSpread[longId][shortId] = std::max(res.spreadIn, res.maxSpread[longId][shortId]);
  res.minSpread[longId][shortId] = std::min(res.spreadIn, res.minSpread[longId][shortId]);
  res.feesLong = btcLong->getFees();
  res.feesShort = btcShort->getFees();
  // The spread range between (current spread , min spread record)
  double spreadRange = res.spreadIn - res.minSpread[longId][shortId];
  // The max spread range in the record between (max spread record , min spread record)
  double spreadRangeMax = res.maxSpread[longId][shortId] - res.minSpread[longId][shortId];
  // The spread threshold to make profit parmas.spreadTarget. 1.2 is used to adjust the entry point, the bigger, the more difficult to entry
  double spreadThresh = params.spreadTarget * 1.2 + 2.0*(res.feesLong + res.feesShort);
  
  if (params.verbose) {
    params.logFile->precision(2);
    *params.logFile << "   " << btcLong->getExchName() << "/" << btcShort->getExchName() << ":\t" << percToStr(res.spreadIn);
    *params.logFile << " [target " << percToStr(params.spreadTarget) << ", min " << percToStr(res.minSpread[longId][shortId]) << ", max " << percToStr(res.maxSpread[longId][shortId]) << "]";
    *params.logFile << " [current_spread_range " << percToStr(spreadRange) << ", spread_threshold " << percToStr(spreadThresh) << ", spread_range_max" << percToStr(spreadRangeMax) <<"]";
   
    std::cout << "Compare   " << btcLong->getExchName() << "/" << btcShort->getExchName() << ":\t" << percToStr(res.spreadIn);
    std::cout << " [target " << percToStr(params.spreadTarget) << ", min " << percToStr(res.minSpread[longId][shortId]) << ", max " << percToStr(res.maxSpread[longId][shortId]) << "]";
    std::cout << " [current_spread_range " << percToStr(spreadRange) << ", spread_threshold " << percToStr(spreadThresh) << ", spread_range_max" << percToStr(spreadRangeMax) <<"]";
    // The short-term volatility is computed and
    // displayed. No other action with it for
    // the moment.
    if (params.useVolatility) {
      if (res.volatility[longId][shortId].size() >= params.volatilityPeriod) {
        auto stdev = compute_sd(begin(res.volatility[longId][shortId]), end(res.volatility[longId][shortId]));
        *params.logFile << "  volat. " << stdev * 100.0 << "%";
        std::cout << "  volat. " << stdev * 100.0 << "%";
      } else {
        *params.logFile << "  volat. n/a " << res.volatility[longId][shortId].size() << "<" << params.volatilityPeriod << " ";
      }
    }
    // Updates the trailing spread
    // TODO: explain what a trailing spread is.
    // See #12 on GitHub for the moment
    if (!equal(res.trailing[longId][shortId], TRAILING_MIN)) {
      *params.logFile << "   trailing " << percToStr(res.trailing[longId][shortId]) << "  " << res.trailingWaitCount[longId][shortId] << "/" << params.trailingCount;
      std::cout << "   trailing " << percToStr(res.trailing[longId][shortId]) << "  " << res.trailingWaitCount[longId][shortId] << "/" << params.trailingCount;
    }
    // If one of the exchanges (or both) hasn't been implemented,
    // we mention in the log file that this spread is for info only.
    if ((!btcLong->getIsImplemented() || !btcShort->getIsImplemented()) && !params.demoMode)
      *params.logFile << "   info only";

    *params.logFile << std::endl;
    std::cout << std::endl;
  }
  // We need both exchanges to be implemented,
  // otherwise we return False regardless of
  // the opportunity found.
  if (!btcLong->getIsImplemented() ||
      !btcShort->getIsImplemented() ||
      res.spreadIn == 0.0)
    return false;

  // the trailing spread is reset for this pair,
  // because once the spread is *below*
  // SpreadEndtry. Again, see #12 on GitHub for
  // more details.
  if (res.spreadIn < params.spreadEntry || spreadRange < spreadThresh) {
    res.trailing[longId][shortId] = TRAILING_MIN;
    res.trailingWaitCount[longId][shortId] = 0;
    return false;
  }

  // Updates the trailingSpread with the new value
  double newTrailValue = res.spreadIn - params.trailingLim;
  //if (res.trailing[longId][shortId] == -1.0) {
  if (equal(res.trailing[longId][shortId], TRAILING_MIN)) {
    res.trailing[longId][shortId] = std::max(newTrailValue, params.spreadEntry);
    return false;
  }

  if (newTrailValue >= res.trailing[longId][shortId]) {
    res.trailing[longId][shortId] = newTrailValue;
    res.trailingWaitCount[longId][shortId] = 0;
  }
  if (res.spreadIn >= res.trailing[longId][shortId]) {
    res.trailingWaitCount[longId][shortId] = 0;
    return false;
  }

  if (res.trailingWaitCount[longId][shortId] < params.trailingCount) {
    res.trailingWaitCount[longId][shortId]++;
    return false;
  }

  // Updates the Result structure with the information about
  // the two trades and return True (meaning an opportunity
  // was found).
  res.idExchLong = longId;
  res.idExchShort = shortId;
  //res.feesLong = btcLong->getFees();
  //res.feesShort = btcShort->getFees();
  res.exchNameLong = btcLong->getExchName();
  res.exchNameShort = btcShort->getExchName();
  res.priceLongIn = priceLong;
  res.priceShortIn = priceShort;
  res.exitTarget = res.spreadIn - params.spreadTarget - 2.0*(res.feesLong + res.feesShort);
  res.trailingWaitCount[longId][shortId] = 0;
  return true;
}

bool checkExit(Bitcoin* btcLong, Bitcoin* btcShort, Result& res, Parameters& params, time_t period) {
  double priceLong  = btcLong->getBid();
  double priceShort = btcShort->getAsk();
  if (priceLong > 0.0 && priceShort > 0.0) {
    res.spreadOut = (priceShort - priceLong) / priceLong;
  } else {
    res.spreadOut = 0.0;
    return false;
  }
  int longId = btcLong->getId();
  int shortId = btcShort->getId();

  res.maxSpread[longId][shortId] = std::max(res.spreadOut, res.maxSpread[longId][shortId]);
  res.minSpread[longId][shortId] = std::min(res.spreadOut, res.minSpread[longId][shortId]);
  double spreadRange = res.spreadOut - res.minSpread[longId][shortId];

  if (params.verbose) {
    params.logFile->precision(2);
    *params.logFile << "   " << btcLong->getExchName() << "/" << btcShort->getExchName() << ":\t" << percToStr(res.spreadOut);
    *params.logFile << " [target " << percToStr(res.exitTarget) << ", min " << percToStr(res.minSpread[longId][shortId]) << ", max " << percToStr(res.maxSpread[longId][shortId]) << "]";
    *params.logFile << " [current_spread_range " << percToStr(spreadRange) << "]";
    std::cout << "   " << btcLong->getExchName() << "/" << btcShort->getExchName() << ":\t" << percToStr(res.spreadOut);
    std::cout << " [target " << percToStr(res.exitTarget) << ", min " << percToStr(res.minSpread[longId][shortId]) << ", max " << percToStr(res.maxSpread[longId][shortId]) << "]";
    std::cout << " [current_spread_range " << percToStr(spreadRange) << "]";
    // The short-term volatility is computed and
    // displayed. No other action with it for
    // the moment.
    if (params.useVolatility) {
      if (res.volatility[longId][shortId].size() >= params.volatilityPeriod) {
        auto stdev = compute_sd(begin(res.volatility[longId][shortId]), end(res.volatility[longId][shortId]));
        *params.logFile << "  volat. " << stdev * 100.0 << "%";
        std::cout << "  volat. " << stdev * 100.0 << "%";
      } else {
        *params.logFile << "  volat. n/a " << res.volatility[longId][shortId].size() << "<" << params.volatilityPeriod << " ";
      }
    }
    if (!equal(res.trailing[longId][shortId], TRAILING_MAX)) {
      *params.logFile << "   trailing " << percToStr(res.trailing[longId][shortId]) << "  " << res.trailingWaitCount[longId][shortId] << "/" << params.trailingCount;
      std::cout << "   trailing " << percToStr(res.trailing[longId][shortId]) << "  " << res.trailingWaitCount[longId][shortId] << "/" << params.trailingCount;
    }
  }
  *params.logFile << std::endl;
  std::cout << std::endl;
  if (period - res.entryTime >= int(params.maxLength)) {
    res.priceLongOut  = priceLong;
    res.priceShortOut = priceShort;
    return true;
  }
  if (res.spreadOut == 0.0) return false;
  if (res.spreadOut > res.exitTarget) {
    res.trailing[longId][shortId] = TRAILING_MAX;
    res.trailingWaitCount[longId][shortId] = 0;
    return false;
  }

  double newTrailValue = res.spreadOut + params.trailingLim;
  if (equal(res.trailing[longId][shortId], TRAILING_MAX)) {
    res.trailing[longId][shortId] = std::min(newTrailValue, res.exitTarget);
    return false;
  }
  if (newTrailValue <= res.trailing[longId][shortId]) {
    res.trailing[longId][shortId] = newTrailValue;
    res.trailingWaitCount[longId][shortId] = 0;
  }
  if (res.spreadOut <= res.trailing[longId][shortId]) {
    res.trailingWaitCount[longId][shortId] = 0;
    return false;
  }
  if (res.trailingWaitCount[longId][shortId] < params.trailingCount) {
    res.trailingWaitCount[longId][shortId]++;
    return false;
  }

  res.priceLongOut  = priceLong;
  res.priceShortOut = priceShort;
  res.trailingWaitCount[longId][shortId] = 0;
  return true;
}
