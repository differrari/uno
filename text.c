#include "doc.h"
#include "uno.h"
#include "math/math.h"
#include "input_keycodes.h"
#include "draw/textdraw.h"

void uno_text_field_shift_cursor_node(document_node *node, i32 x_shift, i32 y_shift);
extern document_node* uno_find_node(document_node *node, int tag);

void uno_text_field_scroll_in_line(document_node *node, bool begin){
    if (!node || !node->ctx) return;
    text_field_info *info = node->ctx;
    buffer *content = info->content;
    bool found = false;
    if (begin){
        for (u64 i = content->cursor; i > 0; i--)
            if (((char*)content->buffer)[i-1] == '\n'){ found = true; content->cursor = i; break; }
        if (!found) content->cursor = 0;
    } else {
        if (((char*)content->buffer)[content->cursor] == '\n'){ return; }
        for (u64 i = content->cursor; i < content->buffer_size; i++)
            if (((char*)content->buffer)[i+1] == '\n'){ found = true; content->cursor = i+1; break; }
        if (!found) content->cursor = content->buffer_size;
    }
    content->cursor = clamp(content->cursor, 0, content->buffer_size);
    uno_refresh();
}

bool uno_text_field_input(document_node *node, kbd_event event, u8 modifier){
    if (!node || !node->ctx) return false;
    text_field_info *info = node->ctx;
    buffer *content = info->content;
    if (!content || !content->buffer) return false;
    if (event.key == KEY_ENTER && !info->multiline) return false;
    if (event.type != KEY_PRESS) return false;
    if (event.key == KEY_HOME)
        uno_text_field_scroll_in_line(node, true);
    if (event.key == KEY_END)
        uno_text_field_scroll_in_line(node, false);
    else if (event.type == KEY_PRESS && ((event.key >= KEY_RIGHT && event.key <= KEY_UP) || event.key == KEY_PAGEUP || event.key == KEY_PAGEDOWN )){
        i32 x_shift = 0;
        i32 y_shift = 0;
        switch (event.key) {
            case KEY_RIGHT: x_shift = 1; break;
            case KEY_LEFT:  x_shift = -1; break;
            case KEY_DOWN:  y_shift = 1; break;
            case KEY_UP:    y_shift = -1; break;
            case KEY_PAGEDOWN:  
                // y_shift = (ctx.height/line_height)-1;
                // offset.y -= ctx.height;
                break;
            case KEY_PAGEUP: 
                // y_shift = -(ctx.height/line_height)-1;
                // offset.y += ctx.height;
                break;
        }
        if (x_shift || y_shift)
            uno_text_field_shift_cursor_node(node, x_shift, y_shift);
        uno_refresh();
    }
    if (event.key == KEY_BACKSPACE){
        if (info->selection.start || info->selection.end){
            buffer_delete(content, info->selection.end, info->selection.end-info->selection.start);
            info->selection.start = 0;
            info->selection.end = 0;
        } else 
            buffer_delete(content, content->cursor, 1);
        uno_refresh();
        return true;
    }
    if (event.key == KEY_DELETE){
        if (info->selection.start || info->selection.end){
            buffer_delete(content, info->selection.end, info->selection.end-info->selection.start);
            info->selection.start = 0;
            info->selection.end = 0;
        } else {
            buffer_delete(content, content->cursor+1, 1);
        }
        uno_refresh();
        return true;
    }
    if (event.key == KEY_TAB){
        char *indent = "\t\t\t\t";
        buffer_write_to(content, indent, 4, content->cursor);
        uno_refresh();
        return true;
    }
    char c = hid_to_char(event.key, modifier, 0);
    if (event.type == KEY_PRESS && c){
        buffer_write_to(content, &c, 1, content->cursor);
        uno_refresh();
        return true;
    }
    
    return false;
}

void uno_text_field_scroll_node(document_node *node, i32 x_shift, i32 y_shift);

bool uno_text_field_mouse(document_node *node, mouse_data data, u8 modifier){
    bool res = false;
    if (data.position.x < node->info.rect.point.x || 
        data.position.x > node->info.rect.point.x + node->info.rect.size.width || 
        data.position.y < node->info.rect.point.y || 
        data.position.y > node->info.rect.point.y + node->info.rect.size.height) return false;
    if (data.raw.scroll){
        if (modifier & KEY_MOD_LSHIFT){
            uno_text_field_scroll_node(node, data.raw.scroll, 0);
        } else {        
            uno_text_field_scroll_node(node, 0, data.raw.scroll);
        }
        uno_refresh();
        res = true;
    }
    if (!mouse_button_down(&data, 0)) return res;
    text_field_info *info = node->ctx;
    if (!info) return false;
    buffer *content = info->content;
    if (!content || !content->buffer) return res;
    
    i32 x = data.position.x/fb_get_char_size(text_to_scale(node->info.type));
    i32 y = data.position.y/(fb_get_char_size(text_to_scale(node->info.type)) + 2);//TODO: line padding should be customizable
    float oy = (float)node->info.offset.y/((i32)fb_get_char_size(text_to_scale(node->info.type)) + 2.f);
    
    float ox = (float)node->info.offset.x/(i32)fb_get_char_size(text_to_scale(node->info.type));

    x -= round_to_int(ox);
    y -= round_to_int(oy);
    u32 selection = lin_col_to_pos(y, x, (string_slice){content->buffer,content->buffer_size});
    
    if (modifier & KEY_MOD_LSHIFT){
        info->selection.start = min(content->cursor,selection);
        info->selection.end   = max(content->cursor,selection);
    } else {
        info->selection.start = 0;
        info->selection.end = 0;
    }
    content->cursor = selection;
    uno_refresh();
    return true;
}

