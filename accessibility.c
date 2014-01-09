#include <stdio.h>
#include <string.h>
#include <Python.h>
#include <structmember.h>
#include <Accessibility.h>

/*
 * Intended to allow formatted error messages. Format strings work like
 * they do in printf(), etc.
 */
char * formattedMessage(char * format, ...) {
    char buf[1024];
    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);
    size_t l = strlen(buf);
    while(l && buf[l-1] =='\n') {
        buf[l-1] = 0;
        l--;
    }
    return strdup(buf);
}

/*
 * Converts a Python string to a CFStringRef that Cocoa/Carbon will understand.
 */
CFStringRef CFStringFromPyString(PyObject * str, char ** c_string) {
    if (PyUnicode_Check(str)) { // Handle Unicode strings
        str = PyUnicode_AsUTF8String(str);
    }
#if PY_MAJOR_VERSION >= 3
    if (!PyBytes_Check(str)) {
#else
    if (!PyString_Check(str)) {
#endif
        PyErr_SetString(PyExc_TypeError, "Non-string parameters are not permitted.");
        return NULL;
    }

    // Get a string representation of the attribute name
#if PY_MAJOR_VERSION >= 3
    *c_string = PyBytes_AsString(str);
#else
    *c_string = PyString_AsString(str);
#endif
    if (!c_string) {
        PyErr_SetString(PyExc_TypeError, "An unknown error occured while converting string arguments to char *.");
        return NULL;
    }

    // Convert that representation to something Cocoa/Carbon will understand.
    CFStringRef cf_string = CFStringCreateWithCString(kCFAllocatorDefault, *c_string, kCFStringEncodingUTF8);
    if (cf_string == NULL) PyErr_SetString(PyExc_TypeError, "An unknown error occured while converting a string argument to a CFStringRef.");
    return cf_string;
}

/* ========
    Module API and Docstrings
======== */

/* Accessible Element class
======== */

PyDoc_STRVAR(module_docstring,
"Extension module that wraps the Accessibility API for Mac OS X.\n\n\
This module provides the :py:class:`AccessibleElement` class to interact with \n\
the references exposed by the Accessibility API (for example, other running \n\
applications). One can query these references for their attributes, modify \n\
these attributes, and watch for notifcations emitted by these references.\n\
\n\
.. codeauthor:: Aaron Jacobs <atheriel@gmail.com>");

PyDoc_STRVAR(AccessibleElement_docstring,
"An AccessibleElement object is a wrapper around the Accessibility API's basic \n\
unit, the `AXUIElementRef`_. It provides access to most of the API's functionality \n\
in a highly object-oriented fashion (in other words, exactly the opposite of \n\
how one interacts with the API itself). Its individual methods are extensively \n\
documented with the details of their use.\n\
\n\
The object implements Python's mapping protocol to make accessing attributes \n\
a more 'pythonic' experience. This includes membership (i.e. 'AXRole' in \n\
myelement), element access using the [] syntax, and the :py:func:`keys` method. \n\
However, it will not respond to the ``len`` function, because this may cause \n\
some semantic confusion with the functionality of the :py:func:`count` method.\n\
\n\
Comparison of two AccessibleElement objects returns True if and only if their \n\
underlying AXUIElementRef references point to the same object.\n\
\n\
A typical piece of code to modify the size of a window might take the following \n\
form:\n\
\n\
.. code-block:: python\n\
\n\
    if 'AXSize' in window_element and window_element.can_set('AXSize'):\n\
        window_element.set('AXSize', (100, 100))\n\
        # or, using the element as a dictionary:\n\
        window_element['AXSize'] == (100, 100)\n\
\n\
Notifications emitted by applications can also be handled by using the \n\
:py:func:`watch` and :py:func:`set_callback` methods.\n\
\n\
Some care has been taken to manage the memory used by the CFTypeRef system, but \n\
this has not been rigorously tested.\n\
\n\
.. _AXUIElementRef: https://developer.apple.com/library/mac/documentation/Appli\
cationServices/Reference/AXUIElement_header_reference/Reference/reference.html");

typedef struct {
    PyObject_HEAD
    AXUIElementRef _ref;
    AXObserverRef _obs;
    PyObject * pid;
    PyObject * callback;
} AccessibleElement;

static void AccessibleElement_dealloc(AccessibleElement *);
static PyObject * AccessibleElement_richcompare(PyObject *, PyObject *, int);
static int AccessibleElement_contains(AccessibleElement *, PyObject *);
static PyObject * AccessibleElement_subscript(AccessibleElement *, PyObject *);
static int AccessibleElement_ass_subscript(AccessibleElement *, PyObject *, PyObject *);

PyDoc_STRVAR(keys_docstring, "keys()\n\n\
Retrieves a list of attribute names available for this element.");

static PyObject * AccessibleElement_keys(AccessibleElement *, PyObject *);

PyDoc_STRVAR(can_set_docstring, "can_set(name)\n\n\
Checks whether the attribute of a given name is modifiable.");

static PyObject * AccessibleElement_can_set(AccessibleElement *, PyObject *);

PyDoc_STRVAR(count_docstring, "count(*names)\n\n\
Counts the number of values for the specified attribute name(s), which may be \n\
zero. If the element does not possess this/these attribute(s), this method will \n\
raise a KeyError.\n\
\n\
:param names: Either a single name or a series of names, all strings.\n\
:rvalue: Either a single value's count or a tuple of the values' counts.\n\
\n\
A common usage might look like the following:\n\
\n\
.. code-block:: python\n\
\n\
    try:\n\
        role_counts = element.count('AXRole', 'AXRoleDescription')\n\
        print 'Count for AXRole: %d and AXRoleDescription: %d' % role_counts\n\
    except KeyError:\n\
        print 'I guess those aren\\'t available...'");

static PyObject * AccessibleElement_count(AccessibleElement *, PyObject *);

PyDoc_STRVAR(get_docstring, "get(*names)\n\n\
Returns a copy of the values for the specified attribute name(s), which may be \n\
``None``. If the element does not possess this/these attribute(s), this method \n\
will raise a ``KeyError``.\n\
\n\
This is the underlying method called when using an AccessibleElement as a dict.\n\
\n\
:param names: Either a single name or a series of names, all strings.\n\
:rvalue: Either a single value or a tuple of the values.\n\
\n\
A common usage might look like the following:\n\
\n\
.. code-block:: python\n\
\n\
    try:\n\
        role = element.get('AXRole')\n\
        print role\n\
    except KeyError:\n\
        print 'Seems this element is not available.'\n\
\n\
This is almost equivalent to using the element like a dictionary:\n\
\n\
.. code-block:: python\n\
\n\
    if 'AXRole' in element:\n\
        print element['AXRole']\n\
    else:\n\
        print 'Seems this element is not available.'");

static PyObject * AccessibleElement_get(AccessibleElement *, PyObject *);

PyDoc_STRVAR(is_alive_docstring, "is_alive()\n\n\
Returns ``True`` if the AXUIElementRef is still valid.");

static PyObject * AccessibleElement_is_alive(AccessibleElement *, PyObject *);

PyDoc_STRVAR(set_docstring, "set(name, value)\n\n\
Sets the value of the attribute with the given name (if possible). If the \n\
element does not possess this attribute, this method will raise a KeyError. If \n\
it is not modifiable, this method will raise a ValueError.\n\
\n\
This is the underlying method called when using an AccessibleElement as a dict.\n\
\n\
:param str name: The name of the attribute to set.\n\
:param value: The new value of the attribute.\n\
\n\
\nEach attribute may set a different kind of value. For example:\n\
\n\
.. code-block:: python\n\
\n\
    if 'AXSize' in window_element and window_element.can_set('AXSize'):\n\
        window_element.set('AXSize', (100, 100))\n\
        # or, using the element as a dictionary:\n\
        window_element['AXSize'] == (100, 100)");

static PyObject * AccessibleElement_set(AccessibleElement *, PyObject *);

PyDoc_STRVAR(set_callback_docstring, "set_callback(func)\n\n\
Sets the callback for handling notifications watched with :py:func:`watch` when \n\
they arise.\n\
\n\
:param func: The function to call when watched notifications occur.\n\
\n\
The correct format for a callback is as follows:\n\
\n\
.. code-block:: python\n\
\n\
    def func(element, notification):\n\
        print element.pid, notification\n\
\n\
    myelement.set_callback(func)");

static PyObject * AccessibleElement_set_callback(AccessibleElement *, PyObject *);

PyDoc_STRVAR(set_timeout_docstring, "set_timeout(timeout)\n\n\
Sets the timeout for requests to the Accessibility API for this element. To \n\
change the value used for all elements, call this method on the system-wide \n\
element like so:\n\
\n\
.. code-block:: python\n\
\n\
    sr = create_systemwide_ref()\n\
    sr.set_timeout(5.0)  # i.e. five seconds to respond\n\
\n\
The default value can be restored by passing the ``DEFAULT_TIMEOUT`` value.");

static PyObject * AccessibleElement_set_timeout(AccessibleElement *, PyObject *);

PyDoc_STRVAR(watch_docstring, "watch(*notifications)\n\n\
Watches for the given notifications. When they occur, the callback passed to \n\
:py:func:`set_callback` will be executed.\n\
\n\
:param str notifications: The name (or series of names) of the notification to watch.\n\
\n\
For example, to watch for when windows are moved or resized in an application:\n\
\n\
.. code-block:: python\n\
\n\
    myelement.watch('AXMoved', 'AXResized')");

static PyObject * AccessibleElement_watch(AccessibleElement *, PyObject *);

/* Module functions
======== */

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
PyDoc_STRVAR(is_enabled_docstring, "is_enabled(prompt = True)\n\n\
Checks whether calls to the Accessibility API are currently possible in this \n\
context, and prompts the user to make the necessary changes in System \n\
Preferences if necessary. This likely means allowing the Terminal application \n\
to \'control your computer\'.");

static PyObject * is_enabled(PyObject *, PyObject *, PyObject *);
#else
static PyObject * is_enabled(PyObject *);
#endif

static AccessibleElement * create_application_ref(PyObject *, PyObject *, PyObject *);
static AccessibleElement * create_systemwide_ref(PyObject *, PyObject *);

PyDoc_STRVAR(element_at_position_docstring, "element_at_position(x, y, element = None)\n\n\
Gets the window element at the (x, y) position. If ``element`` is specified, \n\
then this position is relative to that application. Otherwise, the system \n\
element is used instead.\n\
\n\
:param float x: The x coordinate.\n\
:param float y: The y coordinate.\n\
:param AccessibleElement element: The application element to use as a reference.\n\
:rval: An :py:class:`AccessibleElement` object referring to a window at the given position.");

static AccessibleElement * element_at_position(PyObject *, PyObject *, PyObject *);

/* Module exceptions
======== */

PyDoc_STRVAR(InvalidUIElementError_docstring, 
"Raised when a reference to some AccessibleElement is no longer valid, usually \n\
because the process is dead.");

static PyObject * InvalidUIElementError;

PyDoc_STRVAR(APIDisabledError_docstring, 
"Raised when a the Accessibility API is disabled for some reference. Usually \n\
this is because the user needs to enable Accessibility, although some Apple \n\
applications are known to respond with this error regardless.");

static PyObject * APIDisabledError;

/* ========
    Internal API
======== */

static PyObject * parseCFTypeRef(const CFTypeRef);
static AccessibleElement * elementWithRef(AXUIElementRef *);
static void handleAXErrors(const char *, AXError);
static void NotifcationCallback(AXObserverRef, AXUIElementRef, CFStringRef, void *);

/* ========
    Module Implementation
======== */

/* AccessibleElement class
======== */

static void AccessibleElement_dealloc(AccessibleElement * self) {
    // Use CFRelease to release for the AXUIElementRef
    if (self->_ref != NULL) CFRelease(self->_ref);
    Py_XDECREF(self->pid);
#if PY_MAJOR_VERSION >= 3
    Py_TYPE(self)->tp_free((PyObject *) self);
#else
    self->ob_type->tp_free((PyObject *) self);
#endif
}

static PyObject * AccessibleElement_richcompare(PyObject * self, PyObject * other, int op) {
    PyObject * result = NULL;

    if (self->ob_type != other->ob_type) {
        // Need both to be the same type
        result = Py_NotImplemented;
    } else {
        AccessibleElement * s = (AccessibleElement *) self;
        AccessibleElement * o = (AccessibleElement *) other;
        // Uses CFEqual on the underlying AXUIElementRefs for comparison
        int i = CFEqual(s->_ref, o->_ref) ? 1 : 0;
        switch (op) {
            case Py_EQ:
                result = (i > 0) ? Py_True : Py_False;
                break;
            case Py_NE:
                result = (i > 0) ? Py_False : Py_True;
                break;
            default:
                PyErr_SetString(PyExc_TypeError, "AccessibleElement objects can only be compared with == and != operators.");
                break;
        }
    }

    Py_XINCREF(result);
    return result;
}

/* Sequence Protocol
======== */

static int AccessibleElement_contains(AccessibleElement * self, PyObject * arg) {
    // The argument should be a string
    char * name_string = NULL;
    CFStringRef name_strref = CFStringFromPyString(arg, &name_string);
    if (!name_strref) return 0;

    // Check if the attribute's value can be copied
    CFTypeRef value = NULL;
    AXError error = AXUIElementCopyAttributeValue(self->_ref, name_strref, &value);
    if (name_strref != NULL) CFRelease(name_strref);
    if (value != NULL) CFRelease(value);
    
    return (error == kAXErrorSuccess) ? 1 : 0;
}

static PySequenceMethods AccessibleElement_as_sequence = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    (objobjproc) AccessibleElement_contains,
};

