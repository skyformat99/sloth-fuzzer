#include <functional>
#include "field_mapper.h"

void field_mapper::register_field(std::string name, field::identifier_type id)
{
    str2id.insert(std::make_pair(std::move(name), id));
}

void field_mapper::identify_fields(const field &root) 
{
    using std::placeholders::_1;
    
    root.accept_visitor(
        std::bind(&field_mapper::visit, this, _1)
    );
}

void field_mapper::visit(const field &f) 
{
    id2field.insert({ f.id(), f });
}

const field& field_mapper::find_field(field::identifier_type id) const
{
    return id2field.at(id);
}