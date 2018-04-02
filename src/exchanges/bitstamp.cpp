#include "bitstamp.h"
#include "parameters.h"
#include "utils/restapi.h"
#include "utils/base64.h"
#include "unique_json.hpp"
#include "hex_str.hpp"

#include "openssl/sha.h"
#include "openssl/hmac.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <chrono>

using std::string;
using std::cout;
using std::endl;
using std::min;
using std::max;
using millisecs = std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace Bitstamp {
const char* ccys[5][3]={{"btcusd", "ethbtc", "ethusd"},
                   {"btcusd", "xrpbtc", "xrpusd"},
                   {"btcusd", "ltcbtc", "ltcusd"},
                   {"btcusd", "bchbtc", "bchusd"},
                   {"eurusd", "btceur", "btcusd"}};
/*
const char* CCYS_EUR[4][3]={{"btceur", "ethbtc", "etheur"},
                   {"btceur", "xrpbtc", "xrpeur"},
                   {"btceur", "ltcbtc", "ltceur"},
                   {"btceur", "bchbtc", "bcheur"}};
*/
std::array<std::array<string, 3>, 10> CCYS_USD = 
{
  {
  {"btcusd", "ethbtc", "ethusd"},
  {"btcusd", "xrpbtc", "xrpusd"},
  {"btcusd", "ltcbtc", "ltcusd"},
  {"btcusd", "bchbtc", "bchusd"},
  {"eurusd", "btceur", "btcusd"}
  }
};
std::array<std::array<string, 3>, 10> CCYS_EUR = 
{
  {
  {"btceur", "ethbtc", "etheur"},
  {"btceur", "xrpbtc", "xrpeur"},
  {"btceur", "ltcbtc", "ltceur"},
  {"btceur", "bchbtc", "bcheur"}
  }
};

static json_t* authRequest(Parameters &, std::string, std::string);

static RestApi& queryHandle(Parameters &params)
{
  static RestApi query ("https://www.bitstamp.net",
                        params.cacert.c_str(), *params.logFile);
  return query;
}

static json_t* checkResponse(std::ostream &logFile, json_t *root)
{
  auto errstatus = json_object_get(root, "error");
  if (errstatus)
  {
    // Bitstamp errors could sometimes be strings or objects containing error string
    auto errmsg = json_dumps(errstatus, JSON_ENCODE_ANY);
    logFile << "<Bitstamp> Error with response: "
            << errmsg << '\n';
    free(errmsg);
  }

  return root;
}

quote_t getQuote(Parameters& params)
{
  
  // BTC/USD
  auto &exchange = queryHandle(params);
  unique_json root { exchange.getRequest("/api/ticker") };

  const char *quote = json_string_value(json_object_get(root.get(), "bid"));
  auto btcusd_bid = quote ? atof(quote) : 0.0;
  quote = json_string_value(json_object_get(root.get(), "ask"));
  auto btcusd_ask = quote ? atof(quote) : 0.0;
  
  //arbitrage(params);
  triangular(params, CCYS_USD, "usd", getAvail, getOrderbook, sendLimitOrder, isOrderComplete);
  triangular(params, CCYS_EUR, "eur", getAvail, getOrderbook, sendLimitOrder, isOrderComplete);
  
/*
  // ETH/USD
  unique_json ethusd_root { exchange.getRequest("/api/v2/ticker/ethusd") };
  quote = json_string_value(json_object_get(ethusd_root.get(), "bid"));
  auto ethusd_bid = quote ? atof(quote) : 0.0;
  quote = json_string_value(json_object_get(ethusd_root.get(), "ask"));
  auto ethusd_ask = quote ? atof(quote) : 0.0;
  std::cout << "Bitstamp ETHUSD " << ethusd_bid << " : " << ethusd_ask << std::endl;
  
  // ETH/BTC
  unique_json ethbtc_root { exchange.getRequest("/api/v2/ticker/ethbtc") };
  quote = json_string_value(json_object_get(ethbtc_root.get(), "bid"));
  auto ethbtc_bid = quote ? atof(quote) : 0.0;
  quote = json_string_value(json_object_get(ethbtc_root.get(), "ask"));
  auto ethbtc_ask = quote ? atof(quote) : 0.0;
  std::cout << "Bitstamp ETHUSD " << ethbtc_bid << " : " << ethbtc_ask << std::endl;

  float fees = 0.0;//params.bitstampFees;
  static float account = 1000.0;
  float pnl = account / btcusd_ask / ethbtc_ask * ethusd_bid - fees*(btcusd_ask + ethbtc_ask + ethusd_bid) - account;
  std::cout << "Bitstamp BTC PNL=" << pnl << std::endl;
  if (pnl > account *0.0001)
  {
    cur_balance = getAvail(params, "usd");
   
    
    account += pnl;
    std::cout << "Bitstamp buy BTC, PNL=" << pnl << " Account=" << account << std::endl;
    *params.logFile << "Bitstamp buy BTC, PNL=" << pnl << " Account=" << account << std::endl;
  }
  else
  {
      pnl = account / ethusd_ask * ethbtc_bid * btcusd_bid - fees*(ethusd_ask + ethbtc_bid +btcusd_bid) - account;
      std::cout << "Bitstamp ETH PNL=" << pnl << std::endl;
      if (pnl > 0.0)
      {
        account += pnl;
        std::cout << "Bitstamp buy ETH, PNL=" << pnl << " Account=" << account << std::endl;
        *params.logFile << "Bitstamp buy ETH, PNL=" << pnl << " Account=" << account << std::endl;
      }
  }
*/
  return std::make_pair(btcusd_bid, btcusd_ask);
}

double getAvail(Parameters& params, std::string currency)
{
  unique_json root { authRequest(params, "/api/balance/", "") };
  while (json_object_get(root.get(), "message") != NULL)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto dump = json_dumps(root.get(), 0);
    *params.logFile << "<Bitstamp> Error with JSON: " << dump << ". Retrying..." << std::endl;
    free(dump);
    root.reset(authRequest(params, "/api/balance/", ""));
  }
  double availability = 0.0;
  const char* returnedText = NULL;
  if (currency == "btc")
  {
    returnedText = json_string_value(json_object_get(root.get(), "btc_balance"));
  }
  else if (currency == "usd")
  {
    returnedText = json_string_value(json_object_get(root.get(), "usd_balance"));
  }
  else
  {
    const std::string quote = currency + "_balance";
    returnedText = json_string_value(json_object_get(root.get(), quote.c_str()));
  }
  if (returnedText != NULL)
  {
    availability = atof(returnedText);
  }
  else
  {
    std::cout<< "<Bitstamp> Error with the credentials: " << currency << std::endl;
    *params.logFile << "<Bitstamp> Error with the credentials." << currency << std::endl;
    availability = 0.0;
  }

  return availability;
}

