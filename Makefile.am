
ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = htop

dist_man_MANS = htop.1 htop.colorscheme.5
EXTRA_DIST = $(dist_man_MANS) htop.desktop htop.png scripts/MakeHeader.sh \
install-sh autogen.sh missing
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png

AM_CPPFLAGS = $(LOCAL_CPPFLAGS) -D SYSCONFDIR=\"$(sysconfdir)\" -I "$(my_htop_platform)" -I "$(top_srcdir)/$(my_htop_platform)"
AM_CFLAGS = $(LOCAL_CFLAGS)
AM_LDFLAGS = $(LOCAL_LDFLAGS)

myhtopsources = AvailableMetersPanel.c CategoriesPanel.c CheckItem.c \
ClockMeter.c ColorsPanel.c ColumnsPanel.c CPUMeter.c CRT.c MainPanel.c \
ControlOptionsPanel.c DisplayOptionsPanel.c FunctionBar.c Hashtable.c \
Header.c htop.c ListItem.c LoadAverageMeter.c MemoryMeter.c Meter.c \
MetersPanel.c Object.c Panel.c BatteryMeter.c Process.c ProcessList.c \
RichString.c ScreenManager.c Settings.c SignalsPanel.c StringUtils.c \
IOUtils.c SwapMeter.c TasksMeter.c UptimeMeter.c TraceScreen.c \
UsersTable.c Vector.c AvailableColumnsPanel.c AffinityPanel.c \
HostnameMeter.c OpenFilesScreen.c Affinity.c IncSet.c Action.c ArgScreen.c \
EnvScreen.c InfoScreen.c XAlloc.c KStat.c KernelStackTraceScreen.c \
UsersMeter.c

myhtopheaders = AvailableColumnsPanel.h AvailableMetersPanel.h \
CategoriesPanel.h CheckItem.h ClockMeter.h ColorsPanel.h ColumnsPanel.h \
CPUMeter.h CRT.h MainPanel.h ControlOptionsPanel.h DisplayOptionsPanel.h \
FunctionBar.h Hashtable.h Header.h ListItem.h LoadAverageMeter.h \
MemoryMeter.h BatteryMeter.h Meter.h MetersPanel.h Object.h Panel.h \
ProcessList.h RichString.h ScreenManager.h Settings.h SignalsPanel.h \
StringUtils.h IOUtils.h SwapMeter.h TasksMeter.h UptimeMeter.h \
TraceScreen.h UsersTable.h Vector.h Process.h AffinityPanel.h \
HostnameMeter.h OpenFilesScreen.h Affinity.h IncSet.h Action.h \
ArgScreen.h EnvScreen.h InfoScreen.h XAlloc.h KStat.h \
KernelStackTraceScreen.h FieldData.h UsersMeter.h

if DISK_STATS
myhtopsources += DiskPanel.c DiskList.c Disk.c
myhtopheaders += DiskPanel.h DiskList.h Disk.h
endif

all_platform_headers =

# Linux
# -----

linux_platform_headers = \
	linux/Platform.h \
	linux/IOPriorityPanel.h \
	linux/IOPriority.h \
	linux/LinuxProcess.h \
	linux/LinuxProcessList.h \
	linux/LinuxDiskList.h \
	linux/LinuxDisk.h \
	linux/Battery.h

all_platform_headers += $(linux_platform_headers)

if HTOP_LINUX
myhtopplatsources = linux/Platform.c linux/IOPriorityPanel.c linux/IOPriority.c \
linux/LinuxProcess.c linux/LinuxProcessList.c linux/Battery.c

myhtopplatheaders = $(linux_platform_headers)

if DISK_STATS
myhtopplatsources += \
	linux/LinuxDiskList.c \
	linux/LinuxDisk.c
endif
endif

# FreeBSD
# -------

freebsd_platform_headers = \
	bsd/Platform.h \
	bsd/BSDProcess.h \
	freebsd/Platform.h \
	freebsd/FreeBSDProcessList.h \
	freebsd/FreeBSDProcess.h \
	freebsd/FreeBSDDiskList.h \
	freebsd/FreeBSDDisk.h \
	freebsd/Battery.h

all_platform_headers += $(freebsd_platform_headers)

if HTOP_FREEBSD
myhtopplatsources = \
	bsd/Platform.c \
	bsd/BSDProcess.c \
	freebsd/Platform.c \
	freebsd/FreeBSDProcessList.c \
	freebsd/FreeBSDProcess.c \
	freebsd/Battery.c

