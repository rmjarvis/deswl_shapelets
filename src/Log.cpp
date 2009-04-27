
#include "Log.h"
#include <iostream>
#include <stdexcept>
#include "Params.h"

Log::Log(std::string _logfile, std::string _fits_file, bool desqa) :
  exitcode(SUCCESS), logout(0), fits_file(_fits_file)
{
  if (desqa) {
    if (_logfile == "") {
      logout = &std::cout;
    } else {
      logout = new std::ofstream(_logfile.c_str(),std::ios_base::app);
      if (!logout) {
	throw std::runtime_error(
	    std::string("Error: Unable to open logfile ") + _logfile
	    + " for append output");
      }
    }
  }
}

Log::~Log() 
{
  // I thought this would work, but it doesn't. The reason is a bit subtle.  
  // Basically, the destructors get called in order from most derived 
  // up to the base.
  // So by this point, the derived portions have already been deleted,
  // so the derived implementations of these functions are already gone.
  // The solution is to put this same thing in each derived destructor.
#if 0
  this->WriteLog();
  this->WriteLogToFitsHeader();
#endif
  NoWriteLog(); // deletes logout
}

void Log::NoWriteLog() 
{ 
  if (logout && logout != &std::cout) {
    delete logout;
  }
  logout = 0;
}

ShearLog::ShearLog(std::string _logfile, std::string _fits_file, bool desqa) :
  Log(_logfile,_fits_file,desqa),
  ngals(0), ngoodin(0), ngood(0), nf_range1(0), nf_range2(0), nf_small(0),
  nf_tmverror(0), nf_othererror(0), ns_native(0), nf_native(0),
  ns_mu(0), nf_mu(0), ns_gamma(0), nf_gamma(0)
{}

ShearLog::~ShearLog() {
  this->WriteLog();
  this->WriteLogToFitsHeader();
}

void ShearLog::WriteLogToFitsHeader() const
{

  if ("" == fits_file) return;

  try {
    //std::cout<<"Writing log to Shear file: "<<fits_file<<std::endl;
    bool create=false;
    FitsFile fits(fits_file.c_str(), READWRITE, create);
    fits.GotoHDU(2);

    // use msexit to differentiate from exit codes for other codes
	
	/*
    fits.WriteKey("msexit", XLONG, &exitcode, "Exit code for MeasureShear");
    fits.WriteKey("msnobj", XLONG, &ngals, 
	"# of objects processed by MeasureShear");
    fits.WriteKey("ms_ngoodin", XLONG, &ngoodin,
	"# of objects with no input error flags");
    fits.WriteKey("ms_ngood", XLONG, &ngood,
	"# of measurements with no error flags");
    fits.WriteKey("ns_gamma", XLONG, &ns_gamma, 
	"# of successful shear measurements");
    fits.WriteKey("ns_nativ", XLONG, &ns_native, 
	"# of successful native shapeles measurements");

    fits.WriteKey("nf_rnge1", XLONG, &nf_range1, 
	"# of MeasureShear failures range1");
    fits.WriteKey("nf_rnge2", XLONG, &nf_range2, 
	"# of MeasureShear failures range2");

    fits.WriteKey("nf_nativ", XLONG, &nf_native, 
	"# of MeasureShear failures native calculations");
    fits.WriteKey("nf_small", XLONG, &nf_small, 
	"# of MeasureShear failures too small");

    // prefix ms to make unique
    fits.WriteKey("nf_mstmv", XLONG, &nf_tmverror, 
	"# of MeasureShear failures TMV errors");
    fits.WriteKey("nf_msoth", XLONG, &nf_othererror, 
	"# of MeasureShear failures unclassified errors");

    fits.WriteKey("nf_mu", XLONG, &nf_mu, 
	"# of MeasureShear failures calculating mu");
    fits.WriteKey("nf_gamma", XLONG, &nf_gamma, 
	"# of MeasureShear failures calculating shear");
	*/

	// the enumerated type ExitCode sometimes defaults to long
	int exit=exitcode;
    fits.WriteKey("msexit", XINT, &exit, "Exit code for MeasureShear");
    fits.WriteKey("msnobj", XINT, &ngals, 
	"# of objects processed by MeasureShear");
    fits.WriteKey("ms_ngoodin", XINT, &ngoodin,
	"# of objects with no input error flags");
    fits.WriteKey("ms_ngood", XINT, &ngood,
	"# of measurements with no error flags");
    fits.WriteKey("ns_gamma", XINT, &ns_gamma, 
	"# of successful shear measurements");
    fits.WriteKey("ns_nativ", XINT, &ns_native, 
	"# of successful native shapeles measurements");

    fits.WriteKey("nf_rnge1", XINT, &nf_range1, 
	"# of MeasureShear failures range1");
    fits.WriteKey("nf_rnge2", XINT, &nf_range2, 
	"# of MeasureShear failures range2");

    fits.WriteKey("nf_nativ", XINT, &nf_native, 
	"# of MeasureShear failures native calculations");
    fits.WriteKey("nf_small", XINT, &nf_small, 
	"# of MeasureShear failures too small");

    // prefix ms to make unique
    fits.WriteKey("nf_mstmv", XINT, &nf_tmverror, 
	"# of MeasureShear failures TMV errors");
    fits.WriteKey("nf_msoth", XINT, &nf_othererror, 
	"# of MeasureShear failures unclassified errors");

    fits.WriteKey("nf_mu", XINT, &nf_mu, 
	"# of MeasureShear failures calculating mu");
    fits.WriteKey("nf_gamma", XINT, &nf_gamma, 
	"# of MeasureShear failures calculating shear");


  } 
  catch(...) 
  { if (exitcode == 0) throw; }

}

