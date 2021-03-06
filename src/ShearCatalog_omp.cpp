
#include <iostream>

#include "ShearCatalog.h"
#include "ConfigFile.h"
#include "Params.h"
#include "Image.h"
#include "FittedPsf.h"
#include "Log.h"
#include "MeasureShearAlgo.h"
#include "Ellipse.h"

//#define SINGLEGAL 13
//#define STARTAT 8000
//#define ENDAT 14

#ifdef SINGLEGAL
#undef _OPENMP
#endif

static bool GetPixPsf(
    const Image<double>& im, PixelList& pix,
    const Position pos, double sky, double noise,
    const Image<double>* weight_image, const Transformation& trans,
    const ConfigFile& params, long& flags,
    const FittedPsf& fitpsf, BVec& psf, ShearLog& log)
{
    double max_aperture = params.read<double>("shear_max_aperture");
    // Get the main PixelList for this galaxy:
    try {
        GetPixList(
            im,pix,pos,sky,noise,weight_image,
            trans,max_aperture,params,flags);
        int npix = pix.size();
        dbg<<"npix = "<<npix<<std::endl;
        if (npix < 10) {
            dbg<<"Too few pixels to continue: "<<npix<<std::endl;
            dbg<<"FLAG SHAPELET_NOT_DECONV\n";
            flags |= SHAPELET_NOT_DECONV;
            return false;
        }
    } catch (RangeException& e) {
        dbg<<"distortion range error: \n";
        xdbg<<"p = "<<pos<<", b = "<<e.getBounds()<<std::endl;
        ++log._nf_range1;
        dbg<<"FLAG TRANSFORM_EXCEPTION\n";
        flags |= TRANSFORM_EXCEPTION;
        return false;
    }

    // Get the interpolated psf for this galaxy:
    try {
        psf = fitpsf(pos);
    } catch (RangeException& e) {
        dbg<<"fittedpsf range error: \n";
        xdbg<<"p = "<<pos<<", b = "<<e.getBounds()<<std::endl;
        ++log._nf_range2;
        dbg<<"FLAG FITTEDPSF_EXCEPTION\n";
        flags |= FITTEDPSF_EXCEPTION;
        return false;
    }
    return true;
}
 
