#include "../src/offline_launcher.c"
#include <assert.h>
int main(void) {
    BYTE *region = VirtualAlloc(NULL, 0x10000, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    assert(region != NULL);
    DWORD sites[2] = {(DWORD)(uintptr_t)(region+0x29e0),(DWORD)(uintptr_t)(region+0x4d18)};
    BYTE *host = region + 0x29e0, *join = region + 0x4d18;
    host[0]=0x75; host[1]=0x6a; join[0]=0x75; join[1]=0x61;
    assert(!enable_multiplayer_at(GetCurrentProcess(), 0, sites));
    assert(host[0]==0x75 && join[0]==0x75);
    join[1]=0x60;
    DWORD old;
    assert(VirtualProtect(region,0x10000,PAGE_EXECUTE_READ,&old));
    assert(enable_multiplayer_at(GetCurrentProcess(),0,sites));
    assert(host[0]==0xeb && host[1]==0x6a && join[0]==0xeb && join[1]==0x60);
    MEMORY_BASIC_INFORMATION info;
    assert(VirtualQuery(host,&info,sizeof(info)) && info.Protect==PAGE_EXECUTE_READ);
    assert(VirtualQuery(join,&info,sizeof(info)) && info.Protect==PAGE_EXECUTE_READ);
    assert(enable_multiplayer_at(GetCurrentProcess(),0,sites));
    VirtualFree(region,0,MEM_RELEASE);
    puts("PASS: mismatch leaves both sites unchanged; patch verified; page protections restored; repeated application succeeds");
    return 0;
}
