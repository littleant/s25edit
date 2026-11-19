// Copyright (C) 2009 - 2021 Marc Vester (XaserLE)
// Copyright (C) 2009 - 2021 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CIO/CFont.h"
#include "Texture.h"
#include <boost/filesystem/path.hpp>
#include <Point.h>
#include <memory>
#include <vector>

class CWindow;
class CMap;
class CMenu;

class CGame
{
    friend class CDebug;

public:
    Extent GameResolution;
    bool fullscreen;
    /// Scroll the map by dragging it like a picture with the right mouse button: The cursor moves with the map.
    /// Otherwise the cursor stays in place while the map moves, which needs relative mouse movement (no absolute
    /// pointing devices like graphics tablets or VMs with mouse integration).
    bool dragScrolling = false;

    bool Running;
    bool showLoadScreen;
    SDL_GLContext glContext_ = nullptr;
    SdlWindow window_;

private:
#ifdef _ADMINMODE
    // some debugging variables
    unsigned long int FrameCounter = 0;
#endif
    // milliseconds for SDL_Delay()
    Uint32 msWait = 0;
    Uint32 framesPassedSinceLastFps = 0;
    Uint32 lastFpsTick = 0;
    CFont lastFps;

    Uint32 lastFrameTime = 0;
    Extent appliedResolution_ = Extent{0, 0}; ///< Last resolution we applied to the window/display
    bool appliedFullscreen_ = false;          ///< Last fullscreen state we applied

    // Textures for splash screen and cursor
    Texture splashBg_;
    Texture cursor_;
    Texture cursorClicked_;
    Texture cross_;
    Texture mapTex_; ///< Texture for the map/terrain (Surf_Map may be 8-bit)

    // structure for mouse cursor
    struct
    {
        Position pos = {0, 0};
        bool clicked = false;
        struct
        {
            bool left = false;
            bool right = false;
        } button;
    } Cursor;

    // Object for Menu Screens
    std::vector<std::unique_ptr<CMenu>> Menus;
    // Object for Windows
    std::vector<std::unique_ptr<CWindow>> Windows;
    // Object for Callbacks
    std::vector<void (*)(int)> Callbacks;
    // Object for the Map
    std::unique_ptr<CMap> MapObj;

    void SetAppIcon();
    void setGLViewport();
    bool CreateWindowAndGLContext();
    /// While scrolling the map the cursor stays at the start position, but the position of the button events may
    /// differ (it follows the relative motion if the pointer is locked). Use the cursor position there.
    void useFrozenCursorPos(SDL_MouseButtonEvent& button) const;

public:
    // Apply current GameResolution and fullscreen settings to the window/display.
    void ApplyWindowChanges();

    void LoadSettings();
    void SaveSettings() const;

    CGame(Extent GameResolution_, bool fullscreen_);
    ~CGame();

    int Execute();

    bool Init();
    void UpdateDisplaySize(const Extent& newSize);

    void EventHandling(SDL_Event* Event);

    void GameLoop();

    void Render();

    void RenderPresent();

    CMenu* RegisterMenu(std::unique_ptr<CMenu> Menu);
    bool UnregisterMenu(CMenu* Menu);
    CWindow* RegisterWindow(std::unique_ptr<CWindow> Window);
    bool UnregisterWindow(CWindow* Window);
    void RegisterCallback(void (*callback)(int));
    bool UnregisterCallback(void (*callback)(int));
    void setMapObj(std::unique_ptr<CMap> MapObj);
    CMap* getMapObj();
    void delMapObj();
    void enterEditor(const boost::filesystem::path& filepath);
    auto getRes() const { return GameResolution; }
};
