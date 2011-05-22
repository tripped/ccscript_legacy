/* exception class */
#pragma once

#include <string>

class Exception
{
protected:
	int num;
	std::string msg;

	void Init(int num, const std::string& msg)
	{
		this->num = num;
		this->msg = msg;
	}

public:
	Exception(int num) 
	{
		Init(num, "");
	}

	Exception(const std::string& msg) 
	{
		Init(-1, msg);
	}

	Exception(int num, const std::string& msg)
	{
		Init(num, msg);
	}

	const std::string& GetMessage()
	{
		return msg;
	}
};

