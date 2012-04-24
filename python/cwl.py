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


class WLObject(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, WLObject, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, WLObject, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _cwl.new_WLObject(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _cwl.delete_WLObject
    __del__ = lambda self : None;
    def get_sigma0(self): return _cwl.WLObject_get_sigma0(self)
    def get_flags(self): return _cwl.WLObject_get_flags(self)
WLObject_swigregister = _cwl.WLObject_swigregister
WLObject_swigregister(WLObject)

class WLShear(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, WLShear, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, WLShear, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _cwl.new_WLShear(*args)
        try: self.this.append(this)
        except: self.this = this
    def get_flags(self): return _cwl.WLShear_get_flags(self)
    def get_nu(self): return _cwl.WLShear_get_nu(self)
    def get_cov11(self): return _cwl.WLShear_get_cov11(self)
    def get_cov12(self): return _cwl.WLShear_get_cov12(self)
    def get_cov22(self): return _cwl.WLShear_get_cov22(self)
    def get_shear1(self): return _cwl.WLShear_get_shear1(self)
    def get_shear2(self): return _cwl.WLShear_get_shear2(self)
    def get_prepsf_sigma(self): return _cwl.WLShear_get_prepsf_sigma(self)
    __swig_destroy__ = _cwl.delete_WLShear
    __del__ = lambda self : None;
WLShear_swigregister = _cwl.WLShear_swigregister
WLShear_swigregister(WLShear)



