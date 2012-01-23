
#include "dbg.h"
#include "ConfigFile.h"
#include "Params.h"
#include "Log.h"
#include "MultiShearCatalog.h"
#include "Ellipse.h"
#include "ShearCatalog.h"
#include "ShearCatalogTree.h"
#include "MeasureShearAlgo.h"

int MultiShearCatalog::measureMultiShears(const Bounds& b, ShearLog& log)
{
    dbg<<"Start MeasureMultiShears for b = "<<b<<std::endl;
    int nGals = _skyPos.size();
    dbg<<"ngals = "<<nGals<<std::endl;

    // Read some needed parameters
    double galAperture = _params.read<double>("shear_aperture");
    double maxAperture = _params.read("shear_max_aperture",0.);
    int galOrder = _params.read<int>("shear_gal_order");
    int galOrder2 = _params.read<int>("shear_gal_order2");
    int maxm = _params.read("shear_maxm",galOrder);
    int minGalOrder = _params.read("shear_min_gal_order",4);
    bool baseOrderOnNu = _params.read("shear_base_order_on_nu",true);
    double minFPsf = _params.read("shear_f_psf",1.);
    double maxFPsf = _params.read("shear_max_f_psf",minFPsf);
    double minGalSize = _params.read<double>("shear_min_gal_size");
    bool galFixCen = _params.read("shear_fix_centroid",false);
    bool galFixSigma = _params.keyExists("shear_force_sigma");
    double galFixSigmaValue = _params.read("shear_force_sigma",0.);
    bool shouldOutputDots = _params.read("output_dots",false);
#ifdef _OPENMP
    bool shouldOutputDesQa = _params.read("des_qa",false); 
#endif
    bool nativeOnly = _params.read("shear_native_only",false);

    int nSuccess = 0;

#ifdef ENDAT
    nGals = ENDAT;
#endif

    // Main loop to measure shears
#ifdef _OPENMP
#pragma omp parallel 
    {
        try {
#endif
            ShearLog log1(_params); // just for this thread
            log1.noWriteLog();
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
            for(int i=0;i<nGals;++i) {
                if (!b.includes(_skyPos[i])) continue;
                if (_flags[i]) continue;
#ifdef STARTAT
                if (i < STARTAT) continue;
#endif
#ifdef SINGLEGAL
                if (i < SINGLEGAL) continue;
                if (i > SINGLEGAL) break;
#endif

                if (shouldOutputDots) {
#ifdef _OPENMP
#pragma omp critical (output)
#endif
                    {
                        std::cerr<<"."; std::cerr.flush(); 
                    }
                }
                dbg<<"galaxy "<<i<<":\n";
                dbg<<"id = "<<_id[i]<<std::endl;
                dbg<<"chipPos = "<<_chipPos[i]<<std::endl;
                dbg<<"skypos = "<<_skyPos[i]<<std::endl;

                int nEpoch = _pixList[i].size();
                if (nEpoch == 0) {
                    dbg<<"no valid single epoch images.\n";
                    dbg<<"FLAG NO_SINGLE_EPOCH_IMAGES\n";
                    _flags[i] = NO_SINGLE_EPOCH_IMAGES;
                    continue;
                }

                dbg<<"Using "<<nEpoch<<" epochs\n";
                Assert(nEpoch == int(_seNum[i].size()));
                Assert(nEpoch == int(_sePos[i].size()));
                dbg<<"seImageNum   seImageName   sePos\n";
                for (int k=0;k<nEpoch;++k) {
                    dbg<<_seNum[i][k]<<"  "<<
                        _imageFileList[_seNum[i][k]]<<"  "<<
                        _sePos[i][k]<<std::endl;
                }

                _measGalOrder[i] = galOrder;
#if 1
                DoMeasureShear(
                    // Input data:
                    _pixList[i], _psfList[i],
                    // Parameters:
                    galAperture, maxAperture,
                    _measGalOrder[i], galOrder2, maxm,
                    minGalOrder, baseOrderOnNu,
                    minFPsf, maxFPsf, minGalSize, galFixCen, 
                    galFixSigma, galFixSigmaValue, nativeOnly,
                    // Log information
                    log1,
                    // Ouput values:
                    _shape[i], _shear[i], _cov[i], _nu[i], _flags[i]);
#else
                _shear[i] = std::complex<double>(0.1,0.2);
                _cov[i] << 1., 0., 0., 1.;
                _shape[i].vec().setZero();
                _nu[i] = 10.;
#endif

                if (!_flags[i]) {
                    dbg<<"Successful shear measurements: \n";
                    dbg<<"shape = "<<_shape[i]<<std::endl;
                    dbg<<"shear = "<<_shear[i]<<std::endl;
                    dbg<<"cov = "<<_cov[i]<<std::endl;
                    dbg<<"nu = "<<_nu[i]<<std::endl;
                    ++nSuccess;
                } else {
                    dbg<<"Unsuccessful shear measurement\n"; 
                    dbg<<"flag = "<<_flags[i]<<std::endl;
                }
            }
#ifdef _OPENMP
#pragma omp critical (add_log)
#endif
            {
                log += log1;
            }
#ifdef _OPENMP
        } catch (std::exception& e) {
            // This isn't supposed to happen.
            if (shouldOutputDesQa) {
                std::cerr<<"STATUS5BEG Caught error in parallel region STATUS5END\n";
            } 
            std::cerr<<"Caught "<<e.what()<<std::endl;
            std::cerr<<"Caught error in parallel region.  Aborting.\n";
            exit(1);
        } catch (...) {
            if (shouldOutputDesQa) {
                std::cerr<<"STATUS5BEG Caught error in parallel region STATUS5END\n";
            }
            std::cerr<<"Caught error in parallel region.  Aborting.\n";
            exit(1);
        }
    }
#endif

    dbg<<nSuccess<<" successful shear measurements in this pass.\n";
    dbg<<log._nsGamma<<" successful shear measurements so far.\n";

    // Get value of VmPeak at this point in program.
    double peak_mem = peak_memory_usage(dbgout);
    dbg<<"Peak memory usage so far = "<<peak_mem<<std::endl;
    _params["peak_mem"] = peak_mem; // We'll write it to output file.

    xdbg<<log<<std::endl;

    return nSuccess;
}

