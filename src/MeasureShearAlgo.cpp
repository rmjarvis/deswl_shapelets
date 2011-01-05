
#include "dbg.h"
#include "Params.h"
#include "BVec.h"
#include "Ellipse.h"
#include "Pixel.h"
#include "Image.h"
#include "FittedPsf.h"
#include "Log.h"
#include "Params.h"
#include "MeasureShearAlgo.h"


static void ConvertShearFlagsToShapeFlags(long& flag) 
{
    if (flag & SHEAR_BAD_FLUX) { 
        xdbg<<"flag SHAPE_BAD_FLUX\n";
        flag |= SHAPE_BAD_FLUX;
        flag &= ~SHEAR_BAD_FLUX;
    }
    if (flag & SHEAR_POOR_FIT) { 
        xdbg<<"flag SHAPE_POOR_FIT\n";
        flag |= SHAPE_POOR_FIT;
        flag &= ~SHEAR_POOR_FIT;
    }
    if (flag & SHEAR_LOCAL_MIN) { 
        xdbg<<"flag SHAPE_LOCAL_MIN\n";
        flag |= SHAPE_LOCAL_MIN;
        flag &= ~SHEAR_LOCAL_MIN;
    }
}

void measureSingleShear1(
    Position& cen, const Image<double>& im, double sky,
    const Transformation& trans, const std::vector<BVec>& psf,
    double noise, double gain, const Image<double>* weightIm, 
    double galAperture, double maxAperture,
    int galOrder, int galOrder2,
    double minFPsf, double maxFPsf, double minGalSize, bool fixCen,
    double xOffset, double yOffset,
    bool fixSigma, double fixSigmaValue,
    ShearLog& log, std::complex<double>& shear, 
    DSmallMatrix22& shearcov, BVec& shapelet,
    double& nu, long& flag)
{
    // Find harmonic mean of psf sizes:
    // MJ: Is it better to use the harmonic mean of sigma or sigma^2?
    //     (Or something else entirely?)
    double sigmaP = 0.;
    const int nPsf = psf.size();
    Assert(nPsf > 0);
    for(int i=0;i<nPsf;++i) {
        sigmaP += 1./psf[i].getSigma();
        dbg<<"psf["<<i<<"] sigma = "<<psf[i].getSigma()<<std::endl;
    }
    sigmaP = double(nPsf) / sigmaP;
    dbg<<"Harmonic mean = "<<sigmaP<<std::endl;
    long flag1=0;

    //
    // Define initial sigma and aperture.
    // We start with a native fit using sigma = 2*sigmaP:
    // Or if we are fixed, then sigmaObs^2 = sigmaP^2 + sigma^2
    //
    double sigma = fixSigmaValue;
    double sigpsq = pow(sigmaP,2);
    double sigmaObs = 
        fixSigma ? 
        sqrt(sigpsq + sigma*sigma) : 
        2.*sigmaP;
    double galAp = galAperture * sigmaObs;
    dbg<<"galap = "<<galAperture<<" * "<<sigmaObs<<" = "<<galAp<<std::endl;
    if (maxAperture > 0. && galAp > maxAperture) {
        galAp = maxAperture;
        dbg<<"      => "<<galAp<<std::endl;
    }
    dbg<<"sigma_obs = "<<sigmaObs<<", sigma_p = "<<sigmaP<<std::endl;

    //
    // Load pixels from image
    //
    std::vector<PixelList> pix(1);
    getPixList(im,pix[0],cen,sky,noise,gain,weightIm,trans,
               galAp,xOffset,yOffset,flag);
    int npix = pix[0].size();
    dbg<<"npix = "<<npix<<std::endl;
    if (npix < 10) {
        dbg<<"Too few pixels to continue: "<<npix<<std::endl;
        xdbg<<"flag SHAPELET_NOT_DECONV\n";
        flag |= SHAPELET_NOT_DECONV;
        return;
    }

    //
    // Do a crude measurement based on simple pixel sums.
    //
    Ellipse ell_init;
    if (fixCen) ell_init.fixCen();
    if (fixSigma) ell_init.fixMu();
    ell_init.fixGam();
    if (!fixCen || !fixSigma) {
        ell_init.crudeMeasure(pix[0],sigmaObs);
        xdbg<<"Crude Measure: centroid = "<<ell_init.getCen()<<
            ", mu = "<<ell_init.getMu()<<std::endl;
        sigmaObs *= exp(ell_init.getMu());
        ell_init.setMu(0.);
    }

    //
    // Correct the centroid first.
    //
    Ellipse ell_native = ell_init;
    if (fixCen) ell_native.fixCen();
    ell_native.fixMu();
    ell_native.fixGam();
    if (!fixCen) {
        flag1 = 0;
        if (!ell_native.measure(pix,2,2,sigmaObs,flag1,0.1)) {
            ++log._nfCentroid;
            dbg<<"First centroid pass failed.\n";
            flag |= flag1;
            xdbg<<"flag CENTROID_FAILED\n";
            flag |= CENTROID_FAILED;
            xdbg<<"flag SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            return;
        }
        dbg<<"After first centroid pass: cen = "<<ell_native.getCen()<<std::endl;

        // redo the pixlist:
        pix[0].clear();
        cen += ell_native.getCen();
        ell_native.setCen(0.);
        dbg<<"New center = "<<cen<<std::endl;
        getPixList(im,pix[0],cen,sky,noise,gain,weightIm,trans,
                   galAp,xOffset,yOffset,flag);
        npix = pix[0].size();
        if (npix < 10) {
            dbg<<"Too few pixels to continue: "<<npix<<std::endl;
            xdbg<<"flag SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            return;
        }

        // The centroid doesn't necessarily get all the way to the perfect
        // centroid, so if the input guess is consistently biased in some 
        // direction (e.g. by a different convention for the (0,0) or (1,1) 
        // location), then this results in a bias in the shear values.  
        // To avoid that, we first get to what we think it the correct centroid.
        // Then we redo the aperture from this point.  Then we shift the 
        // centroid value by 0.1 pixel in a random direction, and resolve for
        // the centroid.  This is all pretty fast, so it's worth doing to
        // avoid the subtle bias that can otherwise result.
        std::complex<double> cen1 = ell_native.getCen();
        // get a offset of 0.1 pixels in a random direction.
        double x_offset, y_offset, rsq_offset;
        do {
            x_offset = double(rand())*2./double(RAND_MAX) - 1.;
            y_offset = double(rand())*2./double(RAND_MAX) - 1.;
            rsq_offset = x_offset*x_offset + y_offset*y_offset;
        } while (rsq_offset > 1. || rsq_offset == 0.);
        double r_offset = sqrt(rsq_offset);
        x_offset /= r_offset*10.;
        y_offset /= r_offset*10.;
        //std::cout<<"x_offset: "<<x_offset<<"   y_offset: "<<y_offset<<"\n";
        ell_native.setCen(std::complex<double>(x_offset,y_offset));
        dbg<<"After random offset "<<x_offset<<","<<y_offset<<
            ": cen = "<<cen1+ell_native.getCen()<<std::endl;

        // Now do it again.
        flag1 = 0;
        if (!ell_native.measure(pix,2,2,sigmaObs,flag1,0.1)) {
            ++log._nfCentroid;
            dbg<<"Second centroid pass failed.\n";
            flag |= flag1;
            xdbg<<"flag CENTROID_FAILED\n";
            flag |= CENTROID_FAILED;
            xdbg<<"flag SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            if (!fixCen) cen += ell_native.getCen();
            return;
        }
        dbg<<"After second centroid pass: cen = "<<cen1+ell_native.getCen()<<std::endl;
        dbg<<"Which is now considered "<<ell_native.getCen()<<" relative to the new "
            "center value of "<<cen<<std::endl;

        ++log._nsCentroid;
    }

    //
    // Now adjust the sigma value 
    //
    if (!fixSigma) {
        flag1 = 0;
        ell_native.unfixMu();
        if (ell_native.measure(pix,2,2,sigmaObs,flag1,0.1)) {
            // Don't ++nsNative yet.  Only success if also do round frame.
            dbg<<"Successful native fit:\n";
            dbg<<"Z = "<<ell_native.getCen()<<std::endl;
            dbg<<"Mu = "<<ell_native.getMu()<<std::endl;
        } else {
            ++log._nfNative;
            dbg<<"Native measurement failed\n";
            flag |= flag1;
            xdbg<<"flag NATIVE_FAILED\n";
            flag |= NATIVE_FAILED;
            xdbg<<"flag SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            if (!fixCen) cen += ell_native.getCen();
            return;
        }

        // Adjust the sigma value so we can reset mu = 0.
        sigmaObs *= exp(ell_native.getMu());
        if (sigmaObs < minGalSize*sigmaP) {
            dbg<<"skip: galaxy is too small -- "<<sigmaObs<<
                " psf size = "<<sigmaP<<std::endl;
            ++log._nfSmall;
            flag |= flag1;
            xdbg<<"flag TOO_SMALL\n";
            flag |= TOO_SMALL;
            xdbg<<"flag SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            if (!fixCen) cen += ell_native.getCen();
            return;
        }
        ell_native.setMu(0.);
    }

    //
    // Now find the frame in which the native observation is round.
    //
    Ellipse ell_round = ell_native;
    if (fixCen) ell_round.fixCen();
    if (fixSigma) ell_round.fixMu();
    flag1 = 0;
    if (ell_round.measure(pix,galOrder,galOrder2,sigmaObs,flag1,1.e-3)) {
        ++log._nsNative;
        dbg<<"Successful round fit:\n";
        dbg<<"Z = "<<ell_round.getCen()<<std::endl;
        dbg<<"Mu = "<<ell_round.getMu()<<std::endl;
        dbg<<"Gamma = "<<ell_round.getGamma()<<std::endl;
    } else {
        ++log._nfNative;
        dbg<<"Round measurement failed\n";
        flag |= flag1;
        xdbg<<"flag NATIVE_FAILED\n";
        flag |= NATIVE_FAILED;
        xdbg<<"flag SHAPELET_NOT_DECONV\n";
        flag |= SHAPELET_NOT_DECONV;
        if (!fixCen) cen += ell_round.getCen();
        shear = ell_round.getGamma();
        return;
    }

    // Adjust the sigma value so we can reset mu = 0.
    sigmaObs *= exp(ell_round.getMu());
    ell_round.setMu(0.);
    if (sigmaObs < minGalSize*sigmaP) {
        dbg<<"skip: galaxy is too small -- "<<sigmaObs<<
            " psf size = "<<sigmaP<<std::endl;
        ++log._nfSmall;
        xdbg<<"flag TOO_SMALL\n";
        flag |= TOO_SMALL;
        xdbg<<"flag SHAPELET_NOT_DECONV\n";
        flag |= SHAPELET_NOT_DECONV;
        if (!fixCen) cen += ell_round.getCen();
        shear = ell_round.getGamma();
        return;
    }
    std::complex<double> native_shear = ell_round.getGamma();
    if (!fixCen) cen += ell_round.getCen();
    ell_round.setCen(0.);
    galAp = sigmaObs * galAperture;
    if (maxAperture > 0. && galAp > maxAperture) galAp = maxAperture;
    pix[0].clear();
    getPixList(im,pix[0],cen,sky,noise,gain,weightIm,trans,
               galAp,native_shear,xOffset,yOffset,flag);
    npix = pix[0].size();
    dbg<<"npix = "<<npix<<std::endl;
    if (npix < 10) {
        dbg<<"Too few pixels to do shape measurement.\n";
        xdbg<<"flag SHAPELET_NOT_DECONV\n";
        flag |= SHAPELET_NOT_DECONV;
        shear = ell_round.getGamma();
        return;
    }

    long flag0 = flag;
    for(double fPsf=minFPsf; fPsf<=maxFPsf+1.e-3; fPsf+=0.5) {
        flag = flag0; // In case anything was set in a previous iteration.

        bool lastfpsf = (fPsf + 0.5 > maxFPsf+1.e-3);
        dbg<<"Start fPsf loop: fPsf = "<<fPsf<<std::endl;

        //
        // Set the sigma to use for the shapelet measurement:
        //
        // The sigma to use is 
        // sigma_s^2 = sigma_i^2 + fpsf sigma_p^2
        // And sigma_i^2 = sigma_o^2 - sigma_p^2
        // So sigma_s^2 = sigma_o^2 + (fpsf-1) sigma_p^2
        sigma = pow(sigmaObs,2) + (fPsf-1.) * sigpsq;
        xdbg<<"sigmaP^2 = "<<sigpsq<<std::endl;
        xdbg<<"sigmaObs^2 = "<<pow(sigmaObs,2)<<std::endl;
        xdbg<<"sigma^2 = sigmaObs^2 + (fPsf-1) * sigmap^2 = "<<sigma<<std::endl;
        if (sigma < 0.1*sigpsq) {
            xdbg<<"sigma too small, try larger fPsf\n";
            if (!lastfpsf) continue;
            xdbg<<"flag TOO_SMALL\n";
            flag |= TOO_SMALL;
            shear = ell_round.getGamma();
            return;
        }
        sigma = sqrt(sigma);
        dbg<<"sigma_s = "<<sigma<<std::endl;

        // 
        // Measure a deconvolving fit in the native frame.
        //
        Ellipse ell_deconv = ell_native;
        ell_deconv.fixGam();
        if (fixCen) ell_round.fixCen();
        if (fixSigma) ell_round.fixMu();
        flag1 = 0;
        if (ell_deconv.measure(
                pix,psf,galOrder,galOrder2,sigma,flag1,1.e-3,0,&shapelet)) {
            dbg<<"Successful deconvolving fit:\n";
            xdbg<<"Mu = "<<ell_deconv.getMu()<<std::endl;
            xdbg<<"Cen = "<<ell_deconv.getCen()<<std::endl;
            xdbg<<"flag1 = "<<flag1<<std::endl;
            if (flag1 & !lastfpsf) continue;
            ConvertShearFlagsToShapeFlags(flag1);
            xdbg<<"flag1 => "<<flag1<<std::endl;
            flag |= flag1;
            ++log._nsMu;
        } else {
            dbg<<"Deconvolving measurement failed\n";
            if (!lastfpsf) continue;
            ++log._nfMu;
            ConvertShearFlagsToShapeFlags(flag1);
            xdbg<<"flag1 => "<<flag1<<std::endl;
            flag |= flag1;
            xdbg<<"flag DECONV_FAILED\n";
            flag |= DECONV_FAILED;
        }
        dbg<<"sigma = "<<sigma<<std::endl;
        dbg<<"ell.cen = "<<ell_deconv.getCen()<<std::endl;
        dbg<<"ell.mu = "<<ell_deconv.getMu()<<std::endl;
        dbg<<"Measured deconvolved b_gal = "<<shapelet.vec()<<std::endl;

        //
        // Also measure the isotropic significance
        // TODO: Should this be in the reference frame where galaxy is round?
        //
        BVec flux(0,sigma);
        DMatrix fluxCov(1,1);
        int order0 = 0;
        if (!ell_deconv.measureShapelet(pix,psf,flux,order0,0,&fluxCov) ||
            !(flux(0) > 0) || !(fluxCov(0,0) > 0.) ||
            shapelet(0) >= flux(0)*3. || shapelet(0) <= flux(0)/3.) {
            // If the b00 value in the shapelet doesn't match the direct flux
            // measurement, set a flag.
            dbg<<"Bad flux value: \n";
            dbg<<"flux = "<<flux(0)<<std::endl;
            dbg<<"shapelet = "<<shapelet.vec()<<std::endl;
            if (!lastfpsf) {
                --log._nsMu;
                continue;
            }
            xdbg<<"flag SHAPE_BAD_FLUX\n";
            flag |= SHAPE_BAD_FLUX;
        }
        nu = flux(0) / sqrt(fluxCov(0,0));
        dbg<<"nu = "<<flux(0)<<" / sqrt("<<fluxCov(0,0)<<") = "<<nu<<std::endl;

        //
        // Next, we find the shear where the galaxy looks round.
        //
        flag1 = 0;
        Ellipse ell_shear = ell_round;
        if (fixCen) ell_shear.fixCen();
        if (fixSigma) ell_shear.fixMu();
        else ell_shear.setMu(ell_deconv.getMu());
        DMatrix cov5(5,5);
        if (ell_shear.measure(
                pix,psf,galOrder,galOrder2,sigma,flag1,1.e-3,&cov5)) {
            dbg<<"Successful Gamma fit\n";
            dbg<<"Measured gamma = "<<ell_shear.getGamma()<<std::endl;
            dbg<<"Mu  = "<<ell_shear.getMu()<<std::endl;
            dbg<<"Cen  = "<<ell_shear.getCen()<<std::endl;
            if (flag1) {
                if (!lastfpsf) {
                    dbg<<"However, flag = "<<flag1<<", so try larger fPsf\n";
                    --log._nsMu;
                    continue;
                } else {
                    flag |= flag1;
                }
            }
            ++log._nsGamma;
        } else {
            dbg<<"Measurement failed (2nd pass)\n";
            if (!lastfpsf) {
                --log._nsMu;
                continue;
            }
            ++log._nfGamma;
            flag |= flag1;
            xdbg<<"flag SHEAR_FAILED\n";
            flag |= SHEAR_FAILED;
            shear = ell_shear.getGamma();
            if (!fixCen) cen += ell_shear.getCen();
            return;
        }

        //
        // Copy the shear and covariance to the output variables
        //
        shear = ell_shear.getGamma();
        DSmallMatrix22 cov2 = cov5.TMV_subMatrix(2,4,2,4);
        if (!(cov2.TMV_det() > 0.)) {
            dbg<<"cov2 has bad determinant: "<<cov2.TMV_det()<<std::endl;
            dbg<<"cov2 = "<<cov2<<std::endl;
            dbg<<"Full cov = "<<cov5<<std::endl;
            if (!lastfpsf) {
                --log._nsMu;
                --log._nsGamma;
                continue;
            }
            xdbg<<"flag SHEAR_BAD_COVAR\n";
            flag |= SHEAR_BAD_COVAR;
        } else {
            shearcov = cov2;
        }
        if (!fixCen) cen += ell_shear.getCen();
        break;
    }
}

