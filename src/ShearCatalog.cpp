
#include <fstream>
#include <iostream>
#include <valarray>
#include "TMV.h"
#include <CCfits/CCfits>

#include "ShearCatalog.h"
#include "ConfigFile.h"
#include "dbg.h"
#include "Params.h"
#include "BVec.h"
#include "Ellipse.h"
#include "Pixel.h"
#include "Image.h"
#include "FittedPsf.h"
#include "Log.h"
#include "Params.h"
#include "Form.h"
#include "WlVersion.h"
#include "WriteParam.h"

ShearCatalog::ShearCatalog(
    const InputCatalog& inCat,
    const Transformation& trans, const ConfigFile& params) :
    _id(inCat.getIdList()), _pos(inCat.getPosList()), 
    _sky(inCat.getSkyList()), _noise(inCat.getNoiseList()),
    _flags(inCat.getFlagsList()), _skyPos(inCat.getSkyPosList()),
    _bounds(inCat.getBounds()), _skyBounds(inCat.getSkyBounds()),
    _params(params)
{
    dbg<<"Create ShearCatalog\n";
    const int nGals = _id.size();

    // Fix flags to only have INPUT_FLAG set.
    // (I don't think this is necessary, but just in case.)
    for (int i=0; i<nGals; ++i) {
        _flags[i] &= INPUT_FLAG;
    }

    // Calculate sky positions
    Position skyPosDefault(DEFVALNEG,DEFVALNEG);
    if (_skyPos.size() == 0) {
        _skyPos.resize(nGals,skyPosDefault);
        for(int i=0;i<nGals;++i) {
            try {
                trans.transform(_pos[i],_skyPos[i]);
            } catch (RangeException& e) {
                xdbg<<"distortion range error\n";
                xdbg<<"p = "<<_pos[i]<<", b = "<<e.getBounds()<<std::endl;
            }                       
            if (!_flags[i]) _skyBounds += _skyPos[i];
        }
    } else {
        Assert(int(_skyPos.size()) == nGals);

        xdbg<<"Check transformation:\n";
        double rmsError = 0.;
        int count = 0;
        for(int i=0;i<nGals;++i) {
            try {
                Position temp;
                trans.transform(_pos[i],temp);
                xdbg<<_pos[i]<<"  "<<_skyPos[i]/3600.<<"  "<<temp/3600.;
                xdbg<<"  "<<temp-_skyPos[i]<<std::endl;
                rmsError += std::norm(temp-_skyPos[i]);
                ++count;
            } catch (RangeException& e) {
                xdbg<<"distortion range error\n";
                xdbg<<"p = "<<_pos[i]<<", b = "<<e.getBounds()<<std::endl;
            }
        }
        rmsError /= count;
        rmsError = std::sqrt(rmsError);
        xdbg<<"rms error = "<<rmsError<<" arcsec\n";
        if (_params.read("des_qa",false)) {
            if (rmsError > 0.1) {
                std::cout<<
                    "STATUS3BEG Warning: Positions from WCS transformation "
                    "have rms error of "<<rmsError<<" arcsec relative to "
                    "ra, dec in catalog. STATUS3END"<<std::endl;
            }
        }
    }

    _shear.resize(nGals,std::complex<double>(DEFVALPOS,DEFVALPOS));
    _nu.resize(nGals,DEFVALNEG);

    tmv::SmallMatrix<double,2,2> covDefault;
    covDefault = tmv::ListInit, DEFVALPOS, 0, 0, DEFVALPOS;
    _cov.resize(nGals,covDefault);

    int galOrder = _params.read<int>("shear_gal_order");
    BVec shapeDefault(galOrder,1.);
    shapeDefault.vec().SetAllTo(DEFVALNEG);
    _shape.resize(nGals,shapeDefault);

    Assert(_id.size() == size());
    Assert(_pos.size() == size());
    Assert(_sky.size() == size());
    Assert(_noise.size() == size());
    Assert(_flags.size() == size());
    Assert(_skyPos.size() == size());
    Assert(_shear.size() == size());
    Assert(_nu.size() == size());
    Assert(_cov.size() == size());
    Assert(_shape.size() == size());
}

