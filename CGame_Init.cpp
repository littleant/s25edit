// Copyright (C) 2009 - 2021 Marc Vester (XaserLE)
// Copyright (C) 2009 - 2021 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CGame.h"
#include "CIO/CFile.h"
#include "CIO/CMenu.h"
#include "CIO/CWindow.h"
#include "CMap.h"
#include "callbacks.h"
#include "globals.h"
#include "helpers/mathFuncs.h"
#include "lua/GameDataLoader.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <iostream>

bool CGame::CreateWindowAndGLContext()
{
    if(window_)
        return false;

    // The window size is stored in pixels at 100% scaling. Convert it with the scaling of the primary display the
    // window opens on (also for fullscreen, which is entered afterwards).
    displayScale_ = getDisplayScale(0);
    WindowResolution = Extent(helpers::iround<unsigned>(WindowResolution.x * displayScale_),
                              helpers::iround<unsigned>(WindowResolution.y * displayScale_));
    limitToUsableArea(WindowResolution, 0);

    window_.reset(SDL_CreateWindow(
      "Return to the Roots Map editor [BETA]", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WindowResolution.x,
      WindowResolution.y, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
    if(!window_)
        return false;

    glContext_ = SDL_GL_CreateContext(window_.get());
    if(!glContext_ || !gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        return false;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);

    SDL_ShowWindow(window_.get());

    ApplyWindowChanges();

    SetAppIcon();

    return true;
}

void CGame::ApplyWindowChanges()
{
    if(!window_)
        return;

    if(fullscreen)
    {
        // Desktop fullscreen on the current display: Native resolution without changing the display mode
        if(SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
        {
            std::cerr << "SDL_SetWindowFullscreen failed: " << SDL_GetError() << std::endl;
            return;
        }
    } else if(appliedFullscreen_ || WindowResolution != appliedResolution_ || WindowResolution != getWindowSize())
    {
        // Only touch the window if the mode or size changed, not e.g. for the GUI scale: Windows resizes and moves a
        // maximized window but keeps it maximized, showing its normally off-screen frame beside the content.
        // Remember the current display before leaving fullscreen so we re-center there instead of on the primary one
        int displayIndex = SDL_GetWindowDisplayIndex(window_.get());
        if(displayIndex < 0)
            displayIndex = 0;
        if(SDL_SetWindowFullscreen(window_.get(), 0) != 0)
        {
            std::cerr << "SDL_SetWindowFullscreen failed: " << SDL_GetError() << std::endl;
            return;
        }
        // Keep a maximized window of this size (Windows maximizes it again when leaving fullscreen)
        const bool maximized = (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_MAXIMIZED) != 0;
        if(!maximized || WindowResolution != getWindowSize())
        {
            // A maximized window keeps its size: Wayland even reports the new size, but keeps the old pixel size
            if(maximized)
                SDL_RestoreWindow(window_.get());
            // Limit to the usable area minus the frame (known once the window exists), center there (title bar visible)
            const auto area = limitToUsableArea(WindowResolution, displayIndex);
            SDL_SetWindowSize(window_.get(), WindowResolution.x, WindowResolution.y);
            if(area)
                SDL_SetWindowPosition(window_.get(), area->x + (area->w - static_cast<int>(WindowResolution.x)) / 2,
                                      area->y + (area->h - static_cast<int>(WindowResolution.y)) / 2);
            else
                SDL_SetWindowPosition(window_.get(), SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex),
                                      SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex));
        }
    }

    // X11 and Wayland may apply fullscreen later and send a resize event
    UpdateDisplaySize(fullscreen ? getWindowSize() : WindowResolution);
}

void CGame::setGLViewport() // NOLINT(readability-make-member-function-const)
{
    if(!window_)
        return;
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window_.get(), &w, &h);
    if(w == 0 || h == 0)
        return;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, GameResolution.x, GameResolution.y, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

float CGame::getDisplayScale([[maybe_unused]] int displayIndex)
{
#ifdef _WIN32
    float dpi = 0;
    if(SDL_GetDisplayDPI(displayIndex, nullptr, &dpi, nullptr) == 0 && dpi > 0)
        return dpi / 96.f;
#endif
    return 1.f;
}

Extent CGame::getWindowSize() const
{
    int w = 0, h = 0;
    SDL_GetWindowSize(window_.get(), &w, &h);
    return Extent(w, h);
}

std::optional<SDL_Rect> CGame::limitToUsableArea(Extent& size, int displayIndex) const
{
    SDL_Rect area;
    if(SDL_GetDisplayUsableBounds(displayIndex, &area) != 0)
        return std::nullopt;
    int top = 0, left = 0, bottom = 0, right = 0; // Stay 0 without a window or on failure
    if(window_)
        SDL_GetWindowBordersSize(window_.get(), &top, &left, &bottom, &right);
    area = {area.x + left, area.y + top, area.w - left - right, area.h - top - bottom};
    size.x = std::min(size.x, static_cast<unsigned>(std::max(1, area.w)));
    size.y = std::min(size.y, static_cast<unsigned>(std::max(1, area.h)));
    return area;
}

CGame::GuiScaleRange CGame::getGuiScaleRange(const Extent& windowSize) const
{
    // Same as in s25client, but use the display scaling as the recommended scale: The pixel density on macOS and
    // Wayland, the display scaling of Windows (window coordinates are pixels there) and 100% elsewhere.
    // Don't combine both as SDL_GetDisplayDPI already includes the pixel density with sdl2-compat.
    const float dpiScale = dpiScale_ > 1.f ? dpiScale_ : displayScale_;
    constexpr auto min = 100u;
    const auto recommended = std::max(min, helpers::iround<unsigned>(dpiScale * 100.f));
    // Keep at least 800x600 visible
    const auto maxScale = std::min(windowSize.x * dpiScale_ / 800.f, windowSize.y * dpiScale_ / 600.f);
    const auto max = std::max(helpers::iround<unsigned>(maxScale * 100.f), min);
    return GuiScaleRange{min, max, std::min(recommended, max)};
}

Extent CGame::getFullscreenSize() const
{
    // The current size when in fullscreen, else the bounds of the window's display
    SDL_Rect bounds;
    if(fullscreen || SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window_.get()), &bounds) != 0)
        return appliedResolution_;
    return Extent(bounds.w, bounds.h);
}