/* Mapping Protocol
======== */

static PyObject * AccessibleElement_subscript(AccessibleElement * self, PyObject * key) {
    return AccessibleElement_get(self, Py_BuildValue("(O)", key));
}

static int AccessibleElement_ass_subscript(AccessibleElement * self, PyObject * key, PyObject * value) {
    return (AccessibleElement_set(self, Py_BuildValue("OO", key, value)) != NULL) ? 0 : -1;
}

static PyMappingMethods AccessibleElement_as_mapping = {
    0,
    (binaryfunc) AccessibleElement_subscript,
    (objobjargproc) AccessibleElement_ass_subscript,
};

static PyObject * AccessibleElement_keys(AccessibleElement * self, PyObject * args) {
    PyObject * result = NULL;
    CFArrayRef names;
    AXError error = AXUIElementCopyAttributeNames(self->_ref, &names);
    if (error == kAXErrorSuccess) {
        result = parseCFTypeRef(names);
    } else {
        handleAXErrors("attribute names", error);
    }

    if (names != NULL) CFRelease(names);
    return result;
}

/* Members
======== */

static PyObject * AccessibleElement_can_set(AccessibleElement * self, PyObject * args) {
    PyObject * result = NULL;
    Py_ssize_t attribute_count = PyTuple_Size(args);
    if (attribute_count > 1) {
        PyErr_SetString(PyExc_ValueError, "Too many arguments.");
        return NULL;
    }
    
    // The first argument should be a string
    PyObject * name = PyTuple_GetItem(args, (Py_ssize_t) 0);
    if (!name) return NULL; // PyTuple_GetItem will set an Index error.
    char * name_string = NULL;
    CFStringRef name_strref = CFStringFromPyString(name, &name_string);
    if (!name_strref) return NULL; // CFStringFromPyString will set an error.

    // Check to see if the attribute can be set at all
    Boolean can_set;
    AXError error = AXUIElementIsAttributeSettable(self->_ref, name_strref, &can_set);

    if (error == kAXErrorSuccess) {
        result = can_set ? Py_True : Py_False;
    } else {
        handleAXErrors(name_string, error);
    }

    if (name_strref != NULL) CFRelease(name_strref);
    Py_XINCREF(result);
    return result;
}