ShearCatalog::ShearCatalog(const ConfigFile& params) : _params(params)
{
    read();

    Assert(_id.size() == size());
    Assert(_pos.size() == size());
    Assert(_sky.size() == size());
    Assert(_noise.size() == size());
    Assert(_flags.size() == size());
    Assert(_skyPos.size() == size());
    Assert(_shear.size() == size());
    Assert(_nu.size() == size());
    Assert(_cov.size() == size());
    Assert(_shape.size() == size());
}

void ShearCatalog::writeFits(std::string file) const
{
    Assert(_id.size() == size());
    Assert(_pos.size() == size());
    Assert(_sky.size() == size());
    Assert(_noise.size() == size());
    Assert(_flags.size() == size());
    Assert(_shear.size() == size());
    Assert(_nu.size() == size());
    Assert(_cov.size() == size());
    Assert(_shape.size() == size());
    const int nGals = size();

    // ! means overwrite existing file
    CCfits::FITS fits("!"+file, CCfits::Write);

    const int nFields=17;
    std::vector<string> colNames(nFields);
    std::vector<string> colFmts(nFields);
    std::vector<string> colUnits(nFields);

    colNames[0] = _params.get("shear_id_col");
    colNames[1] = _params.get("shear_x_col");
    colNames[2] = _params.get("shear_y_col");
    colNames[3] = _params.get("shear_sky_col");
    colNames[4] = _params.get("shear_noise_col");
    colNames[5] = _params.get("shear_flags_col");
    colNames[6] = _params.get("shear_ra_col");
    colNames[7] = _params.get("shear_dec_col");
    colNames[8] = _params.get("shear_shear1_col");
    colNames[9] = _params.get("shear_shear2_col");
    colNames[10] = _params.get("shear_nu_col");
    colNames[11] = _params.get("shear_cov00_col");
    colNames[12] = _params.get("shear_cov01_col");
    colNames[13] = _params.get("shear_cov11_col");
    colNames[14] = _params.get("shear_order_col");
    colNames[15] = _params.get("shear_sigma_col");
    colNames[16] = _params.get("shear_coeffs_col");

    colFmts[0] = "1J"; // id
    colFmts[1] = "1D"; // x
    colFmts[2] = "1D"; // y
    colFmts[3] = "1D"; // sky
    colFmts[4] = "1D"; // noise
    colFmts[5] = "1J"; // flags
    colFmts[6] = "1D"; // ra
    colFmts[7] = "1D"; // dec
    colFmts[8] = "1D"; // shear1
    colFmts[9] = "1D"; // shear2
    colFmts[10] = "1D"; // nu
    colFmts[11] = "1D"; // cov00
    colFmts[12] = "1D"; // cov01
    colFmts[13] = "1D"; // cov11
    colFmts[14] = "1J"; // order
    colFmts[15] = "1D"; // sigma

    int nCoeff = _shape[0].size();
    dbg<<"ncoeff = "<<nCoeff<<std::endl;
    colFmts[16] = ConvertibleString(nCoeff) + "D"; // shapelet coeffs
    dbg<<"colfmts[16] = "<<colFmts[16]<<std::endl;

    colUnits[0] = "None";   // id
    colUnits[1] = "Pixels"; // x
    colUnits[2] = "Pixels"; // y
    colUnits[3] = "ADU";    // sky
    colUnits[4] = "ADU^2";  // noise
    colUnits[5] = "None";   // flags
    colUnits[6] = "Deg";    // ra
    colUnits[7] = "Deg";    // dec
    colUnits[8] = "None";   // shear1
    colUnits[9] = "None";   // shear2
    colUnits[10] = "None";  // nu
    colUnits[11] = "None";  // cov00
    colUnits[12] = "None";  // cov01
    colUnits[13] = "None";  // cov11
    colUnits[14] = "None";  // order
    colUnits[15] = "Arcsec";// sigma
    colUnits[16] = "None";  // coeffs

    dbg<<"Before Create table"<<std::endl;
    CCfits::Table* table;
    table = fits.addTable("shearcat",nGals,colNames,colFmts,colUnits);

    // Header keywords
    std::string tmvVers = tmv::TMV_Version();
    std::string wlVers = getWlVersion();

    table->addKey("tmvvers", tmvVers, "version of TMV code");
    table->addKey("wlvers", wlVers, "version of weak lensing code");

    std::string str;
    double dbl;
    int intgr;

    // if serun= is sent we'll put it in the header.  This allows us to 
    // associate some more, possibly complicated, metadata with this file
    if ( _params.keyExists("serun") ) {
        writeParamToTable(_params, table, "serun", str);
    }

    writeParamToTable(_params, table, "noise_method", str);
    writeParamToTable(_params, table, "dist_method", str);

    writeParamToTable(_params, table, "shear_aperture", dbl);
    writeParamToTable(_params, table, "shear_max_aperture", dbl);
    writeParamToTable(_params, table, "shear_gal_order", intgr);
    writeParamToTable(_params, table, "shear_gal_order2", intgr);
    writeParamToTable(_params, table, "shear_min_gal_size", dbl);
    writeParamToTable(_params, table, "shear_f_psf", dbl);

    // data
    // make vector copies for writing
    std::vector<double> x(nGals);
    std::vector<double> y(nGals);
    std::vector<double> ra(nGals);
    std::vector<double> dec(nGals);
    std::vector<double> shear1(nGals);
    std::vector<double> shear2(nGals);
    std::vector<double> cov00(nGals);
    std::vector<double> cov01(nGals);
    std::vector<double> cov11(nGals);

    for(int i=0;i<nGals;++i) 
    {
        x[i] = _pos[i].getX();
        y[i] = _pos[i].getY();
        // internally we use arcseconds
        ra[i] = _skyPos[i].getX()/3600;
        dec[i] = _skyPos[i].getY()/3600;
        shear1[i] = real(_shear[i]);
        shear2[i] = imag(_shear[i]);
        cov00[i] = _cov[i](0,0);
        cov01[i] = _cov[i](0,1);
        cov11[i] = _cov[i](1,1);
    }

    int startRow=1;

    table->column(colNames[0]).write(_id,startRow);
    table->column(colNames[1]).write(x,startRow);
    table->column(colNames[2]).write(y,startRow);
    table->column(colNames[3]).write(_sky,startRow);
    table->column(colNames[4]).write(_noise,startRow);
    table->column(colNames[5]).write(_flags,startRow);
    table->column(colNames[6]).write(ra,startRow);
    table->column(colNames[7]).write(dec,startRow);
    table->column(colNames[8]).write(shear1,startRow);
    table->column(colNames[9]).write(shear2,startRow);
    table->column(colNames[10]).write(_nu,startRow);
    table->column(colNames[11]).write(cov00,startRow);
    table->column(colNames[12]).write(cov01,startRow);
    table->column(colNames[13]).write(cov11,startRow);

    for (int i=0; i<nGals; ++i) {
        int row = i+1;
        long bOrder = _shape[i].getOrder();
        double bSigma = _shape[i].getSigma();

        table->column(colNames[14]).write(&bOrder,1,row);
        table->column(colNames[15]).write(&bSigma,1,row);
        double* cptr = (double *) _shape[i].vec().cptr();
        table->column(colNames[16]).write(cptr, nCoeff, 1, row);
    }
}

