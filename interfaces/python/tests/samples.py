LP_MPS = """\
* A small linear program
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
"""

ROWS_MPS = """\
NAME
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
"""


def written(path, text):
    path.write_text(text)
    return path
