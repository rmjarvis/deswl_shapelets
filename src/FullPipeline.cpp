
#include "Image.h"
#include "Transformation.h"
#include "InputCatalog.h"
#include "StarCatalog.h"
#include "FittedPSF.h"
#include "PSFCatalog.h"
#include "StarFinder.h"
#include "ShearCatalog.h"
#include "TimeVars.h"
#include "Log.h"
#include "BasicSetup.h"

static void DoFullPipeline(ConfigFile& params,
    std::string logfile, std::auto_ptr<Log>& log)
{
  log.reset(new FindStarsLog(logfile,Name(params,"stars")));

  // Load image:
  std::auto_ptr<Image<double> > weight_im;
  Image<double> im(params,weight_im);

  // Read distortion function
  Transformation trans(params);

  // Read input catalog
  InputCatalog incat(params,&im);

  // Create StarCatalog from InputCatalog
  StarCatalog starcat(incat,params);

  // Update the sizes to more robust values
  starcat.CalcSizes(im,weight_im.get(),trans);

  try {
    starcat.FindStars(static_cast<FindStarsLog&>(*log));
  } catch(StarFinderException& e) {
    // Need to catch this here, so we can write the output file
    // with the sizes, even though we haven't figured out which 
    // objects are stars.
    dbg<<"Caught StarFinderException: "<<e.what()<<std::endl;  
    starcat.Write();
    throw;
  } catch (...) {
    dbg<<"Caught unknown exception\n";
    starcat.Write();
    throw;
  }
  dbg<<"After RunFindStars\n";

  // Write star catalog to file
  starcat.Write();

  xdbg<<"FindStarsLog: \n"<<*log<<std::endl;

  log.reset(new PSFLog(logfile,Name(params,"psf")));

  // Create PSFCatalog from StarCatalog
  PSFCatalog psfcat(starcat,params);

  // Estimate the scale size to use for shapelet decompositions
  double sigma_p = psfcat.EstimateSigma(im,weight_im.get(),trans);

  // Do the actual PSF measurements
  psfcat.MeasurePSF(im,weight_im.get(),trans,sigma_p,
      static_cast<PSFLog&>(*log));

  // Write PSF catalog to file
  psfcat.Write();

  // MJ -- Should we make a FittedPSFLog?  Are there parameters here that
  //       we want to ingest into a DB meta-table?
  //       If so, we would need to WritePSFCat after doing the
  //       FittedPSF calculation, since the fittedpsf file is not ingested.

  // Fit the PSF with a polynomial:
  FittedPSF fitpsf(psfcat,params);

  // Write fitted psf to file
  fitpsf.Write();

  xdbg<<"PSFLog: \n"<<*log<<std::endl;

  log.reset(new ShearLog(logfile,Name(params,"shear")));

  // Create shear catalog
  ShearCatalog shearcat(incat,trans,params);

  // Measure shears and shapelet vectors
  shearcat.MeasureShears(im,weight_im.get(),trans,fitpsf,
      static_cast<ShearLog&>(*log));

  // Write results to file
  shearcat.Write();

  xdbg<<"ShearLog: \n"<<*log<<std::endl;

}

int main(int argc, char **argv) try 
{
  ConfigFile params;
  if (BasicSetup(argc,argv,params,"fullpipe")) return EXIT_FAILURE;

  // Setup Log
  std::string logfile = ""; // Default is to stdout
  if (params.keyExists("log_file") || params.keyExists("log_ext")) 
    logfile = Name(params,"log");
  std::auto_ptr<Log> log;

  try {
    DoFullPipeline(params,logfile,log);
  }
#if 0
  // Change to 1 to let gdb see where the program bombed out.
  catch(int) {}
#else
  catch (file_not_found& e)
  {
    dbg<<"Caught \n"<<e.what()<<std::endl;
    std::cerr<<"Caught \n"<<e.what()<<std::endl;
    if (log.get()) {
      log->exitcode = FAILURE_FILE_NOT_FOUND;
      log->extraexitinfo = e.what();
    }
    return EXIT_FAILURE;
  }
  catch (ConfigFile::file_not_found& e)
  {
    dbg<<"Caught \n"<<e.what()<<std::endl;
    std::cerr<<"Caught \n"<<e.what()<<std::endl;
    if (log.get()) {
      log->exitcode = FAILURE_CONFIGFILE_ERROR;
      log->extraexitinfo = e.what();
    }
    return EXIT_FAILURE;
  }
  catch (ConfigFile::key_not_found& e)
  {
    dbg<<"Caught \n"<<e.what()<<std::endl;
    std::cerr<<"Caught \n"<<e.what()<<std::endl;
    if (log.get()) {
      log->exitcode = FAILURE_CONFIGFILE_ERROR;
      log->extraexitinfo = e.what();
    }
    return EXIT_FAILURE;
  }
  catch (tmv::Error& e)
  {
    dbg<<"Caught \n"<<e<<std::endl;
    std::cerr<<"Caught \n"<<e<<std::endl;
    if (log.get()) {
      log->exitcode = FAILURE_TMV_ERROR;
      log->extraexitinfo = e.what();
    }
    return EXIT_FAILURE;
  }
  catch (std::exception& e)
  {
    dbg<<"Caught \n"<<e.what()<<std::endl;
    std::cerr<<"Caught \n"<<e.what()<<std::endl;
    if (log.get()) {
      log->exitcode = FAILURE_STD_EXCEPTION;
      log->extraexitinfo = e.what();
    }
    return EXIT_FAILURE;
  }
  catch (ExitCode e)
  { 
    dbg<<"Caught ExitCode "<<e<<std::endl;
    std::cerr<<"Caught ExitCode "<<e<<std::endl;
    if (log.get()) log->exitcode = e;
    return EXIT_FAILURE;
  }
  catch (...)
  {
    dbg<<"Caught Unknown error\n";
    std::cerr<<"Caught Unknown error\n";
    if (log.get()) {
      log->exitcode = FAILURE;
      log->extraexitinfo = "Caught unknown exception";
    }
    return EXIT_FAILURE;
  }
#endif

  if (dbgout && dbgout != &std::cout) {delete dbgout; dbgout=0;}
  return EXIT_SUCCESS; // = 0 typically.  Defined in <cstdlib>
}
catch (std::exception& e)
{
  std::cerr<<"Fatal error: Caught \n"<<e.what()<<std::endl;
  if (des_qa) 
    std::cout<<"STATUS5BEG Fatal error: "<<e.what()<<" STATUS5END\n";
  return EXIT_FAILURE;
}
catch (...)
{
  std::cerr<<"Fatal error: Cought an exception.\n";
  if (des_qa)
    std::cout<<"STATUS5BEG Fatal error: unknown exception STATUS5END\n";
  return EXIT_FAILURE;
}
