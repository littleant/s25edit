// Copyright (C) 2009 - 2021 Marc Vester (XaserLE)
// Copyright (C) 2009 - 2021 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CGame.h"
#include "CIO/CMenu.h"
#include "CIO/CWindow.h"
#include "CMap.h"
#include "CSurface.h"
#include "CollisionDetection.h"
#include "callbacks.h"
#include "globals.h"
#include "helpers/mathFuncs.h"

void CGame::scaleMouseEvent(SDL_Event& event)
{
    if(event.type == SDL_MOUSEMOTION)
        mouseScreenPos_ = Position(event.motion.x, event.motion.y);
    if(guiScale_ == dpiScale_)
        return;
    switch(event.type)
    {
        case SDL_MOUSEMOTION:
        {
            const Position pos = screenToView(Position(event.motion.x, event.motion.y));
            // Calculate the relative movement from the scaled positions so small movements don't get lost
            const Position lastPos =
              screenToView(Position(event.motion.x - event.motion.xrel, event.motion.y - event.motion.yrel));
            event.motion.xrel = pos.x - lastPos.x;
            event.motion.yrel = pos.y - lastPos.y;
            event.motion.x = pos.x;
            event.motion.y = pos.y;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            const Position pos = screenToView(Position(event.button.x, event.button.y));
            event.button.x = pos.x;
            event.button.y = pos.y;
            break;
        }
        default: break;
    }
}

namespace {
bool tryZoom(Position deltaTriangle, int deltaInc, CMap* mapObj)
{
    if(!mapObj || !mapObj->getMap())
        return false;
    const int newInc = triangleIncrease + deltaInc;
    if(newInc < 1 || newInc > 10)
        return false;

    callback::PleaseWait(INITIALIZING_CALL);
    triangleWidth += deltaTriangle.x;
    triangleHeight += deltaTriangle.y;
    triangleIncrease += deltaInc;
    bobMAP* myMap = mapObj->getMap();
    myMap->updateVertexCoords();
    CSurface::get_nodeVectors(*myMap);
    callback::PleaseWait(WINDOW_QUIT_MESSAGE);
    return true;
}

void resetZoom(CMap* mapObj)
{
    if(!mapObj || !mapObj->getMap())
        return;
    callback::PleaseWait(INITIALIZING_CALL);
    triangleWidth = TRIANGLE_WIDTH_DEFAULT;
    triangleHeight = TRIANGLE_HEIGHT_DEFAULT;
    triangleIncrease = TRIANGLE_INCREASE_DEFAULT;
    bobMAP* myMap = mapObj->getMap();
    myMap->updateVertexCoords();
    CSurface::get_nodeVectors(*myMap);
    callback::PleaseWait(WINDOW_QUIT_MESSAGE);
}
} // namespace

Position CGame::warpMouseBack(Position pos)
{
    const float scale = guiScale_ / dpiScale_;
    const Position screenPos = viewToScreen(pos);
    mouseOffsetRemainder_.x += (mouseScreenPos_.x - screenPos.x) / scale;
    mouseOffsetRemainder_.y += (mouseScreenPos_.y - screenPos.y) / scale;
    const Position offset(static_cast<int>(mouseOffsetRemainder_.x), static_cast<int>(mouseOffsetRemainder_.y));
    mouseOffsetRemainder_.x -= offset.x;
    mouseOffsetRemainder_.y -= offset.y;
    SDL_WarpMouseInWindow(window_.get(), screenPos.x, screenPos.y);
    // The motion event of this warp is then ignored as a duplicate
    mouseScreenPos_ = screenPos;
    lastMousePos_ = pos;
    return offset;
}

bool CGame::isMapCursorFrozen() const
{
    return MapObj && MapObj->isActive() && MapObj->isCursorFrozen();
}

void CGame::useFrozenCursorPos(SDL_MouseButtonEvent& button) const
{
    if(isMapCursorFrozen())
    {
        button.x = Cursor.pos.x;
        button.y = Cursor.pos.y;
    }
}

