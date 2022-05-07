#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.65)
AC_INIT([htop],[2.3.0],[msl0000023508@gmail.com])

AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Required by hwloc scripts
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11])

# Checks for programs.
# ----------------------------------------------------------------------
AC_PROG_CC_C99
AM_PROG_CC_C_O

# Required by hwloc scripts
AC_USE_SYSTEM_EXTENSIONS

# Checks for platform.
# ----------------------------------------------------------------------
case "$target_os" in
linux*)
   my_htop_platform=linux
   ;;
freebsd*|kfreebsd*)
   my_htop_platform=freebsd
   ;;
openbsd*)
   my_htop_platform=openbsd
   ;;
dragonfly*)
   my_htop_platform=dragonflybsd
   ;;
darwin*)
   my_htop_platform=darwin
   ;;
solaris*)
   my_htop_platform=solaris
   ;;
aix*|os400*)
   my_htop_platform=aix
   ;;
interix*)
   my_htop_platform=interix
   ;;
cygwin)
   my_htop_platform=cygwin
   ;;
*)
   my_htop_platform=unsupported
   ;;
esac

# Checks for libraries.
# ----------------------------------------------------------------------
AC_CHECK_LIB([m], [ceil], [], [missing_libraries="$missing_libraries libm"])

# Checks for header files.
# ----------------------------------------------------------------------
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h strings.h sys/param.h sys/time.h unistd.h],[:],[
  missing_headers="$missing_headers $ac_header"
])
AC_CHECK_HEADERS([execinfo.h err.h getopt.h],[:],[:])

AC_HEADER_MAJOR
dnl glibc 2.25 deprecates 'major' and 'minor' in <sys/types.h> and requires to
dnl include <sys/sysmacros.h>. However the logic in AC_HEADER_MAJOR has not yet
dnl been updated in Autoconf 2.69, so use a workaround:
m4_version_prereq([2.70], [],
[if test "x$ac_cv_header_sys_mkdev_h" = xno; then
   AC_CHECK_HEADER(sys/sysmacros.h, [AC_DEFINE(MAJOR_IN_SYSMACROS, 1,
      [Define to 1 if `major', `minor', and `makedev' are declared in <sys/sysmacros.h>.])])
fi])

# Checks for typedefs, structures, and compiler characteristics.
# ----------------------------------------------------------------------
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_UID_T

AC_CHECK_TYPE([struct utmpx],
	[AC_DEFINE(HAVE_UTMPX, 1, Define if 'struct utmpx' is available from utmpx.h)],
	[], [#include <utmpx.h>])

# Checks for library functions and compiler features.
# ----------------------------------------------------------------------
AC_FUNC_CLOSEDIR_VOID
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove strncasecmp strcasestr strstr strdup nanosleep lround getopt_long])

save_cflags="${CFLAGS}"
CFLAGS="$CFLAGS -Wextra"
AC_MSG_CHECKING([if compiler supports -Wextra])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],[
   wextra_flag=-Wextra
   AC_MSG_RESULT([yes])
],[
   wextra_flag=
   AC_MSG_RESULT([no])
])
CFLAGS="$save_cflags"
AC_SUBST(wextra_flag)

AC_ARG_WITH([backtrace],
	[AS_HELP_STRING([--with-backtrace], [use backtrace(3) in case of crashing [auto detect]])],
	[with_backtrace="$withval"], [
		case "$target_os" in
			*-gnu*)
				with_backtrace=yes
				;;
			freebsd*)
				# FreeBSD (not GNU/kFreeBSD) backtrace(3) known to produce useless information
				with_backtrace=no
				;;
			darwin*)
				# No useful information, like FreeBSD
				with_backtrace=no
				;;
			openbsd*)
				# Seems broken
				with_backtrace=no
				;;
			*)
				with_backtrace=check
				;;
		esac
	])