void CGame::setScissor(const Rect& rect) const
{
    const auto left = static_cast<int>(std::floor(rect.left * guiScale_));
    const auto top = static_cast<int>(std::floor(rect.top * guiScale_));
    const auto right = static_cast<int>(std::ceil(rect.right * guiScale_));
    const auto bottom = static_cast<int>(std::ceil(rect.bottom * guiScale_));
    glScissor(left, helpers::iround<int>(appliedResolution_.y * dpiScale_) - bottom, right - left, bottom - top);
}

void CGame::UpdateDisplaySize(const Extent& newSize)
{
    appliedResolution_ = newSize;
    appliedFullscreen_ = fullscreen;
    // In fullscreen keep the window mode size, but scale it with the display scaling like a window
    const float displayScale = getDisplayScale(SDL_GetWindowDisplayIndex(window_.get()));
    if(!fullscreen)
        WindowResolution = newSize;
    else if(displayScale != displayScale_)
        WindowResolution = Extent(WindowResolution * (displayScale / displayScale_));
    displayScale_ = displayScale;

    // The GL drawable is larger than the window on high DPI displays on macOS and Wayland
    int windowWidth = 0, windowHeight = 0, pixelWidth = 0, pixelHeight = 0;
    if(window_)
    {
        SDL_GetWindowSize(window_.get(), &windowWidth, &windowHeight);
        SDL_GL_GetDrawableSize(window_.get(), &pixelWidth, &pixelHeight);
    }
    if(windowWidth > 0 && pixelWidth > 0)
        dpiScale_ = static_cast<float>(pixelWidth) / windowWidth;

    const auto range = getGuiScaleRange(appliedResolution_);
    const auto percent = guiScalePercent == 0 ? range.recommendedPercent :
                                                helpers::clamp(guiScalePercent, range.minPercent, range.maxPercent);
    guiScale_ = percent / 100.f;
    GameResolution = Extent(helpers::iround<unsigned>(appliedResolution_.x * dpiScale_ / guiScale_),
                            helpers::iround<unsigned>(appliedResolution_.y * dpiScale_ / guiScale_));

    setGLViewport();
}

