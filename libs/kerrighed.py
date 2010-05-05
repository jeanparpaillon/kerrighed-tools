#
# kerrighed.py - A Python interface to libkerrighed
#
# Copyright (c) 2009 Kerlabs
# Author: Jean Parpaillon <jean.parpaillon@kerlabs.com>
#
LIBKERRIGHED_VERSION = 2

import os
import ctypes
from ctypes import *
libkerrighed_soname = "libkerrighed.so.%i" % LIBKERRIGHED_VERSION
try:
    if 'get_errno' in dir(ctypes):
        get_errno = ctypes.get_errno
        libkerrighed = CDLL(libkerrighed_soname, use_errno=True)
    else:
        get_errno = None
        libkerrighed = CDLL(libkerrighed_soname)
except OSError, ose:
    print "Can not import " + libkerrighed_soname
    raise SystemExit(1)

class kerrighed_error(Exception):
    pass

class krg_error_handler(object):
    def __init__(self, func):
        super(krg_error_handler, self).__init__()
        self.func = func

    def __call__(self, value):
        if value==-1:
            if get_errno is not None:
                msg = "error in %s: %s" % (self.func.__name___)
            else:
                msg = "error in %s: %s" % (self.func.__name__, os.strerror(get_errno()))
            raise kerrighed_error(msg)
        else:
            return value

#
# From hotplug.h
#
libkerrighed.krg_hotplug_init()

kerrighed_max_nodes = c_int.in_dll(libkerrighed, "kerrighed_max_nodes")
kerrighed_max_clusters = c_int.in_dll(libkerrighed, "kerrighed_max_clusters")

class krg_nodes_t(Structure):
    _fields_ = [("nodes", c_char_p)]
krg_nodes_ptr_t = POINTER(krg_nodes_t)

class krg_clusters_t(Structure):
    _fields_ = [("clusters", c_char_p)]
krg_clusters_ptr_t = POINTER(krg_clusters_t)

class krg_node_set_t(Structure):
    _fields_ = [("subclusterid", c_int),
                ("v", c_char_p)]
krg_node_set_ptr_t = POINTER(krg_node_set_t)

libkerrighed.krg_nodes_create.restype = krg_nodes_ptr_t
libkerrighed.krg_nodes_num.restype = krg_error_handler(libkerrighed.krg_nodes_num)
libkerrighed.krg_nodes_num_online.restype = krg_error_handler(libkerrighed.krg_nodes_num_online)
libkerrighed.krg_nodes_num_possible.restype = krg_error_handler(libkerrighed.krg_nodes_num_possible)
libkerrighed.krg_nodes_num_present.restype = krg_error_handler(libkerrighed.krg_nodes_num_present)
libkerrighed.krg_nodes_is.restype = krg_error_handler(libkerrighed.krg_nodes_is)
libkerrighed.krg_nodes_is_online.restype = krg_error_handler(libkerrighed.krg_nodes_is_online)
libkerrighed.krg_nodes_is_possible.restype = krg_error_handler(libkerrighed.krg_nodes_is_possible)
libkerrighed.krg_nodes_is_present.restype = krg_error_handler(libkerrighed.krg_nodes_is_present)
libkerrighed.krg_nodes_get.restype = krg_node_set_ptr_t
libkerrighed.krg_nodes_get_online.restype = krg_node_set_ptr_t
libkerrighed.krg_nodes_get_possible.restype = krg_node_set_ptr_t
libkerrighed.krg_nodes_get_present.restype = krg_node_set_ptr_t
libkerrighed.krg_nodes_getnode.restype = c_int
libkerrighed.krg_nodes_nextnode.restype = c_int

libkerrighed.krg_clusters_create.restype = krg_clusters_ptr_t
libkerrighed.krg_clusters_is_up.restype = krg_error_handler(libkerrighed.krg_clusters_is_up)

libkerrighed.krg_node_set_create.restype = krg_node_set_ptr_t
libkerrighed.krg_node_set_add.restype = krg_error_handler(libkerrighed.krg_node_set_add)
libkerrighed.krg_node_set_remove.restype = krg_error_handler(libkerrighed.krg_node_set_remove)
libkerrighed.krg_node_set_contains.restype = krg_error_handler(libkerrighed.krg_node_set_contains)
libkerrighed.krg_node_set_weight.restype = krg_error_handler(libkerrighed.krg_node_set_weight)
libkerrighed.krg_node_set_next.restype = c_int

libkerrighed.krg_status_str.restype = c_char_p
libkerrighed.krg_nodes_status.restype = krg_nodes_ptr_t
libkerrighed.krg_cluster_status.restype = krg_clusters_ptr_t
libkerrighed.krg_cluster_shutdown.restype = krg_error_handler(libkerrighed.krg_cluster_shutdown)
libkerrighed.krg_cluster_reboot.restype = krg_error_handler(libkerrighed.krg_cluster_reboot)

libkerrighed.krg_nodes_add.restype = krg_error_handler(libkerrighed.krg_nodes_add)
libkerrighed.krg_nodes_remove.restype = krg_error_handler(libkerrighed.krg_nodes_remove)

