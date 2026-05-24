#pragma once

#include "data/struct/linked_list.h"
#include "string/slice.h"
#include "draw/draw.h"
#include "keyboard_input.h"
#include "mouse_input.h"
#include "files/data_signatures.h"

typedef enum {
    leading,
    horizontal_center,
    trailing,
} horizontal_alignment;

typedef enum {
    top,
    bottom,
    vertical_center,
} vertical_alignment;  

typedef enum { doc_text_none, doc_text_body, doc_text_title, doc_text_subtitle, doc_text_heading, doc_text_subheading, doc_text_footnote, doc_text_caption } doc_text_size;
typedef enum { doc_layout_none, doc_layout_vertical, doc_layout_horizontal, doc_layout_depth } doc_layout_types;
typedef enum { doc_gen_type_none, doc_gen_text, doc_gen_layout, doc_gen_button } doc_gen_type;

typedef enum { 
    size_none, //No rules for size
    size_fit, //Fit to content
    size_fill, //Fill parent
    size_relative, //Percentage of parent
    size_absolute //Absolute positioning within parent
} size_rule;

typedef struct {
    int type;
    doc_gen_type general_type;
    
    color bg_color;
    color fg_color;
    
    size_rule sizing_rule;
    gpu_rect rect;
    
    u32 padding;
    float percentage;//TODO: x,y
    
    horizontal_alignment horiz_alignment;
    vertical_alignment vert_alignment;
    
    gpu_point offset;
    
    wrap_policy text_wrap_policy;
    
    text_format_arr text_formatting;
    
} node_info;

typedef struct document_node document_node;

typedef struct {
    int tag;
    bool (*keyboard_input)(document_node* node,kbd_event event, u8 modifier);
    bool (*mouse_input)(document_node*,mouse_data, u8 modifier);
    void* (*on_copy)(document_node*,size_t *out_size);
    bool (*on_paste)(document_node*,void* buf, size_t data);
} uno_input_info;

typedef struct document_node {
    node_info info;
    linked_list_t *children;
    string_slice content;
    uno_input_info input;
    void *ctx;
} document_node;

typedef struct {
    document_node *root;
} document_data;

void layout_document(gpu_rect canvas, document_data doc);
void render_document(draw_ctx *ctx, document_data doc);
void debug_document(document_data doc);

int text_to_scale(doc_text_size type);
data_signature supported_data(doc_gen_type type);