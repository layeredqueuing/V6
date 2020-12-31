# Process this file with autoconf to produce a configure script.
# $Id: configure.ac 14310 2020-12-31 17:16:57Z greg $
# ------------------------------------------------------------------------
AC_INIT(lqns, 6.1, greg@sce.carleton.ca)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE
LT_INIT
# ------------------------------------------------------------------------
AC_CHECK_FILE([/usr/src/redhat],
    [RPMDIR=/usr/src/redhat],
    AC_CHECK_FILE([$HOME/usr/rpm],
	[RPMDIR=$HOME/usr/rpm],
	AC_CHECK_FILE([$HOME/rpmbuild],[RPMDIR=$HOME/rpmbuild])))
AC_SUBST(RPMDIR)
# ------------------------------------------------------------------------
AC_CONFIG_SUBDIRS([lqx lqiolib libmva lqns srvndiff parasol lqsim lqn2ps lqngen doc])
AC_PROG_MAKE_SET
AC_CONFIG_FILES([Makefile xml/Makefile dist/Makefile dist/examples/Makefile])
AC_OUTPUT
