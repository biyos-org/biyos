from typing import TypeAlias

from typing_extensions import assert_type

from biyos import (
    Complements,
    Cones,
    Constraints,
    Functions,
    Indicators,
    Model,
    Objectives,
    ObjectiveSense,
    SpecialOrderedSets,
    Variables,
    VariableKind,
)

one = 0
flat = [0, 1]
nested = [[0, 1], [1, 2]]
real = 1.0
reals = [1.0, 2.0]
table = [[1.0, 2.0], [3.0, 4.0]]
name = "a"
names = ["a", "b"]

Pair: TypeAlias = tuple[float, float]
Pairs: TypeAlias = tuple[list[float], list[float]]
Tolerance: TypeAlias = tuple[float | None, float | None]
Tolerances: TypeAlias = tuple[list[float | None], list[float | None]]
Label: TypeAlias = str | None
Labels: TypeAlias = list[str | None]


def linear(mdl: Model) -> None:
    mdl.addlinterm(one, one, real)
    mdl.addlinterm(one, flat, reals)
    mdl.addlinterm(flat, flat, reals)
    mdl.addlinterm(flat, nested, table)
    mdl.addlinterm(one, one, reals)  # type: ignore[call-overload]
    mdl.addlinterm(one, one, table)  # type: ignore[call-overload]
    mdl.addlinterm(one, flat, real)  # type: ignore[call-overload]
    mdl.addlinterm(one, flat, table)  # type: ignore[arg-type]
    mdl.addlinterm(one, nested, real)  # type: ignore[call-overload]
    mdl.addlinterm(one, nested, reals)  # type: ignore[arg-type]
    mdl.addlinterm(one, nested, table)  # type: ignore[arg-type]
    mdl.addlinterm(flat, one, real)  # type: ignore[call-overload]
    mdl.addlinterm(flat, one, reals)  # type: ignore[call-overload]
    mdl.addlinterm(flat, one, table)  # type: ignore[call-overload]
    mdl.addlinterm(flat, flat, real)  # type: ignore[call-overload]
    mdl.addlinterm(flat, flat, table)  # type: ignore[arg-type]
    mdl.addlinterm(flat, nested, real)  # type: ignore[call-overload]
    mdl.addlinterm(flat, nested, reals)  # type: ignore[arg-type]


def columns(mdl: Model) -> None:
    mdl.addclinterm(one, one, real)
    mdl.addclinterm(one, flat, reals)
    mdl.addclinterm(flat, nested, table)
    mdl.addclinterm(one, one, reals)  # type: ignore[call-overload]
    mdl.addclinterm(one, one, table)  # type: ignore[call-overload]
    mdl.addclinterm(one, flat, real)  # type: ignore[call-overload]
    mdl.addclinterm(one, flat, table)  # type: ignore[arg-type]
    mdl.addclinterm(one, nested, real)  # type: ignore[call-overload]
    mdl.addclinterm(one, nested, reals)  # type: ignore[arg-type]
    mdl.addclinterm(one, nested, table)  # type: ignore[arg-type]
    mdl.addclinterm(flat, one, real)  # type: ignore[call-overload]
    mdl.addclinterm(flat, one, reals)  # type: ignore[call-overload]
    mdl.addclinterm(flat, one, table)  # type: ignore[call-overload]
    mdl.addclinterm(flat, flat, real)  # type: ignore[call-overload]
    mdl.addclinterm(flat, flat, reals)  # type: ignore[arg-type]
    mdl.addclinterm(flat, flat, table)  # type: ignore[arg-type]
    mdl.addclinterm(flat, nested, real)  # type: ignore[call-overload]
    mdl.addclinterm(flat, nested, reals)  # type: ignore[arg-type]


