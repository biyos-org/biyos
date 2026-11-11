#include "model/model.hpp"

#include "core/error.hpp"
#include "model/names.hpp"

#include <string>
#include <string_view>

namespace biyos
{

const std::string *Model::name() const noexcept
{
    return name_.empty() ? nullptr : &name_;
}

void Model::set_name(std::string_view name)
{
    const std::string fault = name_fault(name);
    if (!fault.empty())
    {
        throw Error(ErrorKind::InvalidArgument, "model", fault);
    }
    name_.assign(name);
}

void Model::del_name() noexcept
{
    name_.clear();
}

}
