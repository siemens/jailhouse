# pyjailhouse

## Using source-tree version of pyjailhouse

Python scripts in the **source tree** (i.e. as opposed to python scripts in an
installation directory) that import pyjailhouse automatically import
pyjailhouse from the jailhouse root directory. This is achieved by setting
`sys.path [0]` to be the absolute path of the root directory, just before
we import pyjailhouse (or something from it):

`sys.path[0] = os.path.dirname(os.path.abspath(__file__)) + "[rel. path]"`
`from pyjailhouse.cell import JailhouseCell`

Where, `[rel.path]` is the relative path from the directory containing the
running script to the root directory. When we install any python script that
uses pyjailhouse, we remove `sys.path[0] = os.path.dirname(...` from the
installed scripts, leaving python to import pyjailhouse from where pip
installed it.

As a usage example, consider the following directory structure for jailhouse:

`|jailhouse/`
`|_____> foo/`
`|___________> bar/`
`|_______________> __init__.py`
`|_______________> baz.py`
`|___________> boz.py`
`|_____> moo.py`
`|_____> pyjailhouse/`

baz.py would have:
`sys.path[0] = os.path.dirname(os.path.abspath(__file__)) + "/../.."`
`import pyjailhouse`

boz.py would have:
`sys.path[0] = os.path.dirname(os.path.abspath(__file__)) + "/.."`
`import pyjailhouse.something`

moo.py would have:
`sys.path[0] = os.path.dirname(os.path.abspath(__file__))`
`from pyjailhouse import other`

Note that any attempt to import a module after writing to sys.path[0], such
that the module is located in the directory containing the script that
initially invoked the python interpreter (with a shebang), will fail.  Read
about sys.path[0] in any python language reference to understand why this is so.
For the example, the following wouldn't work inside of boz.py:

`sys.path[0] = os.path.dirname(os.path.abspath(__file__)) + "/.."`
`import pyjailhouse.something`
`import bar # Will fail`

**Boilerplate code, with sys.path[0] warning**

`
# Imports from directory containing this must be done before the following
sys.path[0] = os.path.dirname(os.path.abspath(__file__)) + ""
from pyjailhouse import *
`
