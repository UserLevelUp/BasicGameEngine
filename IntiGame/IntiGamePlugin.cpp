// IntiGame plugin entry points.
//
// The Inti runners game ships as a separate DLL: BGE is the engine and game
// creation system, not the game itself. The engine discovers this DLL at
// runtime, checks the ABI handshake, and hosts the module through the
// engine-neutral BgeGameModule interface. All Inti game code, content
// loading, and rules live on this side of the boundary.

#include "../BasicGameEngine/include/BgeGameModule.h"

// Defined in IntiGameModule.cpp (plugin-internal accessor).
BgeGameModule& BgeIntiGameModule();

extern "C" __declspec(dllexport) unsigned int BgeGameModuleAbiVersion()
{
    return BGE_GAME_MODULE_ABI_VERSION;
}

extern "C" __declspec(dllexport) BgeGameModule* CreateBgeGameModule()
{
    return &BgeIntiGameModule();
}