class krg_nodes(object):

    def __init__(self, _c=None):
        self.cur = -1
        if not _c:
            _c = libkerrighed.krg_nodes_create()
            if not _c:
                raise krg_error_handler("krg_nodes_create returned NULL")
        self.c = _c

    def __del__(self):
        libkerrighed.krg_nodes_destroy(self.c)

    def getnode(self, node):
        ret = libkerrighed.krg_nodes_getnode(self.c, node)
        if ret>=0:
            return ret
        else:
            raise IndexError

    def __getitem__(self, node):
        return self.getnode(node)

    def __setitem__(self, node, status):
        raise NotImplemented

    def num_possible(self):
        return libkerrighed.krg_nodes_num_possible(self.c)

    def num_present(self):
        return libkerrighed.krg_nodes_num_present(self.c)

    def num_online(self):
        return libkerrighed.krg_nodes_num_online(self.c)

    def is_possible(self, node):
        return libkerrighed.krg_nodes_is_possible(self.c, node)==1

    def is_present(self, node):
        return libkerrighed.krg_nodes_is_present(self.c, node)==1

    def is_online(self, node):
        return libkerrighed.krg_nodes_is_online(self.c, node)==1

    def get_possible(self):
        ret = libkerrighed.krg_nodes_get_possible(self.c)
        if ret is None:
            raise kerrighed_error("error in %s: %s" % (self.get_possible.__name__,
                                                       os.strerror(libkerrighed.krg_get_status)))
        return krg_node_set(ret)

    def get_present(self):
        ret = libkerrighed.krg_nodes_get_present(self.c)
        if ret is None:
            raise kerrighed_error("error in %s: %s" % (self.get_present.__name__,
                                                       os.strerror(libkerrighed.krg_get_status)))
        return krg_node_set(ret)

    def get_online(self):
        ret = libkerrighed.krg_nodes_get_online(self.c)
        if ret is None:
            raise kerrighed_error("error in %s: %s" % (self.get_online.__name__,
                                                       os.strerror(libkerrighed.krg_get_status)))
        return krg_node_set(ret)

    def __iter__(self):
        return self

    def next(self):
        self.cur = libkerrighed.krg_nodes_nextnode(self.c, self.cur)
        if self.cur>=0:
            return self.cur
        else:
            raise StopIteration

    def __str__(self):
        return "\n".join(map(lambda n: "%d:%s" % (n, krg_status_str(self[n])),
                             self))

    def __repr__(self):
        return "[" + ",".join(map(lambda n: "%d:%d" % (n, self[n]),
                                  self)) + "]"

class krg_clusters(object):

    def __init__(self, _c=None):
        if not _c:
            _c = libkerrighed.krg_clusters_create()
            if not _c:
                raise krg_error_handler("krg_clusters_create returned NULL")
        self.c = _c

    def __del__(self):
        libkerrighed.krg_clusters_destroy(self.c)

    def is_up(self, n=0):
        return libkerrighed.krg_clusters_is_up(self.c, n)==1

class krg_node_set(object):
    """
    Implements Python set
    """
    def __init__(self, _c=None):
        self.cur = -1
        self.pop_cur = -1
        if not _c:
            _c = libkerrighed.krg_node_set_create()
            if not _c:
                raise krg_error_handler("krg_node_set_create returned NULL")
        self.c = _c

    def __del__(self):
        libkerrighed.krg_node_set_destroy(self.c)

    def add(self, n):
        return libkerrighed.krg_node_set_add(self.c, n)==1

    def discard(self, n):
        return libkerrighed.krg_node_set_remove(self.c, n)==1

    def contains(self, n):
        return libkerrighed.krg_node_set_contains(self.c, n)==1

    def __contains__(self, n):
        return self.contains(n)

    def weight(self):
        return libkerrighed.krg_node_set_weight(self.c)

    def __len__(self):
        return self.weight()

    def next(self):
        self.cur = libkerrighed.krg_node_set_next(self.c, self.cur)
        if self.cur>=0:
            return self.cur
        else:
            raise StopIteration

    def __iter__(self):
        return self

    def remove(self, n):
        if self.contains(n):
            self.discard(n)
        else:
            raise KeyError

    def pop(self):
        self.pop_cur = libkerrighed.krg_node_set_next(self.c, self.pop_cur)
        if self.pop_cur>=0:
            self.discard(self.pop_cur)
            return self.pop_cur
        else:
            raise KeyError

    def clear(self):
        libkerrighed.krg_node_set_clear(self.c)

    def __str__(self):
        return self.__repr__()

    def __repr__(self):
        return "[" + ",".join(map(lambda n: "%d" % (n, ),
                                  self)) + "]"

def krg_status_str(s):
    """
    return: name of the given status
    """
    return libkerrighed.krg_status_str(s)

def krg_nodes_status():
    """
    return: krg_nodes object
    """
    return krg_nodes(libkerrighed.krg_nodes_status())

def krg_cluster_status():
    """
    return: krg_clusters object
    """
    return krg_clusters(libkerrighed.krg_cluster_status())

def krg_cluster_shutdown(subclusterid=0):
    """
    subclusterid: int
    """
    libkerrighed.krg_cluster_shutdown(subclusterid)

def krg_cluster_reboot(subclusterid=0):
    """
    subclusterid: int
    """
    libkerrighed.krg_cluster_reboot(subclusterid)

def krg_nodes_add(node_set):
    """
    """
    libkerrighed.krg_nodes_add(node_set)

def krg_nodes_remove(node_set):
    """
    """
    libkerrighed.krg_nodes_remove(node_set)
