#include "controller.h"

extern std::ofstream mylog;
Controller Controller::_instance;

void notify_child_finish(int, siginfo_t *info, void *)
{
    pid_t sender = info->si_pid;
    manager.notify_one_finish(sender);
}

Controller::~Controller()
{
    if (!_check_init())
        return;
    for (int i = 0; i < _player_count; ++i)
    {
        _kill_one(i);
    }
}

bool Controller::_check_init()
{
    if (!_is_init)
    {
        mylog << "Manager is not initialised,please check codes." << std::endl;
    }
    return _is_init;
}

void Controller::init(const std::filesystem::path &path, long used_core_count)
{
    using namespace std::filesystem;
    auto PAT = std::regex(R"((libAI_(\d+)_(\d+)).so)", std::regex_constants::ECMAScript | std::regex_constants::icase);
    std::smatch m;
    int player_count = 0;

    bool isfirst = true;

    for (const auto &entry : directory_iterator(path))
    {
        if (is_regular_file(entry))
        {
            auto name = entry.path().filename().string();
            if (std::regex_match(name, m, PAT) && m.size() == 4)
            {
                int team = atoi(m[2].str().c_str());
                int number = atoi(m[3].str().c_str());
                if (0 <= team && team <= 15 && 0 <= number && number <= 3)
                {
                    _info[player_count].team = team;
                    std::string fullpath = entry.path();
                    mylog << "try to load " << fullpath << std::endl;
                    _info[player_count].lib = dlopen(fullpath.c_str(), RTLD_NOW);
                    if (_info[player_count].lib == NULL)
                    {
                        mylog << "LoadLibrary " + fullpath + " error" << std::endl;
                        continue;
                    }
                    else
                    {
                        auto bind_api = (void (*)(Player_Send_Func, Player_Update))dlsym(_info[player_count].lib, "bind_api");
                        _info[player_count].player_func = (AI_Func)dlsym(_info[player_count].lib, "play_game");
                        _info[player_count].recv_func = (Recv_Func)dlsym(_info[player_count].lib, "player_receive");
                        if (bind_api == NULL || _info[player_count].player_func == NULL || _info[player_count].recv_func == NULL)
                        {
                            mylog << "Cannot Get AI API from " << std::endl;
                            _info[player_count].state = AI_STATE::DEAD;
                        }
                        else
                        {
                            (*bind_api)(&controller_receive, &controller_update);
                            _team[team].push_back(player_count);
                            mylog << "Load AI " << fullpath << " as team" << team << std::endl;
                            if (isfirst)
                            {
                                isfirst = false;
                            }
                            else
                            {
                            }
                        }
                        ++player_count;
                    }
                }
                else
                {
                    mylog << "Wrong filename:" << m[0] << std::endl;
                }
            }
        }
    }
    _player_count = player_count;
    _used_core_count = used_core_count;
    //set CPU
    _total_core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (used_core_count == 0 || used_core_count > _total_core_count)
    {
        _used_core_count = _total_core_count;
    }
    else
    {
        _used_core_count = used_core_count;
    }
    mylog << "total core = " << _total_core_count << " used core = " << _used_core_count << std::endl;
    for (int i = 0; i < _player_count; i++)
    {
        _info[i].cpuID = i % _used_core_count + (_total_core_count - _used_core_count);
    }
    for(int i = 0; i < _used_core_count; i++)
    {
        _cpu_batchs.emplace_back(std::vector<int>());
    }
    _is_init = true;
}

Controller::Controller()
{
    //set SIGUSR1
    struct sigaction act;
    act.sa_sigaction = notify_child_finish;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    sigaction(SIGUSR1, &act, nullptr);
}

void Controller::_kill_one(int playerID)
{
    if (_info[playerID].state != AI_STATE::DEAD)
    {
        mylog << playerID << std::endl;
        kill(_info[playerID].pid, SIGKILL);
        shmdt(_info[playerID].shm);
        shmctl(_info[playerID].shmid, IPC_RMID, nullptr);
        dlclose(_info[playerID].lib);
        _info[playerID].state = AI_STATE::DEAD;
        mylog << "kill player: " << playerID << std::endl;
    }
}

bool Controller::has_living_player()
{
    for (int i = 0; i < _player_count; i++)
    {
        if (_info[i].state != AI_STATE::DEAD)
        {
            return true;
        }
    }
    return false;
}

