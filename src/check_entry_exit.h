#ifndef CHECK_ENTRY_EXIT_H
#define CHECK_ENTRY_EXIT_H

#include <string>
#include <ctime>


class  Bitcoin;
struct Result;
struct Parameters;


static const double TRAILING_MAX = 100.0;
static const double TRAILING_MIN = -100.0;

bool euqal(double a, double b);
std::string percToStr(double perc);

// Checks for entry opportunity between two exchanges
// and returns True if an opporunity is found.
bool checkEntry(Bitcoin* btcLong, Bitcoin* btcShort, Result& res, Parameters& params);

// Checks for exit opportunity between two exchanges
// and returns True if an opporunity is found.
bool checkExit(Bitcoin* btcLong, Bitcoin* btcShort, Result& res, Parameters& params, std::time_t period);

#endif