static PyObject * AccessibleElement_count(AccessibleElement * self, PyObject * args) {
    PyObject * result = Py_None;
    // This allows for retrieving multiple objects, so find how many were
    // requested and loop over them.
    Py_ssize_t attribute_count = PyTuple_Size(args);
    if (attribute_count > 1) {
        result = PyTuple_New(attribute_count);
    }

    for (int i = 0; i < attribute_count; i++) {
        PyObject * name = PyTuple_GetItem(args, (Py_ssize_t) i);
        if (!name) {
            if (attribute_count > 1) Py_DECREF(result);
            return NULL; // PyTuple_GetItem will set an Index error.
        }
        char * name_string = NULL;
        CFStringRef name_strref = CFStringFromPyString(name, &name_string);
        if (!name_strref) {
            if (attribute_count > 1) Py_DECREF(result);
            return NULL; // CFStringFromPyString will set an error.
        }
        
        // Get the count itself
        CFIndex count;
        AXError error = AXUIElementGetAttributeValueCount(self->_ref, name_strref, &count);
        
        if (error == kAXErrorSuccess) {
            if (attribute_count > 1) {
                PyTuple_SetItem(result, i, Py_BuildValue("i", count));
            } else {
                result = Py_BuildValue("i", count);
            }
        } else {
            // If any of the requests fail, release memory and raise an exception
            if (attribute_count > 1) Py_DECREF(result);
            if (name_strref != NULL) CFRelease(name_strref);
            handleAXErrors(name_string, error);
            return NULL;
        }
        if (name_strref != NULL) CFRelease(name_strref);
    }
    return result;
}