void ShearCatalog::writeAscii(std::string file, std::string delim) const
{
    Assert(_id.size() == size());
    Assert(_pos.size() == size());
    Assert(_sky.size() == size());
    Assert(_noise.size() == size());
    Assert(_flags.size() == size());
    Assert(_skyPos.size() == size());
    Assert(_shear.size() == size());
    Assert(_nu.size() == size());
    Assert(_cov.size() == size());
    Assert(_shape.size() == size());

    std::ofstream fout(file.c_str());
    if (!fout) {
        throw WriteException("Error opening shear file"+file);
    }

    Form hexForm; hexForm.hex().trail(0);

    const int nGals = size();
    for(int i=0;i<nGals;++i) {
        fout
            << _id[i] << delim
            << _pos[i].getX() << delim
            << _pos[i].getY() << delim
            << _sky[i] << delim
            << _noise[i] << delim
            << hexForm(_flags[i]) << delim
            << _skyPos[i].getX()/3600 << delim // internally we use arcsec
            << _skyPos[i].getY()/3600 << delim
            << real(_shear[i]) << delim
            << imag(_shear[i]) << delim
            << _nu[i] << delim
            << _cov[i](0,0) << delim
            << _cov[i](0,1) << delim
            << _cov[i](1,1) << delim
            << _shape[i].getOrder() << delim
            << _shape[i].getSigma();
        const int nCoeff = _shape[i].size();
        for(int j=0;j<nCoeff;++j)
            fout << delim << _shape[i](j);
        fout << std::endl;
    }
}

