// Copyright (C) 2009 - 2021 Marc Vester (XaserLE)
// Copyright (C) 2009 - 2021 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CIO/CFont.h"
#include "Texture.h"
#include <boost/filesystem/path.hpp>
#include <Point.h>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

class CWindow;
class CMap;
class CMenu;

class CGame
{
    friend class CDebug;

public:
    /// Size of the view in which everything is drawn: size in pixels divided by the GUI scale
    Extent GameResolution;
    /// Size of the window in window mode in window coordinates (pixels, or points on macOS and Wayland) for the current
    /// display, kept while in fullscreen
    Extent WindowResolution;
    bool fullscreen;
    /// GUI scale in percent, 0 = auto (use display scaling)
    unsigned guiScalePercent = 0;

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
    Extent appliedResolution_ = Extent{0, 0}; ///< Current window size (also in fullscreen) in window coordinates
    float guiScale_ = 1.f;                    ///< Currently used GUI scale factor (pixels per view unit)
    float dpiScale_ = 1.f;                    ///< Pixels per window coordinate (> 1 on high DPI macOS/Wayland)
    float displayScale_ = 1.f;                ///< Display scaling of the window's display (Windows only, else 1)
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
    void scaleMouseEvent(SDL_Event& event) const;
    /// Display scaling of the given display on Windows (window coordinates are pixels there), 1 elsewhere
    static float getDisplayScale(int displayIndex);
    /// Current window size reported by SDL in window coordinates
    Extent getWindowSize() const;
    /// Limit the window size to the usable area of the display minus the window frame (none without a window).
    /// Returns that area for the client area or nothing on failure.
    std::optional<SDL_Rect> limitToUsableArea(Extent& size, int displayIndex) const;
    bool CreateWindowAndGLContext();

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

    /// Range of selectable GUI scales in percent for the given window size
    struct GuiScaleRange
    {
        unsigned minPercent, maxPercent, recommendedPercent;
    };
    GuiScaleRange getGuiScaleRange(const Extent& windowSize) const;
    /// Size of the window in fullscreen on its current display in window coordinates
    Extent getFullscreenSize() const;
    /// Pixels per window coordinate (> 1 on high DPI macOS/Wayland). Rounded to 1/120 (unit of Wayland's fractional
    /// scales) as the measured ratio is inexact for small windows.
    float getDpiScale() const { return std::round(dpiScale_ * 120.f) / 120.f; }
    /// Convert a position in window coordinates to view coordinates (like GuiScale::screenToView in s25client)
    Position screenToView(const Position& pos) const
    {
        const float scale = guiScale_ / dpiScale_;
        return Position(static_cast<int>(pos.x / scale), static_cast<int>(pos.y / scale));
    }
    /// Convert a position in view coordinates to window coordinates (like GuiScale::viewToScreen in s25client)
    Position viewToScreen(const Position& pos) const
    {
        // Round up so that converting back with screenToView yields the same position
        const float scale = guiScale_ / dpiScale_;
        return Position(static_cast<int>(std::ceil(pos.x * scale)), static_cast<int>(std::ceil(pos.y * scale)));
    }
    /// Set the GL scissor rect from view coordinates
    void setScissor(const Rect& rect) const;
};