static void getImagePixList(
    std::vector<PixelList>& pixList,
    std::vector<BVec>& psfList,
    std::vector<int>& seNum, std::vector<Position>& sePos, int seIndex,
    long& inputFlags, int& nImagesFound, int& nImagesGotPix,
    const Position& skyPos,
    const Image<double>& im,
    const Transformation& trans,
    const Transformation& invTrans,
    BVec& psf, const FittedPsf& fitPsf,
    const ShearCatalog& shearCat,
    const ShearCatalogTree& shearCatTree,
    const Image<double>*const weightIm,
    const double noise, const double gain,
    const std::string& skyMethod, const double meanSky, 
    const Image<double>*const skyMap,
    double galAperture, double maxAperture, double xOffset, double yOffset,
    bool require_match)
{
    Assert(psfList.size() == pixList.size());
    Assert(seNum.size() == pixList.size());
    Assert(sePos.size() == pixList.size());

    // Convert ra/dec to x,y in this image

    // First, figure out a good starting point for the nonlinear solver:
    Position pos;
    xdbg<<"skypos = "<<skyPos<<std::endl;
    invTrans.transform(skyPos,pos);
    xdbg<<"invtrans(skypos) = "<<pos<<std::endl;

    // Now do the full non-linear solver, which should be pretty fast
    // given the decent initial guess.
    if (!trans.inverseTransform(skyPos, pos) ) {
        dbg << "InverseTransform failed for position "<<skyPos<<".\n";
        dbg << "Initial guess was "<<pos<<".\n";
        inputFlags |= TRANSFORM_EXCEPTION;
    }
    xdbg<<"after exact InverseTransform: pos -> "<<pos<<std::endl;
    if (!(fitPsf.getBounds().includes(pos))) {
        xdbg<<"Reject pos "<<pos<<" not in fitpsf bounds ";
        xdbg<<fitPsf.getBounds()<<std::endl;
        return;
    }

    ++nImagesFound;

    try {
        psf = fitPsf(pos);
    } catch (RangeException& e) {
        xdbg<<"fittedpsf range error: \n";
        xdbg<<"p = "<<pos<<", b = "<<e.getBounds()<<std::endl;
        inputFlags |= FITTEDPSF_EXCEPTION;
        return;
    }

    // Make sure the use of trans in getPixList won't throw:
    try {
        // We don't need to save skyPos.  We just want to catch the range
        // error here, so we don't need to worry about it for dudx, etc.
        Position skyPos1;
        trans.transform(pos,skyPos1);
    } catch (RangeException& e) {
        dbg<<"distortion range error: \n";
        xdbg<<"p = "<<pos<<", b = "<<e.getBounds()<<std::endl;
        inputFlags |= TRANSFORM_EXCEPTION;
        return;
    }

    xdbg<<"Start getImagePixList: mem = "<<memory_usage()<<std::endl;

    // Find the nearest object in the shear catalog:
    int nearest = shearCatTree.findNearestTo(pos);

    double galAp = maxAperture;
    if (std::abs(shearCat.getPos(nearest) - pos) < 1.) { 
        double seSize = shearCat.getShape(nearest).getSigma();
        dbg<<"Single epoch id, pos, shear, size = "<<
            shearCat.getId(nearest)<<"  "<<
            shearCat.getPos(nearest)<<"  "<<
            shearCat.getShear(nearest)<<"  "<<
            seSize<<std::endl;
        // If we have a good measurement from the single_epoch image, use
        // that sigma for the size.
        // But expand it by 30% in case we need it.
        galAp = galAperture * seSize * 1.3;
        if (galAp > maxAperture) galAp = maxAperture;
    } else if (require_match) {
        xdbg<<"No match found in single epoch catalog\n";
        // This isn't really the same meaning of this flag in the
        // normal flags value, but it doesn't seem worth making a 
        // new name.  Here it means that an image was rejected because
        // there was no valid measurement in the single-epoch run.
        inputFlags |= NO_SINGLE_EPOCH_IMAGES;
        return;
    }

    // Calculate the local sky value.
    long flag = 0;
    double sky=0;
    if (skyMethod == "MEAN") {
        sky = meanSky;
    } else if (skyMethod == "NEAREST") {
        sky = shearCat.getSky(nearest);
    } else {
        Assert(skyMethod == "MAP");
        sky = getLocalSky(*skyMap,pos,trans,galAp,xOffset,yOffset,flag);
        // If no pixels in aperture, then
        // a) something is very wrong, but
        // b) use the NEAREST method instead.
        if (flag & BKG_NOPIX) sky = shearCat.getSky(nearest);
        inputFlags |= flag;
    }

    xdbg<<"Before push_back(PixelLise) mem = "<<memory_usage()<<std::endl;
    xdbg<<"pixlist.size = "<<pixList.size()<<std::endl;
    pixList.push_back(PixelList());
    xdbg<<"pixlist.size => "<<pixList.size()<<std::endl;
    xdbg<<"After push_back(PixelLise) mem = "<<memory_usage()<<std::endl;
#ifdef PIXELLIST_BLOCK
    pixList.back().usePool();
#endif

    flag = 0;
    xdbg<<"Before getPixList mem = "<<memory_usage()<<std::endl;
    getPixList(
        im,pixList.back(),pos,
        sky,noise,gain,weightIm,trans,galAp,xOffset,yOffset,flag);
    xdbg<<"Got pixellist, flag = "<<FlagText(flag)<<std::endl;
    xdbg<<"After getPixList mem = "<<memory_usage()<<std::endl;
    

    // Make sure not (edge or < 10 pixels) although edge is already
    // checked above
    if (flag == 0) {
        xdbg<<"Before psfList.push_back(psf): mem = "<<memory_usage()<<std::endl;
        xdbg<<"psflist.size = "<<psfList.size()<<std::endl;
        psfList.push_back(psf);
        xdbg<<"psflist.size => "<<psfList.size()<<std::endl;
        xdbg<<"After psfList.push_back(psf): mem = "<<memory_usage()<<std::endl;
        seNum.push_back(seIndex);
        sePos.push_back(pos);
        ++nImagesGotPix;
    } else {
        inputFlags |= flag;
        xdbg<<"Before pixList.pop_back: mem = "<<memory_usage()<<std::endl;
        xdbg<<"pixlist.size = "<<pixList.size()<<std::endl;
        pixList.pop_back();
        xdbg<<"pixlist.size => "<<pixList.size()<<std::endl;
        xdbg<<"After pixList.pop_back: mem = "<<memory_usage()<<std::endl;
    }
    xdbg<<"Done getImagePixList: mem = "<<memory_usage()<<std::endl;
}