void ShearCatalog::write() const
{
    std::vector<std::string> fileList = makeMultiName(_params, "shear");  

    const int nFiles = fileList.size();
    for(int i=0; i<nFiles; ++i) {
        const std::string& file = fileList[i];
        dbg<<"Writing shear catalog to file: "<<file<<std::endl;

        bool isFitsIo = false;
        if (_params.keyExists("shear_io")) {
            std::vector<std::string> ioList = _params["shear_io"];
            Assert(ioList.size() == fileList.size());
            isFitsIo = (ioList[i] == "FITS");
        } else if (file.find("fits") != std::string::npos) {
            isFitsIo = true;
        }

        try {
            if (isFitsIo) {
                writeFits(file);
            } else {
                std::string delim = "  ";
                if (_params.keyExists("shear_delim")) {
                    std::vector<std::string> delimList = _params["shear_delim"];
                    Assert(delimList.size() == fileList.size());
                    delim = delimList[i];
                } else if (file.find("csv") != std::string::npos) {
                    delim = ",";
                }
                writeAscii(file,delim);
            }
        } catch (CCfits::FitsException& e) {
            throw WriteException(
                "Error writing to "+file+" -- caught error\n" + e.message());
        } catch (std::exception& e) {
            throw WriteException(
                "Error writing to "+file+" -- caught error\n" + e.what());
        } catch (...) {
            throw WriteException(
                "Error writing to "+file+" -- caught unknown error");
        }
    }
    dbg<<"Done Write ShearCatalog\n";
}

