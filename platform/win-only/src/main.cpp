#include <iostream>
#include "controller.h"
#include "pycalling.h"
#include "item.h"
#include "character.h"
#include "api.h"

std::pair<Position, Position> route;


int main(int argc, char *argv[])
{
    //argv[1] is the number of player_count, argv[2] is the current file.
    int player_count = atoi(argv[1]);
    int turn = 0;
    std::cout << "input the number of players=" << player_count;
    if (player_count > MAX_PLAYER)
    {
        player_count = MAX_PLAYER;
        std::cerr << "The maxinum number of players is " << MAX_PLAYER << ".Will use " << MAX_PLAYER << " instead." << std::endl;
    }
    auto h = LoadLibrary("AI");
    manager.init(player_count);
    for (int i = 0; i < player_count; i++)
    {
        auto play_game = (AI_Func)GetProcAddress(h, "play_game");
        auto bind_api = (void(*)(Parachute_Func))GetProcAddress(h, "bind_api");
        auto receive = (Recv_Func)GetProcAddress(h, "receive");
        bind_api(parachute);
        manager.register_AI(i, play_game, receive);
    }
    route = logic.init();
    manager.run();
    logic.parachute(manager.get_parachute_commands());
    FreeLibrary(h);
    return 0;
}
