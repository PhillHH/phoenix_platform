// ==========================================================================
// FILE: include/phoenix/UI/CircularMenu.h
// Circular radial menu widget for LVGL
// ==========================================================================
#pragma once
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <lvgl.h>
#include <functional>
#include <vector>
#include <cmath>

namespace phoenix::ui {

struct MenuItem {
    String32    label;
    const void* icon_src;
    void      (*onClick)(void* ctx);
    void*       ctx;
    lv_color_t  color;
    String128   a11y_label;
};

class CircularMenu {
public:
    CircularMenu(lv_obj_t* parent, int radius = 160, int item_radius = 48);
    ~CircularMenu();
    void setItems(const MenuItem* items, size_t count);
    void setTitle(const char* title);
    void setCenterIcon(const void* icon_src, lv_color_t color);
    void setAnimationEnabled(bool e) { anim_ = e; }
    lv_obj_t* getRoot() const { return root_; }

private:
    lv_obj_t* root_        = nullptr;
    lv_obj_t* title_lbl_   = nullptr;
    lv_obj_t* center_btn_  = nullptr;
    lv_obj_t* items_[12]   = {};
    size_t    item_count_   = 0;
    int       radius_, item_r_;
    bool      anim_ = true;

    void createItem(const MenuItem& item, float angle, size_t idx);
    void animateIn();
    static void btn_cb(lv_event_t* e);
};

} // namespace phoenix::ui