static PyObject * AccessibleElement_get(AccessibleElement * self, PyObject * args) {
    PyObject * result = NULL;
    // This allows for retrieving multiple objects, so find how many were
    // requested and loop over them.
    Py_ssize_t attribute_count = PyTuple_Size(args);
    if (attribute_count > 1) {
        result = PyTuple_New(attribute_count);
    }

    for (int i = 0; i < attribute_count; i++) {
        PyObject * name = PyTuple_GetItem(args, (Py_ssize_t) i);
        if (!name) {
            if (attribute_count > 1) Py_DECREF(result);
            return NULL; // PyTuple_GetItem will set an Index error.
        }
        char * name_string = NULL;
        CFStringRef name_strref = CFStringFromPyString(name, &name_string);
        if (!name_strref) {
            if (attribute_count > 1) Py_DECREF(result);
            return NULL; // CFStringFromPyString will set an error.
        }
        
        // Copy the value
        CFTypeRef value = NULL;
        AXError error = AXUIElementCopyAttributeValue(self->_ref, name_strref, &value);
        
        if (error == kAXErrorSuccess) {
            if (attribute_count > 1) {
                PyTuple_SetItem(result, i, parseCFTypeRef(value));
            } else {
                result = parseCFTypeRef(value);
            }
        } else {
            // If any of the requests fail, release memory and raise an exception
            if (attribute_count > 1) Py_DECREF(result);
            if (name_strref != NULL) CFRelease(name_strref);
            if (value != NULL) CFRelease(value);
            handleAXErrors(name_string, error);
            return NULL;
        }
        if (name_strref != NULL) CFRelease(name_strref);
    }
    return result;
}

static PyObject * AccessibleElement_is_alive(AccessibleElement * self, PyObject * args) {
    // Just check to see if the element responds to a basic attribute request
    CFTypeRef value = NULL;
    AXError error = AXUIElementCopyAttributeValue(self->_ref, kAXRoleAttribute, &value);
    if (value != NULL) CFRelease(value);
    
    if (error == kAXErrorInvalidUIElement) Py_RETURN_FALSE;
    else Py_RETURN_TRUE;
}