// Get pixel lists from the file specified in params
bool MultiShearCatalog::getImagePixelLists(
    int seIndex, const Bounds& bounds)
{
    dbg<<"Start GetImagePixelLists: se_index = "<<seIndex<<std::endl;

    // If the skybounds for each shear catalog have been saved, then
    // we might be able to skip the ShearCatalog load.
    if (int(_savedSeSkyBounds.size()) > seIndex) {
        Bounds seSkyBounds = _savedSeSkyBounds[seIndex];
        dbg<<"saved bounds for image "<<seIndex<<" = "<<seSkyBounds;
        if (!seSkyBounds.intersects(bounds)) {
            dbg<<"Skipping index "<<seIndex<<
                " because bounds don't intersect\n";
            return true;
        }
    }

    // Read the shear catalog
    ShearCatalog shearCat(_params);
    shearCat.read();
    Bounds seSkyBounds = shearCat.getSkyBounds();
    Bounds seBounds = shearCat.getBounds();
    dbg<<"bounds for image "<<seIndex<<" = "<<seSkyBounds;

    // Skip this file if none of the objects in it are in this section of sky.
    if (int(_savedSeSkyBounds.size()) <= seIndex) {
        // Then save the seSkyBounds
        Assert(int(_savedSeSkyBounds.size()) == seIndex);
        _savedSeSkyBounds.push_back(seSkyBounds);
    }
    if (!seSkyBounds.intersects(bounds)) {
        dbg<<"Skipping index "<<seIndex<<" because bounds don't intersect\n";
        return true;
    }

    // Read transformation between ra/dec and x/y
    Transformation trans(_params);

    // Read the psf
    FittedPsf fitPsf(_params);
    fitPsf.read();

    // Make a tree of the shear catalog to more easily find the nearest
    // single-epoch object to each coadd detection.
    ShearCatalogTree shearCatTree(shearCat);

    // Figure out which method we are going to use to calculate the 
    // local sky values.
    std::string skyMethod = _params.get("multishear_sky_method");
    Assert(skyMethod=="MEAN" || skyMethod=="NEAREST" || skyMethod=="MAP");
    double meanSky=0.;
    std::auto_ptr<Image<double> > skyMap(0);
    if (skyMethod == "MEAN") {
        const int nGals = shearCat.size();
        for(int i=0;i<nGals;++i) meanSky += shearCat.getSky(i);
        meanSky /= shearCat.size();
    }
    if (skyMethod == "MAP") {
        std::string skyMapName = makeName(_params,"skymap",true,true);
        int skyMapHdu = getHdu(_params,"skymap",skyMapName,1);
        skyMap.reset(new Image<double>(skyMapName,skyMapHdu));
    }

    BVec psf(fitPsf.getPsfOrder(), fitPsf.getSigma());

    // Make an inverse transformation that we will use as a starting 
    // point for the more accurate InverseTransform function.
    Transformation invTrans;
    Bounds invBounds = invTrans.makeInverseOf(trans,seBounds,4);
    dbg<<"skybounds = "<<_skyBounds<<std::endl;
    dbg<<"se_skybounds = "<<seSkyBounds<<std::endl;
    dbg<<"se_bounds = "<<seBounds<<std::endl;
    dbg<<"invb = "<<invBounds<<std::endl;

    // We always use the maximum aperture size here, since we don't know
    // how big the galaxy is yet, so we don't know what galAp will be.
    double galAperture = _params.get("shear_aperture");
    double maxAperture = _params.get("shear_max_aperture");

    // Load the image
    // The bounds needed are 
    std::auto_ptr<Image<double> > image;
    std::auto_ptr<Image<double> > weightIm;
    if (_skyBounds.includes(seSkyBounds)) {
        image.reset(new Image<double>(_params,weightIm));
    } else if (!_skyBounds.intersects(invBounds)) {
        dbg<<"Skipping index "<<seIndex<<" because invBounds doesn't intersect\n";
        return true;
    } else {
        Bounds intersect = invBounds & _skyBounds;
        dbg<<"intersect = invb & skybounds = "<<intersect<<std::endl;

        Bounds subBounds;
        subBounds += invTrans(intersect.get00());
        subBounds += invTrans(intersect.get01());
        subBounds += invTrans(intersect.get10());
        subBounds += invTrans(intersect.get11());

        // Grow bounds by maxAperture
        DSmallMatrix22 D;
        trans.getDistortion(subBounds.getCenter(),D);
        double det = std::abs(D.TMV_det());
        double pixScale = sqrt(det); // arcsec/pixel
        subBounds.addBorder(maxAperture / pixScale);

        dbg<<"subb = "<<subBounds<<std::endl;
        image.reset(new Image<double>(_params,weightIm,subBounds));
    }

    // We are using the weight image so the noise and gain are dummy variables
    Assert(weightIm.get());
    double noise = 0.0;
    double gain = 0.0;

    dbg<<"Extracting pixel lists\n";
    // loop over the the objects, if the object falls on the image get
    // the pixel list
    Assert(int(_skyPos.size()) == size());
    Assert(int(_pixList.size()) == size());
    Assert(int(_psfList.size()) == size());
    Assert(int(_flags.size()) == size());
    Assert(int(_inputFlags.size()) == size());
    Assert(int(_nImagesFound.size()) == size());
    Assert(int(_nImagesGotPix.size()) == size());

    const int nGals = size();
    double xOffset = _params.read("cat_x_offset",0.);
    double yOffset = _params.read("cat_y_offset",0.);
    double maxMem = _params.read("max_vmem",64)*1024.;
    bool require_match = _params.read("multishear_require_match",false);
    dbg<<"Before getImagePixList loop: memory_usage = "<<memory_usage()<<std::endl;
    for (int i=0; i<nGals; ++i) {
        if (_flags[i]) continue;
        if (!bounds.includes(_skyPos[i])) continue;
        if (!invBounds.includes(_skyPos[i])) continue;
        dbg<<"getImagePixList for galaxy "<<i<<", id = "<<_id[i]<<std::endl;
        getImagePixList(
            _pixList[i], _psfList[i],
            _seNum[i], _sePos[i], seIndex,
            _inputFlags[i], _nImagesFound[i], _nImagesGotPix[i], _skyPos[i], 
            *image, trans, invTrans, psf, fitPsf, shearCat, shearCatTree,
            weightIm.get(), noise, gain,
            skyMethod, meanSky, skyMap.get(),
            galAperture, maxAperture, xOffset, yOffset, require_match);
        double mem = memory_usage();
        // memory_usage is in KB
        // max_vmem is in GB
        if (mem > maxMem) {
            dbg<<"VmSize = "<<mem<<" > max_vmem = "<<maxMem<<std::endl;
            return false;
        }
    }
    // Keep track of how much memory we are using.
    dbg<<"Done getImagePixList loop: memory_usage = "<<memory_usage()<<std::endl;
    if (dbgout) PixelList::dumpPool(*dbgout);
    dbg<<"Using image# "<<seIndex;
    dbg<<"... Memory Usage in MultiShearCatalog = ";
    dbg<<calculateMemoryFootprint()<<" MB";
    dbg<<"\n";
    dbg<<"VMem = "<<memory_usage()<<std::endl;
    double peak_mem = peak_memory_usage();
    dbg<<"VPeak = "<<peak_mem<<std::endl;
    dbg<<"maxMem allowed = "<<maxMem<<std::endl;
    _params["peak_mem"] = peak_mem;  // Keep track

    dbg<<"Done extracting pixel lists\n";
    return true;
}

