#include "io/native/readers/attributes.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/native/utils/body.hpp"
#include "io/native/utils/record.hpp"
#include "model/model.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::native
{

namespace
{

struct Side
{
    std::vector<Index> indices;
    std::vector<Real> values;

    void add(Index index, std::optional<Real> value)
    {
        if (value)
        {
            indices.push_back(index);
            values.push_back(*value);
        }
    }
};

Index index_after(Record &record, const std::vector<Index> &indices)
{
    const Index index = record.index();
    if (!indices.empty() && index <= indices.back())
    {
        record.refuse("index " + text_of(index) + " is out of order");
    }
    return index;
}

}

void read_names(Body &body, Model &model, Family family)
{
    std::vector<Index> indices;
    std::vector<std::string> names;
    while (body.next())
    {
        Record record = body.record();
        indices.push_back(index_after(record, indices));
        names.push_back(record.name());
        record.end();
    }
    const std::vector<std::string_view> views(names.begin(), names.end());
    model.set_names(family, indices, views);
}

void read_tolerances(Body &body, Model &model, Family family)
{
    std::vector<Index> indices;
    Side absolutes;
    Side relatives;
    while (body.next())
    {
        Record record = body.record();
        indices.push_back(index_after(record, indices));
        const std::optional<Real> absolute = record.tolerance();
        const std::optional<Real> relative = record.tolerance();
        if (!absolute && !relative)
        {
            record.refuse("the line sets no tolerance");
        }
        absolutes.add(indices.back(), absolute);
        relatives.add(indices.back(), relative);
        record.end();
    }
    model.set_tolerances(family, absolutes.indices, absolutes.values, {});
    model.set_tolerances(family, relatives.indices, {}, relatives.values);
}

}
