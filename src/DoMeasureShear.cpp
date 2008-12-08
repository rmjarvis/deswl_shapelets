#include "types.h"
#include "Params.h"

#include "BVec.h"
#include "Ellipse.h"
#include "dbg.h"
#include "ConfigFile.h"
#include "Pixel.h"
#include "Image.h"
#include "FittedPSF.h"
#include "DoMeasure.h"
#include "TimeVars.h"
#include "PsiHelper.h"
#include "Name.h"
#include "Log.h"

#include <fstream>
#include <iostream>

#ifdef _OPENMP
//#include <omp.h>
#endif

//#define SINGLEGAL 8146
//#define STARTAT 8000
//#define ENDAT 200


int DoMeasureShear(ConfigFile& params, ShearLog& log) 
{
  // Load image:
  int image_hdu = 1;
  if (params.keyExists("image_hdu")) image_hdu = params["image_hdu"];
  Image<double> im(Name(params,"image",true),image_hdu);

  // Read catalog info
  // (Also calculates the noise or opens the noise image as appropriate)
  std::vector<Position> all_pos;
  std::vector<double> all_sky;
  std::vector<double> all_noise;
  double gain;
  Image<double>* weight_im = 0;
  ReadCatalog(params,"allcat",all_pos,all_sky,all_noise,gain,weight_im);
  dbg<<"Finished Read cat\n";

  // Fix sky if necessary
  if (all_sky.size() == 0) {
    double glob_sky = 0.;
    if (params.keyExists("sky")) glob_sky = params["sky"];
    else glob_sky = im.Median();
    dbg<<"Set global value of sky to "<<glob_sky<<std::endl;
    all_sky.resize(all_pos.size());
    fill(all_sky.begin(),all_sky.end(),glob_sky);
  }

  // Read distortion function
  Transformation trans(params);

  // Read the fitted psf file
  std::string psffile = Name(params,"fitpsf",false,true);
  xdbg<<"Read fitted psf file "<<psffile<<std::endl;
  std::ifstream psfin(psffile.c_str());
  Assert(psfin);
  FittedPSF fittedpsf(psfin);
  xdbg<<"Done reading fittedpsf\n";

  // Read some needed parameters
  int ngals = all_pos.size();
  dbg<<"ngals = "<<ngals<<std::endl;
  Assert(params.keyExists("gal_aperture"));
  double gal_aperture = params["gal_aperture"];
  double max_aperture = 0.;
  if (params.keyExists("max_aperture")) 
    max_aperture = params["max_aperture"];
  Assert(params.keyExists("gal_order"));
  int gal_order = params["gal_order"];
  int gal_order2 = gal_order;
  if (params.keyExists("gal_order2")) gal_order2 = params["gal_order2"];
  Assert(params.keyExists("f_psf"));
  double f_psf = params["f_psf"];
  Assert(params.keyExists("min_gal_size"));
  double min_gal_size = params["min_gal_size"];
  bool output_dots=false;
  if (params.keyExists("output_dots")) output_dots=true;
  bool timing=false;
  if (params.keyExists("timing")) timing=true;

  // Setup output vectors
  std::vector<Position> skypos(ngals);
  std::vector<std::complex<double> > shear(ngals,0.);
  std::vector<tmv::Matrix<double> > shearcov(ngals,tmv::Matrix<double>(2,2));
  std::vector<BVec> shapelet(ngals,BVec(gal_order,DEFVALPOS));
  OverallFitTimes alltimes;
  // Vector of flag values
  std::vector<int32> flagvec(ngals,0);

  // Default values when we have failure
  std::complex<double> shear_default = DEFVALNEG;

  Position skypos_default(DEFVALNEG,DEFVALNEG);
  tmv::Matrix<double> shearcov_default(2,2);
  shearcov_default(0,0) = DEFVALPOS; shearcov_default(0,1) = 0;
  shearcov_default(1,0) = 0;         shearcov_default(1,1) = DEFVALPOS;

  BVec shapelet_default(gal_order,DEFVALPOS);
  for (size_t i=0; i<shapelet_default.size(); i++) {
    shapelet_default[i] = DEFVALNEG;
  }

#ifdef ENDAT
  ngals = ENDAT;
#endif
  
  log.ngals = ngals;
#ifdef STARTAT
  log.ngals -= STARTAT;
#endif
#ifdef SINGLEGAL
  log.ngals = 1;
#endif

  // Main loop to measure shears
#ifdef _OPENMP
#pragma omp parallel 
  {
    try {
#endif
      OverallFitTimes times; // just for this thread
      ShearLog log1; // just for this thread
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
      for(int i=0;i<ngals;i++) {
#ifdef STARTAT
	if (i < STARTAT) continue;
#endif
#ifdef SINGLEGAL
	if (i < SINGLEGAL) continue;
	if (i > SINGLEGAL) break;
#endif
#ifdef _OPENMP
#pragma omp critical
#endif
	{
	  if (output_dots) { std::cerr<<"."; std::cerr.flush(); }
	  dbg<<"galaxy "<<i<<":\n";
	  dbg<<"all_pos[i] = "<<all_pos[i]<<std::endl;
	}

	// Inputs to shear measurement code
	Position skypos1 = skypos_default;
	std::complex<double> shear1 = shear_default;
	tmv::Matrix<double> shearcov1 = shearcov_default;
	BVec shapelet1 = shapelet_default;
	int32 flag1 = 0;

	MeasureSingleShear1(
	    // Input data:
	    all_pos[i], im, all_sky[i], trans, 
	    // Fitted PSF
	    fittedpsf,
	    // Noise variables:
	    all_noise[i], gain, weight_im, 
	    // Parameters:
	    gal_aperture, max_aperture, gal_order, gal_order2, 
	    f_psf, min_gal_size, 
	    // Time stats if desired:
	    timing ? &times : 0, 
	    // Log information
	    log1,
	    // Ouput values:
	    skypos1, shear1, shearcov1, shapelet1, flag1);

#ifdef _OPENMP
#pragma omp critical
#endif
	{
	  // We always write out the answer for each object
	  // These are the default values if a catastrophic error occurred.
	  // Otherwise they are the best estimate up to the point of 
	  // any error.
	  // If there is no error, these are the correct estimates.
	  skypos[i] = skypos1;
	  shear[i] = shear1;
	  shearcov[i] = shearcov1;
	  shapelet[i] = shapelet1;
	  flagvec[i] = flag1;
	  if (!flag1) {
	    dbg<<"Successful shear measurement: "<<shear1<<std::endl;
	  }
	  else {
	    dbg<<"Unsuccessful shear measurement\n"; 
	  }
	}

	if (timing) {
	  dbg<<"So far: ns = "<<times.ns_gamma<<",  nf = "<<times.nf_native;
	  dbg<<", "<<times.nf_mu<<", "<<times.nf_gamma<<std::endl;
	}

      }
#ifdef _OPENMP
#pragma omp critical
#endif
      { 
	if (timing) alltimes += times;
	log += log1;
      }
#ifdef _OPENMP
    } 
    catch (...)
    {
      std::cerr<<"Caught some kind of exception in the parallel region.\n";
    }
  }
  // End openmp parallel section.
#endif

  int nsuccess=0;
  if (timing) {
    dbg<<alltimes.ns_gamma<<" successful shear measurements, ";
    dbg<<alltimes.nf_native<<" + "<<alltimes.nf_mu;
    dbg<<" + "<<alltimes.nf_gamma<<" unsuccessful\n";
    nsuccess = alltimes.ns_gamma;
  } else {
    //for(int i=0;i<ngals;i++) if (flagvec[i] == 0) nsuccess++;
    nsuccess = log.ns_gamma;
    dbg<<nsuccess<<" successful shear measurements, ";
  }
  if (output_dots) { 
	  std::cerr
		  <<std::endl
		  <<"Success rate: "<<nsuccess<<"/"<<ngals
		  <<std::endl; 
  }

  // Output shear information:
  std::string shearfile = Name(params,"shear");
  std::string sheardelim = "  ";
  if (params.keyExists("shear_delim")) sheardelim = params["shear_delim"];
  std::ofstream catout(shearfile.c_str());
  Assert(catout);
  for(int i=0;i<ngals;i++) {
    DoMeasureShearPrint(
	catout,
	skypos[i].GetX(), skypos[i].GetY(),
	flagvec[i],
	shear[i], shearcov[i],
	sheardelim);
  }
  dbg<<"Done writing output shear catalog\n";
  // TODO: Also output shapelets...

  if (timing) std::cerr<<alltimes<<std::endl;
  dbg<<log<<std::endl;

  // Cleanup memory
  if (weight_im) delete weight_im;

  return nsuccess;
}

void DoMeasureShearPrint(
    std::ofstream& ostream,
    double x, double y, 
    int32 flags, 
    const std::complex<double>& shear, 
    const tmv::Matrix<double>& shearcov,
    const std::string& delim)
{
  ostream
    << x             << delim
    << y             << delim
    << flags         << delim
    << real(shear)   << delim
    << imag(shear)   << delim
    << shearcov(0,0) << delim
    << shearcov(0,1) << delim
    << shearcov(1,1)
    << std::endl;
}

