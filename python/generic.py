"""
The process is similar to what happens for the SE and ME runs

In short
    - Generate a runconfig
    - /bin/generate-generic-wq

        - ~/des-wq/{run}/{expname}.yaml
            These are the actual process jobs; calls the code
            for each ccd using the config files under /byccd

        - for each ccd, there are config file
            ~/des-wq/{run}/byccd/{expname}-{ccd}-config.yaml

        - ~/des-wq/{run}/{expname}-check.yaml
            These are jobs to check the processing

        - ~/des-wq/{run}/check-reduce.py
            Run this to collate the results into the "goodlist"
            and "badlist"

    - submit wq jobs
        Follow the .wqlog files for very brief updates.
    - submit the check wq jobs
        Follow the .wqlog files for very brief updates.
    - run the reducer to generate the goodlist and badlist

The GenericProcessor will send stdout and stderr for process, and
some other diagnostics to the 'log' entry in config['output_files']['log']

    - .../DES/{fileclass}/{run}/{expname}/{run}-{expname}-{ccd}.log

Similarly, the GenericProcessor writes the stat file

    - .../DES/{fileclass}/{run}/{expname}/{run}-{expname}-{ccd}-stat.yaml

The stat file holds the exit_status and whatever was in the config file.

The collation step is not yet implemented; probably each owner of the code will
have to do this step.
"""
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

                It must also contain an entry called 'log' where all
                stdout and stderr will be written.

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
        if 'log' not in config['output_files']:
            raise ValueError("required field missing from output_files: 'log'")

        # add a log file
        stat=config['output_files']['stat']
        # this and exit status the only thing that goes to stderr or stdout
        print >>stderr,"log file:",config['output_files']['log']

        for k,v in config.iteritems():
            self[k]=v

        if 'timeout' not in self:
            self['timeout'] = 2*60*60 # two hours
        else:
            self['timeout'] = int(self['timeout'])

        self.setup_files()

        log_name = self.outf['log']['local_url']
        #eu.ostools.makedirs_fromfile(log_name)
        self._log = open(log_name,'w')

    def __enter__(self):
        return self
    def __exit__(self, exception_type, exception_value, traceback):
        # this shouldn't be necessary
        if hasattr(self,'_log'):
            if isinstance(self._log,file):
                self._log.close()

    def run(self):

        self['exit_status'] = -9999
        #self._dorun()
        try:
            self._dorun()
            print >>self._log,'Done'
        finally:
            self.write_status()
            self.cleanup()

    def _dorun(self):
        from subprocess import STDOUT
        print >>self._log,os.uname()[1]
        
        #self.make_output_dirs()
        self.stage()

        command=self.get_command()

        print >>self._log,"running command: \n\t",command
        self._log.flush()
        exit_status, oret, eret = \
            eu.ostools.exec_process(command,
                                    timeout=self['timeout'],
                                    stdout_file=self._log,
                                    stderr_file=STDOUT)

        self['exit_status'] = exit_status
        print >>self._log,'exit_status:',exit_status
        print >>stderr,'exit_status:',exit_status


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
                print >>self._log,'staging:',\
                        v['hdfs_file'].hdfs_url,'->',v['hdfs_file'].localfile
                v['hdfs_file'].stage()
    def cleanup(self):
        """
        Remove local versions of staged hdfs files.
        For input files, put them in hdfs and clean up local storage
        """
        # clean up staged input files
        for k,v in self.inf.iteritems():
            if v['in_hdfs']:
                print >>self._log,'removing:',v['hdfs_file'].localfile
                v['hdfs_file'].cleanup()

        # push local versions of outputs into hdfs, then clean up
        do_log_hdfs=False
        for k,v in self.outf.iteritems():
            if v['in_hdfs']:
                if k == 'log':
                    # save till later so we see the log messages
                    do_log_hdfs=True
                    continue
                if os.path.exists(v['local_url']):
                    print >>self._log,'putting:',\
                        v['hdfs_file'].localfile,'->',v['hdfs_file'].hdfs_url
                    v['hdfs_file'].put(clobber=True)
                else:
                    print >>self._log,'local file not found:',v['local_url']
                v['hdfs_file'].cleanup()
        if do_log_hdfs:
            hf=self.outf['log']
            print >>self._log,'putting:',\
                hf['hdfs_file'].localfile,'->',hf['hdfs_file'].hdfs_url

            self._log.flush()
            hf['hdfs_file'].put(clobber=True)
            self._log.close()
            hf['hdfs_file'].cleanup()

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

    def _make_log_name(self, example):
        idot=example.rfind('.')
        if idot == -1:
            log_name = example+'.log'
        else:
            log_name = example[0:idot]+'.log'
        return log_name

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
                    eu.hdfs.HDFSFile(v)
                fdict[f]['local_url'] = fdict[f]['hdfs_file'].localfile
        return fdict

    def write_status(self):
        """
        Add a bunch of new things to self and write self out as the stat file
        """
        print >>self._log,'writing status file:',self.outf['stat']['local_url']
        outd={}
        for k,v in self.iteritems():
            outd[k] = v
        eu.io.write(self.outf['stat']['local_url'],outd)



