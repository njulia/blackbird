#include "gdax.h"
#include "indicator.h"
#include "parameters.h"
#include "utils/restapi.h"
#include "unique_json.hpp"
#include "utils/base64.h"
#include "openssl/sha.h"
#include "openssl/hmac.h"
#include <iomanip>
#include <vector>
#include <array>

using std::string;
using std::cout;
using std::endl;
using std::min;
using std::max;
//using millisecs = std::chrono::milliseconds;
//using std::this_thread::sleep_for;

namespace GDAX
{
/*
    std::array<string, 3> ccys0 = {"BTC-EUR", "ETH-BTC", "ETH-EUR"};
    std::array<string, 3> ccys1 = {"BTC-EUR", "LTC-BTC", "LTC-EUR"};
    std::array<string, 3> ccys2 = {"BTC-EUR", "BCH-BTC", "BCH-EUR"};
    std::array<std::array<string, 3>, 10> ccys = {ccys0, ccys1, ccys2};
*/
std::array<std::array<string, 3>, 10> CCYS_USD = 
{
  {
  {"BTC-USD", "ETH-BTC", "ETH-USD"},
  {"BTC-USD", "LTC-BTC", "LTC-USD"}, 
  {"BTC-USD", "BCH-BTC", "BCH-USD"}
  }
};
std::array<std::array<string, 3>, 10> CCYS_EUR = 
{
  {
  {"BTC-EUR", "ETH-BTC", "ETH-EUR"},
  {"BTC-EUR", "LTC-BTC", "LTC-EUR"}, 
  {"BTC-EUR", "BCH-BTC", "BCH-EUR"}
  }
};

static RestApi &queryHandle(Parameters &params)
{
  static RestApi query("https://api.gdax.com",
                       params.cacert.c_str(), *params.logFile);
  return query;
}

quote_t getQuote(Parameters &params)
{
  auto &exchange = queryHandle(params);
  std::string pair;
  pair = "/products/";
  pair += params.leg1.c_str();
  pair += "-";
  pair += params.leg2.c_str();
  pair += "/ticker";
  unique_json root{exchange.getRequest(pair)};
  const char *bid, *ask;
  int unpack_fail = json_unpack(root.get(), "{s:s, s:s}", "bid", &bid, "ask", &ask);
  if (unpack_fail)
  {
    bid = "0";
    ask = "0";
  }

  //arbitrage(params);
  //macd(params);

  triangular(params, CCYS_USD, "USD", getAvail, getOrderbook, sendLimitOrder, isOrderComplete);
  triangular(params, CCYS_EUR, "EUR", getAvail, getOrderbook, sendLimitOrder, isOrderComplete);
  return std::make_pair(std::stod(bid), std::stod(ask));
}

double getAvail(Parameters &params, std::string currency)
{
  if (currency.compare("usd") == 0 || currency.compare("USD") == 0)
  {
      currency = "EUR";
      *params.logFile << "Change Currency from USD to EUR" << std::endl;
  }
  unique_json root{authRequest(params, "GET", "/accounts", "")};
  json_t *message  = json_object_get(root.get(), "message");
  if (message != NULL)
  {
    cout << "Message=" << json_string_value(message) << endl;
    return 0.0;
  }
  size_t arraySize = json_array_size(root.get());
  double available = 0.0;
  const char *currstr;
  for (size_t i = 0; i < arraySize; i++)
  {
    std::string tmpCurrency = json_string_value(json_object_get(json_array_get(root.get(), i), "currency"));
    std::string ava = json_string_value(json_object_get(json_array_get(root.get(), i), "available"));
    std::cout << "Currency=" << tmpCurrency << " : " << ava << std::endl;
    *params.logFile << "Currency=" << tmpCurrency << " : " << ava << std::endl;
    if (tmpCurrency.compare(currency.c_str()) == 0)
    {
      currstr = json_string_value(json_object_get(json_array_get(root.get(), i), "available"));
      if (currstr != NULL)
      {
        available = atof(currstr);
      }
      else
      {
        *params.logFile << "<GDAX> Error with currency string" << std::endl;
        std::cout << "<GDAX> Error with currency string" << std::endl;
        available = 0.0;
      }
    }
  }
  return available;
}

double getActivePos(Parameters &params)
{
  // TODO: this is not really a good way to get active positions
  return getAvail(params, "BTC");
}

double getLimitPrice(Parameters &params, double volume, bool isBid)
{
  auto &exchange = queryHandle(params);
  // TODO: Build a real URL with leg1 leg2 and auth post it
  // FIXME: using level 2 order book - has aggregated data but should be sufficient for now.
  unique_json root{exchange.getRequest("/products/BTC-USD/book?level=2")};
  auto bidask = json_object_get(root.get(), isBid ? "bids" : "asks");
  *params.logFile << "<GDAX> Looking for a limit price to fill "
                  << std::setprecision(8) << fabs(volume) << " Legx...\n";
  double tmpVol = 0.0;
  double p = 0.0;
  double v;
  int i = 0;
  while (tmpVol < fabs(volume) * params.orderBookFactor)
  {
    p = atof(json_string_value(json_array_get(json_array_get(bidask, i), 0)));
    v = atof(json_string_value(json_array_get(json_array_get(bidask, i), 1)));
    *params.logFile << "<GDAX> order book: "
                    << std::setprecision(8) << v << " @$"
                    << std::setprecision(8) << p << std::endl;
    tmpVol += v;
    i++;
  }
  return p;
}

std::string sendLimitOrder(Parameters &params, std::string direction, double quantity, double price, const string& ccy_pair)
{
  if (direction.compare("buy") != 0 && direction.compare("sell") != 0)
  {
    *params.logFile << "<GDAX> Error: Neither \"buy\" nor \"sell\" selected" << std::endl;
    return "0";
  }
  *params.logFile << "<GDAX> Trying to send a \"" << direction << "\" limit order: "
                  << ccy_pair << " - "
                  << std::setprecision(8) << quantity << " @ $"
                  << std::setprecision(8) << price << "...\n";
  cout << "<GDAX> Trying to send a \"" << direction << "\" limit order: "
                  << ccy_pair << " - "
                  << std::setprecision(8) << quantity << " @ $"
                  << std::setprecision(8) << price << "...\n";
  std::string type = direction;
  char buff[300];
  snprintf(buff, 300, "{\"size\":\"%.8f\",\"price\":\"%.8f\",\"side\":\"%s\",\"product_id\": \"%s\"}", quantity, price, type.c_str(), ccy_pair.c_str());
  unique_json root{authRequest(params, "POST", "/orders", buff)};
  auto txid = json_string_value(json_object_get(root.get(), "id"));

  *params.logFile << "<GDAX> Done (transaction ID: " << txid << ")\n" << endl;
  cout << "<GDAX> Done (transaction ID: " << txid << ")\n" << endl;
  return txid;
}

std::string sendLongOrder(Parameters &params, std::string direction, double quantity, double price)
{
  if (direction.compare("buy") != 0 && direction.compare("sell") != 0)
  {
    *params.logFile << "<GDAX> Error: Neither \"buy\" nor \"sell\" selected" << std::endl;
    return "0";
  }
  *params.logFile << "<GDAX> Trying to send a \"" << direction << "\" limit order: "
                  << std::setprecision(8) << quantity << " @ $"
                  << std::setprecision(8) << price << "...\n";
  std::string ccy_pair = "BTC-USD";
  std::string type = direction;
  char buff[300];
  snprintf(buff, 300, "{\"size\":\"%.8f\",\"price\":\"%.8f\",\"side\":\"%s\",\"product_id\": \"%s\"}", quantity, price, type.c_str(), ccy_pair.c_str());
  unique_json root{authRequest(params, "POST", "/orders", buff)};
  auto txid = json_string_value(json_object_get(root.get(), "id"));

  *params.logFile << "<GDAX> Done (transaction ID: " << txid << ")\n"
                  << std::endl;
  return txid;
}

bool isOrderComplete(Parameters &params, std::string orderId)
{

  unique_json root{authRequest(params, "GET", "/orders", "")};
  size_t arraySize = json_array_size(root.get());
  bool complete = true;
  const char *idstr;
  for (size_t i = 0; i < arraySize; i++)
  {
    std::string tmpId = json_string_value(json_object_get(json_array_get(root.get(), i), "id"));
    if (tmpId.compare(orderId.c_str()) == 0)
    {
      idstr = json_string_value(json_object_get(json_array_get(root.get(), i), "status"));
      *params.logFile << "<GDAX> Order still open (Status:" << idstr << ")" << std::endl;
      complete = false;
    }
  }
  return complete;
}

json_t *authRequest(Parameters &params, std::string method, std::string request, const std::string &options)
{
  // create timestamp

  //static uint64_t nonce = time(nullptr);
  std::string nonce = gettime();
  // create data string

  std::string post_data = nonce + method + request + options;

  //if (!options.empty())
  //  post_data += options;

  // create decoded key

  std::string decoded_key = base64_decode(params.gdaxSecret);

  // Message Signature using HMAC-SHA256 of (NONCE+ METHOD??? + PATH + body)

  uint8_t *hmac_digest = HMAC(EVP_sha256(),
                              decoded_key.c_str(), decoded_key.length(),
                              reinterpret_cast<const uint8_t *>(post_data.data()), post_data.size(), NULL, NULL);

  // encode the HMAC to base64
  std::string api_sign_header = base64_encode(hmac_digest, SHA256_DIGEST_LENGTH);

  // cURL header

  std::array<std::string, 5> headers{
      "CB-ACCESS-KEY:" + params.gdaxApi,
      "CB-ACCESS-SIGN:" + api_sign_header,
      "CB-ACCESS-TIMESTAMP:" + nonce,
      "CB-ACCESS-PASSPHRASE:" + params.gdaxPhrase,
      "Content-Type: application/json; charset=utf-8",
  };

  // cURL request
  auto &exchange = queryHandle(params);

  //TODO: this sucks please do something better
  if (method.compare("GET") == 0)
  {
    return exchange.getRequest(request, make_slist(std::begin(headers), std::end(headers)));
  }
  else if (method.compare("POST") == 0)
  {
    return exchange.postRequest(request, make_slist(std::begin(headers), std::end(headers)), options);
  }
  else
  {
    std::cout << "Error With Auth method. Exiting with code 0" << std::endl;
    exit(0);
  }
}
std::string gettime()
{

  timeval curTime;
  gettimeofday(&curTime, NULL);
  int milli = curTime.tv_usec / 1000;
  char buffer[80];
  strftime(buffer, 80, "%Y-%m-%dT%H:%M:%S", gmtime(&curTime.tv_sec));
  char time_buffer2[200];
  snprintf(time_buffer2, 200, "%s.%d000Z", buffer, milli);
  snprintf(time_buffer2, 200, "%sZ", buffer);

  //return time_buffer2;

  time_t result = time(NULL);
  long long result2 = result;
  char buff4[40];
  snprintf(buff4, 40, "%lld", result2);

  char buff5[40];
  snprintf(buff5, 40, "%lld.%d000", result2, milli);
  return buff5;
}

void testGDAX()
{

  Parameters params("bird.conf");
  params.logFile = new std::ofstream("./test.log", std::ofstream::trunc);

  std::string orderId;

  //std::cout << "Current value LEG1_LEG2 bid: " << getQuote(params).bid() << std::endl;
  //std::cout << "Current value LEG1_LEG2 ask: " << getQuote(params).ask() << std::endl;
  //std::cout << "Current balance BTC: " << getAvail(params, "BTC") << std::endl;
  //std::cout << "Current balance USD: " << getAvail(params, "USD")<< std::endl;
  //std::cout << "Current balance ETH: " << getAvail(params, "ETH")<< std::endl;
  //std::cout << "Current balance BTC: " << getAvail(params, "BCH")<< std::endl;
  //std::cout << "current bid limit price for 10 units: " << getLimitPrice(params, 10 , true) << std::endl;
  //std::cout << "Current ask limit price for .09 units: " << getLimitPrice(params, 0.09, false) << std::endl;
  //std::cout << "Sending buy order for 0.005 BTC @ ASK! USD - TXID: " << std::endl;
  //orderId = sendLongOrder(params, "buy", 0.005, getLimitPrice(params,.005,false));
  //std::cout << orderId << std::endl;
  //std::cout << "Buy Order is complete: " << isOrderComplete(params, orderId) << std::endl;

  //std::cout << "Sending sell order for 0.02 BTC @ 10000 USD - TXID: " << std::endl;
  //orderId = sendLongOrder(params, "sell", 0.02, 10000);
  //std::cout << orderId << std::endl;
  //std::cout << "Sell order is complete: " << isOrderComplete(params, orderId) << std::endl;
  //std::cout << "Active Position: " << getActivePos(params);
}

bool getOrderbook(Parameters& params, const string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size)
{
  if (ccy_pair.empty())
      return false;
  auto &exchange = queryHandle(params);
  const string url = "/products/" + ccy_pair + "/book?level=1";
  unique_json root { exchange.getRequest(url) };
  json_t *bids = json_object_get(root.get(), "bids");
  json_t *asks = json_object_get(root.get(), "asks");
  if ( !json_is_array(bids) || !json_is_array(asks) )
  {
    cout << "GDAX OrderBook Bids or Asks is not array: " << ccy_pair << endl;
    *params.logFile << "GDAX OrderBook Bids or Asks is not array: " << ccy_pair << endl;
    return false;
  }  
  // Get the top bid and ask in the order book 
  json_t *bid = json_array_get(bids, 0);
  json_t *ask = json_array_get(asks, 0);
  if ( !json_is_array(bid) || !json_is_array(ask) )
  {
    cout << "GDAX OrderBook top Bid or Ask is not array: " << ccy_pair << endl;
    *params.logFile << "GDAX OrderBook top Bid or Ask is not array: " << ccy_pair << endl;
    return false;
  }  
  const char *bid_price_str = json_string_value(json_array_get(bid, 0));
  const char *bid_size_str = json_string_value(json_array_get(bid, 1));
  const char *ask_price_str = json_string_value(json_array_get(ask, 0));
  const char *ask_size_str = json_string_value(json_array_get(ask, 1));

  bid_price = bid_price_str ? atof(bid_price_str) : 0.0; 
  bid_size = bid_size_str ? atof(bid_size_str) : 0.0;
  ask_price = ask_price_str ? atof(ask_price_str) : 0.0;
  ask_size = ask_size_str ? atof(ask_size_str) : 0.0;

  //cout << std::setprecision(8) << "GDAX  " << url  << " : " << bid_price << " x " << bid_size << " : " << ask_price << " x " << ask_size << endl;
  *params.logFile << std::setprecision(8) << "GDAX  " << url  << "  : " << bid_price << " x " << bid_size << " : " << ask_price << " x " << ask_size << endl;
  return true;

} 

bool macd(Parameters& params)
{
  static unsigned int count = 0;
  if (count++ % 200 != 0)
      return false; 
  float fees = params.gdaxFees;
  //BTC-EUR
  static float account_btc = 1000;
  static double btc_balance = 0.0;
  Indicators *pbtc = params.m_pbtc;
  cout << "BTC Indicators=" << pbtc<< endl;
  string ccy_pair = "BTC-EUR";
  double btceur_bid_price, btceur_bid_size, btceur_ask_price, btceur_ask_size;
  if (!getOrderbook(params, ccy_pair, btceur_bid_price, btceur_bid_size, btceur_ask_price, btceur_ask_size))
      return false;
  int btc_signal = pbtc->onUpdate(btceur_bid_price, btceur_ask_price, btceur_bid_size, btceur_ask_size);
  if (btc_signal > 0)
  {
    cout << "Buy signal: BTC-EUR EUR=" << account_btc << " BTC=" << btc_balance << endl;
    *params.logFile << "Buy signal: BTC-EUR EUR=" << account_btc << " BTC=" << btc_balance << endl;
    //double cur_balance = getAvail(params, "EUR");
    // Buy BTC-EUR order
    double ticksize = 0.01;
    double btceur_size = trim(min(account_btc / btceur_ask_price, btceur_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    double btceur_price = btceur_ask_price;
    account_btc -= btceur_size * btceur_price * (1+fees);
    btc_balance += btceur_size;
    cout << "Buy BTC " << btceur_price << " @ " << btceur_size << " , EUR=" << account_btc << " BTC=" << btc_balance << endl;
    *params.logFile << "Buy BTC " << btceur_price << " @ " << btceur_size << " , EUR=" << account_btc << " BTC=" << btc_balance << endl;
    /*
    auto btcusd_id = sendLimitOrder(params, "buy", btcusd_size, btcusd_price, "BTC-EUR"); 
    sleep_for(millisecs(5000));
    bool is_order_complete = isOrderComplete(params, btcusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(5000));
      cout << "Buy BTC-EUR " << btcusd_price << " @ " << btcusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, btcusd_id);
    }
    // Reset the order id
    btcusd_id ="0";
    cout << "Buy BTC-EUR Done" << endl;
    *params.logFile << "Buy BTC-EUR Done" << endl;
    */
  }
  else if (btc_signal < 0 && btc_balance > 0.0) 
  {
    cout << "Sell signal: BTC-EUR EUR=" << account_btc << " BTC=" << btc_balance << endl;
    *params.logFile << "Sell signal: BTC-EUR EUR=" << account_btc << " BTC=" << btc_balance << endl;
    //double cur_balance = getAvail(params, "EUR");
    // Buy BTC-EUR order
    double ticksize = 0.01;
    double btceur_size = btc_balance;
    double btceur_price = btceur_bid_price;
    account_btc += btceur_size * btceur_price * (1-fees);
    btc_balance -= btceur_size;
    cout << "Sell BTC " << btceur_price << " @ " << btceur_size << " , EUR=" << account_btc << " BTC=" << btc_balance << endl;
    *params.logFile << "Sell BTC " << btceur_price << " @ " << btceur_size << " , EUR=" << account_btc << " BTC=" << btc_balance << endl;
  }

  //ETH-EUR
  static float account_eth = 1000;
  static double eth_balance = 0.0;
  Indicators *peth = params.m_peth;
  cout << "ETH Indicators=" << peth << endl;
  ccy_pair = "ETH-EUR";
  double etheur_bid_price, etheur_bid_size, etheur_ask_price, etheur_ask_size;
  if (!getOrderbook(params, ccy_pair, etheur_bid_price, etheur_bid_size, etheur_ask_price, etheur_ask_size))
      return false; 
  int eth_signal = peth->onUpdate(etheur_bid_price, etheur_ask_price, etheur_bid_size, etheur_ask_size);
  if (eth_signal > 0)
  {
    cout << "Buy signal: ETH-EUR EUR=" << account_eth << " ETH=" << eth_balance << endl;
    *params.logFile << "Buy signal: ETH-EUR EUR=" << account_eth << " ETH=" << eth_balance << endl;
    //double cur_balance = getAvail(params, "EUR");
    // Buy ETH-EUR order
    double ticksize = 0.01;
    double etheur_size = trim(min(account_eth / etheur_ask_price, etheur_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    double etheur_price = etheur_ask_price;
    account_eth -= etheur_size * etheur_price * (1+fees);
    eth_balance += etheur_size;
    cout << "Buy ETH " << etheur_price << " @ " << etheur_size << " , EUR=" << account_eth << " BTC=" << eth_balance << endl;
    *params.logFile << "Buy ETH " << etheur_price << " @ " << etheur_size << " , EUR=" << account_eth << " BTC=" << eth_balance << endl;
    /*
    auto btcusd_id = sendLimitOrder(params, "buy", btcusd_size, btcusd_price, "BTC-EUR"); 
    sleep_for(millisecs(5000));
    bool is_order_complete = isOrderComplete(params, btcusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(5000));
      cout << "Buy BTC-EUR " << btcusd_price << " @ " << btcusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, btcusd_id);
    }
    // Reset the order id
    btcusd_id ="0";
    cout << "Buy BTC-EUR Done" << endl;
    *params.logFile << "Buy BTC-EUR Done" << endl;
    */
  }
  else if (eth_signal < 0 && eth_balance > 0.0)
  {
    cout << "Sell signal: ETH-EUR EUR=" << account_eth << " ETH=" << eth_balance << endl;
    *params.logFile << "Sell signal: ETH-EUR EUR=" << account_eth << " ETH=" << eth_balance << endl;
    //double cur_balance = getAvail(params, "EUR");
    // Buy ETH-EUR order
    double ticksize = 0.01;
    double etheur_size = eth_balance;
    double etheur_price = etheur_bid_price;
    account_eth += etheur_size * etheur_price * (1-fees);
    eth_balance -= etheur_size;
    cout << "Sell ETH " << etheur_price << " @ " << etheur_size << " , EUR=" << account_eth << " ETH=" << eth_balance << endl;
    *params.logFile << "Sell ETH " << etheur_price << " @ " << etheur_size << " , EUR=" << account_eth << " ETH=" << eth_balance << endl;
  }
  return false;
}

}
