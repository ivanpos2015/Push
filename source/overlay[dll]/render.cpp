#include <windows.h>
#include "overlay.h"
#include <OvRender.h>
#include "osd.h"
#include "menu.h"
#include <stdio.h>


//UINT8 PushRefreshRate = 60;
//UINT8 AcceptableFps = 55;
BOOLEAN g_SetOSDRefresh = TRUE;
BOOLEAN g_FontInited = FALSE;
BOOLEAN IsStableFramerate;
BOOLEAN DisableAutoOverclock;
UINT16 DiskResponseTime;
UINT32 FrameRate;
UINT8 FrameLimit = 80; //80 fps
UINT64 CyclesWaited;
HANDLE RenderThreadHandle;


double PCFreq = 0.0;
__int64 CounterStart = 0;
typedef enum _OVERCLOCK_UNIT
{
    OC_ENGINE,
    OC_MEMORY

}OVERCLOCK_UNIT;
VOID Overclock(OVERCLOCK_UNIT Unit);

VOID
StartCounter()
{
    LARGE_INTEGER li;
    LARGE_INTEGER freq;

    QueryPerformanceFrequency(&freq);

    PCFreq = (double) freq.QuadPart / 1000.0;

    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
}


double
GetPerformanceCounter()
{
    LARGE_INTEGER li;

    QueryPerformanceCounter(&li);

    return (double) (li.QuadPart - CounterStart) /PCFreq;
}


BOOLEAN IsGpuLag()
{
    if (PushSharedMemory->HarwareInformation.DisplayDevice.Load > 95
        && PushSharedMemory->HarwareInformation.DisplayDevice.EngineClock >= PushSharedMemory->HarwareInformation.DisplayDevice.EngineClockMax)
    {
        return TRUE;
    }
    
    return FALSE;
}


