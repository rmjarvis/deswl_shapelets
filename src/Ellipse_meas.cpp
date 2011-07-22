
#include "Ellipse.h"
#include "EllipseSolver.h"
#include <cmath>
#include "dbg.h"
#include <fstream>
#include "PsiHelper.h"
#include "Params.h"

const double MAX_GAMMA_PRIOR = 0.7;

bool Ellipse::doMeasure(
    const std::vector<PixelList>& pix,
    const std::vector<BVec>* psf,
    int galOrder, int galOrder2, int maxm,
    double sigma, long& flag, double thresh, DSmallMatrix22* cov, 
    const Ellipse* ell_meas)
{
    if (!(ell_meas)) ell_meas = this;

    dbg<<"Start DoMeasure: galOrder = "<<galOrder<<", psf = "<<bool(psf)<<std::endl;
    dbg<<"fix = "<<_isFixedCen<<"  "<<_isFixedGamma<<"  "<<_isFixedMu<<std::endl;
    dbg<<"Thresh = "<<thresh<<std::endl;
    dbg<<"ell_meas = "<<*ell_meas<<std::endl;
    int npix = 0;
    for(size_t i=0;i<pix.size();++i) {
        xdbg<<"npix["<<i<<"] = "<<pix[i].size()<<std::endl;
        npix += pix[i].size();
    }

    int galSize = (galOrder+1)*(galOrder+2)/2;
    if (npix <= galSize) {
        dbg<<"Too few pixels ("<<npix<<") for given gal_order. \n";
        return false;
    }

    BVec b(galOrder,sigma);
    std::auto_ptr<DMatrix> bCov;
    bool doMeanLikelihood = false;
    //bool doMeanLikelihood = psf && !_isFixedGamma;
    if (cov || doMeanLikelihood) 
        bCov.reset(new DMatrix(int(b.size()),int(b.size())));

    if (!ell_meas->doMeasureShapelet(
            pix,psf,b,galOrder,galOrder2,maxm,bCov.get())) {
        xdbg<<"Could not measure a shapelet vector.\n";
        return false;
    }
    if (!b(0) > 0) {
        xdbg<<"Bad flux in measured shapelet\n";
        return false;
    }
    dbg<<"b = "<<b<<std::endl;

    return findRoundFrame(b,psf,*ell_meas,galOrder2,thresh,flag,bCov.get(),cov);
}

