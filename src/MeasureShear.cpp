
//#include <valarray>
#include <sys/time.h>
#include "Image.h"
#include "InputCatalog.h"
#include "FittedPsf.h"
//#include "TimeVars.h"
#include "ShearCatalog.h"
#include "Log.h"
#ifdef NEW
#include "Scripts.h"
#endif
#include "BasicSetup.h"

#ifndef NEW
static void doMeasureShear(ConfigFile& params, ShearLog& log) 
{
    bool isTiming = params.read("timing",false);
    timeval tp;
    double t1=0.,t2=0.;

    if (isTiming) {
        gettimeofday(&tp,0);
        t1 = tp.tv_sec + tp.tv_usec/1.e6;
    }

    // Load image:
    std::auto_ptr<Image<double> > weightIm;
    Image<double> im(params,weightIm);

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Open imgae = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Read distortion function
    Transformation trans(params);

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Read Transformation = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Read input catalog
    InputCatalog inCat(params,&im);
    inCat.read();

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Read InputCatalog = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Read star catalog info
    StarCatalog starCat(params);
    starCat.read();

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Read StarCatalog = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Read the fitted psf file
    FittedPsf fitPsf(params);
    fitPsf.read();

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Read FittedPSF = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Create shear catalog
    ShearCatalog shearCat(inCat,trans,fitPsf,params);

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Create ShearCatalog = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Flag known stars as too small to bother trying to measure the shear.
    shearCat.flagStars(starCat);

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Flag stars = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Measure shears and shapelet vectors
    int nShear = shearCat.measureShears(im,weightIm.get(),log);

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Measure Shears = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    // Write results to file
    shearCat.write();

    if (isTiming) {
        gettimeofday(&tp,0);
        t2 = tp.tv_sec + tp.tv_usec/1.e6;
        std::cout<<"Time: Write ShearCatalog = "<<t2-t1<<std::endl;
        t1 = t2;
    }

    if (nShear == 0) {
        throw ProcessingException(
            "No successful shear measurements");
    }

    xdbg<<"Log: \n"<<log<<std::endl;
}
#endif

int main(int argc, char **argv) try 
{
    ConfigFile params;
    if (basicSetup(argc,argv,params,"measureshear")) return EXIT_FAILURE;

    // Setup Log
    std::string logFile = ""; // Default is to stdout
    if (params.keyExists("log_file") || params.keyExists("log_ext")) 
        logFile = makeName(params,"log",false,false);
    std::string shearFile = makeName(params,"shear",false,false);
    std::auto_ptr<ShearLog> log (
        new ShearLog(params,logFile,shearFile)); 

    try {
#ifdef NEW
        bool isTiming = params.read("timing",false);
        timeval tp;
        double t1=0.,t2=0.;

        if (isTiming) {
            gettimeofday(&tp,0);
            t1 = tp.tv_sec + tp.tv_usec/1.e6;
        }

        // Load image:
        std::auto_ptr<Image<double> > weightIm;
        Image<double> im(params,weightIm);

        if (isTiming) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Open imgae = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Read distortion function
        Transformation trans(params);

        if (isTiming) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read Transformation = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Read input catalog
        InputCatalog inCat(params,&im);
        inCat.read();

        if (isTiming) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read InputCatalog = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Read star catalog info
        StarCatalog starCat(params);
        starCat.read();

        if (isTiming) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read StarCatalog = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Read the fitted psf file
        FittedPsf fitPsf(params);
        fitPsf.read();

        if (isTiming) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read FittedPSF = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        std::auto_ptr<ShearCatalog> shearCat;
        doMeasureShear(
            params,*log,im,weightIm.get(),trans,inCat,starCat,fitPsf,
            shearCat);
#else
        doMeasureShear(params,*log);
#endif
    }
#if 0
    // Change to 1 to let gdb see where the program bombed out.
    catch(int) {}
#else
    CATCHALL;
#endif

    if (dbgout && dbgout != &std::cout) {delete dbgout; dbgout=0;}
    return EXIT_SUCCESS; // = 0 typically.  Defined in <cstdlib>
} catch (std::exception& e) {
    std::cerr<<"Fatal error: Caught \n"<<e.what()<<std::endl;
    std::cout<<"STATUS5BEG Fatal error: "<<e.what()<<" STATUS5END\n";
    return EXIT_FAILURE;
} catch (...) {
    std::cerr<<"Fatal error: Cought an exception.\n";
    std::cout<<"STATUS5BEG Fatal error: unknown exception STATUS5END\n";
    return EXIT_FAILURE;
}