std::string sendLongOrder(Parameters& params, std::string direction, double quantity, double price)
{
  *params.logFile << "<Bitstamp> Trying to send a \"" << direction << "\" limit order: "
                  << std::setprecision(6) << quantity << "@$"
                  << std::setprecision(2) << price << "...\n";
  cout << "<Bitstamp> Trying to send a \"" << direction << "\" limit order: "
                  << std::setprecision(6) << quantity << "@$"
                  << std::setprecision(2) << price << "...\n";
  std::string url = "/api/" + direction + '/';

  std::ostringstream oss;
  oss << "amount=" << quantity << "&price=" << std::fixed << std::setprecision(2) << price;
  std::string options = oss.str();
  unique_json root { authRequest(params, url, options) };
  auto orderId = std::to_string(json_integer_value(json_object_get(root.get(), "id")));
  if (orderId == "0")
  {
    auto dump = json_dumps(root.get(), 0);
    *params.logFile << "<Bitstamp> Order ID = 0. Message: " << dump << '\n';
    cout << "<Bitstamp> Order ID = 0. Message: " << dump << '\n';
    free(dump);
  }
  *params.logFile << "<Bitstamp> Done (order ID: " << orderId << ")\n" << std::endl;
  cout << "<Bitstamp> Done (order ID: " << orderId << ")\n" << std::endl;

  return orderId;
}

