// ==========================================================================
// FILE: src/UI/CircularMenu.cpp
// ==========================================================================
#include "phoenix/UI/CircularMenu.h"
#include <esp_log.h>

namespace phoenix::ui {

CircularMenu::CircularMenu(lv_obj_t* parent, int radius, int item_radius)
    : radius_(radius), item_r_(item_radius)
{
    root_ = lv_obj_create(parent);
    int total = 2 * (radius_ + item_r_ + 16);
    lv_obj_set_size(root_, total, total);
    lv_obj_center(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root_, LV_OPA_0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);

    title_lbl_ = lv_label_create(root_);
    lv_obj_align(title_lbl_, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_color(title_lbl_, lv_color_white(), 0);

    center_btn_ = lv_btn_create(root_);
    lv_obj_set_size(center_btn_, item_r_ * 2, item_r_ * 2);
    lv_obj_center(center_btn_);
    lv_obj_set_style_radius(center_btn_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_btn_, lv_color_hex(0x6A5CFF), 0);
    lv_obj_set_style_shadow_width(center_btn_, 16, 0);
    lv_obj_set_style_shadow_color(center_btn_, lv_color_hex(0x6A5CFF), 0);
    lv_obj_set_style_shadow_opa(center_btn_, LV_OPA_40, 0);
}

CircularMenu::~CircularMenu() {
    if (root_) lv_obj_del(root_);
}

void CircularMenu::setTitle(const char* t) {
    lv_label_set_text(title_lbl_, t);
}

void CircularMenu::setCenterIcon(const void* icon_src, lv_color_t color) {
    lv_obj_set_style_bg_color(center_btn_, color, 0);
    lv_obj_clean(center_btn_);
    lv_obj_t* ic = lv_label_create(center_btn_);
    lv_label_set_text(ic, static_cast<const char*>(icon_src));
    lv_obj_center(ic);
    lv_obj_set_style_text_color(ic, lv_color_white(), 0);
}

void CircularMenu::setItems(const MenuItem* items, size_t count) {
    // Delete old
    for (size_t i = 0; i < item_count_; ++i) {
        if (items_[i]) lv_obj_del(items_[i]);
        items_[i] = nullptr;
    }
    item_count_ = (count > 12) ? 12 : count;
    if (item_count_ == 0) return;

    const float step = 2.0f * 3.14159265f / static_cast<float>(item_count_);
    for (size_t i = 0; i < item_count_; ++i) {
        float angle = -1.5707963f + static_cast<float>(i) * step; // Start top
        createItem(items[i], angle, i);
    }
    if (anim_) animateIn();
}

void CircularMenu::createItem(const MenuItem& item, float angle, size_t idx) {
    lv_coord_t cx = lv_obj_get_width(root_) / 2;
    lv_coord_t cy = lv_obj_get_height(root_) / 2;
    lv_coord_t x = cx + static_cast<lv_coord_t>(radius_ * cosf(angle));
    lv_coord_t y = cy + static_cast<lv_coord_t>(radius_ * sinf(angle));

    lv_obj_t* btn = lv_btn_create(root_);
    lv_obj_set_size(btn, item_r_ * 2, item_r_ * 2);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, item.color, 0);
    lv_obj_set_style_shadow_width(btn, 12, 0);
    lv_obj_set_style_shadow_color(btn, item.color, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    lv_obj_set_pos(btn, x - item_r_, y - item_r_);

    lv_obj_t* lbl = lv_label_create(btn);
    if (item.icon_src) {
        lv_label_set_text(lbl, static_cast<const char*>(item.icon_src));
    } else {
        lv_label_set_text(lbl, item.label.c_str());
    }
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

    // Store callback
    struct CbData { void(*fn)(void*); void* ctx; };
    auto* cbd = new CbData{item.onClick, item.ctx};
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, cbd);

    // Cleanup on delete
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) {
            delete static_cast<CbData*>(lv_event_get_user_data(e));
        }
    }, LV_EVENT_DELETE, cbd);

    // Hover scale
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_set_style_transform_zoom(lv_event_get_target(e), 280, 0);
    }, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_set_style_transform_zoom(lv_event_get_target(e), 256, 0);
    }, LV_EVENT_RELEASED, nullptr);

    items_[idx] = btn;
}

void CircularMenu::btn_cb(lv_event_t* e) {
    struct CbData { void(*fn)(void*); void* ctx; };
    auto* cbd = static_cast<CbData*>(lv_event_get_user_data(e));
    if (cbd && cbd->fn) cbd->fn(cbd->ctx);
}

void CircularMenu::animateIn() {
    for (size_t i = 0; i < item_count_; ++i) {
        lv_obj_t* btn = items_[i];
        if (!btn) continue;

        lv_obj_set_style_opa(btn, LV_OPA_0, 0);
        lv_obj_set_style_transform_zoom(btn, 128, 0);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, btn);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_time(&a, 300);
        lv_anim_set_delay(&a, static_cast<uint32_t>(i) * 50);
        lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
            lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj),
                                 static_cast<lv_opa_t>(v), 0);
        });
        lv_anim_start(&a);

        lv_anim_t s;
        lv_anim_init(&s);
        lv_anim_set_var(&s, btn);
        lv_anim_set_values(&s, 128, 256);
        lv_anim_set_time(&s, 300);
        lv_anim_set_delay(&s, static_cast<uint32_t>(i) * 50);
        lv_anim_set_path_cb(&s, lv_anim_path_overshoot);
        lv_anim_set_exec_cb(&s, [](void* obj, int32_t v) {
            lv_obj_set_style_transform_zoom(
                static_cast<lv_obj_t*>(obj), static_cast<lv_coord_t>(v), 0);
        });
        lv_anim_start(&s);
    }
}

} // namespace phoenix::ui
