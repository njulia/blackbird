#ifndef GDAX_H
#define GDAX_H

#include "quote_t.h"
#include <string>

struct json_t;
struct Parameters;

namespace GDAX {

quote_t getQuote(Parameters& params);

double getAvail(Parameters& params, std::string currency);

double getActivePos(Parameters& params);

double getLimitPrice(Parameters& params, double volume, bool isBid);

std::string sendLimitOrder(Parameters &params, std::string direction, double quantity, double price, const std::string& ccy_pair);

std::string sendLongOrder(Parameters& params, std::string direction, double quantity, double price);

bool isOrderComplete(Parameters& params, std::string orderId);

json_t* authRequest(Parameters& params, std::string method, std::string request,const std::string &options);

std::string gettime();

void testGDAX();

bool getOrderbook(Parameters& params, const std::string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size);

bool macd(Parameters& params);

}

#endif
