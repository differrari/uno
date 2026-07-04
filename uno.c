#include "uno.h"
#include "data/struct/chunk_array.h"
#include "syscalls/syscalls.h"
#include "input_keycodes.h"
#include "memory/memory.h"
#include "math/math.h"
#include "utils/clipboard.h"

void (*view_build_func)();
document_data default_doc_data;
gpu_rect default_canvas;
document_node* current_node;

document_node* focused_node;
int focused_tag;

chunk_array_t *node_stack;

document_node* uno_make_view(node_info info){
    document_node *node = zalloc(sizeof(document_node));
    // print("Alloc %llx",node);
    node->info = info;
    return node;
}

void uno_attach(document_node *parent, document_node *child){
    if (!parent || !child) return;
    if (!parent->children) parent->children = linked_list_create();
    // print("Child now %llx",&parent->children);
    linked_list_push(parent->children, child);
}

void uno_state_push(document_node *new_node){
    if (!node_stack){
        node_stack = chunk_array_create(sizeof(uptr), 64);
    }
    if (current_node){
        chunk_array_push(node_stack, &current_node);
    }
    current_node = new_node;
}

void uno_state_pop(){
    size_t count = chunk_array_count(node_stack);
    if (count){
        current_node = (document_node*)*(uptr*)chunk_array_get(node_stack,chunk_array_count(node_stack)-1);
        chunk_array_remove(node_stack, 1);
    }
}

document_node* uno_begin_vertical(node_info info){
    info.general_type = doc_gen_layout;
    info.type = doc_layout_vertical;
    document_node* vert = uno_make_view(info);
    if (!default_doc_data.root){
        default_doc_data.root = vert;
    }
    if (current_node)
        uno_attach(current_node, vert);
    uno_state_push(vert);
    return vert;
}

void uno_end_vertical(){
    return uno_state_pop();
}

document_node* uno_begin_horizontal(node_info info){
    info.general_type = doc_gen_layout;
    info.type = doc_layout_horizontal;
    document_node* horiz = uno_make_view(info);
    if (!default_doc_data.root){
        default_doc_data.root = horiz;
    }
    if (current_node)
        uno_attach(current_node, horiz);
    uno_state_push(horiz);
    return horiz;
}

void uno_end_horizontal(){
    uno_state_pop();
}

document_node* uno_begin_depth(node_info info){
    info.general_type = doc_gen_layout;
    info.type = doc_layout_depth;
    document_node* depth = uno_make_view(info);
    if (!default_doc_data.root){
        default_doc_data.root = depth;
    }
    if (current_node)
        uno_attach(current_node, depth);
    uno_state_push(depth);
    return depth;
}

void uno_end_depth(){
    uno_state_pop();
}

document_node* uno_create_view(node_info info, string_slice content){
    document_node* node = uno_make_view(info);
    node->content = content;
    if (!default_doc_data.root){
        default_doc_data.root = node;
    }
    if (current_node)
        uno_attach(current_node, node);
    return node;
}

document_node* uno_create_empty_view(node_info info){
    document_node* node = uno_make_view(info);
    if (!default_doc_data.root)
        default_doc_data.root = node;
    if (current_node)
        uno_attach(current_node, node);
    return node;
}

void uno_destroy_node(void *ptr){
    document_node* node = ptr;
    if (!node) return;
    if (node->children){
        linked_list_for_each(node->children, uno_destroy_node);
        // linked_list_destroy(node->children);
        node->children = 0;
    }
    // print("Release %llx",node);
    release(node);
}

void set_document_view(void (*view_builder)(), gpu_rect canvas){
    view_build_func = view_builder;
    default_canvas = canvas;
    uno_refresh();
}

document_data get_current_document_view(){
    return default_doc_data;
}

void uno_refresh(){
    chunk_array_reset(node_stack);
    uno_destroy_node(default_doc_data.root);
    default_doc_data.root = 0;
    current_node = 0;
    if (view_build_func) view_build_func();
    uno_refresh_layout();
    if (focused_tag) uno_focus(focused_tag);
}

void uno_refresh_layout(){
    layout_document(default_canvas, default_doc_data);
}

void uno_draw(draw_ctx *ctx){
    render_document(ctx, default_doc_data);
}

