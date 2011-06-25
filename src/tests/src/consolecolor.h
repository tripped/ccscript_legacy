///////////////////////////////////////////////////////////////////////////////
// Console text output coloring. Woo!

#pragma once

#include <iostream>

namespace color
{

#ifdef WIN32

// Windows console stuff
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

static void SetColor(int clr) {
	SetConsoleTextAttribute(hConsole, clr);
}
static int GetConsoleTextAttribute() {
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(hConsole, &info);
	return info.wAttributes;
}
const int CLR_NORMAL = GetConsoleTextAttribute();
const int CLR_RED = (FOREGROUND_RED | FOREGROUND_INTENSITY);
const int CLR_GREEN = (FOREGROUND_GREEN | FOREGROUND_INTENSITY);
const int CLR_BLUE = (FOREGROUND_BLUE | FOREGROUND_INTENSITY);

const int CLR_DARKRED = (FOREGROUND_RED);
const int CLR_DARKGREEN = (FOREGROUND_GREEN);
const int CLR_DARKBLUE = (FOREGROUND_BLUE);

const int CLR_MAGENTA = (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
const int CLR_YELLOW = (FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
const int CLR_CYAN = (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

const int CLR_DARKMAGENTA = (FOREGROUND_RED | FOREGROUND_BLUE);
const int CLR_DARKYELLOW = (FOREGROUND_GREEN | FOREGROUND_RED);
const int CLR_DARKCYAN = (FOREGROUND_GREEN | FOREGROUND_BLUE);

const int CLR_BLACK = 0;
const int CLR_DARKGRAY = (FOREGROUND_INTENSITY);
const int CLR_GRAY = (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
const int CLR_WHITE = (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

#else
// Linux console stuff here
const int CLR_NORMAL = 0;
const int CLR_RED = 1;
const int CLR_GREEN = 2;
const int CLR_BLUE = 3;
const int CLR_DARKRED = 4;
const int CLR_DARKGREEN = 5;
const int CLR_DARKBLUE = 6;
const int CLR_MAGENTA = 7;
const int CLR_YELLOW = 8;
const int CLR_CYAN = 9;
const int CLR_DARKMAGENTA = 10;
const int CLR_DARKYELLOW = 11;
const int CLR_DARKCYAN = 12;
const int CLR_BLACK = 13;
const int CLR_DARKGRAY = 14;
const int CLR_GRAY = 15;
const int CLR_WHITE = 16;

static void SetColor(int clr) {
	// TODO: do this using ncurses instead of just outputting terminal codes
	switch(clr) {
		case CLR_RED:
			std::cout << "\033[01;31m";	break;
		case CLR_GREEN:
			std::cout << "\033[01;32m"; break;
		case CLR_BLUE:
			std::cout << "\033[01;34m";	break;
		case CLR_DARKRED:
			std::cout << "\033[22;31m";	break;
		case CLR_DARKGREEN:
			std::cout << "\033[22;32m";	break;
		case CLR_DARKBLUE:
			std::cout << "\033[22;34m";	break;
		case CLR_MAGENTA:
			std::cout << "\033[01;35m";	break;
		case CLR_YELLOW:
			std::cout << "\033[01;33m";	break;
		case CLR_CYAN:
			std::cout << "\033[01;36m";	break;
		case CLR_DARKMAGENTA:
			std::cout << "\033[22;35m";	break;
		case CLR_DARKYELLOW:
			std::cout << "\033[22;33m";	break;
		case CLR_DARKCYAN:
			std::cout << "\033[22;36m";	break;
		case CLR_BLACK:
			std::cout << "\033[22;30m";	break;
		case CLR_DARKGRAY:
			std::cout << "\033[01;30m";	break;
		case CLR_GRAY:
			std::cout << "\033[22;37m";	break;
		case CLR_WHITE:
			std::cout << "\033[01;37m";	break;
		case CLR_NORMAL:
			std::cout << "\033[0m";	break;
		default:
			std::cout << "\033[01;39m";	break;
	}
}


#endif

	//
	// Allow the insertion of color options into std:: streams
	//
	class color_setter {
	public:
		color_setter(int clr) : clr(clr) { }
		void set() const { SetColor(clr); }
	private:
		int clr;
	};

#ifdef __GNUC__
	// For some reason, g++ thinks this is not used, even when it is. Hrm.
	__attribute__ ((__used__))
#endif
	static std::ostream& operator<<(std::ostream& stream, const color_setter& clr)
	{
		clr.set();
		return stream;
	}


	const color_setter normal(CLR_NORMAL);

	const color_setter red(CLR_RED);
	const color_setter green(CLR_GREEN);
	const color_setter blue(CLR_BLUE);

	const color_setter darkred(CLR_DARKRED);
	const color_setter darkgreen(CLR_DARKGREEN);
	const color_setter darkblue(CLR_DARKBLUE);

	const color_setter cyan(CLR_CYAN);
	const color_setter magenta(CLR_MAGENTA);
	const color_setter yellow(CLR_YELLOW);

	const color_setter darkcyan(CLR_DARKCYAN);
	const color_setter darkmagenta(CLR_DARKMAGENTA);
	const color_setter darkyellow(CLR_DARKYELLOW);

	const color_setter black(CLR_BLACK);
	const color_setter darkgray(CLR_DARKGRAY);
	const color_setter gray(CLR_GRAY);
	const color_setter white(CLR_WHITE);

	// TODO: Add background color support


}