void ShearCatalog::readFits(std::string file)
{
    int hdu = _params.read("shear_hdu",2);

    dbg<<"Opening FITS file "<<file<<" at hdu "<<hdu<<std::endl;
    // true means read all as part of the construction
    CCfits::FITS fits(file, CCfits::Read, hdu-1, true);

    CCfits::ExtHDU& table=fits.extension(hdu-1);

    long nRows=table.rows();

    dbg<<"  nrows = "<<nRows<<std::endl;
    if (nRows <= 0) {
        throw ReadException(
            "ShearCatalog found to have 0 rows.  Must have > 0 rows.");
    }

    std::string idCol=_params.get("shear_id_col");
    std::string xCol=_params.get("shear_x_col");
    std::string yCol=_params.get("shear_y_col");
    std::string skyCol=_params.get("shear_sky_col");
    std::string noiseCol=_params.get("shear_noise_col");
    std::string flagsCol=_params.get("shear_flags_col");
    std::string raCol=_params.get("shear_ra_col");
    std::string decCol=_params.get("shear_dec_col");
    std::string shear1Col=_params.get("shear_shear1_col");
    std::string shear2Col=_params.get("shear_shear2_col");
    std::string nuCol=_params.get("shear_nu_col");
    std::string cov00Col=_params.get("shear_cov00_col");
    std::string cov01Col=_params.get("shear_cov01_col");
    std::string cov11Col=_params.get("shear_cov11_col");
    std::string orderCol=_params.get("shear_order_col");
    std::string sigmaCol=_params.get("shear_sigma_col");
    std::string coeffsCol=_params.get("shear_coeffs_col");

    long start=1;
    long end=nRows;

    dbg<<"Reading columns"<<std::endl;
    dbg<<"  "<<idCol<<std::endl;
    table.column(idCol).read(_id, start, end);

    dbg<<"  "<<xCol<<"  "<<yCol<<std::endl;
    _pos.resize(nRows);
    std::vector<double> x;
    std::vector<double> y;
    table.column(xCol).read(x, start, end);
    table.column(yCol).read(y, start, end);
    for(long i=0;i<nRows;++i) _pos[i] = Position(x[i],y[i]);

    dbg<<"  "<<skyCol<<std::endl;
    table.column(skyCol).read(_sky, start, end);

    dbg<<"  "<<noiseCol<<std::endl;
    table.column(noiseCol).read(_noise, start, end);

    dbg<<"  "<<flagsCol<<std::endl;
    table.column(flagsCol).read(_flags, start, end);

    dbg<<"  "<<raCol<<"  "<<decCol<<std::endl;
    _skyPos.resize(nRows);
    std::vector<double> ra;
    std::vector<double> dec;
    table.column(raCol).read(ra, start, end);
    table.column(decCol).read(dec, start, end);
    for(long i=0;i<nRows;++i) {
        _skyPos[i] = Position(ra[i],dec[i]);
    }

    dbg<<"  "<<shear1Col<<"  "<<shear2Col<<std::endl;
    _shear.resize(nRows);
    std::vector<double> shear1;
    std::vector<double> shear2;
    table.column(shear1Col).read(shear1, start, end);
    table.column(shear2Col).read(shear2, start, end);
    for(long i=0;i<nRows;++i) {
        _shear[i] = std::complex<double>(shear1[i],shear2[i]);
    }

    dbg<<"  "<<nuCol<<std::endl;
    table.column(nuCol).read(_nu, start, end);

    dbg<<"  "<<cov00Col<<"  "<<cov01Col<<"  "<<cov11Col<<std::endl;
    _cov.resize(nRows);
    std::vector<double> cov00;
    std::vector<double> cov01;
    std::vector<double> cov11;
    table.column(cov00Col).read(cov00, start, end);
    table.column(cov01Col).read(cov01, start, end);
    table.column(cov11Col).read(cov11, start, end);
    for(long i=0;i<nRows;++i) {
        _cov[i] = tmv::ListInit, cov00[i], cov01[i], cov01[i], cov11[i];
    }

    dbg<<"  "<<sigmaCol<<"  "<<orderCol<<std::endl;
    // temporary
    std::vector<double> sigma(nRows);
    std::vector<int> order(nRows);
    table.column(sigmaCol).read(sigma, start, end);
    table.column(orderCol).read(order, start, end);

    dbg<<"  "<<coeffsCol<<std::endl;
    _shape.reserve(nRows);
    for (int i=0; i<nRows; ++i) {
        xxdbg<<"i = "<<i<<std::endl;
        int row=i+1;

        _shape.push_back(BVec(order[i],sigma[i]));
        int nCoeff=_shape[i].size();

        std::valarray<double> coeffs(nCoeff);
        table.column(coeffsCol).read(coeffs, row);
        for(int j=0;j<nCoeff;++j) _shape[i](j) = coeffs[j];
        xxdbg<<"shape => "<<_shape[i].vec()<<std::endl;
    }
    dbg<<"Done ReadFits\n";
}

