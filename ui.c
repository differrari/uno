#include "doc.h"
#include "syscalls/syscalls.h"
#include "uno.h"
#include "input_keycodes.h"
#include "math/math.h"

i32 selected_x = 0;
i32 selected_y = 0;

#define MAX_COLS 3
#define MAX_ROWS 3

buffer text_string = {};
buffer other_text_string = {};

enum { field_none, field_upper, field_lower };

text_field_info info = (text_field_info){&text_string, SLICE_LIT("PLACEHOLDER")};
text_field_info info2 = (text_field_info){ .content = &other_text_string, .placeholder = SLICE_LIT("OTHER PLACEHOLDER")};

void draw_view(){
    if (!text_string.buffer) text_string = buffer_create(0x100, buffer_can_grow);
    if (!other_text_string.buffer) other_text_string = buffer_create(0x100, buffer_can_grow);
    VERTICAL(((node_info){ doc_layout_vertical, doc_gen_layout, .sizing_rule = size_fill, .bg_color = 0xFF123456 + 0x050505 }), {
        uno_text_field(field_upper, (node_info){.sizing_rule = size_relative, .percentage = 0.05f, .bg_color = 0}, &info);
        for (int y = 0; y < MAX_ROWS; y++){
            HORIZONTAL(((node_info){ .type = doc_layout_horizontal, .general_type = doc_gen_layout, .sizing_rule = size_fill}),{
                for (int x = 0; x < MAX_COLS; x++){
                    DEPTH(((node_info){.bg_color = 0xFF123456 + (selected_x == x && selected_y == y ? 0x333333 : 0x111111), .sizing_rule = size_fill, .padding = 4}),{
                        if (selected_x == x && selected_y == y) uno_create_empty_view((node_info){.bg_color = 0xFF123456 + 0x111111, .sizing_rule = size_fill, .padding = 5});
                        uno_create_view((node_info){ .type = doc_text_caption, .general_type = doc_gen_text, .sizing_rule = size_fill, .fg_color = 0xFFFFFFFF, .padding = 5},
                            slice_from_literal("red"));
                        uno_create_view((node_info){ .type = doc_text_title, .general_type = doc_gen_text, .fg_color = 0xFFFFFFFF, .sizing_rule = size_fill,.horiz_alignment = horizontal_center,.vert_alignment = vertical_center},
                            slice_from_string(string_format("%i",(y *3)+x)));
                    });
                }
            });
        }
        
        uno_text_field(field_lower,(node_info){ .sizing_rule = size_relative, .percentage = 0.35f, .bg_color = 0}, &info2);
    });
}

int main(){
    
    draw_ctx ctx = {.width = 1920, .height = 1080};
    
    request_draw_ctx(&ctx);
    
    set_document_view(draw_view, (gpu_rect){ 0,0,ctx.width,ctx.height });
    
    debug_document(default_doc_data);
    
    uno_focus(field_upper);
    
    while (!should_close_ctx()){
        fb_clear(&ctx, 0);
        uno_draw(&ctx);
        commit_draw_ctx(&ctx);
        kbd_event ev = {};
        if (read_event(&ev)){
            if (ev.key == KEY_ESC) return 0;  
            if (ev.type == KEY_PRESS){
                bool changed = false;
                if (!uno_dispatch_kbd(ev, 0)){
                    switch (ev.key) {
                        case KEY_LEFT:  changed = true; selected_x = (selected_x - 1 + MAX_COLS) % MAX_COLS; break;
                        case KEY_RIGHT: changed = true; selected_x = (selected_x + 1) % MAX_COLS; break;
                        case KEY_UP:    changed = true; selected_y = (selected_y - 1 + MAX_ROWS) % MAX_ROWS; break;
                        case KEY_DOWN:  changed = true; selected_y = (selected_y + 1 ) % MAX_ROWS; break;
                        default: break;
                    }
                    if (changed) uno_refresh();
                } 
            }  
        } 
    }
    
    destroy_draw_ctx(&ctx);
    
}