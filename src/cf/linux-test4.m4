AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_CDEV, [
AC_MSG_CHECKING(for i_cdev in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_cdev, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_cdev);], 
ac_cv_linux_fs_struct_inode_has_i_cdev=yes,
ac_cv_linux_fs_struct_inode_has_i_cdev=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_cdev)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_BYTES, [
AC_MSG_CHECKING(for i_bytes in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_bytes, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_bytes);], 
ac_cv_linux_fs_struct_inode_has_i_bytes=yes,
ac_cv_linux_fs_struct_inode_has_i_bytes=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_bytes)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK, [
AC_MSG_CHECKING(for page_lock in struct address_space)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_address_space_has_page_lock, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct address_space _a_s;
printf("%x\n", _a_s.page_lock);], 
ac_cv_linux_fs_struct_address_space_has_page_lock=yes,
ac_cv_linux_fs_struct_address_space_has_page_lock=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_address_space_has_page_lock)
CPPFLAGS="$save_CPPFLAGS"])

