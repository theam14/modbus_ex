#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <boost/variant.hpp>

struct DataType
{
    constexpr static std::string_view FORMAT_UINT8 = "u8";
    constexpr static std::string_view FORMAT_UINT16 = "u16";
    constexpr static std::string_view FORMAT_UINT32 = "u32";
    constexpr static std::string_view FORMAT_SINT32 = "s32";
    constexpr static std::string_view FORMAT_FLOAT = "f32";
};

struct ModbusData
{
    std::vector<std::uint32_t> modbus_response;
};

struct Register
{
    std::string data_type;
    std::uint32_t address;
};

using converted_value_t = boost::variant<std::uint8_t, std::uint16_t, std::uint32_t, std::int32_t, std::float_t>;


template<class T>
void fromBytesToValue(T& value, const std::uint32_t input)
{
    std::memcpy(&value, &input, sizeof(T));
}

std::unordered_map<std::string, std::function<converted_value_t(std::uint32_t)> > conversion_map(
{
    { std::string(DataType::FORMAT_UINT32), [](std::uint32_t addr) -> converted_value_t { return addr; } },

    { std::string(DataType::FORMAT_SINT32), [](std::uint32_t addr) -> converted_value_t { 
        std::int32_t value = {}; 
        fromBytesToValue(value, addr); 
        return value;
    }},

    { std::string(DataType::FORMAT_FLOAT), [](std::uint32_t addr) -> converted_value_t { 
        std::float_t value = {}; 
        fromBytesToValue(value, addr); 
        return value;
    }},

    { std::string(DataType::FORMAT_UINT8), [](std::uint32_t addr) -> converted_value_t { 
        std::uint8_t value = {}; 
        fromBytesToValue(value, (addr & 0x000000FF)); 
        return value;
    }},

    { std::string(DataType::FORMAT_UINT16), [](std::uint32_t addr) -> converted_value_t { 
        std::uint16_t value = {}; 
        fromBytesToValue(value, (addr & 0x0000FFFF) << 16); 
        return value;
    }}
});

class PrintVistor : boost::static_visitor<>
{
public:
    void operator()(const std::uint8_t value)
    {
        std::cout << "-- unsigned int value (8-bits) = " << unsigned(value) << "\n";
    }

    void operator()(const std::uint16_t value)
    {
        std::cout << "-- unsigned int value (16-bits) = " << +value << "\n";
    }

    void operator()(const std::uint32_t value)
    {
        std::cout << "-- unsigned int value (32-bits) = " << value << "\n";
    }

    void operator()(const std::int32_t value)
    {
        std::cout << "-- signed int value (32-bits) = " << value << "\n";
    }

    void operator()(const std::float_t value)
    {
        std::cout << "-- float point value (32-bits) = " << value << "\n";
    }

};

int main(int, char**) 
{
    std::vector<int> values_vect(20);
    std::iota(values_vect.begin(), values_vect.end(), 1);

    ModbusData modbus_data = {};

    const size_t num_bytes = 4;
    const size_t bit_shift = 8;

    /* ensures that only interate over positions multiple of 4 */
    auto end_it = std::cend(values_vect) - (values_vect.size() % num_bytes); 

    if (end_it != std::cend(values_vect))
    {
        std::cout << "WARNING: VALUE ARRAY DOES CONTAINS COMPLETE VALUES\n";
    }

    for (auto val_it = std::cbegin(values_vect); val_it != end_it; val_it += num_bytes)
    {
        size_t shift_multiplier = num_bytes - 1;

        modbus_data.modbus_response.emplace_back(
            std::accumulate(val_it, val_it + num_bytes, 0, [&shift_multiplier](int acc, int val) 
            {
                return std::move(acc) + (val << (bit_shift * (shift_multiplier--)));
            })
        );
    }

    std::cout << "result: ";

    std::copy(modbus_data.modbus_response.begin(),
              modbus_data.modbus_response.end(), std::ostream_iterator<int>(std::cout, " "));
    
    std::cout << "\n";

    // Transform to data type

    std::vector<Register> registers{
        {std::string(DataType::FORMAT_UINT8), modbus_data.modbus_response[0]},
        {std::string(DataType::FORMAT_UINT16), modbus_data.modbus_response[1]},
        {std::string(DataType::FORMAT_UINT32), modbus_data.modbus_response[2]},
        {std::string(DataType::FORMAT_SINT32), modbus_data.modbus_response[3]},
        {std::string(DataType::FORMAT_FLOAT), modbus_data.modbus_response[4]}
        };

    std::vector<converted_value_t> converted_values;
    std::transform(registers.begin(), registers.end(),
                   std::back_insert_iterator(converted_values),
                   [](const auto& reg_val) { return std::move(conversion_map[reg_val.data_type](reg_val.address)); });

    PrintVistor print_visitor;
    std::for_each(converted_values.begin(), converted_values.end(),
                  boost::apply_visitor(print_visitor));
}