# Try to export all symbols if the backtrace(3) GNU extension is expected to use
if test "x$with_backtrace" != xno && test "x$ac_cv_header_execinfo_h" = xyes; then
	AC_CHECK_FUNCS([backtrace], [], [
		AC_CHECK_LIB([execinfo], [backtrace], [
			ac_cv_func_backtrace=yes
			AC_DEFINE(HAVE_BACKTRACE)
			LIBS="$LIBS -lexecinfo"
		])
	])
	if test "x$ac_cv_func_backtrace" = xyes; then
		save_ldflags="$LDFLAGS"
		accepted_flag=
		AC_MSG_CHECKING([for option to export all symbols])
		case "$target_os" in
			darwin*)
				# Can be mislead to '-e xport_dynamic' for other ld(1)
				extra_option=-Wl,-export_dynamic
				;;
			solaris*)
				extra_option=
				;;
			*)
				extra_option=-rdynamic
				;;
		esac
		for o in -Wl,--export-dynamic -Wl,-E $extra_option; do
			LDFLAGS="$save_ldflags $o"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])], [
				accepted_flag="$o"
				break
			], [])
		done
		if test "x$accepted_flag" = x; then
			LDFLAGS="$save_ldflags"
			AC_MSG_RESULT([unsupported])
		else
			AC_MSG_RESULT([$accepted_flag])
		fi
	fi
fi

# Checks for features and flags.
# ----------------------------------------------------------------------
PROCDIR=/proc

AC_ARG_ENABLE(proc, [AS_HELP_STRING([--enable-proc], [use Linux-compatible proc filesystem])], enable_proc="yes", enable_proc="no")
if test "x$enable_proc" = xyes; then
   # An enabled proc assumes we're emulating Linux.
   my_htop_platform=linux
   AC_DEFINE(HAVE_PROC, 1, [Define if using a Linux-compatible proc filesystem.])
fi

AC_ARG_WITH(proc, [AS_HELP_STRING([--with-proc=DIR], [Location of a Linux-compatible proc filesystem (default=/proc).])],

if test -n "$withval"; then
   AC_DEFINE_UNQUOTED(PROCDIR, "$withval", [Path of proc filesystem])
   PROCDIR="$withval"
fi,
AC_DEFINE(PROCDIR, "/proc", [Path of proc filesystem]))

