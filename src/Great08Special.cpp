
#include <sys/time.h>
#include "Image.h"
#include "InputCatalog.h"
#include "FittedPsf.h"
#include "ShearCatalog.h"
#include "Log.h"
#include "Scripts.h"
#include "BasicSetup.h"

int main(int argc, char **argv) try 
{
    const double PI = 3.141592653589793;

    ConfigFile params;
    if (BasicSetup(argc,argv,params,"g08special")) return EXIT_FAILURE;

    // Setup Log
    std::string log_file = ""; // Default is to stdout
    if (params.keyExists("log_file") || params.keyExists("log_ext")) 
        log_file = MakeName(params,"log",false,false);
    std::string shear_file = MakeName(params,"shear",false,false);
    std::auto_ptr<ShearLog> log(new ShearLog(params,log_file,shear_file)); 

    try {
        bool timing = params.read("timing",false);
        timeval tp;
        double t1=0.,t2=0.;

        if (timing) {
            gettimeofday(&tp,0);
            t1 = tp.tv_sec + tp.tv_usec/1.e6;
        }

        // Load image:
        std::auto_ptr<Image<double> > weight_image;
        Image<double> im(params,weight_image);

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

        // Read input catalog
        InputCatalog incat(params,&im);
        incat.read();

        if (timing) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read InputCatalog = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        bool nostars = params.read("cat_no_stars",false);
        if (!nostars) {
            // Read star catalog info
            StarCatalog starcat(params);
            starcat.read();

            if (timing) {
                gettimeofday(&tp,0);
                t2 = tp.tv_sec + tp.tv_usec/1.e6;
                std::cout<<"Time: Read StarCatalog = "<<t2-t1<<std::endl;
                t1 = t2;
            }

            // Flag known stars as too small to bother trying to measure 
            // the shear.
            incat.flagStars(starcat);

            if (timing) {
                gettimeofday(&tp,0);
                t2 = tp.tv_sec + tp.tv_usec/1.e6;
                std::cout<<"Time: Flag stars = "<<t2-t1<<std::endl;
                t1 = t2;
            }
        }

        // Read the fitted psf file
        FittedPsf fitpsf(params);
        fitpsf.read();

        if (timing) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Read FittedPSF = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Create shear catalog
        params["shear_native_only"] = true;
        ShearCatalog shearcat(incat,trans,fitpsf,params);

        if (timing) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Create ShearCatalog = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        // Measure shears and shapelet vectors
        int nShear = shearcat.measureShears(im,weight_image.get(),*log);
        dbg<<"nShear = "<<nShear<<std::endl;

        int ngal = shearcat.size();
        double maxg = 0.;
        std::vector<double> absgamma;
        absgamma.reserve(ngal);
        for(int i=0; i<ngal; ++i) if (shearcat.getFlags(i) == 0) { 
            double absg = std::abs(shearcat.getShear(i));
            if (absg > maxg) maxg = absg;
            absgamma.push_back(absg);
        }
        std::cout<<"absgamma.size = "<<absgamma.size()<<std::endl;
        sort(absgamma.begin(),absgamma.end());
        std::cout<<"shear by decile = "<<std::endl;
        for(int i=0;i<10;++i) {
            std::cout<<i*10<<"%  ==  "<<absgamma[int(absgamma.size()*double(i)/10.)]<<std::endl;
        }
        std::cout<<"100%  ==  "<<absgamma.back()<<std::endl;

        // Distribute the galaxies according to their measured shear.
        // We will have total of 48 bins:
        // There are 4 ranges for abs(g):
        // 0 < abs(g) < g1 
        // g1 < abs(g) < g2
        // g2 < abs(g) < g3
        // g3 < abs(g) < maxg
        //
        // with g1,g2,g3 being the quartile values of abs(g)
        //
        // Within each of these ranges, we have 12 bins in azimuth.
        
        double g1 = absgamma[int(absgamma.size()*0.25)];
        double g2 = absgamma[int(absgamma.size()*0.5)];
        double g3 = absgamma[int(absgamma.size()*0.75)];
        std::vector<std::vector<long> > idLists(48);
        for(int i=0; i<ngal; ++i) if (shearcat.getFlags(i) == 0) {
            double absg = std::abs(shearcat.getShear(i));
            double argg = std::arg(shearcat.getShear(i)) * 180./PI;
            int k1 = 
                absg < g1 ? 0 : 
                absg < g2 ? 1 :
                absg < g3 ? 2 :
                3;
            int k2 = 
                argg < -150. ? 0 : 
                argg < -120. ? 1 :
                argg < -90. ? 2 :
                argg < -60. ? 3 :
                argg < -30. ? 4 :
                argg < 0. ? 5 :
                argg < 30. ? 6 :
                argg < 60. ? 7 :
                argg < 90. ? 8 :
                argg < 120. ? 9 :
                argg < 150. ? 10 :
                11;
            idLists[12*k1 + k2].push_back(i);
        }
        std::cout<<"g ranges are:\n";
        std::cout<<"0 -- "<<g1<<std::endl;
        std::cout<<g1<<" -- "<<g2<<std::endl;
        std::cout<<g2<<" -- "<<g3<<std::endl;
        std::cout<<g3<<" -- "<<maxg<<std::endl;
        for(int k=0;k<48;++k) {
            std::cout<<"idLists["<<k<<"].size = "<<idLists[k].size()<<std::endl;
        }

        if (timing) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Measure Shears = "<<t2-t1<<std::endl;
            std::cout<<"Rate: "<<(t2-t1)/shearcat.size()<<" s / gal\n";
            t1 = t2;
        }

        // Write results to file
        //shearcat.write();

        if (timing) {
            gettimeofday(&tp,0);
            t2 = tp.tv_sec + tp.tv_usec/1.e6;
            std::cout<<"Time: Write ShearCatalog = "<<t2-t1<<std::endl;
            t1 = t2;
        }

        xdbg<<"Shear Log: \n"<<*log<<std::endl;
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
