#pragma once

#include <string>
#include <vector>


enum LuaInterfaceType {
    LUAINTERFACE_CLIENT = 0,
    LUAINTERFACE_SERVER = 1,
    LUAINTERFACE_MENU = 2
};

class CLuaShared
{
public:
    virtual void* padding00() = 0;
    virtual void* padding01() = 0;
    virtual void* padding02() = 0;
    virtual void* padding03() = 0;
    virtual void* padding04() = 0;
    virtual void* padding05() = 0;
    virtual void*  GetLuaInterface(LuaInterfaceType type) = 0;
};