static PyObject * AccessibleElement_set(AccessibleElement * self, PyObject * args) {
    PyObject * result = NULL;
    // There should be at least two arguments
    Py_ssize_t attribute_count = PyTuple_Size(args);
    if (attribute_count <= 1) {
        PyErr_SetString(PyExc_ValueError, "Not enough arguments.");
        return NULL;
    }
    // The first argument should be a string
    PyObject * name = PyTuple_GetItem(args, (Py_ssize_t) 0);
    if (!name) {
        return NULL; // PyTuple_GetItem will set an Index error.
    }
    char * name_string = NULL;
    CFStringRef name_strref = CFStringFromPyString(name, &name_string);
    if (!name_strref) {
        if (attribute_count > 1) Py_DECREF(result);
        return NULL; // CFStringFromPyString will set an error.
    }

    // Check to see if the attribute can be set at all
    Boolean can_set;
    AXError error = AXUIElementIsAttributeSettable(self->_ref, name_strref, &can_set);

    if (error == kAXErrorSuccess && !can_set) {
        PyErr_SetString(PyExc_ValueError, formattedMessage("The %s attribute cannot be modified.", name_string));
        return NULL;
    } else if (error != kAXErrorSuccess) {
        handleAXErrors(name_string, error);
        return result;
    }

    // Try to figure out what to set
    if (CFStringCompare(name_strref, kAXPositionAttribute, 0) == kCFCompareEqualTo) {
        
        // For position, need a tuple of floats
        PyObject * value = PyTuple_GetItem(args, (Py_ssize_t) 1);
        float pair[2];
        if (!PyTuple_Check(value)) {
            PyErr_SetString(PyExc_ValueError, "Setting AXPosition requires a tuple of exactly two floats.");
            return NULL;
        } else if (PyTuple_Size(value) != 2) {
            PyErr_SetString(PyExc_ValueError, "Setting AXPosition requires a tuple of exactly two floats.");
            return NULL;
        } else if (!PyArg_ParseTuple(value, "ff", &pair[0], &pair[1])) {
            PyErr_SetString(PyExc_ValueError, "Setting AXPosition requires a tuple of exactly two floats.");
            return NULL;
        }
        CGPoint pos = CGPointMake((CGFloat) pair[0], (CGFloat) pair[1]);
        CFTypeRef position = (CFTypeRef) AXValueCreate(kAXValueCGPointType, (const void *) &pos);
        AXError error = AXUIElementSetAttributeValue(self->_ref, kAXPositionAttribute, position);

        result = Py_BuildValue("i", error);
        return result;
    } else if (CFStringCompare(name_strref, kAXSizeAttribute, 0) == kCFCompareEqualTo) {
        
        // For size, need a tuple of floats
        PyObject * value = PyTuple_GetItem(args, (Py_ssize_t) 1);
        float pair[2];
        if (!PyTuple_Check(value)) {
            PyErr_SetString(PyExc_ValueError, "Setting AXSize requires a tuple of exactly two floats.");
            return NULL;
        } else if (PyTuple_Size(value) != 2) {
            PyErr_SetString(PyExc_ValueError, "Setting AXSize requires a tuple of exactly two floats.");
            return NULL;
        } else if (!PyArg_ParseTuple(value, "ff", &pair[0], &pair[1])) {
            PyErr_SetString(PyExc_ValueError, "Setting AXSize requires a tuple of exactly two floats.");
            return NULL;
        }
        CGSize s = CGSizeMake((CGFloat) pair[0], (CGFloat) pair[1]);
        CFTypeRef size = (CFTypeRef) AXValueCreate(kAXValueCGSizeType, (const void *) &s);
        AXError error = AXUIElementSetAttributeValue(self->_ref, kAXSizeAttribute, size);

        result = Py_BuildValue("i", error);
        return result;
    } else if (CFStringCompare(name_strref, kAXHiddenAttribute, 0) == kCFCompareEqualTo) {

        // For hidden, need a bool
        PyObject * value = PyTuple_GetItem(args, (Py_ssize_t) 1);
        if (PyObject_IsTrue(value) == -1) {
            PyErr_SetString(PyExc_ValueError, "Setting AXHidden requires a bool.");
            return NULL;
        }
        
        AXError error;
        if (PyObject_IsTrue(value)) {
            error = AXUIElementSetAttributeValue(self->_ref, kAXHiddenAttribute, kCFBooleanTrue);
        } else {
            error = AXUIElementSetAttributeValue(self->_ref, kAXHiddenAttribute, kCFBooleanFalse);
        }
        result = Py_BuildValue("i", error);
        return result;
    }

    PyErr_SetString(PyExc_NotImplementedError, "Not all attributes can yet be modified.");
    return result;
}

static PyObject * AccessibleElement_set_callback(AccessibleElement * self, PyObject * args) {
    PyObject * callable = NULL;
    
    if (!PyArg_ParseTuple(args, "O", &callable))
        return NULL;

    // The argument should be a callable
    if (!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "The callback must be callable.");
        return NULL;
    }
    Py_XINCREF(callable);
    Py_XDECREF(self->callback);
    self->callback = callable;

    Py_RETURN_NONE;
}

static PyObject * AccessibleElement_set_timeout(AccessibleElement * self, PyObject * args) {
    float timeout;

    if (!PyArg_ParseTuple(args, "f", &timeout))
        return NULL;

    AXError error = AXUIElementSetMessagingTimeout(self->_ref, timeout);
    
    if (error != kAXErrorSuccess) {
        handleAXErrors("timeout", error);
        return NULL;
    } else {
        Py_RETURN_NONE;
    }
}

static PyObject * AccessibleElement_watch(AccessibleElement * self, PyObject * args) {
    if (self->pid == Py_None) {
        PyErr_SetString(PyExc_TypeError, "Must have a PID to watch for notifications.");
        return NULL;
    }

    // The observer needs to be initialized before notifications can be watched
    if (self->_obs == NULL) {
        // Get PID
        pid_t pid;
        AXError error = AXUIElementGetPid(self->_ref, &pid);
        if (error != kAXErrorSuccess) {
            handleAXErrors("pid", error);
            return NULL;
        }

        // Create observer
        AXObserverRef temp = NULL;
        error = AXObserverCreate(pid, NotifcationCallback, &temp);
        if (error != kAXErrorSuccess) {
            handleAXErrors("observer", error);
            return NULL;
        }
        self->_obs = temp;

        // Add the observer to the run loop
        CFRunLoopAddSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(self->_obs), kCFRunLoopDefaultMode);
    }
    
    // Since the method accepts an arbitrary number of strings...
    Py_ssize_t attribute_count = PyTuple_Size(args);
    
    for (int i = 0; i < attribute_count; i++) {
        // The arguments should be strings
        PyObject * name = PyTuple_GetItem(args, (Py_ssize_t) i);
        if (!name) {
            return NULL; // PyTuple_GetItem will set an Index error.
        }
        char * name_string = NULL;
        CFStringRef name_strref = CFStringFromPyString(name, &name_string);
        if (!name_strref) return NULL; // CFStringFromPyString will set an error.

        // Add the notification
        AXError error = AXObserverAddNotification(self->_obs, self->_ref, name_strref, self);
        CFRelease(name_strref);
        
        if (error != kAXErrorSuccess) {
            handleAXErrors(name_string, error);
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

static PyMethodDef AccessibleElement_methods[] = {
    {"keys", (PyCFunction) AccessibleElement_keys, METH_NOARGS, keys_docstring},
    {"count", (PyCFunction) AccessibleElement_count, METH_VARARGS, count_docstring},
    {"get", (PyCFunction) AccessibleElement_get, METH_VARARGS, get_docstring},
    {"set", (PyCFunction) AccessibleElement_set, METH_VARARGS, set_docstring},
    {"set_callback", (PyCFunction) AccessibleElement_set_callback, METH_VARARGS, set_callback_docstring},
    {"set_timeout", (PyCFunction) AccessibleElement_set_timeout, METH_VARARGS, set_timeout_docstring},
    {"can_set", (PyCFunction) AccessibleElement_can_set, METH_VARARGS, can_set_docstring},
    {"is_alive", (PyCFunction) AccessibleElement_is_alive, METH_NOARGS, is_alive_docstring},
    {"watch", (PyCFunction) AccessibleElement_watch, METH_VARARGS, watch_docstring},
    {NULL, NULL, 0, NULL}
};

static PyMemberDef AccessibleElement_members[] = {
    {"pid", T_OBJECT_EX, offsetof(AccessibleElement, pid), READONLY, "The process ID associated with this element, if it has one."},
    {NULL, 0, 0, 0, NULL}
};

static PyTypeObject AccessibleElement_type = {
#if PY_MAJOR_VERSION >= 3
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL) 0, /*ob_size*/
#endif
    "accessibility.AccessibleElement", /*tp_name*/
    sizeof(AccessibleElement), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) AccessibleElement_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    &AccessibleElement_as_sequence, /*tp_as_sequence*/
    &AccessibleElement_as_mapping, /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    AccessibleElement_docstring, /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    (richcmpfunc) &AccessibleElement_richcompare, /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    AccessibleElement_methods, /* tp_methods */
    AccessibleElement_members /* tp_members */
};

