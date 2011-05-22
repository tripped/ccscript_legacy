/* error receiver interface */
#pragma once

#include <string>

class ErrorReceiver
{
public:
	virtual void Error(const std::string& msg, int line, int col)=0;
	virtual void Warning(const std::string& msg, int line, int col)=0;
	virtual ~ErrorReceiver() {}
};