void ShearCatalog::readAscii(std::string file, std::string delim)
{
    std::ifstream fin(file.c_str());
    if (!fin) {
        throw ReadException("Error opening stars file"+file);
    }

    _id.clear(); _pos.clear(); _sky.clear(); _noise.clear(); _flags.clear();
    _skyPos.clear(); _shear.clear(); _nu.clear(); _cov.clear(); _shape.clear();

    if (delim == "  ") {
        ConvertibleString flag;
        long id1,bOrder;
        double x,y,sky1,noise1,ra,dec,s1,s2,nu1,c00,c01,c11,bSigma;
        while ( fin >> id1 >> x >> y >> sky1 >> noise1 >> 
                flag >> ra >> dec >> s1 >> s2 >> nu1 >>
                c00 >> c01 >> c11 >> bOrder >> bSigma ) {
            _id.push_back(id1);
            _pos.push_back(Position(x,y));
            _sky.push_back(sky1);
            _noise.push_back(noise1);
            _flags.push_back(flag);
            _skyPos.push_back(Position(ra,dec));
            _shear.push_back(std::complex<double>(s1,s2));
            _nu.push_back(nu1);
            _cov.push_back(tmv::SmallMatrix<double,2,2>());
            _cov.back() = tmv::ListInit, c00, c01, c01, c11;
            _shape.push_back(BVec(bOrder,bSigma));
            const int nCoeff = _shape.back().size();
            for(int j=0; j<nCoeff; ++j)
                fin >> _shape.back()(j);
        }
    } else {
        if (delim.size() > 1) {
            // getline only works with single character delimiters.
            // Since I don't really expect a multicharacter delimiter to
            // be used ever, I'm just going to throw an exception here 
            // if we do need it, and I can write the workaround then.
            throw ParameterException(
                "ReadAscii delimiter must be a single character");
        }
        char d = delim[0];
        long bOrder;
        double x,y,ra,dec,s1,s2,bSigma,c00,c01,c11;
        ConvertibleString temp;
        while (getline(fin,temp,d)) {
            _id.push_back(temp);
            getline(fin,temp,d); x = temp;
            getline(fin,temp,d); y = temp;
            _pos.push_back(Position(x,y));
            getline(fin,temp,d); _sky.push_back(temp);
            getline(fin,temp,d); _noise.push_back(temp);
            getline(fin,temp,d); _flags.push_back(temp);
            getline(fin,temp,d); ra = temp;
            getline(fin,temp,d); dec = temp;
            _skyPos.push_back(Position(ra,dec));
            getline(fin,temp,d); s1 = temp;
            getline(fin,temp,d); s2 = temp;
            _shear.push_back(std::complex<double>(s1,s2));
            getline(fin,temp,d); _nu.push_back(temp);
            getline(fin,temp,d); c00 = temp;
            getline(fin,temp,d); c01 = temp;
            getline(fin,temp,d); c11 = temp;
            _cov.push_back(tmv::SmallMatrix<double,2,2>());
            _cov.back() = tmv::ListInit, c00, c01, c01, c11;
            getline(fin,temp,d); bOrder = temp;
            getline(fin,temp,d); bSigma = temp;
            _shape.push_back(BVec(bOrder,bSigma));
            const int nCoeff = _shape.back().size();
            for(int j=0;j<nCoeff-1;++j) {
                getline(fin,temp,d); _shape.back()(j) = temp;
            }
            // Last one doesn't have a following delimiter
            getline(fin,temp); _shape.back()(nCoeff-1) = temp;
        }
    }
}

void ShearCatalog::read()
{
    std::string file = makeName(_params,"shear",false,true);
    // false,true = input_prefix=false, mustexist=true.
    // It is an input here, but it is in the output_prefix directory.
    dbg<< "Reading Shear cat from file: " << file << std::endl;

    bool isFitsIo = false;
    if (_params.keyExists("shear_io")) 
        isFitsIo = (_params["shear_io"] == "FITS");
    else if (file.find("fits") != std::string::npos) 
        isFitsIo = true;

    if (!doesFileExist(file)) {
        throw FileNotFoundException(file);
    }
    try {
        if (isFitsIo) {
            readFits(file);
        } else {
            std::string delim = "  ";
            if (_params.keyExists("shear_delim")) {
                delim = _params["shear_delim"];
            } else if (file.find("csv") != std::string::npos) {
                delim = ",";
            }
            readAscii(file,delim);
        }
    } catch (CCfits::FitsException& e) {
        throw ReadException(
            "Error reading from "+file+" -- caught error\n" + e.message());
    } catch (std::exception& e) {
        throw ReadException(
            "Error reading from "+file+" -- caught error\n" + e.what());
    } catch (...) {
        throw ReadException(
            "Error reading from "+file+" -- caught unknown error");
    }

    // Update the bounds:
    const int nGals = size();
    Assert(_pos.size() == size());
    Assert(_skyPos.size() == size());
    for(int i=0;i<nGals;++i) _bounds += _pos[i];

    // This corrects for the old value of the skypos values.
    if (_params.read("shear_dc4_input_format",false)) {
        for(int i=0;i<nGals;++i)
            _skyPos[i] *= 3600.; // deg -> arcsec
    }

    // Calculate the skyBounds using only the good objects:
    Bounds skyBounds2; // In degrees, rather than arcsec
    for(int i=0;i<nGals;++i) if (!_flags[i]) {
        _skyBounds += _skyPos[i];
        Position temp = _skyPos[i];
        temp /= 3600.;
        skyBounds2 += temp;
    }
    dbg<<"skyBounds = "<<_skyBounds<<std::endl;
    dbg<<"in degrees: "<<skyBounds2<<std::endl;

    dbg<<"Done Read ShearCatalog\n";
}

