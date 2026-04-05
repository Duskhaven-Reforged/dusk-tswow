#include <ClientDetours.h>
#include <ClientLua.h>
#include <FSRoot.h>
#include <Logger.h>
#include <SharedDefines.h>
#include <World/WMOLogging.h>

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

CLIENT_DETOUR(CMap__SafeOpen, 0x007BD480, __cdecl, int, (char *Src, HANDLE* a2)) {
    for (int i = 0; i < 10; i++) {
        if (SFile::OpenFile(Src, a2)) {
            if (a2 && *a2) {
                WMOLogging::RecordMapAssetOpen(Src, *a2);
            }
            return 1;
        }
    }
    WMOLogging::RecordMapAssetMissing(Src);
    LOG_DEBUG << "BAD FILE READ, NO FIND " << Src;
    return SFile::OpenFile("Spells\\ErrorCube.mdx",a2);
}