def quadratic(mdl: Model) -> None:
    mdl.addquadterm(one, one, one, real)
    mdl.addquadterm(one, flat, flat, reals)
    mdl.addquadterm(flat, flat, flat, reals)
    mdl.addquadterm(flat, nested, nested, table)
    mdl.addquadterm(one, one, one, reals)  # type: ignore[call-overload]
    mdl.addquadterm(one, one, one, table)  # type: ignore[call-overload]
    mdl.addquadterm(one, flat, flat, real)  # type: ignore[call-overload]
    mdl.addquadterm(one, flat, flat, table)  # type: ignore[arg-type]
    mdl.addquadterm(one, nested, nested, real)  # type: ignore[call-overload]
    mdl.addquadterm(one, nested, nested, reals)  # type: ignore[arg-type]
    mdl.addquadterm(one, nested, nested, table)  # type: ignore[arg-type]
    mdl.addquadterm(flat, one, one, real)  # type: ignore[call-overload]
    mdl.addquadterm(flat, one, one, reals)  # type: ignore[call-overload]
    mdl.addquadterm(flat, one, one, table)  # type: ignore[call-overload]
    mdl.addquadterm(flat, flat, flat, real)  # type: ignore[call-overload]
    mdl.addquadterm(flat, flat, flat, table)  # type: ignore[arg-type]
    mdl.addquadterm(flat, nested, nested, real)  # type: ignore[call-overload]
    mdl.addquadterm(flat, nested, nested, reals)  # type: ignore[arg-type]


def pairs(mdl: Model) -> None:
    mdl.addquadterm(one, one, flat, real)  # type: ignore[call-overload]
    mdl.addquadterm(one, one, nested, real)  # type: ignore[call-overload]
    mdl.addquadterm(one, flat, one, reals)  # type: ignore[call-overload]
    mdl.addquadterm(one, flat, nested, reals)  # type: ignore[arg-type]
    mdl.addquadterm(flat, flat, one, reals)  # type: ignore[call-overload]
    mdl.addquadterm(flat, flat, nested, reals)  # type: ignore[arg-type]
    mdl.addquadterm(flat, nested, one, table)  # type: ignore[call-overload]
    mdl.addquadterm(flat, nested, flat, table)  # type: ignore[arg-type]


def views(mdl: Model, f: Functions, x: Variables) -> None:
    mdl.addlinterm(f[0], x[0], real)
    mdl.addlinterm(f[0], x, reals)
    mdl.addlinterm(f, x, reals)
    mdl.addlinterm(f, [x, flat], table)
    mdl.addclinterm(x[0], f, reals)
    mdl.addclinterm(x, [f, flat], table)
    mdl.addquadterm(f, [x, x], [x, flat], table)
    mdl.setvarbnd(x[0], real, real)
    mdl.setvarbnd(x, reals, reals)
    mdl.setindvar(one, x[0], 1)
    mdl.setindvar(flat, x, [1, 0])
    mdl.setcst(f[0], [real])  # type: ignore[call-overload]
    mdl.setcst(f, real)  # type: ignore[call-overload]


def variables(mdl: Model) -> None:
    kind = VariableKind.INTEGER
    mdl.setvarbnd(one, real, None)
    mdl.setvarbnd(flat, ub=reals)
    mdl.setvarknd(one, kind)
    mdl.setvarknd(flat, [kind, kind])
    mdl.setvartol(one, real, real)
    mdl.setvartol(flat, reals, None)
    mdl.setvarname(one, name)
    mdl.setvarname(flat, names)
    assert_type(mdl.getvarbnd(one), Pair)
    assert_type(mdl.getvarbnd(flat), Pairs)
    assert_type(mdl.getvarknd(one), VariableKind)
    assert_type(mdl.getvarknd(flat), list[VariableKind])
    assert_type(mdl.getvartol(one), Tolerance)
    assert_type(mdl.getvartol(flat), Tolerances)
    assert_type(mdl.getvar(one), tuple[VariableKind, float, float])
    assert_type(
        mdl.getvar(flat), tuple[list[VariableKind], list[float], list[float]]
    )
    assert_type(mdl.getvarname(one), Label)
    assert_type(mdl.getvarname(flat), Labels)
    mdl.setvarbnd(one, reals)  # type: ignore[call-overload]
    mdl.setvarbnd(flat, ub=real)  # type: ignore[call-overload]
    mdl.setvarknd(one, [kind])  # type: ignore[call-overload]
    mdl.setvarknd(flat, kind)  # type: ignore[call-overload]
    mdl.setvartol(one, rtol=reals)  # type: ignore[call-overload]
    mdl.setvartol(flat, real)  # type: ignore[call-overload]
    mdl.setvarname(one, names)  # type: ignore[call-overload]