void measureSingleShear(
    Position& cen, const Image<double>& im, double sky,
    const Transformation& trans, const FittedPsf& fitpsf,
    double noise, double gain, const Image<double>* weightIm, 
    double galAperture, double maxAperture, 
    int galOrder, int galOrder2,
    double minFPsf, double maxFPsf, double minGalSize, bool fixCen,
    double xOffset, double yOffset,
    bool fixSigma, double fixSigmaValue,
    ShearLog& log, std::complex<double>& shear, 
    DSmallMatrix22& shearcov, BVec& shapelet,
    double& nu, long& flag)
{
    // Get coordinates of the galaxy, and convert to sky coordinates
    try {
        // We don't need to save skypos.  We just want to catch the range
        // error here, so we don't need to worry about it for dudx, etc.
        Position skyPos;
        trans.transform(cen,skyPos);
        dbg<<"skypos = "<<skyPos<<std::endl;
    } catch (RangeException& e) {
        dbg<<"distortion range error: \n";
        xdbg<<"p = "<<cen<<", b = "<<e.getBounds()<<std::endl;
        ++log._nfRange1;
        xdbg<<"flag TRANSFORM_EXCEPTION\n";
        flag |= TRANSFORM_EXCEPTION;
        return;
    }

    // Calculate the psf from the fitted-psf formula:
    std::vector<BVec> psf(1, BVec(fitpsf.getPsfOrder(), fitpsf.getSigma()));
    try {
        dbg<<"for fittedpsf cen = "<<cen<<std::endl;
        psf[0] = fitpsf(cen);
    } catch (RangeException& e) {
        dbg<<"fittedpsf range error: \n";
        xdbg<<"p = "<<cen<<", b = "<<e.getBounds()<<std::endl;
        ++log._nfRange2;
        xdbg<<"flag FITTEDPSF_EXCEPTION\n";
        flag |= FITTEDPSF_EXCEPTION;
        return;
    }

    // Do the real meat of the calculation:
    dbg<<"measure single shear cen = "<<cen<<std::endl;
    try {
        measureSingleShear1(
            // Input data:
            cen, im, sky, trans, psf,
            // Noise variables:
            noise, gain, weightIm, 
            // Parameters:
            galAperture, maxAperture, galOrder, galOrder2,
            minFPsf, maxFPsf, minGalSize, fixCen, xOffset, yOffset,
            fixSigma, fixSigmaValue,
            // Log information
            log,
            // Ouput values:
            shear, shearcov, shapelet, nu, flag);
#ifdef USE_TMV
    } catch (tmv::Error& e) {
        dbg<<"TMV Error thrown in MeasureSingleShear\n";
        dbg<<e<<std::endl;
        ++log._nfTmvError;
        xdbg<<"flag TMV_EXCEPTION\n";
        flag |= TMV_EXCEPTION;
#endif
    } catch (std::exception& e) {
        dbg<<"std::exception thrown in MeasureSingleShear\n";
        dbg<<e.what()<<std::endl;
        ++log._nfOtherError;
        xdbg<<"flag STD_EXCEPTION\n";
        flag |= STD_EXCEPTION;
    } catch (...) {
        dbg<<"unkown exception in MeasureSingleShear\n";
        ++log._nfOtherError;
        xdbg<<"flag UNKNOWN_EXCEPTION\n";
        flag |= UNKNOWN_EXCEPTION;
    } 

}

