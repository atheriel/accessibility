Accessibility: Wrapper for the Accessibility API
================================================
``accessibility`` is a Python module that wraps the Accessibility API for Mac OS X. It can be used to query and modify attributes of running applications, as well as watch for a variety of notifications. The source code and several examples (in the ``examples`` directory) are hosted on `GitHub <https://github.com/atheriel/accessibility>`_.

The module should compile under recent versions of both Python 2 and 3, and work with Mac OS X 10.8.x and 10.9.x. In addition, to compile the module on version 10.9.0 or later of OS X, you will need to have the Xcode IDE installed.

Building
--------
The module can be compiled using the traditional ``python setup.py clean build install`` provided by setuptools.

Documentation
-------------
The module includes extensive docstrings, complete with examples in many cases. These can be can be browsed using Python's ``help`` command, or one can compile the Sphinx documentation. For the latter: 

1. Initialize the git submodule with ``git submodule init`` to retrieve the custom Sphinx theme.
2. ``cd docs``
3. ``make html`` and then browse the documentation in ``docs/_build/html``.

License
-------
This project is under the ISC License. See the ``LICENSE.txt`` file for details.
