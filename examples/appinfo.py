"""appinfo.py

Demonstrates some of the basic functionality of the module, and why one might
be interested in using it. It prints out some information about the application
associated with the given PID.

This script also demonstrates the dictionary-like behaviour of the module's
main class, the AccessibleElement.
"""

from __future__ import print_function

import sys
import accessibility as acc

try:
    sys.argv[1]
except IndexError:
    print('Usage: appinfo.py PID')
    sys.exit(1)

app = acc.create_application_ref(pid = int(sys.argv[1]))

if app['AXRole'] != 'AXApplication':
    print('This PID is not associated with an application.')
    sys.exit(1)

if 'AXTitle' in app:
    print('Application name: %s' % app['AXTitle'])

if 'AXWindows' in app:
    windows = app['AXWindows']
    if len(windows) == 0:
        print('No windows found for the application.')
    else:
        print('Windows found: %d' % len(windows))
        for window in windows:
            if 'AXTitle' in window and 'AXSize' in window and 'AXPosition' in window:
                x, y = window['AXSize']
                xx, yy = window['AXPosition']
                print('    <%s> of size (%d,%d) at position (%d,%d)' % (window['AXTitle'], x, y, xx, yy))
