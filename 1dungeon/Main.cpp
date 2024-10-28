#include <iostream>

#include "Game.h"
#include <entt/entt.hpp>

int main(int argc, char* argv[])
{
    GameData gameData;
    entt::registry registry;

    bool success = Initialize(&gameData, &registry);
    if (success) {
        RunLoop(&gameData, &registry);
    }
    ShutDown(&gameData);

    return 0;
}