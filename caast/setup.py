from setuptools import setup, Extension
from Cython.Build import cythonize
import os

# Define the path to the parent directory (where aast.c and aast.h live)
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Create the Extension definition
aast_extension = Extension(
    name="caast",                     # The name of the compiled Python module
    sources=[
        "aast.pyx",                   # The Cython bridge file
        os.path.join(ROOT_DIR, "aast.c") # The core C engine
    ],
    include_dirs=[ROOT_DIR],          # Tell the C compiler where to find aast.h and uthash.h
    libraries=["crypto"],             # Link against OpenSSL (libcrypto)
    extra_compile_args=["-std=c11", "-O3", "-D_GNU_SOURCE"], # Aggressive optimization and POSIX unlocking
)

setup(
    name="caast",
    version="1.0.0",
    description="Python Cython Bridge for the Accretive-Abstract-State-Tree (A-AST)",
    ext_modules=cythonize(
        [aast_extension],
        compiler_directives={'language_level': "3"} # Enforce Python 3 semantics
    )
)
