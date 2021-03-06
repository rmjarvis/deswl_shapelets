#ifndef ELLIPSESOLVER_H
#define ELLIPSESOLVER_H

#include <vector>
#include "NLSolver.h"
#include "dbg.h"
#include "Pixel.h"
#include "BVec.h"

class EllipseSolver : public NLSolver
{
public :

    EllipseSolver(
        const BVec& b0, int order,
        bool fixcen=false, bool fixgam=false, bool fixmu=false);
    ~EllipseSolver();

    void calculateF(const DVector& x, DVector& f) const;
    void calculateJ(const DVector& x, const DVector& f, DMatrix& df) const;

    void useNumericJ();
    void dontZeroB11();
    void getCovariance(const DMatrix& b0Cov, DMatrix& cov) const;
    void getInverseCovariance(const DMatrix& b0Cov, DMatrix& invcov) const;

    // callF takes x and f of length 5, rather than whatever shorter
    // length that calculateF takes (depending on if things are fixed).
    void callF(const DVector& x, DVector& f) const;
    void callJ(const DVector& x, const DVector& f, DMatrix& j) const;

    bool solve(DVector& x, DVector& f) const;
    bool testJ(const DVector& x, DVector& f,
               std::ostream* os=0, double relerr=0.) const;

private :

    struct ESImpl;
    ESImpl* _pimpl;

    // private -- shouldn't ever be used.
    // But need them to avoid the above version hiding the virtual fn.
    void getCovariance(DMatrix& ) const {}
    void getInverseCovariance(DMatrix& ) const {}

};

#endif