def functions(mdl: Model) -> None:
    mdl.addcst(one, real)
    mdl.addcst(flat, reals)
    mdl.setcst(one, None)
    mdl.setcst(flat, reals)
    mdl.setfunname(one, name)
    mdl.setfunname(flat, names)
    mdl.attachexpr(one, 0)
    mdl.attachexpr(flat, [0, 1])
    assert_type(mdl.getcst(one), float)
    assert_type(mdl.getcst(flat), list[float])
    assert_type(mdl.getfunname(one), Label)
    assert_type(mdl.getfunname(flat), Labels)
    mdl.addcst(one, reals)  # type: ignore[call-overload]
    mdl.addcst(flat, real)  # type: ignore[call-overload]
    mdl.setcst(one, reals)  # type: ignore[call-overload]
    mdl.setcst(flat, real)  # type: ignore[call-overload]
    mdl.setfunname(one, names)  # type: ignore[call-overload]
    mdl.attachexpr(one, [0])  # type: ignore[call-overload]
    mdl.attachexpr(flat, 0)  # type: ignore[call-overload]


def constraints(mdl: Model) -> None:
    mdl.setconbnd(one, real, real)
    mdl.setconbnd(flat, reals, reals)
    mdl.setcontol(one, atol=real)
    mdl.setcontol(flat, reals, reals)
    mdl.setconname(one, name)
    mdl.setconname(flat, names)
    assert_type(mdl.getconbnd(one), Pair)
    assert_type(mdl.getconbnd(flat), Pairs)
    assert_type(mdl.getcontol(one), Tolerance)
    assert_type(mdl.getcontol(flat), Tolerances)
    assert_type(mdl.getcon(one), tuple[int, float, float])
    assert_type(mdl.getcon(flat), tuple[list[int], list[float], list[float]])
    assert_type(mdl.getconname(one), Label)
    assert_type(mdl.getconname(flat), Labels)
    mdl.setconbnd(one, real, reals)  # type: ignore[call-overload]
    mdl.setconbnd(flat, real)  # type: ignore[call-overload]
    mdl.setcontol(one, reals)  # type: ignore[call-overload]
    mdl.setcontol(flat, None, real)  # type: ignore[call-overload]
    mdl.setconname(one, names)  # type: ignore[call-overload]


def objectives(mdl: Model) -> None:
    sense = ObjectiveSense.MAXIMIZE
    mdl.setobjsense(one, sense)
    mdl.setobjsense(flat, [sense, sense])
    mdl.setobjprio(one, 2)
    mdl.setobjprio(flat, [2, 1])
    mdl.setobjcoef(one, real)
    mdl.setobjcoef(flat, reals)
    mdl.setobjtol(one, real, real)
    mdl.setobjtol(flat, reals, reals)
    mdl.setobjname(one, name)
    mdl.setobjname(flat, names)
    assert_type(mdl.getobjsense(one), ObjectiveSense)
    assert_type(mdl.getobjsense(flat), list[ObjectiveSense])
    assert_type(mdl.getobjprio(one), int)
    assert_type(mdl.getobjprio(flat), list[int])
    assert_type(mdl.getobjcoef(one), float)
    assert_type(mdl.getobjcoef(flat), list[float])
    assert_type(mdl.getobjtol(one), Pair)
    assert_type(mdl.getobjtol(flat), Pairs)
    assert_type(mdl.getobj(one), tuple[int, ObjectiveSense, int, float])
    assert_type(
        mdl.getobj(flat),
        tuple[list[int], list[ObjectiveSense], list[int], list[float]],
    )
    assert_type(mdl.getobjname(one), Label)
    assert_type(mdl.getobjname(flat), Labels)
    mdl.setobjsense(one, [sense])  # type: ignore[call-overload]
    mdl.setobjsense(flat, sense)  # type: ignore[call-overload]
    mdl.setobjprio(one, [2])  # type: ignore[call-overload]
    mdl.setobjprio(flat, 2)  # type: ignore[call-overload]
    mdl.setobjcoef(one, reals)  # type: ignore[call-overload]
    mdl.setobjcoef(flat, real)  # type: ignore[call-overload]
    mdl.setobjtol(one, reals)  # type: ignore[call-overload]
    mdl.setobjtol(flat, real)  # type: ignore[call-overload]
    mdl.setobjname(one, names)  # type: ignore[call-overload]


