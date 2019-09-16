//
// Created by adrien_bedel on 09/09/19.
//

#include <ConstraintExpression.h>
#include <Expression.h>
#include "Model.h"




template <typename T>
void SLRModel<T>::printExpression(const SLRExpr<T> &expr) const
{
    std::cout << "Expression : ";
    for (int i = 0; i <= expr._varIndex; i++)
    {
        if (expr._coeffs[i] != 1)
            std::cout << expr._coeffs[i];
        for (int j = 0; j < expr._vars[i].size(); j++)
        {
            std::cout << expr._vars[i][j].getName();
        }
        std::cout << " ";
        if (i != expr._varIndex)
            std::cout << "+";
        std::cout << " ";
    }
    if (expr._constant != 0)
        std::cout << "+ " << expr._constant;
    std::cout << std::endl;
}

template <typename T>
void SLRModel<T>::printExpression(const SLRConstrExpr<T> &constrExpr) const
{
    std::cout << "Constraint : " << std::endl << " - ";
    printExpression(constrExpr._expr);
    std::cout << " - Sign       : ";
    if (constrExpr._sign == INF)
        std::cout << "<=";
    else if (constrExpr._sign == SUP)
        std::cout << ">=";
    else if (constrExpr._sign == EQUAL)
        std::cout << "==";
    std::cout << std::endl << " - Constraint : " << constrExpr._constr << std::endl;
}






#ifdef GRB

template <typename T>
SLRModel<T>::SLRModel() : _env(true)
{
    _env.set("LogFile", "mip1.log");
    _env.start();
    _model = std::make_shared<GRBModel>(_env);

}

template <typename T>
GRBQuadExpr      SLRModel<T>::SLRExprToGRBLineExpr(const SLRExpr<T> &expr)
{
    GRBQuadExpr finalExpr;
    for (int i =  0; i < expr._vars.size(); i++)
    {
        GRBQuadExpr  grbLinExpr;
        if (expr._vars[i].size() == 2)
        {
            grbLinExpr = _vars[expr._vars[i][0][_varsVector]] * _vars[expr._vars[i][1][_varsVector]] * expr._coeffs[i];
        }
        else
        {
            grbLinExpr = _vars[expr._vars[i][0][_varsVector]] * expr._coeffs[i];

        }
            finalExpr += grbLinExpr;
    }
    finalExpr += expr._constant;
    return (finalExpr);
}

template <typename T>
int     SLRModel<T>::setObjective(const SLRExpr<T> &expr, int goal)
{
    printExpression(expr);
    GRBQuadExpr grbExpr = SLRExprToGRBLineExpr(expr);
    _model->setObjective(grbExpr, goal);
    return (1);

}

template <typename T>
int     SLRModel<T>::addConstr(const SLRConstrExpr<T> &constrExpr, const std::string &name)
{
    GRBQuadExpr grbExpr = SLRExprToGRBLineExpr(constrExpr._expr);
    if (constrExpr._sign == INF)
        _model->addConstr(grbExpr <= constrExpr._constr, name);
    if (constrExpr._sign == SUP)
        _model->addConstr(grbExpr >= constrExpr._constr, name);
    if (constrExpr._sign == EQUAL)
        _model->addConstr(grbExpr == constrExpr._constr, name);

    return (1);
}

template <typename T>
void    SLRModel<T>::optimize()
{
    _model->optimize();
}

template <typename T>
SLRVar<T>   SLRModel<T>::addVar(const T &lowerBound, const T &upperBound, const T &solution, const std::string &name)
{
    SLRVar<T>   variable(lowerBound, upperBound, solution, name);

    _varsVector.push_back(variable);
    _nbVar++;

    if (std::is_same<T, bool>::value)
        _vars.push_back(_model->addVar(lowerBound, upperBound, solution, GRB_BINARY, name));
    else if (std::is_same<T, int>::value)
        _vars.push_back(_model->addVar(lowerBound, upperBound, solution, GRB_INTEGER, name));
    else if (std::is_same<T, float>::value || std::is_same<T, double>::value)
        _vars.push_back(_model->addVar(lowerBound, upperBound, solution, GRB_CONTINUOUS, name));
//else
    // send error

    return (variable);
}

template <typename T>
void    SLRModel<T>::printResult()
{
    for (auto const &var : _vars)
    {
        std::cout << var.get(GRB_StringAttr_VarName) << " "
                  << var.get(GRB_DoubleAttr_X) << std::endl;
    }
}