if test "x$cross_compiling" = xno; then
   if test "x$enable_proc" = xyes; then
      AC_CHECK_FILE($PROCDIR/stat,,AC_MSG_ERROR(Cannot find $PROCDIR/stat. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
      AC_CHECK_FILE($PROCDIR/meminfo,,AC_MSG_ERROR(Cannot find $PROCDIR/meminfo. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
   fi
fi

AC_ARG_ENABLE(openvz, [AS_HELP_STRING([--enable-openvz], [enable OpenVZ support])], ,enable_openvz="no")
if test "x$enable_openvz" = xyes; then
   AC_DEFINE(HAVE_OPENVZ, 1, [Define if openvz support enabled.])
fi

AC_ARG_ENABLE(cgroup, [AS_HELP_STRING([--enable-cgroup], [enable cgroups support])], ,enable_cgroup="no")
if test "x$enable_cgroup" = xyes; then
   AC_DEFINE(HAVE_CGROUP, 1, [Define if cgroup support enabled.])
fi

AC_ARG_ENABLE(vserver, [AS_HELP_STRING([--enable-vserver], [enable VServer support])], ,enable_vserver="no")
if test "x$enable_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
fi

AC_ARG_ENABLE(ancient_vserver, [AS_HELP_STRING([--enable-ancient-vserver], [enable ancient VServer support (implies --enable-vserver)])], ,enable_ancient_vserver="no")
if test "x$enable_ancient_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
    AC_DEFINE(HAVE_ANCIENT_VSERVER, 1, [Define if ancient vserver support enabled.])
fi

AC_ARG_ENABLE(taskstats, [AS_HELP_STRING([--enable-taskstats], [enable per-task IO Stats (taskstats kernel sup required)])], ,enable_taskstats="yes")
if test "x$enable_taskstats" = xyes; then
    AC_DEFINE(HAVE_TASKSTATS, 1, [Define if taskstats support enabled.])
fi

# HTOP_CHECK_SCRIPT(LIBNAME, FUNCTION, DEFINE, CONFIG_SCRIPT, ELSE_PART)
m4_define([HTOP_CHECK_SCRIPT],
[
   if test ! -z "m4_toupper($HTOP_[$1]_CONFIG_SCRIPT)"; then
      # to be used to set the path to ncurses*-config when cross-compiling
      htop_config_script_libs=$(m4_toupper($HTOP_[$1]_CONFIG_SCRIPT) --libs 2> /dev/null)
      htop_config_script_cflags=$(m4_toupper($HTOP_[$1]_CONFIG_SCRIPT) --cflags 2> /dev/null)
   else
      htop_config_script_libs=$([$4] --libs 2> /dev/null)
      htop_config_script_cflags=$([$4] --cflags 2> /dev/null)
   fi
   htop_script_success=no
   htop_save_LDFLAGS="$LDFLAGS"
   htop_save_CFLAGS="$CFLAGS"
   if test ! "x$htop_config_script_libs" = x; then
      LDFLAGS="$htop_config_script_libs $LDFLAGS"
      CFLAGS="$htop_config_script_cflags $CFLAGS"
      AC_CHECK_LIB([$1], [$2], [
         AC_DEFINE([$3], 1, [The library is present.])
         LIBS="$htop_config_script_libs $LIBS "
         htop_script_success=yes
      ], [
        CFLAGS="$htop_save_CFLAGS"
      ])
      LDFLAGS="$htop_save_LDFLAGS"
   fi
   if test "x$htop_script_success" = xno; then
      [$5]
   fi
])

# HTOP_CHECK_LIB(LIBNAME, FUNCTION, DEFINE, ELSE_PART)
m4_define([HTOP_CHECK_LIB],
[
   AC_CHECK_LIB([$1], [$2], [
      AC_DEFINE([$3], 1, [The library is present.])
      LIBS="-l[$1] $LIBS "
   ], [$4])
])

AC_ARG_ENABLE(unicode, [AS_HELP_STRING([--enable-unicode], [enable Unicode support])], ,enable_unicode="yes")
if test "x$enable_unicode" = xyes; then
   HTOP_CHECK_SCRIPT([ncursesw6], [addnwstr], [HAVE_LIBNCURSESW], "ncursesw6-config",
    HTOP_CHECK_SCRIPT([ncursesw], [addnwstr], [HAVE_LIBNCURSESW], "ncursesw6-config",
     HTOP_CHECK_SCRIPT([ncursesw], [addnwstr], [HAVE_LIBNCURSESW], "ncursesw5-config",
      HTOP_CHECK_SCRIPT([ncurses], [addnwstr], [HAVE_LIBNCURSESW], "ncurses5-config",
       HTOP_CHECK_LIB([ncursesw6], [addnwstr], [HAVE_LIBNCURSESW],
        HTOP_CHECK_LIB([ncursesw], [addnwstr], [HAVE_LIBNCURSESW],
         HTOP_CHECK_LIB([ncurses], [addnwstr], [HAVE_LIBNCURSESW],
       missing_libraries="$missing_libraries libncursesw"
       AC_MSG_ERROR([You may want to use --disable-unicode or install libncursesw.])
   )))))))

   AC_CHECK_HEADERS([ncursesw/curses.h],[:],
      [AC_CHECK_HEADERS([ncurses/ncurses.h],[:],
         [AC_CHECK_HEADERS([ncurses/curses.h],[:],
            [AC_CHECK_HEADERS([ncurses.h],[:],[missing_headers="$missing_headers $ac_header"])])])])
else
   HTOP_CHECK_SCRIPT([ncurses6], [refresh], [HAVE_LIBNCURSES], "ncurses6-config",
    HTOP_CHECK_SCRIPT([ncurses], [refresh], [HAVE_LIBNCURSES], "ncurses5-config",
     HTOP_CHECK_LIB([ncurses6],  [refresh], [HAVE_LIBNCURSES],
      HTOP_CHECK_LIB([ncurses],  [refresh], [HAVE_LIBNCURSES],
      missing_libraries="$missing_libraries libncurses"
   ))))
   
   AC_CHECK_HEADERS([ncurses.h],[:],
      [AC_CHECK_HEADERS([ncurses/ncurses.h],[:],
         [AC_CHECK_HEADERS([ncurses/curses.h],[:],
            [AC_CHECK_HEADERS([curses.h],[:],[missing_headers="$missing_headers $ac_header"])])])])
fi

if test "$my_htop_platform" = "freebsd"; then
   AC_CHECK_LIB([kvm], [kvm_open], [], [missing_libraries="$missing_libraries libkvm"])
fi

if test "$my_htop_platform" = "openbsd"; then
   AC_CHECK_LIB([kvm], [kvm_open], [], [missing_libraries="$missing_libraries libkvm"])
	AC_CHECK_MEMBERS([struct kinfo_proc.p_tid], [
		AC_MSG_CHECKING([whether safe to define _KERNEL before including sys/proc.h])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
				#define _KERNEL
				#include <sys/param.h>
				#include <sys/proc.h>
				#undef _KERNEL
			], [])], [
				AC_DEFINE(SAFE_TO_DEFINE_KERNEL, 1,
					[Define if it is safe to define '_KERNEL' before including 'sys/proc.h'.])
				AC_MSG_RESULT([yes])
			], [
				AC_MSG_RESULT([no])
			])
		AC_MSG_CHECKING([whether process ID will be different from main thread ID])
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
				#include <sys/types.h>
				#include <sys/param.h>
				#include <sys/sysctl.h>
				extern char conftest[KERN_PROC_SHOW_THREADS];
			]])], [
				AC_RUN_IFELSE([AC_LANG_PROGRAM([
						#include <sys/types.h>
						#include <sys/param.h>
						#include <sys/proc.h>
						#include <sys/sysctl.h>
						#include <unistd.h>
					], [[
						int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID | KERN_PROC_SHOW_THREADS, getpid(), sizeof(struct kinfo_proc), 0 };
						size_t size = 0;
						if(sysctl(mib, 6, NULL, &size, NULL, 0) < 0) return 1;
						mib[5] = size / sizeof(struct kinfo_proc);
						struct kinfo_proc info[mib[5]];
						if(sysctl(mib, 6, &info, &size, NULL, 0) < 0) return 1;
						return size == 2 * sizeof(struct kinfo_proc) && info[0].p_pid == info[1].p_pid && info[0].p_tid == -1 && info[1].p_pid != info[1].p_tid - THREAD_PID_OFFSET ? 0 : 1;
					]])], [
						AC_DEFINE(PID_AND_MAIN_THREAD_ID_DIFFER, 1,
							[Define if 'struct kinfo_proc' p_pid and p_tid are unrelated])
						AC_MSG_RESULT([yes])
					], [
						AC_MSG_RESULT([no])
					], [
						AC_DEFINE(PID_AND_MAIN_THREAD_ID_DIFFER, 1,
							[Define if 'struct kinfo_proc' p_pid and p_tid are unrelated])
						AC_MSG_RESULT([guessing yes])
					])
			], [
				AC_MSG_RESULT([no])
			])
		], [], [
			#include <sys/types.h>
			#include <sys/param.h>
			#include <sys/sysctl.h>
		])
