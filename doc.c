#include "doc.h"
#include "files/buffer.h"
#include "graphic_types.h"
#include "syscalls/syscalls.h"
#include "math/math.h"
#include "draw/textdraw.h"
#include "uno.h"
#include "data/struct/bt_tree.h"

typedef struct {
    gpu_rect canvas;
    doc_layout_types direction;
} doc_layout;

typedef struct {
    bool force_newline;
} doc_layout_result;

#define draw_text(ctx, rect) ({\
text_draw_result _r = {};\
if (node->info.text_formatting.array_type != fmt_array_none){\
    _r = fb_draw_text(ctx, node->content, rect, (node->info.offset ? *node->info.offset : (gpu_point){}), (text_format){.scale = text_size, .foreground = node->info.fg_color, .wrap = node->info.text_wrap_policy }, node->info.text_formatting);\
} else {\
    _r = fb_draw_single_text(ctx, node->content, rect, (node->info.offset ? *node->info.offset : (gpu_point){}),  (text_format){.scale = text_size, .foreground = node->info.fg_color, .wrap = node->info.text_wrap_policy });\
}\
_r;\
});

int text_to_scale(doc_text_size type){
    switch (type) {
        case doc_text_body:          return 3;
        case doc_text_title:         return 7;
        case doc_text_subtitle:      return 6;
        case doc_text_heading:       return 5;
        case doc_text_subheading:    return 4;
        case doc_text_footnote:      return 3;
        case doc_text_caption:       return 2;
        case doc_text_none:          return 0;
    }
}

data_signature supported_data(doc_gen_type type){
    switch (type) {
        case doc_gen_text: return DATA_SIG_TEXT;
        default: return DATA_SIG_UNKNOWN;
    }
}

int text_force_newline(doc_text_size type){
    return false;
}

static inline float layout_get_size(doc_layout_types direction, gpu_rect rect){
    if (direction == doc_layout_horizontal) return rect.size.width;
    if (direction == doc_layout_vertical || direction == doc_layout_depth) return rect.size.height;
    return 0;
}

static inline float layout_get_offset(doc_layout_types direction, gpu_point *point){
    if (!point) return 0;
    if (direction == doc_layout_horizontal) return -point->x;
    if (direction == doc_layout_vertical || direction == doc_layout_depth) return point->y;
    return 0;
}

static inline void layout_set_size(doc_layout_types direction, doc_layout *layout, float size){
    if (direction == doc_layout_horizontal) layout->canvas.size.width = size;
    if (direction == doc_layout_vertical) layout->canvas.size.height = size;
}

void layout_update_parent_with_child(doc_layout *layout, document_node *node, document_node *child, doc_layout_result layout_result){
    if (layout->direction == doc_layout_horizontal && !layout_result.force_newline){
        // layout->canvas.point.x += child->info.rect.size.width;
        if (node->info.sizing_rule == size_fit){
            node->info.rect.size.width += child->info.rect.size.width;
            node->info.rect.size.height = max(node->info.rect.size.height,child->info.rect.size.height);
        }
    } else if (layout->direction != doc_layout_depth){
        // layout->canvas.point.y += child->info.rect.size.height;
        if (node->info.sizing_rule == size_fit){
            node->info.rect.size.height += child->info.rect.size.height;
            node->info.rect.size.width = max(node->info.rect.size.width,child->info.rect.size.width);
        }
    } else if (node->info.sizing_rule == size_fit){
        node->info.rect.size.height = max(node->info.rect.size.height,child->info.rect.size.height);
        node->info.rect.size.width = max(node->info.rect.size.width,child->info.rect.size.width);
    }
}