void Controller::run()
{
    if (!_check_init())
        return;
    mylog << "\nrun frame: " << _frame << std::endl;
    for (int playerID : dead)
    {
        _kill_one(playerID);
    }
    //clear something
    for (int i = 0; i < _player_count; ++i)
    {
        _command_parachute[i].clear();
        _command_action[i].clear();
    }
    for (int i = 0; i < _used_core_count; ++i)
    {
        _cpu_batchs[i].clear();
    }
    for (int i = _player_count-1; i >= 0; --i)
    {
        if(_info[i].state!=AI_STATE::DEAD)
        {
            _cpu_batchs[_info[i].cpuID].push_back(i);
        }
    }
    //main_loop
    while (true)
    {
        //set batch
        _batch.clear();
        for(int i = 0; i < _used_core_count; ++i)
        {
            if(!_cpu_batchs[i].empty())
            {
                _batch.push_back(_cpu_batchs[i].back());
                _cpu_batchs[i].pop_back();  
            }
        }
        if(_batch.empty())
            break;
        mylog << "batch: ";
        for (const auto i : _batch)
        {
            mylog << i << ' ';
        }
        mylog << std::endl;
        //execute some players each loop. The number is equal to the number of core(_used_core_count)
        for (int i : _batch)
        {
            switch (_info[i].state)
            {
            case AI_STATE::UNUSED: //only first time
                _used_cpuID = _info[i].cpuID;
                _info[i].shmid = shmget(IPC_PRIVATE, sizeof(COMM_BLOCK), IPC_CREAT | 0600);
                _info[i].pid = fork();
                if (_info[i].pid > 0) //manager
                {
                    _info[i].shm = reinterpret_cast<COMM_BLOCK *>(shmat(_info[i].shmid, nullptr, 0));
                    _info[i].shm->init();
                    _send_to_client(i, _serialize_route(i));
                }
                else if (_info[i].pid == 0) //player AI
                {
                    _playerID = i;
                    _info[i].shm = reinterpret_cast<COMM_BLOCK *>(shmat(_info[i].shmid, nullptr, 0));
                    _run_player();
                    return;
                }
                else //error
                {
                    mylog << "player: " << i << " Controller error: fork error" << std::endl;
                    exit(1);
                }
                break;
            case AI_STATE::SUSPENDED:
                _send_to_client(i, _serialize_infos(i));
                break;
            default:
                mylog << "player: " << i << " process state error";
                mylog << (int)_info[i].state << std::endl;
                exit(1);
                break;
            }
        }
        //real start
        for (int i : _batch)
        {
            switch (_info[i].state)
            {
            case AI_STATE::UNUSED: //only first time
                _info[i].shm->set_inited();
                _info[i].state = AI_STATE::ACTIVE;
                break;
            case AI_STATE::SUSPENDED:
                kill(_info[i].pid, SIGCONT);
                _info[i].state = AI_STATE::ACTIVE;
                break;
            default:
                mylog << "player: " << i << " process state error(2)";
                mylog << (int)_info[i].state << std::endl;
                exit(1);
                break;
            }
        }
        int timeout = TIMEOUT;
        if (_frame == 0)
        {
            timeout = START_TIMEOUT;
        }
        //timeout
        bool all_finish = true;
        timeval start, now;
        gettimeofday(&start, nullptr);
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL));
            gettimeofday(&now, nullptr);
            if (now.tv_sec > start.tv_sec || now.tv_usec - start.tv_usec > 1000 * timeout)
            {
                all_finish = false;
                break;
            }
            all_finish = true;
            for (int i : _batch)
            {
                if (_info[i].state == AI_STATE::ACTIVE)
                {
                    all_finish = false;
                    break;
                }
            }
        } while (!all_finish);
        if (!all_finish)
        {
            for (int i : _batch)
            {
                if (_info[i].state == AI_STATE::ACTIVE)
                {
                    //get all locks before stopping clients, avoid deadlocks(if the client has locked it but stopped)
                    _info[i].shm->lock_commands();
                    _info[i].shm->lock_infos();
                }
            }
            for (int i : _batch)
            {
                if (_info[i].state == AI_STATE::ACTIVE)
                {
                    kill(_info[i].pid, SIGSTOP);
                    _info[i].state = AI_STATE::SUSPENDED;
                    _info[i].shm->unlock_infos();
                    _info[i].shm->unlock_commands();
                }
            }
        }
        for (int i : _batch)
        {
            _info[i].shm->lock_commands();
            _receive_from_client(i);
            _info[i].shm->clear_commands();
            _info[i].shm->unlock_commands();
        }
    }
    //kill AI who sended nothing when parachuting
    if (_frame == 0)
    {
        for (int i = 0; i < _player_count; ++i)
        {
            if (_command_parachute[i].empty())
            {
                _kill_one(i);
                mylog << "player: " << i << " is killed because of sending nothing when parachuting" << std::endl;
                COMMAND_PARACHUTE c;
                c.role = -1;
                c.team = _info[i].team;
                c.landing_point = {0, 0};
                _command_parachute[i].push_back(c);
            }
        }
    }
    ++_frame;
}
void Controller::_run_player()
{
    if (!_check_init())
        return;
    //set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(_used_cpuID, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    while (!_info[_playerID].shm->is_init)
        ;
    //player AI
    while (true)
    {
        _info[_playerID].shm->lock_infos();
        _receive_from_server();
        _info[_playerID].shm->unlock_infos();
        if (_playerID >= 0 && _info[_playerID].player_func != nullptr)
        {
            (*_info[_playerID].player_func)();
        }
        kill(getppid(), SIGUSR1);
        raise(SIGSTOP);
    }
}
void Controller::notify_one_finish(pid_t pid)
{
    if (!_check_init())
        return;
    for (int i : _batch)
    {
        if (_info[i].pid == pid)
        {
            _info[i].state = AI_STATE::SUSPENDED;
            return;
        }
    }
    for (int i = 0; i < _player_count; ++i)
    {
        if (_info[i].pid == pid)
        {
            _info[i].state = AI_STATE::SUSPENDED;
            return;
        }
    }
}