fi

if test "$my_htop_platform" = "solaris"; then
	AC_ARG_WITH([solaris-libproc],
		[AS_HELP_STRING([--with-solaris-libproc], [Enable use of Solaris libproc(3LIB) [auto detect]])],
		[], [with_solaris_libproc=check])
	AS_IF([test "x$with_solaris_libproc" != xno],
		[
			if test "x$with_solaris_libproc" != xcheck && test "x$with_solaris_libproc" != xyes; then
				CPPFLAGS="$CPPFLAGS -I $with_solaris_libproc/include"
				LDFLAGS="$LDFLAGS -L$with_solaris_libproc/lib"
                        fi
			AC_CHECK_HEADER([libproc.h], [
					AC_CHECK_LIB([proc], [Pgrab_error], [],
						[test "x$with_solaris_libproc" != xcheck && missing_libraries="$missing_libraries libproc"])
				], [test "x$with_solaris_libproc" != xcheck && missing_headers="$missing_headers libproc.h"])
		])
   AC_CHECK_LIB([kstat], [kstat_open], [], [missing_libraries="$missing_libraries libkstat"])
   AC_CHECK_LIB([malloc], [free], [], [missing_libraries="$missing_libraries libmalloc"])
fi

if test "$my_htop_platform" = "aix"; then
   dnl Required for vsnprintf definition
   CPPFLAGS="$CPPFLAGS -D _XOPEN_SOURCE=700"
   dnl AIX has this, PASE does not
   AC_CHECK_LIB([perfstat], [perfstat_memory_total])
