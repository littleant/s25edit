// Copyright (C) 2026 - 2026 Settlers Freaks <sf-team at siedler25.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CDropDown.h"
#include "../Texture.h"
#include "CButton.h"
#include "CFont.h"
#include "CSelectBox.h"
#include "CollisionDetection.h"
#include <algorithm>

namespace {
constexpr FontSize fontSize = FontSize::Medium;
/// Gap between the field and the list
constexpr int listGap = 2;
} // namespace

CDropDown::CDropDown(Position pos, Extent size, Callback callback)
    : pos_(pos), size_(size), callback_(std::move(callback))
{
    // Square button with an arrow on the right side
    arrowButton_ = std::make_unique<CButton>(nullptr, 0, pos_ + Position(size_.x - size_.y, 0),
                                             Extent(size_.y, size_.y), BUTTON_GREY, nullptr, PICTURE_SMALL_ARROW_DOWN);
    setItems({}, 0);
}

CDropDown::~CDropDown() = default;

void CDropDown::setItems(std::vector<std::string> items, unsigned selection)
{
    items_ = std::move(items);
    selection_ = selection;

    // The list is as high as needed for its items but limited to a maximum number of lines
    const auto numLines = std::min(static_cast<unsigned>(items_.size()), maxListLines);
    list_ = std::make_unique<CSelectBox>(pos_ + Position(0, static_cast<int>(size_.y) + listGap),
                                         Extent(size_.x, CSelectBox::getHeightForLines(numLines, fontSize)), fontSize,
                                         FontColor::Yellow, BUTTON_GREY);
    const auto onClick = [this](int index) noexcept { clickedItem_ = static_cast<unsigned>(index); };
    for(unsigned i = 0; i < items_.size(); i++)
        list_->addOption(items_[i], onClick, static_cast<int>(i));
}

bool CDropDown::isInList(Position mousePos) const
{
    return isPointInRect(mousePos, Rect(list_->getPos(), list_->getSize()));
}

void CDropDown::setMouseData(const SDL_MouseMotionEvent& motion)
{
    arrowButton_->setMouseData(motion);
    if(open_)
        list_->setMouseData(motion);
}

bool CDropDown::setMouseData(const SDL_MouseButtonEvent& button)
{
    arrowButton_->setMouseData(button);
    if(button.button != SDL_BUTTON_LEFT)
        return false;

    const Position mousePos(button.x, button.y);
    if(button.state == SDL_PRESSED && isPointInRect(mousePos, Rect(pos_, size_)))
    {
        // Clicked on the field or the arrow button -> Toggle the list.
        // Not consumed so other drop-downs close their lists.
        open_ = !open_;
        return false;
    }
    if(!open_)
        return false;
    if(button.state == SDL_PRESSED && !isInList(mousePos))
    {
        // Like ctrlComboBox::Msg_LeftDown: Clicked somewhere else -> Close the list
        open_ = false;
        return false;
    }

    const bool consumed = isInList(mousePos);
    list_->setMouseData(button);
    if(!clickedItem_)
        return consumed;

    // Handle the selection after the list finished handling the event as the callback might replace the list
    open_ = false;
    selection_ = *clickedItem_;
    clickedItem_.reset();
    if(callback_)
        callback_(selection_);
    return true;
}

bool CDropDown::setMouseWheel(const SDL_MouseWheelEvent& wheel, Position mousePos)
{
    if(!open_ || !isInList(mousePos))
        return false;
    list_->setMouseWheel(wheel, mousePos);
    return true;
}

void CDropDown::draw(Position parentOrigin) const
{
    const Position absPos = parentOrigin + pos_;

    // Sunken field with the selected item left of the arrow button
    drawButtonBox(Rect(absPos, Extent(size_.x - size_.y, size_.y)), true, BUTTON_GREY_BACKGROUND, BUTTON_GREY_DARK);
    if(selection_ < items_.size())
    {
        const int textY = (static_cast<int>(size_.y) - static_cast<int>(fontSize)) / 2;
        CFont::draw(items_[selection_], absPos + Position(4, textY), fontSize, FontColor::Yellow, FontAlign::Left);
    }
    arrowButton_->draw(parentOrigin);

    if(open_)
        list_->draw(parentOrigin);
}