def indicators(mdl: Model) -> None:
    mdl.setindbnd(one, real, real)
    mdl.setindbnd(flat, reals, reals)
    mdl.setindvar(one, one, 1)
    mdl.setindvar(flat, flat, [1, 0])
    mdl.setindvar(one, val=0)
    mdl.setindname(one, name)
    mdl.setindname(flat, names)
    assert_type(mdl.getindbnd(one), Pair)
    assert_type(mdl.getindbnd(flat), Pairs)
    assert_type(mdl.getindvar(one), tuple[int, int])
    assert_type(mdl.getindvar(flat), tuple[list[int], list[int]])
    assert_type(mdl.getind(one), tuple[int, int, int, float, float])
    assert_type(
        mdl.getind(flat),
        tuple[list[int], list[int], list[int], list[float], list[float]],
    )
    assert_type(mdl.getindname(one), Label)
    assert_type(mdl.getindname(flat), Labels)
    mdl.setindbnd(one, reals)  # type: ignore[call-overload]
    mdl.setindbnd(flat, real)  # type: ignore[call-overload]
    mdl.setindvar(one, flat)  # type: ignore[call-overload]
    mdl.setindvar(flat, one)  # type: ignore[call-overload]
    mdl.setindvar(one, val=[0])  # type: ignore[call-overload]
    mdl.setindvar(flat, val=0)  # type: ignore[call-overload]
    mdl.setindname(one, names)  # type: ignore[call-overload]


def others(mdl: Model) -> None:
    assert_type(mdl.compare(mdl), str | None)
    mdl.setsosname(one, name)
    mdl.setsosname(flat, names)
    mdl.setconetol(one, real, real)
    mdl.setconetol(flat, reals, reals)
    mdl.setconename(one, name)
    mdl.setconename(flat, names)
    mdl.setcconbnd(one, real, real)
    mdl.setcconbnd(flat, reals, reals)
    mdl.setccontol(one, real, real)
    mdl.setccontol(flat, reals, reals)
    mdl.setcconname(one, name)
    mdl.setcconname(flat, names)
    assert_type(mdl.getsosname(one), Label)
    assert_type(mdl.getsosname(flat), Labels)
    assert_type(mdl.getconetol(one), Tolerance)
    assert_type(mdl.getconetol(flat), Tolerances)
    assert_type(mdl.getconename(one), Label)
    assert_type(mdl.getconename(flat), Labels)
    assert_type(mdl.getcconbnd(one), Pair)
    assert_type(mdl.getcconbnd(flat), Pairs)
    assert_type(mdl.getccontol(one), Tolerance)
    assert_type(mdl.getccontol(flat), Tolerances)
    assert_type(mdl.getccon(one), tuple[int, int, float, float])
    assert_type(
        mdl.getccon(flat),
        tuple[list[int], list[int], list[float], list[float]],
    )
    assert_type(mdl.getcconname(one), Label)
    assert_type(mdl.getcconname(flat), Labels)
    mdl.setsosname(one, names)  # type: ignore[call-overload]
    mdl.setconetol(one, reals)  # type: ignore[call-overload]
    mdl.setconetol(flat, real)  # type: ignore[call-overload]
    mdl.setconename(one, names)  # type: ignore[call-overload]
    mdl.setcconbnd(one, reals)  # type: ignore[call-overload]
    mdl.setcconbnd(flat, real)  # type: ignore[call-overload]
    mdl.setccontol(one, reals)  # type: ignore[call-overload]
    mdl.setccontol(flat, real)  # type: ignore[call-overload]
    mdl.setcconname(one, names)  # type: ignore[call-overload]


def lookups(mdl: Model) -> None:
    assert_type(mdl.getvaridx(names), Variables)
    assert_type(mdl.getfunidx(names), Functions)
    assert_type(mdl.getconidx(names), Constraints)
    assert_type(mdl.getobjidx(names), Objectives)
    assert_type(mdl.getindidx(names), Indicators)
    assert_type(mdl.getsosidx(names), SpecialOrderedSets)
    assert_type(mdl.getconeidx(names), Cones)
    assert_type(mdl.getcconidx(names), Complements)