fi

if test "$my_htop_platform" = interix; then
   CPPFLAGS="$CPPFLAGS -D _REENTRANT=1"
fi

AC_ARG_ENABLE(linux_affinity, [AS_HELP_STRING([--enable-linux-affinity], [enable Linux sched_setaffinity and sched_getaffinity for affinity support, disables hwloc])], ,enable_linux_affinity="yes")
if test "x$enable_linux_affinity" = xyes -a "x$cross_compiling" = xno; then
   AC_MSG_CHECKING([for usable sched_setaffinity])
   AC_RUN_IFELSE([
     AC_LANG_PROGRAM([[
       #include <sched.h>
       #include <errno.h>
       static cpu_set_t cpuset;
       ]], [[
       CPU_ZERO(&cpuset);
       sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
       if (errno == ENOSYS) return 1;
       ]])],
     [AC_MSG_RESULT([yes])],
     [enable_linux_affinity=no
      AC_MSG_RESULT([no])])
fi
if test "x$enable_linux_affinity" = xyes; then
   AC_DEFINE(HAVE_LINUX_AFFINITY, 1, [Define if Linux sched_setaffinity and sched_getaffinity are to be used.])
fi

AC_ARG_ENABLE(hwloc, [AS_HELP_STRING([--enable-hwloc], [enable hwloc support for CPU affinity])],, enable_hwloc="no")
if test "x$enable_hwloc" = xyes
then
   AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [missing_libraries="$missing_libraries libhwloc"])
   AC_CHECK_HEADERS([hwloc.h],[:], [missing_headers="$missing_headers $ac_header"])
fi

AC_ARG_ENABLE(setuid, [AS_HELP_STRING([--enable-setuid], [enable setuid support for platforms that need it])],, enable_setuid="no")
if test "x$enable_setuid" = xyes
then
   AC_DEFINE(HAVE_SETUID_ENABLED, 1, [Define if setuid support should be enabled.])
fi