bool Controller::send_to_server(const std::string &data)
{
    if (!_check_init())
        return false;
    //send data to server
    _info[_playerID].shm->lock_commands();
    if (!_info[_playerID].shm->add_command(data))
    {
        mylog << "player " << _playerID << " send too many commands" << std::endl;
    }
    _info[_playerID].shm->unlock_commands();
    return true;
}

void Controller::_receive_from_client(int playerID)
{
    if (!_check_init())
        return;
    auto all = _info[playerID].shm->get_commands();
    for (auto &s : all)
    {
        _parse(s, playerID);
    }
}

void Controller::_send_to_client(int playerID, const std::string &data)
{
    if (!_check_init())
        return;
    //send data to client
    _info[playerID].shm->lock_infos();
    _info[playerID].shm->set_infos(data);
    _info[playerID].shm->frame = _frame;
    _info[playerID].shm->unlock_infos();
    return;
}

void Controller::_receive_from_server()
{
    if (_info[_playerID].frame != _info[_playerID].shm->frame)
    {
        _info[_playerID].frame = _info[_playerID].shm->frame;
        (*_info[_playerID].recv_func)(_info[_playerID].frame, _info[_playerID].shm->get_infos());
    }
}

bool Controller::_parse(const std::string &data, int playerID)
{
    comm::Command recv;
    if (playerID >= 0 && recv.ParseFromString(data))
    {
        if (recv.command_type() != comm::CommandType::PARACHUTE)
        {
            COMMAND_ACTION c;
            switch (recv.command_type())
            {
            case comm::CommandType::MOVE:
                c.command_type = COMMAND_TYPE::MOVE;
                break;
            case comm::CommandType::SHOOT:
                c.command_type = COMMAND_TYPE::SHOOT;
                break;
            case comm::CommandType::PICKUP:
                c.command_type = COMMAND_TYPE::PICKUP;
                break;
            case comm::CommandType::RADIO:
                c.command_type = COMMAND_TYPE::RADIO;
                break;
            default:
                break;
            }
            c.move_angle = recv.move_angle();
            c.view_angle = recv.view_angle();
            c.target_ID = recv.target_id();
            c.parameter = recv.parameter();

            _command_action[playerID].push_back(c);
            return true;
        }
        else
        {
            COMMAND_PARACHUTE c;
            c.landing_point.x = recv.landing_point().x();
            c.landing_point.y = recv.landing_point().y();
            c.role = recv.role();
            c.team = _info[playerID].team;
            _command_parachute[playerID].push_back(c);
            return true;
        }
    }
    else
    {
        return false;
    }
}

std::string Controller::_serialize_route(int playerID)
{
    comm::Route sender;
    sender.mutable_start_pos()->set_x(route.first.x);
    sender.mutable_start_pos()->set_y(route.first.y);
    sender.mutable_over_pos()->set_x(route.second.x);
    sender.mutable_over_pos()->set_y(route.second.y);
    for (auto teammate : _team[_info[playerID].team])
    {
        sender.add_teammates(teammate);
    }
    return sender.SerializeAsString();
}

std::string Controller::_serialize_infos(int playerID)
{
    return player_infos[playerID];
}

std::map<int, COMMAND_PARACHUTE> Controller::get_parachute_commands()
{
    std::map<int, COMMAND_PARACHUTE> m;
    if (!_check_init())
        return m;
    for (int i = 0; i < _player_count; i++)
    {
        if (!_command_parachute[i].empty())
        {
            m[i] = _command_parachute[i].back();
        }
    }
    return m;
}

std::map<int, std::vector<COMMAND_ACTION>> Controller::get_action_commands()
{
    std::map<int, std::vector<COMMAND_ACTION>> m;
    if (!_check_init())
        return m;
    for (int i = 0; i < _player_count; ++i)
    {
        if (!_command_action[i].empty() && _info[i].state != AI_STATE::DEAD)
        {
            m[i] = {_command_action[i].cbegin(), _command_action[i].cend()};
        }
    }
    return m;
}

bool controller_receive(const std::string data)
{
    return manager.send_to_server(data);
}

void controller_update(int)
{
    manager._info[manager._playerID].shm->lock_infos();
    manager._receive_from_server();
    manager._info[manager._playerID].shm->unlock_infos();
}