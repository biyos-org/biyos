#include "sample.hpp"

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace biyos::test
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Terms
{
    std::vector<BIYOS_offset> starts{0};
    std::vector<BIYOS_index> rows;
    std::vector<BIYOS_index> columns;
    std::vector<BIYOS_real> values;
};

double small(Engine &engine)
{
    return static_cast<double>(below(engine, 9) - 4);
}

void add_variables(BIYOS_model *model, Engine &engine, int n)
{
    std::vector<BIYOS_int> kinds;
    std::vector<BIYOS_real> lower;
    std::vector<BIYOS_real> upper;
    for (int k = 0; k < n; ++k)
    {
        kinds.push_back(1 + below(engine, 5));
        lower.push_back(below(engine, 4) / 8.0);
        upper.push_back(0.5 + below(engine, 4) / 8.0);
    }
    const BIYOS_status status =
        BIYOS_addvar(model, n, kinds.data(), lower.data(), upper.data(), nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

void add_row(Engine &engine, int n_variables, Terms &terms)
{
    for (int variable = 0; variable < n_variables; ++variable)
    {
        if (below(engine, 2) == 0)
        {
            terms.columns.push_back(variable);
            terms.values.push_back(small(engine));
        }
    }
    terms.starts.push_back(static_cast<BIYOS_offset>(terms.columns.size()));
}

void add_functions(BIYOS_model *model, Engine &engine, int n, int n_variables)
{
    Terms terms;
    std::vector<BIYOS_real> constants;
    for (int k = 0; k < n; ++k)
    {
        constants.push_back(small(engine));
        add_row(engine, n_variables, terms);
    }
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addfun(model, n, &start), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model, start, n, constants.data()), BIYOS_OK);
    const BIYOS_status status = BIYOS_addrlintermrg(model,
                                                    start,
                                                    n,
                                                    terms.starts.data(),
                                                    terms.columns.data(),
                                                    terms.values.data());
    ASSERT_EQ(status, BIYOS_OK);
}

void add_pairs(Engine &engine, int n_variables, Terms &terms)
{
    for (int row = 0; row < n_variables; ++row)
    {
        for (int column = row; column < n_variables; ++column)
        {
            if (below(engine, 16) == 0)
            {
                terms.rows.push_back(row);
                terms.columns.push_back(column);
                terms.values.push_back(small(engine));
            }
        }
    }
    terms.starts.push_back(static_cast<BIYOS_offset>(terms.columns.size()));
}

void add_squares(BIYOS_model *model, Engine &engine, int n_functions, int n_variables)
{
    Terms terms;
    for (int k = 0; k < n_functions; ++k)
    {
        add_pairs(engine, n_variables, terms);
    }
    const BIYOS_status status = BIYOS_addquadtermrg(model,
                                                    0,
                                                    n_functions,
                                                    terms.starts.data(),
                                                    terms.rows.data(),
                                                    terms.columns.data(),
                                                    terms.values.data());
    ASSERT_EQ(status, BIYOS_OK);
}

void add_constraints(BIYOS_model *model, Engine &engine, int n, int n_functions)
{
    std::vector<BIYOS_index> functions;
    std::vector<BIYOS_real> lower;
    std::vector<BIYOS_real> upper;
    for (int k = 0; k < n; ++k)
    {
        functions.push_back(below(engine, n_functions));
        const double base = small(engine);
        lower.push_back(below(engine, 4) == 0 ? -kInfinity : base);
        upper.push_back(below(engine, 4) == 0 ? kInfinity : base + below(engine, 4));
    }
    const BIYOS_status status =
        BIYOS_addcon(model, n, functions.data(), lower.data(), upper.data(), nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

void add_objectives(BIYOS_model *model, Engine &engine, int n, int n_functions)
{
    std::vector<BIYOS_index> functions;
    std::vector<BIYOS_int> senses;
    std::vector<BIYOS_int> priorities;
    std::vector<BIYOS_real> weights;
    std::vector<BIYOS_real> absolute;
    std::vector<BIYOS_real> relative;
    for (int k = 0; k < n; ++k)
    {
        functions.push_back(below(engine, n_functions));
        senses.push_back(1 + below(engine, 2));
        priorities.push_back(below(engine, 5) - 2);
        weights.push_back((1 + below(engine, 4)) / 2.0);
        absolute.push_back(below(engine, 4) / 4.0);
        relative.push_back(below(engine, 4) / 8.0);
    }
    const BIYOS_status status = BIYOS_addobj(model,
                                             n,
                                             functions.data(),
                                             senses.data(),
                                             priorities.data(),
                                             weights.data(),
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolrg(model, 0, n, absolute.data(), relative.data()), BIYOS_OK);
}

void add_members(Engine &engine, int n_variables, Terms &terms)
{
    for (int variable = 0; variable < n_variables; ++variable)
    {
        if (below(engine, 4) == 0)
        {
            const auto position = static_cast<double>(terms.columns.size());
            terms.columns.push_back(variable);
            terms.values.push_back(position + below(engine, 4) / 8.0);
        }
    }
    terms.starts.push_back(static_cast<BIYOS_offset>(terms.columns.size()));
}

void add_sos(BIYOS_model *model, Engine &engine, int n, int n_variables)
{
    Terms terms;
    std::vector<BIYOS_int> kinds;
    for (int k = 0; k < n; ++k)
    {
        kinds.push_back(1 + below(engine, 2));
        add_members(engine, n_variables, terms);
    }
    const BIYOS_status status = BIYOS_addsos(model,
                                             n,
                                             kinds.data(),
                                             terms.starts.data(),
                                             terms.columns.data(),
                                             terms.values.data(),
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

using Namer = BIYOS_status (*)(BIYOS_model *,
                               BIYOS_index,
                               const BIYOS_index *,
                               const char *const *);

void add_names(BIYOS_model *model, Engine &engine, int n, Namer name)
{
    std::vector<BIYOS_index> named;
    std::vector<std::string> texts;
    for (int k = 0; k < n; ++k)
    {
        if (below(engine, 2) == 0)
        {
            named.push_back(k);
            texts.push_back("n" + std::to_string(below(engine, 8)));
        }
    }
    std::vector<const char *> names;
    for (const std::string &text : texts)
    {
        names.push_back(text.c_str());
    }
    const auto size = static_cast<BIYOS_index>(named.size());
    ASSERT_EQ(name(model, size, named.data(), names.data()), BIYOS_OK);
}

}

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<std::uint32_t>(n));
}

void build_sample(BIYOS_model *model, Engine &engine)
{
    const int variables = 1 + below(engine, 24);
    const int functions = 1 + below(engine, 12);
    const int constraints = 1 + below(engine, 2 * functions);
    const int objectives = 1 + below(engine, 4);
    add_variables(model, engine, variables);
    add_functions(model, engine, functions, variables);
    add_squares(model, engine, functions, variables);
    add_constraints(model, engine, constraints, functions);
    add_objectives(model, engine, objectives, functions);
    add_names(model, engine, variables, BIYOS_setvarnamesp);
    add_names(model, engine, functions, BIYOS_setfunnamesp);
    add_names(model, engine, constraints, BIYOS_setconnamesp);
    add_names(model, engine, objectives, BIYOS_setobjnamesp);
    const int sos = 1 + below(engine, 4);
    add_sos(model, engine, sos, variables);
    add_names(model, engine, sos, BIYOS_setsosnamesp);
}

}
