AC_DEFUN(IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS, [
AC_CACHE_CHECK(for mem in sys/systm.h, ac_cv_irix_sys_systm_h_has_mem_funcs,[
if test -f "/usr/include/sys/systm.h"; then
    have_memcpy=`grep memcpy /usr/include/sys/systm.h`
    if test "x$have_memcpy" = "x"; then
        ac_cv_irix_sys_systm_h_has_mem_funcs=no
    else
        ac_cv_irix_sys_systm_h_has_mem_funcs=yes
    fi
else
    ac_cv_irix_sys_systm_h_has_mem_funcs=no
fi
])
if test "$ac_cv_irix_sys_systm_h_has_mem_funcs" = "yes"; then
  AC_DEFINE(IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS, 1, [define if irix kernel has memcpy et al])dnl
fi
AC_MSG_RESULT($ac_cv_irix_sys_systm_h_has_mem_funcs)])