template <typename T>
inline long long getMemoryFootprint(const T& x)
{
    return sizeof(x); 
}

template <typename T>
inline long long getMemoryFootprint(T*const x)
{
    long long res=sizeof(x);
    res += getMemoryFootprint(*x);
    return res;
}

template <typename T>
inline long long getMemoryFootprint(const T*const x)
{
    long long res=sizeof(x);
    res += getMemoryFootprint(*x);
    return res;
}

template <typename T>
inline long long getMemoryFootprint(const std::auto_ptr<T>& x)
{
    long long res=sizeof(x);
    res += getMemoryFootprint(*x);
    return res;
}

template <typename T, class Alloc>
inline long long getMemoryFootprint(const std::vector<T,Alloc>& x)
{
    long long res=sizeof(x);
    const int xsize = x.size();
    for(int i=0;i<xsize;++i) res += getMemoryFootprint(x[i]);
    return res;
}

inline long long getMemoryFootprint(const PixelList& x)
{
    long long res=sizeof(x);
    res += x.size() * sizeof(Pixel);
    return res;
}

template <typename T>
inline long long getMemoryFootprint(const TVector(T)& x)
{
    long long res=sizeof(x);
    res += x.size() * sizeof(T);
    return res;
}

inline long long getMemoryFootprint(const BVec& x)
{
    long long res=sizeof(x);
    res += x.size() * sizeof(double);
    return res;
}