void CGame::EventHandling(SDL_Event* Event)
{
    // Translate mouse positions from window coordinates to the (scaled) view
    scaleMouseEvent(*Event);

    switch(Event->type)
    {
        case SDL_KEYDOWN:
        {
            // NOTE: we will now deliver the data to menus, windows, map etc., sometimes we have to break the switch and
            // stop
            //      delivering earlier, for doing this we make use of a variable showing us the deliver status
            bool delivered = false;
            // now we walk through the windows and find out, if cursor is on one of these (ordered by priority)
            // we have to change the prioritys of the windows (for rendering), so find the highest one
            int highestPriority = 0;
            for(auto& Window : Windows)
            {
                if(Window->getPriority() > highestPriority)
                    highestPriority = Window->getPriority();
            }

            for(auto& Window : Windows)
            {
                if(!Window->isWaste() && Window->isMarked() && Window->getPriority() == highestPriority
                   && Window->hasActiveInputElement())
                {
                    Window->setKeyboardData(Event->key);
                    delivered = true;
                    break;
                }
            }
            // if (delivered)
            //    break;

            // deliver keyboard data to map if active
            if(!delivered)
            {
                if(MapObj && MapObj->isActive())
                {
                    MapObj->setKeyboardData(Event->key);
                    // data has been delivered to map, so no menu is in the foreground --> stop delivering
                    // break;
                }

                // deliver keyboard data to active menus
                for(auto& Menu : Menus)
                {
                    if(Menu->isActive() && !Menu->isWaste())
                        Menu->setKeyboardData(Event->key);
                }
            }

            switch(Event->key.keysym.sym)
            {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if(Event->key.keysym.mod & KMOD_ALT)
                    {
                        fullscreen = !fullscreen;
                        ApplyWindowChanges();
                        SaveSettings();
                    }
                    break;

#ifdef _ADMINMODE
                case SDLK_F3: // if CTRL and ALT are pressed
                    // if (SDL_GetModState() == (KMOD_LCTRL | KMOD_LALT))
                    callback::debugger(INITIALIZING_CALL);
                    break;
                case SDLK_F4: // if CTRL and ALT are pressed
                    // if (SDL_GetModState() == (KMOD_LCTRL | KMOD_LALT))
                    callback::viewer(INITIALIZING_CALL);
                    break;
#endif
                // F5 - F7 is ZOOM, F5 = zoom in, F6 = normal view, F7 = zoom out
                case SDLK_F5:
                    tryZoom(Position(ZOOM_STEP_WIDTH, ZOOM_STEP_HEIGHT), ZOOM_STEP_INCREASE, MapObj.get());
                    break;
                case SDLK_F6: resetZoom(MapObj.get()); break;
                case SDLK_F7:
                    tryZoom(Position(-ZOOM_STEP_WIDTH, -ZOOM_STEP_HEIGHT), -ZOOM_STEP_INCREASE, MapObj.get());
                    break;

                default: break;
            }

            break;
        }

        case SDL_KEYUP:
        {
            // deliver keyboard data to map
            if(MapObj)
                MapObj->setKeyboardData(Event->key);

            break;
        }

        case SDL_MOUSEMOTION:
        {
            // Avoid duplicate events especially when warping the mouse back while scrolling the map
            {
                const Position newPos(Event->motion.x, Event->motion.y);
                if(newPos == lastMousePos_)
                    break;
                lastMousePos_ = newPos;
            }

            if(MapObj)
                MapObj->onMouseMotion(Event->motion);
            // setup mouse cursor data (it stays at the start position while scrolling the map)
            if(!isMapCursorFrozen())
                Cursor.pos = Position(Event->motion.x, Event->motion.y);
            /*
                        //NOTE: we will now deliver the data to menus, windows, map etc., sometimes we have to break the
               switch and stop
                        //      delivering earlier, for doing this we make use of a variable showing us the deliver
               status int delivered = false;
                        //deliver mouse motion data to the active window
                        for (int i = 0; i < MAXWINDOWS; i++)
                        {
                            if (Windows[i] != nullptr && Windows[i]->isActive() && !Windows[i]->isWaste())
                            {
                                Windows[i]->setMouseData(Event->motion);
                                if ( (Event->motion.x >= Windows[i]->getX()) && (Event->motion.x < Windows[i]->getX() +
               Windows[i]->getW())
               && (Event->motion.y >= Windows[i]->getY()) && (Event->motion.y < Windows[i]->getY() + Windows[i]->getH())
               ) delivered = true;
                            }
                        }
                        if (delivered)
                            break;
            */

            // Keep scrolling the map when the cursor moves over a window.
            if(MapObj && MapObj->isActive() && MapObj->isScrolling())
            {
                MapObj->setMouseData(Event->motion);
                break;
            }

            // NOTE: we will now deliver the data to menus, windows, map etc., sometimes we have to break the switch and
            // stop
            //      delivering earlier, for doing this we make use of a variable showing us the deliver status
            bool delivered = false;
            // now we walk through the windows and find out, if cursor is on one of these (ordered by priority)
            // we have to change the prioritys of the windows (for rendering), so find the highest one
            int highestPriority = 0;
            for(auto& Window : Windows)
            {
                if(Window->getPriority() > highestPriority)
                    highestPriority = Window->getPriority();
            }

            for(int actualPriority = highestPriority; actualPriority >= 0; actualPriority--)
            {
                for(auto& Window : Windows)
                {
                    if(!Window->isWaste() && Window->getPriority() == actualPriority)
                    {
                        // is the cursor INSIDE the window or does the user move or resize the window?
                        if(isPointInRect(Event->motion.x, Event->motion.y, Rect(Window->getPos(), Window->getSize()))
                           || Window->isMoving() || Window->isResizing())
                        {
                            // Windows[i]->setActive();
                            // Windows[i]->setPriority(highestPriority+1);
                            Window->setMouseData(Event->motion);
                            delivered = true;
                            break;
                        }
                    }
                }
                if(delivered)
                    break;
            }
            // if mouse data has been delivered, stop delivering anymore
            if(delivered)
                break;

            // deliver mouse motion data to map if active
            if(MapObj && MapObj->isActive())
            {
                MapObj->setMouseData(Event->motion);
                // data has been delivered to map, so no menu is in the foreground --> stop delivering
                break;
            }

            // deliver mouse motion data to active menus
            for(auto& Menu : Menus)
            {
                if(Menu->isActive() && !Menu->isWaste())
                {
                    Menu->setMouseData(Event->motion);
                    break;
                }
            }

            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        {
            useFrozenCursorPos(Event->button);
            // setup mouse cursor data
            Cursor.clicked = true;
            Cursor.button.left = false;
            Cursor.button.right = false;
            if(Event->button.button == SDL_BUTTON_LEFT)
                Cursor.button.left = true;
            else if(Event->button.button == SDL_BUTTON_RIGHT)
                Cursor.button.right = true;

            // clicking a mouse button will close the S2 loading screen if it is shown
            if(showLoadScreen)
            {
                showLoadScreen = false;
                // prevent pressing another object "behind" the loading screen
                break;
            }

            // NOTE: we will now deliver the data to menus, windows, map etc., sometimes we have to break the switch and
            // stop
            //      delivering earlier, for doing this we make use of a variable showing us the deliver status
            bool delivered = false;
            // now we walk through the windows and find out, if cursor is on one of these (ordered by priority)
            // we have to change the prioritys of the windows (for rendering), so find the highest one
            int highestPriority = 0;
            for(auto& Window : Windows)
            {
                if(Window->getPriority() > highestPriority)
                    highestPriority = Window->getPriority();
            }

            for(int actualPriority = highestPriority; actualPriority >= 0; actualPriority--)
            {
                for(auto& Window : Windows)
                {
                    if(!Window->isWaste() && Window->getPriority() == actualPriority)
                    {
                        // is the cursor INSIDE the window?
                        if(isPointInRect(Event->button.x, Event->button.y, Rect(Window->getPos(), Window->getSize())))
                        {
                            Window->setActive();
                            Window->setPriority(highestPriority + 1);
                            Window->setMouseData(Event->button);
                            delivered = true;
                            break;
                        } else if(Window->isActive())
                            Window->setInactive();
                    }
                }
                if(delivered)
                    break;
            }
            // if mouse data has been deliverd, stop delivering anymore
            if(delivered)
                break;

            // deliver mouse button data to map if active
            if(MapObj && MapObj->isActive())
            {
                MapObj->setMouseData(Event->button);
                // data has been delivered to map, so no menu is in the foreground --> stop delivering
                break;
            }

            // deliver mouse button data to active menus
            for(auto& Menu : Menus)
            {
                if(Menu->isActive() && !Menu->isWaste())
                    Menu->setMouseData(Event->button);
            }

            break;
        }

        case SDL_MOUSEBUTTONUP:
        {
            useFrozenCursorPos(Event->button);
            // setup mouse cursor data
            Cursor.clicked = false;

            // NOTE: we will now deliver the data to menus, windows, map etc., sometimes we have to break the switch and
            // stop
            //      delivering earlier, for doing this we make use of a variable showing us the deliver status
            bool delivered = false;
            // now we walk through the windows and find out, if cursor is on one of these (ordered by priority)
            // we have to change the prioritys of the windows (for rendering), so find the highest one
            int highestPriority = 0;
            for(auto& Window : Windows)
            {
                if(Window->getPriority() > highestPriority)
                    highestPriority = Window->getPriority();
            }

            for(int actualPriority = highestPriority; actualPriority >= 0; actualPriority--)
            {
                for(auto& Window : Windows)
                {
                    if(!Window->isWaste() && Window->getPriority() == actualPriority)
                    {
                        // is the cursor INSIDE the window?
                        if(isPointInRect(Event->button.x, Event->button.y, Rect(Window->getPos(), Window->getSize())))
                        {
                            // Windows[i]->setActive();
                            // Windows[i]->setPriority(highestPriority+1);
                            Window->setMouseData(Event->button);
                            delivered = true;
                            break;
                        }
                        // else if (Windows[i]->isActive())
                        // Windows[i]->setInactive();
                    }
                }
                if(delivered)
                    break;
            }
            // if mouse data has been deliverd, stop delivering anymore
            /// We can't stop here cause of problems with the map. If user has the left mouse button pressed and
            /// modifies the vertices, it will cause a problem if he walks over a window with pressed mouse button and
            /// releases it in the window. So the MapObj needs the "release-event" of the mouse button.
            // if (delivered)
            // break;

            // if still not delivered, keep delivering to secondary elements like menu or map

            // deliver mouse button data to map if active
            if(MapObj && MapObj->isActive())
            {
                MapObj->setMouseData(Event->button);
                // data has been delivered to map, so no menu is in the foreground --> stop delivering
                break;
            }

            /// now we do what we commented out a few lines before
            if(delivered)
                break;

            // deliver mouse button data to active menus
            for(auto& Menu : Menus)
            {
                if(Menu->isActive() && !Menu->isWaste())
                    Menu->setMouseData(Event->button);
            }
            break;
        }

        case SDL_MOUSEWHEEL:
        {
            // Scroll lists in the topmost window below the cursor, or in the menus if there is none
            CWindow* wndAtCursor = nullptr;
            for(auto& Window : Windows)
            {
                if(!Window->isWaste() && isPointInRect(Cursor.pos, Window->getRect())
                   && (!wndAtCursor || Window->getPriority() > wndAtCursor->getPriority()))
                    wndAtCursor = Window.get();
            }
            if(wndAtCursor)
            {
                // Positions of the controls are relative to the window
                wndAtCursor->setMouseWheel(Event->wheel, Cursor.pos - wndAtCursor->getPos());
                break;
            }
            // Zoom the map if active
            if(MapObj && MapObj->isActive())
            {
                int y = Event->wheel.y;
                if(Event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                    y = -y;
                if(y == 0)
                    break;

                const Position deltaTri = (y > 0) ? Position(11, 5) : Position(-11, -5);
                const int deltaInc = (y > 0) ? 1 : -1;

                // Save values before zoom for cursor-centered adjustment
                const PointF oldTriSize(triangleWidth, triangleHeight);
                const PointF mousePos(Cursor.pos);

                if(!tryZoom(deltaTri, deltaInc, MapObj.get()))
                    break;

                // Keep the map point under the cursor stable after zoom
                DisplayRectangle dr = MapObj->getDisplayRect();
                const PointF ratio(triangleWidth / oldTriSize.x, triangleHeight / oldTriSize.y);
                const auto size = dr.getSize();
                const PointF newOrigin = (PointF(dr.getOrigin()) + mousePos) * ratio - mousePos;
                dr.left = static_cast<Sint32>(newOrigin.x);
                dr.top = static_cast<Sint32>(newOrigin.y);
                dr.right = dr.left + size.x;
                dr.bottom = dr.top + size.y;
                MapObj->setDisplayRect(dr);
                break;
            }
            for(auto& Menu : Menus)
            {
                if(Menu->isActive() && !Menu->isWaste())
                    Menu->setMouseWheel(Event->wheel, Cursor.pos);
            }
            break;
        }

        case SDL_WINDOWEVENT:
        {
            // The buttons may be released in another window (e.g. after Alt+Tab), so stop scrolling/modifying the map
            if(Event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            {
                Cursor.clicked = false;
                if(MapObj)
                {
                    MapObj->stopScrolling();
                    MapObj->stopModifying();
                }
            }
            if(Event->window.event == SDL_WINDOWEVENT_RESIZED)
            {
                // Also in fullscreen as X11 and Wayland apply desktop fullscreen later
                const Extent newSize(Event->window.data1, Event->window.data2);
                // Ignore events matching the resolution we already applied, unless the pixel size changed
                // (macOS reports a changed display scale as a resize event with the same size).
                int pixelWidth = 0, pixelHeight = 0;
                SDL_GL_GetDrawableSize(window_.get(), &pixelWidth, &pixelHeight);
                if(newSize == appliedResolution_ && pixelWidth == static_cast<int>(std::lround(newSize.x * dpiScale_)))
                    break;
                UpdateDisplaySize(newSize);
            }
#if SDL_VERSION_ATLEAST(2, 0, 18)
            if(Event->window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED)
            {
                // Windows keeps the size in pixels when moving to a display with another scaling. Scale the window
                // like other DPI aware applications and keep its center, which determines the display.
                const float newDisplayScale = getDisplayScale(SDL_GetWindowDisplayIndex(window_.get()));
                Extent newSize = fullscreen ? getWindowSize() : WindowResolution;
                if(newDisplayScale != displayScale_
                   && !(SDL_GetWindowFlags(window_.get()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
                {
                    const float factor = newDisplayScale / displayScale_;
                    newSize = Extent(helpers::iround<unsigned>(WindowResolution.x * factor),
                                     helpers::iround<unsigned>(WindowResolution.y * factor));
                    // Keep the window including its frame (title bar) inside the usable area of the new display
                    const auto area = limitToUsableArea(newSize, SDL_GetWindowDisplayIndex(window_.get()));
                    int x = 0, y = 0;
                    SDL_GetWindowPosition(window_.get(), &x, &y);
                    x += (static_cast<int>(WindowResolution.x) - static_cast<int>(newSize.x)) / 2;
                    y += (static_cast<int>(WindowResolution.y) - static_cast<int>(newSize.y)) / 2;
                    if(area)
                    {
                        // If it still does not fit keep the top left (title bar) visible
                        x = std::max(area->x, std::min(x, area->x + area->w - static_cast<int>(newSize.x)));
                        y = std::max(area->y, std::min(y, area->y + area->h - static_cast<int>(newSize.y)));
                    }
                    SDL_SetWindowSize(window_.get(), newSize.x, newSize.y);
                    SDL_SetWindowPosition(window_.get(), x, y);
                }
                // Also updates the Auto GUI scale and in fullscreen the window mode size
                UpdateDisplaySize(newSize);
            }
#endif
            break;
        }

        case SDL_QUIT: Running = false; break;

        default: break;
    }
}