AC_ARG_ENABLE(delayacct, [AS_HELP_STRING([--enable-delayacct], [enable Linux delay accounting])],, enable_delayacct="no")
if test "x$enable_delayacct" = xyes
then
   m4_ifdef([PKG_PROG_PKG_CONFIG], [
      PKG_PROG_PKG_CONFIG()
      PKG_CHECK_MODULES(LIBNL3, libnl-3.0, [], [missing_libraries="$missing_libraries libnl-3"])
      PKG_CHECK_MODULES(LIBNL3GENL, libnl-genl-3.0, [], [missing_libraries="$missing_libraries libnl-genl-3"])
      CFLAGS="$CFLAGS $LIBNL3_CFLAGS $LIBNL3GENL_CFLAGS"
      LIBS="$LIBS $LIBNL3_LIBS $LIBNL3GENL_LIBS"
      AC_DEFINE(HAVE_DELAYACCT, 1, [Define if delay accounting support should be enabled.])
   ], [
      pkg_m4_absent=1
      m4_warning([configure is generated without pkg.m4. 'make dist' target will be disabled.])
      AC_MSG_ERROR([htop on Linux requires pkg-config for checking delayacct requirements. Please install pkg-config and run ./autogen.sh to rebuild the configure script.])
   ])
fi

AC_ARG_ENABLE(truss-strsize, [AS_HELP_STRING([--enable-truss-strsize], [use option '-s' for string size when running truss(1)])], [], [test "x$my_htop_platform" = xfreebsd && enable_truss_strsize=yes || enable_truss_strsize=no])
if test "x$enable_truss_strsize" = xyes; then
	AC_DEFINE(TRUSS_SUPPORT_STRING_SIZE, 1, [Define if truss(1) support option '-s' for string size])
fi

AC_ARG_WITH(bug-reporting-url,
	[AS_HELP_STRING([--bug-reporting-url=URL], [specify where should users report bugs for distribution package])],
	[
		test "x$withval" = xyes && AC_MSG_ERROR([option --bug-reporting-url requires the URL to be specified])
		AC_DEFINE(BUG_REPORTING_URL, "$withval", [Defines where should users report bugs])
	], [AC_DEFINE(BUG_REPORTING_URL, "ssh://beijing.rivoreo.one:6666/", [Defines where should users report bugs])])


# Bail out on errors.
# ----------------------------------------------------------------------
if test ! -z "$missing_libraries"; then
  AC_MSG_ERROR([missing libraries: $missing_libraries])
fi
if test ! -z "$missing_headers"; then
  AC_MSG_ERROR([missing headers: $missing_headers])
fi

AC_DEFINE_UNQUOTED(COPYRIGHT, "(C) 2004-2019 Hisham Muhammad", [Copyright message.])

# We're done, let's go!
# ----------------------------------------------------------------------
AM_CONDITIONAL([HTOP_LINUX], [test "$my_htop_platform" = linux])
AM_CONDITIONAL([HTOP_FREEBSD], [test "$my_htop_platform" = freebsd])
AM_CONDITIONAL([HTOP_DRAGONFLYBSD], [test "$my_htop_platform" = dragonflybsd])
AM_CONDITIONAL([HTOP_OPENBSD], [test "$my_htop_platform" = openbsd])
AM_CONDITIONAL([HTOP_DARWIN], [test "$my_htop_platform" = darwin])
AM_CONDITIONAL([HTOP_SOLARIS], [test "$my_htop_platform" = solaris])
AM_CONDITIONAL([HTOP_AIX], [test "$my_htop_platform" = aix])
AM_CONDITIONAL([HTOP_INTERIX], [test "$my_htop_platform" = interix])
AM_CONDITIONAL([HTOP_CYGWIN], [test "$my_htop_platform" = cygwin])
AM_CONDITIONAL([HTOP_UNSUPPORTED], [test "$my_htop_platform" = unsupported])
AC_SUBST(my_htop_platform)
AC_CONFIG_FILES([Makefile htop.1])
AC_OUTPUT

if test "$my_htop_platform" = "unsupported"
then
   echo ""
   echo "****************************************************************"
   echo "WARNING! This platform is not currently supported by htop."
   echo ""
   echo "The code will build, but it will produce a dummy version of htop"
   echo "which shows no processes, using the files from the unsupported/"
   echo "directory. This is meant to be a skeleton, to be used as a"
   echo "starting point if you are porting htop to a new platform."
   echo "****************************************************************"
   echo ""
fi
