
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

#define MAX_ITER 4
#define MAX_DELTA_MU 0.2
#define MAX_DELTA_GAMMA1 0.1
#define MAX_DELTA_GAMMA2 1.e-3
#define FAIL_DELTA_GAMMA2 1.e-1

void measureSingleShear(
    const std::vector<PixelList>& allpix,
    const std::vector<BVec>& psf,
    int& galOrder, const ConfigFile& params,
    ShearLog& log, BVec& shapelet, 
    std::complex<double>& gamma, DSmallMatrix22& cov,
    double& nu, long& flag)
{
    double galAperture = params.read<double>("shear_aperture");
    double maxAperture = params.read("shear_max_aperture",0.);
    // Initial value, but also returned with actual final value.
    int galOrderInit = params.read<int>("shear_gal_order");
    galOrder = galOrderInit;
    int galOrder2 = params.read<int>("shear_gal_order2");
    int maxm = params.read("shear_maxm",galOrder);
    int minGalOrder = params.read("shear_min_gal_order",4);
    bool baseOrderOnNu = params.read("shear_base_order_on_nu",true);
    double minFPsf = params.read("shear_f_psf",1.);
    double maxFPsf = params.read("shear_max_f_psf",minFPsf);
    double minGalSize = params.read<double>("shear_min_gal_size");
    bool fixCen = params.read("shear_fix_centroid",false);
    bool fixSigma = params.keyExists("shear_force_sigma");
    double fixSigmaValue = params.read("shear_force_sigma",0.);
    bool nativeOnly = params.read("shear_native_only",false);

    try {
        dbg<<"Start MeasureSingleShear\n";
        dbg<<"allpix.size = "<<allpix.size()<<std::endl;
        const int nExp = allpix.size();
        for(int i=0;i<nExp;++i)
            dbg<<"allpix["<<i<<"].size = "<<allpix[i].size()<<std::endl;
        dbg<<"psf.size = "<<psf.size()<<std::endl;
        Assert(psf.size() == allpix.size());

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
        std::complex<double> cen_offset = 0.;

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
        // Load pixels from main PixelLists.
        //
        std::vector<PixelList> pix(nExp);
        int npix = 0;
        for(int i=0;i<nExp;++i) {
            getSubPixList(pix[i],allpix[i],cen_offset,galAp,flag1);
            npix += pix[i].size();
        }
        dbg<<"npix = "<<npix<<std::endl;
        if (npix < 10) {
            dbg<<"Too few pixels to continue: "<<npix<<std::endl;
            dbg<<"FLAG LT10PIX\n";
            flag |= LT10PIX;
            dbg<<"FLAG SHAPELET_NOT_DECONV\n";
            flag |= SHAPELET_NOT_DECONV;
            return;
        }

        //
        // Do a crude measurement based on simple pixel sums.
        // TODO: If we have single-epoch measurements, use those for the
        //       starting guess rather than crudeMeasure.
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
            if (!ell_native.measure(pix,2,2,2,sigmaObs,flag1,0.1)) {
                ++log._nfCentroid;
                dbg<<"First centroid pass failed.\n";
                flag |= flag1;
                dbg<<"FLAG CENTROID_FAILED\n";
                flag |= CENTROID_FAILED;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }
            dbg<<"After first centroid pass: cen = "<<ell_native.getCen()<<std::endl;

            // redo the pixlist:
            cen_offset += ell_native.getCen();
            ell_native.setCen(0.);
            dbg<<"New center = "<<cen_offset<<std::endl;
            npix = 0;
            for(int i=0;i<nExp;++i) {
                getSubPixList(pix[i],allpix[i],cen_offset,galAp,flag1);
                npix += pix[i].size();
            }
            if (npix < 10) {
                dbg<<"Too few pixels to continue: "<<npix<<std::endl;
                dbg<<"FLAG LT10PIX\n";
                flag |= LT10PIX;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }

            // The centroid doesn't necessarily get all the way to the perfect
            // centroid, so if the input guess is consistently biased in some 
            // direction (e.g. by a different convention for the (0,0) or (1,1) 
            // location), then this results in a bias in the shear values.  
            // To avoid that, we first get to what we think it the correct centroid.
            // Then we redo the aperture from this point.  Then we shift the 
            // centroid value by 0.1 arcsec in a random direction, and resolve for
            // the centroid.  This is all pretty fast, so it's worth doing to
            // avoid the subtle bias that can otherwise result.
            // get an offset of 0.1 arcsec in a random direction.
            double x_offset, y_offset, rsq_offset;
            do {
                x_offset = double(rand())*2./double(RAND_MAX) - 1.;
                y_offset = double(rand())*2./double(RAND_MAX) - 1.;
                rsq_offset = x_offset*x_offset + y_offset*y_offset;
            } while (rsq_offset > 1. || rsq_offset == 0.);
            double r_offset = sqrt(rsq_offset);
            x_offset /= r_offset*10.;
            y_offset /= r_offset*10.;
            ell_native.setCen(std::complex<double>(x_offset,y_offset));
            dbg<<"After random offset "<<x_offset<<","<<y_offset<<
                ": cen = "<<ell_native.getCen()<<std::endl;

            // Now do it again.
            flag1 = 0;
            if (!ell_native.measure(pix,2,2,2,sigmaObs,flag1,0.1)) {
                ++log._nfCentroid;
                dbg<<"Second centroid pass failed.\n";
                flag |= flag1;
                dbg<<"FLAG CENTROID_FAILED\n";
                flag |= CENTROID_FAILED;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }
            dbg<<"After second centroid pass: cen = "<<
                ell_native.getCen()<<" relative to the new center: "<<
                cen_offset<<std::endl;

            ++log._nsCentroid;
        }

        //
        // Next find a good sigma value 
        //
        if (!fixSigma) {
            for(int iter=1;iter<=MAX_ITER;++iter) {
                dbg<<"Mu iter = "<<iter<<std::endl;
                flag1 = 0;
                ell_native.unfixMu();
                if (ell_native.measure(pix,2,2,2,sigmaObs,flag1,0.1)) {
                    // Don't ++nsNative yet.  Only success if also do round frame.
                    dbg<<"Successful native fit:\n";
                    dbg<<"Z = "<<ell_native.getCen()<<std::endl;
                    dbg<<"Mu = "<<ell_native.getMu()<<std::endl;
                } else {
                    ++log._nfNative;
                    dbg<<"Native measurement failed\n";
                    flag |= flag1;
                    dbg<<"FLAG NATIVE_FAILED\n";
                    flag |= NATIVE_FAILED;
                    dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                    flag |= SHAPELET_NOT_DECONV;
                    return;
                }

                // Adjust the sigma value so we can reset mu = 0.
                double deltaMu = ell_native.getMu();
                cen_offset += ell_native.getCen();
                dbg<<"New center = "<<cen_offset<<std::endl;
                ell_native.setCen(0.);
                sigmaObs *= exp(ell_native.getMu());
                dbg<<"New sigmaObs = "<<sigmaObs<<std::endl;
                ell_native.setMu(0.);
                if (sigmaObs < minGalSize*sigmaP) {
                    dbg<<"skip: galaxy is too small -- "<<sigmaObs<<
                        " psf size = "<<sigmaP<<std::endl;
                    ++log._nfSmall;
                    flag |= flag1;
                    dbg<<"FLAG TOO_SMALL\n";
                    flag |= TOO_SMALL;
                    dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                    flag |= SHAPELET_NOT_DECONV;
                    return;
                }

                // redo the pixlist:
                galAp = sigmaObs * galAperture;
                if (maxAperture > 0. && galAp > maxAperture) 
                    galAp = maxAperture;
                npix = 0;
                for(int i=0;i<nExp;++i) {
                    getSubPixList(pix[i],allpix[i],cen_offset,galAp,flag1);
                    npix += pix[i].size();
                }
                if (npix < 10) {
                    dbg<<"Too few pixels to continue: "<<npix<<std::endl;
                    dbg<<"FLAG LT10PIX\n";
                    flag |= LT10PIX;
                    dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                    flag |= SHAPELET_NOT_DECONV;
                    return;
                }
                dbg<<"deltaMu = "<<deltaMu<<std::endl;
                if (std::abs(deltaMu) < MAX_DELTA_MU) {
                    dbg<<"deltaMu < "<<MAX_DELTA_MU<<std::endl;
                    break;
                } else if (iter < MAX_ITER) {
                    dbg<<"deltaMu >= "<<MAX_DELTA_MU<<std::endl;
                    continue;
                } else {
                    dbg<<"deltaMu >= "<<MAX_DELTA_MU<<std::endl;
                    dbg<<"But iter == "<<MAX_ITER<<", so stop.\n";
                }
            }
        }
        

        //
        // Measure the isotropic significance
        //
        BVec flux(0,sigma);
        DMatrix fluxCov(1,1);
        if (!ell_native.measureShapelet(pix,psf,flux,0,0,0,&fluxCov) ||
            !(flux(0) > 0) || !(fluxCov(0,0) > 0.) ) {
            dbg<<"Failed flux measurement of bad flux value: \n";
            dbg<<"flux = "<<flux(0)<<std::endl;
            dbg<<"fluxCov = "<<fluxCov(0,0)<<std::endl;

            dbg<<"FLAG SHAPE_BAD_FLUX\n";
            flag |= SHAPE_BAD_FLUX;
        }
        dbg<<"flux = "<<flux<<std::endl;
        dbg<<"fluxCov = "<<fluxCov<<std::endl;
        if (flux(0) > 0. && fluxCov(0,0) > 0.) {
            nu = flux(0) / sqrt(fluxCov(0,0));
            dbg<<"nu = "<<flux(0)<<" / sqrt("<<fluxCov(0,0)<<") = "<<
                nu<<std::endl;
        } else {
            nu = DEFVALNEG;
            dbg<<"nu set to error value = "<<nu<<std::endl;
        }


        // 
        // Reduce order if necessary so that
        // (order+1)*(order+2)/2 < nu
        //
        galOrder = galOrderInit;
        if (baseOrderOnNu) {
            int galSize;
            while (galOrder > minGalOrder) {
                if (maxm > galOrder) maxm = galOrder;
                galSize = (maxm+1)*(maxm+2)/2 + (2*maxm+1)*(galOrder-maxm)/2;
                if (galSize <= nu) break;
                else --galOrder;
            }
            if (galOrder < galOrderInit) {
                dbg<<"Reduced galOrder to "<<galOrder<<
                    " so that galSize = "<<galSize<<
                    " <= nu = "<<nu<<std::endl;
            }
        }


        //
        // Next find the frame in which the native observation is round.
        //
        Ellipse ell_round = ell_native;
        if (fixCen) ell_round.fixCen();
        if (fixSigma) ell_round.fixMu();
        for(int iter=1;iter<=MAX_ITER;++iter) {
            dbg<<"Round iter = "<<iter<<std::endl;
            flag1 = 0;
            std::complex<double> gamma_prev = ell_round.getGamma();
            if (ell_round.measure(
                    pix,galOrder,galOrder2,maxm,sigmaObs,flag1,1.e-2)) {
                ++log._nsNative;
                dbg<<"Successful round fit:\n";
                dbg<<"Z = "<<ell_round.getCen()<<std::endl;
                dbg<<"Mu = "<<ell_round.getMu()<<std::endl;
                dbg<<"Gamma = "<<ell_round.getGamma()<<std::endl;
            } else {
                ++log._nfNative;
                dbg<<"Round measurement failed\n";
                flag |= flag1;
                dbg<<"FLAG NATIVE_FAILED\n";
                flag |= NATIVE_FAILED;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }

            // Adjust the sigma value so we can reset mu = 0.
            if (!fixCen) cen_offset += ell_round.getCen();
            ell_round.setCen(0.);
            double deltaMu = ell_round.getMu();
            sigmaObs *= exp(ell_round.getMu());
            ell_round.setMu(0.);
            if (sigmaObs < minGalSize*sigmaP) {
                dbg<<"skip: galaxy is too small -- "<<sigmaObs<<
                    " psf size = "<<sigmaP<<std::endl;
                ++log._nfSmall;
                dbg<<"FLAG TOO_SMALL\n";
                flag |= TOO_SMALL;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }
            gamma = ell_round.getGamma();
            dbg<<"New center = "<<cen_offset<<std::endl;
            if (std::abs(cen_offset) > 1.) {
                dbg<<"FLAG CENTROID_SHIFT\n";
                flag |= CENTROID_SHIFT;
            }
            dbg<<"New sigmaObs = "<<sigmaObs<<std::endl;
            dbg<<"New gamma = "<<gamma<<std::endl;
            std::complex<double> deltaGamma = gamma - gamma_prev;
            dbg<<"ell_round = "<<ell_round<<std::endl;

            // Get the pixels in an elliptical aperture based on the
            // observed shape.
            galAp = sigmaObs * galAperture;
            if (maxAperture > 0. && galAp > maxAperture) 
                galAp = maxAperture;
            npix = 0;
            for(int i=0;i<nExp;++i) {
                getSubPixList(pix[i],allpix[i],cen_offset,gamma,galAp,flag1);
                npix += pix[i].size();
            }
            dbg<<"npix = "<<npix<<std::endl;
            if (npix < 10) {
                dbg<<"Too few pixels to continue: "<<npix<<std::endl;
                dbg<<"FLAG LT10PIX\n";
                flag |= LT10PIX;
                dbg<<"FLAG SHAPELET_NOT_DECONV\n";
                flag |= SHAPELET_NOT_DECONV;
                return;
            }
            dbg<<"deltaMu = "<<deltaMu<<std::endl;
            dbg<<"deltaGamma = "<<deltaGamma<<std::endl;
            if (std::abs(deltaMu) < MAX_DELTA_MU &&
                std::abs(deltaGamma) < MAX_DELTA_GAMMA1) {
                dbg<<"deltaMu < "<<MAX_DELTA_MU;
                dbg<<" and deltaGamma < "<<MAX_DELTA_GAMMA1<<std::endl;
                break;
            } else if (iter < MAX_ITER) {
                dbg<<"deltaMu >= "<<MAX_DELTA_MU;
                dbg<<" or deltaGamma >= "<<MAX_DELTA_GAMMA1<<std::endl;
                continue;
            } else {
                dbg<<"deltaMu >= "<<MAX_DELTA_MU;
                dbg<<" or deltaGamma >= "<<MAX_DELTA_GAMMA1<<std::endl;
                dbg<<"But iter == "<<MAX_ITER<<", so stop.\n";
            }
        }

        if (nativeOnly) return;

        // Start with the specified fPsf, but allow it to increase up to
        // maxFPsf if there are any problems.
        long flag0 = flag;
        dbg<<"minFPsf = "<<minFPsf<<std::endl;
        dbg<<"maxFPsf = "<<maxFPsf<<std::endl;
        Assert(minFPsf <= maxFPsf);
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
                dbg<<"FLAG TOO_SMALL\n";
                flag |= TOO_SMALL;
                return;
            }
            sigma = sqrt(sigma);
            dbg<<"sigma_s = "<<sigma<<std::endl;

            // 
            // Measure a deconvolving fit in the native frame.
            //
            shapelet.setSigma(sigma);
            DMatrix shapeCov(int(shapelet.size()),int(shapelet.size()));
            if (ell_native.measureShapelet(
                    pix,psf,shapelet,galOrder,galOrder2,galOrder,&shapeCov)) {
                dbg<<"Successful deconvolving fit:\n";
                ++log._nsMu;
            } else {
                dbg<<"Deconvolving measurement failed\n";
                if (!lastfpsf) continue;
                ++log._nfMu;
                dbg<<"FLAG DECONV_FAILED\n";
                flag |= DECONV_FAILED;
            }
            dbg<<"Measured deconvolved b_gal = "<<shapelet.vec()<<std::endl;
            xdbg<<"shapeCov = "<<shapeCov<<std::endl;
            if (shapelet(0) >= flux(0)*3. || shapelet(0) <= flux(0)/3.) {
                // If the b00 value in the shapelet doesn't match the direct flux
                // measurement, set a flag.
                dbg<<"Bad flux value: \n";
                dbg<<"flux = "<<flux(0)<<std::endl;
                dbg<<"shapelet = "<<shapelet.vec()<<std::endl;
                dbg<<"flux ratio = "<<flux(0)/shapelet(0)<<std::endl;
            }

            //
            // Next, we find the frame where the deconvolved shape of the
            // galaxy is round.
            //
            for(int tryOrder=galOrder; tryOrder>=minGalOrder; --tryOrder) {
                dbg<<"tryOrder = "<<tryOrder<<std::endl;
                if (tryOrder < galOrder) {
                    dbg<<"FLAG SHEAR_REDUCED_ORDER\n";
                    flag |= SHEAR_REDUCED_ORDER;
                }
                if (maxm > tryOrder) maxm = tryOrder;
                Ellipse ell_meas = ell_round;
                Ellipse ell_shear = ell_round;
                if (fixCen) ell_shear.fixCen();
                if (fixSigma) ell_shear.fixMu();
                //ell_shear.fixMu();
                double w = sqrt(sigma/sigmaP);
                bool success = false;
                cov.setZero();
                for(int iter=1;iter<=MAX_ITER;++iter) {
                    dbg<<"Shear iter = "<<iter<<std::endl;
                    flag1 = 0;
                    std::complex<double> gamma_prev = ell_shear.getGamma();
                    ell_meas.setGamma(
                        (w*ell_shear.getGamma() + ell_round.getGamma())/(w+1.));
                    if (ell_shear.measure(
                            pix,psf,tryOrder,galOrder2,maxm,sigma,flag1,
                            1.e-2,&cov,&ell_meas)) {
                        dbg<<"Successful shear fit:\n";
                        dbg<<"Z = "<<ell_shear.getCen()<<std::endl;
                        dbg<<"Mu = "<<ell_shear.getMu()<<std::endl;
                        dbg<<"Gamma = "<<ell_shear.getGamma()<<std::endl;
                    } else {
                        dbg<<"Shear measurement failed\n";
                        success = false;
                        break;
                    }

                    gamma = ell_shear.getGamma();
                    dbg<<"New gamma = "<<gamma<<std::endl;
                    std::complex<double> deltaGamma = gamma - gamma_prev;
                    dbg<<"ell_shear = "<<ell_shear<<std::endl;

                    dbg<<"deltaGamma = "<<deltaGamma<<std::endl;
                    if (std::abs(deltaGamma) < MAX_DELTA_GAMMA2) {
                        dbg<<"deltaGamma < "<<MAX_DELTA_GAMMA2<<std::endl;
                        success = true;
                        flag1 = 0;
                        break;
                    } else if (iter == MAX_ITER) {
                        dbg<<"deltaGamma >= "<<MAX_DELTA_GAMMA2<<std::endl;
                        dbg<<"But iter == "<<MAX_ITER<<", so stop.\n";
                        success = std::abs(deltaGamma) < FAIL_DELTA_GAMMA2;
                    } else {
                        dbg<<"deltaGamma >= "<<MAX_DELTA_GAMMA2<<std::endl;
                    }
                }
                if (success) {
                    ++log._nsGamma;
                    dbg<<"Successful shear measurement:\n";
                    dbg<<"shear = "<<gamma<<std::endl;
                    dbg<<"cov = "<<cov<<std::endl;
#if 0
                    ell_shear.correctForBias(
                        pix,psf,tryOrder,galOrder2,maxm,sigma,&ell_meas);
                    gamma = ell_shear.getGamma();
                    dbg<<"after correct bias: shear => "<<gamma<<std::endl;
#endif
                    return;
                } else {
                    dbg<<"Unsuccessful shear measurement\n"; 
                    dbg<<"shear = "<<gamma<<std::endl;
                    if (tryOrder == minGalOrder) {
                        flag |= flag1;
                        dbg<<"flag = "<<flag<<std::endl;
                    }
                }
            }
            if (!lastfpsf) continue;
            ++log._nfGamma;
            dbg<<"FLAG SHEAR_FAILED\n";
            flag |= SHEAR_FAILED;
        }
#ifdef USE_TMV
    } catch (tmv::Error& e) {
        dbg<<"TMV Error thrown in MeasureShear\n";
        dbg<<e<<std::endl;
        ++log._nfTmvError;
        dbg<<"FLAG TMV_EXCEPTION\n";
        flag |= TMV_EXCEPTION;
#endif
    } catch (std::exception& e) {
        dbg<<"std::exception thrown in MeasureShear\n";
        dbg<<e.what()<<std::endl;
        ++log._nfOtherError;
        dbg<<"FLAG STD_EXCEPTION\n";
        flag |= STD_EXCEPTION;
    } catch (...) {
        dbg<<"unkown exception in MeasureShear\n";
        ++log._nfOtherError;
        dbg<<"FLAG UNKNOWN_EXCEPTION\n";
        flag |= UNKNOWN_EXCEPTION;
    } 

}