doc_layout_result layout_doc_node(doc_layout layout, document_data doc, document_node *node){
    doc_layout_result result = {};
    if (!node) return result;
    
    if (node->info.general_type == doc_gen_layout && node->info.type != doc_layout_none){
        layout.direction = node->info.type;
    }

    if (node->info.sizing_rule == size_fill || node->info.sizing_rule == size_relative)
        node->info.rect.size = layout.canvas.size;

    if (!node->info.use_absolute_position && node->info.sizing_rule != size_absolute){
        layout.canvas.size.width -= node->info.padding * 2;
        layout.canvas.size.height -= node->info.padding * 2;    
    }
    float total_size = layout_get_size(layout.direction, layout.canvas);
    float overflow = layout_get_offset(layout.direction, node->info.offset);
    if (node->children){
        float remaining_size = total_size+overflow;
        int remaining_children = 0;
        for (linked_list_node_t *n = node->children->head; n; n = n->next){
            if (!n->data) break;
            if (remaining_size <= 0) break;
            document_node *child = n->data;
            if (child->info.sizing_rule != size_fill){
                doc_layout new_layout = layout;
                float allocd_size = 0;
                if (child->info.sizing_rule == size_relative){
                    if (child->info.percentage < 0 || child->info.percentage > 1) continue;
                    allocd_size = floor(total_size*child->info.percentage);
                    if (remaining_size < allocd_size) allocd_size = remaining_size;
                    layout_set_size(layout.direction, &new_layout, allocd_size);
                    remaining_size -= allocd_size;
                    doc_layout_result layout_result = layout_doc_node(new_layout, doc, child);
                    layout_update_parent_with_child(&layout, node, child, layout_result);
                } else if (child->info.sizing_rule == size_fit){
                    allocd_size = remaining_size;
                    layout_set_size(layout.direction, &new_layout, allocd_size);
                    doc_layout_result layout_result = layout_doc_node(new_layout, doc, child);
                    remaining_size -= layout_get_size(layout.direction, child->info.rect);
                    layout_update_parent_with_child(&layout, node, child, layout_result);
                } else if (child->info.sizing_rule == size_absolute){
                    layout_doc_node(new_layout, doc, child);
                }
            } else remaining_children++;
        }
        for (linked_list_node_t *n = node->children->head; n; n = n->next){
            if (!n->data) break;
            if (remaining_size <= 0) break;
            document_node *child = n->data;
            if (child->info.sizing_rule != size_fill) continue;
            doc_layout new_layout = layout;
            float view_size = remaining_size/remaining_children;
            // print("Fill view size %f. Direction %",view_size);
            layout_set_size(layout.direction, &new_layout, view_size);
            doc_layout_result layout_result = layout_doc_node(new_layout, doc, child);
            layout_update_parent_with_child(&layout, node, child, layout_result);
        }
    }
    if (node->content.length){
        int text_size = text_to_scale(node->info.type);
        if (!text_size) return (doc_layout_result){};
        draw_ctx ctx = {.width = layout.canvas.size.width - (node->info.padding * 2),.height = layout.canvas.size.height - (node->info.padding * 2)};
        text_draw_result res = draw_text(&ctx, layout.canvas);
        // print("Label size %ix%i",res.size.width,res.size.height);
        res.size.width += node->info.padding * 2;
        res.size.height += node->info.padding * 2;
        //TODO better absolute positioning absolute 
        if (node->info.sizing_rule == size_fit) node->info.rect.size = res.size;
        result.force_newline = text_force_newline(node->info.type);
    }
    
    return result;
}

void layout_doc_node_pos(doc_layout layout, document_node *node){
    if (!node) return;
    
    if (node->info.sizing_rule != size_fill && !node->info.use_absolute_position){
        switch (node->info.horiz_alignment) {
        case leading:
            node->info.rect.point.x = layout.canvas.point.x; break;
        case horizontal_center:
            node->info.rect.point.x = layout.canvas.point.x + (layout.canvas.size.width-node->info.rect.size.width)/2.f; break;
        case trailing:
            node->info.rect.point.x = layout.canvas.point.x + (layout.canvas.size.width-node->info.rect.size.width); break;
          break;
        }
        switch (node->info.vert_alignment) {
        case top:
            node->info.rect.point.y = layout.canvas.point.y; break;
        case vertical_center:
            node->info.rect.point.y = layout.canvas.point.y + (layout.canvas.size.height-node->info.rect.size.height)/2.f; break;
        case bottom:
            node->info.rect.point.y = layout.canvas.point.y + (layout.canvas.size.height-node->info.rect.size.height); break;
          break;
        }
    } else if (node->info.use_absolute_position){
        node->info.rect.point.x += layout.canvas.point.x;
        node->info.rect.point.y +=  layout.canvas.point.y;
    } else node->info.rect.point = (gpu_point){ layout.canvas.point.x, layout.canvas.point.y };

    if (node->info.general_type == doc_gen_layout && node->info.type != doc_layout_none){
        layout.direction = node->info.type;
    }

    if (node->info.general_type != doc_gen_text && node->info.offset){
        layout.canvas.point.x += node->info.offset->x;
        layout.canvas.point.y += node->info.offset->y;
    }
    
    layout.canvas.point.x += node->info.padding;
    layout.canvas.point.y += node->info.padding;
    layout.canvas.size.width = node->info.rect.size.width - node->info.padding;
    layout.canvas.size.height = node->info.rect.size.height - node->info.padding;
    
    if (node->children)
        for (linked_list_node_t *n = node->children->head; n; n = n->next){
            if (!n->data) continue;
            document_node *child = n->data;
            layout_doc_node_pos(layout, child);
            if (child->info.sizing_rule != size_absolute && layout.direction != doc_layout_depth){
                if (layout.direction == doc_layout_horizontal)
                    layout.canvas.point.x += child->info.rect.size.width;
                else if (layout.direction == doc_layout_vertical)
                    layout.canvas.point.y += child->info.rect.size.height;
            }
        }
}

void layout_document(gpu_rect canvas, document_data doc){
    doc_layout layout = (doc_layout){.canvas = canvas};
    layout_doc_node(layout, doc, doc.root);
    layout_doc_node_pos(layout, doc.root);
}