int ShearCatalog::measureShears(
    const Image<double>& im,
    const Image<double>* weight_image, ShearLog& log)
{
    static bool first = true;
#ifdef _OPENMP
#ifndef __INTEL_COMPILER
    // This should technically be guarded by a critical block, since 
    // it involves writing to global variables.  However, icpc seg faults
    // on the srand call if it is in a critical block.
    // I have no idea why, but it is clearly a bug in icpc.  v10.1 at least.
    // Anyway, it shouldn't be a problem, since this block should not 
    // be in a parallel region anyway, so there shouldn't be any problem
    // with multi-threading here anyway.
#pragma omp critical (srand)
#endif
#endif
    {
        if (first) {
            // initialize random seed:
            // NB: This will only stay deterministic if not using openmp.
            unsigned int seed=0;
            for (int i=0;i<this->size(); i++) {
                seed += i*_flags[i];
            }
            dbg<<"using seed: "<<seed<<"\n";
            srand (seed);
            first = false;
        }
    }

    int ngals = size();
    dbg<<"ngals = "<<ngals<<std::endl;

    // Read some needed parameters
    bool output_dots = _params.read("output_dots",false);
    bool des_qa = _params.read("des_qa",false); 

    // This need to have been set.
    Assert(_trans);
    Assert(_fitpsf);

#ifdef ENDAT
    for(int i=ENDAT; i<ngals; ++i) _flags[i] |= INPUT_FLAG;
    ngals = ENDAT;
#endif

    log._ngals = ngals;
#ifdef STARTAT
    for(int i=0; i<STARTAT; ++i) _flags[i] |= INPUT_FLAG;
    log._ngals -= STARTAT;
#endif
#ifdef SINGLEGAL
    for(int i=0; i<ngals; ++i) if (i != SINGLEGAL)  _flags[i] |= INPUT_FLAG;
    log._ngals = 1;
#endif
    log._ngoodin = std::count(_flags.begin(),_flags.end(),0);
    dbg<<log._ngoodin<<"/"<<log._ngals<<" galaxies with no input flags\n";
    std::vector<Position> initPos = _pos;

    // Main loop to measure shapes
#ifdef _OPENMP
#pragma omp parallel 
    {
        try {
#endif
            // Some variables to use just for this thread
            ShearLog log1(_params); 
            log1.noWriteLog();
            std::vector<PixelList> pix(1);
            std::vector<BVec> psf(
                1, BVec(_fitpsf->getPsfOrder(), _fitpsf->getSigma()));
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
            for(int i=0;i<ngals;++i) {
                if (_flags[i]) {
                    xdbg<<i<<" skipped because has flag "<<_flags[i]<<std::endl;
                    continue;
                }
#ifdef STARTAT
                if (i < STARTAT) continue;
#endif
#ifdef SINGLEGAL
                if (i < SINGLEGAL) continue;
                if (i > SINGLEGAL) break;
#endif
                if (output_dots) {
#ifdef _OPENMP
#pragma omp critical (output)
#endif
                    {
                        std::cerr<<"."; std::cerr.flush(); 
                    }
                }
                dbg<<"galaxy "<<i<<":\n";
                dbg<<"pos = "<<_pos[i]<<std::endl;

                if (!GetPixPsf(
                        im,pix[0],_pos[i],_sky[i],_noise[i],weight_image,
                        *_trans,_params,_flags[i],
                        *_fitpsf,psf[0],log1)) {
                    continue;
                }

                // Now measure the shape and shear:
                MeasureSingleShear(
                    // Input data:
                    pix, psf,
                    // Parameters:
                    _meas_galorder[i], _params,
                    // Log information
                    log1,
                    // Ouput values:
                    _shape[i], _shear[i], _cov[i], _nu[i], _flags[i]);


                if (!_flags[i]) {
                    dbg<<"Successful shear measurements: \n";
                    dbg<<"shape = "<<_shape[i]<<std::endl;
                    dbg<<"shear = "<<_shear[i]<<std::endl;
                    dbg<<"cov = "<<_cov[i]<<std::endl;
                    dbg<<"nu = "<<_nu[i]<<std::endl;
                } else {
                    dbg<<"Unsuccessful shear measurement\n"; 
                    dbg<<"flag = "<<_flags[i]<<std::endl;
                }
            }
            dbg<<"After loop"<<std::endl;
#ifdef _OPENMP
#pragma omp critical (add_log)
#endif
            {
                log += log1;
            }
#ifdef _OPENMP
        } catch (std::exception& e) {
            // This isn't supposed to happen.
            if (des_qa) {
                std::cerr<<"STATUS5BEG Caught error in parallel region STATUS5END\n";
            } 
            std::cerr<<"Caught "<<e.what()<<std::endl;
            std::cerr<<"Caught error in parallel region.  Aborting.\n";
            exit(1);
        } catch (...) {
            if (des_qa) {
                std::cerr<<"STATUS5BEG Caught error in parallel region STATUS5END\n";
            }
            std::cerr<<"Caught error in parallel region.  Aborting.\n";
            exit(1);
        }
    }
#endif
    dbg<<log._ns_gamma<<" successful shape measurements, ";
    dbg<<ngals-log._ns_gamma<<" unsuccessful\n";
    log._ngood = std::count(_flags.begin(),_flags.end(),0);
    dbg<<log._ngood<<" with no flags\n";
    dbg<<"Breakdown of flags:\n";
    if (dbgout) PrintFlags(_flags,*dbgout);

    if (output_dots) {
        std::cerr
            <<std::endl
            <<"Success rate: "<<log._ns_gamma<<"/"<<log._ngoodin
            <<"  # with no flags: "<<log._ngood
            <<std::endl;
        std::cerr<<"Breakdown of flags:\n";
        PrintFlags(_flags,std::cerr);
    }


    static const long ok_flags = (
        EDGE |
        SHEAR_REDUCED_ORDER |
        SHAPE_REDUCED_ORDER |
        SHAPE_POOR_FIT |
        SHAPE_LOCAL_MIN |
        SHAPE_BAD_FLUX );

    if (output_dots && !des_qa) {
        std::complex<double> mean_shear = 0.;
        int ngood_shear = 0;
        for(int i=0;i<ngals;++i) if (!(_flags[i] & ~ok_flags)) {
            mean_shear += _shear[i];
            ++ngood_shear;
        }

        mean_shear /= ngood_shear;
        dbg<<"ngood_shear = "<<ngood_shear<<"   mean_shear = "<<mean_shear<<std::endl;
        std::cerr
            <<"ngood_shear = "<<ngood_shear
            <<"   mean_shear = "<<mean_shear<<std::endl;
    }

    // Check if the input positions are biased with respect to the 
    // actual values.  If they are, then this can cause a bias in the
    // shears, so it's worth fixing.
    std::complex<double> mean_offset(0);
    int noffset=0;
    for(int i=0;i<ngals;++i) if (!_flags[i]) {
        mean_offset += _pos[i] - initPos[i];
        ++noffset;
    }
    mean_offset /= noffset;
    if (std::abs(mean_offset) > 0.5) {
        if (des_qa) {
            std::cerr<<"STATUS3BEG Warning: A bias in the input positions found: "
                <<mean_offset<<".\nThis may bias the shear, so you should "
                <<"change cat_x_offset, cat_y_offset.  STATUS3END\n";
        }
    }
    dbg<<"Found mean offset of "<<mean_offset<<" using "<<noffset<<
        " galaxies with no error codes.\n";

    xdbg<<log<<std::endl;

    return log._ns_gamma;
}
