#ifndef PYCALLING_H
#define PYCALLING_H

#ifdef WIN32

#ifdef _DEBUG
#undef _DEBUG
#include<Python.h>
#define _DEBUG
#else
#include<Python.h>
#endif // _DEBUG

#else
#include <python3.7m/Python.h>
#endif // WIN32


#include"platform.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#ifdef _DEBUG
#ifdef WIN32
//path for finding main.py and config.ini
const char * const LOGIC_PATH = R"(C:\\Users\\zhang\\Desktop\\AIchallenge2\\logic\\)";
const char * const DATA_PATH = R"(C:\Users\zhang\Desktop\AIchallenge2\logic\)";
#else
const char *const LOGIC_PATH = R"(/home/sweetnow/AIchallenge2/logic/)";
const char *const DATA_PATH = R"(/home/sweetnow/AIchallenge2/logic/)";
#endif
#else
#ifdef WIN32
//path for finding main.py and config.ini
const char * const LOGIC_PATH = R"(.\\logic\\)";
const char * const DATA_PATH = R"(.\logic\)";
#else
const char *const LOGIC_PATH = R"(./logic/)";
const char *const DATA_PATH = R"(./logic/)";
#endif
#endif // _DEBUG


const char * const MODULE_NAME = "main";
const char * const MAIN_FUNC_NAME = "game_main";
const char * const INIT_FUNC_NAME = "game_init";
const char * const PARACHUTE_FUNC_NAME = "parachute";

#define logic (Pycalling::get_instance())

class Pycalling
{
public:
	static Pycalling &get_instance()
	{
		return _instance;
	}
	~Pycalling();
	Pycalling(const Pycalling &) = delete;
	Pycalling(Pycalling &&) = delete;
	Pycalling &operator=(const Pycalling &) = delete;
	Pycalling &operator=(Pycalling &&) = delete;
	ROUTE_T init(int debug_level = 100, int seed = 0);
	std::pair<std::map<int, std::string>, std::vector<int>> parachute(const std::map<int, COMMAND_PARACHUTE>&m);
	std::pair<std::map<int, std::string>, std::vector<int>> do_loop(const std::map<int, std::vector<COMMAND_ACTION>>& m);


private:
	std::pair<std::map<int, std::string>, std::vector<int>> _parse_dict(PyObject *state);
	bool _check_init(bool print);
	void _traceback(const std::string &err);
	Pycalling();
	bool _is_init = false;
	static Pycalling _instance;

	//PYTHON API
	PyObject *_game_init = nullptr;
	PyObject *_game_main = nullptr;
	PyObject *_parachute = nullptr;
};

#endif //!PYCALLING_H