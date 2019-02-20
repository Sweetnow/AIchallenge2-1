#include "player.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

extern XYPosition start_pos, over_pos;
extern std::vector<int> teammates;
extern int frame;
extern PlayerInfo info;

void play_game()
{
	srand(time(nullptr));
	int delay = rand() % 10000;
	std::cout << "playeraaaa:frame" << frame << "\nhp:" << info.self.hp << std::endl;
	VOCATION role = VOCATION::ENGINEER;
	XYPosition landing_point = { 5,5 };
	if (frame == 0)
		parachute(role, landing_point);
	else
		move(12, 23, 0);
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
	return;
}