static double calculateLikelihood(
    const std::complex<double>& gamma,
    const Ellipse& ell_meas, const BVec& b, const DMatrix& bCov)
{
    static std::auto_ptr<DMatrix> Gg1;
    static std::auto_ptr<DMatrix> Gg2;
    static std::auto_ptr<DMatrix> Gth;

    if (!Gg1.get() || Gg1->colsize() < b.size()) {
        Gg1.reset(new DMatrix(b.size(),b.size()));
        Gg2.reset(new DMatrix(b.size(),b.size()));
        Gth.reset(new DMatrix(b.size(),b.size()));
        setupGg1(*Gg1,b.getOrder(),b.getOrder());
        setupGg2(*Gg2,b.getOrder(),b.getOrder());
        setupGth(*Gth,b.getOrder(),b.getOrder());
    }

    Ellipse e1;
    e1.setGamma(gamma);
    e1.postShiftBy(0.,-ell_meas.getGamma(),0.);
    e1.removeRotation();
    std::complex<double> dg = e1.getGamma();
    xdbg<<"e1 = "<<e1<<std::endl;
    xdbg<<"gamma = "<<gamma<<std::endl;
    xdbg<<"dg = "<<dg<<std::endl;
    DMatrix S(6,b.size());
    calculateGTransform(dg,2,b.getOrder(),S);
    xdbg<<"S = "<<S<<std::endl;
    DVector bx = S.rowRange(3,5) * b.vec();
    xdbg<<"db = "<<bx<<std::endl;
    DMatrix cx = S.rowRange(3,5) * bCov * S.rowRange(3,5).transpose();
    xdbg<<"c = "<<cx<<std::endl;
    double chisq = bx * (bx/cx);
    xdbg<<"chisq("<<gamma<<") = "<<chisq<<std::endl;
    double l = exp(-chisq/2.);
    xdbg<<"l = exp(-chisq/2) = "<<l<<std::endl;
    l /= sqrt(Det(cx));  // ignore the 2pi factor
    xdbg<<"l /= sqrt(det(c)) = "<<l<<std::endl;

    // So far, this likelihood is a probability density in terms
    // of bx, not in terms of gamma.
    // dP/dgamma = |dbx/dgamma| dP/dbx
    // where |dbx/dgamma| in two dimensions is really the determinant
    // of the jacobian: dbx_1,2 / dgamma_1,2.
    // bx = S b
    // dbx / dgamma_1 = dS/dgamma_1 b
    //                = 1/(1-|g|^2) * S * (Gg1 + g2*Gth) b
    // dbx / dgamma_2 = 1/(1-|g|^2) * S * (Gg2 - g1*Gth) b
    double g1 = real(gamma);
    double g2 = imag(gamma);
    DVector Gthb = Gth->subMatrix(0,b.size(),0,b.size()) * b.vec();
    DVector Gg1b = Gg1->subMatrix(0,b.size(),0,b.size()) * b.vec();
    DVector Gg2b = Gg2->subMatrix(0,b.size(),0,b.size()) * b.vec();
    DMatrix jac(2,2);
    jac.col(0) = S.rowRange(3,5) * (Gg1b + g2*Gthb);
    jac.col(1) = S.rowRange(3,5) * (Gg2b - g1*Gthb);
    jac /= 1.-norm(gamma);
    xdbg<<"jac = "<<jac<<std::endl;
    xdbg<<"det(jac) = "<<Det(jac)<<std::endl;
    l *= std::abs(Det(jac));
    xdbg<<"l *= |det(jac)| => "<<l<<std::endl;
    return l;
}