class GenericSEWQJob(dict):
    """
    Generic WQ job to process all ccds in an exposure.

    You should also create the config files for each exposure/ccd using. config
    files go to deswl.files.se_config_path(run,expname,ccd=ccd)

    Send check=True to generate the check file instead of the processing file
    """
    def __init__(self, run, expname, **keys):
        self['run'] = run
        self['expname'] = expname
        self['groups'] = keys.get('groups',None)
        self['priority'] = keys.get('priority','low')

        self['job_file']= deswl.files.se_wq_path(self['run'], self['expname'])

    def write(self, check=False):

        expname=self['expname']
        groups = self['groups']

        if check:
            groups=None # can run anywhere for sure
            job_file=self['job_file'].replace('.yaml','-check.yaml')
            job_name=expname+'-chk'
        else:
            job_file=self['job_file']
            job_name=expname

        if groups is None:
            groups=''
        else:
            groups='group: ['+groups+']'

        rc=deswl.files.Runconfig(self['run'])
        wl_load = deswl.files._make_load_command('wl',rc['wlvers'])
        esutil_load = deswl.files._make_load_command('esutil', rc['esutilvers'])

        config_file1=deswl.files.se_config_path(self['run'], 
                                                self['expname'], 
                                                ccd=1)
        config_file1=os.path.join('byccd',os.path.basename(config_file1))
        conf=config_file1.replace('01-config.yaml','$i-config.yaml')
        if check:
            chk=config_file1.replace('01-config.yaml','$i-check.json')
            err=config_file1.replace('01-config.yaml','$i-check.err')

            #cmd="wl-check-generic {conf} 1> {chk} 2> {err}"
            cmd="wl-check-generic {conf} 1> {chk}"
            cmd=cmd.format(conf=conf, chk=chk, err=err)
        else:
            # log is now automatically created by GenericProcessor
            # and written into hdfs
            cmd="wl-run-generic {conf}".format(conf=conf)

        text = """
command: |
    source /opt/astro/SL53/bin/setup.hadoop.sh
    source ~astrodat/setup/setup.sh
    source ~/.dotfiles/bash/astro.bnl.gov/modules.sh
    %(esutil_load)s
    %(wl_load)s

    for i in `seq -w 1 62`; do
        echo "ccd: $i"
        %(cmd)s
    done

%(groups)s
priority: %(priority)s
job_name: %(job_name)s\n""" % {'esutil_load':esutil_load,
                               'wl_load':wl_load,
                               'cmd':cmd,
                               'groups':groups,
                               'priority':self['priority'],
                               'job_name':job_name}


        with open(job_file,'w') as fobj:
            fobj.write(text)