void* uno_text_field_copy(document_node* node, size_t *out_size){
    if (!node || !node->ctx || !out_size) return false;
    text_field_info *info = node->ctx;
    buffer *content = info->content;
    if (!content || !content->buffer) return false;
    *out_size = info->selection.end-info->selection.start;
    return &info->content->buffer[info->selection.start];
}

bool uno_text_field_paste(document_node* node, void* buf, size_t size){
    if (!buf || !size) return false;
    if (!node || !node->ctx) return false;
    text_field_info *info = node->ctx;
    buffer *content = info->content;
    if (!content || !content->buffer) return false;
    for (size_t i = 0; i < size-1; i++)
        if (!is_printable(((char*)buf)[i]))
            return false;
    buffer_write_to(content, buf, size, content->cursor);
    uno_refresh();
    return true;
}

document_node* uno_text_field(int tag, node_info info, text_field_info *text_info){
    info.general_type = doc_gen_text;
    if (info.type == doc_text_none) info.type = doc_text_footnote;
    if (!((info.fg_color >> 24) & 0xFF)) info.fg_color |= 0xFF << 24;
    
    info.offset = text_info->offset;
    uno_begin_depth(info);
    node_info text_node_info = info;
    text_node_info.sizing_rule = size_fill;
    info.general_type = doc_gen_text;
    document_node *node = uno_create_view(text_node_info, text_info->content && text_info->content->buffer && text_info->content->buffer_size ? (string_slice){.data = text_info->content->buffer, .length = text_info->content->buffer_size } : text_info->placeholder);
    node->input.keyboard_input = uno_text_field_input;
    node->input.mouse_input = uno_text_field_mouse;
    node->input.on_copy = uno_text_field_copy;
    node->input.on_paste = uno_text_field_paste;
    node->input.tag = tag;
    
    node->ctx = text_info;
    
    i32 lin, col = 0;
    pos_to_lin_col(text_info->content->cursor, (string_slice){text_info->content->buffer,text_info->content->buffer_size}, &lin, &col);

    u32 scale = text_to_scale(info.type);
    
    u32 cw = fb_char_width(scale);
    u32 lh = fb_line_height(scale);
    u32 ls = fb_get_line_spacing(scale);
    u32 ch = lh - ls;
    
    document_node *cursor = uno_create_view((node_info){.bg_color = text_info->selection.start || text_info->selection.end ? 0xFF555555 : text_info->cursor_color, .sizing_rule = size_absolute, .rect = (gpu_rect){(col * cw),(lin * lh), 3, ch}, .use_absolute_position = true}, (string_slice){});
    
    uno_end_depth();

    return node;
}

void uno_text_field_scroll_node(document_node *node, i32 x_shift, i32 y_shift){
    text_field_info *info = node->ctx;
    if (!info) return;
    buffer *content = info->content;
    if (!content || !content->buffer) return;
    u8 cw = fb_line_height(text_to_scale(node->info.type));
    u8 lh = fb_line_height(text_to_scale(node->info.type));
    if (x_shift != 0){
        if (info->offset.x + x_shift > 0) info->offset.x = 0;
        else info->offset.x += x_shift * cw;
    }
    if (y_shift != 0){
        if (info->offset.y + y_shift > 0) info->offset.y = 0;
        else info->offset.y += y_shift * lh;   
    }
}

void uno_text_field_scroll(int tag, i32 x_shift, i32 y_shift){
    document_node *node = uno_find_node(default_doc_data.root, tag);
    if (!node) return;
    if (node->info.general_type != doc_gen_text) return;
    uno_text_field_scroll_node(node, x_shift, y_shift);
}

void uno_text_field_shift_cursor_node(document_node *node, i32 x_shift, i32 y_shift){
    text_field_info *info = node->ctx;
    if (!info) return;
    buffer *content = info->content;
    if (!content || !content->buffer) return;
    u8 cw = fb_line_height(text_to_scale(node->info.type));
    u8 lh = fb_line_height(text_to_scale(node->info.type));
    if (x_shift){
        if ((i64)content->cursor + x_shift < 0) content->cursor = 0;
        else content->cursor += x_shift;
        i32 lin, col = 0;
        string_slice slice = (string_slice){content->buffer,content->buffer_size};
        pos_to_lin_col(content->cursor, slice, &lin, &col);
        i32 dis = col + (info->offset.x/cw) - (node->info.rect.size.width/cw);
        if (dis >= 0) uno_text_field_scroll_node(node, -dis, 0);
        else if (-dis > node->info.rect.size.width/cw) uno_text_field_scroll_node(node, -((node->info.rect.size.width/cw)+dis), 0);
    }
    if (y_shift){
        i32 lin, col = 0;
        string_slice slice = (string_slice){content->buffer,content->buffer_size};
        pos_to_lin_col(content->cursor, slice, &lin, &col);
        if (lin + y_shift < 0) lin = 0;
        else lin += y_shift;
        content->cursor = lin_col_to_pos(lin, col, slice);
        i32 dis = lin + (info->offset.y/lh) - (node->info.rect.size.height/lh);
        if (dis >= 0) uno_text_field_scroll_node(node, 0, -dis);
        else if (-dis > node->info.rect.size.height/lh) uno_text_field_scroll_node(node, 0, -((node->info.rect.size.height/lh)+dis));
    }
    content->cursor = clamp(content->cursor, 0, content->buffer_size);
}

void uno_text_field_shift_cursor(int tag, i32 x_shift, i32 y_shift){
    document_node *node = uno_find_node(default_doc_data.root, tag);
    if (!node) return;
    if (node->info.general_type != doc_gen_text) return;
    uno_text_field_shift_cursor_node(node, x_shift, y_shift);
}