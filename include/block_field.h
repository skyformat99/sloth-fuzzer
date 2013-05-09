#ifndef FUZZER_BLOCK_FIELD_H
#define FUZZER_BLOCK_FIELD_H

#include <vector>
#include "field_impl.h"

template<typename Container>
class generic_block_field_impl : public clonable_field_impl<generic_block_field_impl<Container>> {
public:
    typedef Container container_type;

    generic_block_field_impl(size_t data_size)
    : data(data_size)
    {
        
    }

    void set(size_t index, field_impl::value_type value)
    {
        data[index] = value;
    }
    
    field_impl::value_type get(size_t index) const
    {
        return data[index];
    }
    
    size_t size() const
    {
        return data.size();
    }
private:
    container_type data;
};

typedef generic_block_field_impl<std::vector<field_impl::value_type>> block_field_impl;

#endif // FUZZER_BLOCK_FIELD_H