document_node* uno_find_node(document_node *node, int tag){
    if (!tag || !node) return 0;
    if (node->input.tag == tag){
        return node;
    }
    else if (node->children){
        for (linked_list_node_t *child = node->children->head; child; child = child->next){
            if (child && child->data){
                document_node* ret = uno_find_node(child->data, tag);
                if (ret) return ret;
            }
        }
    }
    return 0;
}

void uno_focus(int tag){
    focused_node = 0;
    if (!tag)
        return;
    document_node *node = default_doc_data.root;
    focused_node = uno_find_node(node, tag);
    if (focused_node) focused_tag = tag;
}

void uno_copy(void* ctx){
    if (focused_node && focused_node->input.on_copy){
        size_t size = 0;
        void *buf = focused_node->input.on_copy(focused_node, &size);
        if (size)
            clipboard_copy(buf, size, supported_data(focused_node->info.general_type));
    }
}

void uno_paste(void* ctx){
    if (focused_node && focused_node->input.on_paste){
        size_t size = 0;
        void *buf = clipboard_paste(supported_data(focused_node->info.general_type), &size);
        focused_node->input.on_paste(focused_node, buf, size);
    }
}

bool mouse_in_node(document_node *node, mouse_data data){
    if (data.position.x < node->info.rect.point.x || 
        data.position.x > node->info.rect.point.x + node->info.rect.size.width || 
        data.position.y < node->info.rect.point.y || 
        data.position.y > node->info.rect.point.y + node->info.rect.size.height) return false;
    return true;
}

bool uno_button_click(document_node *node, mouse_data data, u8 modifier){
    if (!node) return false;
    if (node->info.general_type != doc_gen_button) return false;
    button_info *info = node->ctx;
    if (!info) return false;
    if (mouse_button_down(&data, LMB)){
        if (info->press) info->press(node->input.tag, data.position);
    } else if (info->hover) info->hover(node->input.tag, data.position);//TODO: for hover to work, we need to disable click-only in uno_dispatch_mouse, which can get costly
        
    return true;
}

void uno_button(int tag, node_info info, button_info *b_info, string_slice label){
    info.general_type = doc_gen_button;
    if (info.type == doc_text_none) info.type = doc_text_body;    
    if (info.sizing_rule == size_none) info.sizing_rule = size_fit;
    if (!((info.fg_color >> 24) & 0xFF)) info.fg_color |= 0xFF << 24;
    document_node *node = uno_create_view(info, label);
    node->input.mouse_input = uno_button_click;
    node->input.tag = tag;
    node->ctx = b_info;
    node->content = label;
}

void uno_label(node_info info, doc_text_size size, string_slice content){
    info.general_type = doc_gen_text;
    info.type = size;
    if (info.type == doc_text_none) info.type = doc_text_body;    
    if (info.sizing_rule == size_none) info.sizing_rule = size_fit;
    if (!((info.fg_color >> 24) & 0xFF)) info.fg_color |= 0xFF << 24;
    uno_create_view(info, content);
}

bool uno_dispatch_kbd(kbd_event ev, u8 modifier){
    if (!focused_node) return false;//TODO: find input automatically?
    
    if (focused_node->input.keyboard_input)
        return focused_node->input.keyboard_input(focused_node, ev, modifier);
    
    return false;
}

bool find_mouse_item(document_node *node, mouse_data data, u8 modifier){
    if (!node) return false;
    if (node->children){
        for (linked_list_node_t *n = node->children->head; n; n = n->next)
            if (find_mouse_item(n->data, data, modifier)) return true;
    }
    if (mouse_in_node(node, data)){
        if (node->input.mouse_input) return node->input.mouse_input(node, data, modifier);
    }
    return false;
}

static bool clicked = false;
static mouse_data last_mouse_stat; 

bool uno_dispatch_mouse(mouse_data mouse, u8 modifier){
    if (memcmp(&mouse, &last_mouse_stat, sizeof(mouse)) == 0) return false;
    last_mouse_stat = mouse;
    if (mouse_button_down(&mouse, 0)) clicked = false;
    if (mouse_button_down(&mouse, 0)){
        if (clicked) mouse.raw.buttons &= ~0;
        clicked = true;
    }
    // if (!focused_node){
    //     //TODO: focus
    // }
    
    if (find_mouse_item(default_doc_data.root, mouse, modifier)) return true;
    
    if (focused_node && focused_node->input.mouse_input)
        return focused_node->input.mouse_input(focused_node,mouse, modifier);
    
    return false;
}