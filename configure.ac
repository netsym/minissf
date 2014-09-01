AC_PREREQ([2.63])
AC_INIT([MiniSSF], [1.2], [bugs@primessf.net])
AC_CONFIG_SRCDIR([ssf.h])
AC_CONFIG_HEADERS([ssf_config.h])

AC_PROG_CXX
AC_PROG_CC
AC_PROG_LEX
AC_PROG_RANLIB
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P

SSFDIR=\'`cd .; pwd`\'
AC_SUBST(SSFDIR)

AC_PATH_PROG(AR, ar, "nonexist")
if test x"$AR" = x"nonexist"; then
  AC_MSG_ERROR([Cannot find 'ar' as required by minissf])
fi
AC_SUBST(AR)

# check flex
if test "x$LEX" != "xflex"; then
  HAVE_FLEX="no"
else
  HAVE_FLEX="yes"
fi
AC_SUBST(HAVE_FLEX)
#if test "x$LEX" = "x:"; then
#  AC_MSG_ERROR([Cannot find 'lex' as required by minissf])
#fi
#AC_SUBST(LEX)

AC_PATH_PROG(PERL, perl, "nonexist")
if test x"$PERL" = x"nonexist"; then
  AC_MSG_ERROR([Cannot find perl as required by minissf])
else
  PERL=`which perl`;
fi
AC_SUBST(PERL)

# check mpi
AC_ARG_ENABLE(mpi,    [  --enable-mpi             enable message passing],
  ENABLE_MPI=$enableval, ENABLE_MPI="yes")
if test x"$ENABLE_MPI" = x"yes"; then
  AC_ARG_WITH(mpicxx,   [  --with-mpicxx     	  set the mpi c++ compiler],
    WITH_MPICXX=$withval, WITH_MPICXX="nonexist")
  AC_ARG_WITH(mpicc,    [  --with-mpicc     	  set the mpi c compiler],
    WITH_MPICC=$withval, WITH_MPICC="nonexist")

  if test x"$WITH_MPICXX" = x"nonexist"; then # if the user didn't specify
    AC_PATH_PROG(MPICXX, "mpicxx", "nonexist") # first try mpicxx
    if test x"$MPICXX" = x"nonexist"; then # if mpicxx is not present
      AC_PATH_PROG(MPICXX1, "mpiCC", "nonexist") # then try mpiCC
      MPICXX="$MPICXX1"
      if test x"$MPICXX" = x"nonexist"; then # if mpiCC is also not present, use the orginal
        MPICXX="$CXX"
      fi
    fi
  else # if the user specified the compiler, use it
    MPICXX="$WITH_MPICXX"
  fi

  if test x"$WITH_MPICC" = x"nonexist"; then # if the user didn't specify
    AC_PATH_PROG(MPICC, "mpicc", "nonexist") # only try mpicc
    if test x"$MPICC" = x"nonexist"; then # if mpicc is not present, warn the user
      MPICC="$CC"
    fi
  else # if the user specified the compiler, use it
    MPICC="$WITH_MPICC"
  fi
else
  MPICXX="$CXX"
  MPICC="$CC"
fi
AC_SUBST(MPICXX)
AC_SUBST(MPICC)

# check llvm/clang
AC_ARG_WITH(llvm-path,    [  --with-llvm-path	  set the path to llvm source tree],
  WITH_LLVM_PATH=$withval, WITH_LLVM_PATH="nonexist")
if test x"$WITH_LLVM_PATH" = x"nonexist"; then
  LLVM_CLANG="no"
  LLVM_ROOT=""
else
  WITH_LLVM_PATH=`eval "echo $WITH_LLVM_PATH"` # this is to make sure tilde expansion to happen
  LLVM_CLANG="yes"
  if test -f "$WITH_LLVM_PATH/Release+Asserts/bin/clang"; then
    LLVM_ROOT=\'`cd $WITH_LLVM_PATH/Release+Asserts; pwd`\'
  elif test -f "$WITH_LLVM_PATH/Debug+Asserts/bin/clang"; then
    LLVM_ROOT=\'`cd $WITH_LLVM_PATH/Debug+Asserts; pwd`\'
  elif test -f "$WITH_LLVM_PATH/Release/bin/clang"; then
    LLVM_ROOT=\'`cd $WITH_LLVM_PATH/Release; pwd`\'
  elif test -f "$WITH_LLVM_PATH/bin/clang"; then
    LLVM_ROOT=\'`cd $WITH_LLVM_PATH; pwd`\'
  else
    AC_MSG_ERROR([Can't find clang; Abort!])
    exit 1
  fi
fi
AC_SUBST(LLVM_CLANG)
AC_SUBST(LLVM_ROOT)

AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h pthread.h stdlib.h string.h strings.h sys/param.h sys/time.h unistd.h])

if test x"$ENABLE_MPI" = x"yes"; then
MYCXX="$CXX"
MYCC="$CC"
MYCXXCPP="$CXXCPP"
MYCPP="$CPP"
CXX="$MPICXX"
CC="$MPICC"
CXXCPP="$MPICXX -E"
CPP="$MPICC -E"
AC_CHECK_HEADERS([mpi.h])
CXX="$MYCXX"
CC="$MYCC"
CXXCPP="$MYCXXCPP"
CPP="$MYCPP"
fi

AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_LONG_LONG_INT
#AC_C_VOLATILE
#AC_STRUCT_TM

AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
#AC_FUNC_SELECT_ARGTYPES
#AC_TYPE_SIGNAL
#AC_FUNC_STAT
#AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor gethostname gethrtime gettimeofday memset pow select sqrt stime strcasecmp strchr strdup strerror])

AC_CANONICAL_HOST
case "${host_cpu}-${host_os}" in
  *-darwin*)
    SSFMACH=DARWIN
    ;;
  *)
    SSFMACH=GENERIC
esac
AC_SUBST(SSFMACH)

AC_CONFIG_FILES([Makefile.include bin/cpp.pl bin/ckpr.pl bin/cxx.pl bin/cppcxx.pl bin/ld.pl])
AC_OUTPUT
