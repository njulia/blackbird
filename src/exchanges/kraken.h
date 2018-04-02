#ifndef KRAKEN_H
#define KRAKEN_H

#include "quote_t.h"
#include <string>

struct json_t;
struct Parameters;

namespace Kraken {

quote_t getQuote(Parameters& params);

double getAvail(Parameters& params, std::string currency);

std::string sendLongOrder(Parameters& params, std::string direction, double quantity, double price);

std::string sendShortOrder(Parameters& params, std::string direction, double quantity, double price);

std::string sendLimitOrder(Parameters &params, std::string direction, double quantity, double price, const std::string& pair);

bool isOrderComplete(Parameters& params, std::string orderId);

double getActivePos(Parameters& params);

double getLimitPrice(Parameters& params, double volume, bool isBid);

json_t* authRequest(Parameters& params, std::string request, std::string options = "");

void testKraken();

bool getOrderbook(Parameters& params, const std::string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size);

}

#endif
