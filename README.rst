Virtual Laboratory Environment 1.1.x
====================================

A nodejs/javascript binding for the VFL.

See AUTHORS and COPYRIGHT for the list of contributors.

Requirements
------------

* node (>= 4.5.0 - stable version)
* npm (>= 3.x)
* node-gyp (>= 3.x)
* vle (>= 1.1.3)
* c++ compiler (gcc >= 4.4, clang >= 3.1, intel icc (>= 11.0)

Getting the code
----------------

The source tree is currently hosted on Github. To view the
repository online: https://github.com/vle-forge/node-vle

The URL to clone it:

::

  git clone git://github.com/vle-forge/node-vle.git

Once you have met requirements, compiling and installing is very simple:

::

  cd node-vle
  npm install

License
-------

This software in GPLv3 or later. See the file COPYING. Some files are under a
different license. Check the headers for the copyright info.

Usage
-----

::

 var vle = require('node-vle');
 var vpz = new vle.Vle("test_package", "test_simulation.vpz");

 vpz.condition_port_clear('cond_xxx', 'aVariable');
 vpz.condition_add_value('cond_xxx', 'aVariable', new vle.value([0, 1, 2, {x: 100}]));

 vpz.condition_port_clear('cond_xxx', 'anotherVariable');
 vpz.condition_add_real('cond_xxx', 'anotherVariable', 1000);

 var result = vpz.run();
 var dates = res.view_xxxx.time;
 var series = res.view_xxx.TopModel.SubModel.SubSubModel.AtomicModel.data;
 var sum = 0;
 
 for (var i = 0; i < dates.length; ++i) {
  sum += series[i];
 }