std::string sendLimitOrder(Parameters& params, std::string direction, double quantity, double price, const string& ccy_pair)
{
  *params.logFile << "<Bitstamp> Trying to send a \"" << direction << "\" limit order: "
                  << std::setprecision(6) << quantity << "@$"
                  << std::setprecision(2) << price << "...\n";
  cout << "<Bitstamp> Trying to send a \"" << direction << "\" limit order: "
                  << std::setprecision(6) << quantity << "@$"
                  << std::setprecision(2) << price << "...\n";
  std::string url = "/api/v2/" + direction + '/' + ccy_pair + "/";;

  std::ostringstream oss;
  oss << "amount=" << quantity << "&price=" << std::fixed << std::setprecision(2) << price;
  std::string options = oss.str();
  unique_json root { authRequest(params, url, options) };
  auto orderId = std::to_string(json_integer_value(json_object_get(root.get(), "id")));
  if (orderId == "0")
  {
    auto dump = json_dumps(root.get(), 0);
    *params.logFile << "<Bitstamp> Order ID = 0. Message: " << dump << '\n';
    cout << "<Bitstamp> Order ID = 0. Message: " << dump << '\n';
    free(dump);
  }
  *params.logFile << "<Bitstamp> Done (order ID: " << orderId << ")\n" << std::endl;
  cout << "<Bitstamp> Done (order ID: " << orderId << ")\n" << std::endl;

  return orderId;
}

bool isOrderComplete(Parameters& params, std::string orderId)
{
  if (orderId == "0") return true;

  auto options = "id=" + orderId;
  unique_json root { authRequest(params, "/api/order_status/", options) };
  auto status = json_string_value(json_object_get(root.get(), "status"));
  return status && status == std::string("Finished");
}

double getActivePos(Parameters& params) { return getAvail(params, "btc"); }

double getLimitPrice(Parameters& params, double volume, bool isBid)
{
  auto &exchange = queryHandle(params);
  unique_json root { exchange.getRequest("/api/order_book") };
  auto orderbook = json_object_get(root.get(), isBid ? "bids" : "asks");

  // loop on volume
  *params.logFile << "<Bitstamp> Looking for a limit price to fill "
                  << std::setprecision(6) << fabs(volume) << " BTC...\n";
  double tmpVol = 0.0;
  double p = 0.0;
  double v;
  int i = 0;
  while (tmpVol < fabs(volume) * params.orderBookFactor)
  {
    p = atof(json_string_value(json_array_get(json_array_get(orderbook, i), 0)));
    v = atof(json_string_value(json_array_get(json_array_get(orderbook, i), 1)));
    *params.logFile << "<Bitstamp> order book: "
                    << std::setprecision(6) << v << "@$"
                    << std::setprecision(2) << p << std::endl;
    tmpVol += v;
    i++;
  }
  return p;
}

json_t* authRequest(Parameters &params, std::string request, std::string options)
{
  static uint64_t nonce = time(nullptr) * 4;
  auto msg = std::to_string(++nonce) + params.bitstampClientId + params.bitstampApi;
  uint8_t *digest = HMAC (EVP_sha256(),
                          params.bitstampSecret.c_str(), params.bitstampSecret.size(),
                          reinterpret_cast<const uint8_t *>(msg.data()), msg.size(),
                          nullptr, nullptr);

  std::string postParams = "key=" + params.bitstampApi +
                           "&signature=" + hex_str<upperhex>(digest, digest + SHA256_DIGEST_LENGTH) +
                           "&nonce=" + std::to_string(nonce);
  if (!options.empty())
  {
    postParams += "&";
    postParams += options;
  }

  auto &exchange = queryHandle(params);
  return checkResponse(*params.logFile, exchange.postRequest(request, postParams));
}

