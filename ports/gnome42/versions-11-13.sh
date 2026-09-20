#!/bin/sh
# GNOME 42-era dependency versions used by this BlockOS port layer.
DconfVersion=0.40.0
GVfsVersion=1.50.0
GjsVersion=1.72.0
# GJS 1.72 needs a supported SpiderMonkey/mozjs development package.
# Override MOZJS_PKG if your port tree uses a different package name.
MOZJS_PKG=${MOZJS_PKG:-mozjs-91}
export DconfVersion GVfsVersion GjsVersion MOZJS_PKG
