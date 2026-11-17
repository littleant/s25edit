// Copyright (C) 2026 - 2026 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "defines.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CButton;
class CSelectBox;

/// Combo box like ctrlComboBox of s25client: A field showing the selected item and an arrow button which open a list
class CDropDown
{
public:
    using Callback = std::function<void(unsigned index)>;

    /// The callback is called when the user selects an item
    CDropDown(Position pos, Extent size, Callback callback);
    ~CDropDown();

    /// Set the items and the selected one (no text is shown if selection is out of range)
    void setItems(std::vector<std::string> items, unsigned selection);

    void setMouseData(const SDL_MouseMotionEvent& motion);
    /// Return true if the event was handled by the open list and must not be passed to controls below it
    bool setMouseData(const SDL_MouseButtonEvent& button);
    /// Return true if the open list was scrolled
    bool setMouseWheel(const SDL_MouseWheelEvent& wheel, Position mousePos);
    void draw(Position parentOrigin) const;

private:
    /// Maximum number of lines shown in the list at once
    static constexpr unsigned maxListLines = 20;

    Position pos_;
    Extent size_;
    Callback callback_;
    std::vector<std::string> items_;
    unsigned selection_ = 0;
    bool open_ = false;
    std::unique_ptr<CButton> arrowButton_;
    std::unique_ptr<CSelectBox> list_;
    /// Item clicked in the list, set during the mouse event of the list
    std::optional<unsigned> clickedItem_;

    bool isInList(Position mousePos) const;
};
