#include <Rendering/M2ModelPatches.h>

#include <Logger.h>
#include <Util.h>

#include <Windows.h>
#include <d3d9.h>
#include <detours.h>

namespace {
    constexpr uint32_t kSkinMagicSkix = 0x58;

    constexpr uint32_t kLevelWordLoadPatchAddress = 0x0082C7EA;
    constexpr uint32_t kLevelWordLoadReturnAddress = 0x0082C7F0;

    constexpr uint32_t kStartTrianglePatchAddress = 0x008290C0;
    constexpr uint32_t kStartTriangleReturnAddress = 0x008290CB;
    constexpr uint32_t kStartTriangleRemovedLoadAddress = 0x008290D6;
    constexpr uint32_t kStartTriangleRemovedLeaAddress = 0x008290E4;

    constexpr uint32_t kIndexBufferAllocPatchAddress = 0x00828FB6;
    constexpr uint32_t kIndexBufferAllocReturnAddress = 0x00828FBE;
    constexpr uint32_t kIndexBufferAllocSharedPtrAddress = 0x00C5DF88;
    constexpr uint32_t kIndexBufferAllocStackHint = 0x00828FC7;

    constexpr uint32_t kStartVertexPatchAddress = 0x008363BC;
    constexpr uint32_t kStartVertexReturnAddress = 0x008363C7;

    constexpr uint32_t kGlobalVertexIndexPatchAddress = 0x008363F9;
    constexpr uint32_t kGlobalVertexIndexReturnAddress = 0x00836406;

    constexpr uint32_t kWorldFrameHeuristicPatchAddress = 0x0081D521;
    constexpr uint32_t kWorldFrameHeuristicReturnAddress = 0x0081D531;
    constexpr uint32_t kWorldFrameHeuristicAbortAddress = 0x0081D66E;

    constexpr size_t kDirect3D9CreateDeviceVTableIndex = 16;
    constexpr size_t kDirect3D9ExCreateDeviceExVTableIndex = 20;
    constexpr size_t kDirect3DDevice9CreateIndexBufferVTableIndex = 27;

