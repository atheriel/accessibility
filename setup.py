from setuptools import setup, Extension

setup(
    name = 'accessibility',
    description = 'Extension module that wraps the Accessibility API for Mac OS X.',
    version = '0.2.1',
    author = 'Aaron Jacobs',
    author_email = 'atheriel@gmail.com',
    url = 'https://github.com/atheriel/accessibility',
    license = 'ISC License',
    platforms = ['MacOS X'],
    classifiers = [
        'Development Status :: 3 - Alpha',
        'Programming Language :: Python',
        'Programming Language :: Python :: Implementation :: CPython',
        'Environment :: MacOS X :: Carbon',
        'Environment :: MacOS X :: Cocoa',
        'Operating System :: MacOS :: MacOS X',
        'Natural Language :: English',
        'License :: OSI Approved :: ISC License (ISCL)',
        'Topic :: Utilities'
    ],

    ext_modules = [Extension('accessibility',
        sources = ['accessibility.c'],
        # Uncomment the next line to include debug symbols while compiling
        # extra_compile_args = ['-g'],
        extra_link_args = ['-framework', 'ApplicationServices', '-v']
    )],

    install_requires = [
        'docutils>=0.3',
    ]
)
