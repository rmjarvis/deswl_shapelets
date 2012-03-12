import os
from sys import stderr
import esutil as eu
import deswl

class GenericProcessor(dict):
    def __init__(self, config):
        """
        required keys in config
            - 'run' The run id, just for identification
            - 'input_files', which is itself a dictionary.  The files will be
            checked to see if any files are in hdfs and if so these will be staged
            out.

            - 'output_files', a dictionary.  HDFS files will be first written
            locally and then pushed in.
                - This dict must contain an entry called 'stat' which 
                should be a .yaml file or .json file. It will contain the
                exit_status for the process and other metadata.  This
                file is written by THIS PROGRAM and should not be written
                by the called external program.

            - 'command'.  This can refer to objects in the config file,
            including sub-objects of input_files and output_files, which will
            be pulled into the main name space.  Reference should be made using
            formats like %(name)s %(name)0.2f etc.

            It is important not to put hdfs file names into the command
            directly, as these should instead use the *local* versions
            of the file names.  The correct version will be used during
            interpolation of variables from the config into the command

        Some optional fields

            - 'timeout': a timeout for the code in seconds.  Default two hours,
            2*60*60

        """
        req_fields=['run','input_files','output_files','command']
        for k in req_fields:
            if k not in config:
                raise ValueError("required field missing: '%s'" % k)
        if 'stat' not in config['output_files']:
            raise ValueError("required field missing from output_files: 'stat'")

        for k,v in config.iteritems():
            self[k]=v

        if 'timeout' not in self:
            self['timeout'] = 2*60*60 # two hours
        else:
            self['timeout'] = int(timeout)

        self.setup_files()

    def run(self):

        self['exit_status'] = -9999
        #self._dorun()
        try:
            self._dorun()
        finally:
            self.write_status()
            self.cleanup()
        print >>stderr,'Done'

    def _dorun(self):
        print >>stderr,os.uname()[1]
        
        self.make_output_dirs()
        self.stage()

        command=self.get_command()

        print >>stderr,"running command: \n\t",command
        exit_status, oret, eret = eu.ostools.exec_process(command,
                                                          timeout=self['timeout'],
                                                          stdout_file=None,
                                                          stderr_file=None)

        self['exit_status'] = exit_status
        print >>stderr,'exit_status:',exit_status
        #test=raw_input('testing, hit enter ')


    def get_command(self):
        """
        Interpolate keys into the input command
        """
        command = self['command'] % self.allkeys
        return command

    def make_output_dirs(self):
        """
        run through all the output files and make sure associated
        directories exist
        """
        for k,v in self.outf.iteritems():
            eu.ostools.makedirs_fromfile(v['local_url'])
    def stage(self):
        """
        Stage in hdfs files as necessary
        """
        for k,v in self.inf.iteritems():
            if v['in_hdfs']:
                v['hdfs_file'].stage()
    def cleanup(self):
        """
        Remove local versions of staged hdfs files.
        For input files, put them in hdfs and clean up local storage
        """
        # clean up staged input files
        for k,v in self.inf.iteritems():
            if v['in_hdfs']:
                v['hdfs_file'].cleanup()

        # push local versions of outputs into hdfs, then clean up
        for k,v in self.outf.iteritems():
            if v['in_hdfs']:
                if os.path.exists(v['local_url']):
                    v['hdfs_file'].put(clobber=True)
                else:
                    print >>stderr,'local file not found:',v['local_url']
                v['hdfs_file'].cleanup()

    def setup_files(self):
        """
        Create self.inf and self.out, as well as the self.allkeys dict with all
        keys together from input config and output/input file dicts.

        HDFS files are checked for and marked appropriately.
        """
        inf={}
        outf={}

        self.inf = self._setup_files(self['input_files'])
        self.outf = self._setup_files(self['output_files'])

        # bring all keys into a single namespace for use
        # in the command string interpolation
        self.allkeys={}
        for k,v in self.iteritems():
            self.allkeys[k] = v
        for k,v in self.inf.iteritems():
            self.allkeys[k] = v['local_url']
        for k,v in self.outf.iteritems():
            self.allkeys[k] = v['local_url']

    def _setup_files(self, fdict_in):
        """
        Get info about the files.
        """
        fdict={}
        for f,v in fdict_in.iteritems():
            fdict[f] = {'in_hdfs':False, 'url':v,'local_url':v}
            if v[0:4] == 'hdfs':
                fdict[f]['in_hdfs'] = True
                fdict[f]['hdfs_file'] = \
                    eu.hdfs.HDFSFile(v,verbose=True, 
                                     tmpdir=deswl.wlpipe._wlpipe_tmpdir)
                fdict[f]['local_url'] = fdict[f]['hdfs_file'].localfile
        return fdict

    def write_status(self):
        """
        Add a bunch of new things to self and write self out as the stat file
        """
        print >>stderr,'writing status file:',self.outf['stat']['local_url']
        outd={}
        for k,v in self.iteritems():
            outd[k] = v
        eu.io.write(self.outf['stat']['local_url'],outd)



class GenericSEWQJob(dict):
    """
    Generic WQ job to process all ccds in an exposure.

    You should also create the config files for each exposure/ccd using. config
    files go to deswl.files.se_config_path(run,expname,ccd=ccd)

    """
    def __init__(self, run, expname, **keys):
        self['run'] = run
        self['expname'] = expname
        self['groups'] = keys.get('groups',None)

        self['job_file']= deswl.files.se_wq_path(self['run'], self['expname'])

    def write(self):

        groups = self['groups']
        expname=self['expname']

        if groups is None:
            groups=''
        else:
            groups='group: [' + groups +']'

        rc=deswl.files.Runconfig(self['run'])
        wl_load = deswl.files._make_load_command('wl',rc['wlvers'])
        esutil_load = deswl.files._make_load_command('esutil', rc['esutilvers'])
 

        command_list = []
        for ccd in xrange(1,62+1):
            # get relative config url, just so easier to read
            config_file=deswl.files.se_config_path(self['run'], 
                                                   self['expname'], 
                                                   ccd=ccd)
            config_file=os.path.basename(config_file)
            config_file='byccd/'+config_file

            log_file=config_file.replace('-config.yaml','.out')

            c="wl-run-generic %s &> %s" % (config_file,log_file)
            command_list.append(c)

        cmd='\n    '.join(command_list) 

        text = """
command: |
    source /opt/astro/SL53/bin/setup.hadoop.sh
    source ~astrodat/setup/setup.sh
    source ~/.dotfiles/bash/astro.bnl.gov/modules.sh
    %(esutil_load)s
    %(wl_load)s

    %(cmd)s

%(groups)s
priority: low
job_name: %(job_name)s\n""" % {'esutil_load':esutil_load,
                               'wl_load':wl_load,
                               'cmd':cmd,
                               'groups':groups,
                               'job_name':self['expname']}


        with open(self['job_file'],'w') as fobj:
            fobj.write(text)