bool getOrderbook(Parameters& params, const string& ccy_pair, double& bid_price, double& bid_size, double& ask_price, double& ask_size)
{
  if (ccy_pair.empty())
      return false;
  auto &exchange = queryHandle(params);
  const string& url = "/api/v2/order_book/" + ccy_pair;
  unique_json root { exchange.getRequest(url) };
  json_t *bids = json_object_get(root.get(), "bids");
  json_t *asks = json_object_get(root.get(), "asks");
  if ( !json_is_array(bids) || !json_is_array(asks) )
  {
    cout << "Bitstamp OrderBook Bids or Asks is not array: " << ccy_pair << endl;
    *params.logFile << "Bitstamp OrderBook Bids or Asks is not array: " << ccy_pair << endl;
    return false;
  }  
  // Get the top bid and ask in the order book 
  json_t *bid = json_array_get(bids, 0);
  json_t *ask = json_array_get(asks, 0);
  if ( !json_is_array(bid) || !json_is_array(ask) )
  {
    cout << "Bitstamp OrderBook top Bid or Ask is not array: " << ccy_pair << endl;
    *params.logFile << "Bitstamp OrderBook top Bid or Ask is not array: " << ccy_pair << endl;
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

  //cout << std::setprecision(8) << "Bitstamp  " << url  << " : " << bid_price << " x " << bid_size << " : " << ask_price << " x " << ask_size << endl;
  *params.logFile << std::setprecision(8) << "Bitstamp  " << url  << " : " << bid_price << " x " << bid_size << " : " << ask_price << " x " << ask_size << endl;
  return true;

} 

bool arbitrage(Parameters& params)
{
  static float account = 1000.0;
  float min_balance = 1000.0;
  double cur_balance = getAvail(params, "usd");
/*
  if (cur_balance < min_balance)
      return false;
*/
  for (int i=0; i<5; ++i)
  {
  const string ccy1=ccys[i][0];
  const string ccy2=ccys[i][1];
  const string ccy3=ccys[i][2];
  cout << "ccy1=" << ccy1 << " ccy2=" << ccy2 << " ccy3=" << ccy3 << endl;
  *params.logFile << "ccy1=" << ccy1 << " ccy2=" << ccy2 << " ccy3=" << ccy3 << endl;

  // BTC/USD
  double btcusd_bid_price, btcusd_bid_size, btcusd_ask_price, btcusd_ask_size;
  if (!getOrderbook(params, ccy1, btcusd_bid_price, btcusd_bid_size, btcusd_ask_price, btcusd_ask_size))
      continue;
  // ETH/BTC
  double ethbtc_bid_price, ethbtc_bid_size, ethbtc_ask_price, ethbtc_ask_size;
  if (!getOrderbook(params, ccy2, ethbtc_bid_price, ethbtc_bid_size, ethbtc_ask_price, ethbtc_ask_size))
      continue; 
  // ETH/USD
  double ethusd_bid_price, ethusd_bid_size, ethusd_ask_price, ethusd_ask_size;
  if (!getOrderbook(params, ccy3, ethusd_bid_price, ethusd_bid_size, ethusd_ask_price, ethusd_ask_size))
      continue; 

  float fees = params.bitstampFees;
  double potential_pnl = account / btcusd_ask_price / ethbtc_ask_price * ethusd_bid_price - 3*fees*account - account;
  std::cout << "Potential buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << std::endl;
  *params.logFile << "Potential buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << std::endl;
  // Arbitrage when there is profilt
  if (potential_pnl > account * 0.0001)
  {
    account += potential_pnl;
    std::cout << "Bitstamp Opportunity: buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl  << " Account=" << account << std::endl;
    *params.logFile << "Bitstamp Opportunity: buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << " Account=" << account << std::endl;
    
     if (cur_balance < min_balance)
      return false;
    // Check the maximum fund can be used for arbitrage. Should be less than the top order book, and less than the account balance
    double buy_power  = min(min(min(btcusd_ask_size*btcusd_ask_price, ethbtc_ask_size*ethbtc_ask_price), ethusd_bid_size*ethusd_bid_price), cur_balance); //Only use 90% fund 
    //double buy_size  = min(min(min(btcusd_ask_size, ethbtc_ask_size), ethusd_bid_size), buy_power); 
   
    // Buy BTCUSD order
    double ticksize = 0.01;
    double btcusd_size = trim(min(buy_power / btcusd_ask_price, btcusd_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    double btcusd_price = btcusd_ask_price;
    //double btcusd_price = min((btcusd_bid_price + btcusd_ask_price)/2 + ticksize, btcusd_ask_price - ticksize);
    auto btcusd_id = sendLimitOrder(params, "buy", btcusd_size, btcusd_price, ccy1); 
    sleep_for(millisecs(1000));
    bool is_order_complete = isOrderComplete(params, btcusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Buy " << ccy1 << " : " << btcusd_price << " @ " << btcusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, btcusd_id);
    }
    // Reset the order id
    btcusd_id ="0";
    cout << "Buy " << ccy1 << " Done" << endl;
    *params.logFile << "Buy " << ccy1 << " Done" << endl;
    
    // Buy ETHBTC order
    ticksize = 0.00001;
    //double ethbtc_size = min(btcusd_size / ethbtc_ask_price, btcusd_ask_size); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    //double ethbtc_price = min((ethbtc_bid_price + ethbtc_ask_price)/2 + ticksize, ethbtc_ask_price - ticksize);
    double ethbtc_size = trim(btcusd_size / ethbtc_ask_price);
    double ethbtc_price = ethbtc_ask_price;
    auto ethbtc_id = sendLimitOrder(params, "buy", ethbtc_size, ethbtc_price, ccy2); 
    //auto ethbtc_id = sendLimitOrder(params, "buy", ethbtc_size, ethbtc_price, ccy2); 
    sleep_for(millisecs(1000));
    is_order_complete = isOrderComplete(params, ethbtc_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Buy " << ccy2 << " : " << ethbtc_price << " @ " << ethbtc_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, ethbtc_id);
    }
    // Reset the order id
    ethbtc_id ="0";
    cout << "Buy " << ccy2 << " Done" << endl;
    *params.logFile << "Buy " << ccy2 <<  " Done" << endl;

    // Sell ETHUSD order
    ticksize = 0.01;
    //double ethusd_size = min(ethbtc_size, ethusd_bid_size); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    //double ethusd_price = max((ethusd_bid_price + ethusd_ask_price)/2 - ticksize, ethusd_bid_price + ticksize);
    double ethusd_size = ethbtc_size;
    double ethusd_price = ethusd_bid_price;
    auto ethusd_id = sendLimitOrder(params, "sell", ethusd_size, ethusd_price, ccy3); 
    //auto ethusd_id = sendLimitOrder(params, "sell", ethusd_size, ethusd_price, ccy3); 
    sleep_for(millisecs(1000));
    is_order_complete = isOrderComplete(params, ethusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Sell " << ccy3 << " : " << ethusd_price << " @ " << ethusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, ethusd_id);
    }
    // Reset the order id
    ethusd_id ="0";
    cout << "Sell " << ccy3 << " Done" << endl;
    *params.logFile << "Sell " << ccy3 << " Done" << endl;

    // Check the pnl of this arbitrage
    double new_balance = getAvail(params, "usd");
    double pnl = new_balance - cur_balance;;
    //account += pnl;
    std::cout << "Bitstamp buy " << ccy1 << " -> buy " << ccy2 << " -> sell " << ccy3 << " PNL=" << pnl << " New balance=$" << new_balance << std::endl;
    *params.logFile << "Bitstamp buy " << ccy1 << " -> buy " << ccy2 << " -> sell " << ccy3 << " PNL=" << pnl << " New balance=$" << new_balance << std::endl;
    //*params.logFile << "Bitstamp buy BTC/USD -> buy ETH/BTC -> sell ETH/USD, PNL=" << pnl << " New balance=$" << new_balance << std::endl;
  }
  else
  {
/*
      double pnl = account / ethusd_ask_price * ethbtc_bid_price * btcusd_bid_price - 3*fees*account - account;
      std::cout << "Potential sell " << ccy1  << " -> buy " << ccy3 << " PNL=" << pnl << std::endl;
      if (pnl > account *0.0001)
      {
        account += pnl;
        std::cout << "Bitstamp sell " << ccy1 << " -> buy " << ccy3 << " PNL=" << pnl << " Account=" << account << std::endl;
        *params.logFile << "Bitstamp sell " << ccy1 << " -> buy " << ccy3 << " PNL=" << pnl << " Account=" << account << std::endl;
      }
     
*/}
    potential_pnl = account / ethusd_ask_price * ethbtc_bid_price * btcusd_bid_price - 3*fees*account - account;
    cout << "Potential buy " << ccy3 <<  " -> sell " << ccy1 << " PNL=" << potential_pnl << endl;
    *params.logFile << "Potential buy " << ccy3 <<  " -> sell " << ccy1 << " PNL=" << potential_pnl << endl;
    if (potential_pnl > account * 0.0001)
    {
    account += potential_pnl;
    cout << "Bitstamp Opportunity: buy " << ccy3 << " -> sell " << ccy1 << " PNL=" << potential_pnl << " Account=" << account << endl;
    *params.logFile << "Bitstamp Opportunity: buy " << ccy3 << " -> sell " << ccy1 << " PNL=" << potential_pnl << " Account=" << account << endl;
    double cur_balance = getAvail(params, "EUR");
    if (cur_balance < min_balance)
    {
      cout << "Balance " << cur_balance << " < " << account << endl;
      return false;
    }
    // Check the maximum fund can be used for arbitrage. Should be less than the top order book, and less than the account balance
    double buy_power  = min(min(min(ethusd_ask_size*ethusd_ask_price, ethbtc_bid_size*ethbtc_bid_price), btcusd_bid_size*btcusd_bid_price), cur_balance); //Only use 90% fund 

    // Buy ETH/USD order
    double ticksize = 0.01;
    double ethusd_size = trim(min(buy_power/ethusd_ask_price, ethusd_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
    double ethusd_price = ethusd_ask_price;
    //double ethusd_price = min((ethusd_bid_price + ethusd_ask_price)/2 + ticksize, ethusd_ask_price - ticksize);
    auto ethusd_id = sendLimitOrder(params, "buy", ethusd_size, ethusd_price, ccy3); 
    sleep_for(millisecs(1000));
    bool is_order_complete = isOrderComplete(params, ethusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Buy " << ccy3 << " : " << ethusd_price << " @ " << ethusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, ethusd_id);
    }
    // Reset the order id
    ethusd_id ="0";
    cout << "Buy " << ccy3 << " Done" << endl;
    *params.logFile << "Buy " << ccy3 << " Done" << endl;

    // Sell ETH/BTC order
    ticksize = 0.00001;
    double ethbtc_size = ethusd_size;
    double ethbtc_price = ethbtc_bid_price;
    //double ethbtc_price = min(ethbtc_bid_price+ticksize, ethbtc_ask_price);
    auto ethbtc_id = sendLimitOrder(params, "sell", ethbtc_size, ethbtc_price, ccy2); 
    sleep_for(millisecs(1000));
    is_order_complete = isOrderComplete(params, ethbtc_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Sell " << ccy2 << " : " << ethbtc_price << " @ " << ethbtc_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, ethbtc_id);
    }
    // Reset the order id
    ethbtc_id ="0";
    cout << "Sell " << ccy2 << " Done" << endl;
    *params.logFile << "Sell " << ccy2 << " Done" << endl;
    
    // Sell BTC/USD order
    ticksize = 0.01;
    double btcusd_size = ethbtc_size;
    double btcusd_price = ethusd_bid_price;
    //double btcusd_price = min(ethusd_bid_price+ticksize, ethusd_ask_price);
    auto btcusd_id = sendLimitOrder(params, "sell", btcusd_size, btcusd_price, ccy1); 
    sleep_for(millisecs(1000));
    is_order_complete = isOrderComplete(params, btcusd_id);
    while (!is_order_complete)
    {
      sleep_for(millisecs(1000));
      cout << "Buy " << ccy1 << " : "  << btcusd_price << " @ " << btcusd_size << " still open..." << endl;
      is_order_complete = isOrderComplete(params, btcusd_id);
    }
    // Reset the order id
    btcusd_id ="0";
    cout << "Sell " << ccy1 << " Done" << endl;
    *params.logFile << "Sell " << ccy1 << " Done" << endl;
    

    // Check the pnl of this arbitrage
    double new_balance = getAvail(params, "EUR");
    double pnl = new_balance - cur_balance;;
    account += pnl;
    std::cout << "Bitstamp Buy " << ccy3 << " -> sell " << ccy2 << " -> sell " << ccy1 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
    *params.logFile << "Bitstamp Buy " << ccy3 << " -> sell " << ccy2 << " -> sell " << ccy1 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
  }

}// for loop i
  return false;
}

double trim(double input, unsigned int decimal)
{
  return std::trunc(1e8 * input) / 1e8;

}

}
