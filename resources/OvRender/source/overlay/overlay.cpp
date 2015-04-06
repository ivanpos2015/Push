#include <windows.h>
#include <OvRender.h>
#include "dx8overlay.h"
#include "dx9overlay.h"
#include "dxgioverlay.h"


Dx8Overlay*     OvDx8Overlay;
Dx9Overlay*     D3D9Overlay;
DxgiOverlay*    OvDxgiOverlay;


OvOverlay::OvOverlay()
{
    Line = 0;
    VsyncOverrideMode = VSYNC_UNCHANGED;
}


VOID
OvOverlay::Render()
{
    Line = 0;

    Begin();
    UserRenderFunction( this );
    End();
}


ULONG __stdcall CreateOverlay( LPVOID Param )
{
    OV_HOOK_PARAMS *hookParams = (OV_HOOK_PARAMS*) Param;
    OvOverlay *overlay = NULL;

    if (GetModuleHandleA("d3d8.dll") && OvDx8Overlay == NULL)
    {
        OvDx8Overlay = new Dx8Overlay( hookParams->RenderFunction );
        overlay = OvDx8Overlay;
    }

    if (GetModuleHandleA("d3d9.dll") && D3D9Overlay == NULL)
    {
        D3D9Overlay = new Dx9Overlay( hookParams->RenderFunction );
        overlay = D3D9Overlay;
    }

    if (GetModuleHandleA("dxgi.dll") && OvDxgiOverlay == NULL)
    {
        OvDxgiOverlay = new DxgiOverlay( hookParams->RenderFunction );
        overlay = D3D9Overlay;
    }

    if (overlay)
        overlay->VsyncOverrideMode = hookParams->VsyncOverrideMode;
        
    return NULL;
}


VOID
OvCreateOverlay( OV_RENDER RenderFunction )
{
    OV_HOOK_PARAMS hookParams = {0};

    hookParams.RenderFunction = RenderFunction;

    OvCreateOverlayEx(&hookParams);
}


VOID
OvCreateOverlayEx( OV_HOOK_PARAMS* HookParameters )
{
    OV_HOOK_PARAMS *hookParams;

    hookParams = (OV_HOOK_PARAMS*) HeapAlloc(
        GetProcessHeap(), 
        HEAP_ZERO_MEMORY, 
        sizeof(OV_HOOK_PARAMS)
        );

    hookParams->RenderFunction = HookParameters->RenderFunction;
    hookParams->VsyncOverrideMode = HookParameters->VsyncOverrideMode;
    
    CreateThread(0, 0, &CreateOverlay, hookParams, 0, 0);
}
