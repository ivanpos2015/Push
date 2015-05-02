#include <Windows.h>
#include <time.h>
#include <stdio.h>

#include "overlay.h"
#include <OvRender.h>


/**
* Draws all on-screen display items.
*/

VOID Osd_Draw( OvOverlay* Overlay )
{
    UINT8 i;
    OSD_ITEM *osdItem;
    osdItem = PushSharedMemory->OsdItems;

    for (i = 0; i < PushSharedMemory->NumberOfOsdItems; i++, osdItem++)
    {
        if (!osdItem->Flag //draw if no flag, could be somebody just wants to display stuff on-screen
            || PushSharedMemory->OSDFlags & osdItem->Flag //if it has a flag, is it set?
            || (osdItem->Threshold && osdItem->Value > osdItem->Threshold)) //is the item's value > it's threshold?
        {
            Overlay->DrawText(osdItem->Text, osdItem->Color);
        }
    }
}