bool Ellipse::findRoundFrame(
    const BVec& b, bool psf, const Ellipse& ell_meas,
    int galOrder2, double thresh,
    long& flag, const DMatrix* bCov, DSmallMatrix22* cov)
{
    double trCov = cov ? Trace(*cov) : 0.;
    bool doMeanLikelihood = false;
    bool doMaxLikelihood = true;
    //bool doMeanLikelihood = psf && !_isFixedGamma;
    //bool doMaxLikelihood = !doMeanLikelihood || trCov == 0.0;

    if (doMaxLikelihood) {
        dbg<<"Start Maximum Likelihood estimate\n";
        dbg<<"current value = "<<*this<<std::endl;
        dbg<<"ell_meas = "<<ell_meas<<std::endl;

        DVector x(5);
        DVector f(5);

        double sigma = b.getSigma();
        Ellipse temp = *this;
        temp.postShiftBy(
            -ell_meas.getCen(),-ell_meas.getGamma(),-ell_meas.getMu());
        temp.removeRotation();
        dbg<<"starting guess = "<<temp<<std::endl;
        x[0] = temp.getCen().real()/sigma;
        x[1] = temp.getCen().imag()/sigma;
        x[2] = temp.getGamma().real();
        x[3] = temp.getGamma().imag();
        x[4] = temp.getMu();
        dbg<<"x = "<<x<<std::endl;
        DVector xinit = x;

        EllipseSolver solver(b,galOrder2,_isFixedCen,_isFixedGamma,_isFixedMu);

#ifdef NOTHROW
        solver.noUseCholesky();
#endif
        double ftol = thresh*thresh;
        double gtol = thresh*ftol;
        solver.setTol(ftol,gtol);
        solver.setMinStep(gtol*thresh);
        solver.setOutput(*dbgout);
        if (XDEBUG) solver.useVerboseOutput();
        solver.setMinStep(1.e-6*gtol);
        solver.setDelta0(0.01);
        solver.setMaxIter(200);
        if (psf && !_isFixedMu) {
            solver.setDelta0(0.01);
            solver.useDogleg();
            solver.dontZeroB11();
            solver.useSVD();
        } else {
            solver.useHybrid();
        }
        if (solver.solve(x,f)) {
            dbg<<"Found good round frame:\n";
            dbg<<"x = "<<EIGEN_Transpose(x)<<std::endl;
            dbg<<"f = "<<EIGEN_Transpose(f)<<std::endl;
            if (!doMeanLikelihood) {
                double f_normInf = f.TMV_normInf();
                if (psf && !_isFixedMu && !(f_normInf < solver.getFTol())) {
                    xdbg<<"Oops, Local minimum, not real solution.\n";
                    xdbg<<"f.norm = "<<f.norm()<<std::endl;
                    xdbg<<"f.normInf = "<<f_normInf<<std::endl;
                    xdbg<<"ftol = "<<solver.getFTol()<<std::endl;
                    dbg<<"FLAG SHEAR_LOCAL_MIN\n";
                    flag |= SHEAR_LOCAL_MIN;
                    return false;
                }
            }
        } else {
            dbg<<"findRoundFrame solver failed:\n";
            dbg<<"x = "<<EIGEN_Transpose(x)<<std::endl;
            dbg<<"f = "<<EIGEN_Transpose(f)<<std::endl;
            if (!doMeanLikelihood) {
                //if (XDEBUG) if (!solver.testJ(x,f,dbgout,1.e-5)) exit(1);
                dbg<<"FLAG SHEAR_DIDNT_CONVERGE\n";
                flag |= SHEAR_DIDNT_CONVERGE;
                return false;
            }
        }

        dbg<<"done: x = "<<x<<std::endl;
        dbg<<"Norm(delta x) = "<<Norm(x-xinit)<<std::endl;
        *this = ell_meas;
        preShiftBy(std::complex<double>(x(0),x(1))*sigma,
                   std::complex<double>(x(2),x(3)),
                   x(4));
        removeRotation();

        dbg<<"ell => "<<*this<<std::endl;

        if (cov) {
            Assert(bCov);
            solver.useSVD();
            DMatrix cov5(5,5);
            DMatrix j(5,5);
            solver.calculateJ(x,f,j); // Make sure jj is calculated.
            solver.getCovariance(*bCov,cov5);
            dbg<<"cov5 = "<<cov5<<std::endl;
            *cov = cov5.TMV_subMatrix(2,4,2,4);
            if (!(cov->TMV_det() > 0.)) {
                dbg<<"cov has bad determinant: "<<
                    cov->TMV_det()<<std::endl;
                dbg<<"cov = "<<*cov<<std::endl;
                dbg<<"Full cov = "<<cov5<<std::endl;
                dbg<<"FLAG SHEAR_BAD_COVAR\n";
                flag |= SHEAR_BAD_COVAR;
            }
            // update trCov for next section
            trCov = cov ? Trace(*cov) : 0.;
        } else if (doMeanLikelihood) {
            // Then need good estimate of trCov for step size.
            Assert(bCov);
            solver.useSVD();
            DMatrix cov5(5,5);
            DMatrix j(5,5);
            solver.calculateJ(x,f,j);
            solver.getCovariance(*bCov,cov5);
            xdbg<<"cov5 = "<<cov5<<std::endl;
            trCov = Trace(cov5.TMV_subMatrix(2,4,2,4));
        }
    }

    if (doMeanLikelihood) {
        Assert(bCov);
        Assert(cov);
        dbg<<"Starting likelihood-weighted mean calculation.\n";
        //dbg<<"b = "<<b<<std::endl;
        //dbg<<"bCov = "<<*bCov<<std::endl;
        std::complex<double> gamma = getGamma();
        dbg<<"gamma = "<<gamma<<std::endl;

#if 1
        {
            Ellipse e1;
            e1.setGamma(gamma);
            e1.postShiftBy(0.,-ell_meas.getGamma(),0.);
            e1.removeRotation();
            dbg<<"e1 = "<<e1<<std::endl;
            std::complex<double> dg = e1.getGamma();
            dbg<<"dg = "<<dg<<std::endl;
            DMatrix S(6,b.size());
            calculateGTransform(dg,2,b.getOrder(),S);
            dbg<<"S = "<<S<<std::endl;
            DVector b1 = S * b.vec();
            dbg<<"Sheared b = "<<b1<<std::endl;
            DMatrix c1 = S * (*bCov) * S.transpose();
            dbg<<"c1 = "<<c1<<std::endl;
            DVector b2 = b1.subVector(3,5);
            DMatrix c2 = c1.subMatrix(3,5,3,5);
            double chisq = b2 * (b2/c2);
            dbg<<"chisq("<<gamma<<") = "<<chisq<<std::endl;
        }
#endif

        dbg<<"cov = "<<*cov<<std::endl;
        dbg<<"trCov = "<<trCov<<std::endl;
        double g1c = real(gamma);
        double g2c = imag(gamma);
        double step = sqrt(trCov/2.);
        if (step > 0.1) step = 0.1;
        dbg<<"step = "<<step<<std::endl;
        const int Ngrid = 10;
        double suml=0.;
        std::complex<double> sumlg=0.;
        double sumlg1g1=0., sumlg1g2=0., sumlg2g2=0.;
        DMatrix like(2*Ngrid,2*Ngrid,0.);
        dbg<<"g1 = "<<(g1c+(-Ngrid+0.5)*step)<<" ... "<<(g1c+(Ngrid-0.5)*step)<<std::endl;
        dbg<<"g2 = "<<(g2c+(-Ngrid+0.5)*step)<<" ... "<<(g2c+(Ngrid-0.5)*step)<<std::endl;
        for(int i=-Ngrid;i<Ngrid;++i) for(int j=-Ngrid;j<Ngrid;++j) {
            double g1 = g1c + (i+0.5)*step;
            double g2 = g2c + (j+0.5)*step;
            std::complex<double> gtry(g1,g2);
            xdbg<<"gtry = "<<gtry<<std::endl;
            if (abs(gtry) >= MAX_GAMMA_PRIOR) {
                xdbg<<"|g| >= "<<MAX_GAMMA_PRIOR<<"\n";
                continue;
            }
            double l = calculateLikelihood(gtry,ell_meas,b,*bCov);
            // This gives us P(data | gamma)
            // To get P(gamma | data), we need to multiply by a 
            // prior, P(gamma).
#if 0
            // This next line takes the prior to be the local gaussian
            // around the maximum likelihood answer.
            DVector dg(2);  dg << (g1-g1c), (g2-g2c);
            double Pg = dg * (dg / (*cov));
            dbg<<"dg * C^-1 * dg = "<<Pg<<std::endl;
            Pg = exp(-Pg/2.);
            dbg<<"exp(-dg * C * dg/2.) = "<<Pg<<std::endl;
            Pg /= sqrt(Det(*cov));
            dbg<<"1/sqrt(|C|) exp(-dg * C * dg/2.) = "<<Pg<<std::endl;
            l *= Pg;
#elif 0
            // This gives us the P(gamma) used in Great08.
            // P(e) = e (cos(pi e/2))^2 exp(-(2e/B)^C)
            // e = 2g/(1+g^2)
            // B = 0.05 or 0.19 (for bulge or disk respectively)
            // C = 0.58
            // For this, we take B = 0.1 as a compromise value.
            // Also, for e > 0.9, P(e) = 0.
            double absg = std::abs(gtry);
            double gsq = absg*absg;
            double e = 2.*absg / (1.+gsq);
            if (e > 0.9) {
                l = 0.;
            } else {
                const double PI = 2.*asin(1.);
                double p1 = cos(PI * e/2.);
                p1 *= p1;
                double B = 0.10;
                double C = 0.58;
                double p2 = exp(-std::pow(2.*e/B,C));
                double pe = e * p1 * p2;
                // Now dp/dg = dp/de * de/dg
                // de/dg = ((1+g^2)*2 - 2g*2g) / (1+g^2)^2
                //       = 2(1-g^2) / (1+g^2)^2

                double pg = pe * 2. * (1.-gsq) / std::pow(1.+gsq,2.);
                // Finally, the two dimensional prior is really g P(g)
                l *= absg * pg;
            }
#endif
            like(i+Ngrid,j+Ngrid) = l;
            sumlg += l * gtry;
            suml += l;
            sumlg1g1 += l * g1*g1;
            sumlg1g2 += l * g1*g2;
            sumlg2g2 += l * g2*g2;
        }
        sumlg /= suml;

        dbg<<"like = "<<like<<std::endl;
        dbg<<"middle rows = \n";
        dbg<<"g1 = "<<g1c-0.5*step<<" , "<<g1c+0.5*step<<"  g2 = "<<(g2c-(Ngrid-0.5)*step)<<" ... "<<(g2c+(Ngrid-0.5)*step)<<std::endl;
        dbg<<like.row(Ngrid-1)<<std::endl;
        dbg<<like.row(Ngrid)<<std::endl;
        dbg<<"middle cols = \n";
        dbg<<"g2 = "<<g2c-0.5*step<<" , "<<g2c+0.5*step<<"  g1 = "<<(g1c-(Ngrid-0.5)*step)<<" ... "<<(g1c+(Ngrid-0.5)*step)<<std::endl;
        dbg<<like.col(Ngrid-1)<<std::endl;
        dbg<<like.col(Ngrid)<<std::endl;
        dbg<<"likelihood-weighted mean = "<<sumlg<<std::endl;
        dbg<<"maximum likelihood value = "<<gamma<<std::endl;
        setGamma(sumlg);
        if (cov) {
            sumlg1g1 /= suml;  sumlg1g1 -= real(sumlg) * real(sumlg);
            sumlg1g2 /= suml;  sumlg1g2 -= real(sumlg) * imag(sumlg);
            sumlg2g2 /= suml;  sumlg2g2 -= imag(sumlg) * imag(sumlg);
            dbg<<"old cov = "<<(*cov)(0,0)<<"  "<<(*cov)(0,1)<<"  "<<(*cov)(1,1)<<std::endl;
            dbg<<"new cov = "<<sumlg1g1<<"  "<<sumlg1g2<<"  "<<sumlg2g2<<std::endl;
            (*cov)(0,0) = sumlg1g1;
            (*cov)(0,1) = sumlg1g2;
            (*cov)(1,0) = sumlg1g2;
            (*cov)(1,1) = sumlg2g2;
        }
    }

    return true;
}

