AC_DEFUN(IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS, [
AC_MSG_CHECKING(for mem* in sys/systm.h)
save_CPPFLAGS="$CPPFLAGS"
AC_CACHE_VAL(ac_cv_irix_sys_systm_h_has_mem_funcs,
[
AC_TRY_COMPILE(
[#include <sys/systm.h>],
[
extern void     *memcpy(char *, const void *, size_t);
],
ac_cv_irix_sys_systm_h_has_mem_funcs=no,
ac_cv_irix_sys_systm_h_has_mem_funcs=yes)])
AC_MSG_RESULT($ac_cv_irix_sys_systm_h_has_mem_funcs)])
