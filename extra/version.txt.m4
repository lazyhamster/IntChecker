m4_include(`..\source\version.m4i')m4_dnl
m4_define(`concat',`$1$2')m4_dnl
concat(VMAJOR,concat(VMINOR,VREVISION))
