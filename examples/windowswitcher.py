"""windowswitcher.py

Cycles through an application's windows and performs the first available
action. More than likely this is AXRaise, which brings the window to the front.
"""

import sys
import accessibility as acc

try:
    sys.argv[1]
except IndexError:
    print 'Usage: windowswitcher.py PID'
    sys.exit(1)

app = acc.create_application_ref(pid = int(sys.argv[1]))

if app['AXRole'] != 'AXApplication':
    print 'This PID is not associated with an application.'
    sys.exit(1)

if not 'AXWindows' in app or len(app['AXWindows']) == 0:
    print 'The application does not seem to have any windows.'
    sys.exit(1)

windows = app['AXWindows']

if len(windows[0].actions()) > 0:
    print 'Available actions for the windows:', windows[0].actions()
    for w in windows:
        w.perform_action(w.actions()[0])
else:
    print 'No actions available.'
