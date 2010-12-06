
char fitsparams_config[] = "#\n# This files contains the paramters that define the names of the \n# keys and comment fields in the FITS files.  \n#\n##############################################################################\n#\n# Header keys that are written into all FITS files\n#\nversion_hname = wlvers\nversion_comment = version of weak lensing code\nnoise_method_hname = noismeth\nnoise_method_comment = Noise method, typically WEIGHT_IMAGE\ndist_method_hname = distmeth\ndist_method_comment = Method for dealing with distortions\n\n# record the maximum memory we calculated\nmaxmem_hname = maxmem\nmaxmem_comment = Approximate maximum memory used in Mb.  \n\n#\n##############################################################################\n#\n# StarCatalog:\n#\n# If stars_io == FITS (or stars_ext contains 'fits'), then these define\n# the column names for each catalog item that we need:\n#\nstars_id_col = id\nstars_x_col = x\nstars_y_col = y\nstars_sky_col = sky\nstars_noise_col = noise\nstars_flags_col = size_flags\nstars_mag_col = mag\nstars_objsize_col = sigma0\nstars_isastar_col = star_flag\n#\n# If stars_io == TEXT (or stars_ext does not contain 'fits'), then the\n# values are written as 7 columns in the above order.\n#\n# Other header fields:\n#\nstars_minsize_hname = minsize  \nstars_minsize_comment = Minimum size to consider for PSF stars\nstars_maxsize_hname = maxsize\nstars_maxsize_comment = Maximum size to consider for PSF stars\nstars_logsize_hname = logsize\nstars_logsize_comment = Are size values already log(size)?\nstars_minmag_hname = minmag\nstars_minmag_comment = The minimum mag to consider for PSF stars\nstars_maxmag_hname = maxmag\nstars_maxmag_comment = The maximum mag to consider for PSF stars\nstars_maxoutmag_hname = maxomag\nstars_maxoutmag_comment = The maximum mag to output for PSF stars\nstars_ndivx_hname = ndivx\nstars_ndivx_comment = Number of subdivisions in x direction\nstars_ndivy_hname = ndivy\nstars_ndivy_comment = Number of subdivisions in y direction\nstars_startn1_hname = startn1\nstars_startn1_comment = Fraction of objects to start with per subdivision\nstars_starfrac_hname = starfrac\nstars_starfrac_comment = What fraction of startn1 are probably stars\nstars_magstep1_hname = magstep1\nstars_magstep1_comment = Step size in magnitudes\nstars_miniter1_hname = miniter1\nstars_miniter1_comment = Min times to step up in mag cutoff\nstars_reject1_hname = reject1\nstars_reject1_comment = N sigma for rejection\nstars_binsize1_hname = binsize1\nstars_binsize1_comment = Bin size for histogram\nstars_maxratio1_hname = maxrat1\nstars_maxratio1_comment = Max ratio of valley count to peak count\nstars_okvalcount_hname = okvalcnt\nstars_okvalcount_comment = valley cnt<=okvalcnt then OK even if ratio>maxratio1\nstars_maxrms_hname = maxrms\nstars_maxrms_comment = Max rms of first linear fit\nstars_starsperbin_hname = nperbin\nstars_starsperbin_comment = Minimum stars to expect in each bin\nstars_fitorder_hname = fsorder\nstars_fitorder_comment = Order of fitted function size(x,y)\nstars_fitsigclip_hname = sigclip\nstars_fitsigclip_comment = Sigma clip when fitting size(x,y)\nstars_startn2_hname = startn2\nstars_startn2_comment = How many objects to start with \nstars_magstep2_hname = magstep2\nstars_magstep2_comment = Step size in magnitudes \nstars_miniter2_hname = miniter2\nstars_miniter2_comment = Min times to step up the magnitude cutoff\nstars_minbinsize_hname = minbinsz\nstars_minbinsize_comment = Min width of histogram bins\nstars_reject2_hname = reject2\nstars_reject2_comment = N sigma for rejection \nstars_purityratio_hname = purerat\nstars_purityratio_comment = Max ratio of valley count to peak count\nstars_maxrefititer_hname = maxrefit\nstars_maxrefititer_comment = Max number of times to refit size(x,y) \n#\n##############################################################################\n#\n# PSFCatalog\n#\n# If psf_io == FITS (or psf_ext contains 'fits'), then these define\n# the column names for each catalog item that we need:\n#\npsf_id_col = id\npsf_x_col = x\npsf_y_col = y\npsf_sky_col = sky\npsf_noise_col = noise\npsf_flags_col = psf_flags\npsf_nu_col = psf_signal_to_noise \npsf_order_col = psf_order\npsf_sigma_col = sigma_p\npsf_coeffs_col = shapelets\n#\n# If psf_io == TEXT (or psf_ext does not contain 'fits'), then the\n# values are written as 6 columns in the above order.\n#\n# Other header fields:\n#\npsf_aperture_hname = psfap\npsf_aperture_comment = How many sigma to use for aperture\npsf_order_hname = psford\npsf_order_comment = Order of shapelet expansion for PSF stars.\npsf_seeing_est_hname = see_est\npsf_seeing_est_comment = An estimte of seeing, good to a factor of two\n#\n##############################################################################\n#\n# FittedPSF \n#\n# If fitpsf_io == FITS (or fitpsf_ext contains 'fits'), then these define\n# the column names for each catalog item that we need:\n#\nfitpsf_psf_order_col = psf_order\nfitpsf_sigma_col = sigma\nfitpsf_fit_order_col = fit_order\nfitpsf_npca_col = npca\nfitpsf_xmin_col = xmin\nfitpsf_xmax_col = xmax\nfitpsf_ymin_col = ymin\nfitpsf_ymax_col = ymax\nfitpsf_ave_psf_col = ave_psf\nfitpsf_rot_matrix_col = rot_matrix\nfitpsf_interp_matrix_col = interp_matrix\n#\n# If fitpsf_io == TEXT (or fitpsf_ext does not contain 'fits'), then\n# the code uses a standard text I/O for the FittedPSF class.\n# Also, any fitpsf_delim specifier is ignored.\n#\n# Other header fields:\n#\nfitpsf_order_hname = fpsford\nfitpsf_order_comment = Order in x,y of the fit across chip\nfitpsf_pca_thresh_hname = pcthresh\nfitpsf_pca_thresh_comment = Use PCA components with absval >= thresh*S(0)\n#\n##############################################################################\n#\n# ShearCatalog\n#\n# If shear_io == FITS (or shear_ext contains 'fits'), then these define\n# the column names for each catalog item that we need:\n#\nshear_id_col = id\nshear_x_col = x\nshear_y_col = y\nshear_sky_col = sky\nshear_noise_col = noise\nshear_flags_col = shear_flags\nshear_ra_col = ra\nshear_dec_col = dec\nshear_shear1_col = shear1\nshear_shear2_col = shear2\nshear_nu_col = shear_signal_to_noise\nshear_cov00_col = shear_cov00\nshear_cov01_col = shear_cov01\nshear_cov11_col = shear_cov11\nshear_order_col = gal_order\nshear_sigma_col = shapelet_sigma\nshear_coeffs_col = shapelets_prepsf\nshear_psforder_col = interp_psf_order\nshear_psfsigma_col = interp_psf_sigma\nshear_psfcoeffs_col = interp_psf_coeffs\n#\n# If shear_io == TEXT (or shear_ext does not contain 'fits'), then the\n# values are written as 12 columns in the above order followed by\n# the coefficients (there are (order+1)*(order+2)/2 of these).\n# \n# Other header fields:\n#\nshear_aperture_hname = shap\nshear_aperture_comment = How many sigma to use for aperture\nshear_max_aperture_hname = shmaxap\nshear_max_aperture_comment = Maximum size of the aperture in arcsec\nshear_gal_order_hname = galord\nshear_gal_order_comment = Order of pre-psf shapelet decomp\nshear_gal_order2_hname = galord2\nshear_gal_order2_comment = Order of shapelets for shear measurement\nshear_min_gal_size_hname = mingalsz\nshear_min_gal_size_comment = min object size in multiples of PSF size\nshear_f_psf_hname = shfpsf\nshear_f_psf_comment = sigma^2_obs = sigma^2_gal + f_psf sigma^2_psf\n#\n\n\n# parameters for the multishear output catalog\n# these are all the same as the shear catalog\nmultishear_id_col = id\nmultishear_x_col = x\nmultishear_y_col = y\nmultishear_sky_col = sky\nmultishear_noise_col = noise\nmultishear_flags_col = shear_flags\nmultishear_ra_col = ra\nmultishear_dec_col = dec\nmultishear_shear1_col = shear1\nmultishear_shear2_col = shear2\nmultishear_nu_col = shear_signal_to_noise\nmultishear_cov00_col = shear_cov00\nmultishear_cov01_col = shear_cov01\nmultishear_cov11_col = shear_cov11\nmultishear_order_col = gal_order\nmultishear_sigma_col = shapelet_sigma\nmultishear_coeffs_col = shapelets_prepsf\n\nmultishear_nimages_found_col = nimages_found\nmultishear_nimages_gotpix_col = nimages_gotpix\nmultishear_input_flags_col = input_flags\n\n# The multi-epoch shear run identifier.  Note if this keyword is not sent\n# through the command line it will not be written to the header\nwlmerun_hname = wlmerun\nwlmerun_comment = multi-epoch shear run identifier\n\n# The multi-epoch shear run identifier.  Note if this keyword is not sent\n# through the command line it will not be written to the header\nwlserun_hname = wlserun\nwlserun_comment = single-epoch shear run identifier\n\n";
    
unsigned int fitsparams_config_len = 8278;