/* Module functions implementation
======== */

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9

// Use the prompt system for Mavericks & later
static PyObject * is_enabled(PyObject * self, PyObject * args, PyObject * kwargs) {  
    static char *kwlist [] = {"prompt", NULL};
    int prompt = 1;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &prompt))
        return NULL;

    if (prompt == 1) {
        const void * keys[] = { kAXTrustedCheckOptionPrompt };
        const void * values[] = { kCFBooleanTrue };
        if (AXIsProcessTrustedWithOptions(CFDictionaryCreate(NULL, keys, values, 1, NULL, NULL))){
            Py_RETURN_TRUE;
        } else {
            Py_RETURN_FALSE;
        }
    } else {
        if (AXIsProcessTrustedWithOptions(NULL)) {
            Py_RETURN_TRUE;
        } else {
            Py_RETURN_FALSE;
        }
    }
#else

// Use the old system otherwise
static PyObject * is_enabled(PyObject * self) {
    if (AXAPIEnabled()) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
#endif
}

static PyObject * is_trusted(PyObject * self) {
    if (AXIsProcessTrusted()) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static AccessibleElement * create_application_ref(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char *kwlist [] = {"pid", NULL};
    pid_t pid;
 
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pid))
        return NULL;

    AXUIElementRef ref = AXUIElementCreateApplication(pid);
    
    // Just check to see if the element responds to a basic attribute request
    // This might cause problems in some extreme cases, but it also alleviates
    // the common mistake of passing in a PID with a typo, only to have a call
    // fail later.
    CFTypeRef value = NULL;
    AXError error = AXUIElementCopyAttributeValue(ref, kAXRoleAttribute, &value);
    if (value != NULL) CFRelease(value);
    
    if (error == kAXErrorAPIDisabled) {
        PyErr_SetString(APIDisabledError, "This PID does not respond to Accessibility requests -- perhaps Accessibility is not enabled on the system?");
        return NULL;
    } else if (error != kAXErrorSuccess) {
        PyErr_SetString(PyExc_ValueError, "This PID does not seem to be associated with a valid process.");
        return NULL;
    }
    
    return elementWithRef(&ref);
}

static AccessibleElement * create_systemwide_ref(PyObject * self, PyObject * args) {
    AXUIElementRef ref = AXUIElementCreateSystemWide();
    return elementWithRef(&ref);
}

