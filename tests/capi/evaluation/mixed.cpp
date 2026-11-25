#include "mixed.hpp"

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <array>

namespace biyos::test
{

namespace
{

int user_values(const BIYOS_cbreq *request, double *f, void *)
{
    const double *x = request->x;
    f[0] = x[0] * x[3] * x[3];
    f[1] = x[1] * x[1] * x[1];
    return 0;
}

int user_gradients(const BIYOS_cbreq *request, double *g, void *)
{
    const double *x = request->x;
    g[0] = x[0] * x[3];
    g[1] = 3.0 * x[1] * x[1];
    g[2] = x[3] * x[3];
    g[3] = x[0] * x[3];
    return 0;
}

int user_hessian(const BIYOS_cbreq *request, double *h, void *)
{
    const double *x = request->x;
    const double *w = request->w;
    h[0] = w[0] * x[3];
    h[1] = 6.0 * w[1] * x[1];
    h[2] = 2.0 * w[0] * x[0];
    h[3] = w[0] * x[3];
    return 0;
}

int user_product(const BIYOS_cbreq *request, double *hv, void *)
{
    const double *x = request->x;
    const double *w = request->w;
    const double *v = request->v;
    hv[0] = 2.0 * w[0] * x[3] * v[3];
    hv[1] = 6.0 * w[1] * x[1] * v[1];
    hv[3] = 2.0 * w[0] * x[3] * v[0] + 2.0 * w[0] * x[0] * v[3];
    return 0;
}

int second_values(const BIYOS_cbreq *request, double *f, void *)
{
    f[0] = request->x[2] * request->x[3];
    return 0;
}

int second_gradients(const BIYOS_cbreq *request, double *g, void *)
{
    g[0] = request->x[2];
    g[1] = request->x[3];
    return 0;
}

int second_hessian(const BIYOS_cbreq *request, double *h, void *)
{
    h[0] = request->w[0];
    return 0;
}

void add_graphs(BIYOS_model *model)
{
    const std::array<BIYOS_offset, 1> count{3};
    const std::array kinks{-1.0, 0.0, 2.0};
    const std::array heights{1.0, 0.0, 3.0};
    ASSERT_EQ(BIYOS_addpwl(model, 1, count.data(), kinks.data(), heights.data(), nullptr),
              BIYOS_OK);
    const std::array<BIYOS_offset, 2> counts{5, 3};
    const std::array codes{BIYOS_OP_VAR,
                           BIYOS_OP_EXP,
                           BIYOS_OP_VAR,
                           BIYOS_OP_SIN,
                           BIYOS_OP_PROD,
                           BIYOS_OP_VAR,
                           BIYOS_OP_PWL,
                           BIYOS_OP_EXP};
    const std::array<BIYOS_offset, 8> arguments{1, 1, 2, 1, 2, 3, 0, 1};
    const std::array coefficient{1.0};
    std::array<BIYOS_offset, 2> nodes{};
    ASSERT_EQ(BIYOS_addexpr(model,
                            2,
                            counts.data(),
                            codes.data(),
                            arguments.data(),
                            coefficient.data(),
                            nodes.data()),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_attachexprrg(model, 0, 2, nodes.data()), BIYOS_OK);
}

void add_terms(BIYOS_model *model)
{
    const std::array functions{0, 0, 2};
    const std::array variables{0, 3, 1};
    const std::array linear{1.5, -1.0, 2.0};
    ASSERT_EQ(BIYOS_addlintermsp(model, 3, functions.data(), variables.data(), linear.data()),
              BIYOS_OK);
    const std::array rows{0, 1, 0};
    const std::array columns{0, 2, 3};
    const std::array quadratic{0.7, -0.4, 0.3};
    ASSERT_EQ(BIYOS_addquadtermsp(model,
                                  3,
                                  functions.data(),
                                  rows.data(),
                                  columns.data(),
                                  quadratic.data()),
              BIYOS_OK);
}

void add_user(BIYOS_model *model, bool product)
{
    const std::array covered{2, 1};
    BIYOS_index cb = -1;
    ASSERT_EQ(BIYOS_addcb(model, 2, covered.data(), nullptr, &cb), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcbf(model, cb, user_values), BIYOS_OK);
    const std::array functions{2, 1, 2, 2};
    const std::array variables{3, 1, 0, 3};
    ASSERT_EQ(BIYOS_setcbg(model, cb, 4, functions.data(), variables.data(), user_gradients),
              BIYOS_OK);
    const std::array rows{0, 1, 3, 0};
    const std::array columns{3, 1, 3, 3};
    ASSERT_EQ(BIYOS_setcbh(model, cb, 4, rows.data(), columns.data(), user_hessian), BIYOS_OK);
    if (product)
    {
        ASSERT_EQ(BIYOS_setcbhv(model, cb, user_product), BIYOS_OK);
    }
}

void add_second(BIYOS_model *model)
{
    const std::array covered{0};
    BIYOS_index cb = -1;
    ASSERT_EQ(BIYOS_addcb(model, 1, covered.data(), nullptr, &cb), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcbf(model, cb, second_values), BIYOS_OK);
    const std::array functions{0, 0};
    const std::array variables{3, 2};
    ASSERT_EQ(BIYOS_setcbg(model, cb, 2, functions.data(), variables.data(), second_gradients),
              BIYOS_OK);
    const std::array row{2};
    const std::array column{3};
    ASSERT_EQ(BIYOS_setcbh(model, cb, 1, row.data(), column.data(), second_hessian), BIYOS_OK);
}

}

void build_mixed(BIYOS_model *model, bool product)
{
    ASSERT_EQ(BIYOS_addvar(model, 4, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model, 3, nullptr), BIYOS_OK);
    const std::array constants{0.5, 0.0, -1.0};
    ASSERT_EQ(BIYOS_addcstrg(model, 0, 3, constants.data()), BIYOS_OK);
    add_graphs(model);
    add_terms(model);
    add_user(model, product);
    add_second(model);
}

}
