#include "syscalls/syscalls.h"
#include "alloc/allocate.h"
#include "files/helpers.h"
#include "data/format/scanner/scanner.h"
#include "input_keycodes.h"
#include "data/struct/linked_list.h"
#include "doc.h"

uintptr_t total_size = 0;

node_info interpret_tag(string_slice tag){
    if (slice_lit_match(tag, "p", true))
        return (node_info){doc_text_body,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h1", true))
        return (node_info){doc_text_title,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h2", true))
        return (node_info){doc_text_subtitle,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h3", true))
        return (node_info){doc_text_heading,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h4", true))
        return (node_info){doc_text_subheading,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h5", true))
        return (node_info){doc_text_footnote,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "h6", true))
        return (node_info){doc_text_caption,doc_gen_text,.fg_color = 0xFF000000, .sizing_rule = size_fit };
    if (slice_lit_match(tag, "script", true)){
        in_case_of_js_break_glass();
    }
    return (node_info){};
}

document_node* emit_content(string_slice slice, node_info info){
    document_node* node = zalloc(sizeof(document_node));
    total_size += sizeof(document_node);
    node->content = slice;
    node->info = info;
    return node;
}

document_node* parse_tag(Scanner *s){
    
    document_node* node = zalloc(sizeof(document_node));
    total_size += sizeof(document_node);
    node->children = linked_list_create();
    
    scan_to(s, '<');
    string_slice open = scan_to(s, '>');
    uint32_t in_pos = s->pos;
    open.length--;
    
    node->info = interpret_tag(open);
    if (node->info.type == doc_text_none){
        print("Unknown tag %v",open);
        return node;
    }
    
    uint32_t pos = s->pos;
    
    scan_to(s, '<');
    linked_list_push(node->children, emit_content((string_slice){(char*)s->buf + in_pos, s->pos - in_pos - 1},node->info));
    while (scan_peek(s) != '/'){
        s->pos--;
        linked_list_push(node->children, parse_tag(s));
        in_pos = s->pos;
        scan_to(s, '<');
        linked_list_push(node->children, emit_content((string_slice){(char*)s->buf + in_pos, s->pos - in_pos - 1},node->info));
    }
    string_slice close = scan_to(s, '>');
    if (*(char*)close.data != '/'){
        s->pos = pos;
        parse_tag(s);
    }
    close.data++;
    close.length -= 2;
    
    if (!slices_equal(open, close, true)){
        print("Wrong tag buddy");
        return node;
    }
    
    return node;
}

int main(int argc, char* argv[]){
    print("Ola mundo");
    size_t file_size = 0;
    char *file = read_full_file("/resources/index.html", &file_size);
    
    draw_ctx ctx = {};
    ctx.width = 1920;
    ctx.height = 1080;
    request_draw_ctx(&ctx);
    Scanner s = scanner_make(file, file_size);
    
    document_node *root = zalloc(sizeof(document_node));
    total_size += sizeof(document_node);
    root->children = linked_list_create();
    root->info.sizing_rule = size_fit;
    root->info.general_type = doc_gen_layout;
    root->info.type = doc_layout_vertical;
    
    document_data doc = (document_data){
        .root = root
    };
    
    print("total memory used: %i",total_size);
    
    while (!scan_eof(&s)){
        linked_list_push(root->children, parse_tag(&s));
    }    
    
    layout_document((gpu_rect){{0,0},{ctx.width,ctx.height}}, doc);
    
    debug_document(doc);
    
    while (!should_close_ctx()){
        
        fb_clear(&ctx, 0xFFFFFFFF);
        
        render_document(&ctx,doc);
        
        commit_draw_ctx(&ctx);
        
        kbd_event ev = {};
        if (read_event(&ev) && ev.key == KEY_ESC) return 0;
    }
    
    destroy_draw_ctx(&ctx);
    
    return 0;
    
}