myhtopplatheaders = $(freebsd_platform_headers)

if DISK_STATS
myhtopplatsources += \
	freebsd/FreeBSDDiskList.c \
	freebsd/FreeBSDDisk.c
endif
endif

# DragonFlyBSD
# ------------

dragonflybsd_platform_headers = \
	bsd/Platform.h \
	bsd/BSDProcess.h \
	dragonflybsd/Platform.h \
	dragonflybsd/DragonFlyBSDProcessList.h \
	dragonflybsd/DragonFlyBSDProcess.h \
	dragonflybsd/Battery.h

all_platform_headers += $(dragonflybsd_platform_headers)

if HTOP_DRAGONFLYBSD
AM_LDFLAGS += -lkvm -lkinfo
myhtopplatsources = \
	bsd/Platform.c \
	bsd/BSDProcess.c \
	dragonflybsd/Platform.c \
	dragonflybsd/DragonFlyBSDProcessList.c \
	dragonflybsd/DragonFlyBSDProcess.c \
	dragonflybsd/Battery.c

myhtopplatheaders = $(dragonflybsd_platform_headers)
endif

# OpenBSD
# -------

openbsd_platform_headers = \
	bsd/Platform.h \
	bsd/BSDProcess.h \
	openbsd/Platform.h \
	openbsd/OpenBSDProcessList.h \
	openbsd/OpenBSDProcess.h \
	openbsd/Battery.h

all_platform_headers += $(openbsd_platform_headers)

if HTOP_OPENBSD
myhtopplatsources = \
	bsd/Platform.c \
	bsd/BSDProcess.c \
	openbsd/Platform.c \
	openbsd/OpenBSDProcessList.c \
	openbsd/OpenBSDProcess.c \
	openbsd/Battery.c

myhtopplatheaders = $(openbsd_platform_headers)
endif

# Darwin
# ------

darwin_platform_headers = \
	bsd/Platform.h \
	bsd/BSDProcess.h \
	darwin/Platform.h \
	darwin/DarwinProcess.h \
	darwin/DarwinProcessList.h \
	darwin/Battery.h

all_platform_headers += $(darwin_platform_headers)

if HTOP_DARWIN
AM_LDFLAGS += -framework IOKit -framework CoreFoundation
myhtopplatsources = \
	bsd/Platform.c \
	bsd/BSDProcess.c \
	darwin/Platform.c \
	darwin/DarwinProcess.c \
	darwin/DarwinProcessList.c \
	darwin/Battery.c

myhtopplatheaders = $(darwin_platform_headers)
endif

# Solaris
# -------

solaris_platform_headers = \
	solaris/Platform.h \
	solaris/SolarisProcess.h \
	solaris/SolarisProcessList.h \
	solaris/Battery.h

all_platform_headers += $(solaris_platform_headers)

if HTOP_SOLARIS
myhtopplatsources = \
	solaris/Platform.c \
	solaris/SolarisProcess.c \
	solaris/SolarisProcessList.c \
	solaris/Battery.c

myhtopplatheaders = $(solaris_platform_headers)
endif

# AIX
# -------

aix_platform_headers = \
	aix/Platform.h \
	aix/AixProcess.h \
	aix/AixProcessList.h \
	aix/Battery.h

all_platform_headers += $(aix_platform_headers)

if HTOP_AIX
myhtopplatsources = \
	aix/Platform.c \
	aix/AixProcess.c \
	aix/AixProcessList.c \
	aix/Battery.c

myhtopplatheaders = $(aix_platform_headers)
endif

# Interix
# -------

interix_platform_headers = \
	interix/Platform.h \
	interix/InterixProcess.h \
	interix/Battery.h

all_platform_headers += $(interix_platform_headers)

if HTOP_INTERIX
myhtopplatsources = \
	interix/Platform.c \
	interix/InterixProcess.c \
	interix/InterixProcessList.c \
	interix/Battery.c
myhtopplatheaders = $(interix_platform_headers)
endif

# Cygwin
# ------

cygwin_platform_headers = \
	cygwin/Platform.h \
	cygwin/IOPriorityPanel.h \
	cygwin/IOPriority.h \
	cygwin/LinuxProcess.h \
	cygwin/LinuxProcessList.h \
	cygwin/Battery.h
all_platform_headers += $(cygwin_platform_headers)

