# Our binary is produced here
bin_PROGRAMS = sim
sim_SOURCES = main.c
# main binary will be installed into the default directory for "bin"s (hence we used the bin
# prefix
noinst_LIBRARIES = libcache.a libdebug.a
libcache_a_SOURCES = cache.c
libdebug_a_SOURCES = debug.c
# noinst_ : assures that this library is not installed to the user's computer
# libBLAH.a is a *nix convention. If you noticed, we replaced the . with _ in SOURCES line.
sim_LDADD = libcache.a libdebug.a
# LDADD specifies which libraries to compile into "sim". 
# We have two libraries (libcache and libdebug) from this directory.
