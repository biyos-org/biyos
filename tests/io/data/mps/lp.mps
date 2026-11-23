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
