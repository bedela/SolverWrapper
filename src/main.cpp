//
// Created by adrien_bedel on 09/09/19.
//

#include <vector>
#include <iostream>
#include "Expression.h"
#include "ConstraintExpression.h"

#ifndef GRB
#ifndef OSQP
#error compile selecting one solver, i.e. include in compiler parameters: -DOSQP or -DGRB
#endif
#endif


int main() {
    SLRModel<double> model;

    SLRVar<double> x = model.addVar(0.0, 1.7, 0.0, "x");
    SLRVar<double> y = model.addVar(0.6, 0.7, 0.0, "y");
    SLRVar<double> z = model.addVar(-1.0, -0.7, 0.0, "z");

    model.setObjective(x + y + z + y * z + x * x + y * y + z * z, SLR_MINIMIZE);
    //model.addConstr(x + -2 >= -2 + -1 * z + -2 * y, "c1");
    model.addConstr(x + y + z == 1, "c0");
    //model.addConstr(x + 2 * y + z + 1 == 10, "c1");
    model.optimize();
    model.printResult();
    return 0;
}
