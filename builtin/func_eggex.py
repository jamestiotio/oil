#!/usr/bin/env python2
"""
func_eggex.py
"""
from __future__ import print_function

from _devbuild.gen.value_asdl import value, value_t
from core import state
from core import vm
from frontend import typed_args


class Match(vm._Callable):
    """
    _match(0) or _match():   get the whole match _match(1) ..

    _match(N):  submatch
    """

    def __init__(self, mem):
        # type: (state.Mem) -> None
        vm._Callable.__init__(self)
        self.mem = mem

    def Call(self, rd):
        # type: (typed_args.Reader) -> value_t
        n = rd.OptionalInt(default_=0)

        # TODO: Support strings
        s = self.mem.GetMatch(n)
        # YSH code doesn't deal well with exceptions!
        #if s is None:
        #  raise IndexError('No such group')
        if s is not None:
            return value.Str(s)

        return value.Null


class Start(vm._Callable):
    """Same signature as _match(), but for start positions."""

    def __init__(self, mem):
        # type: (state.Mem) -> None
        vm._Callable.__init__(self)
        self.mem = mem

    def Call(self, rd):
        # type: (typed_args.Reader) -> value_t
        raise NotImplementedError('_start')


class End(vm._Callable):
    """Same signature as _match(), but for end positions."""

    def __init__(self, mem):
        # type: (state.Mem) -> None
        vm._Callable.__init__(self)
        self.mem = mem

    def Call(self, rd):
        # type: (typed_args.Reader) -> value_t
        raise NotImplementedError('_end')


# vim: sw=4
