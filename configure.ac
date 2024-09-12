
m4_define([MAJOR_V], [0])
m4_define([MINOR_V], [1])
m4_define([PATCH_V], [0])

AC_INIT([igni-render], MAJOR_V.MINOR_V.PATCH_V)
CFLAGS="$CFLAGS -DMAJ_V="MAJOR_V" -DMIN_V="MINOR_V" -DPATCH_V="PATCH_V
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

dnl assimp - open asset import library
AC_CHECK_LIB([assimp], [aiImportFile], [], \
	AC_MSG_ERROR([No suitable assimp version found!]))

dnl libm - the maths part of the C standard library
AC_CHECK_LIB([m], [sinf], [], \
	AC_MSG_ERROR([No suitable libm version found!]))

dnl Vulkan - graphics library
AC_CHECK_LIB([vulkan], [vkCreateDisplayPlaneSurfaceKHR], [], \
	AC_MSG_ERROR([No suitable Vulkan version found!]))

dnl GLFW - window library
AC_CHECK_LIB([glfw], [glfwCreateWindow], [], \
	AC_MSG_WARN([No suitable GLFW3 version found!]))

dnl libdrm - direct rendering management
AC_CHECK_LIB([drm], [drmModeGetConnector], [], \
	AC_MSG_ERROR([No suitable libdrm version found!]))
CFLAGS="$CFLAGS -I/usr/include/libdrm"

AC_PROG_CC

AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT

