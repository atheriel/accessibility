# -*- coding: utf-8 -*-
#
# Accessibility documentation build configuration file, created by
# sphinx-quickstart on Wed Jan 8 19:26:38 2014.

import os
import sys

sys.path.append(os.path.abspath('theme'))

# -- General configuration -----------------------------------------------------

extensions = ['sphinx.ext.autodoc', 'sphinx.ext.viewcode', 'sphinx.ext.autosummary']
templates_path = ['_templates']
source_suffix = '.rst'
source_encoding = 'utf-8'
master_doc = 'index'
project = u'Accessibility'
copyright = u'2014, Aaron Jacobs'
version = '0.3.1'
release = '0.3.1'
exclude_patterns = ['_build', 'theme']

# -- Options for HTML output ---------------------------------------------------

html_theme = 'theme'
html_theme_path = ['.']
html_theme_options = {
    'show_navigation': 'false',
    'github_fork': 'atheriel/accessibility',
}
html_static_path = ['_static']
htmlhelp_basename = 'Accessibilitydoc'