#elif OSQP

template <typename T>
SLRModel<T>::SLRModel() : _nbVar(0), _nbConstr(0)
{
    _quadricNb = 0;
}

template <typename T>
SLRVar<T>   SLRModel<T>::addVar(const T &lowerBound, const T &upperBound, const T &solution, const std::string &name)
{
    SLRVar<T>   variable(lowerBound, upperBound, solution, name);

    _varsVector.push_back(variable);
    _nbVar++;

    return (variable);
}

template <typename T>
void SLRModel<T>::updateVariableConstraints()
{

    for (auto const &var : _varsVector)
    {
        std::vector<double> constrCoeff(_nbVar, 0);
        _lowerBound.push_back(var.getLowerBound());
        _upperBound.push_back(var.getUpperBound());
        constrCoeff[var[_varsVector]] = 1;
        _constrCoeffs.push_back(constrCoeff);
        _nbConstr++;
    }
}

template <typename T>
int SLRModel<T>::setObjective(const SLRExpr<T> &expr, int goal)
{
    double   fullMatrix[_nbVar][_nbVar];

    updateVariableConstraints();
    _linearCoeffs = (c_float *)malloc(sizeof(c_float) * _nbVar);
    memset(fullMatrix, 0, sizeof (double) * (_nbVar * _nbVar));
    memset(_linearCoeffs, 0, sizeof (c_float) * (_nbVar));
    for (int i = 0; i < expr._vars.size(); i++)
    {
        if (expr._vars[i].size() == 2)
        {
            fullMatrix[expr._vars[i][0][_varsVector]][expr._vars[i][1][_varsVector]] = expr._coeffs[i];
            _quadricNb++;
        }
        else
        {
            _linearCoeffs[expr._vars[i][0][_varsVector]] = expr._coeffs[i];
        }
    }

    _quadricCoeffs = (c_float *)malloc(sizeof(c_float) * _quadricNb);
    _coeffsRaws = (c_int *)malloc(sizeof(c_int) * _quadricNb);
    _coeffsColumns = (c_int *)malloc(sizeof(c_int) * (_nbVar + 1));

    memset(_coeffsColumns, 0, sizeof (c_int) * _quadricNb);
    _quadricNb = 0;
    for (int i = 0; i < _nbVar; i++)
    {
        for (int j = 0; j < _nbVar; j++)
        {
            if (fullMatrix[j][i] != 0)
            {
                if (i != j)
                    fullMatrix[j][i] /= 2.f;
                _quadricCoeffs[_quadricNb] = fullMatrix[j][i];
                _coeffsRaws[_quadricNb] = j;
                _coeffsColumns[i + 1]++;
                _quadricNb++;
            }
        }
    }
    _coeffsColumns[0] = 0;
    for (int i = 1; i < _nbVar + 1; i++)
    {
        _coeffsColumns[i] += _coeffsColumns[i - 1];
    }
    printExpression(expr);
    return (1);
}

template <typename T>
int         SLRModel<T>::addConstr(const SLRConstrExpr<T> &constrExpr, const std::string &name)
{
    // only linear constraints

    printExpression(constrExpr);
    std::vector<double> constrCoeff(_nbVar, 0);
    for (int i = 0; i < constrExpr._expr._vars.size(); i++)
    {
        constrCoeff[constrExpr._expr._vars[i][0][_varsVector]] = constrExpr._expr._coeffs[i];
    }
    _constrCoeffs.push_back(constrCoeff);
    if (constrExpr._sign == EQUAL)
    {
        _lowerBound.push_back(constrExpr._constr);
        _upperBound.push_back(constrExpr._constr);
    }
    else if (constrExpr._sign == INF)
    {
        _lowerBound.push_back(std::numeric_limits<double>::min());
        _upperBound.push_back(constrExpr._constr);
    }
    else if (constrExpr._sign == SUP)
    {
        _lowerBound.push_back(constrExpr._constr);
        _upperBound.push_back(std::numeric_limits<double>::max());
    }
    _nbConstr++;
    return (1);
}

