#pragma once

#include "types.h"
#include "doc.h"
#include "files/buffer.h"
#include "data/struct/bt_tree.h"

document_node* uno_begin_vertical(node_info info);
void uno_end_vertical();

document_node* uno_begin_horizontal(node_info info);
void uno_end_horizontal();

document_node* uno_begin_depth(node_info info);
void uno_end_depth();

void uno_attach(document_node *parent, document_node *child);

document_node* uno_create_empty_view(node_info info);
document_node* uno_create_view(node_info info, string_slice content);

typedef struct {
    u32 start;
    u32 end;
} select_range;

typedef struct {
    int buffer_index;
    range_t range;
} uno_text_piece;

typedef struct {
    union {
        struct {
            buffer *content;
            buffer *gap;
        };
        buffer *children[2];
    };
    bt_tree *piece_tree;
    string_slice placeholder;
    bool multiline;
    color cursor_color;
    gpu_point *offset;
    select_range selection;
    
    size_t total_size; 
    size_t cursor_count;
    size_t cursor_limit;
    uptr *cursors;
    
} text_field_info;

bt_tree uno_text_make_piece_tree();
void uno_text_piece_identity(text_field_info *info);
void uno_text_add_piece(text_field_info *info, uno_text_piece piece, i64 position);

document_node* uno_text_field(int tag, node_info info, text_field_info *text_info);
void uno_text_field_scroll(int tag, i32 x_shift, i32 y_shift);
void uno_text_field_shift_cursor(int tag, i32 x_shift, i32 y_shift);
bool uno_text_field_mouse(document_node *node, mouse_data data, u8 modifier);
bool uno_text_field_input(document_node *node, kbd_event event, u8 modifier);

typedef struct {
    void (*press)(int buttonIndex, gpu_point location);
    void (*hover)(int buttonIndex, gpu_point location);
} button_info;

void uno_button(int tag, node_info info, button_info *b_info, string_slice label);

void uno_label(node_info info, doc_text_size size, string_slice content);

void uno_set_document_view(void (*view_builder)(), gpu_rect canvas);
document_data get_current_document_view();
void uno_refresh();
void uno_refresh_layout();

bool uno_draw(draw_ctx *ctx);

void uno_focus(int tag);
bool uno_dispatch_kbd(kbd_event ev, u8 modifier);
bool uno_dispatch_mouse(mouse_data mouse, u8 modifier);
void uno_copy(void*);
void uno_paste(void*);

#define VERTICAL(info, children) uno_begin_vertical((info)); children; uno_end_vertical();
#define HORIZONTAL(info, children) uno_begin_horizontal((info)); children; uno_end_horizontal();
#define DEPTH(info, children) uno_begin_depth((info)); children; uno_end_depth();

extern document_data default_doc_data;