VOID RunFrameStatistics()
{
    static double newTickCount = 0.0, lastTickCount = 0.0,
                  oldTick = 0.0, delta = 0.0,
                  oldTick2 = 0.0, frameTime = 0.0,
                  fps = 0.0, acceptableFrameTime = 0.0;

    static UINT32 frames = 0;
    static BOOLEAN inited = FALSE;

    if (!inited)
    {
        DEVMODE devMode;
        StartCounter();
        inited = TRUE;

        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode);

        acceptableFrameTime = (double)1000 / (double)(devMode.dmDisplayFrequency - 1);
    }

    newTickCount = GetPerformanceCounter();
    delta = newTickCount - oldTick;
    frameTime = newTickCount- lastTickCount;

    frames++;

    if (delta > 1000)
    {
        // Every second.

        double dummy = delta / 1000.0f;
        fps         = frames / dummy;
        oldTick = newTickCount;
        frames      = 0;
        g_SetOSDRefresh = TRUE;

        //if (PushSharedMemory->ThreadOptimization || PushSharedMemory->OSDFlags & OSD_MTU)
        //{
            PushRefreshThreadMonitor();

            //if (PushSharedMemory->OSDFlags & OSD_MTU)
            PushSharedMemory->HarwareInformation.Processor.MaxThreadUsage = PushGetMaxThreadUsage();
        //}

        WCHAR buffer[100];

        swprintf(buffer, 100, L"GetDiskResponseTime %i", GetCurrentProcessId());
        CallPipe(buffer, &DiskResponseTime);

        if (!DisableAutoOverclock && IsGpuLag())
        {
            if (PushSharedMemory->HarwareInformation.DisplayDevice.EngineClockMax < PushSharedMemory->HarwareInformation.DisplayDevice.EngineOverclock)
                Overclock(OC_ENGINE);

            if (PushSharedMemory->HarwareInformation.DisplayDevice.MemoryClockMax < PushSharedMemory->HarwareInformation.DisplayDevice.MemoryOverclock)
                Overclock(OC_MEMORY);

            Log(L"E-Clk %i, E-ClkMax %i, E-OC %i, M-Clk %i, M-ClkMax %i, M-OC %i",
                PushSharedMemory->HarwareInformation.DisplayDevice.EngineClock,
                PushSharedMemory->HarwareInformation.DisplayDevice.EngineClockMax,
                PushSharedMemory->HarwareInformation.DisplayDevice.EngineOverclock,
                PushSharedMemory->HarwareInformation.DisplayDevice.MemoryClock,
                PushSharedMemory->HarwareInformation.DisplayDevice.MemoryClockMax,
                PushSharedMemory->HarwareInformation.DisplayDevice.MemoryOverclock
                );
        }

        RenderThreadHandle = GetCurrentThread();
    }

    // Simple Diagnostics.

    if (frameTime > acceptableFrameTime)
    {
        if (PushSharedMemory->HarwareInformation.Processor.Load > 95)
            PushSharedMemory->Overloads |= OSD_CPU_LOAD;

        if (PushSharedMemory->HarwareInformation.Processor.MaxCoreUsage > 95)
            PushSharedMemory->Overloads |= OSD_MCU;

        if (PushSharedMemory->HarwareInformation.Processor.MaxThreadUsage > 95)
            PushSharedMemory->Overloads |= OSD_MTU;

        if (IsGpuLag()) PushSharedMemory->Overloads |= OSD_GPU_LOAD;

        if ((PushSharedMemory->HarwareInformation.DisplayDevice.EngineClock < PushSharedMemory->HarwareInformation.DisplayDevice.EngineClockMax
            || PushSharedMemory->HarwareInformation.DisplayDevice.MemoryClock < PushSharedMemory->HarwareInformation.DisplayDevice.MemoryClockMax)
            && PushSharedMemory->HarwareInformation.DisplayDevice.Load > 90)
        {
            PushSharedMemory->OSDFlags |= OSD_GPU_E_CLK;
            PushSharedMemory->OSDFlags |= OSD_GPU_M_CLK;
            PushSharedMemory->Overloads |= OSD_GPU_E_CLK;
            PushSharedMemory->Overloads |= OSD_GPU_M_CLK;
        }

        if (PushSharedMemory->HarwareInformation.Memory.Used > PushSharedMemory->HarwareInformation.Memory.Total)
            PushSharedMemory->Overloads |= OSD_RAM;

        PushSharedMemory->HarwareInformation.Disk.ResponseTime = DiskResponseTime;

        if (PushSharedMemory->HarwareInformation.Disk.ResponseTime > 4000)
        {
            PushSharedMemory->Overloads |= OSD_DISK_RESPONSE;
            PushSharedMemory->OSDFlags |= OSD_DISK_RESPONSE;
        }

        if (PushSharedMemory->AutoLogFileIo)
            PushSharedMemory->LogFileIo = TRUE;

        if (PushSharedMemory->HarwareInformation.Processor.MaxThreadUsage > 95)
        {
            PushSharedMemory->Overloads |= OSD_MTU;
            PushSharedMemory->OSDFlags |= OSD_MTU;
        }
    }

    if (((newTickCount - oldTick2) > 30000) && !PushSharedMemory->KeepFps)
        //frame rate has been stable for at least 30 seconds, we can disable the thing
        IsStableFramerate = TRUE;
    else
        //you haz really bad frame rates, for you i give an fps meter.
        IsStableFramerate = FALSE;

    if (frameTime > acceptableFrameTime)
    {
        //reset the timer
        oldTick2 = newTickCount;
    }

    FrameRate = (int) fps;

    if (PushSharedMemory->FrameLimit)
    {
        double frameTimeMin = (double)1000 / (double)FrameLimit;

        if (frameTime < frameTimeMin)
        {
            UINT64 cyclesStart, cyclesStop;

            RenderThreadHandle = GetCurrentThread();

            QueryThreadCycleTime(RenderThreadHandle, &cyclesStart);

            lastTickCount = newTickCount;

            while (frameTime < frameTimeMin)
            {
                newTickCount = GetPerformanceCounter();

                frameTime += (newTickCount - lastTickCount);

                lastTickCount = newTickCount;
            }

            QueryThreadCycleTime(RenderThreadHandle, &cyclesStop);

            CyclesWaited += cyclesStop - cyclesStart;
        }

        lastTickCount = newTickCount;
    }
}


VOID InitializeKeyboardHook();


VOID RnRender( OvOverlay* Overlay )
{
    static BOOLEAN rendering = FALSE;

    if (!rendering)
    {
        rendering = TRUE;

        CallPipe(L"Patch", NULL);
        InitializeKeyboardHook();
    }

    RunFrameStatistics();
    Osd_Draw( Overlay );
    MnuRender( Overlay );
    //Overlay->DrawText(L"u can draw anything in this render loop!\n");
}
