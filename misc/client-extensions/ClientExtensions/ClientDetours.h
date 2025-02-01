#pragma once

#include "ClientMacros.h"

#include <vector>
#include <string>

class Main;

class ClientDetours {
public:
    struct Detour {
        std::string const m_name;
        void * const m_oldFn;
        void * const m_newFn;
        std::string const m_filename;
        size_t m_lineno;
    };
    static int Add(std::string const& name, void* clientFun, void* yourFun, std::string const& filename, size_t lineno);
private:
    static void Apply();
    ClientDetours() {};
    friend class Main;
};

// do NOT refactor this name
// without also changing the name in client_header_builder.cpp

#define CLIENT_DETOUR(__detour_name,addr,calltype,retval,...) \
    typedef retval (calltype *__detour_name##Type)__VA_ARGS__;\
    inline __detour_name##Type __detour_name = (__detour_name##Type)(addr);\
    retval __detour_name##Detour __VA_ARGS__; \
    int __detour_name##__Result = ClientDetours::Add(#__detour_name,&__detour_name,__detour_name##Detour,__FILE__,__LINE__);\
    retval __detour_name##Detour __VA_ARGS__
