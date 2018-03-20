#include <vector>
#include "indicator.h"

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
