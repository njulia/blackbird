//#include <algorithm>
#include <chrono>
#include "parameters.h"
#include "indicator.h"

using std::cout;
using std::endl;
using std::min;
using std::max;
using millisecs = std::chrono::milliseconds;
using std::this_thread::sleep_for;

const double Indicators::DECIMAL = 1/1e8;
const double Indicators::INVALID = 1e8;

Indicator::Indicator(double bid, double ask, double bid_size, double ask_size, double obi, double ema12, double ema26, double macd, double macd_signal, double macd_hist)
          :bid(bid),
           ask(ask),
           bid_size(bid_size),
           ask_size(ask_size),
           obi(obi),
           ema12(ema12),
           ema26(ema26),
           macd(macd),
           macd_signal(macd_signal),
           macd_hist(macd_hist)
{
}

Indicators::Indicators()
          :m_obi(INVALID),
           m_ema12(INVALID),
           m_ema26(INVALID),
           m_macd(INVALID),
           m_macd_signal(INVALID),
           m_macd_hist(INVALID)
{
}

Indicators::~Indicators()
{
}

int Indicators::onUpdate(double bid, double ask, double bid_size, double ask_size)
{
    // OBI
    if (bid_size > DECIMAL || ask_size > DECIMAL)
    {
        m_obi = (bid_size - ask_size) / bid_size; 
    } 

    // MACD
    if (m_indicators.size() > 12-2)
    {
        double mid = (bid+ask) / 2;
        macd(mid);  
    }
    
    Indicator ind(bid, ask, bid_size, ask_size, m_obi, m_ema12, m_ema26, m_macd, m_macd_signal, m_macd_hist);
    m_indicators.push_back(ind);
    return checkSignal();
}

int Indicators::checkSignal()
{
    std::vector<Indicator>::reverse_iterator rit = m_indicators.rbegin();
    double current_macd_hist = rit->macd_hist;
    double current_macd_signal = rit->macd_signal;
    double current_macd = rit->macd;
    ++rit;
    double last_macd_hist = rit->macd_hist;
    if (current_macd_hist > 0.0 && last_macd_hist < 0.0 && current_macd > -80.0)
    {
        // Buy signal
        std::cout << "Buy signal" << std::endl;
        return 1;
    }
    if (current_macd_hist < 0.0 && last_macd_hist > 0.0 && current_macd < 80.0)
    {
        // Sell signal
        std::cout << "Sell signal" << std::endl;
        return -1; 
    }
    return 0;
}

void Indicators::macd(double price)
{
    m_ema12 = ema(price, 12);
    m_ema26 = ema(price, 26);
    m_macd = m_ema12 - m_ema26;
    m_macd_signal = ema(m_macd, 9);
    m_macd_hist = m_macd - m_macd_signal; 
    std::cout << "EMA12=" << m_ema12 << " EMA26=" << m_ema26 << " MACD=" << m_macd << " MACD_SIGNAL=" << m_macd_signal << " MACD_HIST=" << m_macd_hist << std::endl;
}

double Indicators::ema(double price, unsigned int period)
{
    double ema = INVALID;
    
    std::vector<Indicator>::size_type sz = m_indicators.size();
    double sum = 0.0;
    if (period ==9)
    {
        if (sz < (26-2+period))
           return ema;
        else if (sz == (26-2+period)) 
        {
            for (size_t i=sz-period+1; i<sz; ++i)
            {
               sum += m_indicators[i].macd;
            }
            sum += price;
            ema = sum / period;
            return ema;
        }
    }
    else if (period == 12 || period == 26)
    {
        if (sz < (period - 1))
            return ema;
        else if (sz == (period-1))
        {
            for (size_t i=sz-period+1; i<sz; ++i)
            {
               sum += (m_indicators[i].bid + m_indicators[i].ask)/2;
            }
            sum += price;
            ema = sum / period;
            return ema;
        }
    }
        
    // Get the last indicator;
    auto rit = m_indicators.rbegin();  
   
    switch (period)
    {
        case 12:
            ema = rit->ema12;
            break;
        case 26:
            ema = rit->ema26;
            break;
        case 9:
            ema = rit->macd_signal;
            break;
        default:
            ema = INVALID;
            break;
    }
    double k = 2.0 /(period+1);
    ema = price * k + ema * (1-k);
    return ema;
}

