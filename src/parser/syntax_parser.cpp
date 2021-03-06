/*
 * Copyright (c) 2012, Matias Fontanini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <fstream>
#include "parser/syntax_parser.h"
#include "compound_field.h"
#include "exceptions.h"
#include "field.h"
#include "functions/hashing.h"
#include "functions/misc.h"
#include "functions/crc.h"
#include "functions/random.h"
#include "block_field.h"
#include "bitfield.h"
#include "compound_field.h"
#include "template_field.h"
#include "variable_block_field.h"
#include "function_value_filler.h"
#include "const_value_node.h"


std::istream *istr = nullptr;
int curr_lineno;
syntax_parser* grammar_syntax_parser = nullptr;


extern "C" int yyparse();

field::filler_type default_filler() {
    return make_unique<random_function>();
}

field::filler_type default_bit_filler() {
    return make_unique<bitrandom_function>();
}

syntax_parser::syntax_parser()
{
    register_filler_function<md5_function>("md5");
    register_filler_function<sha1_function>("sha1");
    register_value_function<size_function>("size");
    register_value_function<field_count_function>("count");
    register_value_function<crc32_function>("crc32");
}

void syntax_parser::parse(const std::string &file_name)
{
    std::ifstream input(file_name);
    parse(input);
}

void syntax_parser::parse(std::istream &input)
{
    istr = &input;
    grammar_syntax_parser = this;
    curr_lineno = 1;
    
    if(yyparse() != 0 || script_root == nullptr)
        throw parse_error();
    grammar_syntax_parser = nullptr;
    istr = nullptr;
    script_root->check_constraints();
}

void syntax_parser::add_template(std::string name, grammar::template_def_node *node)
{
    templates.insert(
        std::make_pair(std::move(name), node)
    );
}

field syntax_parser::allocate_template(const std::string &name, size_t min, size_t max)
{
    return templates.at(name)->allocate(mapper, min, max);
}

void syntax_parser::set_script(grammar::script* scr)
{
    script_root = scr;
}

field syntax_parser::get_root_field()
{
    if(!script_root)
        throw parse_error();
    
    std::vector<field> fields;
    for(const auto& i : script_root->fields)
        fields.push_back(i->allocate(mapper));
    
    auto impl = make_unique<compound_field_impl>(
        std::make_move_iterator(fields.begin()),
        std::make_move_iterator(fields.end())
    );
    
    return field(nullptr, std::move(impl));
}

field_mapper &syntax_parser::get_mapper()
{
    return mapper;
}

auto syntax_parser::allocate_filler_function(const std::string &name, identifier_type id) -> filler_node*
{
    return filler_functions.at(name)(id);
}

auto syntax_parser::allocate_value_function(const std::string &name, identifier_type id) -> value_node*
{
    return value_functions.at(name)(id);
}

grammar::script *syntax_parser::make_script()
{
    return node_alloc<grammar::script>();
}

// block field

auto syntax_parser::make_block_node(filler_node *filler, size_t size) -> field_node *
{
    return node_alloc<grammar::block_field_node>(filler, size);
}

auto syntax_parser::make_block_node(filler_node *filler, size_t size, 
  const std::string &name) -> field_node *
{
    auto id = mapper.register_field(name);
    return node_alloc<grammar::block_field_node>(filler, size, id);
}

auto syntax_parser::make_auto_node(filler_node *filler) -> field_node *
{
    return node_alloc<grammar::auto_field_node>(filler);
}

auto syntax_parser::make_auto_node(filler_node *filler, const std::string &name) -> field_node *
{
    auto id = mapper.register_field(name);
    return node_alloc<grammar::auto_field_node>(filler, id);
}

// bitfield

auto syntax_parser::make_bitfield_node(value_node *vnode, size_t size) -> field_node *
{
    filler_node *filler = vnode ? make_function_value_filler_node(vnode) : nullptr;
    return node_alloc<grammar::bitfield_node>(filler, size);
}

auto syntax_parser::make_bitfield_node(value_node *vnode, size_t size, 
  const std::string &name) -> field_node *
{
    auto id = mapper.register_field(name);
    filler_node *filler = vnode ? make_function_value_filler_node(vnode) : nullptr;
    return node_alloc<grammar::bitfield_node>(filler, size, id);
}

// variable block

auto syntax_parser::make_variable_block_node(filler_node *filler, 
  size_t min_size, size_t max_size) -> field_node *
{
    return node_alloc<grammar::varblock_field_node>(filler, min_size, max_size);
}

auto syntax_parser::make_variable_block_node(filler_node *filler, 
  size_t min_size, size_t max_size, const std::string &name) 
-> field_node *
{
    auto id = mapper.register_field(name);
    return node_alloc<grammar::varblock_field_node>(filler, min_size, max_size, id);
}

// compound

auto syntax_parser::make_compound_field_node(fields_list *fields) -> field_node*
{
    return node_alloc<grammar::compound_field_node>(fields);
}

auto syntax_parser::make_compound_field_node(fields_list *fields, const std::string &name) -> field_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<grammar::compound_field_node>(fields, id);
}

// compound bitfield

auto syntax_parser::make_compound_bitfield_node(fields_list *fields) -> field_node*
{
    return node_alloc<grammar::compound_bitfield_node>(fields);
}

auto syntax_parser::make_compound_bitfield_node(fields_list *fields, const std::string &name) -> field_node *
{
    auto id = mapper.register_field(name);
    return node_alloc<grammar::compound_bitfield_node>(fields, id);
}

// choice field

auto syntax_parser::make_choice_field_node(fields_list *fields) -> field_node*
{
    return node_alloc<grammar::choice_field_node>(fields);
}

auto syntax_parser::make_choice_field_node(fields_list *fields, const std::string &name) -> field_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<grammar::choice_field_node>(fields, id);
}

// template field

auto syntax_parser::make_template_field_node(const std::string &template_name, 
  size_t min, size_t max) -> field_node*
{
    return node_alloc<grammar::template_field_node>(
        templates.at(template_name),
        min, 
        max
    );
}

// template def field

auto syntax_parser::make_template_def_node(fields_list *fields) -> template_def_node*
{
    return node_alloc<grammar::template_def_node>(fields);
}

auto syntax_parser::make_fields_list() -> fields_list *
{
    return node_alloc<fields_list>();
}

// stuff

auto syntax_parser::make_const_value_node(double f) -> value_node *
{
    return node_alloc<grammar::const_value_node>(f);
}

auto syntax_parser::make_const_string_node(const std::string &str) -> filler_node *
{
    return node_alloc<grammar::const_string_node>(str);
}

auto syntax_parser::make_node_value_node(const std::string &name) -> value_node *
{
    auto id = mapper.find_register_field_name(name);
    return node_alloc<grammar::node_value_node>(id);
}

auto syntax_parser::make_node_filler_node(const std::string &field_name, 
  const std::string &function_name) -> filler_node *
{
    auto id = mapper.find_register_field_name(field_name);
    if(is_filler_function(function_name)) {
        return allocate_filler_function(function_name, id);
    }
    else if(is_value_function(function_name)) {
        return node_alloc<grammar::function_value_filler_node>(
            allocate_value_function(function_name, id)
        );
    }
    else {
        return nullptr; // fixme
    }
}

auto syntax_parser::make_node_value_function_node(const std::string &field_name, 
  const std::string &function_name) -> value_node *
{
    auto id = mapper.find_register_field_name(field_name);
    return allocate_value_function(function_name, id);
}

auto syntax_parser::make_function_value_filler_node(value_node *node) -> filler_node*
{
    return node_alloc<grammar::function_value_filler_node>(node);
}

bool syntax_parser::is_filler_function(const std::string &name)
{
    return filler_functions.count(name) == 1;
}

bool syntax_parser::is_value_function(const std::string &name)
{
    return value_functions.count(name) == 1;
}