void render_text_section(draw_ctx *ctx, range_t string_range, string_slice slice, gpu_rect rect, gpu_point scroll, text_draw_result *result, text_format default_format, text_format_arr text_formatting){
    fb_continuous_draw_text(ctx, false, &result->cursor, slice, &string_range, rect, &result->size, scroll, default_format, text_formatting);
    if (ctx->fb) mark_dirty(ctx, rect.point.x, rect.point.y, result->size.width, result->size.height);
}

void render_piece_tree(draw_ctx *ctx, text_field_info *info, document_node *doc_node, gpu_rect rect, gpu_point scroll, text_draw_result *result, text_format default_format, text_format_arr text_formatting){
    bt_tree_traversal traversal = {
        .tree = info->piece_tree
    };

    bt_node *node = 0;
    while ((node = bt_tree_next(&traversal))){
        uno_text_piece *piece = (uno_text_piece*)node->data;
        if (piece->buffer_index >= 2) continue;
        // print("Rendering %x from buffer %i at %r %i",(uptr)piece,piece->buffer_index,piece->range,info->piece_tree->data_size);
        fb_continuous_draw_text(ctx, false, &result->cursor, slice_from_buffer(info->children[piece->buffer_index]), &piece->range, rect, &result->size, scroll, default_format, text_formatting);
        if (ctx->fb) mark_dirty(ctx, rect.point.x, rect.point.y, result->size.width, result->size.height);
    }
    
    for (int i = 0; i < min(info->cursor_count,info->cursor_limit); i++){
        if (info->cursor_color) fb_fill_rect(ctx, rect.point.x + info->cursors[i] * fb_char_width(default_format.scale), rect.point.y, 3, fb_line_height(default_format.scale), info->cursor_color);
    }
}

void render_doc_node(draw_ctx *ctx, document_node *node){
    if (!node) return;
    if (node->info.bg_color){
        fb_fill_rect(ctx, node->info.rect.point.x + node->info.padding, node->info.rect.point.y + node->info.padding, node->info.rect.size.width - (node->info.padding*2), node->info.rect.size.height - (node->info.padding*2), node->info.bg_color);
    }
    if (node->children)
        for (linked_list_node_t *n = node->children->head; n; n = n->next){
            if (!n->data) break;
            render_doc_node(ctx,n->data);
        }
    if (node->content.length){
        int text_size = text_to_scale(node->info.type);
        gpu_rect rect = (gpu_rect){
            {
                node->info.rect.point.x + node->info.padding,
                node->info.rect.point.y + node->info.padding
            }, {
                node->info.rect.size.width - node->info.padding*2,
                node->info.rect.size.height - node->info.padding*2
            }
        };

        if (node->info.general_type == doc_gen_text && node->ctx){
            text_field_info *in = node->ctx;

            if (((in->piece_tree && !in->piece_tree->count) || (!in->piece_tree && in->content->buffer_size == 0)) && in->placeholder.length){
                draw_text(ctx, rect);
                return;
            }

            string_slice slice = { .data = in->content->buffer, in->content->buffer_size};
            text_draw_result result = {};

            if (!in->piece_tree){
                render_text_section(ctx,(range_t){.start = 0, .size = in->content->cursor},slice, rect, node->info.offset ? *node->info.offset : (gpu_point){}, &result, (text_format){.scale = text_size, .foreground = node->info.fg_color, .wrap = node->info.text_wrap_policy }, node->info.text_formatting);

                // if (in->gap) //TODO: put the gap buffer here
                if (in->cursor_color) fb_fill_rect(ctx, rect.point.x + result.cursor.x, rect.point.y + result.cursor.y, 3, fb_line_height(text_size), in->cursor_color);

                render_text_section(ctx,(range_t){.start = in->content->cursor, .size = in->content->buffer_size - in->content->cursor},slice, rect, node->info.offset ? *node->info.offset : (gpu_point){}, &result, (text_format){.scale = text_size, .foreground = node->info.fg_color, .wrap = node->info.text_wrap_policy }, node->info.text_formatting);
            } else {
                render_piece_tree(ctx, in, node,  rect, node->info.offset ? *node->info.offset : (gpu_point){}, &result, (text_format){.scale = text_size, .foreground = node->info.fg_color, .wrap = node->info.text_wrap_policy }, node->info.text_formatting);
            }

        } else draw_text(ctx, rect);
    }
}

void render_document(draw_ctx *ctx, document_data doc){
    render_doc_node(ctx, doc.root);
}

#include "utils/indent.h"

void debug_node(document_node *node, int depth){
    if (!node) return;
    print("%sNode %ix%i - %ix%i - %i - %v",indent_by(depth),node->info.rect.point.x,node->info.rect.point.y,node->info.rect.size.width,node->info.rect.size.height,node->info.sizing_rule,(string_slice){.data = node->content.data, min(node->content.length, 16)});
    if (node->children)
        for (linked_list_node_t *n = node->children->head; n; n = n->next){
            if (!n->data) break;
            debug_node(n->data, depth+1);
        }
}

void debug_document(document_data doc){
    debug_node(doc.root, 0);
}