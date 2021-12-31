/*
MIT License

Copyright (c) 2021 lespalt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"dwrite.lib")


#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>
#include "iracing.h"
#include "Config.h"
#include "OverlayRelative.h"
#include "OverlayInputs.h"
#include "OverlayStandings.h"


static void handleConfigChange( std::vector<Overlay*> overlays, ConnectionStatus status )
{
    for( Overlay* o : overlays )
    {
        o->enable( g_cfg.getBool(o->getName(),"enabled") && (
            status == ConnectionStatus::DRIVING ||
            status == ConnectionStatus::CONNECTED && !o->shouldEnableOnlyWhileDriving()
            ));
        o->configChanged();
    }
}

int main()
{
    // Bump priority up so we get time from the sim
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Load the config and watch it for changes
    g_cfg.load();
    g_cfg.watchForChanges();

    // Register hotkey to enable/disable position/size changes.
    const int hotkey = g_cfg.getString( "General", "ui_edit_hotkey_is_alt_and_this_letter" )[0];
    RegisterHotKey( NULL, 0, MOD_ALT, toupper(hotkey) );
    
    // Register hotkey to enable/disable standings overlay.
    // TODO: make this more flexible/configurable. Perhaps use DInput so we can map it to non-keyboard keys?
    RegisterHotKey( NULL, 1, MOD_CONTROL, 'S' );

    // Create overlays
    std::vector<Overlay*> overlays;
    overlays.push_back( new OverlayRelative() );
    overlays.push_back( new OverlayInputs() );
    overlays.push_back( new OverlayStandings() );

    ConnectionStatus  status = ConnectionStatus::UNKNOWN;
    bool              uiEdit = false;

    while( true )
    {
        ConnectionStatus prevStatus = status;
        status = ir_tick();
        if( status != prevStatus )
        {
            if( status == ConnectionStatus::DISCONNECTED )
                printf("Waiting for iRacing connection...\n");
            else
                printf("Connected to iRacing\n");

            // Enable user-selected overlays, but only if we're driving
            handleConfigChange( overlays, status );
        }

        // Update roughly every 16ms
        for( Overlay* o : overlays )
            o->update();

        // Watch for config change signal
        if( g_cfg.hasChanged() )
        {
            g_cfg.load();
            handleConfigChange( overlays, status );
        }

        // Message pump
        MSG msg = {};
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if( msg.message == WM_HOTKEY )
            {
                if( msg.wParam == 0 )
                {
                    uiEdit = !uiEdit;
                    for( Overlay* o : overlays )
                        o->enableUiEdit( uiEdit );
                }
                else if( msg.wParam == 1 )
                {
                    g_cfg.setBool( "OverlayStandings", "enabled", !g_cfg.getBool("OverlayStandings","enabled") );
                    g_cfg.save();
                    handleConfigChange( overlays, status );
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    for( Overlay* o : overlays )
        delete o;
}
