#ifndef INDICATOR_H
#define INDICATOR_H

#include <vector>

struct Indicator
{
    Indicator(double bid, double ask, double bid_size, double ask_size, double obi, double ema12, double ema26, double macd, double macd_signal, double macd_hist);

    double bid;
    double ask;
    double bid_size;
    double ask_size;
    double obi;
    double ema12;
    double ema26;
    double macd;
    double macd_signal;
    double macd_hist;
};

class Indicators
{
static const double DECIMAL;
static const double INVALID;
public:
   Indicators();
   ~Indicators();

   int onUpdate(double bid, double ask, double bid_size, double ask_size);

private:
   int checkSignal(); // 1: buy, -1: sell, 0: no signal
   void macd(double price);
   double ema(double price, unsigned int period);
  
   std::vector<Indicator> m_indicators;
   double m_obi;
   double m_ema12;
   double m_ema26;
   double m_macd;
   double m_macd_signal;
   double m_macd_hist;
};

#endif

