
#include "Image.h"
#include "FittedPSF.h"
#include "Transformation.h"
#include "Log.h"
#include "PSFCatalog.h"
#include "BVec.h"
#include "BasicSetup.h"
#include <sys/time.h>

static void DoMeasurePSF(ConfigFile& params, PSFLog& log) 
{
  bool timing = params.read("timing",false);
  timeval tp;
  double t1=0.,t2=0.;

  if (timing) {
    gettimeofday(&tp,0);
    t1 = tp.tv_sec + tp.tv_usec/1.e6;
  }

  // Load image:
  std::auto_ptr<Image<double> > weight_im;
  Image<double> im(params,weight_im);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Open imgae = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Read distortion function
  Transformation trans(params);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Read Transformation = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Read star catalog info
  StarCatalog starcat(params);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Read StarCatalog = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Create PSFCatalog from StarCatalog
  PSFCatalog psfcat(starcat,params);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Create PSFCatalog = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Estimate the scale size to use for shapelet decompositions
  double sigma_p = psfcat.EstimateSigma(im,weight_im.get(),trans);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Estimate Sigma = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Do the actual PSF measurements
  int npsf = psfcat.MeasurePSF(im,weight_im.get(),trans,sigma_p,log);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Measure PSF = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Write PSF catalog to file
  psfcat.Write();

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Write PSFCatalog = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  if (npsf == 0) throw ProcessingError("No successful PSF measurements");

  // MJ -- Should we make a FittedPSFLog?  Are there parameters here that
  //       we want to ingest into a DB meta-table?
  //       If so, we would need to WritePSFCat after doing the
  //       FittedPSF calculation, since the fittedpsf file is not ingested.

  // Fit the PSF with a polynomial:
  FittedPSF fitpsf(psfcat,params);

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Fit PSF = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  // Write fitted psf to file
  fitpsf.Write();

  if (timing) {
    gettimeofday(&tp,0);
    t2 = tp.tv_sec + tp.tv_usec/1.e6;
    std::cout<<"Time: Write FittedPSF = "<<t2-t1<<std::endl;
    t1 = t2;
  }

  xdbg<<"Log: \n"<<log<<std::endl;
}

int main(int argc, char **argv) try 
{
  ConfigFile params;
  if (BasicSetup(argc,argv,params,"measurepsf")) return EXIT_FAILURE;

  // Setup Log
  std::string logfile = ""; // Default is to stdout
  if (params.keyExists("log_file") || params.keyExists("log_ext"))
    logfile = Name(params,"log");
  std::string psf_file=Name(params,"psf");
  std::auto_ptr<PSFLog> log (
      new PSFLog(params,logfile,psf_file)); 

  try {
    DoMeasurePSF(params,*log);
  }
#if 0
  // Change to 1 to let gdb see where the program bombed out.
  catch(int) {}
#else
  CATCHALL
#endif

  if (dbgout && dbgout != &std::cout) {delete dbgout; dbgout=0;}
  return EXIT_SUCCESS;
}
catch (std::exception& e)
{
  std::cerr<<"Fatal error: Caught \n"<<e.what()<<std::endl;
  std::cout<<"STATUS5BEG Fatal error: "<<e.what()<<" STATUS5END\n";
  return EXIT_FAILURE;
}
catch (...)
{
  std::cerr<<"Fatal error: Cought an exception.\n";
  std::cout<<"STATUS5BEG Fatal error: unknown exception STATUS5END\n";
  return EXIT_FAILURE;
}
