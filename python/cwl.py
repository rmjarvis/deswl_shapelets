# This file was automatically generated by SWIG (http://www.swig.org).
# Version 1.3.40
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.
# This file is compatible with both classic and new-style classes.

from sys import version_info
if version_info >= (2,6,0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_cwl', [dirname(__file__)])
        except ImportError:
            import _cwl
            return _cwl
        if fp is not None:
            try:
                _mod = imp.load_module('_cwl', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _cwl = swig_import_helper()
    del swig_import_helper
else:
    import _cwl
del version_info
try:
    _swig_property = property
except NameError:
    pass # Python < 2.2 doesn't have 'property'.
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError(name)

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0


class CWL(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, CWL, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, CWL, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _cwl.new_CWL(*args)
        try: self.this.append(this)
        except: self.this = this
    def load_config(self, *args): return _cwl.CWL_load_config(self, *args)
    def load_fitsparams(self): return _cwl.CWL_load_fitsparams(self)
    def set_param(self, *args): return _cwl.CWL_set_param(self, *args)
    def set_log(self, *args): return _cwl.CWL_set_log(self, *args)
    def load_config_images_catalog(self, *args): return _cwl.CWL_load_config_images_catalog(self, *args)
    def load_images(self, *args): return _cwl.CWL_load_images(self, *args)
    def load_catalog(self, *args): return _cwl.CWL_load_catalog(self, *args)
    def load_trans(self, *args): return _cwl.CWL_load_trans(self, *args)
    def write_starcat(self, *args): return _cwl.CWL_write_starcat(self, *args)
    def load_starcat(self, *args): return _cwl.CWL_load_starcat(self, *args)
    def write_psfcat(self, *args): return _cwl.CWL_write_psfcat(self, *args)
    def load_psfcat(self, *args): return _cwl.CWL_load_psfcat(self, *args)
    def write_fitpsf(self, *args): return _cwl.CWL_write_fitpsf(self, *args)
    def load_fitpsf(self, *args): return _cwl.CWL_load_fitpsf(self, *args)
    def write_shearcat(self, *args): return _cwl.CWL_write_shearcat(self, *args)
    def load_shearcat(self, *args): return _cwl.CWL_load_shearcat(self, *args)
    def split_starcat(self, *args): return _cwl.CWL_split_starcat(self, *args)
    def find_stars(self, *args): return _cwl.CWL_find_stars(self, *args)
    def measure_psf(self, *args): return _cwl.CWL_measure_psf(self, *args)
    def measure_shear(self, *args): return _cwl.CWL_measure_shear(self, *args)
    def print_config(self): return _cwl.CWL_print_config(self)
    def set_verbosity(self, *args): return _cwl.CWL_set_verbosity(self, *args)
    __swig_destroy__ = _cwl.delete_CWL
    __del__ = lambda self : None;
CWL_swigregister = _cwl.CWL_swigregister
CWL_swigregister(CWL)


