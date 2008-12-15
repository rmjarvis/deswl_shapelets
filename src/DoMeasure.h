#ifndef DoMeasure_H
#define DoMeasure_H

#include "ConfigFile.h"
#include "Image.h"
#include "TimeVars.h"
#include "BVec.h"
#include "TMV_Small.h"
#include "Transformation.h"
#include "Log.h"
#include "FittedPSF.h"

// Returns how many successful measurements
int DoMeasurePSF_DES(ConfigFile& params, PSFLog& log);
int DoMeasurePSF(ConfigFile& params, PSFLog& log);
int DoMeasureShear(ConfigFile& params, ShearLog& log);
int DoMeasureShear_DES(ConfigFile& params, ShearLog& log);

void DoMeasurePSFPrint(
    std::ofstream& ostream,
    double x, double y, long flags, double nu, 
    int psforder, double sigma_p, const BVec& psf,
    const std::string& delim);
void DoMeasureShearPrint(
    std::ofstream& ostream,
    double x, double y, 
    long flags, 
    const std::complex<double>& shear, 
    const tmv::Matrix<double>& shearcov,
    const std::string& delim);

void ReadCatalog(ConfigFile& params, std::string incat,
    std::vector<Position>& all_pos, std::vector<double>& all_sky,
    std::vector<double>& all_noise, double& gain, Image<double>*& weight_im,
    std::vector<Position>& all_skypos);

void MeasureSingleShear(
    Position cen, const Image<double>& im, double sky, 
    const Transformation& trans,
    const std::vector<BVec>& psf,
    double noise, double gain, const Image<double>* weight_im,
    double gal_aperture, double max_aperture,
    int gal_order, int gal_order2,
    double f_psf, double min_gal_size,
    OverallFitTimes* times, ShearLog& log,
    std::complex<double>& shear, 
    tmv::Matrix<double>& varshear, BVec& shapelet,
    long& flags);

void MeasureSingleShear1(
    Position cen, const Image<double>& im, double sky,
    const Transformation& trans, const FittedPSF& fittedpsf,
    double noise, double gain, const Image<double>* weight_im, 
    double gal_aperture, double max_aperture,
    int gal_order, int gal_order2,
    double f_psf, double min_gal_size,
    OverallFitTimes* times, ShearLog& log,
    std::complex<double>& shear, 
    tmv::Matrix<double>& shearcov, BVec& shapelet,
    long& flags);

double SingleSigma(
    const Image<double>& im,
    const Position& pos,
    double sky,
    double noise,
    double gain,
    const Image<double>& weight_im, 
    const Transformation& trans, 
    double psfap,
    long& flag);

void MeasureSigmas(
    const Image<double>& im,
    const std::vector<Position>& all_pos,
    const std::vector<double>& all_sky,
    const std::vector<double>& all_noise,
    double gain,
    const Image<double>& weight_im, 
    const Transformation& trans, 
    double psfap,
    std::vector<double>& sigmas,
    std::vector<long>& flags);


void EstimateSigma(
    double& sigma_p,
    const Image<double>& im,
    const std::vector<Position>& all_pos, const std::vector<double>& all_sky,
    const std::vector<double>& all_noise, double gain,
    const Image<double>* weight_im, const Transformation& trans, double psfap);

void MeasureSinglePSF(
    Position cen, const Image<double>& im, double sky,
    const Transformation& trans,
    double noise, double gain, const Image<double>* weight_im,
    double sigma_p, double psf_aperture, int psf_order, PSFLog& log,
    BVec& psf, double& nu, long& flags);

#endif
