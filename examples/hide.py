"""hide.py

Hides or unhides an application with the given PID.
"""

import sys
import accessibility as acc

try:
    sys.argv[1]
except IndexError:
    print 'Usage: appinfo.py PID'
    sys.exit(1)

app = acc.create_application_ref(pid = int(sys.argv[1]))

if app['AXRole'] != 'AXApplication':
    print 'This PID is not associated with an application.'
    sys.exit(1)

if not 'AXHidden' in app or not app.can_set('AXHidden'):
    print 'The application does not seem to be hide-able.'
    sys.exit(1)

app['AXHidden'] = False if app['AXHidden'] else True