template <typename T>
inline long long getMemoryFootprint(const TMatrix(T)& x)
{
    long long res=sizeof(x);
    res += x.colsize() * x.rowsize() * sizeof(T);
    return res;
}

double MultiShearCatalog::calculateMemoryFootprint() const
{
    dbg<<"Memory usage:\n";
    dbg<<"input_flags: "<<
        getMemoryFootprint(_inputFlags)/1024./1024.<<" MB\n";
    dbg<<"nimages_found: "<<
        getMemoryFootprint(_nImagesFound)/1024./1024.<<" MB\n";
    dbg<<"pixlist: "<<
        getMemoryFootprint(_pixList)/1024./1024.<<" MB\n";
    dbg<<"nimages_gotpix: "<<
        getMemoryFootprint(_nImagesGotPix)/1024./1024.<<" MB\n";
    dbg<<"id: "<<
        getMemoryFootprint(_id)/1024./1024.<<" MB\n";
    dbg<<"skypos: "<<
        getMemoryFootprint(_skyPos)/1024./1024.<<" MB\n";
    dbg<<"flags: "<<
        getMemoryFootprint(_flags)/1024./1024.<<" MB\n";
    dbg<<"shear: "<<
        getMemoryFootprint(_shear)/1024./1024.<<" MB\n";
    dbg<<"nu: "<<
        getMemoryFootprint(_nu)/1024./1024.<<" MB\n";
    dbg<<"cov: "<<
        getMemoryFootprint(_cov)/1024./1024.<<" MB\n";
    dbg<<"shape: "<<
        getMemoryFootprint(_shape)/1024./1024.<<" MB\n";
    dbg<<"psflist: "<<
        getMemoryFootprint(_psfList)/1024./1024.<<" MB\n";
    dbg<<"image_file_list: "<<
        getMemoryFootprint(_imageFileList)/1024./1024.<<" MB\n";
    dbg<<"shear_file_list: "<<
        getMemoryFootprint(_shearFileList)/1024./1024.<<" MB\n";
    dbg<<"fitpsf_file_list: "<<
        getMemoryFootprint(_fitPsfFileList)/1024./1024.<<" MB\n";
    dbg<<"skymap_file_list: "<<
        getMemoryFootprint(_skyMapFileList)/1024./1024.<<" MB\n";
    dbg<<"saved_se_skybounds: "<<
        getMemoryFootprint(_savedSeSkyBounds)/1024./1024.<<" MB\n";

    double totMem = 
        getMemoryFootprint(_inputFlags) +
        getMemoryFootprint(_nImagesFound) +
        getMemoryFootprint(_pixList) +
        getMemoryFootprint(_nImagesGotPix) +
        getMemoryFootprint(_id) +
        getMemoryFootprint(_skyPos) +
        getMemoryFootprint(_flags) +
        getMemoryFootprint(_shear) +
        getMemoryFootprint(_nu) +
        getMemoryFootprint(_cov) +
        getMemoryFootprint(_shape) +
        getMemoryFootprint(_psfList) +
        getMemoryFootprint(_imageFileList) +
        getMemoryFootprint(_shearFileList) +
        getMemoryFootprint(_fitPsfFileList) +
        getMemoryFootprint(_skyMapFileList) +
        getMemoryFootprint(_savedSeSkyBounds);
    totMem /= 1024.*1024.;  // B -> MB
    dbg<<"totmem = "<<totMem<<std::endl;

    return totMem;
}