inline std::string StatusText1(ExitCode exitcode)
{ 
  return std::string("STATUS") +
    char('0'+Status(exitcode)) +
    std::string("BEG"); 
}
inline std::string StatusText2(ExitCode exitcode)
{
  return std::string("STATUS") +
    char('0'+Status(exitcode)) +
    std::string("END");
}

void ShearLog::WriteLog() const
{
  if (logout) {
    // Emit logging information
    if (exitcode) {
      *logout << 
	StatusText1(exitcode)<<" "<<
	Text(exitcode)<<" "<<
	extraexitinfo<<" ";
      if (fits_file != "")
	*logout << " (Name="<<fits_file<<") ";
      *logout<< StatusText2(exitcode)<<std::endl;
    } else {
      std::string name = "measureshear";
      if (fits_file != "") name = fits_file;
      *logout << 
	"QA2BEG "<<
	"Name="<<name <<" & "<<
	"ngals="<<ngals <<" & "<<
	"ngoodin="<<ngoodin <<" & "<<
	"ngood="<<ngood <<" & "<<
	"ns_gamma="<<ns_gamma <<" & "<<
	"ns_native="<<ns_native <<" & "<<
	"nf_range1="<<nf_range1 <<" & "<<
	"nf_range2="<<nf_range2 <<" & "<<
	"nf_native="<<nf_native <<" & "<<
	"nf_small="<<nf_small <<" & "<<
	"nf_tmverror="<<nf_tmverror <<" & "<<
	"nf_othererror="<<nf_othererror <<" & "<<
	"nf_mu="<<nf_mu <<" & "<<
	"nf_gamma="<<nf_gamma <<
	" QA2END"<<std::endl;
    }
  }
}

void ShearLog::Write(std::ostream& os) const
{
  os<<"N Total objects = "<<ngals<<std::endl;
  os<<"N objects with no input error flags = "<<ngoodin<<std::endl;
  os<<"N objects with no measurement error flags = "<<ngood<<std::endl;

  os<<"N_Rejected for range error - distortion = "<<nf_range1<<std::endl;
  os<<"N_Rejected for range error - psf interp = "<<nf_range2<<std::endl;

  os<<"Native fits:\n";
  os<<"  N_Success = "<<ns_native<<std::endl;
  os<<"  N_Fail = "<<nf_native<<std::endl;

  os<<"N_Rejected for being too small = "<<nf_small<<std::endl;

  os<<"Mu fits:\n";
  os<<"  N_Success = "<<ns_mu<<std::endl;
  os<<"  N_Fail = "<<nf_mu<<std::endl;

  os<<"Gamma fits:\n";
  os<<"  N_Success = "<<ns_gamma<<std::endl;
  os<<"  N_Fail = "<<nf_gamma<<std::endl;

  os<<"N_Error: TMV Error caught = "<<nf_tmverror<<std::endl;
  os<<"N_Error: Other caught = "<<nf_othererror<<std::endl;
}

PSFLog::PSFLog(std::string _logfile, std::string _fits_file, bool desqa) :
  Log(_logfile,_fits_file,desqa),
  nstars(0), ngoodin(0), ngood(0), nf_range(0),
  nf_tmverror(0), nf_othererror(0), ns_psf(0), nf_psf(0) {}