    using Direct3DCreate9_t = IDirect3D9* (WINAPI*)(UINT sdkVersion);
    using Direct3DCreate9Ex_t = HRESULT (WINAPI*)(UINT sdkVersion, IDirect3D9Ex** outD3D);
    using Direct3D9CreateDevice_t = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDeviceInterface);
    using Direct3D9ExCreateDevice_t = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDeviceInterface);
    using Direct3D9ExCreateDeviceEx_t = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, D3DDISPLAYMODEEX* fullscreenDisplayMode, IDirect3DDevice9Ex** returnedDeviceInterface);
    using Direct3DDevice9CreateIndexBuffer_t = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9* self, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle);
    using Direct3DDevice9ExCreateIndexBuffer_t = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9Ex* self, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle);

    Direct3DCreate9_t s_direct3DCreate9 = nullptr;
    Direct3DCreate9Ex_t s_direct3DCreate9Ex = nullptr;
    Direct3D9CreateDevice_t s_direct3D9CreateDevice = nullptr;
    Direct3D9ExCreateDevice_t s_direct3D9ExCreateDevice = nullptr;
    Direct3D9ExCreateDeviceEx_t s_direct3D9ExCreateDeviceEx = nullptr;
    Direct3DDevice9CreateIndexBuffer_t s_direct3DDevice9CreateIndexBuffer = nullptr;
    Direct3DDevice9ExCreateIndexBuffer_t s_direct3DDevice9ExCreateIndexBuffer = nullptr;

    uint32_t s_lastSkinMagicSuffix = 0;
    bool s_modelPatchesApplied = false;
    bool s_d3dExportHooksApplied = false;

    void WriteRelativeJump(uint32_t fromAddress, void* target, size_t overwrittenBytes)
    {
        Util::SetByteAtAddress(reinterpret_cast<void*>(fromAddress), 0xE9);
        Util::OverwriteUInt32AtAddress(
            fromAddress + 1,
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target) - (fromAddress + 5)));

        if (overwrittenBytes > 5) {
            Util::OverwriteBytesAtAddress(reinterpret_cast<void*>(fromAddress + 5), 0x90, overwrittenBytes - 5);
        }
    }

    template <typename T>
    void PatchVTableEntry(void* instance, size_t index, T detour, T& original, char const* label)
    {
        if (instance == nullptr) {
            return;
        }

        void** vtable = *reinterpret_cast<void***>(instance);
        T current = reinterpret_cast<T>(vtable[index]);
        if (current == detour) {
            return;
        }

        if (original == nullptr) {
            original = current;
        }

        DWORD oldProtect = 0;
        VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        vtable[index] = reinterpret_cast<void*>(detour);
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);

        LOG_INFO << "Patched vtable entry for " << label;
    }

    bool ShouldUseSkixIndexFormat()
    {
        if (s_lastSkinMagicSuffix != kSkinMagicSkix) {
            return false;
        }

        void* stackFrames[64] = {};
        const USHORT frameCount = CaptureStackBackTrace(0, 64, stackFrames, nullptr);
        for (USHORT i = 0; i < frameCount; ++i) {
            if (stackFrames[i] == reinterpret_cast<void*>(kIndexBufferAllocStackHint)) {
                return true;
            }
        }

        return false;
    }

    HRESULT STDMETHODCALLTYPE Direct3DDevice9CreateIndexBufferDetour(IDirect3DDevice9* self, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle)
    {
        if (ShouldUseSkixIndexFormat()) {
            format = D3DFMT_INDEX32;
            length *= 2;
        }

        return s_direct3DDevice9CreateIndexBuffer(self, length, usage, format, pool, indexBuffer, sharedHandle);
    }

    HRESULT STDMETHODCALLTYPE Direct3DDevice9ExCreateIndexBufferDetour(IDirect3DDevice9Ex* self, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle)
    {
        if (ShouldUseSkixIndexFormat()) {
            format = D3DFMT_INDEX32;
            length *= 2;
        }

        return s_direct3DDevice9ExCreateIndexBuffer(self, length, usage, format, pool, indexBuffer, sharedHandle);
    }

    void HookDirect3DDevice(IDirect3DDevice9* device)
    {
        PatchVTableEntry(
            device,
            kDirect3DDevice9CreateIndexBufferVTableIndex,
            &Direct3DDevice9CreateIndexBufferDetour,
            s_direct3DDevice9CreateIndexBuffer,
            "IDirect3DDevice9::CreateIndexBuffer");
    }

    void HookDirect3DDevice(IDirect3DDevice9Ex* device)
    {
        PatchVTableEntry(
            device,
            kDirect3DDevice9CreateIndexBufferVTableIndex,
            &Direct3DDevice9ExCreateIndexBufferDetour,
            s_direct3DDevice9ExCreateIndexBuffer,
            "IDirect3DDevice9Ex::CreateIndexBuffer");
    }

    HRESULT STDMETHODCALLTYPE Direct3D9CreateDeviceDetour(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDeviceInterface)
    {
        const HRESULT hr = s_direct3D9CreateDevice(self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDeviceInterface);
        if (SUCCEEDED(hr) && returnedDeviceInterface != nullptr && *returnedDeviceInterface != nullptr) {
            HookDirect3DDevice(*returnedDeviceInterface);
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Direct3D9ExCreateDeviceDetour(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDeviceInterface)
    {
        const HRESULT hr = s_direct3D9ExCreateDevice(self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDeviceInterface);
        if (SUCCEEDED(hr) && returnedDeviceInterface != nullptr && *returnedDeviceInterface != nullptr) {
            HookDirect3DDevice(*returnedDeviceInterface);
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Direct3D9ExCreateDeviceExDetour(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, D3DDISPLAYMODEEX* fullscreenDisplayMode, IDirect3DDevice9Ex** returnedDeviceInterface)
    {
        const HRESULT hr = s_direct3D9ExCreateDeviceEx(self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, fullscreenDisplayMode, returnedDeviceInterface);
        if (SUCCEEDED(hr) && returnedDeviceInterface != nullptr && *returnedDeviceInterface != nullptr) {
            HookDirect3DDevice(*returnedDeviceInterface);
        }
        return hr;
    }

    void HookDirect3D9(IDirect3D9* d3d)
    {
        PatchVTableEntry(
            d3d,
            kDirect3D9CreateDeviceVTableIndex,
            &Direct3D9CreateDeviceDetour,
            s_direct3D9CreateDevice,
            "IDirect3D9::CreateDevice");
    }

    void HookDirect3D9(IDirect3D9Ex* d3d)
    {
        PatchVTableEntry(
            d3d,
            kDirect3D9CreateDeviceVTableIndex,
            &Direct3D9ExCreateDeviceDetour,
            s_direct3D9ExCreateDevice,
            "IDirect3D9Ex::CreateDevice");
        PatchVTableEntry(
            d3d,
            kDirect3D9ExCreateDeviceExVTableIndex,
            &Direct3D9ExCreateDeviceExDetour,
            s_direct3D9ExCreateDeviceEx,
            "IDirect3D9Ex::CreateDeviceEx");
    }

    IDirect3D9* WINAPI Direct3DCreate9Detour(UINT sdkVersion)
    {
        IDirect3D9* d3d = s_direct3DCreate9(sdkVersion);
        HookDirect3D9(d3d);
        return d3d;
    }

    HRESULT WINAPI Direct3DCreate9ExDetour(UINT sdkVersion, IDirect3D9Ex** outD3D)
    {
        const HRESULT hr = s_direct3DCreate9Ex(sdkVersion, outD3D);
        if (SUCCEEDED(hr) && outD3D != nullptr && *outD3D != nullptr) {
            HookDirect3D9(*outD3D);
        }
        return hr;
    }

    void ApplyDirect3DHooks()
    {
        if (s_d3dExportHooksApplied) {
            return;
        }

        HMODULE d3d9Module = LoadLibraryW(L"d3d9.dll");
        if (d3d9Module == nullptr) {
            LOG_WARN << "Failed to load d3d9.dll for SKIX index buffer hooks";
            return;
        }

        s_direct3DCreate9 = reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(d3d9Module, "Direct3DCreate9"));
        s_direct3DCreate9Ex = reinterpret_cast<Direct3DCreate9Ex_t>(GetProcAddress(d3d9Module, "Direct3DCreate9Ex"));

        if (s_direct3DCreate9 == nullptr && s_direct3DCreate9Ex == nullptr) {
            LOG_WARN << "Could not resolve Direct3DCreate9 or Direct3DCreate9Ex";
            return;
        }

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        if (s_direct3DCreate9 != nullptr) {
            DetourAttach(reinterpret_cast<void**>(&s_direct3DCreate9), reinterpret_cast<void*>(&Direct3DCreate9Detour));
        }

        if (s_direct3DCreate9Ex != nullptr) {
            DetourAttach(reinterpret_cast<void**>(&s_direct3DCreate9Ex), reinterpret_cast<void*>(&Direct3DCreate9ExDetour));
        }

        const LONG status = DetourTransactionCommit();
        if (status != NO_ERROR) {
            LOG_ERROR << "Failed to install Direct3D export hooks: " << status;
            return;
        }

        s_d3dExportHooksApplied = true;
        LOG_INFO << "Installed Direct3D export hooks for SKIX index buffers";
    }

    __declspec(naked) void LevelWordLoadDetour()
    {
        __asm {
            mov eax, [ebp - 4]
            movzx eax, word ptr [eax + edx]
            push 082C7F0h
            ret
        }
    }

    __declspec(naked) void StartTriangleOffsetDetour()
    {
        __asm {
            movzx edx, byte ptr [esi + 2]
            shl edx, 16
            mov dx, cx
            mov ecx, edx

            movzx edx, byte ptr [edi + 3]
            cmp edx, 58h
            je skix_path

        skin_path:
            mov edx, [edi + 10h]
            add eax, eax
            push eax
            lea eax, [edx + ecx * 2]
            push eax
            push ebx
            movzx edx, word ptr [esi + 0Ah]
            lea ebx, [ebx + edx * 2]
            push 08290CBh
            ret

        skix_path:
            imul eax, 4
            push eax
            mov edx, [edi + 10h]
            lea eax, [edx + ecx * 4]
            push eax
            push ebx
            movzx edx, word ptr [esi + 0Ah]
            lea ebx, [ebx + edx * 4]
            push 08290CBh
            ret
        }
    }

    __declspec(naked) void IndexBufferAllocSizeDetour()
    {
        __asm {
            mov ecx, [esi + 2Ch]
            mov ecx, [ecx + 170h]
            movzx ecx, byte ptr [ecx + 3]
            mov dword ptr [s_lastSkinMagicSuffix], ecx
            cmp ecx, 58h
            je skix_path

        skin_path:
            mov ecx, dword ptr ds:[0C5DF88h]
            mov edx, [ecx]
            push 0828FBEh
            ret

        skix_path:
            mov ecx, [eax + 14h]
            imul ecx, 2
            mov [eax + 14h], ecx
            mov ecx, dword ptr ds:[0C5DF88h]
            mov edx, [ecx]
            push 0828FBEh
            ret
        }
    }

    __declspec(naked) void StartVertexOffsetDetour()
    {
        __asm {
            movzx edx, word ptr [eax + 4]
            movzx ecx, byte ptr [eax + 3]
            shl ecx, 16
            mov cx, dx
            mov edx, ecx
            movzx ecx, word ptr [eax + 0Ch]
            imul ecx, esi
            push 08363C7h
            ret
        }
    }

    __declspec(naked) void GlobalVertexIndexLoadDetour()
    {
        __asm {
            mov esi, dword ptr [ebx + 170h]
            movzx esi, byte ptr [esi + 3]
            cmp esi, 58h
            je skix_path

        skin_path:
            movzx esi, word ptr [ecx + edx * 2]
            jmp loaded_index

        skix_path:
            mov esi, dword ptr [ecx + edx * 4]

        loaded_index:
            mov ecx, esi
            lea esi, [ecx + ecx * 2]
            mov ecx, [ebx + 150h]
            push 0836406h
            ret
        }
    }

    __declspec(naked) void WorldFrameHeuristicDetour()
    {
        __asm {
            movzx eax, word ptr [edi]
            cmp [ebp + 10h], eax
            jg abort_path

            movzx eax, word ptr [edi + 2]
            cmp [ebp + 10h], eax
            jg abort_path

            movzx eax, word ptr [edi + 4]
            cmp [ebp + 10h], eax
            jg abort_path

            fldz
            push ebx
            mov ebx, [ebp + 10h]
            push esi
            mov esi, [ecx + 124h]
            mov [ebp + 8], esi
            push 081D531h
            ret

        abort_path:
            push 081D66Eh
            ret
        }
    }

    void ApplyModelCodePatches()
    {
        if (s_modelPatchesApplied) {
            return;
        }

        Util::OverwriteBytesAtAddress(reinterpret_cast<void*>(kStartTriangleRemovedLoadAddress), 0x90, 4);
        Util::OverwriteBytesAtAddress(reinterpret_cast<void*>(kStartTriangleRemovedLeaAddress), 0x90, 3);

        WriteRelativeJump(
            kLevelWordLoadPatchAddress,
            reinterpret_cast<void*>(&LevelWordLoadDetour),
            kLevelWordLoadReturnAddress - kLevelWordLoadPatchAddress);
        WriteRelativeJump(
            kStartTrianglePatchAddress,
            reinterpret_cast<void*>(&StartTriangleOffsetDetour),
            kStartTriangleReturnAddress - kStartTrianglePatchAddress);
        WriteRelativeJump(
            kIndexBufferAllocPatchAddress,
            reinterpret_cast<void*>(&IndexBufferAllocSizeDetour),
            kIndexBufferAllocReturnAddress - kIndexBufferAllocPatchAddress);
        WriteRelativeJump(
            kStartVertexPatchAddress,
            reinterpret_cast<void*>(&StartVertexOffsetDetour),
            kStartVertexReturnAddress - kStartVertexPatchAddress);
        WriteRelativeJump(
            kGlobalVertexIndexPatchAddress,
            reinterpret_cast<void*>(&GlobalVertexIndexLoadDetour),
            kGlobalVertexIndexReturnAddress - kGlobalVertexIndexPatchAddress);
        WriteRelativeJump(
            kWorldFrameHeuristicPatchAddress,
            reinterpret_cast<void*>(&WorldFrameHeuristicDetour),
            kWorldFrameHeuristicReturnAddress - kWorldFrameHeuristicPatchAddress);

        s_modelPatchesApplied = true;
        LOG_INFO << "Applied SKIX model parsing patches";
    }
}

void M2ModelPatches::Apply()
{
    ApplyModelCodePatches();
    ApplyDirect3DHooks();
}