bool triangular(Parameters& params, const array<array<string, 3>, 10>& ccys, const string& base_ccy, getAvailType getAvail, getOrderbookType getOrderbook, sendLimitOrderType sendLimitOrder, isOrderCompleteType isOrderComplete)
{
    float account = 1000.0;
    float min_balance = 600.0;
    double cur_balance = getAvail(params, base_ccy);
/*
    if (cur_balance < min_balance)
        return false;
*/
    for (int i=0; i<ccys.size(); ++i)
    {
        const string ccy1 = ccys[i][0];
        const string ccy2 = ccys[i][1];
        const string ccy3 = ccys[i][2];
        if (ccy1.empty() || ccy2.empty() || ccy3.empty())
            continue;
        cout << "ccy1=" << ccy1 << " ccy2=" << ccy2 << " ccy3=" << ccy3 << endl;
        *params.logFile << "ccy1=" << ccy1 << " ccy2=" << ccy2 << " ccy3=" << ccy3 << endl;

        // BTC-EUR
        double btcusd_bid_price, btcusd_bid_size, btcusd_ask_price, btcusd_ask_size;
        if (!getOrderbook(params, ccy1, btcusd_bid_price, btcusd_bid_size, btcusd_ask_price, btcusd_ask_size))
           continue; 
        // ETH-BTC
        double ethbtc_bid_price, ethbtc_bid_size, ethbtc_ask_price, ethbtc_ask_size;
        if (!getOrderbook(params, ccy2, ethbtc_bid_price, ethbtc_bid_size, ethbtc_ask_price, ethbtc_ask_size))
           continue; 
        // ETH/USD
        double ethusd_bid_price, ethusd_bid_size, ethusd_ask_price, ethusd_ask_size;
        if (!getOrderbook(params, ccy3, ethusd_bid_price, ethusd_bid_size, ethusd_ask_price, ethusd_ask_size))
            continue; 

/*
        cur_balance = 1164.15;
        btcusd_bid_price = 9089.68;
        btcusd_bid_size = 0.00175634;
        btcusd_ask_price = 9099.99;
        btcusd_ask_size = 0.0109000;
        ethbtc_bid_price = 0.05560001;
        ethbtc_bid_size = 0.00558259;
        ethbtc_ask_price = 0.05614791;
        ethbtc_ask_size = 0.36108267;
        ethusd_bid_price = 496.0000;
        ethusd_bid_size = 0.5360000;
        ethusd_ask_price = 501.38000;
        ethusd_ask_size = 0.21293444;
*/
        float fees = params.gdaxFees;
        double potential_pnl = account / btcusd_ask_price / ethbtc_ask_price * ethusd_bid_price - 3*fees*account - account;
        cout << "Potential buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << endl;
        *params.logFile << "Potential buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << endl;
        // Arbitrage when there is profilt
        if (potential_pnl > account * 0.001)
        {
            account += potential_pnl;
            cout << "Opportunity: buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << " Account=" << account << endl;
            *params.logFile << "Opportunity: buy " << ccy1 << " -> sell " << ccy3 << " PNL=" << potential_pnl << " Account=" << account << endl;

            if (cur_balance < min_balance)
                return false;
           
             // Check the maximum fund can be used for arbitrage. Should be less than the top order book, and less than the account balance
             //double buy_power  = min(min(min(btcusd_ask_size*btcusd_ask_price, ethbtc_ask_size*ethbtc_ask_price), ethusd_bid_size*ethusd_bid_price), cur_balance); //Only use 90% fund 
             //double buy_power = min( min(min(cur_balance/btcusd_ask_price, btcusd_ask_size) /ethbtc_ask_price, ethbtc_ask_size), ethusd_bid_size);
             double buy_power = min( cur_balance, min(btcusd_ask_size, min(ethbtc_ask_size, ethusd_bid_size)*ethbtc_ask_price) * btcusd_ask_price);
             cout << "Current balance=" << cur_balance << base_ccy << " Buy_power=" << buy_power <<  endl;
             *params.logFile << "Current balance=" << cur_balance << base_ccy << " Buy_power=" << buy_power <<  endl;
             if (buy_power < 10 || buy_power > cur_balance) 
                 continue;         

             // Buy BTC-EUR order
             double ticksize = 0.01;
             double btcusd_size = trim(min(buy_power / btcusd_ask_price, btcusd_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
             //double btcusd_size = trim(buy_power);
             double btcusd_price = btcusd_ask_price;
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
    
             // Buy ETH-BTC order
             ticksize = 0.00001;
             double ethbtc_size = trim(btcusd_size / ethbtc_ask_price);
             double ethbtc_price = ethbtc_ask_price;
             auto ethbtc_id = sendLimitOrder(params, "buy", ethbtc_size, ethbtc_price, ccy2); 
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
             *params.logFile << "Buy " << ccy2 << " Done" << endl;

             // Sell ETH-EUR order
             ticksize = 0.01;
             double ethusd_size = ethbtc_size;
             double ethusd_price = ethusd_bid_price;
             auto ethusd_id = sendLimitOrder(params, "sell", ethusd_size, ethusd_price, ccy3); 
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
             double new_balance = getAvail(params, base_ccy);
             double pnl = new_balance - cur_balance;;
             account += pnl;
             std::cout << "Buy " << ccy1 << " -> buy " << ccy2 << " -> sell " << ccy3 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
             *params.logFile << "Buy " << ccy1 << " -> buy " << ccy2 << " -> sell " << ccy3 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
         }
         else
         {
             potential_pnl = account / ethusd_ask_price * ethbtc_bid_price * btcusd_bid_price - 3*fees*account - account;
             cout << "Potential buy " << ccy3 <<  " -> sell " << ccy1 << " PNL=" << potential_pnl << endl;
             *params.logFile << "Potential buy " << ccy3 <<  " -> sell " << ccy1 << " PNL=" << potential_pnl << endl;
             if (potential_pnl > account * 0.0001)
             {
                 account += potential_pnl;
                 cout << "Opportunity: buy " << ccy3 << " -> sell " << ccy1 << " PNL=" << potential_pnl << " Account=" << account << endl;
                 *params.logFile << "Opportunity: buy " << ccy3 << " -> sell " << ccy1 << " PNL=" << potential_pnl << " Account=" << account << endl;
                 if (cur_balance < min_balance)
                 {
                     cout << "Balance " << cur_balance << " < " << account << endl;
                     return false;
                 } 
   
                 // Check the maximum fund can be used for arbitrage. Should be less than the top order book, and less than the account balance
                 //double buy_power  = min(min(min(ethusd_ask_size*ethusd_ask_price, ethbtc_bid_size*ethbtc_bid_price), btcusd_bid_size*btcusd_bid_price), cur_balance); //Only use 90% fund 
                 //double buy_power = min( min(min(cur_balance/ethusd_ask_price, ethusd_ask_size), ethbtc_bid_size), btcusd_bid_size); 
                 double buy_power = min( cur_balance, min(ethusd_ask_size, min(ethbtc_bid_size, btcusd_bid_size/ethbtc_bid_price)*ethusd_ask_price));
                 cout << "Current balance=" << cur_balance << base_ccy << " Buy_power=" << buy_power <<  endl;
                 *params.logFile << "Current balance=" << cur_balance << base_ccy << " Buy_power=" << buy_power <<  endl;
                 if (buy_power < 10 || buy_power > cur_balance)
                     continue;

                 // Buy ETH-EUR order
                 double ticksize = 0.01;
                 double ethusd_size = trim(min(buy_power/ethusd_ask_price, ethusd_ask_size)); // Mutipled buy a factor (<1) to buy less than the max fund can be used
                 //double ethusd_size = trim(buy_power);
                 double ethusd_price = ethusd_ask_price;
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

                 // Sell ETH-BTC order
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
    
                 // Sell BTC-EUR order
                 ticksize = 0.01;
                 double btcusd_size = ethbtc_size * ethbtc_price;
                 double btcusd_price = btcusd_bid_price;
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
                 std::cout << "Buy " << ccy3 << " -> sell " << ccy2 << " -> sell " << ccy1 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
                 *params.logFile << "Buy " << ccy3 << " -> sell " << ccy2 << " -> sell " << ccy1 << " PNL=" << pnl << " New balance=e" << new_balance << std::endl;
             }
         }
    }//for loop i
    return false;
}

double trim(double input, unsigned int decimal)
{
  return std::trunc(1e8 * input) / 1e8;

}

