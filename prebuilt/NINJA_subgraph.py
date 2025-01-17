"""
prebuilt/NINJA_subgraph.py
"""

from __future__ import print_function

from build import ninja_lib
from build.ninja_lib import log

_ = log


def NinjaGraph(ru):

    ru.comment('Generated by %s' % __name__)

    # These files are checked in.  See prebuilt/translate.sh includes

    ru.cc_library(
        '//prebuilt/asdl/runtime.mycpp',
        srcs=['prebuilt/asdl/runtime.mycpp.cc'],
        deps=[
            '//asdl/hnode.asdl',
            # Should //cpp/qsn exist?
        ])

    ru.cc_library(
        '//prebuilt/core/error.mycpp',
        srcs=['prebuilt/core/error.mycpp.cc'],
        deps=[
            '//asdl/hnode.asdl',
            '//frontend/syntax.asdl',
            # Should //cpp/qsn exist?
        ])

    ru.cc_library(
        '//prebuilt/frontend/args.mycpp',
        srcs=['prebuilt/frontend/args.mycpp.cc'],
        deps=[
            '//asdl/hnode.asdl',
            '//core/runtime.asdl',
            '//frontend/syntax.asdl',
            '//cpp/frontend_flag_spec',
            # Should //cpp/qsn exist?
        ])