PSFLog::~PSFLog() {
  this->WriteLog();
  this->WriteLogToFitsHeader();
}

void PSFLog::WriteLogToFitsHeader() const
{
  if ("" == fits_file) return;

  try {
    bool create=false;
    //std::cout<<"Writing log to PSF file: "<<fits_file<<std::endl;
    FitsFile fits(fits_file.c_str(), READWRITE, create);
    fits.GotoHDU(2);

    // use fsexit to differentiate from exit codes for other codes
	
	/*
    fits.WriteKey("mpexit", XLONG, &exitcode, "Exit code for MeasurePSF");
    fits.WriteKey("mpnstars", XLONG, &nstars, 
	"# of PSF star candidates used");
    fits.WriteKey("mpgoodin", XLONG, &ngoodin,
	"# of objects with no input error flags");
    fits.WriteKey("mp_ngood", XLONG, &ngood,
	"# of measurements with no error flags");
    fits.WriteKey("ns_psf", XLONG, &ns_psf, 
	"# of successful PSF decompositions");
    fits.WriteKey("nf_range", XLONG, &nf_range, 
	"# PSF failures due to range error");
    fits.WriteKey("nf_mptmv", XLONG, &nf_tmverror, 
	"# PSF failures due to TMV errors");
    fits.WriteKey("nf_mpoth", XLONG, &nf_othererror, 
	"# PSF failures due to unclassified errors");
    fits.WriteKey("nf_psf", XLONG, &nf_psf, 
	"# PSF failures");
	*/

	
	// the enumerated type ExitCode sometimes defaults to long
	int exit=exitcode;
    fits.WriteKey("mpexit", XINT, &exit, "Exit code for MeasurePSF");
    fits.WriteKey("mpnstars", XINT, &nstars, 
	"# of PSF star candidates used");
    fits.WriteKey("mpgoodin", XINT, &ngoodin,
	"# of objects with no input error flags");
    fits.WriteKey("mp_ngood", XINT, &ngood,
	"# of measurements with no error flags");
    fits.WriteKey("ns_psf", XINT, &ns_psf, 
	"# of successful PSF decompositions");
    fits.WriteKey("nf_range", XINT, &nf_range, 
	"# PSF failures due to range error");
    fits.WriteKey("nf_mptmv", XINT, &nf_tmverror, 
	"# PSF failures due to TMV errors");
    fits.WriteKey("nf_mpoth", XINT, &nf_othererror, 
	"# PSF failures due to unclassified errors");
    fits.WriteKey("nf_psf", XINT, &nf_psf, 
	"# PSF failures");


  }
  catch (...)
  { if (exitcode == 0) throw; }
}

void PSFLog::WriteLog() const
{
  if (logout) {
    // Emit logging information
    if (exitcode) {
      *logout << 
	StatusText1(exitcode)<<" "<<
	Text(exitcode)<<" "<<
	extraexitinfo<<" ";
      if (fits_file != "")
	*logout << " (Name="<<fits_file<<") ";
      *logout<< StatusText2(exitcode)<<std::endl;
    } else {
      std::string name = "measurepsf";
      if (fits_file != "") name = fits_file;
      *logout << 
	"QA2BEG "<<
	"Name="<<name <<" & "<<
	"nstars="<<nstars <<" & "<<
	"ngoodin="<<ngoodin <<" & "<<
	"ngood="<<ngood <<" & "<<
	"ns_psf="<<ns_psf <<" & "<<
	"nf_range="<<nf_range <<" & "<<
	"nf_tmverror="<<nf_tmverror <<" & "<<
	"nf_othererror="<<nf_othererror <<" & "<<
	"nf_psf="<<nf_psf <<
	" QA2END"<<std::endl;
    }
  }
}

void PSFLog::Write(std::ostream& os) const
{
  os<<"N Total stars = "<<nstars<<std::endl;
  os<<"N stars with no input error flags = "<<ngoodin<<std::endl;
  os<<"N stars with no measurement error flags = "<<ngood<<std::endl;

  os<<"N_Rejected for range error - distortion = "<<nf_range<<std::endl;

  os<<"PSF measurement:\n";
  os<<"  N_Success = "<<ns_psf<<std::endl;
  os<<"  N_Fail = "<<nf_psf<<std::endl;

  os<<"N_Error: TMV Error caught = "<<nf_tmverror<<std::endl;
  os<<"N_Error: Other caught = "<<nf_othererror<<std::endl;
}


