#pragma once

#include <string_view>

namespace biyos::test::nl
{

inline constexpr std::string_view kTinyHeader = R"(g3 1 1 0
 1 1 1 0 0
 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 1 1
 0 0
 0 0 0 0 0
)";

inline constexpr std::string_view kDefinedHeader = R"(g3 1 1 0
 1 1 1 0 0
 1 1
 0 0
 1 1 1
 0 0 0 1
 0 0 0 0 0
 0 0
 0 0
 1 0 0 0 0
)";

inline constexpr std::string_view kLinearNlFile = R"(g3 1 1 0	# problem lp
 2 2 1 0 1	# vars, constraints, objectives, ranges, eqns
 0 0	# nonlinear constraints, objectives
 0 0	# network constraints: nonlinear, linear
 0 0 0	# nonlinear vars in constraints, objectives, both
 0 0 0 1	# linear network variables; functions; arith, flags
 0 0 0 0 0	# discrete variables: binary, integer, nonlinear (b,c,o)
 4 2	# nonzeros in Jacobian, gradients
 0 0	# max name lengths: constraints, variables
 0 0 0 0 0	# common exprs: b,c,o,c1,o1
C0	#cover
n0
C1	#balance
n0
O0 0	#cost
n1
r	#2 ranges (rhs's)
2 1
4 0
b	#2 bounds (on variables)
0 0 10
2 0
k1	#intermediate Jacobian column lengths
2
J0 2
0 1
1 1
J1 2
0 1
1 -1
G0 2
0 2
1 3
)";

inline constexpr std::string_view kNonlinearNlFile = R"(g3 1 1 0	# problem curved
 2 1 1 0 0
 1 1
 0 0
 2 2 2
 0 0 0 1
 0 0 0 0 0
 2 2
 0 0
 1 0 0 0 0
V2 1 0	#shared
0 2
o41	#sin
v0
C0	#ring
o2	#*
v2	#shared
v1
O0 1	#gain
o0	#+
v2	#shared
o5	#^
v0
n2
r
1 4
b
3
3
k1
1
J0 2
0 0
1 0
G0 2
0 0
1 0
)";

inline constexpr std::string_view kKindsNlFile = R"(g3 1 1 0
 5 0 1 0 0
 0 1
 0 0
 3 2 2
 0 0 0 1
 1 1 1 1 0
 0 0
 0 0
 0 0 0 0 0
O0 0
n0
b
0 0 1
0 0 1
0 0 1
0 0 1
0 0 1
)";

inline constexpr std::string_view kOrderNlFile = R"(g3 1 1 0
 6 0 1 0 0
 0 1
 0 0
 2 4 1
 0 0 0 1
 1 0 0 1 1
 0 0
 0 0
 0 0 0 0 0
O0 0
n0
b
0 0 1
0 0 1
0 0 1
0 0 1
0 0 1
0 0 1
)";

inline constexpr std::string_view kObjectivesNlFile = R"(g3 1 1 0
 1 0 2 0 0
 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 0 1
 0 0
 0 0 0 0 0
O0 0
n1
O1 1
n0
b
1 5
G1 1
0 2
)";

inline constexpr std::string_view kNlFiles[] = {
    kLinearNlFile,
    kNonlinearNlFile,
    kKindsNlFile,
    kOrderNlFile,
    kObjectivesNlFile,
};

}