static AccessibleElement * element_at_position(PyObject * self, PyObject * args, PyObject * kwargs) {
    AccessibleElement * result = NULL;
    AccessibleElement * parent = NULL;
    float x, y;

    static char *kwlist [] = {"x", "y", "element", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ff|O", kwlist, &x, &y, &parent))
        return NULL;
    
    AXUIElementRef ref = (parent != NULL) ? parent->_ref : AXUIElementCreateSystemWide();
    AXUIElementRef element;
    AXError error = AXUIElementCopyElementAtPosition (ref, x, y, &element);

    if (error == kAXErrorSuccess) {
        result = elementWithRef(&element);
    } else {
        handleAXErrors("(element at position)", error);
    }

    if (ref != NULL && parent == NULL) CFRelease(ref);
    return result;
}

/* Module definition
======== */
 
static PyMethodDef methods[] = {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
    {"is_enabled", (PyCFunction) is_enabled, METH_VARARGS|METH_KEYWORDS, is_enabled_docstring},
#else
    {"is_enabled", (PyCFunction) is_enabled, METH_NOARGS, "is_enabled()\n\nCheck if accessibility has been enabled on the system."},
#endif
    {"is_trusted", (PyCFunction) is_trusted, METH_NOARGS, "is_trusted()\n\nCheck if this application is a trusted process."},
    {"create_application_ref", (PyCFunction) create_application_ref, METH_VARARGS|METH_KEYWORDS, "create_application_ref(pid)\n\nCreate an accessibile application with the given PID."},
    {"create_systemwide_ref", (PyCFunction) create_systemwide_ref, METH_NOARGS, "create_systemwide_ref()\n\nGet a system-wide accessible element reference."},
    {"element_at_position", (PyCFunction) element_at_position, METH_VARARGS|METH_KEYWORDS, element_at_position_docstring},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "accessibility",
    module_docstring,
    (Py_ssize_t) -1,
    methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_accessibility(void) {
    PyObject * m = PyModule_Create(&module);
#else
PyMODINIT_FUNC initaccessibility(void) {
    PyObject * m = Py_InitModule3("accessibility", methods, module_docstring);
    if (m == NULL) {
        return;
    }
#endif

    AccessibleElement_type.tp_new = PyType_GenericNew;
#if PY_MAJOR_VERSION >= 3
    if (PyType_Ready(&AccessibleElement_type) < 0) return m;
#else
    if (PyType_Ready(&AccessibleElement_type) < 0) return;
#endif

    Py_INCREF(&AccessibleElement_type);
    PyModule_AddObject(m, "AccessibleElement", (PyObject *) &AccessibleElement_type);
    PyModule_AddObject(m, "DEFAULT_TIMEOUT", PyFloat_FromDouble(0.0));
#if PY_MAJOR_VERSION >= 3
    PyModule_AddObject(m, "__author__", PyBytes_FromString("Aaron Jacobs <atheriel@gmail.com>"));
    PyModule_AddObject(m, "__version__", PyBytes_FromString("0.3.2"));
#else
    PyModule_AddObject(m, "__author__", PyString_FromString("Aaron Jacobs <atheriel@gmail.com>"));
    PyModule_AddObject(m, "__version__", PyString_FromString("0.3.2"));
#endif

    InvalidUIElementError = PyErr_NewExceptionWithDoc("accessibility.InvalidUIElementError", InvalidUIElementError_docstring, PyExc_ValueError, NULL);
    PyModule_AddObject(m, "InvalidUIElementError", InvalidUIElementError);

    APIDisabledError = PyErr_NewExceptionWithDoc("accessibility.APIDisabledError", APIDisabledError_docstring, PyExc_Exception, NULL);
    PyModule_AddObject(m, "APIDisabledError", APIDisabledError);

    if (!PyEval_ThreadsInitialized()) {
        PyEval_InitThreads();
    }

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}

/* ========
    Private Member Implementations
======== */

static AccessibleElement * elementWithRef(AXUIElementRef * ref) {
    AccessibleElement * self;
    self = PyObject_New(AccessibleElement, &AccessibleElement_type);
    if (self == NULL)
        return NULL;
    self->_ref = *ref;
    self->_obs = NULL;

    // Sets the pid, which should never change
    pid_t pid;
    AXError error = AXUIElementGetPid(*ref, &pid);
    if (error == kAXErrorSuccess) {
#if PY_MAJOR_VERSION >= 3
        self->pid = PyLong_FromLong(pid);
#else
        self->pid = PyInt_FromLong(pid);
#endif
    } else {
        self->pid = Py_None;
        Py_INCREF(Py_None);
    }

    // Set the callback to None for now
    self->callback = Py_None;
    Py_INCREF(Py_None);
    
    return self;
}

static PyObject * parseCFTypeRef(const CFTypeRef value) {
    PyObject * result = NULL;
    
    // Check what type the return is
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        
        // The value is a CFStringRef, so try to decode it to a char *
        CFIndex length = CFStringGetLength(value);
        if (length == 0) { // Empty string
            result = Py_None;
        } else {
            char * buffer = CFStringGetCStringPtr(value, kCFStringEncodingUTF8); // Fast way
            if (!buffer) {
                // Slow way
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                buffer = (char *) malloc(maxSize);
                if (CFStringGetCString(value, buffer, maxSize, kCFStringEncodingUTF8)) {
                    result = Py_BuildValue("s", buffer);
                } else {
                    PyErr_SetString(PyExc_TypeError, "The referenced string representation could not be parsed.");
                }
            } else {
                result = Py_BuildValue("s", buffer);
            }
        }

    } else if (CFGetTypeID(value) == CFBooleanGetTypeID()) {

        // The value is a boolean, so get its value and return True/False
        if (CFBooleanGetValue(value)) {
            result = Py_True;
        } else {
            result = Py_False;
        }

    } else if (CFGetTypeID(value) == AXUIElementGetTypeID()) {

        // The value is another AXUIElementRef (probably a window...)
        result = (PyObject *) elementWithRef((AXUIElementRef *) &value);

    } else if (CFGetTypeID(value) == AXValueGetTypeID()) {

        // The value is one of the AXValues
        AXValueType value_type = AXValueGetType(value);
        if (value_type == kAXValueCGPointType) {
            
            // It's a point
            CGPoint point;
            if (AXValueGetValue(value, kAXValueCGPointType, (void *) &point)) {
                result = Py_BuildValue("ff", point.x, point.y);
            } else {
                PyErr_SetString(PyExc_ValueError, "The value cannot be retrieved.");
                result = NULL;
            }
        } else if (value_type == kAXValueCGSizeType) {
            
            // It's a size
            CGSize size;
            if (AXValueGetValue(value, kAXValueCGSizeType, (void *) &size)) {
                result = Py_BuildValue("ff", size.width, size.height);
            } else {
                PyErr_SetString(PyExc_ValueError, "The value cannot be retrieved.");
                result = NULL;
            }
        } else if (value_type == kAXValueCGRectType) {
            
            // It's a rect
            PyErr_SetString(PyExc_NotImplementedError, "Not all AXValue can yet be parsed.");
            result = NULL;
        } else {
            
            PyErr_SetString(PyExc_NotImplementedError, "Not all AXValue can yet be parsed.");
            result = NULL;
        }

    } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {

        // An array
        if (CFArrayGetCount(value) <= 0) {
            // Empty array
            result = Py_None;
        } else {
            // It's an array... gonna have to do this recursively...
            Py_ssize_t size = CFArrayGetCount(value);
            PyObject * list = PyList_New(size);
            if (!list) {
                // Shouldn't happen
                PyErr_SetString(PyExc_TypeError, "A list could not be created for the reference.");
            } else {
                for (CFIndex i = 0; i < CFArrayGetCount(value); i++) {
                    PyObject * element = parseCFTypeRef(CFArrayGetValueAtIndex(value, i));
                    PyList_SetItem(list, i, element);
                }
                result = list;
            }
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "Unknown CFTypeRef type.");
    }

    return result;
}

static void handleAXErrors(const char * attribute_name, AXError error) {
    switch(error) {
        case kAXErrorCannotComplete:
            PyErr_SetString(PyExc_Exception, formattedMessage("The request for %s could not be completed (perhaps the application is not responding?).", attribute_name));
            break;

        case kAXErrorAttributeUnsupported:
            PyErr_SetString(PyExc_KeyError, formattedMessage("This element does not possess the attribute %s.", attribute_name));
            break;

        case kAXErrorIllegalArgument:
            PyErr_SetString(PyExc_ValueError, formattedMessage("Invalid argument. This is probably caused by a faulty AccessibleElement."));
            break;

        case kAXErrorNoValue:
            PyErr_SetString(PyExc_ValueError, formattedMessage("The attribute %s has no value.", attribute_name));
            break;

        case kAXErrorInvalidUIElement:
            PyErr_SetString(InvalidUIElementError, formattedMessage("This element is no longer valid (perhaps the application has been closed?)."));
            break;

        case kAXErrorNotImplemented:
            PyErr_SetString(PyExc_NotImplementedError, formattedMessage("This element does not implement the Accessibility API for the attribute %s.", attribute_name));
            break;

        case kAXErrorAPIDisabled:
            PyErr_SetString(APIDisabledError, formattedMessage("This element does not respond to Accessibility requests -- perhaps Accessibility is not enabled on the system?"));
            break;

        default:
            PyErr_SetString(PyExc_Exception, formattedMessage("Error %ld encountered with attibute %s.", error, attribute_name));
            break;
    }
}

static void NotifcationCallback(AXObserverRef obs, AXUIElementRef ref, CFStringRef notification, void * element) {
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    AccessibleElement * elem = (AccessibleElement *) element;
    
    if (elem->callback != Py_None) {
        PyObject * args = Py_BuildValue("()");
        PyObject * kwargs = PyDict_New();
#if PY_MAJOR_VERSION >= 3
        PyObject * element_key = PyBytes_FromString("element");
        PyObject * notification_key = PyBytes_FromString("notification");
#else
        PyObject * element_key = PyString_FromString("element");
        PyObject * notification_key = PyString_FromString("notification");
#endif
        if (PyDict_SetItem(kwargs, element_key, elem) == -1) {
            PyErr_SetString(PyExc_Warning, "The element could not be passed to the callback.");
        }

        // The notification is a CFStringRef, so try to decode it to a char *
        CFIndex length = CFStringGetLength(notification);
        if (length == 0) {
            if (PyDict_SetItem(kwargs, notification_key, Py_None) == -1) {
                PyErr_SetString(PyExc_Warning, "The notification type passed to the callback could not be identified.");
            } else {
                Py_INCREF(Py_None);
            }
        } else { // Non-empty string
            char * buffer = CFStringGetCStringPtr(notification, kCFStringEncodingUTF8); // Fast way
            if (!buffer) {
                // Slow way
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                buffer = (char *) malloc(maxSize);
                if (CFStringGetCString(notification, buffer, maxSize, kCFStringEncodingUTF8)) {
#if PY_MAJOR_VERSION >= 3
                    PyObject * buffer_value = PyBytes_FromString(buffer);
#else
                    PyObject * buffer_value = PyString_FromString(buffer);
#endif
                    if (PyDict_SetItem(kwargs, notification_key, buffer_value) == -1) {
                        PyErr_SetString(PyExc_Warning, "The callback could not be constructed.");
                    }
                } else {
                    if (PyDict_SetItem(kwargs, notification_key, Py_None) == -1) {
                        PyErr_SetString(PyExc_Warning, "The notification type passed to the callback could not be identified.");
                    } else {
                        Py_INCREF(Py_None);
                    }
                }
            } else {
#if PY_MAJOR_VERSION >= 3
                PyObject * buffer_value = PyBytes_FromString(buffer);
#else
                PyObject * buffer_value = PyString_FromString(buffer);
#endif
                if (PyDict_SetItem(kwargs, notification_key, buffer_value) == -1) {
                    PyErr_SetString(PyExc_Warning, "The callback could not be constructed.");
                }
            }
        }
        PyObject * result = PyObject_Call(elem->callback, args, kwargs);
        if (result == NULL) {
            PyErr_SetString(PyExc_Warning, "The callback could not be completed.");
        }
        Py_XDECREF(args);
        Py_XDECREF(kwargs);
        Py_XDECREF(result); 
    } else {
        PyErr_SetString(PyExc_Exception, "No callback is defined to handle notifications. Be sure to make use of myelement.set_callback(func).");
    }

    if (PyErr_Occurred() != NULL) {
        PyErr_Print();
    }

    PyGILState_Release(gstate);
}
