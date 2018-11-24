#ifndef PLATFORM_H
#define PLATFORM_H

//struct, header, constant that platform needs

#include"constant.h"
#include"character.h"
#include<map>
#include<vector>
#include <iostream>
#include <string>
#include <cstdlib>

struct COMMAND_PARACHUTE
{
    VOCATION_TYPE role;
    Position landing_point;
};

enum class COMMAND_TYPE
{
    MOVE = 0,
    SHOOT = 1,
    PICKUP = 2,
    RADIO = 3
};

struct COMMAND_ACTION
{
    COMMAND_TYPE command_type;
    int target_ID;
    double move_angle;
    double view_angle;
    int parameter;
};

//type of "player_receive" in AI.dll 
using Recv_Func = void(*)(bool, const std::string);
//type of "play_game" in AI.dll 
using AI_Func = void(*)();
//type of "player_send" in AI.dll 
using Player_Send_Func = bool(*)(bool, const std::string);

#endif