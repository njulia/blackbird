#ifndef BITSTAMP_H
#define BITSTAMP_H

#include "quote_t.h"

#include <string>

struct json_t;
struct Parameters;

namespace Bitstamp {

quote_t getQuote(Parameters& params);

double getAvail(Parameters& params, std::string currency);

std::string sendLongOrder(Parameters& params, std::string direction, double quantity, double price);
std::string sendLimitOrder(Parameters& params, std::string direction, double quantity, double price, const std::string& ccy_pair);

bool isOrderComplete(Parameters& params, std::string orderId);

double getActivePos(Parameters& params);

double getLimitPrice(Parameters& params, double volume, bool isBid);

bool get_orderbook(Parameters& params, const std::string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size);

bool arbitrage(Parameters& params);

}

#endif
