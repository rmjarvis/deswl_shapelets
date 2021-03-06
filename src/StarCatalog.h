#ifndef StarCatalog_H
#define StarCatalog_H

#include <vector>
#include <string>
#include "Transformation.h"
#include "ConfigFile.h"
#include "Bounds.h"
#include "InputCatalog.h"
#include "Image.h"
#include "Log.h"

// This function is also used by PSFCatalog.
void CalculateSigma(
    double& sigma, // Initial value -- use <=0 if no initial guess
     double &nu, // if less than zero it will not be calculated
    const Image<double>& im, const Position& pos, double sky, 
    double noise, const Image<double>* weight_image,
    const Transformation& trans, const ConfigFile& params,
    long& flag, bool use_shapelet_sigma);

class StarCatalog
{
public:
    // Make from incat
    // fs_prefix is the prefix of the keywords for the 
    // parameters used by the findstars algorithm.
    StarCatalog(
        const InputCatalog& incat,
        const ConfigFile& params, std::string fs_prefix = "stars_");

    // copy the input StarCatalog
    StarCatalog(const StarCatalog& rhs);

    // In this version, we take a subset of the input star catalog.
    // note indices must be sorted!
    StarCatalog(const StarCatalog& rhs, const std::vector<long> indices);

    // Setup the parameters.  Normally followed by read() or similar.
    StarCatalog(const ConfigFile& params, std::string fs_prefix = "stars_");

    // a deterministic seed is generated based on size of catalog and number
    // of stars
    void splitInTwo(const std::string f1, const std::string f2) const;
    // seed explicitly sent
    void splitInTwo(
        const std::string f1, const std::string f2, const int seed) const;

    int size() const { return _id.size(); }
    void read();
    void write() const;

    void readFits(std::string file);
    void readFitsOld(std::string file);
    void readAscii(std::string file, std::string delim = "  ");
    void writeFitsOld(std::string file) const;
    void writeFits(std::string file) const;
    void writeAscii(std::string file, std::string delim = "  ") const;

    void calculateSizes(
        const Image<double>& im, 
        const Image<double>* weight_im, const Transformation& trans);

    int findStars(FindStarsLog& log);

    const std::vector<long>& getIdList() const { return _id; }
    const std::vector<Position>& getPosList() const { return _pos; }
    const std::vector<double>& getSkyList() const { return _sky; }
    const std::vector<double>& getNoiseList() const { return _noise; }
    const std::vector<long>& getFlagsList() const { return _flags; }
    const std::vector<double>& getMagList() const { return _mag; }
    const std::vector<double>& getSgList() const { return _sg; }
    const std::vector<double>& getObjSizeList() const { return _objsize; }
    const std::vector<bool>& getIsStarList() const { return _is_star; }


    const ConfigFile& getParams() const { return _params; }
    template <typename T>
    void setPar(const std::string key, const T& val) { _params.add(key,val); }

    long     getId(int i)      const { return _id[i]; }
    Position getPos(int i)     const { return _pos[i]; }
    double   getSky(int i)     const { return _sky[i]; }
    double   getNoise(int i)   const { return _noise[i]; }
    long     getFlags(int i)   const { return _flags[i]; }
    double   getMag(int i)     const { return _mag[i]; }
    double   getSg(int i)      const { return _sg[i]; }
    double   getObjSize(int i) const { return _objsize[i]; }
    bool     getIsStar(int i)  const { return _is_star[i]; }
             
    // This one looks for the given id.  
    // If it is in the catalog _and_ it is marked as a star, it returns true.
    // This way the StarCatalog may have a different number of objects than
    // some other catalog that wants to use it, so the i's don't have to match.
    // But the ID's should still refer to the same objects.
    // If you know you have the same i values, getIsStar is faster.
    bool isStar(long id) const;

    void printall(int i);

private :

    std::vector<long> _id;
    std::vector<Position> _pos;
    std::vector<double> _sky;
    std::vector<double> _noise;
    std::vector<long> _flags;

    std::vector<double> _mag;
    std::vector<double> _sg;
    std::vector<double> _nu;  // these are not currently written to file
    std::vector<double> _objsize;
    std::vector<bool> _is_star;

    //const ConfigFile& _params;
    // We need to be able to alter this if we want to write alternative
    // names.  So we'll make our own copy
    ConfigFile _params;
    std::string _prefix;
};

#endif