if HTOP_CYGWIN
myhtopplatsources = \
	cygwin/Platform.c \
	cygwin/IOPriorityPanel.c \
	cygwin/IOPriority.c \
	cygwin/LinuxProcess.c \
	cygwin/LinuxProcessList.c \
	cygwin/Battery.c
myhtopplatheaders = $(cygwin_platform_headers)
endif

# GNU/Hurd
# --------

hurd_platform_headers = \
	hurd/Platform.h \
	hurd/HurdProcess.h \
	hurd/HurdProcessList.h \
	hurd/Battery.h
all_platform_headers += $(hurd_platform_headers)

if HTOP_HURD
myhtopplatsources = \
	hurd/Platform.c \
	hurd/HurdProcess.c \
	hurd/HurdProcessList.c \
	hurd/Battery.c
myhtopplatheaders = $(hurd_platform_headers)
endif

# Haiku
# -----

haiku_platform_headers = \
	haiku/Platform.h \
	haiku/HaikuProcess.h \
	haiku/HaikuProcessList.h \
	haiku/Battery.h
all_platform_headers += $(haiku_platform_headers)

if HTOP_HAIKU
myhtopplatsources = \
	haiku/Platform.c \
	haiku/HaikuProcess.c \
	haiku/HaikuProcessList.c \
	haiku/Battery.c
myhtopplatheaders = $(haiku_platform_headers)
endif

# Unsupported
# -----------

unsupported_platform_headers = \
	unsupported/Platform.h \
	unsupported/UnsupportedProcess.h \
	unsupported/UnsupportedProcessList.h \
	unsupported/Battery.h

all_platform_headers += $(unsupported_platform_headers)

if HTOP_UNSUPPORTED
myhtopplatsources = \
	unsupported/Platform.c \
	unsupported/UnsupportedProcess.c \
	unsupported/UnsupportedProcessList.c \
	unsupported/Battery.c

myhtopplatheaders = $(unsupported_platform_headers)
endif

# ----

SUFFIXES = .h

BUILT_SOURCES = $(myhtopheaders) $(myhtopplatheaders)
htop_SOURCES  = $(myhtopheaders) $(myhtopplatheaders) $(myhtopsources) $(myhtopplatsources) config.h

DUMP_BUILTIN_COLOR_SCHEME_OBJECTS = dump-builtin-color-scheme.o CRT.o StringUtils.o XAlloc.o RichString.o

.PHONY: htop-headers clean-htop-headers

htop-headers: $(myhtopheaders) $(all_platform_headers)

clean-htop-headers:
	-rm -f $(myhtopheaders) $(all_platform_headers)

target:
	echo $(htop_SOURCES)

profile:
	LOCAL_CPPFLAGS="-D NDEBUG=1" LOCAL_CFLAGS=-pg $(MAKE) all

debug:
	LOCAL_CPPFLAGS="-D DEBUG=1" LOCAL_CFLAGS=-g $(MAKE) all

coverage:
	LOCAL_CPPFLAGS="-D DEBUG=1" LOCAL_CFLAGS="-fprofile-arcs -ftest-coverage" LDADD=-lgcov $(MAKE) all

dump-builtin-color-scheme:	$(DUMP_BUILTIN_COLOR_SCHEME_OBJECTS)
	$(AM_V_CCLD)$(LINK) $(DUMP_BUILTIN_COLOR_SCHEME_OBJECTS) -Wl,--as-needed $(LIBS)

.c.h:
	@case "$@" in \
		/*) \
			printf %s\\n "Error: attempt to generate header with absolute path '$@'!" 1>&2; \
			exit 1; \
			;; \
		../*|*/../*) \
			;; \
		*) \
			[ -d $(@D) ] || mkdir -p $(@D) \
			;; \
	esac
	@if [ -h $@ ]; then rm -f $@ || exit; fi
	@srcdir@/scripts/MakeHeader.sh $< > $@

cppcheck:
	cppcheck -q -v . --enable=all -DHAVE_CGROUP -DHAVE_OPENVZ -DHAVE_TASKSTATS

dist-hook: $(top_distdir)/configure
	@if grep -Fq pkg_m4_absent=yes '$(top_distdir)/configure'; then \
	   echo 'configure is generated without pkg.m4. Please supply pkg.m4 and run autogen.sh to rebuild the configure script.' 1>&2; \
	   exit 1; \
	else :; \
	fi

.PHONY: lcov

lcov:
	mkdir -p lcov
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory lcov

clean-local:
	-rm -f dump-builtin-color-scheme