bool CGame::Init()
{
    std::cout << "Return to the Roots Map editor\n";

    SDL_ShowCursor(SDL_DISABLE);

    std::cout << "Create Window...";
    if(!CreateWindowAndGLContext())
    {
        std::cout << "failure";
        return false;
    }
    CFile::init();

    /*NOTE: its important to load a palette at first,
     *      otherwise all images will be black (in exception of the LBM-Files, they have their own palette).
     *      if its necessary to load pictures from a
     *      specified file with a special palette, so
     *      load this palette from a file and set
     *      CFile::palActual = CFile::palArray - 1 (--> last loaden palette)
     *      and after loading the images set
     *      CFile::palActual = CFile::palArray (--> first palette)
     */

    // load some pictures (after all the splash-screens)
    // at first GFX/PICS/SETUP997.LBM, cause this is the S2-loading picture
    std::cout << "\nLoading file: GFX/PICS/SETUP997.LBM...";
    if(!CFile::open_file(global::gameDataFilePath / "GFX/PICS/SETUP997.LBM", LBM))
    {
        std::cout << "failure";
        // if SETUP997.LBM doesn't exist, it's probably settlers2+mission cd and there we have SETUP998.LBM instead
        std::cout << "\nTry to load file: GFX/PICS/SETUP998.LBM instead...";
        if(!CFile::open_file(global::gameDataFilePath / "GFX/PICS/SETUP998.LBM", LBM))
        {
            std::cout << "failure";
            return false;
        }
    }

    // Create texture for splash background
    splashBg_.load(global::bmpArray[SPLASHSCREEN_LOADING_S2SCREEN].surface.get(), true);

    // std::cout << "\nShow loading screen...";
    showLoadScreen = true;
    glClear(GL_COLOR_BUFFER_BIT);
    splashBg_.draw(Rect(0, 0, GameResolution.x, GameResolution.y));
    SDL_GL_SwapWindow(window_.get());

    GameDataLoader gdLoader(global::worldDesc);
    if(!gdLoader.load())
    {
        std::cerr << "Failed to load game data!" << std::endl;
        return false;
    }

    // continue loading pictures
    for(const std::string file :
        {"GFX/PICS/SETUP000.LBM", "GFX/PICS/SETUP010.LBM", "GFX/PICS/SETUP011.LBM", "GFX/PICS/SETUP012.LBM",
         "GFX/PICS/SETUP013.LBM", "GFX/PICS/SETUP014.LBM", "GFX/PICS/SETUP015.LBM"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LBM))
        {
            std::cout << "failure";
            // if it doesn't exist, it's probably settlers2+missioncd and we simply load SETUP010.LBM instead
            std::cout << "\nLoading file: GFX/PICS/SETUP010.LBM instead...";
            if(!CFile::open_file(global::gameDataFilePath / "GFX/PICS/SETUP010.LBM", LBM))
            {
                std::cout << "failure";
                return false;
            }
        }
    }

    for(const std::string file :
        {"GFX/PICS/SETUP666.LBM", "GFX/PICS/SETUP667.LBM", "GFX/PICS/SETUP801.LBM", "GFX/PICS/SETUP802.LBM",
         "GFX/PICS/SETUP803.LBM", "GFX/PICS/SETUP804.LBM", "GFX/PICS/SETUP805.LBM", "GFX/PICS/SETUP806.LBM",
         "GFX/PICS/SETUP810.LBM", "GFX/PICS/SETUP811.LBM", "GFX/PICS/SETUP895.LBM", "GFX/PICS/SETUP896.LBM"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LBM))
        {
            std::cout << "failure";
            return false;
        }
    }

    for(const std::string file : {"GFX/PICS/SETUP897.LBM", "GFX/PICS/SETUP898.LBM"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LBM))
        {
            std::cout << "failure";
            // if it doesn't exist, it's probably settlers2+missioncd and we simply load SETUP896.LBM instead
            std::cout << "\nLoading file: GFX/PICS/SETUP896.LBM instead...";
            if(!CFile::open_file(global::gameDataFilePath / "GFX/PICS/SETUP896.LBM", LBM))
            {
                std::cout << "failure";
                return false;
            }
        }
    }

    for(const std::string file :
        {"GFX/PICS/SETUP899.LBM", "GFX/PICS/SETUP990.LBM", "GFX/PICS/WORLD.LBM", "GFX/PICS/WORLDMSK.LBM"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LBM))
        {
            std::cout << "failure";
            return false;
        }
    }

    // load gouraud data
    for(const std::string file : {"DATA/TEXTURES/GOU5.DAT", "DATA/TEXTURES/GOU6.DAT", "DATA/TEXTURES/GOU7.DAT"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, GOU))
        {
            std::cout << "failure";
            return false;
        }
    }

    // load only the palette at this time from editres.idx
    std::cout << "\nLoading palette from file: DATA/EDITRES.IDX...";
    if(!CFile::open_file(global::gameDataFilePath / "DATA/EDITRES.IDX", IDX, true))
    {
        std::cout << "failure";
        return false;
    }
    // set the right palette
    CFile::set_palActual(CFile::get_palArray() - 1);
    std::cout << "\nLoading file: DATA/EDITRES.IDX...";
    if(!CFile::open_file(global::gameDataFilePath / "DATA/EDITRES.IDX", IDX))
    {
        std::cout << "failure";
        return false;
    }
    // set back palette
    CFile::set_palActual(CFile::get_palArray());
    // load only the palette at this time from editio.idx
    std::cout << "\nLoading palette from file: DATA/IO/EDITIO.IDX...";
    if(!CFile::open_file(global::gameDataFilePath / "DATA/IO/EDITIO.IDX", IDX, true))
    {
        std::cout << "failure";
        return false;
    }
    // set the right palette
    CFile::set_palActual(CFile::get_palArray() - 1);
    std::cout << "\nLoading file: DATA/IO/EDITIO.IDX...";
    if(!CFile::open_file(global::gameDataFilePath / "DATA/IO/EDITIO.IDX", IDX))
    {
        std::cout << "failure";
        return false;
    }
    // set back palette
    CFile::set_palActual(CFile::get_palArray());
    std::cout << "\nLoading file: DATA/EDITBOB.LST...";
    if(!CFile::open_file(global::gameDataFilePath / "DATA/EDITBOB.LST", LST))
    {
        std::cout << "failure";
        return false;
    }

    // texture tilesets
    for(const std::string file : {"GFX/TEXTURES/TEX5.LBM", "GFX/TEXTURES/TEX6.LBM", "GFX/TEXTURES/TEX7.LBM"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LBM))
        {
            std::cout << "failure";
            return false;
        }
    }

    /*
    std::cout << "\nLoading palette from file: GFX/PALETTE/PAL5.BBM...";
    if ( !CFile::open_file(global::gameDataFilePath / "GFX/PALETTE/PAL5.BBM", BBM, true) )
    {
        std::cout << "failure";
        return false;
    }
    */

    // EVERY MISSION-FILE SHOULD BE LOADED SEPARATLY IF THE SPECIFIED MISSION GOES ON -- SO THIS IS TEMPORARY
    for(const std::string file : {"DATA/MIS0BOBS.LST", "DATA/MIS1BOBS.LST", "DATA/MIS2BOBS.LST", "DATA/MIS3BOBS.LST",
                                  "DATA/MIS4BOBS.LST", "DATA/MIS5BOBS.LST"})
    {
        std::cout << "\nLoading file: " << file << "...";
        if(!CFile::open_file(global::gameDataFilePath / file, LST))
        {
            std::cout << "failure";
            return false;
        }
    }

    // create the mainmenu
    callback::mainmenu(INITIALIZING_CALL);

    // Create textures for cursor
    cursor_.load(global::bmpArray[CURSOR].surface.get());
    cursorClicked_.load(global::bmpArray[CURSOR_CLICKED].surface.get());
    cross_.load(global::bmpArray[CROSS].surface.get());

    return true;
}
