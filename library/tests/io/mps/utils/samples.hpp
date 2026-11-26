#pragma once

#include <string_view>

namespace biyos::test
{

inline constexpr std::string_view kLpMpsFile = R"(* A small linear program
NAME          small
ROWS
 N  cost
 L  limit
 G  floor
 E  balance
COLUMNS
    x         cost         1.0   limit        2.0
    x         balance      1.0
    y         cost        -3.5   floor        1.0
    y         limit        1.0   balance     -1.0
RHS
    rhs       cost         4.0   limit       10.0
    rhs       floor        2.0
ENDATA
)";

inline constexpr std::string_view kConstraintsMpsFile = R"(NAME          constraints
ROWS
 L  cap
 E  fix
COLUMNS
    x         cap          1.0   fix          1.0
    y         cap          1.0
RHS
    rhs       cap          4.0   fix          1.0
ENDATA
)";

inline constexpr std::string_view kRangesMpsFile = R"(NAME          ranges
ROWS
 N  obj
 L  less
 G  more
 E  upward
 E  downward
COLUMNS
    x         obj          1.0   less         1.0
    x         more         1.0   upward       1.0
    x         downward     1.0
RHS
    rhs       less         4.0   more         1.0
    rhs       upward       2.0   downward     3.0
RANGES
    rng       less         1.5   more        -2.0
    rng       upward       0.5   downward    -0.5
ENDATA
)";

inline constexpr std::string_view kBoundsMpsFile = R"(NAME          bounds
ROWS
 N  obj
COLUMNS
    lo        obj          1.0
    up        obj          1.0
    fx        obj          1.0
    fr        obj          1.0
    mi        obj          1.0
    pl        obj          1.0
    bv        obj          1.0
    li        obj          1.0
    ui        obj          1.0
    sc        obj          1.0
    si        obj          1.0
    neg       obj          1.0
    pair      obj          1.0
RHS
BOUNDS
 LO bnd       lo           -2.0
 UP bnd       up           3.0
 FX bnd       fx           1.5
 FR bnd       fr
 MI bnd       mi
 PL bnd       pl
 BV bnd       bv
 LI bnd       li           -1
 UI bnd       ui           7
 SC bnd       sc           5.0
 SI bnd       si           9.0
 UP bnd       neg          -4.0
 LO bnd       pair         1.0
 UP bnd       pair         2.0
ENDATA
)";

inline constexpr std::string_view kMarkersMpsFile = R"(NAME          markers
ROWS
 N  obj
 L  cap
COLUMNS
    a         obj          1.0   cap          1.0
    MARKER                 'MARKER'                 'INTORG'
    b         obj          2.0   cap          1.0
    c         obj          3.0   cap          1.0
    MARKER                 'MARKER'                 'INTEND'
    d         obj          4.0   cap          1.0
RHS
    rhs       cap          5.0
BOUNDS
 UP bnd       c            8.0
 LO bnd       d            1.0
ENDATA
)";

inline constexpr std::string_view kMaximizeMpsFile = R"(NAME          maximize
OBJSENSE
    MAX
ROWS
 N  profit
 L  cap
COLUMNS
    x         profit       2.0   cap          1.0
RHS
    rhs       cap          3.0
ENDATA
)";

inline constexpr std::string_view kRowsMpsFile = R"(NAME
ROWS
 N  first
 L  cap
 N  second
 N  third
COLUMNS
    x         first        1.0   second       2.0
    x         cap          1.0   third        3.0
RHS
    rhs       cap          1.0   second       5.0
ENDATA
)";

inline constexpr std::string_view kSetsMpsFile = R"(NAME          sets
ROWS
 N  obj
COLUMNS
    x         obj          1.0
    y         obj          1.0
    z         obj          1.0
RHS
BOUNDS
 UP bnd       x            4.0
SOS
 S1 pick
    x         1.5
    y         3.0
 S2
    y
    z         $ weight 2
    x
 S1 empty
ENDATA
)";

inline constexpr std::string_view kIndicatorsMpsFile = R"(NAME          indicators
ROWS
 N  obj
 E  fix
 L  cap
 G  low
COLUMNS
    MARKER                 'MARKER'                 'INTORG'
    z         obj          1.0
    MARKER                 'MARKER'                 'INTEND'
    b         obj          2.0
    x         obj          1.0   fix          1.0
    x         cap          1.0   low          1.0
    y         cap          1.0   low          2.0
RHS
    rhs       fix          2.0   cap          4.0
    rhs       low          1.0
BOUNDS
 BV bnd       b
INDICATORS
 IF cap       z            1
 IF low       b            0
ENDATA
)";

inline constexpr std::string_view kMpsFiles[] = {
    kBoundsMpsFile,
    kConstraintsMpsFile,
    kIndicatorsMpsFile,
    kLpMpsFile,
    kMarkersMpsFile,
    kMaximizeMpsFile,
    kRangesMpsFile,
    kRowsMpsFile,
    kSetsMpsFile,
};

}