template <typename T>
void    SLRModel<T>::printOSQPVariables(OSQPData &data)
{
    std::cout << "Begin ----------" << std::endl;
    std::cout << data.m << std::endl;
    std::cout << data.n << std::endl;
    std::cout <<  data.A->nzmax<< std::endl << "x = ";

    for (int i = 0; i < data.A->nzmax; i++)
        std::cout << data.A->x[i] << " ";
    std::cout << std::endl << "i = ";
    for (int i = 0; i < data.A->nzmax; i++)
        std::cout << data.A->i[i]<< " ";
    std::cout << std::endl << "p = ";
    for (int i = 0; i < data.n + 1; i++)
        std::cout << data.A->p[i] << " ";
    std::cout << std::endl << "l = ";
    for (int i = 0; i < data.m; i++)
        std::cout << data.l[i] << " ";
    std::cout << std::endl << "u = ";
    for (int i = 0; i < data.m; i++)
        std::cout << data.u[i] << " ";
    std::cout << std::endl;

    std::cout <<  data.P->nzmax<< std::endl << "x = ";

    for (int i = 0; i < data.P->nzmax; i++)
        std::cout << data.P->x[i] << " ";
    std::cout << std::endl << "i = ";
    for (int i = 0; i < data.P->nzmax; i++)
        std::cout << data.P->i[i] << " ";
    std::cout << std::endl << "p = ";
    for (int i = 0; i < data.n + 1; i++)
        std::cout << data.P->p[i] << " ";
    std::cout << std::endl << "q = ";
    for (int i = 0; i < data.n; i++)
        std::cout << data.q[i] << " ";
    std::cout << std::endl;
    std::cout << "End ----------" << std::endl;

}

template <typename T>
void    SLRModel<T>::fillData()
{
    c_int nonZeroCoeffNb = 0;
    for (int i = 0; i < _constrCoeffs.size(); i++)
    {
        for (int j = 0; j < _constrCoeffs[i].size(); j++)
        {
            if (_constrCoeffs[i][j] != 0)
                nonZeroCoeffNb++;
        }
    }
    c_float   *nonZeroCoeffs = (c_float*)malloc(sizeof(c_float) * nonZeroCoeffNb);
    c_int   *coeffsRaws = (c_int*)malloc(sizeof(c_int) * nonZeroCoeffNb);
    c_int   *coeffsColumns = (c_int*)malloc(sizeof(c_int) * (_nbVar + 1));

    memset(coeffsColumns, 0, sizeof(c_int) * (_nbVar + 1));

    nonZeroCoeffNb = 0;
    for (int i = 0; i < _constrCoeffs[0].size(); i++)
    {
        for (int j = 0; j < _constrCoeffs.size(); j++)
        {
            if (_constrCoeffs[j][i] != 0)
            {
                nonZeroCoeffs[nonZeroCoeffNb] = _constrCoeffs[j][i];
                coeffsRaws[nonZeroCoeffNb] = j;
                coeffsColumns[i + 1]++;
                nonZeroCoeffNb++;
            }
        }
    }
    for (int i = 1; i < _nbVar + 1; i++)
    {
        coeffsColumns[i] += coeffsColumns[i - 1];
    }

    c_float *lowerBound = (c_float *)malloc(sizeof(c_float) * _lowerBound.size());
    c_float *upperBound = (c_float *)malloc(sizeof(c_float) * _upperBound.size());
    std::copy(_lowerBound.begin(), _lowerBound.end(), lowerBound);
    std::copy(_upperBound.begin(), _upperBound.end(), upperBound);

    _data.m = _constrCoeffs.size();
    _data.n = _nbVar;
    _data.A = csc_matrix(_data.m, _nbVar, nonZeroCoeffNb, nonZeroCoeffs, coeffsRaws, coeffsColumns);
    _data.l = lowerBound;
    _data.u = upperBound;
    _data.P = csc_matrix(_nbVar, _nbVar, _quadricNb, _quadricCoeffs, _coeffsRaws, _coeffsColumns);
    _data.q = _linearCoeffs;
}

template <typename T>
void    SLRModel<T>::optimize()
{

    fillData();

    printOSQPVariables(_data);

    c_int exitflag = 0;
    osqp_set_default_settings(&_settings);
    _settings.alpha = 1.0; // Change alpha parameter
    exitflag = osqp_setup(&_work, &_data, &_settings);
    osqp_solve(_work);
    //return exitlag;
}

template <typename T>
void    SLRModel<T>::printResult()
{
    std::cout << _work->solution[0].x[0] << " " << _work->solution[0].x[1] << " " << _work->solution[0].x[2] << std::endl;
}


#endif