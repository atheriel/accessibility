"""watchwindow.py

Demonstrates how to use the notication system to watch events in other
applications, in this case when a window is moved or resized in the application
with the given PID.

The script includes two definitions of a callback function that are equivalent
but may fit different styles of programming.
"""

import sys
import signal
import accessibility as acc
from Quartz import CFRunLoopRun

try:
	sys.argv[1]
except IndexError:
	print 'Usage: watchwindow.py PID'
	sys.exit(1)

# Allow quitting with CTRL-C
signal.signal(signal.SIGINT, signal.SIG_DFL)

def func1(notification, element):
    print 'Notification <%s> for application <%s>.' % (notification, element['AXTitle'])

def func2(*args, **kwargs):
	print 'Notification <%s> for application <%s>.' % (kwargs['notification'], kwargs['element']['AXTitle'])

mess = acc.create_application_ref(pid = int(sys.argv[1]))
mess.set_callback(func2)  # or func1, if you prefer
mess.watch("AXMoved", "AXWindowResized")

CFRunLoopRun()
