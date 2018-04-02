#ifndef GEMINI_H
#define GEMINI_H

#include "quote_t.h"
#include <string>

struct json_t;
struct Parameters;

namespace Gemini {

quote_t getQuote(Parameters& params);

double getAvail(Parameters& params, std::string currency);

std::string sendLongOrder(Parameters& params, std::string direction, double quantity, double price);

bool isOrderComplete(Parameters& params, std::string orderId);

double getActivePos(Parameters& params);

double getLimitPrice(Parameters& params, double volume, bool isBid);

json_t* authRequest(Parameters& params, std::string url, std::string request, std::string options);

std::string sendLimitOrder(Parameters &params, std::string direction, double quantity, double price, const std::string& ccy_pair);

bool getOrderbook(Parameters& params, const std::string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size);
}

#endif