FindStarsLog::FindStarsLog(std::string _logfile, std::string _fits_file, 
    bool desqa) :
  Log(_logfile,_fits_file,desqa),
  ntot(0), nr_flag(0), nr_mag(0), nr_size(0),
  nobj(0), nallstars(0), nstars(0) {}

FindStarsLog::~FindStarsLog() {
  this->WriteLog();
  this->WriteLogToFitsHeader();
}

void FindStarsLog::WriteLogToFitsHeader() const
{
  if ("" == fits_file) return;

  try {
    bool create=false;
    //std::cout<<"Writing log to FindStars file: "<<fits_file<<std::endl;
    FitsFile fits(fits_file.c_str(), READWRITE, create);
    fits.GotoHDU(2);

    // not efficient since entire file must be resized and all data moved
    // forward each time

    // copying out to avoid g++ errors due to const incorrectness
	
	/*
    fits.WriteKey("fsexit", XLONG, &exitcode, "Exit code for FindStars");
    fits.WriteKey("fsntot", XLONG, &ntot, 
	"# of total objects processed in FindStars");
    fits.WriteKey("fsnrflag", XLONG, &nr_flag,
	"# of objects rejected because of input flag");
    fits.WriteKey("fsnrmag", XLONG, &nr_mag,
	"# of objects rejected because of mag limits");
    fits.WriteKey("fsnrsize", XLONG, &nr_size,
	"# of objects rejected because of size limits");
    fits.WriteKey("fsnobj", XLONG, &nobj, 
	"# of objects processed in FindStars");
    fits.WriteKey("fsnall", XLONG, &nallstars,
	"# of total stars found by FindStars");
    fits.WriteKey("fsnstars", XLONG, &nstars, 
	"# of good stars found by FindStars");
	*/
	
	// the enumerated type ExitCode sometimes defaults to long
	int exit=exitcode;
    fits.WriteKey("fsexit", XINT, &exit, "Exit code for FindStars");
    fits.WriteKey("fsntot", XINT, &ntot, 
	"# of total objects processed in FindStars");
    fits.WriteKey("fsnrflag", XINT, &nr_flag,
	"# of objects rejected because of input flag");
    fits.WriteKey("fsnrmag", XINT, &nr_mag,
	"# of objects rejected because of mag limits");
    fits.WriteKey("fsnrsize", XINT, &nr_size,
	"# of objects rejected because of size limits");
    fits.WriteKey("fsnobj", XINT, &nobj, 
	"# of objects processed in FindStars");
    fits.WriteKey("fsnall", XINT, &nallstars,
	"# of total stars found by FindStars");
    fits.WriteKey("fsnstars", XINT, &nstars, 
	"# of good stars found by FindStars");


  }
  catch(...)
  { if (exitcode == 0) throw; }
}

void FindStarsLog::WriteLog() const
{
  if (logout) {
    // Emit logging information
    if (exitcode) {
      *logout << 
	StatusText1(exitcode)<<" "<<
	Text(exitcode)<<" "<<
	extraexitinfo<<" ";
      if (fits_file != "")
	*logout << " (Name="<<fits_file<<") ";
      *logout<< StatusText2(exitcode)<<std::endl;
    } else {
      std::string name = "findstars";
      if (fits_file != "") name = fits_file;
      *logout << 
	"QA2BEG "<<
	"Name="<<name <<" & "<<
	"ntot="<<ntot <<" & "<<
	"nr_flag="<<nr_flag <<" & "<<
	"nr_mag="<<nr_mag <<" & "<<
	"nr_size="<<nr_size <<" & "<<
	"nobj="<<nobj <<" & "<<
	"nallstars="<<nallstars <<" & "<<
	"nstars="<<nstars <<
	" QA2END"<<std::endl;
    }
  }
}

void FindStarsLog::Write(std::ostream& os) const
{
  os<<"N total input objects = "<<ntot<<std::endl;
  os<<"N rejected because of input flag = "<<nr_flag<<std::endl;
  os<<"N rejected because of mag limits = "<<nr_mag<<std::endl;
  os<<"N rejected because of size limits = "<<nr_size<<std::endl;
  os<<"N objects processed in FindStars = "<<nobj<<std::endl;
  os<<"N total stars found by FindStars = "<<nallstars<<std::endl;
  os<<"N good stars found by FindStars = "<<nstars<<std::endl;
}