void Ellipse::correctForBias(
    const std::vector<PixelList>& pix,
    const std::vector<BVec>& psf,
    int galOrder, int galOrder2, int maxm,
    double sigma, const Ellipse* ell_meas)
{
#if 0
    if (!(ell_meas)) ell_meas = this;

    dbg<<"Start correctForBias: galOrder = "<<galOrder<<std::endl;
    dbg<<"fix = "<<_isFixedCen<<"  "<<_isFixedGamma<<"  "<<_isFixedMu<<std::endl;
    dbg<<"ell_meas = "<<*ell_meas<<std::endl;
    int galSize = (galOrder+1)*(galOrder+2)/2;

    BVec b(galOrder,sigma);
    DMatrix bCov(b.size(),b.size());

    if (!ell_meas->measureShapelet(
            pix,psf,b,galOrder,galOrder2,maxm,&bCov)) {
        dbg<<"Could not measure a shapelet vector.\n";
        return;
    }
    if (!b(0) > 0) {
        dbg<<"Bad flux in measured shapelet\n";
        return;
    }
    dbg<<"b = "<<b<<std::endl;
    xdbg<<"bCov = "<<bCov<<std::endl;

    int galSize2 = (galOrder2+1)*(galOrder2+2)/2;

    Ellipse ell_diff = *this;
    ell_diff.postShiftBy(-ell_meas->getCen()*sigma,
                         -ell_meas->getGamma(),
                         -ell_meas->getMu());
    ell_diff.removeRotation();

    DMatrix T(6,galSize2);
    calculateZTransform(ell_diff.getCen(),2,galOrder2,T);
    DMatrix S(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma(),galOrder2,S);
    DMatrix D(galSize2,b.size());
    calculateMuTransform(ell_diff.getMu(),galOrder2,galOrder,D);

    DMatrix M = T.rowRange(1,5) * S * D;
    xdbg<<"M = "<<M<<std::endl;
    DVector Mb = M * b.vec();
    xdbg<<"Mb = "<<Mb<<std::endl;
    xdbg<<"Mb/b(0) = "<<Mb/b(0)<<std::endl;

    const double delta = 1.e-4;
    typedef std::complex<double> CT;

    DMatrix T1(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(1,0),2,galOrder2,T1);
    DMatrix T2(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(-1,0),2,galOrder2,T2);
    DMatrix T3(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(0,1),2,galOrder2,T3);
    DMatrix T4(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(0,-1),2,galOrder2,T4);
    DMatrix T5(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(1,1),2,galOrder2,T5);
    DMatrix T6(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(1,-1),2,galOrder2,T6);
    DMatrix T7(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(-1,1),2,galOrder2,T7);
    DMatrix T8(6,galSize2);
    calculateZTransform(ell_diff.getCen()+delta*CT(-1,-1),2,galOrder2,T8);

    DMatrix S1(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(1,0),galOrder2,S1);
    DMatrix S2(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(-1,0),galOrder2,S2);
    DMatrix S3(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(0,1),galOrder2,S3);
    DMatrix S4(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(0,-1),galOrder2,S4);
    DMatrix S5(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(1,1),galOrder2,S5);
    DMatrix S6(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(1,-1),galOrder2,S6);
    DMatrix S7(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(-1,1),galOrder2,S7);
    DMatrix S8(galSize2,galSize2);
    calculateGTransform(ell_diff.getGamma()+delta*CT(-1,-1),galOrder2,S8);

#if 0
    DMatrix D1(galSize2,b.size());
    calculateMuTransform(ell_diff.getMu()+delta,galOrder2,galOrder,D1);
    DMatrix D2(galSize2,b.size());
    calculateMuTransform(ell_diff.getMu()-delta,galOrder2,galOrder,D2);
#endif

    DVector dMdxb = (T1-T2)*(S*(D*b.vec())) / (2.*delta);
    xdbg<<"dMdxb = "<<dMdxb<<std::endl;
    DVector dMdyb = (T3-T4)*(S*(D*b.vec())) / (2.*delta);
    xdbg<<"dMdyb = "<<dMdyb<<std::endl;
    DVector dMdg1b = T*((S1-S2)*(D*b.vec())) / (2.*delta);
    xdbg<<"dMdg1b = "<<dMdg1b<<std::endl;
    DVector dMdg2b = T*((S3-S4)*(D*b.vec())) / (2.*delta);
    xdbg<<"dMdg2b = "<<dMdg2b<<std::endl;
#if 0
    DVector dMdmub = T*(S*((D1-D2)*b.vec())) / (2.*delta);
    xdbg<<"dMdmub = "<<dMdmub<<std::endl;
#endif

    DMatrix J(4,4);
    J.col(0) = dMdxb.subVector(1,5);
    J.col(1) = dMdyb.subVector(1,5);
    J.col(2) = dMdg1b.subVector(1,5);
    J.col(3) = dMdg2b.subVector(1,5);
    //J.col(4) = dMdmub.subVector(1,6);
    xdbg<<"J = "<<J<<std::endl;
    xdbg<<"J.inv = "<<J.inverse()<<std::endl;

    DVector d2Mdx2b = (T1+T2-2.*T)*(S*(D*b.vec())) / (delta*delta);
    DVector d2Mdy2b = (T3+T4-2.*T)*(S*(D*b.vec())) / (delta*delta);
    DVector d2Mdg12b = T*((S1+S2-2.*S)*(D*b.vec())) / (delta*delta);
    DVector d2Mdg22b = T*((S3+S4-2.*S)*(D*b.vec())) / (delta*delta);
    //DVector d2Mdmu2b = T*(S*((D1+D2-2.*D)*b.vec())) / (delta*delta);

    DVector d2Mdxdyb = (T5+T8-T6-T7)*(S*(D*b.vec())) / (4.*delta*delta);
    DVector d2Mdg1dg2b = T*((S5+S8-S6-S7)*(D*b.vec())) / (4.*delta*delta);

    DVector d2Mdxdg1b = (T1-T2)*((S1-S2)*(D*b.vec())) / (4.*delta*delta);
    DVector d2Mdxdg2b = (T1-T2)*((S3-S4)*(D*b.vec())) / (4.*delta*delta);
    //DVector d2Mdxdmub = (T1-T2)*(S*((D1-D2)*b.vec())) / (4.*delta*delta);

    DVector d2Mdydg1b = (T3-T4)*((S1-S2)*(D*b.vec())) / (4.*delta*delta);
    DVector d2Mdydg2b = (T3-T4)*((S3-S4)*(D*b.vec())) / (4.*delta*delta);
    //DVector d2Mdydmub = (T3-T4)*(S*((D1-D2)*b.vec())) / (4.*delta*delta);

    //DVector d2Mdg1dmub = T*((S1-S2)*((D1-D2)*b.vec())) / (4.*delta*delta);
    //DVector d2Mdg2dmub = T*((S3-S4)*((D1-D2)*b.vec())) / (4.*delta*delta);

    DSymMatrix H0(4);
    DSymMatrix H1(4);
    DSymMatrix H2(4);
    DSymMatrix H3(4);
    //DSymMatrix H4(5);

    H0(0,0) = d2Mdx2b(1);
    H1(0,0) = d2Mdx2b(2);
    H2(0,0) = d2Mdx2b(3);
    H3(0,0) = d2Mdx2b(4);
    //H4(0,0) = d2Mdx2b(5);
 
    H0(0,1) = d2Mdxdyb(1);
    H1(0,1) = d2Mdxdyb(2);
    H2(0,1) = d2Mdxdyb(3);
    H3(0,1) = d2Mdxdyb(4);
    //H4(0,1) = d2Mdxdyb(5);
 
    H0(0,2) = d2Mdxdg1b(1);
    H1(0,2) = d2Mdxdg1b(2);
    H2(0,2) = d2Mdxdg1b(3);
    H3(0,2) = d2Mdxdg1b(4);
    //H4(0,2) = d2Mdxdg1b(5);
 
    H0(0,3) = d2Mdxdg2b(1);
    H1(0,3) = d2Mdxdg2b(2);
    H2(0,3) = d2Mdxdg2b(3);
    H3(0,3) = d2Mdxdg2b(4);
    //H4(0,3) = d2Mdxdg2b(5);
 
    //H0(0,4) = d2Mdxdmub(1);
    //H1(0,4) = d2Mdxdmub(2);
    //H2(0,4) = d2Mdxdmub(3);
    //H3(0,4) = d2Mdxdmub(4);
    //H4(0,4) = d2Mdxdmub(5);

    H0(1,1) = d2Mdy2b(1);
    H1(1,1) = d2Mdy2b(2);
    H2(1,1) = d2Mdy2b(3);
    H3(1,1) = d2Mdy2b(4);
    //H4(1,1) = d2Mdy2b(5);

    H0(1,2) = d2Mdydg1b(1);
    H1(1,2) = d2Mdydg1b(2);
    H2(1,2) = d2Mdydg1b(3);
    H3(1,2) = d2Mdydg1b(4);
    //H4(1,2) = d2Mdydg1b(5);

    H0(1,3) = d2Mdydg2b(1);
    H1(1,3) = d2Mdydg2b(2);
    H2(1,3) = d2Mdydg2b(3);
    H3(1,3) = d2Mdydg2b(4);
    //H4(1,3) = d2Mdydg2b(5);

    //H0(1,4) = d2Mdydmub(1);
    //H1(1,4) = d2Mdydmub(2);
    //H2(1,4) = d2Mdydmub(3);
    //H3(1,4) = d2Mdydmub(4);
    //H4(1,4) = d2Mdydmub(5);

    H0(2,2) = d2Mdg12b(1);
    H1(2,2) = d2Mdg12b(2);
    H2(2,2) = d2Mdg12b(3);
    H3(2,2) = d2Mdg12b(4);
    //H4(2,2) = d2Mdg12b(5);

    H0(2,3) = d2Mdg1dg2b(1);
    H1(2,3) = d2Mdg1dg2b(2);
    H2(2,3) = d2Mdg1dg2b(3);
    H3(2,3) = d2Mdg1dg2b(4);
    //H4(2,3) = d2Mdg1dg2b(5);

    //H0(2,4) = d2Mdg1dmub(1);
    //H1(2,4) = d2Mdg1dmub(2);
    //H2(2,4) = d2Mdg1dmub(3);
    //H3(2,4) = d2Mdg1dmub(4);
    //H4(2,4) = d2Mdg1dmub(5);

    H0(3,3) = d2Mdg22b(1);
    H1(3,3) = d2Mdg22b(2);
    H2(3,3) = d2Mdg22b(3);
    H3(3,3) = d2Mdg22b(4);
    //H4(3,3) = d2Mdg22b(5);

    //H0(3,4) = d2Mdg2dmub(1);
    //H1(3,4) = d2Mdg2dmub(2);
    //H2(3,4) = d2Mdg2dmub(3);
    //H3(3,4) = d2Mdg2dmub(4);
    //H4(3,4) = d2Mdg2dmub(5);

    //H0(4,4) = d2Mdmu2b(1);
    //H1(4,4) = d2Mdmu2b(2);
    //H2(4,4) = d2Mdmu2b(3);
    //H3(4,4) = d2Mdmu2b(4);
    //H4(4,4) = d2Mdmu2b(5);

    xdbg<<"H0 = "<<H0<<std::endl;
    xdbg<<"H1 = "<<H1<<std::endl;
    xdbg<<"H2 = "<<H2<<std::endl;
    xdbg<<"H3 = "<<H3<<std::endl;
    //dbg<<"H4 = "<<H4<<std::endl;

    DMatrix xCov = J.inverse() * M * bCov * M.transpose() * J.transpose().inverse();
    xdbg<<"bCov = "<<bCov<<std::endl;
    xdbg<<"ell = "<<*this<<std::endl;
    xdbg<<"M = "<<M<<std::endl;
    xdbg<<"fCov = "<<M*bCov*M.transpose()<<std::endl;
    xdbg<<"J = "<<J<<std::endl;
    xdbg<<"J.inv = "<<J.inverse()<<std::endl;
    xdbg<<"xCov = "<<xCov<<std::endl;

    DVector y(4);
    y(0) = Trace(H0 * xCov);
    y(1) = Trace(H1 * xCov);
    y(2) = Trace(H2 * xCov);
    y(3) = Trace(H3 * xCov);
    //y(4) = Trace(H4 * xCov);
    xdbg<<"y = "<<y<<std::endl;

    DVector bias = -0.5 * y / J;

    dbg<<"bias = "<<bias<<std::endl;

    std::complex<double> zbias(bias(0),bias(1));
    std::complex<double> gbias(bias(2),bias(3));
    double absg = std::abs(gbias);
    gbias *= tanh(absg)/absg;

    if (absg < 0.3)
        preShiftBy(-zbias*sigma,gbias,0.);
#endif
}
